// ps5-homebrew-ui - Bounded, asynchronous artwork loading for a catalog.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <pthread.h>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace hui::gfx
{

using ArtworkId = std::uint64_t;

struct ArtworkRequest
{
    ArtworkId id;
    std::string path;
};

enum class ArtworkStatus : std::uint8_t
{
    missing,
    pending,
    ready,
    unavailable,
};

struct Artwork
{
    ArtworkStatus status = ArtworkStatus::missing;
    std::uint32_t texture = 0;
    float aspect = 1.0f;
    float brightness = 0.0f;
};

struct ArtworkLimits
{
    std::size_t texture_bytes = 24u << 20;
    std::size_t requests = 48;
    int max_dimension = 1024;
};

// Own on the render thread. Decode runs on a single worker. Texture creation
// and deletion only call the supplied backend on the owning thread. Handle
// lifetimes end when pump() or select() evicts an entry, so look up each frame.
class ArtworkCache
{
  public:
    struct Pixels
    {
        int width = 0;
        int height = 0;
        std::vector<std::uint8_t> rgba;
        float brightness = 0.0f;
    };

    using Upload = std::function<std::uint32_t(const Pixels &)>;
    using Release = std::function<void(std::uint32_t)>;
    using Decode = std::function<Pixels(const std::string &, int)>;

    ArtworkCache(Upload upload, Release release, ArtworkLimits limits = {}, Decode decode = {});
    ~ArtworkCache();
    ArtworkCache(const ArtworkCache &) = delete;
    ArtworkCache &operator=(const ArtworkCache &) = delete;

    // Highest priority first. Changing the path for an ID invalidates it.
    // Jobs outside the window are cancelled, including completions in flight.
    void select(std::span<const ArtworkRequest> requests);
    // Mark only a failed request for another attempt on the next select().
    void retry_unavailable(ArtworkId id);
    void pump(std::size_t uploads = 1);
    Artwork get(ArtworkId id) const;
    std::size_t bytes() const
    {
        return bytes_;
    }
    std::size_t peak_bytes() const
    {
        return peak_bytes_;
    }

  private:
    struct Entry
    {
        Artwork artwork;
        std::string path;
        std::uint64_t ticket = 0;
        std::uint64_t touched = 0;
        std::size_t bytes = 0;
        bool wanted = false;
        std::size_t priority = 0; // smaller is closer to focus
    };

    struct Job
    {
        ArtworkRequest request;
        std::uint64_t ticket;
    };

    struct Completion
    {
        ArtworkId id;
        std::uint64_t ticket;
        Pixels pixels;
    };

    static void *worker_entry(void *self);
    void worker();
    void release_entry(const Entry &entry);
    bool make_room(std::size_t bytes, ArtworkId except);

    Upload upload_;
    Release release_;
    Decode decode_;
    ArtworkLimits limits_;
    std::unordered_map<ArtworkId, Entry> entries_;
    std::uint64_t ticket_ = 0;
    std::uint64_t clock_ = 0;
    std::size_t bytes_ = 0;
    std::size_t peak_bytes_ = 0;

    pthread_t thread_{};
    bool started_ = false;
    bool stopping_ = false;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> jobs_;
    std::optional<Job> current_;
    std::optional<Completion> complete_;
};

} // namespace hui::gfx
