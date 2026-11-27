// ps5-homebrew-ui - Queue, progress, and recovery for background tasks.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui/components/dialog.hpp"
#include "ui/components/notification_center.hpp"
#include "ui/components/progress.hpp"

#include <optional>

namespace hui::ui
{

enum class TransferState : std::uint8_t
{
    queued,
    active,
    paused,
    failed,
    completed,
};

enum class TransferAction : std::uint8_t
{
    pause,
    resume,
    retry,
    cancel,
};

struct TransferJob
{
    std::uint64_t id = 0;
    std::string title;
    TransferState state = TransferState::queued;
    float progress = 0.0f;
    std::string detail; // supply a human-readable failure reason
};

// Pass the latest service state to set_jobs(). On activation, send on_action
// to the service. The queue reflects new state when the service reports it.
class TransferCenter
{
  public:
    ComponentStyle style;
    std::function<void(std::uint64_t, TransferAction)> on_action;

    TransferCenter();
    TransferCenter(const TransferCenter &) = delete;
    TransferCenter &operator=(const TransferCenter &) = delete;
    void set_jobs(std::vector<TransferJob> jobs);
    void set_bounds(gfx::Rect bounds);
    void set_active(bool active);
    const TransferJob *selected() const;
    bool showing_history() const
    {
        return show_history_;
    }
    bool confirming() const
    {
        return confirm_.visible();
    }
    Event handle(const InputFrame &input, Feedback &feedback);
    void update(float dt);
    void draw(Canvas &canvas) const;
    void draw_modal(Canvas &canvas) const;

  private:
    void draw_row(Canvas &canvas, const gfx::Rect &rect, int index) const;
    void record(const TransferJob &job);

    gfx::Rect bounds_{96, 190, 1728, 740};
    gfx::Rect details_{};
    std::vector<TransferJob> jobs_;
    ListView queue_;
    ProgressBar progress_;
    NotificationCenter history_;
    Dialog confirm_;
    std::optional<std::uint64_t> pending_cancel_;
    bool show_history_ = false;
};

} // namespace hui::ui
