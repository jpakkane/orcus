/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "root.hpp"
#include "document.hpp"
#include "global.hpp"

#include "orcus/format_detection.hpp"
#include "orcus/info.hpp"

#include <iostream>
#include <sstream>

#include <object.h>

using namespace std;

namespace orcus { namespace python {

PyObject* info(PyObject*, PyObject*, PyObject*)
{
    cout << "orcus version: "
        << orcus::get_version_major() << '.'
        << orcus::get_version_minor() << '.'
        << orcus::get_version_micro() << endl;

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* detect_format(PyObject* /*module*/, PyObject* args, PyObject* kwargs)
{
    stream_data data = read_stream_object_from_args(args, kwargs);
    if (!data.stream)
        return nullptr;

    const char* p = PyBytes_AS_STRING(data.stream.get());
    size_t n = PyBytes_Size(data.stream.get());

    try
    {
        format_t ft = orcus::detect(reinterpret_cast<const unsigned char*>(p), n);

        switch (ft)
        {
            case format_t::ods:
                return get_python_enum_value("FormatType", "ODS");
            case format_t::xlsx:
                return get_python_enum_value("FormatType", "XLSX");
            case format_t::gnumeric:
                return get_python_enum_value("FormatType", "GNUMERIC");
            case format_t::xls_xml:
                return get_python_enum_value("FormatType", "XLS_XML");
            case format_t::csv:
                return get_python_enum_value("FormatType", "CSV");
            case format_t::unknown:
            default:
                return get_python_enum_value("FormatType", "UNKNOWN");
        }
    }
    catch (const std::exception&)
    {
        PyErr_SetString(PyExc_ValueError, "failed to perform deep detection on this file.");
        return nullptr;
    }
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

