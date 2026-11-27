// ps5-homebrew-ui - Audio and caption tracks with a live appearance preview.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui/components/form.hpp"
#include "ui/components/media_controls.hpp"

namespace hui::ui
{

struct CaptionOptions
{
    int audio = 0;
    int subtitle = 0; // 0 is off
    int size = 1;     // small, medium, large
    float opacity = 0.8f;
};

// Call open when MediaControls emits MediaCommand::tracks. Read options()
// on change and apply it to the player. Back restores the transport focus.
class CaptionTrackPicker
{
  public:
    ComponentStyle style;
    std::string preview = "Preview captions at this size";

    void set_tracks(std::vector<std::string> audio, std::vector<std::string> subtitles);
    void set_bounds(gfx::Rect bounds);
    void open(MediaControls &transport, const CaptionOptions &options, Feedback &feedback);
    void dismiss(MediaControls &transport);
    bool is_open() const
    {
        return open_;
    }
    CaptionOptions options() const;
    Event handle(MediaControls &transport, const InputFrame &input, Feedback &feedback);
    void update(float dt);
    void draw(Canvas &canvas) const;

  private:
    gfx::Rect bounds_{0, 0, 1920, 1080};
    gfx::Rect panel_{};
    Form form_;
    MediaControl return_focus_ = MediaControl::play;
    bool open_ = false;
};

} // namespace hui::ui
