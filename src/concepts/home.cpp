// ps5-homebrew-ui - The playable opening screen and developer handoff.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concepts/concepts.hpp"
#include "core/tween.hpp"
#include "ui/glyphs.hpp"
#include "ui/widgets.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

namespace hui::concepts
{
namespace
{
using gfx::Color;
using gfx::Rect;
const Color kWhite = Color::rgb(0xf7f8ff);
const Color kQuiet = Color::rgb(0xd7ddeb);
const Color kNight = Color::rgb(0x101523);
constexpr gfx::ArtworkId kLaunchArtwork = 0x484f4d45;
constexpr const char *kPresets[] = {"Original", "Cinema", "Cool"};
constexpr const char *kTechniques[] = {
    "A playable settings panel drawn from the selected theme's real widget tokens",
    "Large Text reflows the launch actions above the preview",
    "A single developer reference page with the repository address, examples and agent guide",
};
constexpr auto home_tour(Direction next_action)
{
    return std::to_array<app::TourStep>({
        {0.4f, action_bit(Action::west)},
        {0.3f, 0, Direction::down},
        {0.3f, 0, Direction::right},
        {0.4f, action_bit(Action::jump_next)},
        {0.6f, action_bit(Action::back), Direction::none, "preview"},
        {0.2f, 0, next_action},
        {0.2f, 0, next_action},
        {0.2f, action_bit(Action::confirm)},
        {0.6f, action_bit(Action::back), Direction::none, "guide"},
        {0.3f, action_bit(Action::jump_prev)},
    });
}

class Home final : public app::Concept
{
  public:
    explicit Home(app::Context &context) : context_(context)
    {
        brightness_motion_.snap(brightness_ / 10.f);
        sound_motion_.snap(1.f);
    }
    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo info{"home",
                                           "Homebrew UI Lab",
                                           "A playable theme and widget library for PS5 homebrew",
                                           "src/concepts/home.cpp",
                                           audio::SoundSet::glass,
                                           Color::rgb(0x9ae3ff),
                                           kTechniques};
        return info;
    }
    std::span<const app::TourStep> tour() const override
    {
        static constexpr auto normal = home_tour(Direction::down);
        static constexpr auto large = home_tour(Direction::right);
        return context_.settings.large_text ? large : normal;
    }

    void enter() override
    {
        const ui::Theme *saved = ui::find_theme(context_.settings.theme_id);
        theme_ = saved && !context_.touring ? static_cast<int>(saved - ui::themes().data()) : 0;
        age_ = 0.f;
        theme_fade_ = 0.f;
        builder_ = false;
        editing_ = false;
        if (context_.touring)
            action_ = 0;
        for (int i = 0; i < 3; ++i)
            action_focus_[i].snap(i == action_ ? 1.f : 0.f);
        if (context_.artwork)
        {
            const gfx::ArtworkRequest request{kLaunchArtwork,
                                              std::string(context_.artwork_root) + "/home.jpg"};
            context_.artwork->select({&request, 1});
        }
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        theme_fade_ = std::max(0.f, theme_fade_ - dt * 4.5f);
        if (context_.artwork)
            context_.artwork->pump();
        if (builder_)
            update_builder(input, feedback);
        else if (input.is_pressed(Action::jump_next) || input.is_pressed(Action::jump_prev))
        {
            const int count = static_cast<int>(ui::themes().size());
            theme_ = (theme_ + (input.is_pressed(Action::jump_next) ? 1 : count - 1)) % count;
            if (!context_.touring)
            {
                context_.settings.theme_id = ui::themes()[theme_].id;
                context_.settings_changed = true;
            }
            theme_fade_ = context_.settings.reduced_motion ? 0.f : 1.f;
            cue(feedback, audio::Cue::tab);
        }
        else if (input.is_pressed(Action::west))
        {
            editing_ = !editing_;
            cue(feedback, audio::Cue::focus);
        }
        else if (editing_)
            update_preview(input, feedback);
        else
            update_actions(input, feedback);
        animate(brightness_motion_, brightness_ / 10.f, dt);
        animate(sound_motion_, sound_ ? 1.f : 0.f, dt);
        for (int i = 0; i < 3; ++i)
            animate(action_focus_[i], !editing_ && i == action_ ? 1.f : 0.f, dt);
    }

    void draw(app::Frame &frame) const override
    {
        const ui::Theme theme = ui::with_legibility(ui::themes()[theme_], context_.settings);
        frame.backdrop = theme.backdrop;
        frame.backdrop.time = context_.settings.reduced_motion ? 0.f : age_;
        auto &scene = frame.scene;
        const gfx::Artwork art =
            context_.artwork ? context_.artwork->get(kLaunchArtwork) : gfx::Artwork{};
        if (art.texture)
        {
            const Color grade[] = {kWhite, Color::rgb(0xffcfac), Color::rgb(0xb8deff)};
            scene.image(art.texture, {0, 0, 1920, 1080}, gfx::kFullUv,
                        gfx::mix(grade[preset_], theme.primary, 0.13f)
                            .with_alpha(0.53f + brightness_motion_.value * 0.46f));
        }
        scene.gradient_rect_h({0, 0, 1920, 1080}, 0, kNight.with_alpha(0.96f),
                              kNight.with_alpha(0.12f));
        scene.gradient_rect({0, 0, 1920, 1080}, 0, kNight.with_alpha(0.f),
                            kNight.with_alpha(0.86f));
        if (builder_)
        {
            draw_builder(frame.overlay);
            return;
        }
        const bool large = context_.settings.large_text;
        const auto &fonts = context_.fonts;
        auto &list = frame.overlay;
        ui::text(list, fonts.display, "Homebrew UI Lab", 104, large ? 205 : 363, large ? 108 : 86,
                 kWhite);
        if (large)
            ui::text(list, fonts.regular, "A theme and widget library for PS5 homebrew.", 108, 278,
                     40, kQuiet);
        else
        {
            ui::text(list, fonts.regular, "A theme and widget library", 108, 437, 38, kQuiet);
            ui::text(list, fonts.regular, "for PS5 homebrew.", 108, 487, 38, kQuiet);
        }
        char explore[64];
        std::snprintf(explore, sizeof(explore), "Browse %zu themes", ui::themes().size());
        const char *labels[] = {context_.last_design == "themes" ? "Resume Theme Lab" : explore,
                                "Widget library", "Developer guide"};
        for (int i = 0; i < 3; ++i)
        {
            const float lift = context_.settings.reduced_motion ? 0.f : action_focus_[i].value;
            const Rect button = large ? Rect{108.f + i * 574.f, 338 - 6 * lift, 550, 82}
                                      : Rect{108 + 9 * lift, 548.f + i * 95.f, 552, 72};
            action_button(list, button, labels[i], !editing_ && action_ == i);
        }
        frame.glass = theme.style == ui::SurfaceStyle::glass && !theme.solid_surfaces;
        const float arrival = context_.settings.reduced_motion
                                  ? 0.f
                                  : 1.f - tween::quint_out(tween::clamp01(age_ / 0.45f));
        list.push_transform(1.f, 0, 0, 0, 28.f * arrival);
        draw_preview(list, theme, frame.glass_texture);
        list.pop_transform();
        draw_footer(list);
    }

  private:
    void animate(tween::Spring &value, float target, float dt) const
    {
        if (context_.settings.reduced_motion)
            value.snap(target);
        else
        {
            value.target = target;
            value.update(dt, ui::themes()[theme_].omega);
        }
    }
    void cue(app::Feedback &feedback, audio::Cue type) const
    {
        feedback.play(type);
        feedback.cues.back().set = ui::themes()[theme_].sounds;
    }
    void move(int &index, int delta, int count, const InputFrame &input, app::Feedback &feedback)
    {
        const int next = std::clamp(index + delta, 0, count - 1);
        if (next != index)
            cue(feedback, audio::Cue::focus);
        else if (!input.nav_repeat)
            cue(feedback, audio::Cue::error);
        index = next;
    }
    void update_actions(const InputFrame &input, app::Feedback &feedback)
    {
        const bool large = context_.settings.large_text;
        if (input.is_pressed(Action::confirm))
        {
            cue(feedback, audio::Cue::select);
            if (action_ < 2)
                context_.requested_design = action_ == 0 ? "themes" : "components";
            else
                builder_ = true;
        }
        else if (input.nav == (large ? Direction::right : Direction::down))
            move(action_, 1, 3, input, feedback);
        else if (input.nav == (large ? Direction::left : Direction::up))
            move(action_, -1, 3, input, feedback);
        else if (input.nav == (large ? Direction::down : Direction::right))
        {
            editing_ = true;
            cue(feedback, audio::Cue::focus);
        }
    }
    void update_preview(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::back) ||
            (context_.settings.large_text && input.nav == Direction::up && control_ == 0))
        {
            editing_ = false;
            cue(feedback, audio::Cue::back);
        }
        else if (input.nav == Direction::down || input.nav == Direction::up)
            move(control_, input.nav == Direction::down ? 1 : -1, 3, input, feedback);
        else if (input.nav == Direction::left || input.nav == Direction::right ||
                 input.is_pressed(Action::confirm))
        {
            const int delta = input.nav == Direction::left ? -1 : 1;
            if (control_ == 0)
                preset_ = (preset_ + delta + 3) % 3;
            else if (control_ == 1)
                brightness_ = std::clamp(brightness_ + delta, 0, 10);
            else
                sound_ = input.nav == Direction::none ? !sound_ : delta > 0;
            if (sound_)
                cue(feedback, audio::Cue::select);
        }
    }
    void update_builder(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::back))
        {
            builder_ = false;
            cue(feedback, audio::Cue::back);
        }
    }

    void action_button(gfx::DrawList &list, Rect r, const char *label, bool focus) const
    {
        const bool large = context_.settings.large_text;
        list.bordered_rect(
            r, 12,
            focus ? kWhite : kNight.with_alpha(context_.settings.reduce_transparency ? 1.f : 0.84f),
            focus ? 3.f : 1.5f, focus ? kWhite : Color::rgb(0x8493ac));
        const Color ink = focus ? kNight : kWhite;
        const float size = large ? 35.f : 31.f;
        ui::text(list, context_.fonts.semibold, label, r.x + 29, r.cy() + size * 0.32f, size, ink);
        if (focus)
        {
            list.line(r.x + r.w - 40, r.cy() - 7, r.x + r.w - 32, r.cy(), 3, ink);
            list.line(r.x + r.w - 32, r.cy(), r.x + r.w - 40, r.cy() + 7, 3, ink);
            if (context_.settings.strong_focus)
                list.bordered_rect({r.x - 6, r.y - 6, r.w + 12, r.h + 12}, 18, Color::rgb(0, 0), 3,
                                   kWhite);
        }
    }
    void draw_preview(gfx::DrawList &list, const ui::Theme &theme, std::uint32_t glass) const
    {
        const bool large = context_.settings.large_text;
        const Rect panel = large ? Rect{108, 476, 1704, 364} : Rect{930, 462, 878, 475};
        // An opaque backing keeps translucent and outline themes readable over bright artwork.
        list.rounded_rect(panel, std::min(theme.radius_card, 28.f), theme.page.with_alpha(1.f));
        ui::Painter paint(list, context_.fonts, theme, glass);
        paint.panel(panel);
        const float x = panel.x + 44;
        const Color ink =
            ui::readable_ink(theme.text, theme.surface.a < 1.f ? theme.page : theme.surface);
        const float title_size = large ? 52.f : 47.f;
        paint.heading(theme.name, x, panel.y + (large ? 84 : 90), title_size, ink);
        paint.body("Settings preview", x, panel.y + (large ? 133 : 139), large ? 35 : 29, ink);
        const char *labels[] = {"Graphics preset", "Brightness", "Preview sound"};
        char percent[12];
        std::snprintf(percent, sizeof(percent), "%d%%", brightness_ * 10);
        for (int i = 0; i < 3; ++i)
        {
            const float left = large ? x + 620.f : x;
            const float cy = panel.y + (large ? 81.f + i * 99.f : 212.f + 91.f * i);
            paint.body(labels[i], left, cy + 9, large ? 34 : 29, ink);
            const Rect r{left + (large ? 386.f : 325.f), cy - 28, large ? 600.f : 455.f, 56};
            const ui::Look look{editing_ && control_ == i ? 1.f : 0.f};
            if (i == 0)
                paint.button(r, kPresets[preset_], ui::ButtonKind::secondary, look);
            else if (i == 1)
            {
                paint.slider({r.x, r.y, r.w - 105, r.h}, brightness_motion_.value, look);
                paint.body(percent, r.x + r.w, r.cy() + 10, large ? 33 : 28, ink,
                           gfx::Align::right);
            }
            else
            {
                paint.toggle({r.x, r.y + 5, 91, 46}, sound_motion_.value, look);
                paint.body(sound_ ? "On" : "Off", r.x + 117, r.cy() + 10, large ? 33 : 28, ink);
            }
        }
        if (theme_fade_ > 0.f)
            paint.fill(panel, theme.radius_card, theme.page.with_alpha(theme_fade_ * 0.6f));
    }
    void draw_builder(gfx::DrawList &list) const
    {
        const bool large = context_.settings.large_text;
        const auto &fonts = context_.fonts;
        list.rounded_rect({0, 0, 1920, 1080}, 0, kNight.with_alpha(0.87f));
        ui::text(list, fonts.display, "Developer guide", 108, 326, large ? 104 : 88, kWhite);
        ui::text(list, fonts.semibold, "github.com/BlackBearRevived/ps5-homebrew-ui", 110, 470,
                 large ? 62 : 52, kWhite);
        const float size = large ? 43.f : 36.f;
        const float line = large ? 61.f : 52.f;
        const float next = ui::paragraph(
            list, fonts.regular, "The repository includes examples of themes and widgets in use.",
            110, 576, size, 1640, line, kQuiet);
        ui::paragraph(list, fonts.regular,
                      "AI coding agents can follow AGENTS.md to use the library in a homebrew app.",
                      110, next + 12, size, 1640, line, kQuiet);
        draw_footer(list);
    }
    void draw_footer(gfx::DrawList &list) const
    {
        const bool large = context_.settings.large_text;
        ui::text(list, context_.fonts.regular,
                 "Made by BlackBearReloaded  /  Open source (GPL-3.0-or-later)", 108,
                 large ? 905 : 890, large ? 30 : 24, kQuiet);
        const ui::Hint home[] = {{ui::Button::l2, "Theme", ui::Button::r2},
                                 {ui::Button::square, "Edit preview"},
                                 {ui::Button::confirm, "Open"},
                                 {ui::Button::l1, "Designs", ui::Button::r1}};
        const ui::Hint edit[] = {{ui::Button::dpad, "Choose / adjust"},
                                 {ui::Button::l2, "Theme", ui::Button::r2},
                                 {ui::Button::back, "Menu"}};
        const ui::Hint build[] = {{ui::Button::back, "Home"}};
        const std::span<const ui::Hint> hints = builder_   ? std::span<const ui::Hint>(build)
                                                : editing_ ? std::span<const ui::Hint>(edit)
                                                           : std::span<const ui::Hint>(home);
        ui::HintLayout layout;
        layout.cy = 985;
        layout.size = context_.settings.large_text ? 47 : 38;
        layout.text_size = context_.settings.large_text ? 33 : 26;
        ui::draw_hints(list, context_.fonts, ui::GlyphStyle::dark(context_.settings.swap_confirm),
                       hints.data(), static_cast<int>(hints.size()), 108, false, layout);
    }
    app::Context &context_;
    int theme_ = 0;
    int action_ = 0;
    int control_ = 0;
    int preset_ = 0;
    int brightness_ = 7;
    bool sound_ = true;
    bool builder_ = false;
    bool editing_ = false;
    float age_ = 0;
    float theme_fade_ = 0;
    tween::Spring brightness_motion_, sound_motion_;
    std::array<tween::Spring, 3> action_focus_;
};
} // namespace

std::unique_ptr<app::Concept> make_home(app::Context &context)
{
    return std::make_unique<Home>(context);
}
} // namespace hui::concepts
