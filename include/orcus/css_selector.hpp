/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ORCUS_CSS_SELECTOR_HPP
#define INCLUDED_ORCUS_CSS_SELECTOR_HPP

#include "orcus/env.hpp"
#include "orcus/pstring.hpp"
#include "orcus/css_types.hpp"

#include <ostream>
#include <vector>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

namespace orcus {

struct ORCUS_DLLPUBLIC css_simple_selector_t
{
    typedef boost::unordered_set<pstring, pstring::hash> classes_type;

    pstring name;
    pstring id;
    classes_type classes;
    css::pseudo_class_t pseudo_classes;

    css_simple_selector_t();

    void clear();
    bool empty() const;

    bool operator== (const css_simple_selector_t& r) const;
    bool operator!= (const css_simple_selector_t& r) const;

    struct hash
    {
        size_t operator() (const css_simple_selector_t& ss) const;
    };
};

struct ORCUS_DLLPUBLIC css_chained_simple_selector_t
{
    css::combinator_t combinator;
    css_simple_selector_t simple_selector;

    bool operator== (const css_chained_simple_selector_t& r) const;

    css_chained_simple_selector_t();
    css_chained_simple_selector_t(const css_simple_selector_t& ss);
    css_chained_simple_selector_t(css::combinator_t op, const css_simple_selector_t& ss);
};

/**
 * Each CSS selector consists of one or more chained simple selectors.
 */
struct ORCUS_DLLPUBLIC css_selector_t
{
    typedef std::vector<css_chained_simple_selector_t> chained_type;
    css_simple_selector_t first;
    chained_type chained;

    void clear();

    bool operator== (const css_selector_t& r) const;
};

/**
 * Structure representing a single CSS property value.
 */
struct ORCUS_DLLPUBLIC css_property_value_t
{
    css::property_value_t type;

    union
    {
        /**
         * Pointer to a string value. The actual string value must be stored
         * in the string pool associated with the document tree storage.
         */
        const char* str;

        struct
        {
            union
            {
                struct
                {
                    uint8_t red;   /// 0 to 255
                    uint8_t green; /// 0 to 255
                    uint8_t blue;  /// 0 to 255
                };

                struct
                {
                    uint16_t hue;        /// 0 to 360 where 0-red, 120-green, and 240-blue
                    uint8_t  saturation; /// percentage
                    uint8_t  lightness;  /// percentage
                };
            };

            double  alpha; /// 0 to 1.0
        };
    };

    css_property_value_t();
    css_property_value_t(const css_property_value_t& r);

    css_property_value_t& operator= (const css_property_value_t& r);

    void swap(css_property_value_t& r);
};

typedef boost::unordered_map<pstring, std::vector<pstring>, pstring::hash> css_properties_t;
typedef boost::unordered_map<css::pseudo_element_t, css_properties_t> css_pseudo_element_properties_t;

ORCUS_DLLPUBLIC std::ostream& operator<< (std::ostream& os, const css_simple_selector_t& v);
ORCUS_DLLPUBLIC std::ostream& operator<< (std::ostream& os, const css_selector_t& v);

}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
