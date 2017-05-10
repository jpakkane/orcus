/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "orcus/spreadsheet/view.hpp"
#include "orcus/spreadsheet/document.hpp"
#include "orcus/global.hpp"

namespace orcus { namespace spreadsheet {

struct view::impl
{
    document& m_doc;

    std::vector<std::unique_ptr<sheet_view>> m_sheet_views;
    sheet_t m_active_sheet;

    impl(document& doc) : m_doc(doc), m_active_sheet(0) {}
};

view::view(document& doc) : mp_impl(orcus::make_unique<impl>(doc)) {}
view::~view() {}

sheet_view* view::get_or_create_sheet_view(sheet_t sheet)
{
    if (sheet < 0)
        return nullptr;

    sheet_t n = mp_impl->m_doc.sheet_size();
    if (sheet >= n)
        return nullptr;

    // Make sure the container is large enough for the requested sheet view index.
    n = mp_impl->m_sheet_views.size();
    if (sheet >= n)
        mp_impl->m_sheet_views.resize(sheet+1);

    if (!mp_impl->m_sheet_views[sheet])
        mp_impl->m_sheet_views[sheet] = orcus::make_unique<sheet_view>();

    return mp_impl->m_sheet_views[sheet].get();
}

void view::set_active_sheet(sheet_t sheet)
{
    mp_impl->m_active_sheet = sheet;
}

namespace {

/**
 * Stores all data for a single sheet pane.
 */
struct sheet_pane_data
{
    range_t m_selection;
};

} // anonymous namespace

struct sheet_view::impl
{

    sheet_pane_data m_panes[4];

    sheet_pane_data& get_pane(sheet_pane_t pos)
    {
        switch (pos)
        {
            case sheet_pane_t::top_left:
                return m_panes[0];
            case sheet_pane_t::top_right:
                return m_panes[1];
            case sheet_pane_t::bottom_left:
                return m_panes[2];
            case sheet_pane_t::bottom_right:
                return m_panes[3];
            case sheet_pane_t::unspecified:
            default:
                throw std::runtime_error("invalid sheet pane.");
        }
    }
};

sheet_view::sheet_view() : mp_impl(orcus::make_unique<impl>()) {}
sheet_view::~sheet_view() {}

void sheet_view::set_selection(sheet_pane_t pos, const range_t& range)
{
    sheet_pane_data& pd = mp_impl->get_pane(pos);
    pd.m_selection = range;
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */