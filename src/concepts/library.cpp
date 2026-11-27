// ps5-homebrew-ui - Cinema library with asynchronous covers and reversible details.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concepts/concepts.hpp"

#include "ui/components/activity_pill.hpp"
#include "ui/components/artwork_detail.hpp"
#include "ui/components/search_filter.hpp"
#include "ui/components/shelf_browser.hpp"
#include "ui/components/transition.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <utility>

namespace hui::concepts
{
namespace
{

using gfx::Color;
using gfx::Rect;

// Both complete rows fit above the controller hints, including the larger captions.
constexpr Rect kShelves{96, 460, 1728, 517};
constexpr Rect kPreview{96, 130, 1728, 306};
constexpr Rect kHero{96, 130, 1728, 642};
constexpr ui::ShelfId kResume = 1000;
constexpr ui::ShelfId kMissing = 1001;
constexpr int kItemCount = 1100;
constexpr int kFavoriteTag = 1;
constexpr int kArchiveTag = 2;
constexpr const char *kTitles[] = {
    "Salt Meridian",   "The Tidal Line",  "Ember Monument", "Last Freight",
    "The Cedar House", "Dust Country",    "Blue Hours",     "October Orchard",
    "The High Refuge", "After the Glass", "Storm Harbor",   "The White Horizon",
};
constexpr const char *kDescriptions[] = {
    "Follow a light across the salt flats to an observatory that keeps its own time.",
    "A coastal field station, a rising tide, and a narrow path along the cliffs.",
    "Survey a ruined monument before a desert storm buries its last records.",
    "Hold the line at a freight stop where every train carries a different story.",
    "Find your way through a wet cedar forest by its lanterns and old stairways.",
    "Trace a river through a canyon carved from red stone and pale wind.",
    "Follow an evening courier through a city reflected in its own floodwater.",
    "Walk the paths of an orchard at harvest, choosing what to carry home.",
    "Cross the ridge to a lit refuge before the mountain weather turns.",
    "Restore a greenhouse on the edge of a meadow before the first frost.",
    "Chart a safe channel through a harbor that has lost its lights.",
    "Navigate an expedition across snow, white sky, and the last hours of sun.",
};
constexpr const char *kTechniques[] = {
    "A highlight enters the selected cover from the navigation edge",
    "The activity pill measures completed cover uploads and returns to the same shelf",
    "Stable IDs preserve focus across reordered and filtered shelves",
    "The artwork cache decodes JPEG files off the render thread",
    "The shared cover can reverse in flight when the player presses Back",
    "The hero scrim adapts to the artwork's brightness",
};

ui::ShelfId shelf_of(ui::ShelfId item)
{
    return item <= 500 ? 1 : item <= 1000 ? 2 : 3;
}

std::size_t art_index(ui::ShelfId item)
{
    if (item == kResume)
        return 11; // the bright cover is the detail contrast check
    return static_cast<std::size_t>((item - 1) % std::size(kTitles));
}

std::string title_of(ui::ShelfId item)
{
    if (item > 12)
        return std::string(kTitles[art_index(item)]) + " / " + std::to_string(item);
    return kTitles[art_index(item)];
}

std::string folded(std::string text)
{
    for (char &character : text)
    {
        if (character >= 'A' && character <= 'Z')
            character = static_cast<char>(character - 'A' + 'a');
    }
    return text;
}

class Library final : public app::Concept
{
  public:
    explicit Library(app::Context &context) : context_(context)
    {
        theme_ = *ui::find_theme("cinema");
        shelves_.style.carousel.mode = ui::CarouselMode::paged;
        shelves_.style.carousel.per_page = 6;
        shelves_.style.carousel.peek = 0.0f;
        shelves_.style.carousel.gap = 24.0f;
        shelves_.style.carousel.dots = false;
        shelves_.style.carousel.card.art_aspect = 16.0f / 9.0f;
        shelves_.style.carousel.card.focus_scale = 1.022f;
        shelves_.style.carousel.card.title_size = 27.0f;
        shelves_.style.carousel.card.glow = true;
        shelves_.style.carousel.card.directional_light = true;
        shelves_.style.carousel.card.subtitle_size = 0.0f;
        shelves_.style.carousel.counter = true;
        shelves_.style.carousel.entrance_step = 0.0f;
        shelves_.style.carousel.title_gap = 10.0f;
        shelves_.style.shelf_gap = 26.0f;
        hero_.set_bounds(kHero);
        preview_.set_bounds(kPreview);
        ui::TagOption favorite{"Favorites"};
        favorite.tag = kFavoriteTag;
        ui::TagOption archive{"Archive"};
        archive.tag = kArchiveTag;
        filters_.set_options({favorite, archive}, {"Catalog order", "Title A to Z"});
        restyle();
        rebuild();
        build_tour();
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo info{
            "library",
            "Cinema Library",
            "A large artwork-led library with reversible details",
            "src/concepts/library.cpp",
            audio::SoundSet::glass,
            Color::rgb(0xffddaf),
            kTechniques,
        };
        return info;
    }

    void enter() override
    {
        artwork_dirty_ = true; // Home shares the cache and selects its own artwork.
        activity_.dismiss();
        activity_visible_ = false;
        shelves_.enter();
        shelves_.set_active(true);
        hero_.set_active(false);
        filters_.dismiss();
        transition_.reset();
        hero_cell_.reset();
        heading_shown_.snap(0.0f);
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        if (reduced_ != context_.settings.reduced_motion ||
            (theme_.text_scale > 1.0f) != context_.settings.large_text ||
            theme_.solid_surfaces != context_.settings.reduce_transparency ||
            theme_.strong_focus != context_.settings.strong_focus)
            restyle();

        if (activity_.expanded())
        {
            if (activity_.handle(input, feedback) == ui::Event::activated && context_.artwork)
            {
                for (auto id = activity_first_; id < activity_first_ + 3; ++id)
                    context_.artwork->retry_unavailable(id);
                artwork_dirty_ = true;
                activity_job_.state = ui::TransferState::active;
            }
            shelves_.set_active(!activity_.expanded());
        }
        else if (filters_.is_open())
        {
            if (filters_.handle(input, feedback) == ui::Event::activated)
                apply(filters_.draft());
            if (filters_.is_open())
            {
                const ui::SearchFilters draft = filters_.draft();
                if (draft != counted_)
                {
                    counted_ = draft;
                    filters_.set_result_count(matches(draft));
                }
            }
        }
        else if (transition_.expanded())
        {
            if (input.is_pressed(Action::back))
            {
                transition_.set_expanded(false);
                hero_.set_active(false);
                shelves_.set_active(true);
                played_ = false;
                ui::play_cue(feedback, hero_.style, audio::Cue::back);
            }
            else if (hero_.handle(input, feedback) == ui::Event::activated)
                played_ = true;
        }
        else if (input.is_pressed(Action::north) && !transition_.running())
        {
            if (!activity_visible_ && shelves_.selection())
            {
                const auto id = shelves_.selection()->item;
                activity_first_ = std::min<ui::ShelfId>(id, kItemCount - 2);
                activity_job_ = {id, "Library cover set", ui::TransferState::active, 0.f,
                                 "Preparing cover artwork."};
                activity_visible_ = true;
                artwork_dirty_ = true;
                activity_.set_job(activity_job_);
            }
            if (activity_visible_)
            {
                activity_.handle(input, feedback);
                shelves_.set_active(!activity_.expanded());
            }
        }
        else if (input.is_pressed(Action::menu))
        {
            counted_ = applied_;
            filters_.set_result_count(result_count_);
            filters_.open(applied_, feedback);
        }
        else if (input.is_pressed(Action::west) && !transition_.running())
        {
            ui::SearchFilters draft = applied_;
            const auto found = std::ranges::find(draft.tags, kFavoriteTag);
            if (found == draft.tags.end())
                draft.tags.push_back(kFavoriteTag);
            else
                draft.tags.erase(found);
            apply(std::move(draft));
            ui::play_cue(feedback, shelves_.style, audio::Cue::toggle);
        }
        else if (input.is_pressed(Action::jump_next) && !transition_.running())
        {
            if (shelves_.select(shelf_of(kResume), kResume))
                ui::play_cue(feedback, shelves_.style, audio::Cue::focus);
        }
        else if (shelves_.handle(input, feedback) == ui::Event::activated)
        {
            const auto chosen = shelves_.selection();
            if (chosen)
            {
                hero_cell_ = chosen;
                hero_.title = kTitles[art_index(chosen->item)];
                hero_.metadata = metadata(chosen->item);
                hero_.placeholder = placeholder(chosen->item);
                hero_.set_active(true);
                shelves_.set_active(false);
                transition_.set_bounds(shelves_.focused_art(), kHero);
                transition_.set_expanded(true);
            }
        }

        shelves_.update(dt);
        transition_.update(dt, reduced_);
        if (!transition_.running() && !transition_.expanded())
            hero_cell_.reset();
        if (hero_cell_)
        {
            const Rect source = shelves_.art_rect(*hero_cell_);
            if (source.w > 0.0f)
                transition_.set_bounds(source, kHero);
        }
        update_artwork();
        update_activity();
        activity_.update(dt);
        const auto current = shelves_.selection();
        const ui::ShelfId item = current ? current->item : 0;
        if (item != preview_id_)
        {
            preview_id_ = item;
            preview_.placeholder = item == 0 ? theme_.surface : placeholder(item);
            preview_.title = item == 0 ? "No titles match" : kTitles[art_index(item)];
            preview_.metadata = item == 0 ? "Change or clear your filters" : metadata(item);
            heading_shown_.snap(0.0f);
        }
        heading_shown_.target = 1.0f;
        heading_shown_.update(dt, reduced_ ? 42.0f : 22.0f);
        const auto tint = item ? placeholder(item) : theme_.surface;
        if (reduced_)
            room_light_.snap(tint);
        else
        {
            room_light_.target(tint);
            room_light_.update(dt, 7.0f);
            backdrop_time_ += dt;
        }
        preview_.update(dt);
        hero_.update(dt);
        filters_.update(dt);
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop = theme_.backdrop;
        const auto light = room_light_.value();
        frame.backdrop.colors[0] = gfx::mix(theme_.page, light, 0.39f);
        frame.backdrop.colors[2] = light;
        frame.backdrop.time = backdrop_time_;
        ui::Canvas canvas{frame.scene, context_.fonts};
        ui::Painter paint(canvas.list, canvas.fonts, theme_);
        const float amount = transition_.progress();
        paint.heading("Library", 96, 108, 58.0f, theme_.text);
        paint.label(std::to_string(result_count_) + " titles", 1824, 102, 24.0f, theme_.text_muted,
                    gfx::Align::right);
        if (amount < 1.0f)
        {
            canvas.list.push_opacity(1.0f - amount);
            canvas.list.shadow(kPreview, theme_.radius_card, 20.0f, theme_.shadow);
            preview_.draw_artwork(canvas, kPreview);
            canvas.list.gradient_rect_h(kPreview, theme_.radius_card, Color::rgb(0x100d13, 0.99f),
                                        Color::rgb(0x100d13, 0.10f));
            canvas.list.push_opacity(heading_shown_.value);
            paint.label(preview_.metadata, 140, 193, 24.0f, theme_.primary);
            paint.heading(fit_preview_title(canvas), 138, 280, 68.0f, theme_.text);
            if (preview_id_)
            {
                ui::paragraph(canvas.list, canvas.fonts.regular,
                              kDescriptions[art_index(preview_id_)], 140, 350, 27.0f, 650.0f, 36.0f,
                              theme_.text, 2);
            }
            canvas.list.pop_opacity();
            shelves_.draw(canvas);
            canvas.list.pop_opacity();
        }
        if (amount > 0.0f && hero_cell_)
        {
            if (reduced_)
                canvas.list.push_opacity(amount);
            hero_.draw_artwork(canvas, reduced_ ? kHero : transition_.bounds());
            if (reduced_)
                canvas.list.pop_opacity();
            canvas.list.push_opacity(tween::smoothstep((amount - 0.35f) / 0.65f));
            hero_.draw_content(canvas);
            const std::string_view details =
                played_ ? "Preview selected" : kDescriptions[art_index(hero_cell_->item)];
            ui::paragraph(canvas.list, canvas.fonts.regular, details, 96, 848, 30.0f, 1220.0f,
                          42.0f, theme_.text, 2);
            if (hero_.artwork.status == gfx::ArtworkStatus::unavailable)
                paint.body("Cover unavailable", 1824, 848, 24.0f, theme_.text_muted,
                           gfx::Align::right);
            canvas.list.pop_opacity();
        }

        ui::Canvas above{frame.overlay, context_.fonts};
        if (activity_visible_ && !filters_.is_open() && !hero_cell_)
            activity_.draw(above);
        filters_.draw(above);
        ui::HintLayout layout;
        layout.size *= theme_.text_scale;
        layout.text_size *= theme_.text_scale;
        const ui::GlyphStyle glyphs = ui::GlyphStyle::dark(context_.settings.swap_confirm);
        if (activity_.expanded())
        {
            const ui::Hint hints[] = {
                {ui::Button::confirm,
                 activity_job_.state == ui::TransferState::failed ? "Retry" : "Close"},
                {ui::Button::back, "Back to shelf"}};
            ui::draw_hints(above.list, above.fonts, glyphs, hints, 2, 1824, true, layout);
        }
        else if (transition_.expanded() || filters_.is_open())
        {
            const ui::Hint hints[] = {{ui::Button::confirm, "Choose"}, {ui::Button::back, "Back"}};
            ui::draw_hints(above.list, above.fonts, glyphs, hints, 2, 1824, true, layout);
        }
        else
        {
            const ui::Hint hints[] = {{ui::Button::confirm, "Details"},
                                      {ui::Button::options, "Search"},
                                      {ui::Button::square, "Favorites"},
                                      {ui::Button::r2, "Resume"},
                                      {ui::Button::triangle, "Cover activity"}};
            ui::draw_hints(above.list, above.fonts, glyphs, hints, 5, 1824, true, layout);
        }
    }

    std::span<const app::TourStep> tour() const override
    {
        return tour_;
    }

  private:
    static Color placeholder(ui::ShelfId item)
    {
        // Midtones from the cover plates also tint the room while browsing.
        constexpr std::uint32_t colors[] = {0x9a546f, 0x577660, 0xa55435, 0x5e547b,
                                            0x526347, 0xac603f, 0x3c6286, 0xa36b32,
                                            0x596b80, 0x985f67, 0x38696a, 0x978884};
        static_assert(std::size(colors) == std::size(kTitles));
        return Color::rgb(colors[art_index(item)]);
    }

    static std::string metadata(ui::ShelfId id)
    {
        return std::string(id <= 500    ? "Adventure"
                           : id <= 1000 ? "Discovery"
                                        : "Archive") +
               "  /  " + (id % 3 == 0 ? "2 players" : "1 player") + "  /  No. " +
               std::to_string(id);
    }

    bool matches_item(ui::ShelfId id, const ui::SearchFilters &filters,
                      std::string_view query) const
    {
        if (std::ranges::find(filters.tags, kFavoriteTag) != filters.tags.end() && id % 5 != 1)
            return false;
        if (std::ranges::find(filters.tags, kArchiveTag) != filters.tags.end() && shelf_of(id) != 3)
            return false;
        return query.empty() || folded(title_of(id)).find(query) != std::string::npos;
    }

    std::size_t matches(const ui::SearchFilters &filters) const
    {
        const std::string query = folded(filters.query);
        std::size_t count = 0;
        for (ui::ShelfId id = 1; id <= kItemCount; ++id)
            count += matches_item(id, filters, query);
        return count;
    }

    void restyle()
    {
        reduced_ = context_.settings.reduced_motion;
        theme_ = ui::with_legibility(*ui::find_theme("cinema"), context_.settings);
        for (ui::ComponentStyle *style :
             {static_cast<ui::ComponentStyle *>(&shelves_.style),
              static_cast<ui::ComponentStyle *>(&hero_.style),
              static_cast<ui::ComponentStyle *>(&preview_.style), &filters_.style})
        {
            style->theme = theme_;
            style->reduced_motion = reduced_;
        }
        activity_.style.theme = theme_;
        activity_.style.reduced_motion = reduced_;
        activity_.style.active_label = "Loading covers";
        activity_.style.completed_label = "Covers ready";
        activity_.set_bounds({1100, 142, 690, 250});
        shelves_.set_bounds(kShelves);
        hero_.action = "Select preview";
    }

    void rebuild()
    {
        std::vector<ui::Shelf> data{
            {1, "Adventures", {}}, {2, "Discoveries", {}}, {3, "Archive", {}}};
        result_count_ = 0;
        const std::string query = folded(applied_.query);
        for (ui::ShelfId id = 1; id <= kItemCount; ++id)
        {
            if (!matches_item(id, applied_, query))
                continue;
            ui::CardItem item;
            item.title = kTitles[art_index(id)];
            item.top = placeholder(id);
            item.bottom = theme_.surface;
            const auto row = static_cast<std::size_t>(shelf_of(id) - 1);
            data[row].items.push_back({id, std::move(item)});
            ++result_count_;
        }
        if (applied_.sort == 1)
        {
            for (ui::Shelf &shelf : data)
                std::ranges::stable_sort(shelf.items, {},
                                         [](const ui::ShelfItem &item) -> const std::string &
                                         { return item.card.title; });
        }
        shelves_.set_shelves(std::move(data));
        requested_.clear();
        artwork_dirty_ = true;
    }

    void apply(ui::SearchFilters draft)
    {
        if (draft == applied_)
            return;
        if (applied_ == ui::SearchFilters{})
            filter_bookmark_ = shelves_.selection();
        applied_ = std::move(draft);
        rebuild();
        if (applied_ == ui::SearchFilters{} && filter_bookmark_)
        {
            shelves_.select(filter_bookmark_->shelf, filter_bookmark_->item);
            filter_bookmark_.reset();
        }
    }

    std::string artwork_path(ui::ShelfId id) const
    {
        char name[16];
        std::snprintf(name, sizeof(name), "/%02zu.jpg", art_index(id));
        return std::string(context_.artwork_root) + (id == kMissing ? "/missing.jpg" : name);
    }

    void update_artwork()
    {
        gfx::ArtworkCache *cache = context_.artwork;
        if (!cache)
            return;
        const auto current = shelves_.selection();
        const auto detail_id = hero_cell_ ? hero_cell_->item : 0;
        const auto selected_id = current ? current->item : 0;
        if (artwork_dirty_ || selected_id != requested_id_ || detail_id != requested_detail_)
        {
            for (const ui::ShelfSelection &cell : requested_)
                shelves_.set_artwork(cell, 0, 1.0f);
            requested_.clear();
            shelves_.visit_near(6, [&](const ui::ShelfSelection &cell)
                                { requested_.push_back(cell); });
            std::vector<gfx::ArtworkRequest> requests;
            requests.reserve(requested_.size() + 1);
            if (activity_visible_ && activity_job_.state == ui::TransferState::active)
                for (auto id = activity_first_; id < activity_first_ + 3; ++id)
                    requests.push_back({id, artwork_path(id)});
            if (hero_cell_)
                requests.push_back({detail_id, artwork_path(detail_id)});
            for (const ui::ShelfSelection &cell : requested_)
                requests.push_back({cell.item, artwork_path(cell.item)});
            cache->select(requests);
            requested_id_ = selected_id;
            requested_detail_ = detail_id;
            artwork_dirty_ = false;
        }
        cache->pump(1);
        for (const ui::ShelfSelection &cell : requested_)
        {
            const auto art = cache->get(cell.item);
            shelves_.set_artwork(cell, art.texture, art.aspect);
        }
        preview_.artwork = cache->get(selected_id);
        hero_.artwork = cache->get(detail_id);
    }

    void update_activity()
    {
        if (!activity_visible_ || activity_job_.state != ui::TransferState::active)
            return;
        int ready = 0;
        for (auto id = activity_first_; id < activity_first_ + 3; ++id)
        {
            const auto status = context_.artwork ? context_.artwork->get(id).status
                                                 : gfx::ArtworkStatus::unavailable;
            if (status == gfx::ArtworkStatus::unavailable)
            {
                activity_job_.state = ui::TransferState::failed;
                activity_job_.detail = std::string(kTitles[art_index(id)]) + " cover unavailable.";
                break;
            }
            ready += status == gfx::ArtworkStatus::ready;
        }
        activity_job_.progress = static_cast<float>(ready) / 3.f;
        if (activity_job_.state != ui::TransferState::failed)
        {
            activity_job_.detail = std::to_string(ready) + " of 3 covers loaded";
            if (ready == 3)
                activity_job_.state = ui::TransferState::completed;
        }
        activity_.set_job(activity_job_);
    }

    std::string fit_preview_title(ui::Canvas &canvas) const
    {
        return canvas.fonts.display.font->fit(preview_.title, 68.0f, 1050.0f);
    }

    void build_tour()
    {
        tour_.push_back({0.4f, 0, Direction::right});
        tour_.push_back({0.10f, 0, Direction::left, "focus-right"});
        tour_.push_back({0.10f, 0, Direction::none, "focus-first"});
        for (int i = 0; i < 18; ++i)
        {
            app::TourStep step{0.11f, 0, Direction::right};
            step.hold = action_bit(Action::right);
            step.nav_repeat = i != 0;
            tour_.push_back(step);
        }
        tour_.insert(tour_.end(),
                     {
                         {0.5f, action_bit(Action::confirm), Direction::none, "uncached-browse"},
                         {0.13f, action_bit(Action::back), Direction::none, "expanding"},
                         {0.10f, 0, Direction::right, "reversing"},
                         {0.75f, action_bit(Action::jump_next)},
                         {0.9f, action_bit(Action::confirm), Direction::none, "restored-1000"},
                         {1.2f, action_bit(Action::back), Direction::none, "bright-detail"},
                         {0.9f, action_bit(Action::west)},
                         {0.9f, action_bit(Action::west), Direction::none, "filtered"},
                         {0.9f, 0, Direction::down, "filter-restored-1000"},
                         {0.8f, action_bit(Action::confirm)},
                         {1.1f, action_bit(Action::back), Direction::none, "missing-cover"},
                         {0.9f, action_bit(Action::menu)},
                         {0.6f, action_bit(Action::back), Direction::none, "search"},
                         {0.7f, action_bit(Action::north)},
                         {0.7f, action_bit(Action::back), Direction::none, "cover-activity"},
                         {0.7f, 0, Direction::none, "activity-collapsed"},
                     });
    }

    app::Context &context_;
    ui::Theme theme_{};
    ui::ShelfBrowser shelves_;
    ui::ActivityPill activity_;
    ui::TransferJob activity_job_;
    ui::ShelfId activity_first_ = 1;
    bool activity_visible_ = false;
    ui::ArtworkDetailHeader preview_;
    ui::ArtworkDetailHeader hero_;
    ui::SearchFilterPanel filters_;
    ui::SharedElementTransition transition_;
    tween::Spring heading_shown_;
    ui::SpringColor room_light_;
    float backdrop_time_ = 0.0f;
    ui::SearchFilters applied_;
    ui::SearchFilters counted_;
    std::optional<ui::ShelfSelection> hero_cell_;
    std::optional<ui::ShelfSelection> filter_bookmark_;
    std::vector<ui::ShelfSelection> requested_;
    std::vector<app::TourStep> tour_;
    std::size_t result_count_ = 0;
    ui::ShelfId preview_id_ = 0;
    ui::ShelfId requested_id_ = 0;
    ui::ShelfId requested_detail_ = 0;
    bool reduced_ = false;
    bool artwork_dirty_ = true;
    bool played_ = false;
};

} // namespace

std::unique_ptr<app::Concept> make_library(app::Context &context)
{
    return std::make_unique<Library>(context);
}

} // namespace hui::concepts
