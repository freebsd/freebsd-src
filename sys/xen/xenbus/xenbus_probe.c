/******************************************************************************
 * Talks to Xen Store to figure out what devices we have.
 *
 * Copyright (C) 2008 Doug Rabson
 * Copyright (C) 2005 Rusty Russell, IBM Corporation
 * Copyright (C) 2005 Mike Wray, Hewlett-Packard
 * Copyright (C) 2005 XenSource Ltd
 * 
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#if 0
#define DPRINTK(fmt, args...) \
    printf("xenbus_probe (%s:%d) " fmt ".\n", __FUNCTION__, __LINE__, ##args)
#else
#define DPRINTK(fmt, args...) ((void)0)
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>

#include <machine/xen/xen-os.h>
#include <machine/stdarg.h>

#include <xen/gnttab.h>
#include <xen/xenbus/xenbusvar.h>
#include <xen/xenbus/xenbus_comms.h>

struct xenbus_softc {
	struct xenbus_watch xs_devicewatch;
	struct task	xs_probechildren;
	struct intr_config_hook xs_attachcb;
	device_t	xs_dev;
};

struct xenbus_device_ivars {
	struct xenbus_watch xd_otherend_watch; /* must be first */
	struct sx	xd_lock;
	device_t	xd_dev;
	char		*xd_node;	/* node name in xenstore */
	char		*xd_type;	/* xen device type */
	enum xenbus_state xd_state;
	int		xd_otherend_id;
	char		*xd_otherend_path;
};

/* Simplified asprintf. */
char *
kasprintf(const char *fmt, ...)
{
	va_list ap;
	unsigned int len;
	char *p, dummy[1];

	va_start(ap, fmt);
	/* FIXME: vsnprintf has a bug, NULL should work */
	len = vsnprintf(dummy, 0, fmt, ap);
	va_end(ap);

	p = kmalloc(len + 1, GFP_KERNEL);
	if (!p)
		return NULL;
	va_start(ap, fmt);
	vsprintf(p, fmt, ap);
	va_end(ap);
	return p;
}

static void
xenbus_identify(driver_t *driver, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, "xenbus", 0);
}

static int 
xenbus_probe(device_t dev)
{
	int err = 0;

	DPRINTK("");

	/* Initialize the interface to xenstore. */
	err = xs_init(); 
	if (err) {
		log(LOG_WARNING,
		    "XENBUS: Error initializing xenstore comms: %i\n", err);
		return (ENXIO);
	}
	err = gnttab_init();
	if (err) {
		log(LOG_WARNING,
		    "XENBUS: Error initializing grant table: %i\n", err);
		return (ENXIO);
	}
	device_set_desc(dev, "Xen Devices");

	return (0);
}

static enum xenbus_state
xenbus_otherend_state(struct xenbus_device_ivars *ivars)
{

	return (xenbus_read_driver_state(ivars->xd_otherend_path));
}

static void
xenbus_backend_changed(struct xenbus_watch *watch, const char **vec,
    unsigned int len)
{
	struct xenbus_device_ivars *ivars;
	device_t dev;
	enum xenbus_state newstate;

	ivars = (struct xenbus_device_ivars *) watch;
	dev = ivars->xd_dev;

	if (!ivars->xd_otherend_path
	    || strncmp(ivars->xd_otherend_path, vec[XS_WATCH_PATH],
		strlen(ivars->xd_otherend_path)))
		return;

	newstate = xenbus_otherend_state(ivars);
	XENBUS_BACKEND_CHANGED(dev, newstate);
}

static int
xenbus_device_exists(device_t dev, const char *node)
{
	device_t *kids;
	struct xenbus_device_ivars *ivars;
	int i, count, result;

	if (device_get_children(dev, &kids, &count))
		return (FALSE);

	result = FALSE;
	for (i = 0; i < count; i++) {
		ivars = device_get_ivars(kids[i]);
		if (!strcmp(ivars->xd_node, node)) {
			result = TRUE;
			break;
		}
	}
	free(kids, M_TEMP);

	return (result);
}

static int
xenbus_add_device(device_t dev, const char *bus,
    const char *type, const char *id)
{
	device_t child;
	struct xenbus_device_ivars *ivars;
	enum xenbus_state state;
	char *statepath;

	ivars = malloc(sizeof(struct xenbus_device_ivars),
	    M_DEVBUF, M_ZERO|M_WAITOK);
	ivars->xd_node = kasprintf("%s/%s/%s", bus, type, id);

	if (xenbus_device_exists(dev, ivars->xd_node)) {
		/*
		 * We are already tracking this node
		 */
		free(ivars->xd_node, M_DEVBUF);
		free(ivars, M_DEVBUF);
		return (0);
	}

	state = xenbus_read_driver_state(ivars->xd_node);

	if (state != XenbusStateInitialising) {
		/*
		 * Device is not new, so ignore it. This can
		 * happen if a device is going away after
		 * switching to Closed.
		 */
		free(ivars->xd_node, M_DEVBUF);
		free(ivars, M_DEVBUF);
		return (0);
	}

	/*
	 * Find the backend details
	 */
	xenbus_gather(XBT_NIL, ivars->xd_node,
	    "backend-id", "%i", &ivars->xd_otherend_id,
	    "backend", NULL, &ivars->xd_otherend_path,
	    NULL);

	sx_init(&ivars->xd_lock, "xdlock");
	ivars->xd_type = strdup(type, M_DEVBUF);
	ivars->xd_state = XenbusStateInitialising;

	statepath = malloc(strlen(ivars->xd_otherend_path)
	    + strlen("/state") + 1, M_DEVBUF, M_NOWAIT);
	sprintf(statepath, "%s/state", ivars->xd_otherend_path);

	ivars->xd_otherend_watch.node = statepath;
	ivars->xd_otherend_watch.callback = xenbus_backend_changed;

	child = device_add_child(dev, NULL, -1);
	ivars->xd_dev = child;
	device_set_ivars(child, ivars);

	return (0);
}

static int
xenbus_enumerate_type(device_t dev, const char *bus, const char *type)
{
	char **dir;
	unsigned int i, count;

	dir = xenbus_directory(XBT_NIL, bus, type, &count);
	if (IS_ERR(dir))
		return (EINVAL);
	for (i = 0; i < count; i++)
		xenbus_add_device(dev, bus, type, dir[i]);

	free(dir, M_DEVBUF);

	return (0);
}

static int
xenbus_enumerate_bus(device_t dev, const char *bus)
{
	char **dir;
	unsigned int i, count;

	dir = xenbus_directory(XBT_NIL, bus, "", &count);
	if (IS_ERR(dir))
		return (EINVAL);
	for (i = 0; i < count; i++) {
		xenbus_enumerate_type(dev, bus, dir[i]);
	}
	free(dir, M_DEVBUF);

	return (0);
}

static int
xenbus_probe_children(device_t dev)
{
	device_t *kids;
	struct xenbus_device_ivars *ivars;
	int i, count;

	/*
	 * Probe any new devices and register watches for any that
	 * attach successfully. Since part of the protocol which
	 * establishes a connection with the other end is interrupt
	 * driven, we sleep until the device reaches a stable state
	 * (closed or connected).
	 */
	if (device_get_children(dev, &kids, &count) == 0) {
		for (i = 0; i < count; i++) {
			if (device_get_state(kids[i]) != DS_NOTPRESENT)
				continue;

			if (device_probe_and_attach(kids[i]))
				continue;
			ivars = device_get_ivars(kids[i]);
			register_xenbus_watch(
				&ivars->xd_otherend_watch);
			sx_xlock(&ivars->xd_lock);
			while (ivars->xd_state != XenbusStateClosed
			    && ivars->xd_state != XenbusStateConnected)
				sx_sleep(&ivars->xd_state, &ivars->xd_lock,
				    0, "xdattach", 0);
			sx_xunlock(&ivars->xd_lock);
		}
		free(kids, M_TEMP);
	}

	return (0);
}

static void
xenbus_probe_children_cb(void *arg, int pending)
{
	device_t dev = (device_t) arg;

	xenbus_probe_children(dev);
}

static void
xenbus_devices_changed(struct xenbus_watch *watch,
    const char **vec, unsigned int len)
{
	struct xenbus_softc *sc = (struct xenbus_softc *) watch;
	device_t dev = sc->xs_dev;
	char *node, *bus, *type, *id, *p;

	node = strdup(vec[XS_WATCH_PATH], M_DEVBUF);;
	p = strchr(node, '/');
	if (!p)
		goto out;
	bus = node;
	*p = 0;
	type = p + 1;

	p = strchr(type, '/');
	if (!p)
		goto out;
	*p = 0;
	id = p + 1;

	p = strchr(id, '/');
	if (p)
		*p = 0;

	xenbus_add_device(dev, bus, type, id);
	taskqueue_enqueue(taskqueue_thread, &sc->xs_probechildren);
out:
	free(node, M_DEVBUF);
}

static void
xenbus_attach_deferred(void *arg)
{
	device_t dev = (device_t) arg;
	struct xenbus_softc *sc = device_get_softc(dev);
	int error;
	
	error = xenbus_enumerate_bus(dev, "device");
	if (error)
		return;
	xenbus_probe_children(dev);

	sc->xs_dev = dev;
	sc->xs_devicewatch.node = "device";
	sc->xs_devicewatch.callback = xenbus_devices_changed;

	TASK_INIT(&sc->xs_probechildren, 0, xenbus_probe_children_cb, dev);

	register_xenbus_watch(&sc->xs_devicewatch);

	config_intrhook_disestablish(&sc->xs_attachcb);
}

static int
xenbus_attach(device_t dev)
{
	struct xenbus_softc *sc = device_get_softc(dev);

	sc->xs_attachcb.ich_func = xenbus_attach_deferred;
	sc->xs_attachcb.ich_arg = dev;
	config_intrhook_establish(&sc->xs_attachcb);

	return (0);
}

static void
xenbus_suspend(device_t dev)
{
	DPRINTK("");
	panic("implement me");
#if 0
	bus_for_each_dev(&xenbus_frontend.bus, NULL, NULL, suspend_dev);
	bus_for_each_dev(&xenbus_backend.bus, NULL, NULL, suspend_dev);
#endif
	xs_suspend();
}

static void
xenbus_resume(device_t dev)
{
	xb_init_comms();
	xs_resume();
	panic("implement me");
#if 0
	bus_for_each_dev(&xenbus_frontend.bus, NULL, NULL, resume_dev);
	bus_for_each_dev(&xenbus_backend.bus, NULL, NULL, resume_dev);
#endif 
}

static int
xenbus_print_child(device_t dev, device_t child)
{
	struct xenbus_device_ivars *ivars = device_get_ivars(child);
	int	retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += printf(" at %s", ivars->xd_node);
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static int
xenbus_read_ivar(device_t dev, device_t child, int index,
    uintptr_t * result)
{
	struct xenbus_device_ivars *ivars = device_get_ivars(child);

	switch (index) {
	case XENBUS_IVAR_NODE:
		*result = (uintptr_t) ivars->xd_node;
		return (0);
	case XENBUS_IVAR_TYPE:
		*result = (uintptr_t) ivars->xd_type;
		return (0);
	case XENBUS_IVAR_STATE:
		*result = (uintptr_t) ivars->xd_state;
		return (0);

	case XENBUS_IVAR_OTHEREND_ID:
		*result = (uintptr_t) ivars->xd_otherend_id;
		return (0);

	case XENBUS_IVAR_OTHEREND_PATH:
		*result = (uintptr_t) ivars->xd_otherend_path;
		return (0);
	}

	return (ENOENT);
}

static int
xenbus_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
{
	struct xenbus_device_ivars *ivars = device_get_ivars(child);
	enum xenbus_state newstate;
	int currstate;
	int error;

	switch (index) {
	case XENBUS_IVAR_STATE:
		newstate = (enum xenbus_state) value;
		sx_xlock(&ivars->xd_lock);
		if (ivars->xd_state == newstate)
			goto out;

		error = xenbus_scanf(XBT_NIL, ivars->xd_node, "state",
		    "%d", &currstate);
		if (error < 0)
			goto out;

		error = xenbus_printf(XBT_NIL, ivars->xd_node, "state",
		    "%d", newstate);
		if (error) {
			if (newstate != XenbusStateClosing) /* Avoid looping */
				xenbus_dev_fatal(dev, error, "writing new state");
			goto out;
		}
		ivars->xd_state = newstate;
		wakeup(&ivars->xd_state);
	out:
		sx_xunlock(&ivars->xd_lock);
		return (0);

	case XENBUS_IVAR_NODE:
	case XENBUS_IVAR_TYPE:
	case XENBUS_IVAR_OTHEREND_ID:
	case XENBUS_IVAR_OTHEREND_PATH:
		/*
		 * These variables are read-only.
		 */
		return (EINVAL);
	}
	return (ENOENT);
}

SYSCTL_DECL(_dev);
SYSCTL_NODE(_dev, OID_AUTO, xen, CTLFLAG_RD, NULL, "Xen");
#if 0
SYSCTL_INT(_dev_xen, OID_AUTO, xsd_port, CTLFLAG_RD, &xen_store_evtchn, 0, "");
SYSCTL_ULONG(_dev_xen, OID_AUTO, xsd_kva, CTLFLAG_RD, (u_long *) &xen_store, 0, "");
#endif

static device_method_t xenbus_methods[] = { 
	/* Device interface */ 
	DEVMETHOD(device_identify,	xenbus_identify),
	DEVMETHOD(device_probe,         xenbus_probe), 
	DEVMETHOD(device_attach,        xenbus_attach), 
	DEVMETHOD(device_detach,        bus_generic_detach), 
	DEVMETHOD(device_shutdown,      bus_generic_shutdown), 
	DEVMETHOD(device_suspend,       xenbus_suspend), 
	DEVMETHOD(device_resume,        xenbus_resume), 
 
	/* Bus interface */ 
	DEVMETHOD(bus_print_child,      xenbus_print_child),
	DEVMETHOD(bus_read_ivar,        xenbus_read_ivar), 
	DEVMETHOD(bus_write_ivar,       xenbus_write_ivar), 
 
	{ 0, 0 } 
}; 

static char driver_name[] = "xenbus";
static driver_t xenbus_driver = { 
	driver_name, 
	xenbus_methods, 
	sizeof(struct xenbus_softc),
}; 
devclass_t xenbus_devclass; 
 
#ifdef XENHVM
DRIVER_MODULE(xenbus, xenpci, xenbus_driver, xenbus_devclass, 0, 0);
#else
DRIVER_MODULE(xenbus, nexus, xenbus_driver, xenbus_devclass, 0, 0);
#endif
