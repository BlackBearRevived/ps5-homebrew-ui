// ps5-homebrew-ui - Unicode text editing for controller input.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/text_edit.hpp"

#define UTF8PROC_STATIC
#include "third_party/utf8proc/utf8proc.h"

namespace hui::text_edit
{
namespace
{

struct Scalar
{
    utf8proc_int32_t value;
    std::size_t bytes;
    bool valid;
};

Scalar decode(std::string_view text)
{
    utf8proc_int32_t value = 0;
    const auto size = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t *>(text.data()),
                                       static_cast<utf8proc_ssize_t>(text.size()), &value);
    return size > 0 ? Scalar{value, static_cast<std::size_t>(size), true}
                    : Scalar{0xfffd, 1, false};
}

// The state must see every adjacent pair, including pairs that have a break.
template <typename Visit> void boundaries(std::string_view text, Visit visit)
{
    if (text.empty())
        return;
    Scalar previous = decode(text);
    utf8proc_int32_t state = 0;
    for (std::size_t at = previous.bytes; at < text.size();)
    {
        const Scalar next = decode(text.substr(at));
        if (utf8proc_grapheme_break_stateful(previous.value, next.value, &state) && !visit(at))
            return;
        previous = next;
        at += next.bytes;
    }
    visit(text.size());
}

} // namespace

std::uint32_t first_codepoint(std::string_view text)
{
    return text.empty() ? 0 : static_cast<std::uint32_t>(decode(text).value);
}

int length(std::string_view text)
{
    int result = 0;
    boundaries(text,
               [&](std::size_t)
               {
                   ++result;
                   return true;
               });
    return result;
}

void erase_first(std::string &text)
{
    std::size_t end = 0;
    boundaries(text,
               [&](std::size_t at)
               {
                   end = at;
                   return false;
               });
    text.erase(0, end);
}

void erase_last(std::string &text)
{
    std::size_t start = 0;
    boundaries(text,
               [&](std::size_t at)
               {
                   if (at < text.size())
                       start = at;
                   return true;
               });
    text.resize(start);
}

std::string single_line(std::string_view input)
{
    std::string clean;
    clean.reserve(input.size());
    while (!input.empty())
    {
        const Scalar scalar = decode(input);
        if (!scalar.valid)
            clean.append("\xef\xbf\xbd");
        else if (scalar.value >= 0x20 && scalar.value != 0x7f)
            clean.append(input.substr(0, scalar.bytes));
        input.remove_prefix(scalar.bytes);
    }
    return clean;
}

bool append(std::string &text, std::string_view input, int limit)
{
    std::string joined = text;
    joined.append(single_line(input));
    std::size_t end = joined.size();
    if (limit > 0)
    {
        boundaries(joined,
                   [&](std::size_t at)
                   {
                       if (--limit != 0)
                           return true;
                       end = at;
                       return false;
                   });
    }
    if (end <= text.size())
        return false;
    text.append(joined, text.size(), end - text.size());
    return true;
}

} // namespace hui::text_edit
