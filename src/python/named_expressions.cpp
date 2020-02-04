/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "named_expressions.hpp"
#include "orcus/spreadsheet/document.hpp"

#include <ixion/formula.hpp>
#include <ixion/model_context.hpp>
#include <ixion/named_expressions_iterator.hpp>
#include <ixion/formula_name_resolver.hpp>
#include <structmember.h>

namespace ss = orcus::spreadsheet;

namespace orcus { namespace python {

namespace {

/** non-python part. */
struct named_exps_data
{
    ixion::named_expressions_iterator iter;
};

/** python object. */
struct pyobj_named_exps
{
    PyObject_HEAD

    named_exps_data* data;
};

inline pyobj_named_exps* t(PyObject* self)
{
    return reinterpret_cast<pyobj_named_exps*>(self);
}

void tp_dealloc(pyobj_named_exps* self)
{
    delete self->data;
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

int tp_init(pyobj_named_exps* self, PyObject* /*args*/, PyObject* /*kwargs*/)
{
    return 0;
}

PyObject* tp_new(PyTypeObject* type, PyObject* /*args*/, PyObject* /*kwargs*/)
{
    pyobj_named_exps* self = t(type->tp_alloc(type, 0));
    self->data = new named_exps_data;
    return reinterpret_cast<PyObject*>(self);
}

PyObject* tp_iter(PyObject* self)
{
    Py_INCREF(self);
    return self;
}

PyObject* tp_iternext(PyObject* self)
{
    PyErr_SetNone(PyExc_StopIteration);
    return nullptr;
}

PySequenceMethods tp_as_sequence =
{
    0,         // lenfunc sq_length
    0,         // binaryfunc sq_concat
    0,         // ssizeargfunc sq_repeat
    0,         // ssizeargfunc sq_item
    0,         // void *was_sq_slice
    0,         // ssizeobjargproc sq_ass_item
    0,         // void *was_sq_ass_slice
    0,         // objobjproc sq_contains
    0,         // binaryfunc sq_inplace_concat
    0,         // ssizeargfunc sq_inplace_repeat
};

PyMethodDef tp_methods[] =
{
    { nullptr }
};

PyMemberDef tp_members[] =
{
    { nullptr }
};

PyTypeObject named_exps_type =
{
    PyVarObject_HEAD_INIT(nullptr, 0)
    "orcus.NamedExpressions",                 // tp_name
    sizeof(pyobj_named_exps),                 // tp_basicsize
    0,                                        // tp_itemsize
    (destructor)tp_dealloc,                   // tp_dealloc
    0,                                        // tp_print
    0,                                        // tp_getattr
    0,                                        // tp_setattr
    0,                                        // tp_compare
    0,                                        // tp_repr
    0,                                        // tp_as_number
    &tp_as_sequence,                          // tp_as_sequence
    0,                                        // tp_as_mapping
    0,                                        // tp_hash
    0,                                        // tp_call
    0,                                        // tp_str
    0,                                        // tp_getattro
    0,                                        // tp_setattro
    0,                                        // tp_as_buffer
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, // tp_flags
    "orcus spreadsheet formula tokens",       // tp_doc
    0,		                                  // tp_traverse
    0,		                                  // tp_clear
    0,		                                  // tp_richcompare
    0,		                                  // tp_weaklistoffset
    tp_iter,		                          // tp_iter
    tp_iternext,                              // tp_iternext
    tp_methods,                               // tp_methods
    tp_members,                               // tp_members
    0,                                        // tp_getset
    0,                                        // tp_base
    0,                                        // tp_dict
    0,                                        // tp_descr_get
    0,                                        // tp_descr_set
    0,                                        // tp_dictoffset
    (initproc)tp_init,                        // tp_init
    0,                                        // tp_alloc
    tp_new,                                   // tp_new
};

} // anonymous namespace

PyObject* create_named_expressions_object(
    spreadsheet::sheet_t origin_sheet, const spreadsheet::document& doc, ixion::named_expressions_iterator iter)
{
    return nullptr;
}

PyTypeObject* get_named_exps_type()
{
    return &named_exps_type;
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
