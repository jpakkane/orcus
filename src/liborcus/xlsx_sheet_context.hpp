/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ORCUS_XLSX_SHEET_CONTEXT_HPP
#define ORCUS_XLSX_SHEET_CONTEXT_HPP

#include "xml_context_base.hpp"
#include "ooxml_types.hpp"
#include "xlsx_types.hpp"

#include "orcus/spreadsheet/types.hpp"
#include "orcus/string_pool.hpp"

#include <memory>

namespace orcus {

struct session_context;

namespace spreadsheet { namespace iface {

class import_sheet;
class import_reference_resolver;

}}

/**
 * Top-level context for xl/worksheets/sheet<num>.xml.
 */
class xlsx_sheet_context : public xml_context_base
{
public:

    struct formula
    {
        spreadsheet::formula_t type;
        pstring str; /// formula expression string
        pstring ref; /// formula reference
        pstring data_table_ref1;
        pstring data_table_ref2;
        int shared_id;
        bool data_table_2d:1;
        bool data_table_row_based:1;
        bool data_table_ref1_deleted:1;
        bool data_table_ref2_deleted:1;
        formula();

        void reset();
    };

    xlsx_sheet_context(
        session_context& session_cxt, const tokens& tokens,
        spreadsheet::sheet_t sheet_id,
        spreadsheet::iface::import_reference_resolver& resolver,
        spreadsheet::iface::import_sheet& sheet);
    virtual ~xlsx_sheet_context();

    virtual bool can_handle_element(xmlns_id_t ns, xml_token_t name) const;
    virtual xml_context_base* create_child_context(xmlns_id_t ns, xml_token_t name);
    virtual void end_child_context(xmlns_id_t ns, xml_token_t name, xml_context_base* child);

    virtual void start_element(xmlns_id_t ns, xml_token_t name, const xml_attrs_t& attrs);
    virtual bool end_element(xmlns_id_t ns, xml_token_t name);
    virtual void characters(const pstring& str, bool transient);

    void pop_rel_extras(opc_rel_extras_t& other);

private:
    void start_element_sheet_view(const xml_token_pair_t& parent, const xml_attrs_t& attrs);
    void end_element_cell();
    void push_raw_cell_value();

private:
    std::unique_ptr<xml_context_base> mp_child;

    spreadsheet::iface::import_reference_resolver& m_resolver;
    spreadsheet::iface::import_sheet& m_sheet; /// sheet model instance for the loaded document.
    string_pool m_pool;
    spreadsheet::sheet_t m_sheet_id; /// ID of this sheet.
    spreadsheet::row_t m_cur_row;
    spreadsheet::col_t m_cur_col;
    xlsx_cell_t m_cur_cell_type;
    size_t       m_cur_cell_xf;
    pstring      m_cur_str;
    pstring      m_cur_value;
    formula m_cur_formula;

    /**
     * Extra data to pass on to subsequent parts via relations.
     */
    opc_rel_extras_t m_rel_extras;
};

}

#endif
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
