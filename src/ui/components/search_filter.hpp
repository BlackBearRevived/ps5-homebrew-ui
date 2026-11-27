// ps5-homebrew-ui - Transactional search, filters and sorting for a catalog.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui/components/button.hpp"
#include "ui/components/choice.hpp"
#include "ui/components/input_prompt.hpp"
#include "ui/components/search_field.hpp"
#include "ui/components/tag_select.hpp"

namespace hui::ui
{

struct SearchFilters
{
    std::string query;
    std::vector<int> tags; // TagOption::tag values
    int sort = 0;

    bool operator==(const SearchFilters &) const = default;
};

// Open with the applied filters. On activation, replace them with draft().
//   panel.open(filters, feedback);
//   panel.set_result_count(count_matches(panel.draft()));
class SearchFilterPanel
{
  public:
    ComponentStyle style;

    SearchFilterPanel();
    void set_options(std::vector<TagOption> tags, std::vector<std::string> sort_options);
    void set_bounds(gfx::Rect bounds);
    void open(const SearchFilters &applied, Feedback &feedback);
    bool is_open() const
    {
        return open_;
    }
    void dismiss();
    SearchFilters draft() const;
    void set_result_count(std::size_t count)
    {
        count_ = count;
    }
    Event handle(const InputFrame &input, Feedback &feedback);
    void update(float dt);
    void draw(Canvas &canvas) const;

  private:
    enum class Part : std::uint8_t
    {
        search,
        tags,
        sort,
        buttons,
    };
    void focus(Part part, Feedback *feedback = nullptr);
    void use_filters(const SearchFilters &value);
    void layout();

    gfx::Rect bounds_{0, 0, 1920, 1080};
    gfx::Rect panel_{};
    SearchField search_;
    InputPrompt prompt_;
    TagSelect tags_;
    ChoicePicker sort_;
    PushButton apply_;
    PushButton clear_;
    Part part_ = Part::search;
    bool clear_focused_ = false;
    bool open_ = false;
    std::size_t count_ = 0;
};

} // namespace hui::ui
