/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/types.hpp"

#include <limits>
#include <sstream>

namespace orcus {

const xmlns_id_t XMLNS_UNKNOWN_ID = nullptr;
const xml_token_t XML_UNKNOWN_TOKEN = 0;

const size_t index_not_found = std::numeric_limits<size_t>::max();
const size_t unspecified = std::numeric_limits<size_t>::max()-1;

xml_token_attr_t::xml_token_attr_t() :
    ns(XMLNS_UNKNOWN_ID), name(XML_UNKNOWN_TOKEN), transient(false) {}

xml_token_attr_t::xml_token_attr_t(
    xmlns_id_t _ns, xml_token_t _name, const pstring& _value, bool _transient) :
    ns(_ns), name(_name), value(_value), transient(_transient) {}

xml_token_attr_t::xml_token_attr_t(
    xmlns_id_t _ns, xml_token_t _name, const pstring& _raw_name, const pstring& _value, bool _transient) :
    ns(_ns), name(_name), raw_name(_raw_name), value(_value), transient(_transient) {}

xml_token_element_t::xml_token_element_t() : ns(nullptr), name(XML_UNKNOWN_TOKEN) {}

xml_token_element_t::xml_token_element_t(
    xmlns_id_t _ns, xml_token_t _name, const pstring& _raw_name, std::vector<xml_token_attr_t>&& _attrs)  :
    ns(_ns), name(_name), raw_name(_raw_name), attrs(std::move(_attrs)) {}

xml_token_element_t::xml_token_element_t(const xml_token_element_t& other) :
    ns(other.ns), name(other.name), raw_name(other.raw_name), attrs(other.attrs) {}

xml_token_element_t::xml_token_element_t(xml_token_element_t&& other) :
    ns(other.ns), name(other.name), raw_name(other.raw_name), attrs(std::move(other.attrs)) {}

xml_declaration_t::xml_declaration_t() :
    version_major(1),
    version_minor(0),
    encoding(xml_encoding_t::unspecified),
    standalone(false) {}

xml_declaration_t::xml_declaration_t(const xml_declaration_t& other) :
    version_major(other.version_major),
    version_minor(other.version_minor),
    encoding(other.encoding),
    standalone(other.standalone) {}

xml_declaration_t::~xml_declaration_t() {}

xml_declaration_t& xml_declaration_t::operator= (const xml_declaration_t& other)
{
    version_major = other.version_major;
    version_minor = other.version_minor;
    encoding = other.encoding;
    standalone = other.standalone;
    return *this;
}

length_t::length_t() : unit(length_unit_t::unknown), value(0.0) {}

std::string length_t::to_string() const
{
    std::ostringstream os;
    os << value;

    switch (unit)
    {
        case length_unit_t::centimeter:
            os << " cm";
        break;
        case length_unit_t::inch:
            os << " in";
        break;
        case length_unit_t::point:
            os << " pt";
        break;
        case length_unit_t::twip:
            os << " twip";
        break;
        case length_unit_t::unknown:
        default:
            ;
    }

    return os.str();
}

date_time_t::date_time_t() :
    year(0), month(0), day(0), hour(0), minute(0), second(0.0) {}

date_time_t::date_time_t(int _year, int _month, int _day) :
    year(_year), month(_month), day(_day), hour(0), minute(0), second(0.0) {}

date_time_t::date_time_t(int _year, int _month, int _day, int _hour, int _minute, double _second) :
    year(_year), month(_month), day(_day), hour(_hour), minute(_minute), second(_second) {}

date_time_t::date_time_t(const date_time_t& other) :
    year(other.year),
    month(other.month),
    day(other.day),
    hour(other.hour),
    minute(other.minute),
    second(other.second) {}

date_time_t::~date_time_t() {}

date_time_t& date_time_t::operator= (date_time_t other)
{
    swap(other);
    return *this;
}

void date_time_t::swap(date_time_t& other)
{
    std::swap(year, other.year);
    std::swap(month, other.month);
    std::swap(day, other.day);
    std::swap(hour, other.hour);
    std::swap(minute, other.minute);
    std::swap(second, other.second);
}

bool date_time_t::operator== (const date_time_t& other) const
{
    return year == other.year && month == other.month && day == other.day &&
        hour == other.hour && minute == other.minute && second == other.second;
}

bool date_time_t::operator!= (const date_time_t& other) const
{
    return !operator== (other);
}

std::string date_time_t::to_string() const
{
    std::ostringstream os;
    os << year << "-" << month << "-" << day << "T" << hour << ":" << minute << ":" << second;
    return os.str();
}

std::ostream& operator<< (std::ostream& os, const date_time_t& v)
{
    os << v.to_string();
    return os;
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
