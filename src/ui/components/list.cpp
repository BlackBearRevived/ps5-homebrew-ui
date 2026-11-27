// ps5-homebrew-ui - Component: ListView.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components/list.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>

namespace hui::ui
{

using gfx::Color;
using gfx::Rect;

void ListView::set_items(std::vector<ListItem> items)
{
    items_ = std::move(items);
    headers_before_.clear();
    headers_before_.reserve(items_.size());
    int headers = 0;
    for (const ListItem &item : items_)
    {
        headers_before_.push_back(headers);
        headers += item.header;
    }
    set_focus(focus_);
}

void ListView::set_bounds(const Rect &bounds)
{
    bounds_ = bounds;
    retarget(true);
}

void ListView::set_focus(int index, bool snap)
{
    focus_ = -1;
    if (!items_.empty())
    {
        index = std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
        focus_ = index;
        if (items_[static_cast<std::size_t>(index)].header)
        {
            focus_ = step(index, 1);
            if (focus_ < 0)
                focus_ = step(index, -1);
        }
    }
    retarget(snap);
}

void ListView::enter()
{
    age_ = 0.0f;
}

Rect ListView::inner() const
{
    return style.panel ? bounds_.inset(style.panel_padding) : bounds_;
}

float ListView::row_size(int index) const
{
    return std::max(items_[static_cast<std::size_t>(index)].header ? style.header_height
                                                                   : style.row_height,
                    0.0f);
}

float ListView::row_top(int index) const
{
    const int headers = headers_before_[static_cast<std::size_t>(index)];
    return static_cast<float>(headers) * std::max(style.header_height, 0.0f) +
           static_cast<float>(index - headers) * std::max(style.row_height, 0.0f) +
           static_cast<float>(index) * std::max(style.gap, 0.0f);
}

float ListView::content_height() const
{
    if (items_.empty())
        return 0.0f;
    const int last = static_cast<int>(items_.size()) - 1;
    return row_top(last) + row_size(last);
}

Rect ListView::row_rect(int index) const
{
    const Rect in = inner();
    return {in.x, in.y + row_top(index) - scroll_.offset(), in.w, row_size(index)};
}

// The next focusable row in a direction, or -1 at the end.
int ListView::step(int from, int direction) const
{
    const int count = static_cast<int>(items_.size());
    for (int i = from + direction; i >= 0 && i < count; i += direction)
    {
        if (!items_[static_cast<std::size_t>(i)].header)
            return i;
    }
    return -1;
}

void ListView::retarget(bool snap)
{
    const Rect in = inner();
    layout_inner_ = in;
    layout_row_height_ = style.row_height;
    layout_header_height_ = style.header_height;
    layout_gap_ = style.gap;
    if (focus_ < 0)
    {
        scroll_.position.snap(0.0f);
        return;
    }
    const float top = row_top(focus_);
    // Reveal the section header above the first row of a section too.
    const bool under_header = focus_ > 0 && items_[static_cast<std::size_t>(focus_ - 1)].header;
    scroll_.reveal(under_header ? row_top(focus_ - 1) : top, top + row_size(focus_), in.h,
                   style.row_height * 0.5f, content_height());
    if (snap)
        scroll_.position.snap(scroll_.position.target);
    // The highlight lives in content space: scrolling must not make it lag.
    const Rect target{0.0f, top, in.w, row_size(focus_)};
    highlight_.target(target);
    if (snap)
        highlight_.snap(target);
}

Event ListView::handle(const InputFrame &input, Feedback &feedback)
{
    if (items_.empty())
        return Event::none;
    const float x = bounds_.cx();
    if (focus_ >= 0 && (input.nav == Direction::up || input.nav == Direction::down))
    {
        const int direction = input.nav == Direction::down ? 1 : -1;
        int next = step(focus_, direction);
        if (next < 0 && style.wrap && !input.nav_repeat)
            next = step(direction > 0 ? -1 : static_cast<int>(items_.size()), direction);
        if (next < 0 || next == focus_)
            return refuse(feedback, style, input, highlight_.refusal(), x);
        focus_ = next;
        retarget(false);
        const float along = items_.size() > 1
                                ? static_cast<float>(focus_) / static_cast<float>(items_.size() - 1)
                                : 0.0f;
        play_cue(feedback, style, style.sounds.move, x,
                 style.pitch_by_position ? tween::lerp(1.05f, 0.95f, along) : 1.0f);
        return Event::moved;
    }
    if (focus_ >= 0 && input.is_pressed(Action::confirm))
    {
        if (items_[static_cast<std::size_t>(focus_)].disabled)
            return refuse(feedback, style, input, highlight_.refusal(), x);
        press_.trigger();
        play_cue(feedback, style, style.sounds.activate, x);
        if (style.sounds.rumble > 0.0f)
            feedback.rumble(0.3f * style.sounds.rumble, 0.04f);
        return Event::activated;
    }
    if (input.is_pressed(Action::back))
    {
        play_cue(feedback, style, style.sounds.cancel, x);
        return Event::cancelled;
    }
    return Event::none;
}

void ListView::update(float dt)
{
    const Rect in = inner();
    if (in.w != layout_inner_.w || in.h != layout_inner_.h ||
        style.row_height != layout_row_height_ || style.header_height != layout_header_height_ ||
        style.gap != layout_gap_)
        retarget(true);
    age_ += dt;
    highlight_.update(dt, style);
    scroll_.update(dt, std::max(style.omega(), 14.0f));
    active_amount_.target = active_ ? 1.0f : 0.0f;
    active_amount_.update(dt, 18.0f);
    press_.update(dt, 10.0f);
}

void ListView::draw(Canvas &canvas) const
{
    const Theme &theme = style.theme;
    gfx::DrawList &list = canvas.list;
    Painter paint(list, canvas.fonts, theme, canvas.glass);
    if (style.panel)
        paint.panel(bounds_);
    if (items_.empty())
        return;

    const Rect in = inner();
    const float scroll = scroll_.offset();
    const bool overflow = content_height() > in.h + 0.5f;
    // The clip is a little taller than the rows so a ring or a glow around
    // the first and last row is not cut.
    const float bleed = style.highlight.kind == HighlightKind::glow ? 30.0f : 10.0f;
    // A list that scrolls keeps a smaller margin: enough for a ring, and the
    // rows passing through it are already faded.
    const float above = overflow ? std::min(bleed, 9.0f) : bleed;
    list.push_clip({in.x - bleed, in.y - above, in.w + 2.0f * bleed, in.h + 2.0f * above});

    // How visible a row is: 1 inside the view, fading as it is cut.
    const auto visibility = [&](const Rect &row)
    {
        if (!overflow || style.edge_fade <= 0.0f)
            return 1.0f;
        // A row is invisible until a fifth of it shows, so a sliver of its
        // second line never peeks over the edge.
        const float shown = std::min(row.y + row.h - in.y, in.y + in.h - row.y);
        return tween::clamp01((shown - row.h * 0.2f) / (row.h * style.edge_fade));
    };
    const int count = static_cast<int>(items_.size());
    const auto rows = std::views::iota(0, count);
    const auto begin = std::ranges::partition_point(
        rows, [&](int i) { return row_top(i) + row_size(i) < scroll - above; });
    const int first = static_cast<int>(begin - rows.begin());
    const auto end =
        std::ranges::partition_point(std::ranges::subrange(begin, rows.end()),
                                     [&](int i) { return row_top(i) <= scroll + in.h + above; });
    const int last = static_cast<int>(end - rows.begin());
    const auto entrance = [&](int index)
    {
        if (index == focus_)
            return 1.0f;
        if (style.entrance_step <= 0.0f || style.reduced_motion)
            return tween::cubic_out(age_ / 0.2f);
        return tween::stagger(age_, index - first, style.entrance_step, 0.32f);
    };

    if (style.cards || style.dividers)
    {
        for (int i = first; i < last; ++i)
        {
            const ListItem &item = items_[static_cast<std::size_t>(i)];
            const Rect row = row_rect(i);
            const float alpha = visibility(row) * entrance(i);
            if (item.header || alpha <= 0.0f)
                continue;
            list.push_opacity(alpha);
            if (style.cards)
                paint.surface(row, paint.control_radius(row), theme.surface_high, theme.outline,
                              1.0f);
            else if (i + 1 < count && !items_[static_cast<std::size_t>(i + 1)].header)
                list.rounded_rect({row.x + style.padding, row.y + row.h + style.gap * 0.5f - 0.75f,
                                   row.w - 2.0f * style.padding, 1.5f},
                                  0.0f, theme.text_muted.with_alpha(0.22f));
            list.pop_opacity();
        }
    }

    // The highlight is kept in content space; bring it to the screen here.
    if (focus_ >= 0)
    {
        list.push_transform(1.0f, 0.0f, 0.0f, in.x, in.y - scroll);
        const float amount = 0.35f + 0.65f * active_amount_.value;
        HighlightStyle look = style.highlight;
        look.grow -= 2.0f * press_.value; // a press pushes it in for a moment
        highlight_.draw(canvas, style, look, amount);
        list.pop_transform();
    }

    for (int i = first; i < last; ++i)
    {
        const ListItem &item = items_[static_cast<std::size_t>(i)];
        const Rect row = row_rect(i);
        const float in_amount = entrance(i);
        const float alpha = visibility(row) * in_amount;
        if (alpha <= 0.0f || row.y > in.y + in.h || row.y + row.h < in.y)
            continue;
        list.push_opacity(alpha);
        const float slide = style.reduced_motion ? 0.0f : 18.0f * (1.0f - in_amount);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, slide);

        if (item.header)
        {
            paint.label(upper(item.title), row.x + style.padding, row.y + row.h - 14.0f,
                        style.header_size,
                        style.panel || style.cards ? theme.text_muted : paint.page_text_muted());
        }
        else
        {
            const float focus = focus_ >= 0 ? highlight_.coverage({0.0f, row_top(i), in.w, row.h}) *
                                                  active_amount_.value
                                            : 0.0f;
            if (content)
            {
                content(canvas, row, item, i, focus);
            }
            else
            {
                // Rows without a surface of their own sit on the page, and a
                // theme may use other text colours there.
                const bool on_page = !style.panel && !style.cards;
                Color ink = Highlight::text_color(style, style.highlight, focus,
                                                  on_page ? paint.page_text() : theme.text);
                Color quiet = gfx::mix(on_page ? paint.page_text_muted() : theme.text_muted, ink,
                                       focus * 0.6f);
                if (item.disabled)
                {
                    ink = ink.with_alpha(0.42f);
                    quiet = quiet.with_alpha(0.42f);
                }
                float left = row.x + style.padding;
                float right = row.x + row.w - style.padding;
                if (style.leading_width > 0.0f)
                {
                    if (leading)
                        leading(canvas, {left, row.y, style.leading_width, row.h}, item, i, focus);
                    left += style.leading_width + 18.0f;
                }
                else if (item.swatch.a > 0.0f)
                {
                    list.circle(left + 8.0f, row.cy(), 8.0f, item.swatch);
                    left += 34.0f;
                }
                left += style.reduced_motion ? 0.0f : style.focus_shift * focus;

                if (trailing)
                {
                    trailing(canvas, row, item, i, focus);
                    right -= row.h; // a square at the end is the slot's to use
                }
                else
                {
                    if (item.chevron)
                    {
                        const float cx = right - 7.0f + 4.0f * focus;
                        list.line(cx - 5.0f, row.cy() - 9.0f, cx + 4.0f, row.cy(), 2.5f, quiet);
                        list.line(cx + 4.0f, row.cy(), cx - 5.0f, row.cy() + 9.0f, 2.5f, quiet);
                        right -= 30.0f;
                    }
                    if (!item.value.empty())
                    {
                        right -= paint.label(item.value, right, row.cy() + style.value_size * 0.34f,
                                             style.value_size, quiet, gfx::Align::right);
                        right -= 18.0f;
                    }
                    if (!item.badge.empty())
                    {
                        const float size = style.subtitle_size * 0.9f;
                        const float width = paint.label_width(item.badge, size) + 24.0f;
                        const Rect pill{right - width, row.cy() - 15.0f, width, 30.0f};
                        paint.fill(pill, theme.pill_chips ? 15.0f : std::min(theme.radius, 15.0f),
                                   theme.accent);
                        paint.label(item.badge, pill.cx(), pill.cy() + size * 0.34f, size,
                                    Painter::on(theme.accent), gfx::Align::center);
                        right -= width + 18.0f;
                    }
                }

                const float room = std::max(right - left, 40.0f);
                if (item.subtitle.empty())
                {
                    paint.label(fit_label(paint, item.title, style.title_size, room), left,
                                row.cy() + style.title_size * 0.34f, style.title_size, ink);
                }
                else
                {
                    const float block = style.title_size + style.subtitle_size * 1.25f;
                    const float first = row.cy() - block * 0.5f + style.title_size * 0.84f;
                    paint.label(fit_label(paint, item.title, style.title_size, room), left, first,
                                style.title_size, ink);
                    paint.body(fit_body(paint, item.subtitle, style.subtitle_size, room), left,
                               first + style.subtitle_size * 1.3f, style.subtitle_size, quiet);
                }
            }
        }
        list.pop_transform();
        list.pop_opacity();
    }
    list.pop_clip();

    if (style.scroll_thumb && overflow)
    {
        const float track = in.h - 16.0f;
        const float size = std::max(track * in.h / content_height(), 36.0f);
        const float travel = track - size;
        const float at = scroll / std::max(content_height() - in.h, 1.0f);
        const float x = in.x + in.w + 8.0f;
        list.rounded_rect({x, in.y + 8.0f, 4.0f, track}, 2.0f, theme.text_muted.with_alpha(0.16f));
        list.rounded_rect({x, in.y + 8.0f + travel * tween::clamp01(at), 4.0f, size}, 2.0f,
                          theme.text_muted.with_alpha(0.7f));
    }
}

} // namespace hui::ui
