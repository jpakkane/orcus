/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "cell.hpp"
#include "memory.hpp"

#include <structmember.h>
#include <string>

namespace orcus { namespace python {

cell_data::cell_data() {}
cell_data::~cell_data() {}

namespace {

/**
 * Python object for orcus.Cell.
 */
struct pyobj_cell
{
    PyObject_HEAD

    PyObject* type;
    PyObject* value;

    cell_data* m_data;
};

void cell_dealloc(pyobj_cell* self)
{
    delete self->m_data;

    Py_CLEAR(self->type);
    Py_CLEAR(self->value);

    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject* cell_new(PyTypeObject* type, PyObject* /*args*/, PyObject* /*kwargs*/)
{
    pyobj_cell* self = (pyobj_cell*)type->tp_alloc(type, 0);
    self->m_data = new cell_data;
    return reinterpret_cast<PyObject*>(self);
}

PyObject* create_celltype_enum(const char* value_name)
{
    py_scoped_ref orcus_mod = PyImport_ImportModule("orcus");
    if (!orcus_mod)
    {
        PyErr_SetString(PyExc_RuntimeError, "failed to import orcus module.");
        return nullptr;
    }

    py_scoped_ref cls_celltype = PyObject_GetAttrString(orcus_mod.get(), "CellType");
    if (!cls_celltype)
    {
        PyErr_SetString(PyExc_RuntimeError, "failed to find class orcus.CellType.");
        return nullptr;
    }

    return PyObject_GetAttrString(cls_celltype.get(), value_name);
}

PyObject* create_and_init_cell_object()
{
    PyTypeObject* cell_type = get_cell_type();
    if (!cell_type)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get the cell type object.");
        return nullptr;
    }

    PyObject* obj = cell_type->tp_new(cell_type, nullptr, nullptr);
    if (!obj)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to instantiate a cell object.");
        return nullptr;
    }

    cell_type->tp_init(obj, nullptr, nullptr);
    return obj;
}

int cell_init(pyobj_cell* self, PyObject* /*args*/, PyObject* /*kwargs*/)
{
    Py_INCREF(Py_None);
    self->value = Py_None;

    return 0;
}

PyMemberDef cell_members[] =
{
    { (char*)"type", T_OBJECT_EX, offsetof(pyobj_cell, type), READONLY, (char*)"cell type" },
    { (char*)"value", T_OBJECT_EX, offsetof(pyobj_cell, value), READONLY, (char*)"cell value" },
    { nullptr }
};

PyTypeObject cell_type =
{
    PyVarObject_HEAD_INIT(nullptr, 0)
    "orcus.Cell",                             // tp_name
    sizeof(pyobj_cell),                       // tp_basicsize
    0,                                        // tp_itemsize
    (destructor)cell_dealloc,                 // tp_dealloc
    0,                                        // tp_print
    0,                                        // tp_getattr
    0,                                        // tp_setattr
    0,                                        // tp_compare
    0,                                        // tp_repr
    0,                                        // tp_as_number
    0,                                        // tp_as_sequence
    0,                                        // tp_as_mapping
    0,                                        // tp_hash
    0,                                        // tp_call
    0,                                        // tp_str
    0,                                        // tp_getattro
    0,                                        // tp_setattro
    0,                                        // tp_as_buffer
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, // tp_flags
    "orcus spreadsheet cell",                 // tp_doc
    0,		                                  // tp_traverse
    0,		                                  // tp_clear
    0,		                                  // tp_richcompare
    0,		                                  // tp_weaklistoffset
    0,		                                  // tp_iter
    0,                                        // tp_iternext
    0,                                        // tp_methods
    cell_members,                             // tp_members
    0,                                        // tp_getset
    0,                                        // tp_base
    0,                                        // tp_dict
    0,                                        // tp_descr_get
    0,                                        // tp_descr_set
    0,                                        // tp_dictoffset
    (initproc)cell_init,                      // tp_init
    0,                                        // tp_alloc
    cell_new,                                 // tp_new
};

}

PyObject* create_cell_object_empty()
{
    PyObject* obj = create_and_init_cell_object();
    if (!obj)
        return nullptr;

    pyobj_cell* obj_data = reinterpret_cast<pyobj_cell*>(obj);
    obj_data->type = create_celltype_enum("EMPTY");

    return obj;
}

PyObject* create_cell_object_boolean(bool v)
{
    PyObject* obj = create_and_init_cell_object();
    if (!obj)
        return nullptr;

    pyobj_cell* obj_data = reinterpret_cast<pyobj_cell*>(obj);
    obj_data->type = create_celltype_enum("BOOLEAN");

    if (v)
    {
        Py_INCREF(Py_True);
        obj_data->value = Py_True;
    }
    else
    {
        Py_INCREF(Py_False);
        obj_data->value = Py_False;
    }

    return obj;
}

PyObject* create_cell_object_string(const std::string* p)
{
    PyObject* obj = create_and_init_cell_object();
    if (!obj)
        return nullptr;

    pyobj_cell* obj_data = reinterpret_cast<pyobj_cell*>(obj);
    obj_data->type = create_celltype_enum("STRING");

    if (p)
        obj_data->value = PyUnicode_FromStringAndSize(p->data(), p->size());
    else
    {
        Py_INCREF(Py_None);
        obj_data->value = Py_None;
    }

    return obj;
}

PyTypeObject* get_cell_type()
{
    return &cell_type;
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
