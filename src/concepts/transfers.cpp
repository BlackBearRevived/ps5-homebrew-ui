// ps5-homebrew-ui - Broadcast-styled transfer queue and failure recovery.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concepts/concepts.hpp"

#include "ui/components/transfer_center.hpp"
#include "ui/glyphs.hpp"

#include <algorithm>

namespace hui::concepts
{
namespace
{
constexpr const char *kTechniques[] = {
    "The queue keeps the same focused task when the service sends new progress",
    "An action is sent to the service and waits for its reported state",
    "Status symbols and labels accompany color",
};
constexpr app::TourStep kTour[] = {
    {0.6f, action_bit(Action::confirm), Direction::none, "active"},
    {0.5f, action_bit(Action::confirm), Direction::none, "paused"},
    {0.6f, 0, Direction::down},
    {0.6f, 0, Direction::down},
    {0.6f, action_bit(Action::confirm), Direction::none, "failure"},
    {0.6f, action_bit(Action::north), Direction::none, "retry"},
    {0.6f, action_bit(Action::back), Direction::none, "cancel-confirmation"},
    {0.6f, action_bit(Action::west)},
    {0.6f, action_bit(Action::west), Direction::none, "history"},
};

class Transfers final : public app::Concept
{
  public:
    explicit Transfers(app::Context &context) : context_(context)
    {
        center_.set_bounds({96, 92, 1728, 660});
        center_.on_action = [this](std::uint64_t id, ui::TransferAction action)
        {
            auto job = std::ranges::find(jobs_, id, &ui::TransferJob::id);
            if (job == jobs_.end())
                return;
            switch (action)
            {
            case ui::TransferAction::pause:
                job->state = ui::TransferState::paused;
                break;
            case ui::TransferAction::resume:
                job->state = ui::TransferState::active;
                break;
            case ui::TransferAction::retry:
                job->state = ui::TransferState::active;
                job->detail = "Retrying from the last verified block.";
                break;
            case ui::TransferAction::cancel:
                jobs_.erase(job);
                break;
            }
        };
        center_.set_jobs(jobs_);
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo info{
            "transfers",
            "Transfer Center",
            "Live progress and recovery in the Broadcast theme",
            "src/concepts/transfers.cpp",
            audio::SoundSet::glass,
            gfx::Color::rgb(0xffc45e),
            kTechniques,
        };
        return info;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        center_.style.theme = ui::with_legibility(*ui::find_theme("broadcast"), context_.settings);
        center_.style.reduced_motion = context_.settings.reduced_motion;
        if (!center_.style.reduced_motion)
            backdrop_time_ += dt;
        center_.handle(input, feedback);
        for (auto &job : jobs_)
        {
            if (job.state != ui::TransferState::active)
                continue;
            job.progress = std::min(job.progress + dt * 0.09f, 1.0f);
            if (job.progress == 1.0f)
            {
                job.state = ui::TransferState::completed;
                job.detail = "Every block verified.";
            }
        }
        center_.set_jobs(jobs_);
        center_.update(dt);
    }

    void draw(app::Frame &frame) const override
    {
        const auto &theme = center_.style.theme;
        frame.backdrop = theme.backdrop;
        frame.backdrop.time = backdrop_time_;
        ui::Canvas canvas{frame.scene, context_.fonts};
        center_.draw(canvas);
        ui::Painter paint(frame.scene, context_.fonts, theme);
        paint.label("SIMULATED TRANSFERS", 1824, 153.0f, 24.0f, theme.accent, gfx::Align::right);
        if (!center_.showing_history())
        {
            using State = ui::TransferState;
            struct Channel
            {
                State state;
                const char *label;
                gfx::Color ink;
            };
            const Channel channels[] = {{State::active, "In progress", theme.accent},
                                        {State::queued, "Queued", theme.focus},
                                        {State::paused, "Paused", theme.text_muted},
                                        {State::failed, "Failed", theme.danger},
                                        {State::completed, "Complete", theme.success}};
            const gfx::Rect strip{96, 795, 1728, 135};
            paint.panel(strip);
            float share_x = strip.x + 28;
            const float width = strip.w - 56;
            for (std::size_t i = 0; i < std::size(channels); ++i)
            {
                const auto &channel = channels[i];
                const auto count =
                    std::ranges::count(jobs_, channel.state, &ui::TransferJob::state);
                const float x = strip.x + 28 + static_cast<float>(i) * width / std::size(channels);
                frame.scene.circle(x + 5, strip.y + 65, 5, channel.ink);
                paint.heading(std::to_string(count), x + 23, strip.y + 83, 54, theme.text);
                paint.label(channel.label, x + 86, strip.y + 73, 24, theme.text_muted);
                if (count > 0)
                {
                    const float share =
                        width * static_cast<float>(count) / static_cast<float>(jobs_.size());
                    frame.scene.rounded_rect({share_x, strip.y + 108, share - 3, 5}, 2,
                                             channel.ink);
                    share_x += share;
                }
            }
        }
        ui::Canvas above{frame.overlay, context_.fonts};
        center_.draw_modal(above);
        constexpr ui::Hint queue[] = {{ui::Button::confirm, "Action"},
                                      {ui::Button::triangle, "Cancel transfer"},
                                      {ui::Button::square, "History"}};
        constexpr ui::Hint history[] = {{ui::Button::square, "Queue"}};
        constexpr ui::Hint confirm[] = {{ui::Button::confirm, "Choose"},
                                        {ui::Button::back, "Back"}};
        const std::span<const ui::Hint> hints =
            center_.confirming()        ? std::span<const ui::Hint>{confirm}
            : center_.showing_history() ? std::span<const ui::Hint>{history}
                                        : std::span<const ui::Hint>{queue};
        ui::draw_hints(above.list, above.fonts,
                       ui::GlyphStyle::dark(context_.settings.swap_confirm), hints.data(),
                       static_cast<int>(hints.size()), 1824, true);
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    app::Context &context_;
    ui::TransferCenter center_;
    float backdrop_time_ = 0.0f;
    std::vector<ui::TransferJob> jobs_{
        {1, "Observation atlas", ui::TransferState::active, 0.30f, "Copying the image archive."},
        {2, "Audio commentary", ui::TransferState::queued, 0.0f, "Waiting for an open slot."},
        {3, "Weather station map", ui::TransferState::failed, 0.44f,
         "The remote source timed out. You can retry from the last verified block."},
        {4, "Cedar House chapters", ui::TransferState::paused, 0.28f, "Paused by the player."},
        {5, "Field notes", ui::TransferState::completed, 1.0f, "Every block verified."},
    };
};

} // namespace

std::unique_ptr<app::Concept> make_transfers(app::Context &context)
{
    return std::make_unique<Transfers>(context);
}

} // namespace hui::concepts
