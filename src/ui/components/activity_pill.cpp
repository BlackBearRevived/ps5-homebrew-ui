// ps5-homebrew-ui - Persistent status and detail for one background activity.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components/activity_pill.hpp"

#include <algorithm>
#include <utility>

namespace hui::ui
{
void ActivityPill::set_job(TransferJob job)
{
    if (job.id != job_.id)
        dismiss();
    job_ = std::move(job);
}
Event ActivityPill::handle(const InputFrame &input, Feedback &feedback)
{
    if (!expanded_ && !input.is_pressed(Action::north))
        return Event::none;
    if (expanded_ && input.is_pressed(Action::confirm) && job_.state == TransferState::failed)
    {
        play_cue(feedback, style, style.sounds.activate);
        return Event::activated;
    }
    if (input.is_pressed(Action::north) ||
        (expanded_ && (input.is_pressed(Action::back) || input.is_pressed(Action::confirm))))
    {
        expanded_ = !expanded_;
        play_cue(feedback, style, expanded_ ? style.sounds.open : style.sounds.close);
        return expanded_ ? Event::changed : Event::cancelled;
    }
    return Event::none;
}
void ActivityPill::update(float dt)
{
    amount_.target = expanded_ ? 1.f : 0.f;
    if (style.reduced_motion)
        amount_.snap(amount_.target);
    else
        amount_.update(dt, 22.f);
}
void ActivityPill::draw(Canvas &canvas) const
{
    const auto &t = style.theme;
    Painter paint(canvas.list, canvas.fonts, t, canvas.glass);
    const float size = 27.f * t.text_scale;
    std::string status;
    switch (job_.state)
    {
    case TransferState::queued:
        status = "Queued";
        break;
    case TransferState::active:
        status = style.active_label + "  " +
                 std::to_string(static_cast<int>(std::clamp(job_.progress, 0.f, 1.f) * 100.f)) +
                 "%";
        break;
    case TransferState::paused:
        status = "Paused";
        break;
    case TransferState::failed:
        status = "Action needed";
        break;
    case TransferState::completed:
        status = style.completed_label;
        break;
    }
    const auto ink = job_.state == TransferState::failed      ? t.danger
                     : job_.state == TransferState::completed ? t.success
                                                              : t.accent;
    const float a = tween::clamp01(amount_.value);
    const float h = std::min(bounds_.h, 76.f);
    const float compact = std::min(bounds_.w, paint.label_width(status, size) + 94.f);
    const float width = tween::lerp(compact, bounds_.w, a);
    const gfx::Rect panel{bounds_.x + bounds_.w - width, bounds_.y, width,
                          tween::lerp(h, bounds_.h, a)};
    const float radius = tween::lerp(h * .5f, 20.f, a);
    canvas.list.rounded_rect(panel, radius, solid_surface(t));
    paint.stroke(panel, radius, 2.f, expanded_ ? t.focus : t.outline);
    canvas.list.circle(panel.x + 32.f, panel.y + h * .5f, 8.f, ink);
    paint.label(fit_label(paint, status, size, panel.w - 78.f), panel.x + 56.f,
                panel.y + h * .5f + size * .34f, size, t.text);
    if (a < .01f)
        return;
    canvas.list.push_clip(panel);
    canvas.list.push_opacity(a);
    const float room = panel.w - 56.f;
    paint.label(fit_label(paint, job_.title, size, room), panel.x + 28.f, panel.y + 108.f, size,
                t.text);
    paint.body(fit_body(paint, job_.detail, size, room), panel.x + 28.f, panel.y + 154.f, size,
               t.text_muted);
    if (job_.state == TransferState::active)
        paint.progress({panel.x + 28.f, panel.y + 190.f, room, 12.f}, job_.progress);
    if (job_.state == TransferState::failed)
        paint.button({panel.x + 28.f, panel.y + 170.f, 200.f, 60.f}, "Retry", ButtonKind::primary,
                     {1.f});
    canvas.list.pop_opacity();
    canvas.list.pop_clip();
}
} // namespace hui::ui
