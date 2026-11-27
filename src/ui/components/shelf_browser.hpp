// ps5-homebrew-ui - Stack of controller-driven shelves with stable focus.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui/components/carousel.hpp"

#include <optional>
#include <unordered_map>

namespace hui::ui
{

using ShelfId = std::uint64_t;

struct ShelfItem
{
    ShelfId id;
    CardItem card;
};

struct Shelf
{
    ShelfId id;
    std::string title;
    std::vector<ShelfItem> items;
};

struct ShelfSelection
{
    ShelfId shelf;
    ShelfId item;
    int shelf_index;
    int item_index;
};

struct ShelfBrowserStyle : ComponentStyle
{
    CarouselStyle carousel;
    float shelf_gap = 32.0f;
};

// Restore with IDs from your catalog, even when a new result set reorders it.
//   browser.set_shelves(result);
//   browser.select(saved_shelf_id, saved_item_id);
//   if (browser.handle(input, feedback) == ui::Event::activated)
//       open(browser.selection()->item);
class ShelfBrowser
{
  public:
    ShelfBrowserStyle style;

    void set_shelves(std::vector<Shelf> shelves);
    // Reapply after changing card dimensions or theme.
    void set_bounds(gfx::Rect bounds);
    bool select(ShelfId shelf, ShelfId item, bool snap = true);
    std::optional<ShelfSelection> selection() const;
    gfx::Rect focused_art() const;
    gfx::Rect art_rect(const ShelfSelection &cell) const;
    void enter();
    void set_active(bool active);
    Event handle(const InputFrame &input, Feedback &feedback);
    void update(float dt);
    void draw(Canvas &canvas) const;

    using Visit = std::function<void(const ShelfSelection &)>;
    // Focus first, then progressively more distant items on adjacent shelves.
    void visit_near(int item_radius, const Visit &visit) const;
    void set_artwork(const ShelfSelection &cell, std::uint32_t texture, float aspect);

  private:
    struct Row
    {
        ShelfId id;
        std::vector<ShelfId> ids;
        Carousel carousel;
    };
    void retain_selection();
    void reveal(bool snap);
    void activate_rows();

    std::vector<Row> rows_;
    std::unordered_map<ShelfId, ShelfId> remembered_;
    gfx::Rect bounds_{96, 320, 1728, 640};
    int focus_ = -1;
    bool active_ = true;
    Scroller scroll_;
    Pulse refused_;
};

} // namespace hui::ui
