/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2006 James Bursa <james@semichrome.net>
 */

/* Python module for RUfl. */

#include "Python.h"
#include "rufl.h"


static char pyrufl_paint__doc__[] =
"paint(font_family, font_style, font_size, string, x, y, flags)\n\n"
"Render Unicode text."
;

static PyObject *
pyrufl_paint(PyObject *self /* Not used */, PyObject *args)
{
	const char *font_family;
	rufl_style font_style;
	unsigned int font_size;
	const char *string;
	int length;
	int x;
	int y;
	unsigned int flags;

	if (!PyArg_ParseTuple(args, "siIs#iiI",
			&font_family, &font_style, &font_size,
			&string, &length, &x, &y, &flags))
		return NULL;

	rufl_paint(font_family, font_style, font_size, string, length,
			x, y, flags);

	Py_INCREF(Py_None);
	return Py_None;
}


static char pyrufl_width__doc__[] =
"width(font_family, font_style, font_size, string)\n\n"
"Return the width of Unicode text."
;

static PyObject *
pyrufl_width(PyObject *self /* Not used */, PyObject *args)
{
	const char *font_family;
	rufl_style font_style;
	unsigned int font_size;
	const char *string;
	int length;
	int width = 0;

	if (!PyArg_ParseTuple(args, "siIs#",
			&font_family, &font_style, &font_size,
			&string, &length))
		return NULL;

	rufl_width(font_family, font_style, font_size, string, length,
			&width);

	return PyInt_FromLong(width);
}


static char pyrufl_x_to_offset__doc__[] =
"x_to_offset(font_family, font_style, font_size, string, click_x)\n\n"
"Return a pair of the character offset in string that click_x falls,\n"
"and the actual x coordinate for that character offset."
;

static PyObject *
pyrufl_x_to_offset(PyObject *self /* Not used */, PyObject *args)
{
	const char *font_family;
	rufl_style font_style;
	unsigned int font_size;
	const char *string;
	int length;
	int click_x;
	size_t char_offset = 0;
	int actual_x = 0;

	if (!PyArg_ParseTuple(args, "siIs#i",
			&font_family, &font_style, &font_size,
			&string, &length, &click_x))
		return NULL;

	rufl_x_to_offset(font_family, font_style, font_size, string, length,
			click_x,
			&char_offset, &actual_x);

	return Py_BuildValue("ii", (int) char_offset, actual_x);
}


static char pyrufl_split__doc__[] =
"split(font_family, font_style, font_size, string, width)\n\n"
"Return a pair of the character offset in string that fits in width,\n"
"and the actual x coordinate for that character offset."
;

static PyObject *
pyrufl_split(PyObject *self /* Not used */, PyObject *args)
{
	const char *font_family;
	rufl_style font_style;
	unsigned int font_size;
	const char *string;
	int length;
	int width;
	size_t char_offset = 0;
	int actual_x = 0;

	if (!PyArg_ParseTuple(args, "siIs#i",
			&font_family, &font_style, &font_size,
			&string, &length, &width))
		return NULL;

	rufl_split(font_family, font_style, font_size, string, length,
			width,
			&char_offset, &actual_x);

	return Py_BuildValue("ii", (int) char_offset, actual_x);
}


static char pyrufl_invalidate_cache__doc__[] =
"invalidate_cache()\n\n"
"Clear the internal font handle cache.\n"
"Call this function on mode changes or output redirection changes."
;

static PyObject *
pyrufl_invalidate_cache(PyObject *self /* Not used */, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	rufl_invalidate_cache();

	Py_INCREF(Py_None);
	return Py_None;
}


/* List of methods defined in the module */

static struct PyMethodDef pyrufl_methods[] = {
	{"paint", (PyCFunction)pyrufl_paint, METH_VARARGS, pyrufl_paint__doc__},
	{"width", (PyCFunction)pyrufl_width, METH_VARARGS, pyrufl_width__doc__},
	{"x_to_offset", (PyCFunction)pyrufl_x_to_offset, METH_VARARGS,
			pyrufl_x_to_offset__doc__},
	{"split", (PyCFunction)pyrufl_split, METH_VARARGS, pyrufl_split__doc__},
	{"invalidate_cache", (PyCFunction)pyrufl_invalidate_cache, METH_VARARGS,
			pyrufl_invalidate_cache__doc__},

	{NULL, (PyCFunction)NULL, 0, NULL}		/* sentinel */
};


/* Initialization function for the module (*must* be called initrufl) */

static char pyrufl_module_documentation[] =
"This module provides access to the RISC OS Unicode font library\n"
"All string parameters must be in UTF-8."
;

void
initrufl(void)
{
	PyObject *module;
	rufl_code code;

	code = rufl_init();
	if (code != rufl_OK)
		Py_FatalError("rufl_init() failed");

	Py_AtExit(rufl_quit);

	/* Create the module and add the functions */
	module = Py_InitModule4("rufl", pyrufl_methods,
			pyrufl_module_documentation,
			(PyObject *) NULL, PYTHON_API_VERSION);

	/* Add some symbolic constants to the module */
	PyModule_AddIntConstant(module, "regular", rufl_REGULAR);
	PyModule_AddIntConstant(module, "slanted", rufl_SLANTED);
	PyModule_AddIntConstant(module, "bold", rufl_BOLD);
	PyModule_AddIntConstant(module, "bold_slanted", rufl_BOLD_SLANTED);

	PyModule_AddIntConstant(module, "blend", rufl_BLEND_FONT);

	/* Check for errors */
	if (PyErr_Occurred())
		Py_FatalError("can't initialize module rufl");
}

