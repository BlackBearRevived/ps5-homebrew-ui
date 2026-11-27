// ps5-homebrew-ui - Screenprinted coastal chapter select and playable signal dial.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concepts/concepts.hpp"
#include "ui/components/list.hpp"
#include "ui/glyphs.hpp"

#include <algorithm>
#include <array>

namespace hui::concepts
{
namespace
{
using gfx::Color;
struct Chapter
{
    const char *title;
    const char *station;
    const char *description;
    int channel;
};
constexpr std::array<Chapter, 3> kChapters{
    {{"The Breakwater", "BEACON 01", "Wake the light above the harbor.", 3},
     {"Last Ferry", "BEACON 02", "Reach the ferry before it slips offshore.", 5},
     {"High Ridge", "BEACON 03", "Carry the signal over the summit.", 2}}};
constexpr const char *kTechniques[] = {
    "Original two-ink coastal posters composed from native shapes",
    "A slanted selection strip follows controller focus",
    "Each chapter contains a playable radio relay"};
constexpr app::TourStep kTour[] = {{.6f, 0, Direction::down},
                                   {.5f, action_bit(Action::confirm), Direction::none, "ferry"},
                                   {.45f, 0, Direction::right, "dial"},
                                   {.15f, 0, Direction::right},
                                   {.15f, 0, Direction::right},
                                   {.15f, 0, Direction::right},
                                   {.25f, action_bit(Action::confirm)},
                                   {.6f, action_bit(Action::back), Direction::none, "connected"},
                                   {.4f, 0, Direction::down},
                                   {.5f, 0, Direction::none, "ridge"}};

void poster(gfx::DrawList &list, int chapter, const ui::Theme &t)
{
    constexpr gfx::Rect frame{970, 112, 848, 798};
    const Color red = t.primary, ink = t.text, paper = t.surface;
    list.rounded_rect({frame.x + 9, frame.y + 9, frame.w, frame.h}, 0, ink);
    list.rounded_rect(frame, 0, ink);
    const auto area = frame.inset(12.f);
    list.push_clip(area);
    list.rounded_rect(area, 0, paper);
    list.circle(1538, 357, 172, red);
    // Hatch only the sky. Wide spacing survives scaling and TV sharpening.
    for (int i = 0; i < 17; ++i)
        list.line(908.f + i * 72, 594, 1064.f + i * 72, 133, 2, red.with_alpha(.25f));
    const float horizon[] = {970, 639,  1118, 585,  1230, 599,  1376, 514, 1510,
                             553, 1642, 479,  1818, 532,  1818, 910,  970, 910};
    list.polygon(horizon, 9, red);
    if (chapter == 0)
    {
        const float pier[] = {970, 735,  1230, 604,  1460, 581, 1574,
                              627, 1818, 645,  1818, 912,  970, 912};
        list.polygon(pier, 7, ink);
        const float tower[] = {1328, 620, 1365, 356, 1444, 356, 1481, 620};
        list.polygon(tower, 4, ink);
        list.rounded_rect({1350, 346, 109, 15}, 0, ink);
        list.rounded_rect({1365, 287, 79, 56}, 0, ink);
        list.rounded_rect({1380, 300, 49, 31}, 0, paper);
        list.triangle({1345, 246, 119, 44}, ink);
        list.line(1404, 250, 1404, 225, 6, ink);
    }
    else if (chapter == 1)
    {
        const float hull[] = {1102, 532, 1664, 520, 1597, 600, 1224, 620};
        list.polygon(hull, 4, ink);
        list.rounded_rect({1204, 448, 321, 73}, 0, ink);
        list.rounded_rect({1295, 410, 170, 48}, 0, ink);
        list.line(1410, 411, 1409, 309, 7, ink);
        for (int i = 0; i < 7; ++i)
            list.rounded_rect({1224.f + i * 41, 467, 26, 23}, 0, paper);
        for (int i = 0; i < 8; ++i)
            list.line(1020.f + i * 109, 658, 1134.f + i * 109, 646, 5, ink);
    }
    else
    {
        const float ridge[] = {970, 779,  1096, 671,  1212, 707,  1479, 373, 1548,
                               460, 1612, 443,  1818, 616,  1818, 914,  970, 914};
        list.polygon(ridge, 9, ink);
        const float snow[] = {1379, 500, 1479, 376, 1545, 462, 1501, 448, 1480, 471, 1456, 439};
        list.polygon(snow, 6, paper);
        list.line(1479, 376, 1479, 307, 5, ink);
        list.triangle({1478, 307, 45, 22}, red, 0, 1.57f);
    }
    // A radio operator in the foreground ties the three printed landscapes together.
    const float coat[] = {1054, 658,  1070, 622,  1097, 611,  1120, 624,  1149, 670,  1135,
                          682,  1120, 663,  1130, 720,  1070, 720,  1078, 655,  1065, 674};
    list.polygon(coat, 11, ink);
    list.circle(1093, 599, 17, ink);
    list.rotated_rect({1070, 587, 45, 9}, 0, -0.10f, ink);
    list.line(1112, 638, 1132, 612, 9, ink);
    list.line(1132, 613, 1131, 587, 4, ink);
    list.line(1083, 712, 1076, 766, 13, ink);
    list.line(1113, 712, 1127, 766, 14, ink);
    list.rounded_rect({970, 766, 848, 144}, 0, ink);
    list.pop_clip();
}

class Chapters final : public app::Concept
{
  public:
    explicit Chapters(app::Context &context) : context_(context)
    {
        std::vector<ui::ListItem> rows;
        for (const auto &c : kChapters)
            rows.emplace_back().title = c.title;
        list_.set_items(std::move(rows));
        list_.style.row_height = 98;
        list_.style.highlight.kind = ui::HighlightKind::none;
        list_.style.entrance_step = .05f;
        list_.set_bounds({100, 460, 756, 356});
        list_.content = [this](ui::Canvas &canvas, const gfx::Rect &r, const ui::ListItem &item,
                               int index, float focus)
        {
            const auto &t = list_.style.theme;
            const auto ink = t.text;
            const auto paper = t.surface;
            ui::Painter paint(canvas.list, canvas.fonts, t);
            if (focus > .01f)
            {
                const float pts[] = {r.x + 6,        r.y + 9,       r.x + r.w - 6, r.y + 3,
                                     r.x + r.w - 26, r.y + r.h - 9, r.x + 3,       r.y + r.h - 3};
                canvas.list.polygon(pts, 4, gfx::mix(t.page, ink, focus));
            }
            const Color words = focus > .48f ? paper : ink;
            paint.label(std::to_string(index + 1), r.x + 34, r.cy() + 12, 30 * t.text_scale, words);
            paint.label(item.title, r.x + 114, r.cy() + 13, 36 * t.text_scale, words);
            if (cleared_[index])
                paint.label("On air", r.x + r.w - 42, r.cy() + 10, 24 * t.text_scale, words,
                            gfx::Align::right);
        };
        restyle();
    }
    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo info{"chapters",
                                           "Night Signal",
                                           "A playable coastal chapter select in two inks",
                                           "src/concepts/chapters.cpp",
                                           audio::SoundSet::paper,
                                           Color::rgb(0xa52b31),
                                           kTechniques};
        return info;
    }
    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }
    void enter() override
    {
        playing_ = false;
        reveal_ = 1.f;
        list_.set_active(true);
        list_.enter();
    }
    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        if (list_.style.reduced_motion != context_.settings.reduced_motion ||
            (list_.style.theme.text_scale > 1.f) != context_.settings.large_text ||
            list_.style.theme.solid_surfaces != context_.settings.reduce_transparency ||
            list_.style.theme.strong_focus != context_.settings.strong_focus)
            restyle();
        const int index = list_.focus();
        if (!playing_)
        {
            if (list_.handle(input, feedback) == ui::Event::activated)
            {
                playing_ = true;
                channel_ = 1;
                wrong_ = false;
                connected_ = false;
            }
        }
        else if (input.is_pressed(Action::back) ||
                 (connected_ && input.is_pressed(Action::confirm)))
        {
            playing_ = false;
            ui::play_cue(feedback, list_.style, audio::Cue::back);
        }
        else if (!connected_ && (input.nav == Direction::left || input.nav == Direction::right))
        {
            const int next = std::clamp(channel_ + (input.nav == Direction::right ? 1 : -1), 1, 6);
            if (next != channel_)
            {
                channel_ = next;
                wrong_ = false;
                ui::play_cue(feedback, list_.style, audio::Cue::slider);
            }
            else
                ui::refuse(feedback, list_.style, input, refused_);
        }
        else if (!connected_ && input.is_pressed(Action::confirm))
        {
            connected_ = channel_ == kChapters[index].channel;
            wrong_ = !connected_;
            if (connected_)
                cleared_[index] = true;
            ui::play_cue(feedback, list_.style,
                         connected_ ? audio::Cue::select : audio::Cue::error);
        }
        if (index != list_.focus())
        {
            previous_ = index;
            reveal_ = 0.f;
        }
        reveal_ = list_.style.reduced_motion ? 1.f : std::min(1.f, reveal_ + dt * 4.5f);
        list_.update(dt);
        refused_.update(dt, 8);
    }
    void draw(app::Frame &frame) const override
    {
        const auto &t = list_.style.theme;
        const float scale = t.text_scale;
        const int index = list_.focus();
        const auto &c = kChapters[index];
        frame.backdrop = t.backdrop;
        ui::Canvas canvas{frame.scene, context_.fonts};
        ui::Painter p(frame.scene, context_.fonts, t);
        if (reveal_ < 1.f)
        {
            poster(frame.scene, previous_, t);
            frame.scene.push_clip({970, 112, 858.f * tween::cubic_out(reveal_), 810});
        }
        poster(frame.scene, index, t);
        if (reveal_ < 1.f)
            frame.scene.pop_clip();
        const std::string plate = "0" + std::to_string(index + 1);
        p.heading(plate, 1770, 887, 121, t.primary, gfx::Align::right);
        p.heading(plate, 1766, 883, 121, t.surface, gfx::Align::right);
        p.label(c.station, 1030, 832, 29 * scale, t.surface);
        p.label(c.title, 1030, 882, 39 * scale, t.surface);
        p.label(playing_ ? "RADIO RELAY" : "CHAPTER SELECT", 100, 175, 30 * scale, t.primary);
        p.heading("Night Signal", 96, 320, 102 * scale, t.text);
        if (!playing_)
        {
            p.body(c.description, 102, 411, 30 * scale, t.text);
            list_.draw(canvas);
        }
        else
        {
            const std::string target = "Tune to channel " + std::to_string(c.channel);
            p.label(connected_ ? "Signal connected" : target, 102, 477, 42 * scale, t.text);
            for (int i = 1; i <= 6; ++i)
            {
                const gfx::Rect key{104.f + (i - 1) * 128.f, 550, 104, 96};
                const bool selected = i == channel_;
                frame.scene.rounded_rect(key, 0, selected ? t.text : t.surface);
                p.stroke(key, 0, 3.f, t.text);
                p.label(std::to_string(i), key.cx(), key.cy() + 15, 40 * scale,
                        selected ? t.surface : t.text, gfx::Align::center);
            }
            p.body(connected_ ? "Chapter complete"
                   : wrong_   ? "No carrier. Check the channel."
                              : "Select a channel and transmit.",
                   104, 738, 30 * scale, t.text);
        }
        const ui::Hint browse[] = {{ui::Button::dpad, "Chapter"},
                                   {ui::Button::confirm, "Play chapter"}};
        const ui::Hint play[] = {{ui::Button::dpad, "Tune"},
                                 {ui::Button::confirm, "Transmit"},
                                 {ui::Button::back, "Chapters"}};
        const ui::Hint done[] = {{ui::Button::confirm, "Chapters"}};
        const std::span<const ui::Hint> hints = !playing_    ? std::span<const ui::Hint>{browse}
                                                : connected_ ? std::span<const ui::Hint>{done}
                                                             : std::span<const ui::Hint>{play};
        ui::HintLayout layout;
        layout.size *= scale;
        layout.text_size *= scale;
        ui::draw_hints(frame.overlay, context_.fonts,
                       ui::GlyphStyle::light(context_.settings.swap_confirm), hints.data(),
                       static_cast<int>(hints.size()), 1820, true, layout);
    }

  private:
    void restyle()
    {
        list_.style.theme = ui::with_legibility(*ui::find_theme("screenprint"), context_.settings);
        list_.style.reduced_motion = context_.settings.reduced_motion;
    }
    app::Context &context_;
    ui::ListView list_;
    float reveal_ = 1.f;
    int previous_ = 0;
    ui::Pulse refused_;
    std::array<bool, 3> cleared_{};
    int channel_ = 1;
    bool playing_ = false, connected_ = false, wrong_ = false;
};
} // namespace
std::unique_ptr<app::Concept> make_chapters(app::Context &context)
{
    return std::make_unique<Chapters>(context);
}
} // namespace hui::concepts
