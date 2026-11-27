// ps5-homebrew-ui - Controller track selection and caption styling.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components/caption_tracks.hpp"

#include "ui/components/overlay.hpp"

#include <algorithm>
#include <utility>

namespace hui::ui
{
namespace
{
enum Field
{
    audio_track = 1,
    subtitles,
    text_size,
    opacity,
};
}

void CaptionTrackPicker::set_tracks(std::vector<std::string> audio_tracks,
                                    std::vector<std::string> subtitle_tracks)
{
    const CaptionOptions previous = options();
    form_.clear();
    if (audio_tracks.empty())
        audio_tracks.emplace_back("Default");
    subtitle_tracks.insert(subtitle_tracks.begin(), "Off");
    form_.add_choice(audio_track, "Audio", std::move(audio_tracks), previous.audio);
    form_.add_choice(subtitles, "Subtitles", std::move(subtitle_tracks), previous.subtitle);
    form_.add_choice(text_size, "Text size", {"Small", "Medium", "Large"}, previous.size);
    form_.add_slider(opacity, "Caption background", previous.opacity * 100.0f, 0.0f, 100.0f, 5.0f)
        .unit = " %";
}

void CaptionTrackPicker::set_bounds(gfx::Rect bounds)
{
    bounds_ = bounds;
    const float width = std::min(bounds.w, 960.0f);
    const float height = std::min(bounds.h, 620.0f);
    panel_ = {bounds.cx() - width * 0.5f, bounds.cy() - height * 0.5f, width, height};
    form_.set_bounds({panel_.x + 30.0f, panel_.y + 100.0f, width - 60.0f, height - 234.0f});
}

void CaptionTrackPicker::open(MediaControls &transport, const CaptionOptions &options,
                              Feedback &feedback)
{
    if (open_)
        return;
    if (form_.row_count() == 0)
        set_tracks({}, {});
    set_bounds(bounds_);
    return_focus_ = transport.focus();
    transport.set_active(false);
    form_.set_choice(audio_track, options.audio);
    form_.set_choice(subtitles, options.subtitle);
    form_.set_choice(text_size, options.size);
    form_.set_slider(opacity, options.opacity * 100.0f);
    form_.set_focus(0);
    form_.set_active(true);
    form_.enter();
    open_ = true;
    play_cue(feedback, style, style.sounds.open, panel_.cx());
}

CaptionOptions CaptionTrackPicker::options() const
{
    if (form_.row_count() == 0)
        return {};
    return {form_.choice_index(audio_track), form_.choice_index(subtitles),
            form_.choice_index(text_size), form_.slider_value(opacity) * 0.01f};
}

Event CaptionTrackPicker::handle(MediaControls &transport, const InputFrame &input,
                                 Feedback &feedback)
{
    if (!open_)
        return Event::none;
    if (input.is_pressed(Action::back))
    {
        dismiss(transport);
        play_cue(feedback, style, style.sounds.close, panel_.cx());
        return Event::cancelled;
    }
    return form_.handle(input, feedback);
}

void CaptionTrackPicker::dismiss(MediaControls &transport)
{
    if (!open_)
        return;
    open_ = false;
    transport.set_focus(return_focus_);
    transport.set_active(true);
}

void CaptionTrackPicker::update(float dt)
{
    static_cast<ComponentStyle &>(form_.style) = style;
    form_.update(dt);
}

void CaptionTrackPicker::draw(Canvas &canvas) const
{
    if (!open_)
        return;
    canvas.list.rounded_rect(bounds_, 0.0f, gfx::Color::rgb(0x000000, 0.65f));
    draw_overlay_panel(canvas, style.theme, panel_, false);
    Painter paint(canvas.list, canvas.fonts, style.theme);
    paint.heading("Audio and subtitles", panel_.x + 30.0f, panel_.y + 66.0f, 39.0f);
    form_.draw(canvas);
    const CaptionOptions live = options();
    const float em = 24.0f + 6.0f * static_cast<float>(live.size);
    const gfx::Rect sample{panel_.x + 30.0f, panel_.y + panel_.h - 108.0f, panel_.w - 60.0f, 78.0f};
    canvas.list.gradient_rect(sample, 8.0f, gfx::Color::rgb(0x678899), gfx::Color::rgb(0x324957));
    const auto text = canvas.fonts.regular.font->fit(preview, em, sample.w - 32.0f);
    const float width = canvas.fonts.regular.measure(text, em) + 24.0f;
    canvas.list.rounded_rect(
        {sample.cx() - width * 0.5f, sample.cy() - em * 0.65f, width, em * 1.3f}, 4.0f,
        gfx::Color::rgb(0x000000, live.opacity));
    ui::text(canvas.list, canvas.fonts.regular, text, sample.cx(), sample.cy() + em * 0.35f, em,
             gfx::Color::rgb(0xffffff), gfx::Align::center);
}

} // namespace hui::ui
