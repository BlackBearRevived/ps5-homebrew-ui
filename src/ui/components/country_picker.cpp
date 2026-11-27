// ps5-homebrew-ui - Offline geographic country and territory picker.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components/country_picker.hpp"

#include "core/text_edit.hpp"
#include "ui/components/overlay.hpp"
#include "ui/components/progress.hpp"

#define UTF8PROC_STATIC
#include "third_party/utf8proc/utf8proc.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace hui::ui
{
namespace
{
using gfx::Color;
using gfx::Rect;
struct LandPart
{
    int first, count, left, top, right, bottom;
};
struct Country
{
    const char *code, *name, *display_name, *region, *aliases;
    int x, y, color, first, count;
};
// Geometry is in hundredths of a degree, north up. The final record is
// background land without a picker code. It participates only in drawing.
#include "ui/components/country_geometry.inc"
constexpr int kCount = static_cast<int>(std::size(kCountries)) - 1;
constexpr float kZoom[] = {1.f, 2.8f, 7.f};
float wrap(float dx)
{
    return std::remainder(dx, 360.f);
}
int find_code(std::string_view code)
{
    const auto it = std::find_if(kCountries, kCountries + kCount,
                                 [&](const auto &c) { return code == c.code; });
    return it == kCountries + kCount ? -1 : static_cast<int>(it - kCountries);
}
// The data baker stores accent-insensitive ASCII keys for the English directory.
std::string search_key(std::string_view text)
{
    std::string result;
    while (!text.empty())
    {
        utf8proc_int32_t c = 0;
        const auto n = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t *>(text.data()),
                                        static_cast<utf8proc_ssize_t>(text.size()), &c);
        if (n <= 0)
            break;
        text.remove_prefix(static_cast<std::size_t>(n));
        utf8proc_int32_t chars[32]{};
        const auto count = utf8proc_decompose_char(
            c, chars, 32,
            static_cast<utf8proc_option_t>(UTF8PROC_DECOMPOSE | UTF8PROC_COMPAT |
                                           UTF8PROC_CASEFOLD | UTF8PROC_STRIPMARK),
            nullptr);
        for (int i = 0; i < count && i < 32; ++i)
            if ((chars[i] >= 'a' && chars[i] <= 'z') || chars[i] == ' ' ||
                (chars[i] >= '0' && chars[i] <= '9'))
                result += static_cast<char>(chars[i]);
    }
    return result;
}
void rule(gfx::DrawList &list, const Rect &r, Color c)
{
    list.rounded_rect(r, 0, c);
}
} // namespace

CountryPicker::CountryPicker()
{
    focus_ = find_code("BR");
    keys_.style.bindings.backspace = Action::west;
    keys_.style.bindings.space = Action::north;
    keys_.style.max_length = 48;
    keys_.on_text = [this](std::string_view text)
    {
        if (text_edit::append(query_, text, 48))
            filter();
    };
    keys_.on_backspace = [this]
    {
        text_edit::erase_last(query_);
        filter();
    };
    list_.style.row_height = 72;
    list_.style.title_size = 30;
    list_.style.value_size = 25;
    list_.style.entrance_step = 0;
    list_.style.highlight.kind = HighlightKind::fill;
    filter();
    layout();
    camera(true, 0);
}
std::size_t CountryPicker::country_count()
{
    return kCount;
}
CountryLocation CountryPicker::focused() const
{
    const auto &c = kCountries[focus_];
    return {c.code, c.name, c.region, c.x * .01f, -c.y * .01f};
}
std::string_view CountryPicker::selected_code() const
{
    return selected_ < 0 ? std::string_view{} : kCountries[selected_].code;
}
bool CountryPicker::set_focus(std::string_view code)
{
    const int i = find_code(code);
    if (i < 0)
        return false;
    focus_ = i;
    query_.clear();
    filter();
    camera(true, 0);
    return true;
}
bool CountryPicker::set_selected(std::string_view code)
{
    if (!set_focus(code))
        return false;
    selected_ = focus_;
    return true;
}
void CountryPicker::set_query(std::string_view text)
{
    std::string value;
    text_edit::append(value, text, 48);
    query_ = std::move(value);
    filter();
}
void CountryPicker::filter()
{
    std::vector<ListItem> rows;
    matches_.clear();
    const std::string wanted = search_key(query_);
    int exact = -1;
    int row = 0;
    for (int i = 0; i < kCount; ++i)
    {
        const auto &c = kCountries[i];
        if (!query_.empty() &&
            (wanted.empty() ||
             std::string_view(c.aliases).find(wanted) == std::string_view::npos) &&
            std::string_view(c.name).find(query_) == std::string_view::npos)
            continue;
        if (wanted.size() == 2 && wanted[0] == c.code[0] + ('a' - 'A') &&
            wanted[1] == c.code[1] + ('a' - 'A'))
            exact = static_cast<int>(rows.size());
        if (i == focus_)
            row = static_cast<int>(rows.size());
        matches_.push_back(i);
        auto &item = rows.emplace_back();
        item.title = c.display_name;
        item.value = c.code;
    }
    list_.set_items(std::move(rows));
    if (!matches_.empty())
    {
        if (exact >= 0)
            row = exact;
        list_.set_focus(row);
        focus_ = matches_[static_cast<std::size_t>(row)];
    }
    keys_.set_text_context(query_);
}
void CountryPicker::set_bounds(Rect r)
{
    bounds_ = r;
    layout();
}
void CountryPicker::layout()
{
    const float gap = 20.f;
    const float side = bounds_.w * .27f;
    map_ = {bounds_.x, bounds_.y, bounds_.w - gap - side, bounds_.h};
    panel_ = {map_.x + map_.w + gap, bounds_.y, side, bounds_.h};
    list_.set_bounds({map_.x + 24, map_.y + 110, map_.w - 48, map_.h - 132});
    keys_.set_bounds({map_.x + 22, map_.y + 152, map_.w - 44, map_.h - 182});
}
void CountryPicker::enter()
{
    view_ = AtlasView::map;
    query_.clear();
    filter();
    camera(true, 0);
}
int CountryPicker::neighbour(const InputFrame &input) const
{
    float dx = 0, dy = 0;
    switch (input.nav)
    {
    case Direction::up:
        dy = -1;
        break;
    case Direction::down:
        dy = 1;
        break;
    case Direction::left:
        dx = -1;
        break;
    case Direction::right:
        dx = 1;
        break;
    default:
        return -1;
    }
    if (input.nav_from_stick && std::hypot(input.stick_x, input.stick_y) > .35f)
    {
        dx = input.stick_x;
        dy = input.stick_y;
    }
    float best = 1e9f;
    int next = -1;
    const auto &from = kCountries[focus_];
    for (int i = 0; i < kCount; ++i)
    {
        if (i == focus_)
            continue;
        const float x = wrap((kCountries[i].x - from.x) * .01f);
        const float y = (kCountries[i].y - from.y) * .01f;
        const float ahead = x * dx + y * dy, side = std::abs(x * dy - y * dx);
        if (ahead <= 0 || side > ahead * 1.25f)
            continue;
        const float cost = ahead + 2.f * side;
        if (cost < best)
        {
            best = cost;
            next = i;
        }
    }
    return next;
}
Event CountryPicker::handle(const InputFrame &input, Feedback &feedback)
{
    if (view_ == AtlasView::search)
    {
        const auto event = keys_.handle(input, feedback);
        if (event == Event::activated || event == Event::cancelled)
        {
            if (event == Event::cancelled)
            {
                focus_ = original_focus_;
                set_query(original_query_);
            }
            view_ = AtlasView::directory;
            return Event::changed;
        }
        return event == Event::refused ? event : Event::none;
    }
    if (input.is_pressed(Action::north))
    {
        if (view_ == AtlasView::map)
        {
            query_.clear();
            filter();
            view_ = AtlasView::directory;
        }
        else
        {
            original_query_ = query_;
            original_focus_ = focus_;
            view_ = AtlasView::search;
            keys_.set_active(true);
            keys_.enter();
        }
        play_cue(feedback, style, style.sounds.open);
        return Event::changed;
    }
    if (input.is_pressed(Action::back))
    {
        play_cue(feedback, style, style.sounds.cancel);
        if (view_ == AtlasView::directory)
        {
            view_ = AtlasView::map;
            return Event::changed;
        }
        return Event::cancelled;
    }
    if (input.is_pressed(Action::confirm))
    {
        if (view_ == AtlasView::directory && matches_.empty())
            return refuse(feedback, style, input, refused_);
        selected_ = focus_;
        view_ = AtlasView::map;
        play_cue(feedback, style, style.sounds.activate);
        return Event::activated;
    }
    if (view_ == AtlasView::directory)
    {
        const auto event = list_.handle(input, feedback);
        if (event == Event::moved && list_.focus() >= 0)
            focus_ = matches_[static_cast<std::size_t>(list_.focus())];
        return event;
    }
    if (input.is_pressed(Action::west))
    {
        zoom_ = (zoom_ + 1) % static_cast<int>(std::size(kZoom));
        play_cue(feedback, style, style.sounds.change);
        return Event::changed;
    }
    if (input.nav == Direction::none)
        return Event::none;
    const int next = neighbour(input);
    if (next < 0)
        return refuse(feedback, style, input, refused_);
    focus_ = next;
    play_cue(feedback, style, style.sounds.move);
    return Event::moved;
}
void CountryPicker::camera(bool snap, float dt)
{
    const auto c = focused();
    scale_.target = kZoom[zoom_];
    longitude_.target = longitude_.value + wrap((zoom_ ? c.longitude : 0.f) - longitude_.value);
    latitude_.target = zoom_ ? -c.latitude : 0.f;
    pin_x_.target = pin_x_.value + wrap(c.longitude - pin_x_.value);
    pin_y_.target = -c.latitude;
    for (auto *spring : {&scale_, &longitude_, &latitude_, &pin_x_, &pin_y_})
        if (snap || style.reduced_motion)
            spring->snap(spring->target);
        else
            spring->update(dt, 12.f);
}
void CountryPicker::update(float dt)
{
    if (guide_focus_ != focus_)
    {
        constexpr Direction directions[] = {Direction::left, Direction::right, Direction::up,
                                            Direction::down};
        for (std::size_t i = 0; i < std::size(directions); ++i)
        {
            InputFrame probe{};
            probe.nav = directions[i];
            guide_targets_[i] = neighbour(probe);
        }
        guide_focus_ = focus_;
    }
    camera(false, dt);
    list_.style.theme = keys_.style.theme = style.theme;
    list_.style.reduced_motion = keys_.style.reduced_motion = style.reduced_motion;
    list_.style.title_size = 30.f * style.theme.text_scale;
    keys_.style.label_size = 28.f * style.theme.text_scale;
    if (view_ == AtlasView::directory)
        list_.update(dt);
    if (view_ == AtlasView::search)
        keys_.update(dt);
    refused_.update(dt, 8.f);
}

void CountryPicker::draw_map(Canvas &canvas) const
{
    auto &list = canvas.list;
    const auto &theme = style.theme;
    Painter paint(list, canvas.fonts, theme);
    const Color ocean = Color::rgb(0x0a2231), grid = Color::rgb(0x83c4cd, .20f);
    const Color cream = Color::rgb(0xffdfa1), white = Color::rgb(0xe6f5f3),
                quiet = Color::rgb(0xa5c7ce);
    const float em = theme.text_scale;
    list.gradient_rect(map_, 14, Color::rgb(0x183c4b), ocean);
    const Rect plot{map_.x + 12, map_.y + 68, map_.w - 24, map_.h - 126};
    if (plot.w <= 0 || plot.h <= 0)
        return;
    const float unit = std::min((plot.w - 32) / 360.f, (plot.h - 16) / 180.f) * scale_.value;
    if (unit <= 0)
        return;
    const float cx = std::remainder(longitude_.value, 360.f);
    const float cy = latitude_.value;
    const auto px = [&](float lon) { return plot.cx() + (lon - cx) * unit; };
    const auto py = [&](float south) { return plot.cy() + (south - cy) * unit; };
    list.push_clip(plot);
    if (style.graticule)
    {
        for (int lon = -540; lon <= 540; lon += 30)
            if (px(lon) >= plot.x && px(lon) <= plot.x + plot.w)
                list.line(px(lon), plot.y, px(lon), plot.y + plot.h, 1.3f, grid);
        for (int lat = -90; lat <= 90; lat += 30)
            list.line(plot.x, py(lat), plot.x + plot.w, py(lat), lat == 0 ? 2.f : 1.3f, grid);
    }
    constexpr std::uint32_t land[] = {0x40818a, 0x519096, 0x68a6a4, 0x598e88,
                                      0x82afb0, 0x497d82, 0x70a096};
    const auto indices = std::span{kIndices};
    for (int world = -1; world <= 1; ++world)
    {
        const float left = px(world * 360.f);
        if (left + 180 * unit < plot.x || left - 180 * unit > plot.x + plot.w)
            continue;
        list.push_transform(unit * .01f, 0, 0, left, py(0));
        for (int i = 0; i <= kCount; ++i)
        {
            const auto &c = kCountries[i];
            const auto fill =
                i == focus_ ? cream
                : i == selected_
                    ? white
                    : Color::rgb(land[static_cast<unsigned>(c.color) % std::size(land)]);
            for (int j = c.first; j < c.first + c.count; ++j)
            {
                const auto &p = kParts[j];
                if (left + p.right * unit * .01f < plot.x ||
                    left + p.left * unit * .01f > plot.x + plot.w || py(p.bottom * .01f) < plot.y ||
                    py(p.top * .01f) > plot.y + plot.h)
                    continue;
                list.indexed_triangles(kXY, indices.subspan(p.first, p.count), fill);
            }
        }
        list.pop_transform();
    }
    // A pin stays TV-sized at every scale. A tiny island gets the same focus target as a continent.
    const auto c = focused();
    const float x = plot.cx() + wrap(pin_x_.value - cx) * unit;
    const float y = py(pin_y_.value);
    if (guide_focus_ == focus_)
        for (int i : guide_targets_)
        {
            if (i < 0)
                continue;
            const float tx = plot.cx() + wrap(kCountries[i].x * .01f - cx) * unit;
            const float ty = py(kCountries[i].y * .01f);
            const float distance = std::hypot(tx - x, ty - y);
            if (distance < 25 || tx < plot.x || tx > plot.x + plot.w || ty < plot.y ||
                ty > plot.y + plot.h)
                continue;
            if (zoom_ && distance < 230)
                list.line(x, y, tx, ty, 1.5f, quiet.with_alpha(.52f));
            list.circle(tx, ty, 5, ocean);
            list.ring(tx, ty, 5, 2, quiet);
        }
    list.ring(x, y, 15, 5, ocean);
    list.ring(x, y, 16, 2.8f, cream);
    list.circle(x, y, 5, ocean);
    list.circle(x, y, 2.4f, white);
    const float fs = 25.f * em;
    const auto name =
        fit_label(paint, kCountries[focus_].display_name, fs, std::max(70.f, plot.w * .49f - 28));
    const float w = paint.label_width(name, fs) + 28, h = fs + 16;
    const float bx = std::clamp(x + 24.f + w < plot.x + plot.w ? x + 22.f : x - 22.f - w, plot.x,
                                plot.x + std::max(0.f, plot.w - w));
    const float by = std::clamp(y - h * .5f, plot.y, plot.y + std::max(0.f, plot.h - h));
    list.bordered_rect({bx, by, w, h}, 6, ocean, 1.5f, cream);
    paint.label(name, bx + 14, by + h * .5f + fs * .32f, fs, white);
    list.pop_clip();
    paint.label(zoom_ == 0   ? "WORLD"
                : zoom_ == 1 ? "REGIONAL"
                             : "LOCAL",
                map_.x + 28, map_.y + 44, 24 * em, white);
    list.triangle({map_.x + map_.w - 75, map_.y + 20, 15, 21}, cream);
    paint.label("N", map_.x + map_.w - 49, map_.y + 40, 22 * em, quiet);
    rule(list, {map_.x + 26, map_.y + map_.h - 52, map_.w - 52, 1.5f}, grid);
    char coords[64];
    std::snprintf(coords, sizeof(coords), "%.1f %c    %.1f %c", std::abs(c.latitude),
                  c.latitude < 0 ? 'S' : 'N', std::abs(c.longitude), c.longitude < 0 ? 'W' : 'E');
    paint.label(coords, map_.x + 28, map_.y + map_.h - 18, 22 * em, quiet);
    paint.label("Natural Earth", map_.x + map_.w - 28, map_.y + map_.h - 18, 21 * em, quiet,
                gfx::Align::right);
}

void CountryPicker::draw_panel(Canvas &canvas) const
{
    auto &list = canvas.list;
    const auto &t = style.theme;
    Painter paint(list, canvas.fonts, t, canvas.glass);
    paint.panel(panel_);
    const auto c = focused();
    const float em = t.text_scale, x = panel_.x + 28, w = panel_.w - 56;
    const auto accent = readable_ink(t.accent, solid_surface(t));
    const bool has_focus = view_ == AtlasView::map || !matches_.empty();
    paint.label("COUNTRY / REGION", x, panel_.y + 55, 23 * em, t.text_muted);
    if (has_focus)
    {
        const float size = 44.f * em;
        float y = panel_.y + 136;
        for (const auto &line :
             wrap_heading(canvas, t, kCountries[focus_].display_name, size, w, 3))
        {
            paint.heading(line, x, y, size, t.text);
            y += size * 1.15f;
        }
        for (const auto &line : wrap_body(paint, c.region, 27 * em, w, 2))
        {
            paint.body(line, x, y, 27 * em, t.text_muted);
            y += 33 * em;
        }
        const Rect chip{x, y + 3, 96, 53};
        auto chip_fill = gfx::mix(solid_surface(t), t.accent, .17f);
        chip_fill.a = 1;
        list.bordered_rect(chip, 8, chip_fill, 1.5f, t.outline);
        paint.label(c.code, chip.cx(), chip.cy() + 10, 28 * em, readable_ink(t.text, chip_fill),
                    gfx::Align::center);
    }
    const float base = panel_.y + panel_.h - 172;
    rule(list, {x, base, w, 2}, t.outline);
    paint.label("YOUR SELECTION", x, base + 52, 22 * em, t.text_muted);
    const char *choice = selected_ >= 0 ? kCountries[selected_].display_name : "Choose a country";
    float baseline = base + 101;
    for (const auto &line : wrap_body(paint, choice, 28 * em, w, 2))
    {
        paint.body(line, x, baseline, 28 * em, selected_ >= 0 ? accent : t.text);
        baseline += 34 * em;
    }
}

void CountryPicker::draw(Canvas &canvas) const
{
    if (map_.w <= 100 || map_.h <= 180 || panel_.w <= 64)
        return;
    const auto &t = style.theme;
    const float em = t.text_scale;
    Painter paint(canvas.list, canvas.fonts, t, canvas.glass);
    if (view_ == AtlasView::map)
        draw_map(canvas);
    else
    {
        paint.panel(map_);
        const float x = map_.x + 28;
        const bool typing = view_ == AtlasView::search;
        const auto title = typing           ? "Find a country"
                           : query_.empty() ? "Country directory"
                                            : query_.c_str();
        const float size = 34 * em;
        paint.label(fit_label(paint, title, size, map_.w - 245), x, map_.y + 58, size, t.text);
        paint.body(std::to_string(matches_.size()) + " results", map_.x + map_.w - 28, map_.y + 57,
                   24 * em, t.text_muted, gfx::Align::right);
        if (typing)
        {
            paint.field({x, map_.y + 75, map_.w - 56, 62}, query_.empty() ? "Name or code" : query_,
                        false, {1.f});
            keys_.draw(canvas);
        }
        else if (matches_.empty())
            paint.body("Try another name or country code.", x, map_.y + 170, 28 * em, t.text);
        else
            list_.draw(canvas);
    }
    draw_panel(canvas);
}
} // namespace hui::ui
