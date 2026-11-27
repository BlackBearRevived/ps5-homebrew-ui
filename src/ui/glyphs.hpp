// ps5-homebrew-ui - Controller button glyphs and on-screen hint rows.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/input.hpp"
#include "gfx/draw_list.hpp"
#include "ui/fonts.hpp"

#include <cstdint>

namespace hui::ui
{

// Every controller input the hints show. The glyphs are drawn from shapes,
// so they stay sharp at any size and take the colours of the active design.
enum class Button : std::uint8_t
{
    none,
    cross,
    circle,
    square,
    triangle,
    l1,
    r1,
    l2,
    r2,
    options,
    left_stick,
    right_stick,
    dpad,
    touchpad,
    confirm, // logical action, resolved through GlyphStyle
    back,
};

// The button an action is on. Confirm and back are Cross and Circle unless
// the player swapped them (pass the setting).
inline Button button_for(Action action, bool swap_confirm = false)
{
    switch (action)
    {
    case Action::up:
    case Action::down:
    case Action::left:
    case Action::right:
        return Button::dpad;
    case Action::confirm:
        return swap_confirm ? Button::circle : Button::cross;
    case Action::back:
        return swap_confirm ? Button::cross : Button::circle;
    case Action::north:
        return Button::triangle;
    case Action::west:
        return Button::square;
    case Action::page_prev:
        return Button::l1;
    case Action::page_next:
        return Button::r1;
    case Action::jump_prev:
        return Button::l2;
    case Action::jump_next:
        return Button::r2;
    case Action::menu:
        return Button::options;
    case Action::touch:
        return Button::touchpad;
    case Action::l3:
        return Button::left_stick;
    case Action::r3:
        return Button::right_stick;
    default:
        return Button::none;
    }
}

// How glyphs are coloured. dark() suits dark screens, light() light ones;
// mono() draws the face symbols in the ink colour for strict palettes.
struct GlyphStyle
{
    gfx::Color body;          // the button's cap
    gfx::Color edge;          // its outline
    gfx::Color ink;           // lettering and symbols
    gfx::Color label;         // the text beside a glyph in a hint row
    bool tinted_faces = true; // face symbols in their DualSense colours

    bool swap_confirm = false;

    static GlyphStyle dark(bool swap_confirm = false);
    static GlyphStyle light(bool swap_confirm = false);
    static GlyphStyle mono(gfx::Color ink, gfx::Color body);
};

// Width of a glyph drawn at the given height.
float button_width(Button button, float size);
// Draws a glyph with its left edge at x, vertically centred on cy.
void draw_button(gfx::DrawList &list, const Fonts &fonts, const GlyphStyle &style, Button button,
                 float x, float cy, float size);

struct Hint
{
    Button button;
    const char *label;
    Button second = Button::none; // pairs such as L2 / R2
};

// Geometry of a hint row: glyph height, text size and the centre line.
struct HintLayout
{
    float size = 40.0f;
    float text_size = 26.0f;
    float cy = 1010.0f;
    float item_gap = 44.0f;
    const FontRef *font = nullptr; // label face; the regular one when null
};

// The width draw_hints would draw, for placing a plate behind the row.
float measure_hints(const Fonts &fonts, const Hint *hints, int count,
                    const HintLayout &layout = {});

// Draws a row of [glyph label] hints. With right_align the row ends at x;
// otherwise it starts there. Returns its width.
float draw_hints(gfx::DrawList &list, const Fonts &fonts, const GlyphStyle &style,
                 const Hint *hints, int count, float x, bool right_align,
                 const HintLayout &layout = {});

} // namespace hui::ui
