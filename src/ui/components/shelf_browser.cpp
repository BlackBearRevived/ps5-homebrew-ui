// ps5-homebrew-ui - Stable focus and vertical scrolling across shelves.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components/shelf_browser.hpp"

#include <algorithm>
#include <utility>

namespace hui::ui
{

void ShelfBrowser::set_shelves(std::vector<Shelf> shelves)
{
    const auto previous = selection();
    retain_selection();
    rows_.clear();
    for (Shelf &shelf : shelves)
    {
        if (shelf.items.empty())
            continue;
        Row &row = rows_.emplace_back();
        row.id = shelf.id;
        row.carousel.title = std::move(shelf.title);
        std::vector<CardItem> cards;
        cards.reserve(shelf.items.size());
        row.ids.reserve(shelf.items.size());
        for (ShelfItem &item : shelf.items)
        {
            row.ids.push_back(item.id);
            cards.push_back(std::move(item.card));
        }
        row.carousel.set_items(std::move(cards));
        if (const auto saved = remembered_.find(row.id); saved != remembered_.end())
        {
            const auto match = std::find(row.ids.begin(), row.ids.end(), saved->second);
            if (match != row.ids.end())
                row.carousel.set_focus(static_cast<int>(match - row.ids.begin()));
            else if (previous && previous->shelf == row.id)
                row.carousel.set_focus(previous->item_index);
        }
    }
    focus_ = rows_.empty() ? -1 : 0;
    if (focus_ >= 0 && previous)
    {
        const auto same = std::find_if(rows_.begin(), rows_.end(),
                                       [&](const Row &row) { return row.id == previous->shelf; });
        focus_ = same == rows_.end()
                     ? std::min(previous->shelf_index, static_cast<int>(rows_.size()) - 1)
                     : static_cast<int>(same - rows_.begin());
    }
    set_bounds(bounds_);
    activate_rows();
}

void ShelfBrowser::set_bounds(gfx::Rect bounds)
{
    bounds_ = bounds;
    float y = bounds.y;
    for (Row &row : rows_)
    {
        row.carousel.style = style.carousel;
        row.carousel.style.theme = style.theme;
        row.carousel.style.sounds = style.sounds;
        row.carousel.style.reduced_motion = style.reduced_motion;
        row.carousel.set_bounds({bounds.x, y, bounds.w, 0.0f});
        const float height = row.carousel.preferred_height();
        row.carousel.set_bounds({bounds.x, y, bounds.w, height});
        y += height + style.shelf_gap;
    }
    reveal(true);
}

bool ShelfBrowser::select(ShelfId shelf, ShelfId item, bool snap)
{
    const auto row = std::find_if(rows_.begin(), rows_.end(),
                                  [&](const Row &entry) { return entry.id == shelf; });
    if (row == rows_.end())
        return false;
    const auto position = std::find(row->ids.begin(), row->ids.end(), item);
    if (position == row->ids.end())
        return false;
    retain_selection();
    focus_ = static_cast<int>(row - rows_.begin());
    row->carousel.set_focus(static_cast<int>(position - row->ids.begin()), snap);
    activate_rows();
    reveal(snap);
    return true;
}

std::optional<ShelfSelection> ShelfBrowser::selection() const
{
    if (focus_ < 0)
        return {};
    const Row &row = rows_[static_cast<std::size_t>(focus_)];
    const int item = row.carousel.focus();
    return ShelfSelection{row.id, row.ids[static_cast<std::size_t>(item)], focus_, item};
}

gfx::Rect ShelfBrowser::focused_art() const
{
    const auto cell = selection();
    return cell ? art_rect(*cell) : gfx::Rect{};
}

gfx::Rect ShelfBrowser::art_rect(const ShelfSelection &cell) const
{
    if (cell.shelf_index < 0 || cell.shelf_index >= static_cast<int>(rows_.size()))
        return {};
    const Row &row = rows_[static_cast<std::size_t>(cell.shelf_index)];
    if (row.id != cell.shelf || cell.item_index < 0 ||
        cell.item_index >= static_cast<int>(row.ids.size()) ||
        row.ids[static_cast<std::size_t>(cell.item_index)] != cell.item)
        return {};
    const Carousel &carousel = row.carousel;
    const gfx::Rect card = carousel.item_rect(cell.item_index);
    CardState state;
    state.focus = 1.0f;
    gfx::Rect art = card_placed(card, card_art(carousel.style.card, card),
                                card_scale(carousel.style, carousel.style.card, state),
                                card_lift(carousel.style, carousel.style.card, state));
    art.y -= scroll_.offset();
    return art;
}

void ShelfBrowser::retain_selection()
{
    if (const auto cell = selection())
        remembered_[cell->shelf] = cell->item;
}

void ShelfBrowser::reveal(bool snap)
{
    if (focus_ < 0)
    {
        scroll_.position.snap(0.0f);
        return;
    }
    const auto &row = rows_[static_cast<std::size_t>(focus_)].carousel.bounds();
    const auto &last = rows_.back().carousel.bounds();
    scroll_.reveal(row.y - bounds_.y, row.y + row.h - bounds_.y, bounds_.h, 0.0f,
                   last.y + last.h - bounds_.y);
    if (snap || style.reduced_motion)
        scroll_.position.snap(scroll_.position.target);
}

void ShelfBrowser::activate_rows()
{
    for (int i = 0; i < static_cast<int>(rows_.size()); ++i)
        rows_[static_cast<std::size_t>(i)].carousel.set_active(active_ && i == focus_);
}

void ShelfBrowser::enter()
{
    for (Row &row : rows_)
        row.carousel.enter();
}

void ShelfBrowser::set_active(bool active)
{
    active_ = active;
    activate_rows();
}

Event ShelfBrowser::handle(const InputFrame &input, Feedback &feedback)
{
    if (focus_ < 0)
        return input.is_pressed(Action::back) ? Event::cancelled : Event::none;
    if (input.nav != Direction::up && input.nav != Direction::down)
        return rows_[static_cast<std::size_t>(focus_)].carousel.handle(input, feedback);

    const int next = focus_ + (input.nav == Direction::down ? 1 : -1);
    if (next < 0 || next >= static_cast<int>(rows_.size()))
        return refuse(feedback, style, input, refused_, bounds_.cx());
    retain_selection();
    focus_ = next;
    activate_rows();
    reveal(false);
    play_cue(feedback, style, style.sounds.move, bounds_.cx());
    return Event::moved;
}

void ShelfBrowser::update(float dt)
{
    scroll_.update(dt, style.omega());
    refused_.update(dt);
    for (Row &row : rows_)
        row.carousel.update(dt);
}

void ShelfBrowser::draw(Canvas &canvas) const
{
    // The row clips its own horizontal edges, including room for the focus
    // ring. This outer clip only masks rows scrolling above or below the shelf.
    canvas.list.push_clip({bounds_.x - kCarouselFocusMargin, bounds_.y,
                           bounds_.w + 2.0f * kCarouselFocusMargin, bounds_.h});
    canvas.list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, -scroll_.offset());
    for (const Row &row : rows_)
    {
        const gfx::Rect rect = row.carousel.bounds();
        if (rect.y + rect.h >= bounds_.y + scroll_.offset() &&
            rect.y <= bounds_.y + scroll_.offset() + bounds_.h)
            row.carousel.draw(canvas);
    }
    canvas.list.pop_transform();
    canvas.list.pop_clip();
}

void ShelfBrowser::visit_near(int item_radius, const Visit &visit) const
{
    if (focus_ < 0)
        return;
    for (int distance = 0; distance <= 1; ++distance)
    {
        for (int index = std::max(focus_ - distance, 0);
             index <= std::min(focus_ + distance, static_cast<int>(rows_.size()) - 1); ++index)
        {
            if (distance != 0 && index == focus_)
                continue;
            const Row &row = rows_[static_cast<std::size_t>(index)];
            const int item = row.carousel.focus();
            visit({row.id, row.ids[static_cast<std::size_t>(item)], index, item});
            for (int offset = 1; offset <= item_radius; ++offset)
            {
                for (int candidate : {item + offset, item - offset})
                {
                    if (candidate >= 0 && candidate < static_cast<int>(row.ids.size()))
                        visit({row.id, row.ids[static_cast<std::size_t>(candidate)], index,
                               candidate});
                }
            }
        }
    }
}

void ShelfBrowser::set_artwork(const ShelfSelection &cell, std::uint32_t texture, float aspect)
{
    if (cell.shelf_index < 0 || cell.shelf_index >= static_cast<int>(rows_.size()))
        return;
    auto &row = rows_[static_cast<std::size_t>(cell.shelf_index)];
    if (row.id != cell.shelf || cell.item_index < 0 ||
        cell.item_index >= static_cast<int>(row.ids.size()) ||
        row.ids[static_cast<std::size_t>(cell.item_index)] != cell.item)
        return;
    CardItem &card = row.carousel.item(cell.item_index);
    card.texture = texture;
    card.image_aspect = aspect;
}

} // namespace hui::ui
