// ps5-homebrew-ui - Interactive shelf, detail, filter, and transfer examples.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concepts/components/page.hpp"

#include "ui/components/activity_pill.hpp"
#include "ui/components/artwork_detail.hpp"
#include "ui/components/search_filter.hpp"
#include "ui/components/shelf_browser.hpp"
#include "ui/components/transfer_center.hpp"

#include <algorithm>
#include <utility>

namespace hui::concepts::gallery
{
namespace
{

constexpr gfx::Rect kShelves{96, 300, 1728, 606};
constexpr gfx::Rect kDetails{210, 295, 1500, 608};
constexpr app::TourStep kTour[] = {
    {0.4f, action_bit(Action::l3)},
    {0.5f, action_bit(Action::back), Direction::none, "app-flows-activity"},
    {0.6f, action_bit(Action::confirm), Direction::none, "app-flows"},
    {0.55f, action_bit(Action::back), Direction::none, "app-flows-detail"},
    {0.4f, action_bit(Action::north)},
    {0.55f, action_bit(Action::back), Direction::none, "app-flows-filter"},
    {0.35f, action_bit(Action::west)},
    {0.65f, action_bit(Action::west), Direction::none, "app-flows-transfer"},
};
constexpr ui::Hint kShelfHints[] = {
    {ui::Button::dpad, "Browse"},     {ui::Button::confirm, "Details"},
    {ui::Button::triangle, "Filter"}, {ui::Button::left_stick, "Activity"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kTransferHints[] = {
    {ui::Button::dpad, "Select job"}, {ui::Button::confirm, "Action"},
    {ui::Button::triangle, "Cancel"}, {ui::Button::left_stick, "History"},
    {ui::Button::square, "Variant"},
};
constexpr ui::Hint kModalHints[] = {{ui::Button::confirm, "Choose"}, {ui::Button::back, "Back"}};

class AppsPage final : public Page
{
  public:
    explicit AppsPage(app::Context &context) : context_(context)
    {
        browser_.style.carousel.item_width = 220;
        browser_.style.carousel.card.art_aspect = 16.0f / 10.0f;
        browser_.style.carousel.card.subtitle_size = 0;
        browser_.style.carousel.counter = false;
        pill_.set_bounds({1080, 285, 720, 250});
        detail_.set_bounds(kDetails);
        detail_.artwork_uv = gfx::kCanvasUv;
        detail_.action = "Back to shelf";
        ui::TagOption second{"Second shelf only"};
        second.tag = 1;
        search_.set_options({second}, {"Catalog order", "Title A to Z"});
        transfers_.set_bounds({96, 245, 1728, 668});
        transfers_.set_jobs(jobs_);
        transfers_.on_action = [this](std::uint64_t id, ui::TransferAction action)
        {
            for (auto it = jobs_.begin(); it != jobs_.end(); ++it)
            {
                if (it->id != id)
                    continue;
                if (action == ui::TransferAction::cancel)
                    jobs_.erase(it);
                else
                {
                    if (action == ui::TransferAction::retry)
                        it->detail = "Resuming the transfer.";
                    it->state = action == ui::TransferAction::pause ? ui::TransferState::paused
                                                                    : ui::TransferState::active;
                }
                break;
            }
            transfers_.set_jobs(jobs_);
        };
        rebuild();
    }

    const char *title() const override
    {
        return "App flows";
    }
    const char *summary() const override
    {
        return "Shelves, search and filters, artwork detail, and task recovery";
    }
    const char *variant() const override
    {
        return transfers_mode_ ? "Transfers" : "Catalog";
    }

    void restyle(const ui::Theme &theme, bool reduced_motion) override
    {
        browser_.style.theme = detail_.style.theme = search_.style.theme = transfers_.style.theme =
            theme;
        browser_.style.reduced_motion = detail_.style.reduced_motion =
            search_.style.reduced_motion = transfers_.style.reduced_motion = reduced_motion;
        pill_.style.theme = theme;
        pill_.style.reduced_motion = reduced_motion;
        browser_.set_bounds(kShelves);
    }

    void enter() override
    {
        detail_open_ = false;
        pill_.dismiss();
        browser_.set_active(true);
        search_.dismiss();
        browser_.enter();
    }

    void update(const InputFrame &input, float dt, ui::Feedback &feedback) override
    {
        if (!jobs_.empty())
            pill_.set_job(jobs_.back());
        if (!transfers_mode_ && !detail_open_ && !search_.is_open() && !jobs_.empty() &&
            (pill_.expanded() || input.is_pressed(Action::l3)))
        {
            auto forwarded = input;
            if (input.is_pressed(Action::l3))
                forwarded.pressed |= action_bit(Action::north);
            if (pill_.handle(forwarded, feedback) == ui::Event::activated)
                transfers_.on_action(pill_.job().id, ui::TransferAction::retry);
            browser_.set_active(!pill_.expanded());
        }
        else if (search_.is_open())
        {
            if (search_.handle(input, feedback) == ui::Event::activated)
            {
                applied_ = search_.draft();
                rebuild();
            }
            if (search_.is_open())
            {
                const auto draft = search_.draft();
                if (draft != counted_)
                {
                    counted_ = draft;
                    search_.set_result_count(count(make_data(draft)));
                }
            }
        }
        else if (detail_open_)
        {
            if (input.is_pressed(Action::back) ||
                detail_.handle(input, feedback) == ui::Event::activated)
                detail_open_ = false;
        }
        else if (input.is_pressed(Action::west) && !transfers_.confirming())
        {
            transfers_mode_ = !transfers_mode_;
            ui::play_cue(feedback, browser_.style, browser_.style.sounds.change);
        }
        else if (transfers_mode_)
        {
            InputFrame forwarded = input;
            if (input.is_pressed(Action::l3))
                forwarded.pressed |= action_bit(Action::west);
            transfers_.handle(forwarded, feedback);
        }
        else if (input.is_pressed(Action::north))
        {
            counted_ = applied_;
            search_.set_result_count(count(make_data(applied_)));
            search_.open(applied_, feedback);
        }
        else if (browser_.handle(input, feedback) == ui::Event::activated)
        {
            if (const auto selected = browser_.selection())
            {
                const demo::Item &item = context_.catalog[selected->item - 1];
                detail_.title = item.title;
                detail_.metadata = item.studio;
                detail_.placeholder = item.dark;
                detail_.artwork = {gfx::ArtworkStatus::ready, item.cover, 1.0f, 0.75f};
                detail_.set_active(true);
                detail_open_ = true;
            }
        }
        browser_.update(dt);
        detail_.update(dt);
        search_.update(dt);
        transfers_.update(dt);
        pill_.update(dt);
    }

    void draw(ui::Canvas &canvas) const override
    {
        if (transfers_mode_)
            transfers_.draw(canvas);
        else if (detail_open_)
            detail_.draw(canvas);
        else
            browser_.draw(canvas);
    }

    void draw_modal(ui::Canvas &canvas) const override
    {
        if (!jobs_.empty() && !transfers_mode_ && !detail_open_ && !search_.is_open())
            pill_.draw(canvas);
        search_.draw(canvas);
        transfers_.draw_modal(canvas);
    }

    std::span<const ui::Hint> hints() const override
    {
        if (pill_.expanded() || detail_open_ || search_.is_open() || transfers_.confirming())
            return kModalHints;
        return transfers_mode_ ? std::span<const ui::Hint>{kTransferHints}
                               : std::span<const ui::Hint>{kShelfHints};
    }

    std::span<const app::TourStep> tour() const override
    {
        return kTour;
    }

  private:
    std::vector<ui::Shelf> make_data(const ui::SearchFilters &filters) const
    {
        std::vector<ui::Shelf> shelves{{1, "Discover", {}}, {2, "Collection", {}}};
        for (std::size_t i = 0; i < 12; ++i)
        {
            if ((!filters.tags.empty() && i < 6) ||
                (!filters.query.empty() &&
                 std::string_view(context_.catalog[i].title).find(filters.query) ==
                     std::string_view::npos))
                continue;
            const auto &item = context_.catalog[i];
            ui::CardItem card;
            card.title = item.title;
            card.texture = item.cover;
            card.uv = gfx::kCanvasUv;
            card.top = item.mid;
            card.bottom = item.dark;
            shelves[i / 6].items.push_back({i + 1, std::move(card)});
        }
        if (filters.sort != 0)
        {
            for (auto &shelf : shelves)
                std::ranges::sort(shelf.items, {},
                                  [](const auto &item) { return item.card.title; });
        }
        return shelves;
    }

    void rebuild()
    {
        auto data = make_data(applied_);
        search_.set_result_count(count(data));
        browser_.set_shelves(std::move(data));
    }

    static std::size_t count(const std::vector<ui::Shelf> &data)
    {
        std::size_t total = 0;
        for (const auto &shelf : data)
            total += shelf.items.size();
        return total;
    }

    app::Context &context_;
    ui::ShelfBrowser browser_;
    ui::ArtworkDetailHeader detail_;
    ui::SearchFilterPanel search_;
    ui::TransferCenter transfers_;
    ui::ActivityPill pill_;
    ui::SearchFilters applied_;
    ui::SearchFilters counted_;
    bool transfers_mode_ = false;
    bool detail_open_ = false;
    std::vector<ui::TransferJob> jobs_{
        {1, "Artwork", ui::TransferState::active, 0.67f, "Verifying the image files."},
        {2, "Archive", ui::TransferState::queued, 0.0f, "Waiting for an open slot."},
        {3, "Commentary", ui::TransferState::failed, 0.18f, "The source timed out."},
    };
};

} // namespace

std::unique_ptr<Page> make_apps_page(app::Context &context)
{
    return std::make_unique<AppsPage>(context);
}

} // namespace hui::concepts::gallery
