/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/spreadsheet/sheet.hpp"

#include "orcus/spreadsheet/styles.hpp"
#include "orcus/spreadsheet/shared_strings.hpp"
#include "orcus/spreadsheet/sheet_properties.hpp"
#include "orcus/spreadsheet/sheet_range.hpp"
#include "orcus/spreadsheet/document.hpp"
#include "orcus/spreadsheet/auto_filter.hpp"

#include "orcus/global.hpp"
#include "orcus/exception.hpp"
#include "orcus/measurement.hpp"
#include "orcus/string_pool.hpp"

#include "data_table.hpp"
#include "table.hpp"
#include "formula_global.hpp"
#include "json_dumper.hpp"
#include "csv_dumper.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <vector>
#include <cassert>
#include <memory>
#include <cstdlib>
#include <unordered_map>

#include <mdds/flat_segment_tree.hpp>
#include <mdds/multi_type_matrix.hpp>

#include <ixion/cell.hpp>
#include <ixion/formula.hpp>
#include <ixion/formula_name_resolver.hpp>
#include <ixion/formula_result.hpp>
#include <ixion/formula_tokens.hpp>
#include <ixion/matrix.hpp>
#include <ixion/model_context.hpp>
#include <ixion/address.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/greg_date.hpp>

#define ORCUS_DEBUG_SHEET 0

using namespace std;
using namespace boost;

namespace orcus { namespace spreadsheet {

namespace {

struct merge_size
{
    col_t width;
    row_t height;

    merge_size(col_t _width, row_t _height) :
        width(_width), height(_height) {}
};

// Merged cell data stored in sheet.
typedef std::unordered_map<row_t, merge_size> merge_size_type;
typedef std::unordered_map<col_t, merge_size_type*> col_merge_size_type;

// Overlapped cells per row, used when rendering sheet content.
typedef mdds::flat_segment_tree<col_t, bool> overlapped_col_index_type;
typedef std::unordered_map<row_t, overlapped_col_index_type*> overlapped_cells_type;

typedef mdds::flat_segment_tree<row_t, size_t>  segment_row_index_type;
typedef std::unordered_map<col_t, segment_row_index_type*> cell_format_type;

// Widths and heights are stored in twips.
typedef mdds::flat_segment_tree<col_t, col_width_t> col_widths_store_type;
typedef mdds::flat_segment_tree<row_t, row_height_t> row_heights_store_type;

// hidden information
typedef mdds::flat_segment_tree<col_t, bool> col_hidden_store_type;
typedef mdds::flat_segment_tree<row_t, bool> row_hidden_store_type;

struct overlapped_cells_tree_builder : std::unary_function<overlapped_cells_type::value_type, void>
{
    void operator() (overlapped_cells_type::value_type& node)
    {
        node.second->build_tree();
    }
};

class sheet_auto_filter : public orcus::spreadsheet::iface::import_auto_filter
{
    sheet& m_sheet;
    orcus::string_pool& m_string_pool;
    const ixion::formula_name_resolver* mp_resolver;
    std::unique_ptr<auto_filter_t> mp_data;
    col_t m_cur_col;
    auto_filter_column_t m_cur_col_data;

public:
    sheet_auto_filter(sheet& sh, orcus::string_pool& sp) :
        m_sheet(sh),
        m_string_pool(sp),
        mp_resolver(nullptr),
        m_cur_col(-1) {}

    void reset()
    {
        mp_resolver = nullptr;
        mp_data.reset(new auto_filter_t);
        m_cur_col = -1;
        m_cur_col_data.reset();
    }

    void set_resolver(const ixion::formula_name_resolver* resolver)
    {
        mp_resolver = resolver;
    }

    virtual void set_range(const char* p_ref, size_t n_ref)
    {
        if (!mp_resolver)
            return;

        mp_data->range = to_abs_range(*mp_resolver, p_ref, n_ref);
    }

    virtual void set_column(orcus::spreadsheet::col_t col)
    {
        m_cur_col = col;
    }

    virtual void append_column_match_value(const char* p, size_t n)
    {
        // The string pool belongs to the document.
        pstring s = m_string_pool.intern(p, n).first;
        m_cur_col_data.match_values.insert(s);
    }

    virtual void commit_column()
    {
        if (!mp_data)
            return;

        mp_data->commit_column(m_cur_col, m_cur_col_data);
        m_cur_col_data.reset();
    }

    virtual void commit()
    {
        m_sheet.set_auto_filter_data(mp_data.release());
    }
};

}

struct sheet_impl
{
    document& m_doc;
    sheet_properties m_sheet_props;     /// sheet properties import interface.
    data_table m_data_table;            /// data table import interface.
    sheet_auto_filter m_auto_filter;    /// auto filter import interface.
    table m_table;                      /// table import interface.

    mutable col_widths_store_type m_col_widths;
    mutable row_heights_store_type m_row_heights;
    col_widths_store_type::const_iterator m_col_width_pos;
    row_heights_store_type::const_iterator m_row_height_pos;

    mutable col_hidden_store_type m_col_hidden;
    mutable row_hidden_store_type m_row_hidden;
    col_hidden_store_type::const_iterator m_col_hidden_pos;
    row_hidden_store_type::const_iterator m_row_hidden_pos;

    col_merge_size_type m_merge_ranges; /// 2-dimensional merged cell ranges.
    overlapped_cells_type m_overlapped_ranges;

    std::unique_ptr<auto_filter_t> mp_auto_filter_data;

    cell_format_type m_cell_formats;
    row_t m_row_size;
    col_t m_col_size;
    const sheet_t m_sheet; /// sheet ID

    sheet_impl() = delete;
    sheet_impl(const sheet_impl&) = delete;
    sheet_impl& operator=(const sheet_impl&) = delete;

    sheet_impl(document& doc, sheet& sh, sheet_t sheet_index, row_t row_size, col_t col_size) :
        m_doc(doc),
        m_sheet_props(doc, sh), m_data_table(sh),
        m_auto_filter(sh, doc.get_string_pool()),
        m_table(doc, sh),
        m_col_widths(0, col_size, get_default_column_width()),
        m_row_heights(0, row_size, get_default_row_height()),
        m_col_width_pos(m_col_widths.begin()),
        m_row_height_pos(m_row_heights.begin()),
        m_col_hidden(0, col_size, false),
        m_row_hidden(0, row_size, false),
        m_col_hidden_pos(m_col_hidden.begin()),
        m_row_hidden_pos(m_row_hidden.begin()),
        m_row_size(row_size), m_col_size(col_size), m_sheet(sheet_index) {}

    ~sheet_impl()
    {
        for_each(m_cell_formats.begin(), m_cell_formats.end(),
                 map_object_deleter<cell_format_type>());
        for_each(m_merge_ranges.begin(), m_merge_ranges.end(),
                 map_object_deleter<col_merge_size_type>());

        clear_overlapped_ranges();
    }

    const merge_size* get_merge_size(row_t row, col_t col) const
    {
        col_merge_size_type::const_iterator it_col = m_merge_ranges.find(col);
        if (it_col == m_merge_ranges.end())
            return nullptr;

        merge_size_type& col_merge_sizes = *it_col->second;
        merge_size_type::const_iterator it_row = col_merge_sizes.find(row);
        if (it_row == col_merge_sizes.end())
            return nullptr;

        return &it_row->second;
    }

    const overlapped_col_index_type* get_overlapped_ranges(row_t row) const
    {
        overlapped_cells_type::const_iterator it = m_overlapped_ranges.find(row);
        if (it == m_overlapped_ranges.end())
            return nullptr;

        return it->second;
    }

    void clear_overlapped_ranges()
    {
        for_each(m_overlapped_ranges.begin(), m_overlapped_ranges.end(),
                 map_object_deleter<overlapped_cells_type>());
        m_overlapped_ranges.clear();
    }

    void update_overlapped_ranges()
    {
        clear_overlapped_ranges();

        col_merge_size_type::const_iterator it_col = m_merge_ranges.begin(), it_col_end = m_merge_ranges.end();
        for (; it_col != it_col_end; ++it_col)
        {
            col_t col = it_col->first;
            const merge_size_type& data = *it_col->second;
            merge_size_type::const_iterator it = data.begin(), it_end = data.end();
            for (; it != it_end; ++it)
            {
                row_t row = it->first;
                const merge_size& item = it->second;
                for (row_t i = 0; i < item.height; ++i, ++row)
                {
                    // Get the container for this row.
                    overlapped_cells_type::iterator it_cont = m_overlapped_ranges.find(row);
                    if (it_cont == m_overlapped_ranges.end())
                    {
                        std::unique_ptr<overlapped_col_index_type> p(new overlapped_col_index_type(0, m_col_size, false));
                        std::pair<overlapped_cells_type::iterator, bool> r =
                            m_overlapped_ranges.insert(overlapped_cells_type::value_type(row, p.get()));

                        if (!r.second)
                        {
                            // Insertion failed.
                            return;
                        }

                        p.release();
                        it_cont = r.first;
                    }

                    overlapped_col_index_type& cont = *it_cont->second;
                    cont.insert_back(col, col+item.width, true);
                }
            }
        }

        // Build trees.
        for_each(m_overlapped_ranges.begin(), m_overlapped_ranges.end(),
                 overlapped_cells_tree_builder());
    }

    ixion::abs_range_t get_data_range() const
    {
        const ixion::model_context& cxt = m_doc.get_model_context();
        return cxt.get_data_range(m_sheet);
    }
};

const row_t sheet::max_row_limit = 1048575;
const col_t sheet::max_col_limit = 1023;

sheet::sheet(document& doc, sheet_t sheet_index, row_t row_size, col_t col_size) :
    mp_impl(new sheet_impl(doc, *this, sheet_index, row_size, col_size)) {}

sheet::~sheet()
{
    delete mp_impl;
}

iface::import_sheet_properties* sheet::get_sheet_properties()
{
    return &mp_impl->m_sheet_props;
}

iface::import_data_table* sheet::get_data_table()
{
    mp_impl->m_data_table.reset();
    return &mp_impl->m_data_table;
}

iface::import_table* sheet::get_table()
{
    mp_impl->m_table.reset();
    return &mp_impl->m_table;
}

iface::import_auto_filter* sheet::get_auto_filter()
{
    mp_impl->m_auto_filter.reset();
    mp_impl->m_auto_filter.set_resolver(mp_impl->m_doc.get_formula_name_resolver());
    return &mp_impl->m_auto_filter;
}

void sheet::set_auto(row_t row, col_t col, const char* p, size_t n)
{
    if (!p || !n)
        return;

    ixion::model_context& cxt = mp_impl->m_doc.get_model_context();

    // First, see if this can be parsed as a number.
    char* endptr = nullptr;
    double val = strtod(p, &endptr);
    const char* endptr_check = p + n;
    if (endptr == endptr_check)
        // Treat this as a numeric value.
        cxt.set_numeric_cell(ixion::abs_address_t(mp_impl->m_sheet,row,col), val);
    else
        // Treat this as a string value.
        cxt.set_string_cell(ixion::abs_address_t(mp_impl->m_sheet,row,col), p, n);
}

void sheet::set_string(row_t row, col_t col, size_t sindex)
{
    ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    cxt.set_string_cell(ixion::abs_address_t(mp_impl->m_sheet,row,col), sindex);

#if ORCUS_DEBUG_SHEET
    cout << "sheet::set_string: sheet=" << mp_impl->m_sheet << ", row=" << row << ", col=" << col << ", si=" << sindex << endl;
#endif
}

void sheet::set_value(row_t row, col_t col, double value)
{
    ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    cxt.set_numeric_cell(ixion::abs_address_t(mp_impl->m_sheet,row,col), value);
}

void sheet::set_bool(row_t row, col_t col, bool value)
{
    ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    cxt.set_boolean_cell(ixion::abs_address_t(mp_impl->m_sheet,row,col), value);
}

void sheet::set_date_time(row_t row, col_t col, int year, int month, int day, int hour, int minute, double second)
{
    // Convert this to a double value representing days since epoch.

    date_time_t dt_origin = mp_impl->m_doc.get_origin_date();

    gregorian::date origin(dt_origin.year, dt_origin.month, dt_origin.day);
    gregorian::date d(year, month, day);

    double days_since_epoch = (d - origin).days();

    double ms = second * 1000000.0;

    posix_time::time_duration t(
        posix_time::hours(hour) +
        posix_time::minutes(minute) +
        posix_time::microseconds(ms)
    );

    double time_as_day = t.total_microseconds();
    time_as_day /= 1000000.0; // microseconds to seconds
    time_as_day /= 60.0 * 60.0 * 24.0; // seconds to day

    set_value(row, col, days_since_epoch + time_as_day);
}

void sheet::set_format(row_t row, col_t col, size_t index)
{
    set_format(row, col, row, col, index);
}

void sheet::set_format(row_t row_start, col_t col_start, row_t row_end, col_t col_end, size_t index)
{
    for (col_t col = col_start; col <= col_end; ++col)
    {
        cell_format_type::iterator itr = mp_impl->m_cell_formats.find(col);
        if (itr == mp_impl->m_cell_formats.end())
        {
            std::unique_ptr<segment_row_index_type> p(new segment_row_index_type(0, mp_impl->m_row_size+1, 0));

            pair<cell_format_type::iterator, bool> r =
                mp_impl->m_cell_formats.insert(cell_format_type::value_type(col, p.get()));

            if (!r.second)
            {
                cerr << "insertion of new cell format container failed!" << endl;
                return;
            }

            p.release();
            itr = r.first;
        }

        segment_row_index_type& con = *itr->second;
        con.insert_back(row_start, row_end+1, index);
    }
}

void sheet::set_formula(row_t row, col_t col, formula_grammar_t grammar,
                        const char* p, size_t n)
{
    const ixion::formula_name_resolver* resolver = mp_impl->m_doc.get_formula_name_resolver();
    if (!resolver)
        return;

    // Tokenize the formula string and store it.
    ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    ixion::abs_address_t pos(mp_impl->m_sheet, row, col);
    cxt.set_formula_cell(pos, p, n, *resolver);
    ixion::register_formula_cell(cxt, pos);
    mp_impl->m_doc.insert_dirty_cell(pos);
}

void sheet::set_shared_formula(
    row_t row, col_t col, formula_grammar_t grammar, size_t sindex,
    const char* p_formula, size_t n_formula, const char* p_range, size_t n_range)
{
    const ixion::formula_name_resolver* resolver = mp_impl->m_doc.get_formula_name_resolver();
    if (!resolver)
        return;

    ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    ixion::abs_address_t pos(mp_impl->m_sheet, row, col);
    cxt.set_shared_formula(pos, sindex, p_formula, n_formula, p_range, n_range, *resolver);
    set_shared_formula(row, col, sindex);
}

void sheet::set_shared_formula(
    row_t row, col_t col, formula_grammar_t grammar, size_t sindex,
    const char* p_formula, size_t n_formula)
{
    const ixion::formula_name_resolver* resolver = mp_impl->m_doc.get_formula_name_resolver();
    if (!resolver)
        return;

    ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    ixion::abs_address_t pos(mp_impl->m_sheet, row, col);
    cxt.set_shared_formula(pos, sindex, p_formula, n_formula, *resolver);
    set_shared_formula(row, col, sindex);
}

void sheet::set_shared_formula(row_t row, col_t col, size_t sindex)
{
    ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    ixion::abs_address_t pos(mp_impl->m_sheet, row, col);
    cxt.set_formula_cell(pos, sindex, true);
    ixion::register_formula_cell(cxt, pos);
    mp_impl->m_doc.insert_dirty_cell(pos);
}

void sheet::set_array_formula(
    row_t row, col_t col, formula_grammar_t grammar,
    const char* p, size_t n, row_t array_rows, col_t array_cols)
{
}

void sheet::set_array_formula(
    row_t row, col_t col, formula_grammar_t grammar,
    const char* p, size_t n, const char* p_range, size_t n_range)
{
}

void sheet::set_formula_result(row_t row, col_t col, double value)
{
}

void sheet::set_formula_result(row_t row, col_t col, const char* p, size_t n)
{
}

void sheet::write_string(ostream& os, row_t row, col_t col) const
{
    const ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    ixion::abs_address_t pos(mp_impl->m_sheet, row, col);
    switch (cxt.get_celltype(pos))
    {
        case ixion::celltype_t::string:
        {
            size_t str_id = cxt.get_string_identifier(pos);
            const string* p = cxt.get_string(str_id);
            if (p)
                os << *p;
        }
        break;
        case ixion::celltype_t::numeric:
            os << cxt.get_numeric_value(pos);
        break;
        default:
            ;
    }
}

void sheet::set_col_width(col_t col, col_width_t width)
{
    mp_impl->m_col_width_pos =
        mp_impl->m_col_widths.insert(mp_impl->m_col_width_pos, col, col+1, width).first;
}

col_width_t sheet::get_col_width(col_t col, col_t* col_start, col_t* col_end) const
{
    col_widths_store_type& col_widths = mp_impl->m_col_widths;
    if (!col_widths.is_tree_valid())
        col_widths.build_tree();

    col_width_t ret = 0;
    if (!col_widths.search_tree(col, ret, col_start, col_end).second)
        throw orcus::general_error("sheet::get_col_width: failed to search tree.");

    return ret;
}

void sheet::set_col_hidden(col_t col, bool hidden)
{
    mp_impl->m_col_hidden_pos =
        mp_impl->m_col_hidden.insert(mp_impl->m_col_hidden_pos, col, col+1, hidden).first;
}

bool sheet::is_col_hidden(col_t col, col_t* col_start, col_t* col_end) const
{
    col_hidden_store_type& col_hidden = mp_impl->m_col_hidden;
    if (!col_hidden.is_tree_valid())
        col_hidden.build_tree();

    bool hidden = false;
    if (!col_hidden.search_tree(col, hidden, col_start, col_end).second)
        throw orcus::general_error("sheet::is_col_hidden: failed to search tree.");

    return hidden;
}

void sheet::set_row_height(row_t row, row_height_t height)
{
    mp_impl->m_row_height_pos =
        mp_impl->m_row_heights.insert(mp_impl->m_row_height_pos, row, row+1, height).first;
}

row_height_t sheet::get_row_height(row_t row, row_t* row_start, row_t* row_end) const
{
    row_heights_store_type& row_heights = mp_impl->m_row_heights;
    if (!row_heights.is_tree_valid())
        row_heights.build_tree();

    row_height_t ret = 0;
    if (!row_heights.search_tree(row, ret, row_start, row_end).second)
        throw orcus::general_error("sheet::get_row_height: failed to search tree.");

    return ret;
}

void sheet::set_row_hidden(row_t row, bool hidden)
{
    mp_impl->m_row_hidden_pos =
        mp_impl->m_row_hidden.insert(mp_impl->m_row_hidden_pos, row, row+1, hidden).first;
}

bool sheet::is_row_hidden(row_t row, row_t* row_start, row_t* row_end) const
{
    row_hidden_store_type& row_hidden = mp_impl->m_row_hidden;
    if (!row_hidden.is_tree_valid())
        row_hidden.build_tree();

    bool hidden = false;
    if (!row_hidden.search_tree(row, hidden, row_start, row_end).second)
        throw orcus::general_error("sheet::is_row_hidden: failed to search tree.");

    return hidden;
}

void sheet::set_merge_cell_range(const range_t& range)
{
    col_merge_size_type::iterator it_col = mp_impl->m_merge_ranges.find(range.first.column);
    if (it_col == mp_impl->m_merge_ranges.end())
    {
        std::unique_ptr<merge_size_type> p(new merge_size_type);
        pair<col_merge_size_type::iterator, bool> r =
            mp_impl->m_merge_ranges.insert(
                col_merge_size_type::value_type(range.first.column, p.get()));

        if (!r.second)
            // Insertion failed.
            return;

        p.release();
        it_col = r.first;
    }

    merge_size_type& col_data = *it_col->second;
    merge_size sz(range.last.column-range.first.column+1, range.last.row-range.first.row+1);
    col_data.insert(
        merge_size_type::value_type(range.first.row, sz));
}

range_t sheet::get_merge_cell_range(row_t row, col_t col) const
{
    range_t ret;
    ret.first.column = col;
    ret.first.row = row;
    ret.last.column = col;
    ret.last.row = row;

    col_merge_size_type::const_iterator it_col = mp_impl->m_merge_ranges.find(col);
    if (it_col == mp_impl->m_merge_ranges.end())
        return ret; // not a merged cell

    const merge_size_type& col_data = *it_col->second;
    merge_size_type::const_iterator it = col_data.find(row);
    if (it == col_data.end())
        return ret; // not a merged cell

    const merge_size& ms = it->second;
    ret.last.column += ms.width - 1;
    ret.last.row += ms.height - 1;

    return ret;
}

size_t sheet::get_string_identifier(row_t row, col_t col) const
{
    const ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    return cxt.get_string_identifier(ixion::abs_address_t(mp_impl->m_sheet, row, col));
}

auto_filter_t* sheet::get_auto_filter_data()
{
    return mp_impl->mp_auto_filter_data.get();
}

const auto_filter_t* sheet::get_auto_filter_data() const
{
    return mp_impl->mp_auto_filter_data.get();
}

void sheet::set_auto_filter_data(auto_filter_t* p)
{
    mp_impl->mp_auto_filter_data.reset(p);
}

ixion::abs_range_t sheet::get_data_range() const
{
    return mp_impl->get_data_range();
}

sheet_range sheet::get_sheet_range(
    row_t row_start, col_t col_start, row_t row_end, col_t col_end) const
{
    if (row_end < row_start || col_end < col_start)
    {
        std::ostringstream os;
        os << "sheet::get_sheet_range: invalid range (rows: "
            << row_start << "->" << row_end << "; columns: "
            << col_start << "->" << col_end << ")";
        throw orcus::general_error(os.str());
    }

    const ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    const ixion::column_stores_t* stores = cxt.get_columns(mp_impl->m_sheet);
    if (!stores)
        throw orcus::general_error(
            "sheet::get_sheet_range: failed to get column stores from the model.");

    return sheet_range(cxt, *stores, row_start, col_start, row_end, col_end);
}

row_t sheet::row_size() const
{
    return mp_impl->m_row_size;
}

col_t sheet::col_size() const
{
    return mp_impl->m_col_size;
}

sheet_t sheet::get_index() const
{
    return mp_impl->m_sheet;
}

date_time_t sheet::get_date_time(row_t row, col_t col) const
{
    const ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    const ixion::column_stores_t* stores = cxt.get_columns(mp_impl->m_sheet);
    if (!stores)
        throw orcus::general_error(
            "sheet::get_date_time: failed to get column stores from the model.");

    if (col < 0 || static_cast<size_t>(col) >= stores->size())
    {
        std::ostringstream os;
        os << "invalid column index (" << col << ")";
        throw std::invalid_argument(os.str());
    }

    const ixion::column_store_t* col_store = (*stores)[col];
    assert(col_store);

    if (row < 0 || static_cast<size_t>(row) >= col_store->size())
    {
        std::ostringstream os;
        os << "invalid row index (" << row << ")";
        throw std::invalid_argument(os.str());
    }

    double dt_raw = col_store->get<double>(row); // raw value as days since epoch.

    double days_since_epoch = std::floor(dt_raw);
    double time_fraction = dt_raw - days_since_epoch;

    date_time_t dt_origin = mp_impl->m_doc.get_origin_date();

    posix_time::ptime origin(
        gregorian::date(
            gregorian::greg_year(dt_origin.year),
            gregorian::greg_month(dt_origin.month),
            gregorian::greg_day(dt_origin.day)
        )
    );

    posix_time::ptime date_part = origin + gregorian::days(days_since_epoch);

    long hours = 0;
    long minutes = 0;
    double seconds = 0.0;

    if (time_fraction)
    {
        // Convert a fraction day to microseconds.
        double ms = time_fraction * 24.0 * 60.0 * 60.0 * 1000000.0;
        posix_time::time_duration td = posix_time::microsec(ms);

        hours = td.hours();
        minutes = td.minutes();
        seconds = td.seconds(); // long to double

        td -= posix_time::hours(hours);
        td -= posix_time::minutes(minutes);
        td -= posix_time::seconds(seconds);

        ms = td.total_microseconds(); // remaining microseconds.

        seconds += ms / 1000000.0;
    }

    gregorian::date d = date_part.date();

    return date_time_t(d.year(), d.month(), d.day(), hours, minutes, seconds);
}

void sheet::finalize()
{
    mp_impl->m_col_widths.build_tree();
    mp_impl->m_row_heights.build_tree();
    mp_impl->update_overlapped_ranges();
}

void sheet::dump_flat(std::ostream& os) const
{
    ixion::abs_range_t range = mp_impl->get_data_range();
    if (!range.valid())
        // Sheet is empty.  Nothing to print.
        return;

    const ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    const ixion::formula_name_resolver* resolver = mp_impl->m_doc.get_formula_name_resolver();

    size_t row_count = range.last.row + 1;
    size_t col_count = range.last.column + 1;
    os << "rows: " << row_count << "  cols: " << col_count << endl;

    typedef mdds::multi_type_matrix<mdds::mtm::std_string_trait> mx_type;
    mx_type mx(row_count, col_count);

    // Put all cell values into matrix as string elements first.
    for (size_t row = 0; row < row_count; ++row)
    {
        for (size_t col = 0; col < col_count; ++col)
        {
            ixion::abs_address_t pos(mp_impl->m_sheet,row,col);
            switch (cxt.get_celltype(pos))
            {
                case ixion::celltype_t::string:
                {
                    size_t sindex = cxt.get_string_identifier(pos);
                    const string* p = cxt.get_string(sindex);
                    assert(p);
                    mx.set(row, col, *p);
                    break;
                }
                case ixion::celltype_t::numeric:
                {
                    ostringstream os2;
                    os2 << cxt.get_numeric_value(pos) << " [v]";
                    mx.set(row, col, os2.str());
                    break;
                }
                case ixion::celltype_t::boolean:
                {
                    ostringstream os2;
                    os2 << (cxt.get_boolean_value(pos) ? "true" : "false") << " [b]";
                    mx.set(row, col, os2.str());
                    break;
                }
                case ixion::celltype_t::formula:
                {
                    // print the formula and the formula result.
                    const ixion::formula_cell* cell = cxt.get_formula_cell(pos);
                    assert(cell);
                    size_t index = cell->get_identifier();
                    const ixion::formula_tokens_t* t = nullptr;
                    if (cell->is_shared())
                        t = cxt.get_shared_formula_tokens(mp_impl->m_sheet, index);
                    else
                        t = cxt.get_formula_tokens(mp_impl->m_sheet, index);

                    if (t)
                    {
                        ostringstream os2;
                        string formula;
                        if (resolver)
                        {
                            formula = ixion::print_formula_tokens(
                               mp_impl->m_doc.get_model_context(), pos, *resolver, *t);
                        }
                        else
                            formula = "???";

                        os2 << formula;

                        const ixion::formula_result& res = cell->get_result_cache();
                        os2 << " (" << res.str(mp_impl->m_doc.get_model_context()) << ")";

                        mx.set(row, col, os2.str());
                    }
                    break;
                }
                default:
                    ;
            }
        }
    }

    // Calculate column widths first.
    mx_type::size_pair_type sp = mx.size();
    vector<size_t> col_widths(sp.column, 0);

    for (size_t r = 0; r < sp.row; ++r)
    {
        for (size_t c = 0; c < sp.column; ++c)
        {
            if (mx.get_type(r, c) == mdds::mtm::element_empty)
                continue;

            const string s = mx.get_string(r, c);
            if (col_widths[c] < s.size())
                col_widths[c] = s.size();
        }
    }

    // Create a row separator string;
    ostringstream os2;
    os2 << '+';
    for (size_t i = 0; i < col_widths.size(); ++i)
    {
        os2 << '-';
        size_t cw = col_widths[i];
        for (size_t i = 0; i < cw; ++i)
            os2 << '-';
        os2 << "-+";
    }

    string sep = os2.str();

    // Now print to stdout.
    os << sep << endl;
    for (size_t r = 0; r < row_count; ++r)
    {
        os << "|";
        for (size_t c = 0; c < col_count; ++c)
        {
            size_t cw = col_widths[c]; // column width
            if (mx.get_type(r, c) == mdds::mtm::element_empty)
            {
                for (size_t i = 0; i < cw; ++i)
                    os << ' ';
                os << "  |";
            }
            else
            {
                const string s = mx.get_string(r, c);
                os << ' ' << s;
                cw -= s.size();
                for (size_t i = 0; i < cw; ++i)
                    os << ' ';
                os << " |";
            }
        }
        os << endl;
        os << sep << endl;
    }
}

namespace {

void write_cell_position(ostream& os, const pstring& sheet_name, row_t row, col_t col)
{
    os << sheet_name << '/' << row << '/' << col << ':';
}

string escape_chars(const string& str)
{
    if (str.empty())
        return str;

    string ret;
    const char* p = &str[0];
    const char* p_end = p + str.size();
    for (; p != p_end; ++p)
    {
        if (*p == '"')
            ret.push_back('\\');
        ret.push_back(*p);
    }
    return ret;
}

}

void sheet::dump_check(ostream& os, const pstring& sheet_name) const
{
    ixion::abs_range_t range = mp_impl->get_data_range();
    if (!range.valid())
        // Sheet is empty.  Nothing to print.
        return;

    const ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
    const ixion::formula_name_resolver* resolver = mp_impl->m_doc.get_formula_name_resolver();

    size_t row_count = range.last.row + 1;
    size_t col_count = range.last.column + 1;

    for (size_t row = 0; row < row_count; ++row)
    {
        for (size_t col = 0; col < col_count; ++col)
        {
            ixion::abs_address_t pos(mp_impl->m_sheet, row, col);
            switch (cxt.get_celltype(pos))
            {
                case ixion::celltype_t::string:
                {
                    write_cell_position(os, sheet_name, row, col);
                    size_t sindex = cxt.get_string_identifier(pos);
                    const string* p = cxt.get_string(sindex);
                    assert(p);
                    os << "string:\"" << escape_chars(*p) << '"' << endl;
                    break;
                }
                case ixion::celltype_t::numeric:
                {
                    write_cell_position(os, sheet_name, row, col);
                    os << "numeric:" << cxt.get_numeric_value(pos) << endl;
                    break;
                }
                case ixion::celltype_t::boolean:
                {
                    write_cell_position(os, sheet_name, row, col);
                    os << "boolean:" << (cxt.get_boolean_value(pos) ? "true" : "false") << endl;
                    break;
                }
                case ixion::celltype_t::formula:
                {
                    write_cell_position(os, sheet_name, row, col);
                    os << "formula";

                    // print the formula and the formula result.
                    const ixion::formula_cell* cell = cxt.get_formula_cell(pos);
                    assert(cell);
                    size_t index = cell->get_identifier();
                    const ixion::formula_tokens_t* t = nullptr;
                    if (cell->is_shared())
                        t = cxt.get_shared_formula_tokens(mp_impl->m_sheet, index);
                    else
                        t = cxt.get_formula_tokens(mp_impl->m_sheet, index);

                    if (t)
                    {
                        string formula;
                        if (resolver)
                        {
                            formula = ixion::print_formula_tokens(
                                mp_impl->m_doc.get_model_context(), pos, *resolver, *t);
                        }
                        else
                            formula = "???";

                        os << ':' << formula;

                        const ixion::formula_result& res = cell->get_result_cache();
                        os << ':' << res.str(mp_impl->m_doc.get_model_context());
                    }
                    os << endl;
                    break;
                }
                default:
                    ;
            }
        }
    }
}

namespace {

void build_rgb_color(ostringstream& os, const color_t& color_value)
{
    // Special colors.
    if (color_value.alpha == 255 && color_value.red == 0 && color_value.green == 0 && color_value.blue == 0)
    {
        os << "black";
        return;
    }

    if (color_value.alpha == 255 && color_value.red == 255 && color_value.green == 0 && color_value.blue == 0)
    {
        os << "red";
        return;
    }

    if (color_value.alpha == 255 && color_value.red == 0 && color_value.green == 255 && color_value.blue == 0)
    {
        os << "green";
        return;
    }

    if (color_value.alpha == 255 && color_value.red == 0 && color_value.green == 0 && color_value.blue == 255)
    {
        os << "blue";
        return;
    }

    os << "rgb("
        << static_cast<short>(color_value.red) << ","
        << static_cast<short>(color_value.green) << ","
        << static_cast<short>(color_value.blue) << ")";
}

const char* css_style_global =
"table, td { "
    "border-collapse : collapse; "
"}\n"

"table { "
    "border-spacing : 0px; "
"}\n"

"td { "
    "width : 1in; "
"}\n"

"td.empty { "
    "color : white; "
"}\n";

class html_elem
{
public:
    struct attr
    {
        string name;
        string value;

        attr(const string& _name, const string& _value) : name(_name), value(_value) {}
    };

    typedef vector<attr> attrs_type;

    html_elem(ostream& strm, const char* name, const char* style = nullptr, const char* style_class = nullptr) :
        m_strm(strm), m_name(name)
    {
        m_strm << '<' << m_name;

        if (style)
            m_strm << " style=\"" << style << "\"";

        if (style_class)
            m_strm << " class=\"" << style_class << "\"";

        m_strm << '>';
    }

    html_elem(ostream& strm, const char* name, const attrs_type& attrs) :
        m_strm(strm), m_name(name)
    {
        m_strm << '<' << m_name;

        attrs_type::const_iterator it = attrs.begin(), it_end = attrs.end();
        for (; it != it_end; ++it)
            m_strm << " " << it->name << "=\"" << it->value << "\"";

        m_strm << '>';
    }

    ~html_elem()
    {
        m_strm << "</" << m_name << '>';
    }

private:
    ostream& m_strm;
    const char* m_name;
};

void print_formatted_text(ostream& strm, const string& text, const format_runs_t& formats)
{
    typedef html_elem elem;

    const char* p_span = "span";

    size_t pos = 0;
    format_runs_t::const_iterator itr = formats.begin(), itr_end = formats.end();
    for (; itr != itr_end; ++itr)
    {
        const format_run& run = *itr;
        if (pos < run.pos)
        {
            // flush unformatted text.
            strm << string(&text[pos], run.pos-pos);
            pos = run.pos;
        }

        if (!run.size)
            continue;

        string style = "";
        if (run.bold)
            style += "font-weight: bold;";
        else
            style += "font-weight: normal;";

        if (run.italic)
            style += "font-style: italic;";
        else
            style += "font-style: normal;";

        if (!run.font.empty())
            style += "font-family: " + run.font.str() + ";";

        if (run.font_size)
        {
            ostringstream os;
            os << "font-size: " << run.font_size << "pt;";
            style += os.str();
        }

        const color_t& col = run.color;
        if (col.red || col.green || col.blue)
        {
            ostringstream os;
            os << "color: ";
            build_rgb_color(os, col);
            os << ";";
            style += os.str();
        }

        if (style.empty())
            strm << string(&text[pos], run.size);
        else
        {
            elem span(strm, p_span, style.c_str());
            strm << string(&text[pos], run.size);
        }

        pos += run.size;
    }

    if (pos < text.size())
    {
        // flush the remaining unformatted text.
        strm << string(&text[pos], text.size() - pos);
    }
}

void build_border_style(ostringstream& os, const char* style_name, const border_attrs_t& attrs)
{
    os << style_name << ": ";
    if (attrs.style == border_style_t::thin)
    {
        os << "solid 1px ";
    }
    else if (attrs.style == border_style_t::medium)
    {
        os << "solid 2px ";
    }
    else if (attrs.style == border_style_t::thick)
    {
        os << "solid 3px ";
    }
    else if (attrs.style == border_style_t::hair)
    {
        os << "solid 0.5px ";
    }
    else if (attrs.style == border_style_t::dotted)
    {
        os << "dotted 1px ";
    }
    else if (attrs.style == border_style_t::dashed)
    {
        os << "dashed 1px ";
    }
    else if (attrs.style == border_style_t::double_border)
    {
        os << "3px double ";
    }
    else if (attrs.style == border_style_t::dash_dot)
    {
        // CSS doesn't support dash-dot.
        os << "dashed 1px ";
    }
    else if (attrs.style == border_style_t::dash_dot_dot)
    {
        // CSS doesn't support dash-dot-dot.
        os << "dashed 1px ";
    }
    else if (attrs.style == border_style_t::medium_dashed)
    {
        os << "dashed 2px ";
    }
    else if (attrs.style == border_style_t::medium_dash_dot)
    {
        // CSS doesn't support dash-dot.
        os << "dashed 2px ";
    }
    else if (attrs.style == border_style_t::medium_dash_dot_dot)
    {
        // CSS doesn't support dash-dot-dot.
        os << "dashed 2px ";
    }
    else if (attrs.style == border_style_t::slant_dash_dot)
    {
        // CSS doesn't support dash-dot.
        os << "dashed 2px ";
    }

    build_rgb_color(os, attrs.border_color);
    os << "; ";
}

void build_style_string(string& str, const styles& styles, const cell_format_t& fmt)
{
    ostringstream os;
    if (fmt.font)
    {
        const font_t* p = styles.get_font(fmt.font);
        if (p)
        {
            if (!p->name.empty())
                os << "font-family: " << p->name << ";";
            if (p->size)
                os << "font-size: " << p->size << "pt;";
            if (p->bold)
                os << "font-weight: bold;";
            if (p->italic)
                os << "font-style: italic;";

            const color_t& r = p->color;
            if (r.red || r.green || r.blue)
            {
                os << "color: ";
                build_rgb_color(os, r);
                os << ";";
            }
        }
    }
    if (fmt.fill)
    {
        const fill_t* p = styles.get_fill(fmt.fill);
        if (p)
        {
            if (p->pattern_type == fill_pattern_t::solid)
            {
                const color_t& r = p->fg_color;
                os << "background-color: ";
                build_rgb_color(os, r);
                os << ";";
            }
        }
    }

    if (fmt.border)
    {
        const border_t* p = styles.get_border(fmt.border);
        if (p)
        {
            build_border_style(os, "border-top", p->top);
            build_border_style(os, "border-bottom", p->bottom);
            build_border_style(os, "border-left", p->left);
            build_border_style(os, "border-right", p->right);
        }
    }

    if (fmt.apply_alignment)
    {
        if (fmt.hor_align != hor_alignment_t::unknown)
        {
            os << "text-align: ";
            switch (fmt.hor_align)
            {
                case hor_alignment_t::left:
                    os << "left";
                break;
                case hor_alignment_t::center:
                    os << "center";
                break;
                case hor_alignment_t::right:
                    os << "right";
                break;
                default:
                    ;
            }
            os << ";";
        }

        if (fmt.ver_align != ver_alignment_t::unknown)
        {
            os << "vertical-align: ";
            switch (fmt.ver_align)
            {
                case ver_alignment_t::top:
                    os << "top";
                break;
                case ver_alignment_t::middle:
                    os << "middle";
                break;
                case ver_alignment_t::bottom:
                    os << "bottom";
                break;
                default:
                    ;
            }
            os << ";";
        }
    }

    str += os.str();
}

void dump_html_head(ostream& os)
{
    typedef html_elem elem;

    const char* p_head = "head";
    const char* p_style = "style";

    elem elem_head(os, p_head);
    {
        elem elem_style(os, p_style);
        os << css_style_global;
    }
}

void build_html_elem_attributes(html_elem::attrs_type& attrs, const string& style, const merge_size* p_merge_size)
{
    attrs.push_back(html_elem::attr("style", style));
    if (p_merge_size)
    {
        if (p_merge_size->width > 1)
        {
            ostringstream os2;
            os2 << p_merge_size->width;
            attrs.push_back(html_elem::attr("colspan", os2.str()));
        }

        if (p_merge_size->height > 1)
        {
            ostringstream os2;
            os2 << p_merge_size->height;
            attrs.push_back(html_elem::attr("rowspan", os2.str()));
        }
    }
}

}

void sheet::dump_html(std::ostream& os) const
{
    typedef html_elem elem;

    const char* p_html  = "html";
    const char* p_body  = "body";
    const char* p_table = "table";
    const char* p_tr    = "tr";
    const char* p_td    = "td";

    ixion::abs_range_t range = mp_impl->get_data_range();

    if (!mp_impl->m_col_widths.is_tree_valid())
        mp_impl->m_col_widths.build_tree();

    if (!mp_impl->m_row_heights.is_tree_valid())
        mp_impl->m_row_heights.build_tree();

    elem root(os, p_html);
    dump_html_head(os);

    {
        elem elem_body(os, p_body);

        if (!range.valid())
            // Sheet is empty.  Nothing to print.
            return;

        const ixion::model_context& cxt = mp_impl->m_doc.get_model_context();
        const ixion::formula_name_resolver* resolver = mp_impl->m_doc.get_formula_name_resolver();
        const import_shared_strings* sstrings = mp_impl->m_doc.get_shared_strings();

        elem table(os, p_table);

        row_t row_count = range.last.row + 1;
        col_t col_count = range.last.column + 1;
        for (row_t row = 0; row < row_count; ++row)
        {
            // Set the row height.
            string row_style;
            row_height_t rh;
            if (mp_impl->m_row_heights.search_tree(row, rh).second)
            {
                // Convert height from twip to inches.
                if (rh != get_default_row_height())
                {
                    string style;
                    double val = orcus::convert(rh, length_unit_t::twip, length_unit_t::inch);
                    ostringstream os_style;
                    os_style << "height: " << val << "in;";
                    row_style += os_style.str();
                }
            }

            const char* style_str = nullptr;
            if (!row_style.empty())
                style_str = row_style.c_str();
            elem tr(os, p_tr, style_str);

            const overlapped_col_index_type* p_overlapped = mp_impl->get_overlapped_ranges(row);

            for (col_t col = 0; col < col_count; ++col)
            {
                ixion::abs_address_t pos(mp_impl->m_sheet,row,col);

                const merge_size* p_merge_size = mp_impl->get_merge_size(row, col);
                if (!p_merge_size && p_overlapped)
                {
                    // Check if this cell is overlapped by a merged cell.
                    bool overlapped = false;
                    col_t last_col;
                    if (p_overlapped->search_tree(col, overlapped, nullptr, &last_col).second && overlapped)
                    {
                        // Skip all overlapped cells on this row.
                        col = last_col - 1;
                        continue;
                    }
                }
                size_t xf_id = get_cell_format(row, col);
                string style;

                if (row == 0)
                {
                    // Set the column width.
                    col_width_t cw;
                    if (mp_impl->m_col_widths.search_tree(col, cw).second)
                    {
                        // Convert width from twip to inches.
                        if (cw != get_default_column_width())
                        {
                            double val = orcus::convert(cw, length_unit_t::twip, length_unit_t::inch);
                            ostringstream os_style;
                            os_style << "width: " << val << "in;";
                            style += os_style.str();
                        }
                    }
                }

                if (xf_id)
                {
                    // Apply cell format.
                    styles& styles = mp_impl->m_doc.get_styles();
                    const cell_format_t* fmt = styles.get_cell_format(xf_id);
                    if (fmt)
                        build_style_string(style, styles, *fmt);
                }

                ixion::celltype_t ct = cxt.get_celltype(pos);
                if (ct == ixion::celltype_t::empty)
                {
                    html_elem::attrs_type attrs;
                    build_html_elem_attributes(attrs, style, p_merge_size);
                    attrs.push_back(html_elem::attr("class", "empty"));
                    elem td(os, p_td, attrs);
                    os << '-'; // empty cell.
                    continue;
                }

                html_elem::attrs_type attrs;
                build_html_elem_attributes(attrs, style, p_merge_size);
                elem td(os, p_td, attrs);

                switch (ct)
                {
                    case ixion::celltype_t::string:
                    {
                        size_t sindex = cxt.get_string_identifier(pos);
                        const string* p = cxt.get_string(sindex);
                        assert(p);
                        const format_runs_t* pformat = sstrings->get_format_runs(sindex);
                        if (pformat)
                            print_formatted_text(os, *p, *pformat);
                        else
                            os << *p;

                        break;
                    }
                    case ixion::celltype_t::numeric:
                        os << cxt.get_numeric_value(pos);
                        break;
                    case ixion::celltype_t::boolean:
                        os << (cxt.get_boolean_value(pos) ? "true" : "false");
                        break;
                    case ixion::celltype_t::formula:
                    {
                        // print the formula and the formula result.
                        const ixion::formula_cell* cell = cxt.get_formula_cell(pos);
                        assert(cell);
                        size_t index = cell->get_identifier();
                        const ixion::formula_tokens_t* t = nullptr;
                        if (cell->is_shared())
                            t = cxt.get_shared_formula_tokens(mp_impl->m_sheet, index);
                        else
                            t = cxt.get_formula_tokens(mp_impl->m_sheet, index);

                        if (t)
                        {
                            string formula;
                            if (resolver)
                            {
                                formula = ixion::print_formula_tokens(
                                    mp_impl->m_doc.get_model_context(), pos, *resolver, *t);
                            }
                            else
                                formula = "???";

                            os << formula;

                            const ixion::formula_result& res = cell->get_result_cache();
                            os << " (" << res.str(mp_impl->m_doc.get_model_context()) << ")";
                        }
                        break;
                    }
                    default:
                        ;
                }
            }
        }
    }
}

void sheet::dump_json(std::ostream& os) const
{
    detail::json_dumper dumper(mp_impl->m_doc);
    dumper.dump(os, mp_impl->m_sheet);
}

void sheet::dump_csv(std::ostream& os) const
{
    detail::csv_dumper dumper(mp_impl->m_doc);
    dumper.dump(os, mp_impl->m_sheet);
}

size_t sheet::get_cell_format(row_t row, col_t col) const
{
    cell_format_type::const_iterator itr = mp_impl->m_cell_formats.find(col);
    if (itr == mp_impl->m_cell_formats.end())
        return 0;

    segment_row_index_type& con = *itr->second;
    if (!con.is_tree_valid())
        con.build_tree();

    size_t index;
    if (!con.search_tree(row, index).second)
        return 0;

    return index;
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
