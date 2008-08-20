/******************************************************************************
 * Talks to Xen Store to figure out what devices we have.
 *
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
#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/sema.h>
#include <sys/eventhandler.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/sx.h>

#include <machine/xen/xen-os.h>
#include <machine/xen/hypervisor.h>
#include <machine/xen/xenbus.h>
#include <machine/xen/evtchn.h>
#include <machine/stdarg.h>

#include <xen/xenbus/xenbus_comms.h>

struct xendev_list_head xenbus_device_frontend_list;
struct xendev_list_head xenbus_device_backend_list;
static LIST_HEAD(, xenbus_driver) xendrv_list; 

extern struct sx xenwatch_mutex;

EVENTHANDLER_DECLARE(xenstore_event, xenstore_event_handler_t);
static struct eventhandler_list *xenstore_chain;
device_t xenbus_dev;
device_t xenbus_backend_dev;
static MALLOC_DEFINE(M_XENDEV, "xenintrdrv", "xen system device");

#define streq(a, b) (strcmp((a), (b)) == 0)

static int watch_otherend(struct xenbus_device *dev);


/* If something in array of ids matches this device, return it. */
static const struct xenbus_device_id *
match_device(const struct xenbus_device_id *arr, struct xenbus_device *dev)
{
	for (; !streq(arr->devicetype, ""); arr++) {
		if (streq(arr->devicetype, dev->devicetype))
			return arr;
	}
	return NULL;
}

#if 0
static int xenbus_match(device_t _dev)
{
	struct xenbus_driver *drv; 
	struct xenbus_device *dev;

	dev = device_get_softc(_dev);
	drv = dev->driver;

	if (!drv->ids)
		return 0;

	return match_device(drv->ids, to_xenbus_device(_dev)) != NULL;
}
#endif


/* device/<type>/<id> => <type>-<id> */
static int frontend_bus_id(char bus_id[BUS_ID_SIZE], const char *nodename)
{
	nodename = strchr(nodename, '/');
	if (!nodename || strlen(nodename + 1) >= BUS_ID_SIZE) {
			log(LOG_WARNING, "XENBUS: bad frontend %s\n", nodename);
		return -EINVAL;
	}

	strlcpy(bus_id, nodename + 1, BUS_ID_SIZE);
	if (!strchr(bus_id, '/')) {
			log(LOG_WARNING, "XENBUS: bus_id %s no slash\n", bus_id);
		return -EINVAL;
	}
	*strchr(bus_id, '/') = '-';
	return 0;
}


static void free_otherend_details(struct xenbus_device *dev)
{
		kfree((void*)(uintptr_t)dev->otherend);
		dev->otherend = NULL;
}


static void free_otherend_watch(struct xenbus_device *dev)
{
	if (dev->otherend_watch.node) {
		unregister_xenbus_watch(&dev->otherend_watch);
		kfree(dev->otherend_watch.node);
		dev->otherend_watch.node = NULL;
	}
}

int 
read_otherend_details(struct xenbus_device *xendev, char *id_node, 
					  char *path_node)
{
	int err = xenbus_gather(XBT_NIL, xendev->nodename,
				id_node, "%i", &xendev->otherend_id,
				path_node, NULL, &xendev->otherend,
				NULL);
	if (err) {
		xenbus_dev_fatal(xendev, err,
				 "reading other end details from %s",
				 xendev->nodename);
		return err;
	}
	if (strlen(xendev->otherend) == 0 ||
	    !xenbus_exists(XBT_NIL, xendev->otherend, "")) {
		xenbus_dev_fatal(xendev, -ENOENT, "missing other end from %s",
						 xendev->nodename);
		kfree((void *)(uintptr_t)xendev->otherend);
		xendev->otherend = NULL;
		return -ENOENT;
	}

	return 0;
}


static int read_backend_details(struct xenbus_device *xendev)
{
	return read_otherend_details(xendev, "backend-id", "backend");
}

#ifdef notyet
/* XXX - move to probe backend */
static int read_frontend_details(struct xenbus_device *xendev)
{
	if (strncmp(xendev->nodename, "backend", 7))
		return -ENOENT;
	return read_otherend_details(xendev, "frontend-id", "frontend");
}
#endif

/* Bus type for frontend drivers. */
static int xenbus_probe_frontend(const char *type, const char *name);
static struct xen_bus_type xenbus_frontend = {
	.root = "device",
	.levels = 2, 		/* device/type/<id> */
	.get_bus_id = frontend_bus_id,
	.probe = xenbus_probe_frontend,
	.bus = &xenbus_device_frontend_list,
#if 0
	/* this initialization needs to happen dynamically */
	.bus = {
		.name  = "xen",
		.match = xenbus_match,
	},
	.dev = {
		.bus_id = "xen",
	},
#endif
};

#if 0
static int xenbus_hotplug_backend(device_t dev, char **envp,
				  int num_envp, char *buffer, int buffer_size)
{
	panic("implement me");
#if 0
	struct xenbus_device *xdev;
	struct xenbus_driver *drv = NULL;
	int i = 0;
	int length = 0;
	char *basepath_end;
	char *frontend_id;

	DPRINTK("");

	if (dev == NULL)
		return -ENODEV;

	xdev = to_xenbus_device(dev);
	if (xdev == NULL)
		return -ENODEV;

	if (dev->driver)
		drv = to_xenbus_driver(dev->driver);

	/* stuff we want to pass to /sbin/hotplug */
	add_hotplug_env_var(envp, num_envp, &i,
			    buffer, buffer_size, &length,
			    "XENBUS_TYPE=%s", xdev->devicetype);

	add_hotplug_env_var(envp, num_envp, &i,
			    buffer, buffer_size, &length,
			    "XENBUS_PATH=%s", xdev->nodename);

	add_hotplug_env_var(envp, num_envp, &i,
			    buffer, buffer_size, &length,
			    "XENBUS_BASE_PATH=%s", xdev->nodename);

	basepath_end = strrchr(envp[i - 1], '/');
	length -= strlen(basepath_end);
	*basepath_end = '\0';
	basepath_end = strrchr(envp[i - 1], '/');
	length -= strlen(basepath_end);
	*basepath_end = '\0';

	basepath_end++;
	frontend_id = kmalloc(strlen(basepath_end) + 1, GFP_KERNEL);
	strcpy(frontend_id, basepath_end);
	add_hotplug_env_var(envp, num_envp, &i,
			    buffer, buffer_size, &length,
			    "XENBUS_FRONTEND_ID=%s", frontend_id);
	kfree(frontend_id);

	/* terminate, set to next free slot, shrink available space */
	envp[i] = NULL;
	envp = &envp[i];
	num_envp -= i;
	buffer = &buffer[length];
	buffer_size -= length;

	if (drv && drv->hotplug)
		return drv->hotplug(xdev, envp, num_envp, buffer, buffer_size);

#endif
	return 0;
}
#endif

#if 0
static int xenbus_probe_backend(const char *type, const char *domid, int unit);
static struct xen_bus_type xenbus_backend = {
	.root = "backend",
	.levels = 3, 		/* backend/type/<frontend>/<id> */
	.get_bus_id = backend_bus_id,
	.probe = xenbus_probe_backend,
	/* at init time */
	.bus = &xenbus_device_backend_list,
#if 0
	.bus = {
		.name  = "xen-backend",
		.match = xenbus_match,
		.hotplug = xenbus_hotplug_backend,
	},
	.dev = {
		.bus_id = "xen-backend",
	},
#endif
};
#endif

static void otherend_changed(struct xenbus_watch *watch,
			     const char **vec, unsigned int len)
{

	struct xenbus_device *dev = (struct xenbus_device *)watch;
	struct xenbus_driver *drv = dev->driver;
	XenbusState state;
		
	/* Protect us against watches firing on old details when the otherend
	   details change, say immediately after a resume. */
	if (!dev->otherend || strncmp(dev->otherend, vec[XS_WATCH_PATH],
	    strlen(dev->otherend))) {
		DPRINTK("Ignoring watch at %s", vec[XS_WATCH_PATH]);
		return;
	}

	state = xenbus_read_driver_state(dev->otherend);

	DPRINTK("state is %d, %s, %s", state, dev->otherend_watch.node,
		vec[XS_WATCH_PATH]);

	/*
	 * Ignore xenbus transitions during shutdown. This prevents us doing
	 * work that can fail e.g., when the rootfs is gone.
	 */
#if 0	
	if (system_state > SYSTEM_RUNNING) {
		struct xen_bus_type *bus = bus;
		bus = container_of(dev->dev.bus, struct xen_bus_type, bus);
		/* If we're frontend, drive the state machine to Closed. */
		/* This should cause the backend to release our resources. */
		if ((bus == &xenbus_frontend) && (state == XenbusStateClosing))
			xenbus_frontend_closed(dev);
		return;
	}
#endif
	if (drv->otherend_changed)
		drv->otherend_changed(dev, state);

}


static int talk_to_otherend(struct xenbus_device *dev)
{
	struct xenbus_driver *drv;

	drv = dev->driver;

	free_otherend_watch(dev);
	free_otherend_details(dev);

	return drv->read_otherend_details(dev);
}

static int watch_otherend(struct xenbus_device *dev)
{
	return xenbus_watch_path2(dev, dev->otherend, "state",
				  &dev->otherend_watch, otherend_changed);
}

static int 
xenbus_dev_probe(struct xenbus_device *dev)
{
	struct xenbus_driver *drv = dev->driver;
	const struct xenbus_device_id *id;
	int err;
		
	DPRINTK("");
		
	if (!drv->probe) {
		err = -ENODEV;
		goto fail;
	}
		
	id = match_device(drv->ids, dev);
	if (!id) {
		err = -ENODEV;
		goto fail;
	}
		
	err = talk_to_otherend(dev);
	if (err) {
			log(LOG_WARNING,
		       "xenbus_probe: talk_to_otherend on %s failed.\n",
		       dev->nodename);
		return err;
	}
		
	err = drv->probe(dev, id);
	if (err)
		goto fail;
	
	err = watch_otherend(dev);
	if (err) {
			log(LOG_WARNING,
		   "xenbus_probe: watch_otherend on %s failed.\n",
		   dev->nodename);
		return err;
	}
		
	return 0;
 fail:
	xenbus_dev_error(dev, err, "xenbus_dev_probe on %s", dev->nodename);
	xenbus_switch_state(dev, XenbusStateClosed);
	return -ENODEV;		
}

static void xenbus_dev_free(struct xenbus_device *xendev)
{
	LIST_REMOVE(xendev, list);
	kfree(xendev);
}

int
xenbus_remove_device(struct xenbus_device *dev)
{
	struct xenbus_driver *drv = dev->driver;

	DPRINTK("");

	free_otherend_watch(dev);
	free_otherend_details(dev);

	if (drv->remove)
		drv->remove(dev);

	xenbus_switch_state(dev, XenbusStateClosed);

	if (drv->cleanup_device)
		return drv->cleanup_device(dev);

	xenbus_dev_free(dev);

	return 0;
}

#if 0
static int 
xenbus_dev_remove(device_t _dev)
{
	return xenbus_remove_device(to_xenbus_device(_dev));
}
#endif

int xenbus_register_driver_common(struct xenbus_driver *drv,
					 struct xen_bus_type *bus)
{
	struct xenbus_device *xdev;
		
#if 0
	int ret;
	/* this all happens in the driver itself 
	 * doing this here simple serves to obfuscate
	 */

	drv->driver.name = drv->name;
	drv->driver.bus = &bus->bus;
	drv->driver.owner = drv->owner;
	drv->driver.probe = xenbus_dev_probe;
	drv->driver.remove = xenbus_dev_remove;

	return ret;
#endif
	sx_xlock(&xenwatch_mutex);
	LIST_INSERT_HEAD(&xendrv_list, drv, list);
	sx_xunlock(&xenwatch_mutex);
	LIST_FOREACH(xdev, bus->bus, list) {
		if (match_device(drv->ids, xdev)) {
			xdev->driver = drv;
			xenbus_dev_probe(xdev);
		}
	}
	return 0;
}

int xenbus_register_frontend(struct xenbus_driver *drv)
{
	drv->read_otherend_details = read_backend_details;

	return xenbus_register_driver_common(drv, &xenbus_frontend);
}
EXPORT_SYMBOL(xenbus_register_frontend);


void xenbus_unregister_driver(struct xenbus_driver *drv)
{
#if 0
	driver_unregister(&drv->driver);
#endif
}
EXPORT_SYMBOL(xenbus_unregister_driver);

struct xb_find_info
{
	struct xenbus_device *dev;
	const char *nodename;
};

static struct xenbus_device *
xenbus_device_find(const char *nodename, struct xendev_list_head *bus)
{
	struct xenbus_device *xdev;
	LIST_FOREACH(xdev, bus, list) {
		if (streq(xdev->nodename, nodename)) { 
			return xdev;
#if 0
			get_device(dev);
#endif	
		}
	}
	return NULL;
}
#if 0
static int cleanup_dev(device_t dev, void *data)
{
	struct xenbus_device *xendev = device_get_softc(dev);
	struct xb_find_info *info = data;
	int len = strlen(info->nodename);

	DPRINTK("%s", info->nodename);

	if (!strncmp(xendev->nodename, info->nodename, len)) {
		info->dev = xendev;
#if 0
		get_device(dev);
#endif
		return 1;
	}
	return 0;
}

#endif
static void xenbus_cleanup_devices(const char *path, struct xendev_list_head * bus)
{
#if 0
	struct xb_find_info info = { .nodename = path };

	do {
		info.dev = NULL;
		bus_for_each_dev(bus, NULL, &info, cleanup_dev);
		if (info.dev) {
			device_unregister(&info.dev->dev);
			put_device(&info.dev->dev);
		}
	} while (info.dev);
#endif 
}

#if 0
void xenbus_dev_release(device_t dev)
{
	/* 
	 * nothing to do softc gets freed with the device
	 */
		
}
#endif
/* Simplified asprintf. */
char *kasprintf(const char *fmt, ...)
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

#if 0
static ssize_t xendev_show_nodename(struct device *dev, char *buf)
{
	return sprintf(buf, "%s\n", to_xenbus_device(dev)->nodename);
}
DEVICE_ATTR(nodename, S_IRUSR | S_IRGRP | S_IROTH, xendev_show_nodename, NULL);

static ssize_t xendev_show_devtype(struct device *dev, char *buf)
{
	return sprintf(buf, "%s\n", to_xenbus_device(dev)->devicetype);
}
DEVICE_ATTR(devtype, S_IRUSR | S_IRGRP | S_IROTH, xendev_show_devtype, NULL);
#endif

int xenbus_probe_node(struct xen_bus_type *bus, const char *type,
			     const char *nodename)
{
#define CHECK_FAIL				\
	do {					\
		if (err)			\
			goto fail;		\
	} while (0)				\



	int err;
	struct xenbus_device *xendev;
	struct xenbus_driver *xdrv;
	size_t stringlen;
	char *tmpstring;

	XenbusState state = xenbus_read_driver_state(nodename);

	if (bus->error)
			return (bus->error);
	
	
	if (state != XenbusStateInitialising) {
		/* Device is not new, so ignore it.  This can happen if a
		   device is going away after switching to Closed.  */
		return 0;
	}
	
	stringlen = strlen(nodename) + 1 + strlen(type) + 1;
	xendev = kmalloc(sizeof(*xendev) + stringlen, GFP_KERNEL);
	if (!xendev)
		return -ENOMEM;
	memset(xendev, 0, sizeof(*xendev));
	xendev->state = XenbusStateInitialising;
	
	/* Copy the strings into the extra space. */
	
	tmpstring = (char *)(xendev + 1);
	strcpy(tmpstring, nodename);
	xendev->nodename = tmpstring;
	
	tmpstring += strlen(tmpstring) + 1;
	strcpy(tmpstring, type);
	xendev->devicetype = tmpstring;
	/*
	 * equivalent to device registration 
	 * events
	 */
	LIST_INSERT_HEAD(bus->bus, xendev, list);
	LIST_FOREACH(xdrv, &xendrv_list, list) {
		if (match_device(xdrv->ids, xendev)) {
			xendev->driver = xdrv;
			if (!xenbus_dev_probe(xendev))
				break;
		}
	}

#if 0
	xendev->dev.parent = &bus->dev;
	xendev->dev.bus = &bus->bus;
	xendev->dev.release = xenbus_dev_release;
	
	err = bus->get_bus_id(xendev->dev.bus_id, xendev->nodename);
	CHECK_FAIL;
	
	/* Register with generic device framework. */
	err = device_register(&xendev->dev);
	CHECK_FAIL;
	
	device_create_file(&xendev->dev, &dev_attr_nodename);
	device_create_file(&xendev->dev, &dev_attr_devtype);
#endif
	return 0;
	
#undef CHECK_FAIL
#if 0
 fail:
	xenbus_dev_free(xendev);
#endif
	return err;
}

/* device/<typename>/<name> */
static int xenbus_probe_frontend(const char *type, const char *name)
{
	char *nodename;
	int err;

	nodename = kasprintf("%s/%s/%s", xenbus_frontend.root, type, name);
	if (!nodename)
		return -ENOMEM;

	DPRINTK("%s", nodename);

	err = xenbus_probe_node(&xenbus_frontend, type, nodename);
	kfree(nodename);
	return err;
}

static int xenbus_probe_device_type(struct xen_bus_type *bus, const char *type)
{
	int err = 0;
	char **dir;
	unsigned int dir_n = 0;
	int i;

	dir = xenbus_directory(XBT_NIL, bus->root, type, &dir_n);
	if (IS_ERR(dir))
		return PTR_ERR(dir);

	for (i = 0; i < dir_n; i++) {
		err = bus->probe(type, dir[i]);
		if (err)
			break;
	}
	kfree(dir);
	return err;
}

int xenbus_probe_devices(struct xen_bus_type *bus)
{
	int err = 0;
	char **dir;
	unsigned int i, dir_n;

	dir = xenbus_directory(XBT_NIL, bus->root, "", &dir_n);
	if (IS_ERR(dir))
		return PTR_ERR(dir);

	for (i = 0; i < dir_n; i++) {
		err = xenbus_probe_device_type(bus, dir[i]);
		if (err)
			break;
	}
	kfree(dir);

	return err;
}

static unsigned int char_count(const char *str, char c)
{
	unsigned int i, ret = 0;

	for (i = 0; str[i]; i++)
		if (str[i] == c)
			ret++;
	return ret;
}

static int strsep_len(const char *str, char c, unsigned int len)
{
	unsigned int i;

	for (i = 0; str[i]; i++)
		if (str[i] == c) {
			if (len == 0)
				return i;
			len--;
		}
	return (len == 0) ? i : -ERANGE;
}

void dev_changed(const char *node, struct xen_bus_type *bus)
{
	int exists, rootlen;
	struct xenbus_device *dev;
	char type[BUS_ID_SIZE];
	const char *p;
	char *root;

	DPRINTK("");
	if (char_count(node, '/') < 2)
		return;

	exists = xenbus_exists(XBT_NIL, node, "");
	if (!exists) {
		xenbus_cleanup_devices(node, bus->bus);
		return;
	}

	/* backend/<type>/... or device/<type>/... */
	p = strchr(node, '/') + 1;
	snprintf(type, BUS_ID_SIZE, "%.*s", (int)strcspn(p, "/"), p);
	type[BUS_ID_SIZE-1] = '\0';

	rootlen = strsep_len(node, '/', bus->levels);
	if (rootlen < 0)
		return;
	root = kasprintf("%.*s", rootlen, node);
	if (!root)
		return;

	dev = xenbus_device_find(root, bus->bus);
	if (!dev)
		xenbus_probe_node(bus, type, root);
#if 0
	else
		put_device(&dev->dev);
#endif
	kfree(root);
}

static void frontend_changed(struct xenbus_watch *watch,
			     const char **vec, unsigned int len)
{
	DPRINTK("");

	dev_changed(vec[XS_WATCH_PATH], &xenbus_frontend);
}

/* We watch for devices appearing and vanishing. */
static struct xenbus_watch fe_watch = {
	.node = "device",
	.callback = frontend_changed,
};

#ifdef notyet

static int suspend_dev(device_t dev, void *data)
{
	int err = 0;
	struct xenbus_driver *drv;
	struct xenbus_device *xdev;

	DPRINTK("");

	xdev = device_get_softc(dev);

	drv = xdev->driver;

	if (device_get_driver(dev) == NULL)
		return 0;

	if (drv->suspend)
		err = drv->suspend(xdev);
#if 0
	/* bus_id ? */
	if (err)
			log(LOG_WARNING, "xenbus: suspend %s failed: %i\n", 
		       dev->bus_id, err);
#endif
	return 0;
}



static int resume_dev(device_t dev, void *data)
{
	int err;
	struct xenbus_driver *drv;
	struct xenbus_device *xdev;

	DPRINTK("");

	if (device_get_driver(dev) == NULL)
		return 0;
	xdev = device_get_softc(dev);
	drv = xdev->driver;

	err = talk_to_otherend(xdev);
#if 0
	if (err) {
			log(LOG_WARNING,
			   "xenbus: resume (talk_to_otherend) %s failed: %i\n",
			   dev->bus_id, err);
		return err;
	}
#endif
	if (drv->resume)
		err = drv->resume(xdev);

	err = watch_otherend(xdev);
#if 0
	/* bus_id? */
	if (err)
			log(LOG_WARNING,
			   "xenbus: resume %s failed: %i\n", dev->bus_id, err);
#endif
	return err;
}

#endif
void xenbus_suspend(void)
{
	DPRINTK("");
	panic("implement me");
#if 0
	bus_for_each_dev(&xenbus_frontend.bus, NULL, NULL, suspend_dev);
	bus_for_each_dev(&xenbus_backend.bus, NULL, NULL, suspend_dev);
#endif
	xs_suspend();
}
EXPORT_SYMBOL(xenbus_suspend);

void xenbus_resume(void)
{
	xb_init_comms();
	xs_resume();
	panic("implement me");
#if 0
	bus_for_each_dev(&xenbus_frontend.bus, NULL, NULL, resume_dev);
	bus_for_each_dev(&xenbus_backend.bus, NULL, NULL, resume_dev);
#endif 
}
EXPORT_SYMBOL(xenbus_resume);

#if 0
static device_t 
xenbus_add_child(device_t bus, int order, const char *name, int unit) 
{ 
	device_t child; 

	child = device_add_child_ordered(bus, order, name, unit);  
   
	return(child); 
} 
#endif

/* A flag to determine if xenstored is 'ready' (i.e. has started) */
int xenstored_ready = 0; 


int register_xenstore_notifier(xenstore_event_handler_t func, void *arg, int priority)
{
	int ret = 0;

	if (xenstored_ready > 0) 
		ret = func(NULL);
	else 
		eventhandler_register(xenstore_chain, "xenstore", func, arg, priority);

	return ret;
}
EXPORT_SYMBOL(register_xenstore_notifier);
#if 0
void unregister_xenstore_notifier(struct notifier_block *nb)
{
	notifier_chain_unregister(&xenstore_chain, nb);
}
EXPORT_SYMBOL(unregister_xenstore_notifier);
#endif



#ifdef DOM0
static struct proc_dir_entry *xsd_mfn_intf;
static struct proc_dir_entry *xsd_port_intf;


static int xsd_mfn_read(char *page, char **start, off_t off,
                        int count, int *eof, void *data)
{
	int len; 
	len  = sprintf(page, "%ld", xen_start_info->store_mfn); 
	*eof = 1; 
	return len; 
}

static int xsd_port_read(char *page, char **start, off_t off,
			 int count, int *eof, void *data)
{
	int len; 

	len  = sprintf(page, "%d", xen_start_info->store_evtchn); 
	*eof = 1; 
	return len; 
}

#endif
static int 
xenbus_probe_sysinit(void *unused)
{
	int err = 0, dom0;

	DPRINTK("");

	LIST_INIT(&xenbus_device_frontend_list);
	LIST_INIT(&xenbus_device_backend_list);
	LIST_INIT(&xendrv_list);
#if 0
	if (xen_init() < 0) {
		DPRINTK("failed");
		return -ENODEV;
	}


	/* Register ourselves with the kernel bus & device subsystems */
	bus_register(&xenbus_frontend.bus);
	bus_register(&xenbus_backend.bus);
	device_register(&xenbus_frontend.dev);
	device_register(&xenbus_backend.dev);
#endif

	/*
	** Domain0 doesn't have a store_evtchn or store_mfn yet.
	*/
	dom0 = (xen_start_info->store_evtchn == 0);


#ifdef DOM0
	if (dom0) {

		unsigned long page;
		evtchn_op_t op = { 0 };
		int ret;


		/* Allocate page. */
		page = get_zeroed_page(GFP_KERNEL);
		if (!page) 
			return -ENOMEM; 

		/* We don't refcnt properly, so set reserved on page.
		 * (this allocation is permanent) */
		SetPageReserved(virt_to_page(page));

		xen_start_info->store_mfn =
				pfn_to_mfn(virt_to_phys((void *)page) >>
				   PAGE_SHIFT);
	
		/* Next allocate a local port which xenstored can bind to */
		op.cmd = EVTCHNOP_alloc_unbound;
		op.u.alloc_unbound.dom        = DOMID_SELF;
		op.u.alloc_unbound.remote_dom = 0; 

		ret = HYPERVISOR_event_channel_op(&op);
		BUG_ON(ret); 
		xen_start_info->store_evtchn = op.u.alloc_unbound.port;

		/* And finally publish the above info in /proc/xen */
		if((xsd_mfn_intf = create_xen_proc_entry("xsd_mfn", 0400)))
			xsd_mfn_intf->read_proc = xsd_mfn_read; 
		if((xsd_port_intf = create_xen_proc_entry("xsd_port", 0400)))
			xsd_port_intf->read_proc = xsd_port_read;
	}
#endif
	/* Initialize the interface to xenstore. */
	err = xs_init(); 
	if (err) {
			log(LOG_WARNING,
			   "XENBUS: Error initializing xenstore comms: %i\n", err);
		return err; 
	}

	if (!dom0) {
		xenstored_ready = 1;
#if 0
		xenbus_dev = BUS_ADD_CHILD(parent, 0, "xenbus", 0);
		if (xenbus_dev == NULL)
			panic("xenbus: could not attach"); 
		xenbus_backend_dev = BUS_ADD_CHILD(parent, 0, "xb_be", 0);
		if (xenbus_backend_dev == NULL)
			panic("xenbus: could not attach"); 
#endif
		BUG_ON((xenstored_ready <= 0)); 
	

		
		/* Enumerate devices in xenstore. */
		xenbus_probe_devices(&xenbus_frontend);
		register_xenbus_watch(&fe_watch);
#ifdef notyet
		xenbus_backend_probe_and_watch();
#endif		
		
		/* Notify others that xenstore is up */
		EVENTHANDLER_INVOKE(xenstore_event);
	}

	return 0;
}

SYSINIT(xenbus_probe_sysinit, SI_SUB_PSEUDO, SI_ORDER_FIRST, xenbus_probe_sysinit, NULL);

#if 0
static device_method_t xenbus_methods[] = { 
	/* Device interface */ 
#if 0
	DEVMETHOD(device_identify,      xenbus_identify), 
	DEVMETHOD(device_probe,         xenbus_probe), 
	DEVMETHOD(device_attach,        xenbus_attach), 

	DEVMETHOD(device_detach,        bus_generic_detach), 
	DEVMETHOD(device_shutdown,      bus_generic_shutdown), 
#endif
	DEVMETHOD(device_suspend,       xenbus_suspend), 
	DEVMETHOD(device_resume,        xenbus_resume), 
 
	/* Bus interface */ 
	DEVMETHOD(bus_print_child,      bus_generic_print_child),
	DEVMETHOD(bus_add_child,        xenbus_add_child), 
	DEVMETHOD(bus_read_ivar,        bus_generic_read_ivar), 
	DEVMETHOD(bus_write_ivar,       bus_generic_write_ivar), 
#if 0
	DEVMETHOD(bus_set_resource,     bus_generic_set_resource), 
	DEVMETHOD(bus_get_resource,     bus_generic_get_resource),
#endif 
	DEVMETHOD(bus_alloc_resource,   bus_generic_alloc_resource), 
	DEVMETHOD(bus_release_resource, bus_generic_release_resource), 
#if 0
	DEVMETHOD(bus_delete_resource,  bus_generic_delete_resource),
#endif 
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource), 
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource), 
	DEVMETHOD(bus_setup_intr,       bus_generic_setup_intr), 
	DEVMETHOD(bus_teardown_intr,    bus_generic_teardown_intr), 
 
	{ 0, 0 } 
}; 

static char driver_name[] = "xenbus";
static driver_t xenbus_driver = { 
	driver_name, 
	xenbus_methods, 
	sizeof(struct xenbus_device),                      
}; 
devclass_t xenbus_devclass; 
 
DRIVER_MODULE(xenbus, nexus, xenbus_driver, xenbus_devclass, 0, 0); 

#endif





/*
 * Local variables:
 *  c-file-style: "bsd"
 *  indent-tabs-mode: t
 *  c-indent-level: 4
 *  c-basic-offset: 8
 *  tab-width: 4
 * End:
 */
