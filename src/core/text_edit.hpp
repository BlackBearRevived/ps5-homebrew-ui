// ps5-homebrew-ui - Grapheme boundaries shared by text entry components.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace hui::text_edit
{

std::uint32_t first_codepoint(std::string_view text);
int length(std::string_view text);
void erase_first(std::string &text);
void erase_last(std::string &text);

// Drops single-line control characters and replaces malformed UTF-8.
std::string single_line(std::string_view text);
// Appends as many complete graphemes as fit, including when the insertion
// extends the last grapheme already in the field. Zero leaves the limit open.
bool append(std::string &text, std::string_view input, int limit);

} // namespace hui::text_edit
