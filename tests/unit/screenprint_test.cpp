// ps5-homebrew-ui - Directional lighting, activity ownership, and playable chapter checks.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "concepts/concepts.hpp"
#include "ui/components/activity_pill.hpp"
#include "ui/components/carousel.hpp"

namespace
{
using namespace hui;
using NewDesigns = hui::testing::ComponentFixture;

TEST_F(NewDesigns, LightingReversesAtTheShelfEndsAndObeysReducedMotion)
{
    ui::Carousel shelf;
    shelf.style.card.directional_light = true;
    shelf.style.theme = *ui::find_theme("cinema");
    std::vector<ui::CardItem> cards(2);
    shelf.set_items(cards);
    shelf.set_bounds({96, 100, 1200, 400});
    const auto light = [&]()
    {
        auto c = canvas();
        shelf.draw(c);
        for (const auto &i : list_.instances())
            if (i.extra[1] == 1.f && i.params[3] == 0.f && i.color_top[3] != i.color_bottom[3])
                return i.color_top[3] - i.color_bottom[3];
        return 0.f;
    };
    EXPECT_EQ(shelf.handle(nav(Direction::right), feedback_), ui::Event::moved);
    EXPECT_GT(light(), 0);
    EXPECT_EQ(shelf.handle(nav(Direction::left), feedback_), ui::Event::moved);
    EXPECT_LT(light(), 0);
    shelf.style.reduced_motion = true;
    EXPECT_EQ(light(), 0);
}

TEST_F(NewDesigns, ActivityUpdatesPreserveFocusAndRetryTargetsTheFailedJob)
{
    ui::ActivityPill pill;
    ui::TransferJob job{42, "Archive", ui::TransferState::active, .62f, "Downloading"};
    pill.set_job(job);
    EXPECT_FALSE(pill.expanded());
    EXPECT_EQ(pill.handle(press(Action::north), feedback_), ui::Event::changed);
    job.state = ui::TransferState::failed;
    job.detail = "Source unavailable";
    pill.set_job(job);
    ASSERT_TRUE(pill.expanded());
    EXPECT_EQ(pill.handle(press(Action::confirm), feedback_), ui::Event::activated);
    EXPECT_EQ(pill.job().id, 42u);
    for (const auto &theme : ui::themes())
    {
        pill.style.theme = theme;
        pill.style.reduced_motion = true;
        pill.update(kFrame);
        auto c = canvas();
        pill.draw(c);
        expect_drawn(theme.name);
    }
    EXPECT_EQ(pill.handle(press(Action::back), feedback_), ui::Event::cancelled);
    EXPECT_FALSE(pill.expanded());
    pill.handle(press(Action::north), feedback_);
    job.id = 43;
    pill.set_job(job);
    EXPECT_FALSE(pill.expanded());
}

TEST_F(NewDesigns, FerrySignalRejectsWrongChannelAndReturnsToSelectedChapter)
{
    auto design = concepts::make_chapters(context_);
    design->enter();
    send(*design, nav(Direction::down));
    send(*design, press(Action::confirm));
    send(*design, press(Action::confirm));
    EXPECT_TRUE(played(audio::Cue::error));
    for (int i = 0; i < 4; ++i)
        send(*design, nav(Direction::right));
    send(*design, press(Action::confirm));
    EXPECT_TRUE(played(audio::Cue::select));
    send(*design, press(Action::back));
    send(*design, press(Action::confirm));
    for (int i = 0; i < 4; ++i)
        send(*design, nav(Direction::right));
    send(*design, press(Action::confirm));
    EXPECT_TRUE(played(audio::Cue::select));
    expect_drawable(*design);
}
} // namespace
