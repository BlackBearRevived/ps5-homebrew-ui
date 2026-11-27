// ps5-homebrew-ui - Flagship component and artwork acceptance tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "gfx/artwork_cache.hpp"
#include "ui/components/artwork_detail.hpp"
#include "ui/components/caption_tracks.hpp"
#include "ui/components/search_filter.hpp"
#include "ui/components/shelf_browser.hpp"
#include "ui/components/transfer_center.hpp"
#include "ui/components/transition.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

namespace
{
using namespace hui;
using ui::Event;

class RoadmapComponents : public hui::testing::ComponentFixture
{
};

TEST_F(RoadmapComponents, RestoresShelfAndItemAcrossMovesAndReorders)
{
    ui::ShelfBrowser browser;
    ui::Shelf first{1, "First", {}};
    ui::Shelf second{2, "Second", {}};
    for (std::uint64_t id = 1; id <= 1100; ++id)
    {
        ui::CardItem card;
        card.title = std::to_string(id);
        (id < 501 ? first : second).items.push_back({id, std::move(card)});
    }
    browser.set_shelves({first, second});
    ASSERT_TRUE(browser.select(2, 1000));
    EXPECT_EQ(browser.handle(nav(Direction::up), feedback_), Event::moved);
    EXPECT_EQ(browser.handle(nav(Direction::down), feedback_), Event::moved);
    EXPECT_EQ(browser.selection()->item, 1000u);
    std::ranges::reverse(second.items);
    browser.set_shelves({second, first});
    EXPECT_EQ(browser.selection()->item, 1000u);
    const auto index = browser.selection()->item_index;
    second.items.erase(second.items.begin() + index);
    browser.set_shelves({first, second});
    EXPECT_EQ(browser.selection()->shelf, 2u);
    EXPECT_NE(browser.selection()->item, 1000u);
    browser.set_shelves({});
    EXPECT_FALSE(browser.selection());
}

TEST_F(RoadmapComponents, ShelfClipIncludesFocusedArtworkAtBothPageEdges)
{
    ui::ShelfBrowser browser;
    browser.style.carousel.mode = ui::CarouselMode::paged;
    browser.style.carousel.per_page = 6;
    browser.style.carousel.peek = 0;
    browser.style.carousel.card.focus_scale = 1.022f;
    browser.set_bounds({96, 472, 1728, 505});
    ui::Shelf shelf{1, "Adventures", {}};
    for (std::uint32_t id = 1; id <= 12; ++id)
    {
        ui::CardItem item;
        item.texture = 100 + id;
        shelf.items.push_back({id, std::move(item)});
    }
    browser.set_shelves({std::move(shelf)});
    for (std::uint32_t id : {1u, 6u, 7u, 12u})
    {
        ASSERT_TRUE(browser.select(1, id));
        auto target = canvas();
        browser.draw(target);
        const auto &runs = list_.runs();
        const auto run = std::ranges::find(runs, 100 + id, &gfx::Run::texture);
        ASSERT_NE(run, runs.end()) << id;
        ASSERT_TRUE(run->clipped);
        const auto art = browser.focused_art();
        EXPECT_LT(run->clip.x, art.x) << id;
        EXPECT_GT(run->clip.x + run->clip.w, art.x + art.w) << id;
    }
}

TEST_F(RoadmapComponents, ReversingTheHeroPreservesItsPoseAndReturns)
{
    ui::SharedElementTransition transition;
    const gfx::Rect cover{105, 650, 270, 150};
    const gfx::Rect detail{96, 130, 1728, 642};
    transition.set_bounds(cover, detail);
    transition.set_expanded(true);
    transition.update(0.13f);
    const auto pose = transition.bounds();
    EXPECT_GT(pose.w, cover.w);
    EXPECT_LT(pose.w, detail.w);
    transition.set_expanded(false);
    EXPECT_FLOAT_EQ(transition.bounds().w, pose.w);
    EXPECT_FLOAT_EQ(transition.bounds().y, pose.y);
    for (int i = 0; i < 90; ++i)
        transition.update(kFrame);
    EXPECT_FALSE(transition.running());
    EXPECT_FLOAT_EQ(transition.bounds().w, cover.w);
}

TEST_F(RoadmapComponents, CaptionShortcutAndBackPreserveTheTransportFocus)
{
    ui::MediaControls transport;
    transport.style.tracks_action = Action::l3;
    transport.set_focus(ui::MediaControl::scrubber);
    EXPECT_EQ(transport.handle(press(Action::l3), feedback_), Event::activated);
    EXPECT_EQ(transport.command(), ui::MediaCommand::tracks);
    ui::CaptionTrackPicker picker;
    picker.set_tracks({"Stereo", "Commentary"}, {"English"});
    picker.open(transport, {}, feedback_);
    ASSERT_TRUE(picker.is_open());
    EXPECT_EQ(picker.handle(transport, nav(Direction::right), feedback_), Event::changed);
    EXPECT_EQ(picker.options().audio, 1);
    EXPECT_EQ(picker.handle(transport, press(Action::back), feedback_), Event::cancelled);
    EXPECT_FALSE(picker.is_open());
    EXPECT_EQ(transport.focus(), ui::MediaControl::scrubber);
}

TEST_F(RoadmapComponents, TransferCancelChecksTheLiveJobAfterConfirmation)
{
    ui::TransferCenter center;
    std::vector<ui::TransferJob> jobs{{10, "Cover art", ui::TransferState::active, 0.1f, "Reading"},
                                      {11, "Manual", ui::TransferState::failed, 0.2f, "Timed out"}};
    std::optional<ui::TransferAction> command;
    std::uint64_t id = 0;
    center.on_action = [&](std::uint64_t item, ui::TransferAction action)
    {
        id = item;
        command = action;
    };
    center.set_jobs(jobs);
    EXPECT_EQ(center.handle(press(Action::confirm), feedback_), Event::activated);
    EXPECT_EQ(command, ui::TransferAction::pause);
    EXPECT_EQ(id, 10u);
    command.reset();
    center.handle(press(Action::north), feedback_);
    EXPECT_EQ(center.handle(press(Action::confirm), feedback_), Event::activated);
    EXPECT_FALSE(command); // the safe choice opens focused
    center.handle(press(Action::north), feedback_);
    EXPECT_EQ(center.handle(nav(Direction::right), feedback_), Event::moved);
    jobs[0].state = ui::TransferState::completed;
    std::ranges::reverse(jobs);
    center.set_jobs(jobs);
    EXPECT_EQ(center.selected()->id, 10u);
    EXPECT_EQ(center.handle(press(Action::confirm), feedback_), Event::activated);
    EXPECT_FALSE(command); // the selected transfer is complete by now
}

TEST_F(RoadmapComponents, SearchFilterEditsStayInTheDraftUntilApply)
{
    ui::SearchFilterPanel filter;
    ui::TagOption installed{"Installed"};
    installed.tag = 8;
    filter.set_options({installed}, {"Catalog order", "Title A to Z"});
    ui::SearchFilters applied;
    filter.open(applied, feedback_);
    filter.handle(nav(Direction::down), feedback_);
    EXPECT_EQ(filter.handle(press(Action::confirm), feedback_), Event::changed);
    EXPECT_EQ(filter.draft().tags, std::vector<int>{8});
    EXPECT_TRUE(applied.tags.empty());
    EXPECT_EQ(filter.handle(press(Action::back), feedback_), Event::cancelled);
    filter.open(applied, feedback_);
    EXPECT_TRUE(filter.draft().tags.empty());
    filter.handle(nav(Direction::down), feedback_);
    filter.handle(press(Action::confirm), feedback_);
    filter.handle(nav(Direction::down), feedback_);
    filter.handle(nav(Direction::right), feedback_);
    filter.handle(nav(Direction::down), feedback_);
    EXPECT_EQ(filter.handle(press(Action::confirm), feedback_), Event::activated);
    EXPECT_EQ(filter.draft().tags, std::vector<int>{8});
    EXPECT_EQ(filter.draft().sort, 1);
}

TEST(ArtworkCache, DiscardsStaleDecodesAndKeepsTextureUsageBounded)
{
    using namespace std::chrono_literals;
    const auto render = std::this_thread::get_id();
    std::atomic<bool> old_started = false;
    std::atomic<bool> release_old = false;
    std::vector<int> uploaded;
    std::vector<std::uint32_t> released;
    gfx::ArtworkCache cache(
        [&](const gfx::ArtworkCache::Pixels &pixels)
        {
            EXPECT_EQ(std::this_thread::get_id(), render);
            uploaded.push_back(pixels.rgba.front());
            return static_cast<std::uint32_t>(uploaded.size());
        },
        [&](std::uint32_t texture) { released.push_back(texture); }, {16, 8, 16},
        [&](const std::string &name, int)
        {
            if (name == "old")
            {
                old_started.store(true);
                while (!release_old.load())
                    std::this_thread::sleep_for(1ms);
            }
            if (name == "missing")
                return gfx::ArtworkCache::Pixels{};
            const std::uint8_t marker = name == "replacement" ? 2 : 1;
            return gfx::ArtworkCache::Pixels{2, 2, std::vector<std::uint8_t>(16, marker), 0.0f};
        });
    const auto wait = [&](auto predicate)
    {
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        while (!predicate() && std::chrono::steady_clock::now() < deadline)
        {
            cache.pump();
            std::this_thread::sleep_for(1ms);
        }
        return predicate();
    };
    const gfx::ArtworkRequest old[] = {{42, "old"}};
    cache.select(old);
    const bool started = wait([&] { return old_started.load(); });
    const gfx::ArtworkRequest new_job[] = {{42, "replacement"}, {43, "missing"}};
    cache.select(new_job);
    release_old.store(true);
    ASSERT_TRUE(started);
    ASSERT_TRUE(wait([&] { return cache.get(43).status == gfx::ArtworkStatus::unavailable; }));
    EXPECT_EQ(uploaded, std::vector<int>{2});
    EXPECT_EQ(cache.get(42).status, gfx::ArtworkStatus::ready);
    const gfx::ArtworkRequest next[] = {{44, "another"}, {42, "replacement"}};
    cache.select(next);
    ASSERT_TRUE(wait([&] { return cache.get(44).status == gfx::ArtworkStatus::ready; }));
    EXPECT_EQ(cache.get(42).texture, 0u);
    EXPECT_EQ(released, std::vector<std::uint32_t>{1});
    EXPECT_EQ(cache.peak_bytes(), 16u);
}

} // namespace
