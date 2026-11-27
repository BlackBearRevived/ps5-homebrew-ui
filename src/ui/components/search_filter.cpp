// ps5-homebrew-ui - Search panel with draft edits and explicit apply.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components/search_filter.hpp"

#include "ui/components/overlay.hpp"

#include <algorithm>
#include <utility>

namespace hui::ui
{

SearchFilterPanel::SearchFilterPanel()
{
    search_.set_placeholder("Search titles");
    search_.style.exits.down = true;
    search_.style.max_rows = 2;
    tags_.set_title("Filters");
    tags_.style.exits.up = tags_.style.exits.down = true;
    prompt_.set_title("Search titles");
    prompt_.style.allow_empty = true;
    prompt_.style.max_length = 80;
    apply_.label = "Apply";
    clear_.label = "Clear filters";
    clear_.style.role = ButtonRole::secondary;
    layout();
}

void SearchFilterPanel::set_options(std::vector<TagOption> tags,
                                    std::vector<std::string> sort_options)
{
    tags_.set_options(std::move(tags));
    sort_.set_options(std::move(sort_options));
}

void SearchFilterPanel::set_bounds(gfx::Rect bounds)
{
    bounds_ = bounds;
    layout();
}

void SearchFilterPanel::layout()
{
    const float width = std::min(bounds_.w, 940.0f);
    const float height = std::min(bounds_.h, 740.0f);
    panel_ = {bounds_.cx() - width * 0.5f, bounds_.cy() - height * 0.5f, width, height};
    const gfx::Rect content = panel_.inset(32.0f);
    search_.set_bounds({content.x, content.y + 68.0f, content.w, 200.0f});
    tags_.set_bounds({content.x, content.y + 284.0f, content.w, 194.0f});
    sort_.set_bounds({content.x + 140.0f, content.y + 498.0f, content.w - 140.0f, 62.0f});
    const float buttons = content.y + content.h - 64.0f;
    clear_.set_bounds({content.x, buttons, 230.0f, 64.0f});
    apply_.set_bounds({content.x + content.w - 220.0f, buttons, 220.0f, 64.0f});
    prompt_.set_bounds(bounds_);
}

void SearchFilterPanel::use_filters(const SearchFilters &value)
{
    search_.set_text(value.query);
    search_.refresh();
    const auto &options = tags_.options();
    for (std::size_t index = 0; index < options.size(); ++index)
        tags_.set_selected(static_cast<int>(index),
                           std::ranges::find(value.tags, options[index].tag) != value.tags.end());
    sort_.set_index(value.sort);
}

SearchFilters SearchFilterPanel::draft() const
{
    SearchFilters result;
    result.query = search_.text();
    result.sort = sort_.index();
    for (int index : tags_.selection())
        result.tags.push_back(tags_.options()[static_cast<std::size_t>(index)].tag);
    return result;
}

void SearchFilterPanel::open(const SearchFilters &applied, Feedback &feedback)
{
    open_ = true;
    prompt_.dismiss();
    use_filters(applied);
    clear_focused_ = false;
    focus(Part::search);
    play_cue(feedback, style, style.sounds.open, bounds_.cx());
}

void SearchFilterPanel::dismiss()
{
    open_ = false;
    prompt_.dismiss();
}

void SearchFilterPanel::focus(Part part, Feedback *feedback)
{
    part_ = part;
    search_.set_active(part == Part::search);
    tags_.set_active(part == Part::tags);
    sort_.set_active(part == Part::sort);
    apply_.set_active(part == Part::buttons && !clear_focused_);
    clear_.set_active(part == Part::buttons && clear_focused_);
    if (feedback)
        play_cue(*feedback, style, style.sounds.move, panel_.cx());
}

Event SearchFilterPanel::handle(const InputFrame &input, Feedback &feedback)
{
    if (!open_)
        return Event::none;
    if (prompt_.is_open())
    {
        if (prompt_.handle(input, feedback) == Event::activated)
        {
            search_.set_text(prompt_.text());
            return Event::changed;
        }
        return Event::none;
    }
    if (input.is_pressed(Action::back))
    {
        dismiss();
        play_cue(feedback, style, style.sounds.close, panel_.cx());
        return Event::cancelled;
    }
    if (part_ == Part::search)
    {
        const Event event = search_.handle(input, feedback);
        if (search_.exit() == Direction::down)
            focus(tags_.options().empty() ? Part::sort : Part::tags, &feedback);
        else if (event == Event::activated)
        {
            if (!search_.has_pick())
                prompt_.open(feedback, search_.text());
            return Event::changed;
        }
        return event;
    }
    if (part_ == Part::tags)
    {
        const Event event = tags_.handle(input, feedback);
        if (tags_.exit() == Direction::up)
            focus(Part::search, &feedback);
        else if (tags_.exit() == Direction::down)
            focus(Part::sort, &feedback);
        return event;
    }
    if (part_ == Part::sort)
    {
        if (input.nav == Direction::up || input.nav == Direction::down)
        {
            focus(input.nav == Direction::down ? Part::buttons
                  : tags_.options().empty()    ? Part::search
                                               : Part::tags,
                  &feedback);
            return Event::moved;
        }
        return sort_.handle(input, feedback);
    }
    if (input.nav == Direction::up)
    {
        focus(Part::sort, &feedback);
        return Event::moved;
    }
    if (input.nav == Direction::left || input.nav == Direction::right)
    {
        clear_focused_ = input.nav == Direction::left;
        focus(Part::buttons, &feedback);
        return Event::moved;
    }
    if (clear_focused_)
    {
        if (clear_.handle(input, feedback) == Event::activated)
        {
            use_filters({});
            return Event::changed;
        }
    }
    else if (apply_.handle(input, feedback) == Event::activated)
    {
        search_.add_recent(search_.text());
        dismiss();
        return Event::activated;
    }
    return Event::none;
}

void SearchFilterPanel::update(float dt)
{
    if (!open_)
        return;
    for (ComponentStyle *child :
         {static_cast<ComponentStyle *>(&search_.style),
          static_cast<ComponentStyle *>(&tags_.style), static_cast<ComponentStyle *>(&sort_.style),
          static_cast<ComponentStyle *>(&apply_.style),
          static_cast<ComponentStyle *>(&clear_.style),
          static_cast<ComponentStyle *>(&prompt_.style)})
        *child = style;
    search_.update(dt);
    tags_.update(dt);
    sort_.update(dt);
    apply_.update(dt);
    clear_.update(dt);
    prompt_.update(dt);
}

void SearchFilterPanel::draw(Canvas &canvas) const
{
    if (!open_)
        return;
    canvas.list.rounded_rect(bounds_, 0.0f, gfx::Color::rgb(0x000000, 0.68f));
    draw_overlay_panel(canvas, style.theme, panel_, false);
    Painter paint(canvas.list, canvas.fonts, style.theme);
    paint.heading("Search and filter", panel_.x + 32.0f, panel_.y + 68.0f, 40.0f);
    paint.label("Sort", panel_.x + 32.0f, sort_.bounds().cy() + 8.0f, 24.0f, style.theme.text);
    const std::string results = std::to_string(count_) + (count_ == 1 ? " result" : " results");
    paint.body(results, panel_.cx(), apply_.bounds().cy() + 8.0f, 24.0f, style.theme.text_muted,
               gfx::Align::center);
    search_.draw(canvas);
    tags_.draw(canvas);
    sort_.draw(canvas);
    apply_.draw(canvas);
    clear_.draw(canvas);
    prompt_.draw(canvas);
}

} // namespace hui::ui
