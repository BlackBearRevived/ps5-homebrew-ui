// ps5-homebrew-ui - Offline country navigation, directory and confirmation tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "component_fixture.hpp"
#include "ui/components/country_picker.hpp"

#include <set>

namespace
{
using namespace hui;
using AtlasTest = hui::testing::ComponentFixture;

TEST_F(AtlasTest, EveryCountryAndTerritoryIsReachableOnTheMapWithTheDpad)
{
    ui::CountryPicker picker;
    std::set<std::string_view> seen{picker.focused().code};
    std::vector<std::string_view> pending{picker.focused().code};
    while (!pending.empty())
    {
        const auto start = pending.back();
        pending.pop_back();
        for (auto dir : {Direction::left, Direction::right, Direction::up, Direction::down})
        {
            ASSERT_TRUE(picker.set_focus(start));
            picker.handle(nav(dir), feedback_);
            const auto code = picker.focused().code;
            if (seen.insert(code).second)
                pending.push_back(code);
            feedback_.clear();
        }
    }
    EXPECT_EQ(seen.size(), 250u);
    EXPECT_EQ(seen.size(), picker.country_count());
    EXPECT_TRUE(seen.contains("VA"));
    EXPECT_TRUE(seen.contains("KI"));
    EXPECT_TRUE(seen.contains("XK"));
}

TEST_F(AtlasTest, SearchAndMapShareFocusButOnlyConfirmChangesTheSelection)
{
    ui::CountryPicker picker;
    ASSERT_TRUE(picker.set_selected("FJ"));
    EXPECT_FALSE(picker.set_focus("ZZ"));
    picker.handle(press(Action::north), feedback_);
    EXPECT_EQ(picker.view(), ui::AtlasView::directory);
    picker.set_query("nAuRu");
    ASSERT_EQ(picker.result_count(), 1u);
    EXPECT_EQ(picker.focused().code, "NR");
    EXPECT_EQ(picker.selected_code(), "FJ");
    picker.handle(press(Action::north), feedback_);
    EXPECT_EQ(picker.view(), ui::AtlasView::search);
    picker.set_query("xyzmissing");
    EXPECT_EQ(picker.result_count(), 0u);
    picker.handle(press(Action::back), feedback_);
    EXPECT_EQ(picker.query(), "nAuRu");
    EXPECT_EQ(picker.focused().code, "NR");
    picker.set_query("no matches here");
    EXPECT_EQ(picker.handle(press(Action::confirm), feedback_), ui::Event::refused);
    picker.set_query("us");
    EXPECT_EQ(picker.focused().code, "US");
    picker.set_query("côte d'ivoire");
    EXPECT_EQ(picker.focused().code, "CI");
    picker.set_query("kiribati");
    EXPECT_EQ(picker.handle(press(Action::confirm), feedback_), ui::Event::activated);
    EXPECT_EQ(picker.selected_code(), "KI");
    EXPECT_EQ(picker.view(), ui::AtlasView::map);
    EXPECT_EQ(picker.handle(press(Action::back), feedback_), ui::Event::cancelled);
}

TEST_F(AtlasTest, ControllerKeyboardCanTypeEraseAndReturnToTheResults)
{
    ui::CountryPicker picker;
    picker.handle(press(Action::north), feedback_);
    picker.handle(press(Action::north), feedback_);
    picker.handle(press(Action::confirm), feedback_);
    EXPECT_EQ(picker.query(), "1");
    picker.handle(press(Action::west), feedback_);
    EXPECT_TRUE(picker.query().empty());
    picker.handle(nav(Direction::down), feedback_);
    picker.handle(press(Action::confirm), feedback_);
    picker.handle(nav(Direction::down), feedback_);
    picker.handle(press(Action::confirm), feedback_);
    EXPECT_EQ(picker.focused().code, "QA");
    for (int i = 0; i < 2; ++i)
        picker.handle(nav(Direction::down), feedback_);
    for (int i = 0; i < 4; ++i)
        picker.handle(nav(Direction::right), feedback_);
    picker.handle(press(Action::confirm), feedback_);
    EXPECT_EQ(picker.view(), ui::AtlasView::directory);
    EXPECT_TRUE(picker.selected_code().empty());
    picker.handle(press(Action::confirm), feedback_);
    EXPECT_EQ(picker.selected_code(), "QA");
}

TEST_F(AtlasTest, OfflineGeometryDrawsAcrossAllThemesAndZoomLevels)
{
    ui::CountryPicker picker;
    picker.set_bounds({96, 236, 1728, 716});
    ASSERT_TRUE(picker.set_focus("VA"));
    settings_.large_text = true;
    picker.style.reduced_motion = true;
    for (const auto &t : ui::themes())
    {
        picker.style.theme = ui::with_legibility(t, settings_);
        for (int zoom = 0; zoom < 3; ++zoom)
        {
            EXPECT_EQ(picker.zoom_level(), zoom);
            picker.update(kFrame);
            auto c = canvas();
            picker.draw(c);
            expect_drawn(t.name);
            EXPECT_FALSE(list_.mesh_vertices().empty());
            picker.handle(press(Action::west), feedback_);
        }
    }
}
} // namespace
