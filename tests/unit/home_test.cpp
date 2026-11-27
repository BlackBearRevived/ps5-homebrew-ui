// ps5-homebrew-ui - Opening screen navigation, previews and saved settings.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/shell.hpp"
#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"
#include "ui/theme.hpp"

#include <filesystem>

namespace
{
using namespace hui;
using HomeTest = hui::testing::ConceptFixture;

TEST_F(HomeTest, SelectedThemeSurvivesHandoffBackAndRelaunch)
{
    const auto dir = std::filesystem::path(::testing::TempDir()) / "hui_home_resume";
    std::filesystem::create_directories(dir);
    std::filesystem::remove(dir / "settings.bin");
    app::Shell shell(fonts_, catalog_, dir.string(), 0);
    const auto current = [&]()
    { return std::string_view(shell.concept_at(shell.current()).info().id); };
    EXPECT_EQ(current(), "home");
    shell.update(press(Action::jump_next), 0.016f);
    EXPECT_EQ(shell.settings().theme_id, ui::themes()[1].id);
    shell.update(press(Action::confirm), 0.016f);
    EXPECT_EQ(current(), "themes");
    shell.update(press(Action::jump_next), 0.016f);
    EXPECT_EQ(shell.settings().theme_id, ui::themes()[2].id);
    shell.update(press(Action::back), 0.016f);
    EXPECT_EQ(current(), "home");
    shell.update(idle(), 2.f);
    app::Shell reopened(fonts_, catalog_, dir.string(), 0);
    EXPECT_STREQ(reopened.concept_at(reopened.current()).info().id, "home");
    EXPECT_STREQ(reopened.concept_at(reopened.settings().concept_index).info().id, "themes");
    EXPECT_EQ(reopened.settings().theme_id, ui::themes()[2].id);
    shell.update(nav(Direction::down), 0.016f);
    shell.update(press(Action::confirm), 0.016f);
    EXPECT_EQ(current(), "components");
    shell.update(press(Action::touch), 0.016f);
    shell.update(press(Action::confirm), 0.016f);
    EXPECT_EQ(current(), "home");
}

TEST_F(HomeTest, PreviewOwnsConfirmAndAllThemesDrawWithAccessibilitySettings)
{
    const auto home = concepts::make_home(context_);
    settings_.large_text = settings_.strong_focus = settings_.reduced_motion = true;
    home->enter();
    send(*home, press(Action::west));
    send(*home, nav(Direction::down));
    send(*home, nav(Direction::right));
    for (std::size_t i = 0; i < ui::themes().size(); ++i)
    {
        send(*home, press(Action::jump_next));
        send(*home, press(Action::confirm));
        EXPECT_TRUE(context_.requested_design.empty());
        expect_drawable(*home);
    }
    EXPECT_EQ(settings_.theme_id, "acrylic");
    send(*home, press(Action::back));
    send(*home, press(Action::confirm));
    EXPECT_EQ(context_.requested_design, "themes");
}

TEST_F(HomeTest, OlderSavesUseAcrylicAndPreserveGalleryAndAccessibilityPreferences)
{
    settings_.large_text = true;
    settings_.concept_index = 18;
    std::string legacy = encode_settings(settings_).substr(0, 15);
    legacy[0] = 2;
    Settings read;
    ASSERT_TRUE(decode_settings(legacy, &read));
    EXPECT_TRUE(read.large_text);
    EXPECT_EQ(read.concept_index, 18);
    EXPECT_EQ(read.theme_id, "acrylic");
    auto truncated = encode_settings(settings_);
    truncated.pop_back();
    EXPECT_FALSE(decode_settings(truncated, &read));
}
} // namespace
