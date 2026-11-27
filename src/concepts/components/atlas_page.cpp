// ps5-homebrew-ui - Interactive gallery page for the offline country picker.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concepts/components/page.hpp"
#include "ui/components/country_picker.hpp"

namespace hui::concepts::gallery
{
namespace
{
constexpr ui::Hint kMap[] = {{ui::Button::dpad, "Move"},
                             {ui::Button::confirm, "Select"},
                             {ui::Button::square, "Zoom"},
                             {ui::Button::triangle, "Directory"}};
constexpr ui::Hint kDirectory[] = {{ui::Button::dpad, "Browse"},
                                   {ui::Button::confirm, "Select"},
                                   {ui::Button::triangle, "Search"},
                                   {ui::Button::back, "Map"}};
constexpr ui::Hint kSearch[] = {{ui::Button::confirm, "Type"},
                                {ui::Button::square, "Erase"},
                                {ui::Button::triangle, "Space"},
                                {ui::Button::back, "Cancel"}};
constexpr app::TourStep kTour[] = {
    {.7f, 0, Direction::right, "atlas"},
    {.4f, action_bit(Action::west)},
    {.7f, action_bit(Action::confirm), Direction::none, "atlas-regional"},
    {.4f, action_bit(Action::north)},
    {.5f, 0, Direction::down, "atlas-directory"},
    {.3f, action_bit(Action::north)},
    {.5f, action_bit(Action::back), Direction::none, "atlas-search"},
    {.3f, action_bit(Action::confirm)},
    {.5f, action_bit(Action::west), Direction::none, "atlas-choice"},
    {.2f, action_bit(Action::west)}};
class AtlasPage final : public Page
{
  public:
    AtlasPage()
    {
        picker_.set_bounds(kPageArea);
    }
    const char *title() const override
    {
        return "World Atlas";
    }
    const char *summary() const override
    {
        return "Choose a country or territory on the map or in the directory.";
    }
    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        picker_.style.theme = theme;
        picker_.style.reduced_motion = reduced_motion;
    }
    void enter() override
    {
        picker_.enter();
    }
    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        picker_.handle(input, feedback);
        picker_.update(dt);
    }
    void draw(ui::Canvas &canvas) const override
    {
        picker_.draw(canvas);
    }
    std::span<const ui::Hint> hints() const override
    {
        if (picker_.view() == ui::AtlasView::search)
            return kSearch;
        if (picker_.view() == ui::AtlasView::directory)
            return kDirectory;
        return kMap;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    ui::CountryPicker picker_;
};
} // namespace
std::unique_ptr<Page> make_atlas_page(app::Context &)
{
    return std::make_unique<AtlasPage>();
}
} // namespace hui::concepts::gallery
