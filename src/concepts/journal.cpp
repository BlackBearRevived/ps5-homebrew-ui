// ps5-homebrew-ui - An illustrated reference with the Field Journal theme.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concepts/concepts.hpp"

#include "ui/components/list.hpp"
#include "ui/components/text_view.hpp"
#include "ui/glyphs.hpp"

#include <array>
#include <cmath>

namespace hui::concepts
{
namespace
{

constexpr const char *kSections[] = {"Observatory", "Salt basin", "Tide station"};
constexpr const char *kEntries[] = {
    "A ring of numbered stones surrounds the observatory. At dawn, the shadow "
    "passes over the western stones first. Compare the shadow against the notch in the door "
    "before writing a bearing in the log.",
    "The white crust is brightest where the basin is shallow. Keep to the marked path after rain. "
    "A tide gauge near the eastern bank measures the water against three etched lines.",
    "The station takes two readings each day, one as the water rises and one as it falls. "
    "Use the higher of the two to judge how much space the current will leave below the "
    "footbridge.",
};
constexpr const char *kTechniques[] = {
    "The list's focus bar marks the active section in the margin",
    "Serif display type and a warm background support longer reading",
    "Diagram annotations stay clear when the page is restyled",
};
struct SurveyPlate
{
    const char *title;
    const char *unit;
    std::array<int, 5> readings;
    int scale_max;
    std::array<const char *, 5> marks;
};
constexpr SurveyPlate kPlates[] = {
    {"Morning shadow",
     "Length (cm)",
     {82, 61, 42, 29, 18},
     100,
     {"06:00", "07:00", "08:00", "09:00", "10:00"}},
    {"Across the salt basin",
     "Depth (cm)",
     {3, 11, 19, 15, 6},
     20,
     {"West bank", "Stake II", "Midpoint", "Stake IV", "East bank"}},
    {"Tide gauge",
     "Height (cm)",
     {32, 71, 115, 86, 45},
     120,
     {"Mon AM", "Mon PM", "Tue AM", "Tue PM", "Wed AM"}},
};
constexpr app::TourStep kTour[] = {
    {0.6f, 0, Direction::down, "observatory"},
    {0.6f, 0, Direction::down, "basin"},
    {0.6f, 0, Direction::none, "tide-station"},
};

// Three survey sites share one locator map. The contour lines stay fixed as
// the selected station changes, so the reader keeps a sense of place.
void draw_site_map(ui::Canvas &canvas, const ui::Theme &theme, int selected)
{
    constexpr gfx::Rect plate{96, 590, 430, 305};
    ui::Painter paint(canvas.list, canvas.fonts, theme);
    canvas.list.rounded_rect(plate, 2, theme.surface_high);
    const auto ink = theme.outline.with_alpha(0.76f);
    canvas.list.push_clip(plate);
    struct Point
    {
        float x, y;
    };
    static const auto contours = []
    {
        std::array<std::array<Point, 40>, 9> lines{};
        for (std::size_t band = 0; band < lines.size(); ++band)
        {
            const float level = static_cast<float>(band + 1);
            for (std::size_t i = 0; i < lines[band].size(); ++i)
            {
                const float a = 6.2831853f * static_cast<float>(i) / lines[band].size();
                const float r =
                    level * (1.0f + 0.15f * std::sin(3.0f * a + level * 0.05f) +
                             0.10f * std::cos(2.0f * a - 0.7f) + 0.05f * std::sin(5.0f * a));
                lines[band][i] = {217 + level * 5 + std::cos(a) * 32 * r,
                                  801 - level * 3 + std::sin(a) * 22 * r};
            }
        }
        return lines;
    }();
    for (std::size_t band = 0; band < contours.size(); ++band)
    {
        const auto &line = contours[band];
        Point last = line.back();
        for (Point p : line)
        {
            canvas.list.line(last.x, last.y, p.x, p.y, band % 3 == 2 ? 2.0f : 1.0f, ink);
            last = p;
        }
    }
    constexpr float xs[]{234, 312, 419};
    constexpr float ys[]{784, 733, 773};
    for (int i = 0; i < 3; ++i)
    {
        if (i > 0)
            canvas.list.line(xs[i - 1], ys[i - 1], xs[i], ys[i], 3, theme.text_muted);
        canvas.list.circle(xs[i], ys[i], 7, theme.surface);
        canvas.list.ring(xs[i], ys[i], 8, 2, theme.text_muted);
    }
    canvas.list.circle(xs[selected], ys[selected], 8, theme.accent);
    canvas.list.ring(xs[selected], ys[selected], 18, 4, theme.focus);
    canvas.list.pop_clip();
    paint.label("Survey route", 112, 580, 23, theme.text_muted);
}

class Journal final : public app::Concept
{
  public:
    explicit Journal(app::Context &context) : context_(context)
    {
        std::vector<ui::ListItem> items;
        for (const char *section : kSections)
        {
            ui::ListItem item;
            item.title = section;
            items.push_back(std::move(item));
        }
        sections_.set_items(std::move(items));
        sections_.style.highlight.kind = ui::HighlightKind::bar;
        sections_.style.dividers = true;
        sections_.style.scroll_thumb = false;
        sections_.set_bounds({96, 274, 430, 360});
        entry_.set_bounds({600, 238, 1196, 396});
        entry_.style.panel = false;
        entry_.set_active(false);
        entry_.style.footer = false;
        entry_.style.max_width = 1140.0f;
        show_section();
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo info{
            "journal",
            "Field Journal",
            "An illustrated field reference with long-form typography",
            "src/concepts/journal.cpp",
            audio::SoundSet::glass,
            gfx::Color::rgb(0x936421),
            kTechniques,
        };
        return info;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        const auto theme = ui::with_legibility(*ui::find_theme("field-journal"), context_.settings);
        sections_.style.theme = entry_.style.theme = theme;
        sections_.style.reduced_motion = entry_.style.reduced_motion =
            context_.settings.reduced_motion;
        entry_.style.body_size = 29.0f * theme.text_scale;
        if (sections_.handle(input, feedback) == ui::Event::moved)
            show_section();
        sections_.update(dt);
        entry_.update(dt);
    }

    void draw(app::Frame &frame) const override
    {
        const auto &theme = sections_.style.theme;
        frame.backdrop = theme.backdrop;
        ui::Canvas canvas{frame.scene, context_.fonts};
        ui::Painter paint(frame.scene, context_.fonts, theme);
        paint.heading("Field Journal", 96, 135.0f, 72.0f, theme.text);
        frame.scene.line(96, 179, 1824, 179, 2.0f, theme.accent);
        paint.label("SECTIONS", 96, 244, 24.0f, theme.text_muted);
        sections_.draw(canvas);
        draw_site_map(canvas, theme, sections_.focus());
        paint.panel({566, 215, 1258, 724});
        frame.scene.rounded_rect({566, 215, 8, 724}, 0, theme.accent);
        entry_.draw(canvas);

        const auto &plate = kPlates[sections_.focus()];
        const float title_y = context_.settings.large_text ? 636.0f : 530.0f;
        const float top = title_y + 55;
        constexpr float left = 732, right = 1652, base = 832;
        paint.heading(plate.title, 628, title_y, 36, theme.text);
        paint.label(plate.unit, 1738, title_y - 2, 26, theme.text_muted, gfx::Align::right);
        for (int grid = 0; grid <= 2; ++grid)
        {
            const float y = base - static_cast<float>(grid) * (base - top) / 2;
            frame.scene.line(left, y, right, y, grid == 0 ? 2.0f : 1.0f, theme.outline);
            paint.label(std::to_string(grid * plate.scale_max / 2), left - 27, y + 8, 22,
                        theme.text_muted, gfx::Align::right);
        }
        auto height = [&](std::size_t i)
        {
            return base - (base - top) * static_cast<float>(plate.readings[i]) /
                              static_cast<float>(plate.scale_max);
        };
        constexpr float pitch = (right - left) / 4;
        for (std::size_t i = 0; i + 1 < plate.readings.size(); ++i)
        {
            const float x = left + pitch * static_cast<float>(i);
            const float section[]{x, base, x, height(i), x + pitch, height(i + 1), x + pitch, base};
            frame.scene.polygon(section, 4, theme.accent.with_alpha(0.12f));
            frame.scene.line(x, height(i), x + pitch, height(i + 1), 4.0f, theme.accent);
        }
        for (std::size_t i = 0; i < plate.readings.size(); ++i)
        {
            const float x = left + pitch * static_cast<float>(i);
            const auto y = height(i);
            frame.scene.circle(x, y, 8, theme.surface);
            frame.scene.ring(x, y, 8, 3, theme.accent);
            paint.label(std::to_string(plate.readings[i]), x, y - 17, 26, theme.text,
                        gfx::Align::center);
            paint.label(plate.marks[i], x, base + 49, 24, theme.text_muted, gfx::Align::center);
        }
        constexpr ui::Hint hints[] = {{ui::Button::dpad, "Browse sections"}};
        ui::draw_hints(frame.overlay, context_.fonts, ui::GlyphStyle::light(), hints, 1, 1824,
                       true);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    void show_section()
    {
        const auto index = static_cast<std::size_t>(sections_.focus());
        entry_.set_content(
            {ui::TextBlock::heading(kSections[index]), ui::TextBlock::paragraph(kEntries[index])});
    }

    app::Context &context_;
    ui::ListView sections_;
    ui::TextView entry_;
};

} // namespace

std::unique_ptr<app::Concept> make_journal(app::Context &context)
{
    return std::make_unique<Journal>(context);
}

} // namespace hui::concepts
