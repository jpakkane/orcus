/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/parser_global.hpp"
#include "orcus/cell_buffer.hpp"

#include <cassert>

namespace orcus {

bool is_blank(char c)
{
    return is_in(c, " \t\n\r");
}

bool is_alpha(char c)
{
    if ('a' <= c && c <= 'z')
        return true;
    if ('A' <= c && c <= 'Z')
        return true;
    return false;
}

bool is_name_char(char c)
{
    return is_in(c, "-_");
}

bool is_numeric(char c)
{
    return ('0' <= c && c <= '9');
}

bool is_in(char c, const char* allowed)
{
    for (; *allowed != '\0'; ++allowed)
    {
        if (c == *allowed)
            return true;
    }
    return false;
}

void write_to(std::ostringstream& os, const char* p, size_t n)
{
    if (!p)
        return;

    const char* pend = p + n;
    for (; p != pend; ++p)
        os << *p;
}

double parse_numeric(const char*& p, size_t max_length)
{
    const char* p_end = p + max_length;
    double ret = 0.0, divisor = 1.0;
    bool negative_sign = false;
    bool before_decimal_pt = true;

    // Check for presence of a sign.
    if (p != p_end)
    {
        switch (*p)
        {
            case '+':
                ++p;
            break;
            case '-':
                negative_sign = true;
                ++p;
            break;
            default:
                ;
        }
    }

    for (; p != p_end; ++p)
    {
        if (*p == '.')
        {
            if (!before_decimal_pt)
            {
                // Second '.' encountered. Terminate the parsing.
                ret /= divisor;
                return negative_sign ? -ret : ret;
            }

            before_decimal_pt = false;
            continue;
        }

        if (*p < '0' || '9' < *p)
        {
            ret /= divisor;
            return negative_sign ? -ret : ret;
        }

        ret *= 10.0;
        ret += *p - '0';

        if (!before_decimal_pt)
            divisor *= 10.0;
    }

    ret /= divisor;
    return negative_sign ? -ret : ret;
}

long parse_integer(const char*& p, size_t max_length)
{
    const char* p_end = p + max_length;

    long ret = 0.0;
    bool negative_sign = false;

    // Check for presence of a sign.
    if (p != p_end)
    {
        switch (*p)
        {
            case '+':
                ++p;
            break;
            case '-':
                negative_sign = true;
                ++p;
            break;
            default:
                ;
        }
    }

    for (; p != p_end; ++p)
    {
        if (*p < '0' || '9' < *p)
            return negative_sign ? -ret : ret;

        ret *= 10;
        ret += *p - '0';
    }

    return negative_sign ? -ret : ret;
}

namespace {

enum class escape_char_t
{
    illegal,
    legal,
    control_char
};

/**
 * Given a character that occurs immediately after the escape character '\',
 * return what type this character is.
 *
 * @param c character that occurs immediately after the escape character
 *          '\'.
 *
 * @return enum value representing the type of escape character.
 */
escape_char_t get_escape_char_type(char c)
{
    switch (c)
    {
        case '"':
        case '\\':
        case '/':
            return escape_char_t::legal;
        case 'b': // backspace
        case 'f': // formfeed
        case 'n': // newline
        case 'r': // carriage return
        case 't': // horizontal tab
            return escape_char_t::control_char;
        default:
            ;
    }

    return escape_char_t::illegal;
}

parse_quoted_string_state parse_string_with_escaped_char(
    const char*& p, size_t max_length, const char* p_parsed, size_t n_parsed, char c,
    cell_buffer& buffer)
{
    const char* p_end = p + max_length;

    parse_quoted_string_state ret;
    ret.str = nullptr;
    ret.length = 0;

    // Start the buffer with the string we've parsed so far.
    buffer.reset();
    if (p_parsed && n_parsed)
        buffer.append(p_parsed, n_parsed);
    buffer.append(&c, 1);

    ++p;
    if (p == p_end)
    {
        ret.length = parse_quoted_string_state::error_no_closing_quote;
        return ret;
    }

    size_t len = 0;
    const char* p_head = p;
    bool escape = false;

    for (; p != p_end; ++p, ++len)
    {
        c = *p;

        if (escape)
        {
            escape = false;

            switch (get_escape_char_type(c))
            {
                case escape_char_t::legal:
                    buffer.append(p_head, len-1);
                    buffer.append(&c, 1);
                    ++p;
                    len = 0;
                    p_head = p;
                break;
                case escape_char_t::control_char:
                    // do nothing on control characters.
                break;
                case escape_char_t::illegal:
                default:
                    ret.length = parse_quoted_string_state::error_illegal_escape_char;
                    return ret;
            }
        }

        switch (*p)
        {
            case '"':
                // closing quote.
                buffer.append(p_head, len);
                ++p; // skip the quote.
                ret.str = buffer.get();
                ret.length = buffer.size();
                return ret;
            break;
            case '\\':
            {
                escape = true;
                continue;
            }
            break;
            default:
                ;
        }
    }

    ret.length = parse_quoted_string_state::error_no_closing_quote;
    return ret;
}

}

parse_quoted_string_state parse_quoted_string(
    const char*& p, size_t max_length, cell_buffer& buffer)
{
    assert(*p == '"');
    const char* p_end = p + max_length;
    ++p;

    parse_quoted_string_state ret;
    ret.str = p;
    ret.length = 0;

    if (p == p_end)
    {
        ret.str = nullptr;
        ret.length = parse_quoted_string_state::error_no_closing_quote;
        return ret;
    }

    bool escape = false;

    for (; p != p_end; ++p, ++ret.length)
    {
        if (escape)
        {
            char c = *p;
            escape = false;

            switch (get_escape_char_type(c))
            {
                case escape_char_t::legal:
                    return parse_string_with_escaped_char(p, max_length, ret.str, ret.length-1, c, buffer);
                case escape_char_t::control_char:
                    // do nothing on control characters.
                break;
                case escape_char_t::illegal:
                default:
                    ret.str = nullptr;
                    ret.length = parse_quoted_string_state::error_illegal_escape_char;
                    return ret;
            }
        }

        switch (*p)
        {
            case '"':
                // closing quote.
                ++p; // skip the quote.
                return ret;
            break;
            case '\\':
            {
                escape = true;
                continue;
            }
            break;
            default:
                ;
        }
    }

    ret.str = nullptr;
    ret.length = parse_quoted_string_state::error_no_closing_quote;
    return ret;
}

double clip(double input, double low, double high)
{
    if (input < low)
        input = low;
    if (input > high)
        input = high;
    return input;
}

}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
