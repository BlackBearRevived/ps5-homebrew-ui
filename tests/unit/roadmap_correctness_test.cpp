// ps5-homebrew-ui - Contrast, input glyphs and decorative motion regressions.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"
#include "ui/glyphs.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

#include <algorithm>
#include <array>
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
        EXPECT_GE(ui::text_contrast(theme.action_text, surface), 4.5f);
        EXPECT_GE(ui::text_contrast(theme.focus, surface), 3.0f);
        EXPECT_GE(ui::text_contrast(ui::Painter::on(theme.accent), theme.accent), 4.5f);
        EXPECT_GE(ui::text_contrast(theme.on_secondary,
                                    gfx::mix(surface, theme.secondary, theme.secondary.a)),
                  4.5f);
    }
}

TEST_F(RoadmapCorrectness, ThemeLabMarkerFollowsEveryThemeAndWrapsInBothDirections)
{
    using namespace hui;
    const auto count = ui::themes().size();
    auto lab = concepts::make_themes(context_);
    lab->enter();
    const auto verify = [&](std::size_t index)
    {
        frame_.reset();
        lab->draw(frame_);
        const auto &shapes = frame_.overlay.instances();
        const auto is_marker = [](const gfx::Instance &shape)
        { return shape.rect[2] == 6 && shape.rect[3] == 22; };
        ASSERT_EQ(std::ranges::count_if(shapes, is_marker), 1);
        const auto marker = std::ranges::find_if(shapes, is_marker);
        EXPECT_FLOAT_EQ(marker->rect[0], 1818.0f - 14.0f * static_cast<float>(count - 1 - index));
    };
    send(*lab, press(Action::jump_prev));
    verify(count - 1);
    for (std::size_t index = 0; index < count; ++index)
    {
        settings_.large_text = index % 2 == 0;
        send(*lab, press(Action::jump_next));
        verify(index);
    }
}

TEST_F(RoadmapCorrectness, LargeTextEnlargesThemeLabControlsAndHints)
{
    using namespace hui;
    auto lab = concepts::make_themes(context_);
    lab->enter();
    send(*lab, idle());
    // Isolate labels by their controls so a growing text field cannot mask
    // a fixed-size tab, chip, or footer hint.
    constexpr gfx::Rect regions[] = {{135, 332, 237, 64}, {136, 432, 540, 58},  {716, 439, 128, 44},
                                     {136, 510, 340, 64}, {1256, 668, 528, 56}, {96, 975, 800, 64}};
    auto heights = [&]
    {
        frame_.reset();
        lab->draw(frame_);
        std::array<float, std::size(regions)> result{};
        for (const auto &shape : frame_.overlay.instances())
        {
            if (shape.params[3] != static_cast<float>(gfx::Shape::glyph))
                continue;
            const float x = shape.rect[0] + shape.rect[2] * 0.5f;
            const float y = shape.rect[1] + shape.rect[3] * 0.5f;
            for (std::size_t i = 0; i < result.size(); ++i)
            {
                const auto &r = regions[i];
                if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h)
                    result[i] = std::max(result[i], shape.rect[3]);
            }
        }
        return result;
    };
    const auto normal = heights();
    settings_.large_text = true;
    send(*lab, idle());
    const auto large = heights();
    for (std::size_t i = 0; i < normal.size(); ++i)
    {
        EXPECT_GT(normal[i], 0) << i;
        EXPECT_GT(large[i], normal[i] * 1.15f) << i;
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
    data.resize(12); // version 1 ends with the gallery index
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
