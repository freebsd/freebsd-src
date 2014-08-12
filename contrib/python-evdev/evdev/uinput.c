#include <Python.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef __FreeBSD__
#include <dev/evdev/input.h>
#include <dev/evdev/uinput.h>
#else
#include <linux/input.h>
#include <linux/uinput.h>
#endif

int _uinput_close(int fd)
{
    if (ioctl(fd, UI_DEV_DESTROY) < 0) {
        int oerrno = errno;
        close(fd);
        errno = oerrno;
        return -1;
    }

    return close(fd);
}


static PyObject *
uinput_open(PyObject *self, PyObject *args)
{
    const char* devnode;

    int ret = PyArg_ParseTuple(args, "s", &devnode);
    if (!ret) return NULL;

    int fd = open(devnode, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        PyErr_SetString(PyExc_IOError, "could not open uinput device in write mode");
        return NULL;
    }

    return Py_BuildValue("i", fd);
}


static PyObject *
uinput_create(PyObject *self, PyObject *args) {
    int fd, len, i, abscode;
    uint16_t vendor, product, version, bustype;

    PyObject *absinfo = NULL, *item = NULL;

    struct uinput_user_dev uidev;
    const char* name;

    int ret = PyArg_ParseTuple(args, "ishhhhO", &fd, &name, &vendor,
                               &product, &version, &bustype, &absinfo);
    if (!ret) return NULL;

    memset(&uidev, 0, sizeof(uidev));
    strncpy(uidev.name, name, UINPUT_MAX_NAME_SIZE);
    uidev.id.vendor  = vendor;
    uidev.id.product = product;
    uidev.id.version = version;
    uidev.id.bustype = bustype;

    len = PyList_Size(absinfo);
    for (i=0; i<len; i++) {
        // item -> (ABS_X, 0, 255, 0, 0)
        item = PyList_GetItem(absinfo, i);
        abscode = (int)PyLong_AsLong(PyList_GetItem(item, 0));

        uidev.absmin[abscode]  = PyLong_AsLong(PyList_GetItem(item, 1));
        uidev.absmax[abscode]  = PyLong_AsLong(PyList_GetItem(item, 2));
        uidev.absfuzz[abscode] = PyLong_AsLong(PyList_GetItem(item, 3));
        uidev.absflat[abscode] = PyLong_AsLong(PyList_GetItem(item, 4));
    }

    if (write(fd, &uidev, sizeof(uidev)) != sizeof(uidev))
        goto on_err;

	/* if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) */
    /*     goto on_err; */
    /* int i; */
	/* for (i=0; i<KEY_MAX && fd; i++) { */
	/* 	if (ioctl(fd, UI_SET_KEYBIT, i) < 0) */
    /*         goto on_err; */
	/* } */

	if (ioctl(fd, UI_DEV_CREATE) < 0)
        goto on_err;

    Py_RETURN_NONE;

    on_err:
        _uinput_close(fd);
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
}


static PyObject *
uinput_close(PyObject *self, PyObject *args)
{
    int fd;

    int ret = PyArg_ParseTuple(args, "i", &fd);
    if (!ret) return NULL;

    if (_uinput_close(fd) < 0) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }

    Py_RETURN_NONE;
}


static PyObject *
uinput_write(PyObject *self, PyObject *args)
{
    int fd, type, code, value;

    int ret = PyArg_ParseTuple(args, "iiii", &fd, &type, &code, &value);
    if (!ret) return NULL;

    struct input_event event;
    memset(&event, 0, sizeof(event));
    gettimeofday(&event.time, 0);
    event.type = type;
    event.code = code;
    event.value = value;

    if (write(fd, &event, sizeof(event)) != sizeof(event)) {
        // @todo: elaborate
        // PyErr_SetString(PyExc_IOError, "error writing event to uinput device");
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }

    Py_RETURN_NONE;
}


static PyObject *
uinput_enable_event(PyObject *self, PyObject *args)
{
    int fd;
    uint16_t type, code;
    unsigned long req;

    int ret = PyArg_ParseTuple(args, "ihh", &fd, &type, &code);
    if (!ret) return NULL;

    switch (type) {
        case EV_KEY: req = UI_SET_KEYBIT; break;
        case EV_ABS: req = UI_SET_ABSBIT; break;
        case EV_REL: req = UI_SET_RELBIT; break;
        case EV_MSC: req = UI_SET_MSCBIT; break;
        case EV_SW:  req = UI_SET_SWBIT;  break;
        case EV_LED: req = UI_SET_LEDBIT; break;
        case EV_FF:  req = UI_SET_FFBIT;  break;
        case EV_SND: req = UI_SET_SNDBIT; break;
        default:
            errno = EINVAL;
            goto on_err;
    }

    if (ioctl(fd, UI_SET_EVBIT, type) < 0)
        goto on_err;

    if (ioctl(fd, req, code) < 0)
        goto on_err;

    Py_RETURN_NONE;

    on_err:
        _uinput_close(fd);
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
}


#define MODULE_NAME "_uinput"
#define MODULE_HELP "Python bindings for parts of linux/uinput.c"

static PyMethodDef MethodTable[] = {
    { "open",  uinput_open, METH_VARARGS,
      "Open uinput device node."},

    { "create",  uinput_create, METH_VARARGS,
      "Create an uinput device."},

    { "close",  uinput_close, METH_VARARGS,
      "Destroy uinput device."},

    { "write",  uinput_write, METH_VARARGS,
      "Write event to uinput device."},

    { "enable", uinput_enable_event, METH_VARARGS,
      "Enable a type of event."},

    { NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    MODULE_NAME,
    MODULE_HELP,
    -1,          /* m_size */
    MethodTable, /* m_methods */
    NULL,        /* m_reload */
    NULL,        /* m_traverse */
    NULL,        /* m_clear */
    NULL,        /* m_free */
};

static PyObject *
moduleinit(void)
{
    PyObject* m = PyModule_Create(&moduledef);
    if (m == NULL) return NULL;

    PyModule_AddIntConstant(m, "maxnamelen", UINPUT_MAX_NAME_SIZE);
    return m;
}

PyMODINIT_FUNC
PyInit__uinput(void)
{
    return moduleinit();
}

#else
static PyObject *
moduleinit(void)
{
    PyObject* m = Py_InitModule3(MODULE_NAME, MethodTable, MODULE_HELP);
    if (m == NULL) return NULL;

    PyModule_AddIntConstant(m, "maxnamelen", UINPUT_MAX_NAME_SIZE);
    return m;
}

PyMODINIT_FUNC
init_uinput(void)
{
    moduleinit();
}
#endif
