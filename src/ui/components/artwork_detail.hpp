// ps5-homebrew-ui - Full-width artwork with a readable detail header.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "gfx/artwork_cache.hpp"
#include "ui/components/button.hpp"
#include "ui/components/card.hpp"

namespace hui::ui
{

struct ArtworkDetailStyle : ComponentStyle
{
    float inset = 38.0f;
    float title_size = 64.0f;
    float metadata_size = 26.0f;
    float action_width = 280.0f;
    float action_height = 66.0f;
};

// Place the detail header once, then update its artwork when it becomes
// resident. The image and placeholder share the same bounds.
//   detail.title = item.name;
//   detail.artwork = cache.get(item.id);
//   detail.draw(canvas);
class ArtworkDetailHeader
{
  public:
    ArtworkDetailStyle style;
    gfx::Artwork artwork;
    gfx::Rect artwork_uv = gfx::kFullUv;
    gfx::Color placeholder = gfx::Color::rgb(0x2c2530);
    std::string title;
    std::string metadata;
    std::string action = "Open";
    bool disabled = false;

    void set_bounds(gfx::Rect bounds);
    void set_active(bool active);
    Event handle(const InputFrame &input, Feedback &feedback);
    void update(float dt);
    void draw(Canvas &canvas) const;
    void draw_artwork(Canvas &canvas, gfx::Rect pose) const;
    void draw_content(Canvas &canvas) const;
    gfx::Rect bounds() const
    {
        return bounds_;
    }

  private:
    gfx::Rect bounds_{96, 110, 1728, 560};
    PushButton button_;
    tween::Spring image_opacity_;
    std::uint32_t previous_texture_ = 0;
};

} // namespace hui::ui
