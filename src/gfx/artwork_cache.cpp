// ps5-homebrew-ui - Decode, cancel and budget catalog textures.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gfx/artwork_cache.hpp"

#include "core/save_file.hpp"
#include "third_party/stb/stb_image.h"

#include <algorithm>
#include <utility>

namespace hui::gfx
{
namespace
{

ArtworkCache::Pixels decode_file(const std::string &path, int limit)
{
    std::string bytes;
    if (!save::read_file(path, &bytes, 8u << 20))
        return {};
    const auto *data = reinterpret_cast<const stbi_uc *>(bytes.data());
    const int size = static_cast<int>(bytes.size());
    int width = 0;
    int height = 0;
    int channels = 0;
    if (!stbi_info_from_memory(data, size, &width, &height, &channels) || width < 1 || height < 1 ||
        width > limit || height > limit)
        return {};
    stbi_uc *pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    if (pixels == nullptr)
        return {};
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    ArtworkCache::Pixels result{width, height, {pixels, pixels + count}};
    stbi_image_free(pixels);

    // A bright cover needs a deeper scrim. Sample across the entire image
    // because a cover may be cropped differently in a card and a detail hero.
    for (int y = 0; y < height; y += std::max(height / 32, 1))
    {
        for (int x = 0; x < width; x += std::max(width / 32, 1))
        {
            const auto at = (static_cast<std::size_t>(y) * width + x) * 4;
            const float light = 0.2126f * result.rgba[at] + 0.7152f * result.rgba[at + 1] +
                                0.0722f * result.rgba[at + 2];
            result.brightness = std::max(result.brightness, light / 255.0f);
        }
    }
    return result;
}

} // namespace

ArtworkCache::ArtworkCache(Upload upload, Release release, ArtworkLimits limits, Decode decode)
    : upload_(std::move(upload)), release_(std::move(release)),
      decode_(decode ? std::move(decode) : Decode(decode_file)), limits_(limits)
{
    limits_.max_dimension = std::clamp(limits_.max_dimension, 1, 2048);
}

ArtworkCache::~ArtworkCache()
{
    if (started_)
    {
        {
            const std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_one();
        pthread_join(thread_, nullptr);
    }
    for (const auto &[id, entry] : entries_)
        release_entry(entry);
}

void *ArtworkCache::worker_entry(void *self)
{
    static_cast<ArtworkCache *>(self)->worker();
    return nullptr;
}

void ArtworkCache::worker()
{
    std::unique_lock lock(mutex_);
    while (true)
    {
        wake_.wait(lock, [&] { return stopping_ || (!jobs_.empty() && !complete_); });
        if (stopping_)
            return;
        current_ = std::move(jobs_.front());
        jobs_.pop_front();
        const Job &job = *current_;
        lock.unlock();
        Pixels pixels = decode_(job.request.path, limits_.max_dimension);
        lock.lock();
        complete_ = Completion{job.request.id, job.ticket, std::move(pixels)};
        current_.reset();
    }
}

void ArtworkCache::release_entry(const Entry &entry)
{
    if (entry.artwork.texture != 0)
        release_(entry.artwork.texture);
    bytes_ -= entry.bytes;
}

void ArtworkCache::select(std::span<const ArtworkRequest> requests)
{
    if (!started_ && !requests.empty())
        started_ = pthread_create(&thread_, nullptr, worker_entry, this) == 0;

    for (auto &[id, entry] : entries_)
        entry.wanted = false;

    std::deque<Job> wanted;
    std::size_t priority = 0;
    const auto window = requests.first(std::min(requests.size(), limits_.requests));
    for (const ArtworkRequest &request : window)
    {
        Entry &entry = entries_[request.id];
        if (entry.wanted)
            continue;
        if (entry.path != request.path || entry.artwork.status == ArtworkStatus::missing)
        {
            release_entry(entry);
            entry = {};
            entry.path = request.path;
            entry.ticket = ++ticket_;
            entry.artwork.status = started_ ? ArtworkStatus::pending : ArtworkStatus::unavailable;
        }
        entry.wanted = true;
        entry.priority = priority++;
        entry.touched = ++clock_;
        if (entry.artwork.status == ArtworkStatus::pending)
            wanted.push_back({request, entry.ticket});
    }
    // Completed textures can stay resident until their bytes are needed.
    std::erase_if(
        entries_, [](const auto &item)
        { return !item.second.wanted && item.second.artwork.status != ArtworkStatus::ready; });
    {
        const std::lock_guard lock(mutex_);
        std::erase_if(wanted,
                      [&](const Job &job)
                      {
                          return (current_ && current_->ticket == job.ticket) ||
                                 (complete_ && complete_->ticket == job.ticket);
                      });
        jobs_.swap(wanted);
    }
    wake_.notify_one();
}

bool ArtworkCache::make_room(std::size_t needed, ArtworkId except)
{
    if (needed > limits_.texture_bytes)
        return false;
    const std::size_t incoming_priority = entries_.at(except).priority;
    while (bytes_ > limits_.texture_bytes - needed)
    {
        auto victim = entries_.end();
        for (auto it = entries_.begin(); it != entries_.end(); ++it)
        {
            const Entry &entry = it->second;
            if (it->first == except || entry.bytes == 0 ||
                (entry.wanted && entry.priority <= incoming_priority))
                continue;
            if (victim == entries_.end() ||
                (entry.wanted != victim->second.wanted ? !entry.wanted
                 : entry.wanted                        ? entry.priority > victim->second.priority
                                                       : entry.touched < victim->second.touched))
                victim = it;
        }
        if (victim == entries_.end())
            return false;
        release_entry(victim->second);
        entries_.erase(victim);
    }
    return true;
}

void ArtworkCache::pump(std::size_t uploads)
{
    while (uploads-- != 0)
    {
        std::optional<Completion> result;
        {
            const std::lock_guard lock(mutex_);
            result.swap(complete_);
        }
        wake_.notify_one();
        if (!result)
            break;

        const auto found = entries_.find(result->id);
        if (found == entries_.end() || found->second.ticket != result->ticket ||
            !found->second.wanted)
            continue;
        Entry &entry = found->second;
        const Pixels &pixels = result->pixels;
        entry.artwork.status = ArtworkStatus::unavailable;
        if (pixels.width < 1 || pixels.height < 1 || pixels.width > limits_.max_dimension ||
            pixels.height > limits_.max_dimension)
            continue;
        const auto size = static_cast<std::size_t>(pixels.width) * pixels.height * 4;
        if (pixels.rgba.size() != size)
            continue;
        if (!make_room(size, result->id))
        {
            entry.artwork.status = ArtworkStatus::missing;
            continue;
        }
        if (const auto texture = upload_(pixels); texture != 0)
        {
            entry.artwork = {ArtworkStatus::ready, texture,
                             static_cast<float>(pixels.width) / static_cast<float>(pixels.height),
                             pixels.brightness};
            entry.bytes = size;
            bytes_ += size;
            peak_bytes_ = std::max(peak_bytes_, bytes_);
        }
    }
}

Artwork ArtworkCache::get(ArtworkId id) const
{
    const auto entry = entries_.find(id);
    return entry == entries_.end() ? Artwork{} : entry->second.artwork;
}

} // namespace hui::gfx
