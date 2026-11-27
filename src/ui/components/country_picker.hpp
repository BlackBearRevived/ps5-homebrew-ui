// ps5-homebrew-ui - Offline geographic country and territory picker.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui/components/keyboard.hpp"
#include "ui/components/list.hpp"

namespace hui::ui
{
struct CountryLocation
{
    std::string_view code, name, region;
    float longitude, latitude;
};
enum class AtlasView
{
    map,
    directory,
    search
};

struct CountryPickerStyle : ComponentStyle
{
    bool graticule = true;
};

// picker.set_bounds(region); picker.set_selected("JP");
// if (picker.handle(input, feedback) == ui::Event::activated) save(picker.selected_code());
// Browsing moves focus only. A confirmed choice survives browsing or cancelling search.
class CountryPicker
{
  public:
    CountryPicker();
    CountryPicker(const CountryPicker &) = delete;
    CountryPicker &operator=(const CountryPicker &) = delete;
    CountryPickerStyle style;
    void set_bounds(gfx::Rect bounds);
    bool set_focus(std::string_view code);
    bool set_selected(std::string_view code);
    CountryLocation focused() const;
    std::string_view selected_code() const;
    AtlasView view() const
    {
        return view_;
    }
    int zoom_level() const
    {
        return zoom_;
    }
    static std::size_t country_count();
    // Search by English name, alias or country code. Empty text restores the full list.
    void set_query(std::string_view text);
    std::string_view query() const
    {
        return query_;
    }
    std::size_t result_count() const
    {
        return matches_.size();
    }
    void enter();
    Event handle(const InputFrame &input, Feedback &feedback);
    void update(float dt);
    void draw(Canvas &canvas) const;

  private:
    void layout();
    void filter();
    int neighbour(const InputFrame &input) const;
    void camera(bool snap, float dt);
    void draw_map(Canvas &canvas) const;
    void draw_panel(Canvas &canvas) const;
    gfx::Rect bounds_{96, 236, 1728, 716}, map_{}, panel_{};
    AtlasView view_ = AtlasView::map;
    ListView list_;
    Keyboard keys_;
    std::vector<int> matches_;
    std::string query_, original_query_;
    tween::Spring longitude_, latitude_, scale_, pin_x_, pin_y_;
    int guide_focus_ = -1;
    int guide_targets_[4]{};
    Pulse refused_;
    int focus_ = 0, selected_ = -1, zoom_ = 0, original_focus_ = 0;
};
} // namespace hui::ui
