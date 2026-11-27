// ps5-homebrew-ui - Transfer state, completed history, and safe cancellation.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components/transfer_center.hpp"

#include <algorithm>
#include <utility>

namespace hui::ui
{
namespace
{

struct StatusText
{
    const char *label;
    StatusKind icon;
};

StatusText describe(TransferState state)
{
    switch (state)
    {
    case TransferState::active:
        return {"In progress", StatusKind::info};
    case TransferState::paused:
        return {"Paused", StatusKind::warning};
    case TransferState::failed:
        return {"Failed", StatusKind::danger};
    case TransferState::completed:
        return {"Complete", StatusKind::success};
    default:
        return {"Queued", StatusKind::question};
    }
}

bool can_cancel(TransferState state)
{
    return state == TransferState::queued || state == TransferState::active ||
           state == TransferState::paused;
}

} // namespace

TransferCenter::TransferCenter()
{
    queue_.style.row_height = 76.0f;
    queue_.style.dividers = true;
    queue_.content = [this](Canvas &canvas, const gfx::Rect &row, const ListItem &, int index,
                            float) { draw_row(canvas, row, index); };
    set_bounds(bounds_);
}

void TransferCenter::set_bounds(gfx::Rect bounds)
{
    bounds_ = bounds;
    const float width = bounds.w * 0.54f;
    queue_.set_bounds({bounds.x, bounds.y + 96.0f, width, bounds.h - 96.0f});
    details_ = {bounds.x + width + 32.0f, bounds.y + 96.0f, bounds.w - width - 32.0f,
                bounds.h - 96.0f};
    progress_.set_bounds({details_.x + 30.0f, details_.y + 146.0f, details_.w - 60.0f, 24.0f});
    history_.set_bounds({bounds.x, bounds.y + 96.0f, bounds.w, bounds.h - 96.0f});
    confirm_.set_bounds(bounds);
}

void TransferCenter::set_jobs(std::vector<TransferJob> jobs)
{
    const int old_index = queue_.focus();
    const auto old_id = selected() ? std::optional(selected()->id) : std::nullopt;
    const bool reordered = !std::ranges::equal(jobs_, jobs, {}, &TransferJob::id, &TransferJob::id);
    for (const TransferJob &job : jobs)
    {
        if (job.state != TransferState::completed && job.state != TransferState::failed)
            continue;
        const auto old = std::ranges::find(jobs_, job.id, &TransferJob::id);
        if (old != jobs_.end() && old->state != job.state)
            record(job);
    }
    jobs_ = std::move(jobs);
    if (!reordered)
        return;
    queue_.set_items(std::vector<ListItem>(jobs_.size()));
    const auto current = old_id ? std::ranges::find(jobs_, *old_id, &TransferJob::id) : jobs_.end();
    queue_.set_focus(current == jobs_.end() ? old_index
                                            : static_cast<int>(current - jobs_.begin()));
}

void TransferCenter::record(const TransferJob &job)
{
    Notification notification;
    notification.source = "Transfers";
    notification.title = job.title;
    notification.kind = describe(job.state).icon;
    notification.body = job.detail.empty() ? describe(job.state).label : job.detail;
    history_.record(std::move(notification), CloseReason::dismissed);
}

void TransferCenter::set_active(bool active)
{
    queue_.set_active(active && !show_history_);
    history_.set_active(active && show_history_);
}

const TransferJob *TransferCenter::selected() const
{
    if (queue_.focus() < 0)
        return nullptr;
    return &jobs_[static_cast<std::size_t>(queue_.focus())];
}

Event TransferCenter::handle(const InputFrame &input, Feedback &feedback)
{
    if (confirm_.is_open())
    {
        const Event event = confirm_.handle(input, feedback);
        if (event == Event::activated && confirm_.choice() == 1 && pending_cancel_)
        {
            const auto job = std::ranges::find(jobs_, *pending_cancel_, &TransferJob::id);
            if (job != jobs_.end() && can_cancel(job->state) && on_action)
                on_action(job->id, TransferAction::cancel);
        }
        if (!confirm_.is_open())
            pending_cancel_.reset();
        return event;
    }
    if (input.is_pressed(Action::west))
    {
        show_history_ = !show_history_;
        set_active(true);
        if (show_history_)
            history_.enter();
        play_cue(feedback, style, style.sounds.page, bounds_.cx());
        return Event::changed;
    }
    if (show_history_)
        return history_.handle(input, feedback);
    const TransferJob *job = selected();
    if (job && input.is_pressed(Action::north) && can_cancel(job->state))
    {
        pending_cancel_ = job->id;
        confirm_.open({StatusKind::warning,
                       "Cancel transfer?",
                       job->title,
                       {{"Keep"}, {"Cancel transfer", ButtonKind::primary, true}}},
                      feedback);
        return Event::none;
    }
    const Event event = queue_.handle(input, feedback);
    if (event != Event::activated || !job)
        return event;
    if (job->state == TransferState::active || job->state == TransferState::paused ||
        job->state == TransferState::failed)
    {
        if (on_action)
            on_action(job->id, job->state == TransferState::active   ? TransferAction::pause
                               : job->state == TransferState::paused ? TransferAction::resume
                                                                     : TransferAction::retry);
        return Event::activated;
    }
    return Event::none;
}

void TransferCenter::update(float dt)
{
    static_cast<ComponentStyle &>(queue_.style) = style;
    static_cast<ComponentStyle &>(confirm_.style) = style;
    static_cast<ComponentStyle &>(history_.style) = style;
    static_cast<ComponentStyle &>(progress_.style) = style;
    if (const TransferJob *job = selected())
        progress_.set_value(job->state == TransferState::completed ? 1.0f : job->progress);
    queue_.update(dt);
    confirm_.update(dt);
    history_.update(dt);
    progress_.update(dt);
}

void TransferCenter::draw_row(Canvas &canvas, const gfx::Rect &rect, int index) const
{
    const TransferJob &job = jobs_[static_cast<std::size_t>(index)];
    const auto status = describe(job.state);
    const Theme &theme = style.theme;
    Painter paint(canvas.list, canvas.fonts, theme);
    const bool wide = rect.w >= 760.0f;
    const float title_size = (wide ? 30.0f : 26.0f) * style.theme.text_scale;
    const float status_size = (wide ? 26.0f : 22.0f) * style.theme.text_scale;
    const float available = rect.w - 110.0f - paint.label_width(status.label, status_size);
    draw_status_icon(canvas, theme, status.icon, rect.x + 30.0f, rect.cy(), wide ? 32.0f : 25.0f);
    paint.body(fit_body(paint, job.title, title_size, available), rect.x + 62.0f,
               rect.cy() + title_size * 0.32f, title_size, theme.text);
    paint.label(status.label, rect.x + rect.w - 24.0f, rect.cy() + status_size * 0.32f, status_size,
                theme.text_muted, gfx::Align::right);
    if (wide && job.state == TransferState::active)
    {
        const gfx::Rect track{rect.x + 62, rect.y + rect.h - 9, available, 4};
        canvas.list.rounded_rect(track, 2, theme.surface_high);
        const float filled = track.w * tween::clamp01(job.progress);
        if (filled > 0)
            canvas.list.rounded_rect({track.x, track.y, filled, track.h}, 2, theme.accent);
    }
}

void TransferCenter::draw(Canvas &canvas) const
{
    const Theme &theme = style.theme;
    Painter paint(canvas.list, canvas.fonts, theme);
    paint.heading(show_history_ ? "Transfer history" : "Transfers", bounds_.x, bounds_.y + 64.0f,
                  52.0f, theme.text);
    if (show_history_)
    {
        history_.draw(canvas);
        return;
    }
    queue_.draw(canvas);
    paint.panel(details_);
    if (const auto *job = selected())
    {
        const float left = details_.x + 30.0f;
        const float width = details_.w - 60.0f;
        const auto status = describe(job->state);
        paint.heading(fit_label(paint, job->title, 34.0f, width), left, details_.y + 68.0f, 34.0f);
        paint.label(status.label, left, details_.y + 118.0f, 25.0f, theme.text_muted);
        const bool wide = width >= 620.0f && details_.h >= 550.0f;
        float text_x = left;
        float y = details_.y + 228.0f;
        if (wide)
        {
            const float cx = left + 103.0f;
            const float cy = details_.y + 290.0f;
            const float value = tween::clamp01(progress_.value());
            const auto color = job->state == TransferState::failed      ? theme.danger
                               : job->state == TransferState::completed ? theme.success
                                                                        : theme.accent;
            canvas.list.ring(cx, cy, 96, 9, theme.surface_high);
            if (value > 0.0f)
                canvas.list.arc(cx, cy, 96, 9, -1.5708f, 6.28318f * value, color);
            paint.heading(std::to_string(static_cast<int>(100.0f * value + 0.5f)) + "%", cx,
                          cy + 15, 43, theme.text, gfx::Align::center);
            text_x += 248.0f;
            y = cy - 18.0f;
            canvas.list.line(left, details_.y + 155, left + width, details_.y + 155, 1,
                             theme.outline);
        }
        else
            progress_.draw(canvas);
        const auto reason = wrap_body(paint, job->detail, 26.0f, width - (text_x - left), 5);
        for (const std::string &line : reason)
        {
            paint.body(line, text_x, y, 26.0f, theme.text);
            y += 36.0f;
        }
        const char *action = job->state == TransferState::active   ? "Confirm to pause"
                             : job->state == TransferState::paused ? "Confirm to resume"
                             : job->state == TransferState::failed ? "Confirm to retry"
                                                                   : "";
        paint.body(action, left, details_.y + details_.h - 36.0f, 24.0f, theme.text_muted);
    }
    else
        paint.body("The queue is empty", details_.cx(), details_.cy(), 28.0f, theme.text,
                   gfx::Align::center);
}

void TransferCenter::draw_modal(Canvas &canvas) const
{
    confirm_.draw(canvas);
}

} // namespace hui::ui
