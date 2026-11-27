// ps5-homebrew-ui - Stable artwork geometry and adaptive text scrims.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components/artwork_detail.hpp"

#include <algorithm>

namespace hui::ui
{

void ArtworkDetailHeader::set_bounds(gfx::Rect bounds)
{
    bounds_ = bounds;
}

void ArtworkDetailHeader::set_active(bool active)
{
    button_.set_active(active);
}

Event ArtworkDetailHeader::handle(const InputFrame &input, Feedback &feedback)
{
    return button_.handle(input, feedback);
}

void ArtworkDetailHeader::update(float dt)
{
    button_.style.theme = style.theme;
    button_.style.sounds = style.sounds;
    button_.style.reduced_motion = style.reduced_motion;
    button_.style.on_page = false;
    button_.label = action;
    button_.set_disabled(disabled);
    button_.set_bounds(
        {bounds_.x + style.inset, bounds_.y + bounds_.h - style.inset - style.action_height,
         std::min(style.action_width, bounds_.w - 2.0f * style.inset), style.action_height});
    button_.update(dt);
    if (artwork.texture != previous_texture_)
    {
        image_opacity_.snap(0.0f);
        previous_texture_ = artwork.texture;
    }
    image_opacity_.target = artwork.texture == 0 ? 0.0f : 1.0f;
    image_opacity_.update(dt, style.reduced_motion ? 40.0f : 20.0f);
}

void ArtworkDetailHeader::draw_artwork(Canvas &canvas, gfx::Rect pose) const
{
    const float radius = std::min(style.theme.radius_card, std::min(pose.w, pose.h) * 0.5f);
    canvas.list.gradient_rect(pose, radius, placeholder,
                              gfx::mix(placeholder, style.theme.page, 0.72f));
    if (artwork.texture != 0)
    {
        canvas.list.image(artwork.texture, pose, cover_uv(artwork_uv, artwork.aspect, pose),
                          gfx::Color::rgb(0xffffff, image_opacity_.value), radius);
    }
}

void ArtworkDetailHeader::draw_content(Canvas &canvas) const
{
    const float radius = style.theme.radius_card;
    using gfx::Color;
    // A single rounded gradient follows the artwork outline at both ends.
    // Brighter covers deepen the lower part where the title and controls sit.
    const float darkness = 0.77f + 0.23f * tween::clamp01(artwork.brightness);
    canvas.list.gradient_rect(bounds_, radius, Color::rgb(0x08070b, 0.0f),
                              Color::rgb(0x08070b, darkness));

    const gfx::Rect button = button_.bounds();
    const float x = bounds_.x + style.inset;
    const float width = bounds_.w - 2.0f * style.inset;
    Painter paint(canvas.list, canvas.fonts, style.theme, canvas.glass);
    const FontRef &head = paint.font(style.theme.heading);
    paint.heading(head.font->fit(title, style.title_size, width), x,
                  button.y - style.metadata_size - 38.0f, style.title_size, Color::rgb(0xfff6e9));
    paint.body(fit_body(paint, metadata, style.metadata_size, width), x, button.y - 18.0f,
               style.metadata_size, Color::rgb(0xffead6));
    button_.draw(canvas);
}

void ArtworkDetailHeader::draw(Canvas &canvas) const
{
    draw_artwork(canvas, bounds_);
    draw_content(canvas);
}

} // namespace hui::ui
