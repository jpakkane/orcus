/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "csv_dumper.hpp"
#include "dumper_global.hpp"
#include "orcus/spreadsheet/document.hpp"

#include <ixion/model_context.hpp>
#include <ixion/formula_name_resolver.hpp>
#include <ixion/formula_result.hpp>
#include <mdds/multi_type_vector/collection.hpp>

#include <fstream>
#include <sstream>
#include <iostream>

namespace orcus { namespace spreadsheet { namespace detail {

namespace {

void dump_string(std::ostream& os, const std::string& s)
{
    // Scan for any special characters that necessitate quoting.
    bool outer_quotes = s.find_first_of(",\"") != std::string::npos;

    if (outer_quotes)
        os << '"';

    for (const char c : s)
    {
        switch (c)
        {
            case '"':
            {
                os << c << c;
                break;
            }
            default:
                os << c;
        }
    }

    if (outer_quotes)
        os << '"';
}

void dump_empty(std::ostream& /*os*/)
{
    // Do nothing.
}

}

csv_dumper::csv_dumper(const document& doc) :
    m_doc(doc), m_sep(','), m_quote('"')
{
}

void csv_dumper::dump(std::ostream& os, ixion::sheet_t sheet_id) const
{
    const ixion::model_context& cxt = m_doc.get_model_context();
    ixion::abs_range_t data_range = cxt.get_data_range(sheet_id);
    if (!data_range.valid())
        return;

    const ixion::column_stores_t* p = cxt.get_columns(sheet_id);
    if (!p)
        return;

    columns_type columns(p->begin(), p->end());

    // Only iterate through the data range.
    columns.set_collection_range(0, data_range.last.column+1);
    columns.set_element_range(0, data_range.last.row+1);

    std::for_each(columns.begin(), columns.end(),
        [&](const columns_type::const_iterator::value_type& node)
        {
            size_t row = node.position;
            size_t col = node.index;

            if (col == 0 && row > 0)
                os << std::endl;

            if (col > 0)
                os << m_sep;

            dump_cell_value(os, cxt, node, dump_string, dump_empty);
        }
    );
}

}}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
