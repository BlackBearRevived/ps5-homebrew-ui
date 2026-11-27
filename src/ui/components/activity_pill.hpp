// ps5-homebrew-ui - Persistent status and detail for one background activity.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui/components/transfer_center.hpp"

namespace hui::ui
{
// Feed service snapshots to set_job(). Triangle expands the pill. While expanded,
// route input here exclusively. Event::activated requests a retry of job().id.
struct ActivityPillStyle : ComponentStyle
{
    std::string active_label = "Installing";
    std::string completed_label = "Ready to play";
};

class ActivityPill
{
  public:
    ActivityPillStyle style;
    void set_bounds(gfx::Rect bounds)
    {
        bounds_ = bounds;
    }
    void set_job(TransferJob job);
    const TransferJob &job() const
    {
        return job_;
    }
    bool expanded() const
    {
        return expanded_;
    }
    void dismiss()
    {
        expanded_ = false;
        amount_.snap(0);
    }
    Event handle(const InputFrame &input, Feedback &feedback);
    void update(float dt);
    void draw(Canvas &canvas) const;

  private:
    gfx::Rect bounds_{1124, 112, 700, 244};
    TransferJob job_;
    tween::Spring amount_;
    bool expanded_ = false;
};
} // namespace hui::ui
