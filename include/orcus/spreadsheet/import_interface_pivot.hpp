/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ORCUS_SPREADSHEET_IMPORT_INTERFACE_PIVOT_HPP
#define INCLUDED_ORCUS_SPREADSHEET_IMPORT_INTERFACE_PIVOT_HPP

#include <cstdlib>

#include "orcus/spreadsheet/types.hpp"
#include "orcus/types.hpp"
#include "orcus/env.hpp"

// NB: This header must not depend on ixion, as it needs to be usable for
// those clients that provide their own formula engine.  Other headers in
// the orcus::spreadsheet namespace may depend on ixion.

namespace orcus { namespace spreadsheet { namespace iface {

class import_pivot_cache_field_group;

/**
 * Interface for importing pivot cache definition.
 */
class ORCUS_DLLPUBLIC import_pivot_cache_definition
{
public:
    virtual ~import_pivot_cache_definition();

    virtual void set_worksheet_source(
        const char* ref, size_t n_ref, const char* sheet_name, size_t n_sheet_name) = 0;

    virtual void set_field_count(size_t n) = 0;

    virtual void set_field_name(const char* p, size_t n) = 0;

    virtual void set_field_min_value(double v) = 0;

    virtual void set_field_max_value(double v) = 0;

    virtual void set_field_min_date(const date_time_t& dt) = 0;

    virtual void set_field_max_date(const date_time_t& dt) = 0;

    /**
     * Mark the current field as a group field.
     *
     * This method gets called first to signify that the current field is a
     * group field.
     *
     * @param base_index 0-based index of the field this field is the parent
     *                   group of.
     * @return interface for importing group field data.
     */
    virtual import_pivot_cache_field_group* create_field_group(size_t base_index) = 0;

    virtual void commit_field() = 0;

    virtual void set_field_item_string(const char* p, size_t n) = 0;

    virtual void set_field_item_numeric(double v) = 0;

    virtual void set_field_item_datetime(const date_time_t& dt) = 0;

    virtual void commit_field_item() = 0;

    virtual void commit() = 0;
};

/**
 * Interface for importing group field settings in a pivot cache.
 */
class ORCUS_DLLPUBLIC import_pivot_cache_field_group
{
public:
    virtual ~import_pivot_cache_field_group();

    /**
     * Establish a linkage between a base item to a group item.
     *
     * The index to corresponding base item is inferred from the order of this
     * method being called; the first call to this method implies a base item
     * index of 0, the second call implies an index of 1, and so on.
     *
     * This method is called only for a non-range group field; a group field
     * where parent-to-child item relationships are manually defined.
     *
     * @param group_item_index 0-based index for the group item.
     */
    virtual void link_base_to_group_items(size_t group_item_index) = 0;

    /**
     * Set an individual field item value that is of string type to the
     * current internal buffer.
     *
     * This method can be called either for a range group field or a non-range
     * one.
     *
     * @param p pointer to a char array.
     * @param n size of the array.
     */
    virtual void set_field_item_string(const char* p, size_t n) = 0;

    /**
     * Set an individual field item value that is of numeric type to the
     * current internal buffer.
     *
     * This method can be called either for a range group field or a non-range
     * one.
     *
     * @param v field item value.
     */
    virtual void set_field_item_numeric(double v) = 0;

    /**
     * Commit the current internal field item buffer to the group.
     */
    virtual void commit_field_item() = 0;

    /**
     * Set the range grouping type.
     *
     * The current group field implicitly becomes a range group field when
     * this method is called.
     *
     * @param group_by type of range grouping.
     */
    virtual void set_range_grouping_type(pivot_cache_group_by_t group_by) = 0;

    /**
     * Set whether the current range group field has an automatic start
     * position.
     *
     * The current group field implicitly becomes a range group field when
     * this method is called.
     *
     * @param b whether or not the current range group field has an automatic
     *          start position.
     */
    virtual void set_range_auto_start(bool b) = 0;

    /**
     * Set whether the current range group field has an automatic end
     * position.
     *
     * The current group field implicitly becomes a range group field when
     * this method is called.
     *
     * @param b whether or not the current range group field has an automatic
     *          end position.
     */
    virtual void set_range_auto_end(bool b) = 0;

    virtual void set_range_start_number(double v) = 0;

    virtual void set_range_end_number(double v) = 0;

    virtual void set_range_start_date(const date_time_t& dt) = 0;

    virtual void set_range_end_date(const date_time_t& dt) = 0;

    virtual void set_range_interval(double v) = 0;

    /**
     * Commit the current field group data to the parent field.
     */
    virtual void commit() = 0;
};

}}}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
