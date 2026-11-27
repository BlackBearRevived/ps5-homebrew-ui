// ps5-homebrew-ui - Contrast, input glyphs and decorative motion regressions.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"
#include "ui/glyphs.hpp"
#include "ui/theme.hpp"

#include <algorithm>
#include <iterator>

namespace
{

class RoadmapCorrectness : public hui::testing::ConceptFixture
{
};

TEST_F(RoadmapCorrectness, ThemeTextContrast)
{
    using namespace hui;
    for (const ui::Theme &theme : ui::themes())
    {
        SCOPED_TRACE(theme.id);
        const auto surface = gfx::mix(theme.page, theme.surface, theme.surface.a);
        const auto primary = gfx::mix(surface, theme.primary, theme.primary.a);
        EXPECT_GE(ui::text_contrast(theme.text_muted, surface), 4.5f);
        EXPECT_GE(ui::text_contrast(theme.on_primary, primary), 4.5f);
    }
}

TEST_F(RoadmapCorrectness, AccessibilitySettingsRoundTripAndVersionOneStillLoads)
{
    settings_.large_text = true;
    settings_.reduce_transparency = true;
    settings_.strong_focus = true;
    hui::Settings loaded;
    std::string data = hui::encode_settings(settings_);
    ASSERT_TRUE(hui::decode_settings(data, &loaded));
    for (const auto &theme : hui::ui::themes())
    {
        const auto legible = hui::ui::with_legibility(theme, loaded);
        EXPECT_GT(legible.text_scale, 1.0f);
        EXPECT_FLOAT_EQ(legible.surface.a, 1.0f);
        EXPECT_TRUE(legible.strong_focus);
        EXPECT_GE(legible.focus_width, 5.0f);
    }
    data.resize(data.size() - 3);
    data[0] = 1;
    ASSERT_TRUE(hui::decode_settings(data, &loaded));
    EXPECT_FALSE(loaded.large_text);
    EXPECT_FALSE(loaded.reduce_transparency);
    EXPECT_FALSE(loaded.strong_focus);
}

TEST_F(RoadmapCorrectness, LogicalGlyphUsesThePlayerMapping)
{
    using namespace hui;
    gfx::DrawList logical;
    gfx::DrawList physical;
    gfx::DrawList fixed;
    for (const bool swap : {false, true})
    {
        const ui::GlyphStyle style = ui::GlyphStyle::dark(swap);
        logical.clear();
        physical.clear();
        fixed.clear();
        ui::draw_button(logical, fonts_, style, ui::Button::confirm, 0, 30, 40);
        ui::draw_button(physical, fonts_, style, ui::button_for(Action::confirm, swap), 0, 30, 40);
        ui::draw_button(fixed, fonts_, style, ui::Button::cross, 0, 30, 40);
        EXPECT_EQ(logical.instances().size(), physical.instances().size());
        EXPECT_EQ(logical.instances().size() == fixed.instances().size(), !swap);
    }
}

TEST_F(RoadmapCorrectness, ReducedMotionKeepsTheSettledGalleryBackdropStill)
{
    settings_.reduced_motion = true;
    auto gallery = hui::concepts::make_components(context_);
    gallery->enter();
    send(*gallery, idle(), 2.0f);
    frame_.reset();
    gallery->draw(frame_);
    const auto first = frame_.scene.instances();
    const float time = frame_.backdrop.time;
    send(*gallery, idle(), 1.0f);
    frame_.reset();
    gallery->draw(frame_);
    EXPECT_EQ(frame_.backdrop.time, time);
    ASSERT_TRUE(frame_.glass);
    ASSERT_GE(first.size(), 4u);
    ASSERT_EQ(frame_.scene.instances().size(), first.size());
    for (std::size_t i = 0; i < first.size(); ++i)
        EXPECT_TRUE(std::equal(std::begin(first[i].rect), std::end(first[i].rect),
                               std::begin(frame_.scene.instances()[i].rect)));
}

} // namespace
