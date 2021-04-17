/*
 * Python bindings for wpa_ctrl (wpa_supplicant/hostapd control interface)
 * Copyright (c) 2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <Python.h>
#include <structmember.h>

#include "wpa_ctrl.h"


struct wpaspy_obj {
	PyObject_HEAD
	struct wpa_ctrl *ctrl;
	int attached;
};

static PyObject *wpaspy_error;


static int wpaspy_open(struct wpaspy_obj *self, PyObject *args)
{
	const char *path;

	if (!PyArg_ParseTuple(args, "s", &path))
		return -1;
	self->ctrl = wpa_ctrl_open(path);
	if (self->ctrl == NULL)
		return -1;
	self->attached = 0;
	return 0;
}


static void wpaspy_close(struct wpaspy_obj *self)
{
	if (self->ctrl) {
		if (self->attached)
			wpa_ctrl_detach(self->ctrl);
		wpa_ctrl_close(self->ctrl);
		self->ctrl = NULL;
	}

	PyObject_Del(self);
}


static PyObject * wpaspy_request(struct wpaspy_obj *self, PyObject *args)
{
	const char *cmd;
	char buf[4096];
	size_t buflen;
	int ret;

	if (!PyArg_ParseTuple(args, "s", &cmd))
		return NULL;

	buflen = sizeof(buf) - 1;
	ret = wpa_ctrl_request(self->ctrl, cmd, strlen(cmd), buf, &buflen,
			       NULL);
	if (ret == -2) {
		PyErr_SetString(wpaspy_error, "Request timed out");
		return NULL;
	}
	if (ret) {
		PyErr_SetString(wpaspy_error, "Request failed");
		return NULL;
	}

	buf[buflen] = '\0';
	return Py_BuildValue("s", buf);
}


static PyObject * wpaspy_attach(struct wpaspy_obj *self)
{
	int ret;

	if (self->attached)
		Py_RETURN_NONE;

	ret = wpa_ctrl_attach(self->ctrl);
	if (ret) {
		PyErr_SetString(wpaspy_error, "Attach failed");
		return NULL;
	}
	Py_RETURN_NONE;
}


static PyObject * wpaspy_detach(struct wpaspy_obj *self)
{
	int ret;

	if (!self->attached)
		Py_RETURN_NONE;

	ret = wpa_ctrl_detach(self->ctrl);
	if (ret) {
		PyErr_SetString(wpaspy_error, "Detach failed");
		return NULL;
	}
	Py_RETURN_NONE;
}


static PyObject * wpaspy_pending(struct wpaspy_obj *self)
{
	switch (wpa_ctrl_pending(self->ctrl)) {
	case 1:
		Py_RETURN_TRUE;
	case 0:
		Py_RETURN_FALSE;
	default:
		PyErr_SetString(wpaspy_error, "wpa_ctrl_pending failed");
		break;
	}

	return NULL;
}


static PyObject * wpaspy_recv(struct wpaspy_obj *self)
{
	int ret;
	char buf[4096];
	size_t buflen;

	buflen = sizeof(buf) - 1;
	Py_BEGIN_ALLOW_THREADS
	ret = wpa_ctrl_recv(self->ctrl, buf, &buflen);
	Py_END_ALLOW_THREADS

	if (ret) {
		PyErr_SetString(wpaspy_error, "wpa_ctrl_recv failed");
		return NULL;
	}

	buf[buflen] = '\0';
	return Py_BuildValue("s", buf);
}


static PyMethodDef wpaspy_methods[] = {
	{
		"request", (PyCFunction) wpaspy_request, METH_VARARGS,
		"Send a control interface command and return response"
	},
	{
		"attach", (PyCFunction) wpaspy_attach, METH_NOARGS,
		"Attach as an event monitor"
	},
	{
		"detach", (PyCFunction) wpaspy_detach, METH_NOARGS,
		"Detach an event monitor"
	},
	{
		"pending", (PyCFunction) wpaspy_pending, METH_NOARGS,
		"Check whether any events are pending"
	},
	{
		"recv", (PyCFunction) wpaspy_recv, METH_NOARGS,
		"Received pending event"
	},
	{ NULL, NULL, 0, NULL }
};

static PyMemberDef wpaspy_members[] = {
	{
		"attached", T_INT, offsetof(struct wpaspy_obj, attached),
		READONLY,
		"Whether instance is attached as event monitor"
	},
	{ NULL }
};

static PyTypeObject wpaspy_ctrl = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "wpaspy.Ctrl",
	.tp_basicsize = sizeof(struct wpaspy_obj),
	.tp_getattro = PyObject_GenericGetAttr,
	.tp_setattro = PyObject_GenericSetAttr,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_methods = wpaspy_methods,
	.tp_members = wpaspy_members,
	.tp_init = (initproc) wpaspy_open,
	.tp_dealloc = (destructor) wpaspy_close,
	.tp_new = PyType_GenericNew,
};


#if PY_MAJOR_VERSION < 3
static PyMethodDef module_methods[] = {
	{ NULL, NULL, 0, NULL }
};


PyMODINIT_FUNC initwpaspy(void)
{
	PyObject *mod;

	PyType_Ready(&wpaspy_ctrl);
	mod = Py_InitModule("wpaspy", module_methods);
	wpaspy_error = PyErr_NewException("wpaspy.error", NULL, NULL);

	Py_INCREF(&wpaspy_ctrl);
	Py_INCREF(wpaspy_error);

	PyModule_AddObject(mod, "Ctrl", (PyObject *) &wpaspy_ctrl);
	PyModule_AddObject(mod, "error", wpaspy_error);
}
#else
static struct PyModuleDef wpaspy_def = {
	PyModuleDef_HEAD_INIT,
	"wpaspy",
};


PyMODINIT_FUNC initwpaspy(void)
{
	PyObject *mod;

	mod = PyModule_Create(&wpaspy_def);
	if (!mod)
		return NULL;

	wpaspy_error = PyErr_NewException("wpaspy.error", NULL, NULL);

	Py_INCREF(&wpaspy_ctrl);
	Py_INCREF(wpaspy_error);

	if (PyModule_AddObject(mod, "Ctrl", (PyObject *) &wpaspy_ctrl) < 0 ||
	    PyModule_AddObject(mod, "error", wpaspy_error) < 0) {
		Py_DECREF(&wpaspy_ctrl);
		Py_DECREF(wpaspy_error);
		Py_DECREF(mod);
		mod = NULL;
	}

	return mod;
}
#endif
