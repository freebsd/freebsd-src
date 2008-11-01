/******************************************************************************
 * xenbus.h
 *
 * Talks to Xen Store to figure out what devices we have.
 *
 * Copyright (C) 2005 Rusty Russell, IBM Corporation
 * Copyright (C) 2005 XenSource Ltd.
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
 *
 * $FreeBSD$
 */

#ifndef _ASM_XEN_XENBUS_H
#define _ASM_XEN_XENBUS_H

#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <xen/interface/io/xenbus.h>
#include <xen/interface/io/xs_wire.h>

LIST_HEAD(xendev_list_head, xenbus_device); 

/* Register callback to watch this node. */
struct xenbus_watch
{
	LIST_ENTRY(xenbus_watch) list;

	/* Path being watched. */
	char *node;

	/* Callback (executed in a process context with no locks held). */
	void (*callback)(struct xenbus_watch *,
			 const char **vec, unsigned int len);
};


/* A xenbus device. */
struct xenbus_device {
		struct xenbus_watch otherend_watch; /* must be first */
		const char *devicetype;
		const char *nodename;
		const char *otherend;
		int otherend_id;
		struct xendev_list_head *bus;
		struct xenbus_driver *driver;
		int has_error;
		enum xenbus_state state;
		void *dev_driver_data;
		LIST_ENTRY(xenbus_device) list;
};

static inline struct xenbus_device *to_xenbus_device(device_t dev)
{	
		return device_get_softc(dev);
}

struct xenbus_device_id
{
	/* .../device/<device_type>/<identifier> */
	char devicetype[32]; 	/* General class of device. */
};

/* A xenbus driver. */
struct xenbus_driver {
		char *name;
		struct module *owner;
		const struct xenbus_device_id *ids;
		int (*probe)(struct xenbus_device *dev,
					 const struct xenbus_device_id *id);
		void (*otherend_changed)(struct xenbus_device *dev,
								 XenbusState backend_state);
		int (*remove)(struct xenbus_device *dev);
		int (*suspend)(struct xenbus_device *dev);
		int (*resume)(struct xenbus_device *dev);
		int (*hotplug)(struct xenbus_device *, char **, int, char *, int);
#if 0
		struct device_driver driver;
#endif
		driver_t driver;
		int (*read_otherend_details)(struct xenbus_device *dev);
		int (*watch_otherend)(struct xenbus_device *dev);
		int (*cleanup_device)(struct xenbus_device *dev);
		int (*is_ready)(struct xenbus_device *dev);
		LIST_ENTRY(xenbus_driver) list;
};

static inline struct xenbus_driver *to_xenbus_driver(driver_t *drv)
{
#if 0
	return container_of(drv, struct xenbus_driver, driver);
#endif
	return NULL;
}
typedef int (*xenstore_event_handler_t)(void *);

int xenbus_register_frontend(struct xenbus_driver *drv);
int xenbus_register_backend(struct xenbus_driver *drv);
void xenbus_unregister_driver(struct xenbus_driver *drv);

int xenbus_remove_device(struct xenbus_device *dev);

struct xenbus_transaction
{
		uint32_t id;
};

#define XBT_NIL ((struct xenbus_transaction) { 0 })

char **xenbus_directory(struct xenbus_transaction t,
			const char *dir, const char *node, unsigned int *num);
void *xenbus_read(struct xenbus_transaction t,
		  const char *dir, const char *node, unsigned int *len);
int xenbus_write(struct xenbus_transaction t,
		 const char *dir, const char *node, const char *string);
int xenbus_mkdir(struct xenbus_transaction t,
		 const char *dir, const char *node);
int xenbus_exists(struct xenbus_transaction t,
		  const char *dir, const char *node);
int xenbus_rm(struct xenbus_transaction t, const char *dir, const char *node);
int xenbus_transaction_start(struct xenbus_transaction *t);
int xenbus_transaction_end(struct xenbus_transaction t, int abort);

/* Single read and scanf: returns -errno or num scanned if > 0. */
int xenbus_scanf(struct xenbus_transaction t,
		 const char *dir, const char *node, const char *fmt, ...)
	__attribute__((format(scanf, 4, 5)));

/* Single printf and write: returns -errno or 0. */
int xenbus_printf(struct xenbus_transaction t,
		  const char *dir, const char *node, const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

/* Generic read function: NULL-terminated triples of name,
 * sprintf-style type string, and pointer. Returns 0 or errno.*/
int xenbus_gather(struct xenbus_transaction t, const char *dir, ...);

/* notifer routines for when the xenstore comes up */
int register_xenstore_notifier(xenstore_event_handler_t func, void *arg, int priority);
#if 0
void unregister_xenstore_notifier();
#endif
int register_xenbus_watch(struct xenbus_watch *watch);
void unregister_xenbus_watch(struct xenbus_watch *watch);
void xs_suspend(void);
void xs_resume(void);

/* Used by xenbus_dev to borrow kernel's store connection. */
void *xenbus_dev_request_and_reply(struct xsd_sockmsg *msg);

/* Called from xen core code. */
void xenbus_suspend(void);
void xenbus_resume(void);

#define XENBUS_IS_ERR_READ(str) ({			\
	if (!IS_ERR(str) && strlen(str) == 0) {		\
		free(str, M_DEVBUF);				\
		str = ERR_PTR(-ERANGE);			\
	}						\
	IS_ERR(str);					\
})

#define XENBUS_EXIST_ERR(err) ((err) == -ENOENT || (err) == -ERANGE)


/**
 * Register a watch on the given path, using the given xenbus_watch structure
 * for storage, and the given callback function as the callback.  Return 0 on
 * success, or -errno on error.  On success, the given path will be saved as
 * watch->node, and remains the caller's to free.  On error, watch->node will
 * be NULL, the device will switch to XenbusStateClosing, and the error will
 * be saved in the store.
 */
int xenbus_watch_path(struct xenbus_device *dev, char *path,
		      struct xenbus_watch *watch, 
		      void (*callback)(struct xenbus_watch *,
				       const char **, unsigned int));


/**
 * Register a watch on the given path/path2, using the given xenbus_watch
 * structure for storage, and the given callback function as the callback.
 * Return 0 on success, or -errno on error.  On success, the watched path
 * (path/path2) will be saved as watch->node, and becomes the caller's to
 * kfree().  On error, watch->node will be NULL, so the caller has nothing to
 * free, the device will switch to XenbusStateClosing, and the error will be
 * saved in the store.
 */
int xenbus_watch_path2(struct xenbus_device *dev, const char *path,
		       const char *path2, struct xenbus_watch *watch, 
		       void (*callback)(struct xenbus_watch *,
					const char **, unsigned int));


/**
 * Advertise in the store a change of the given driver to the given new_state.
 * which case this is performed inside its own transaction.  Return 0 on
 * success, or -errno on error.  On error, the device will switch to
 * XenbusStateClosing, and the error will be saved in the store.
 */
int xenbus_switch_state(struct xenbus_device *dev,
			XenbusState new_state);


/**
 * Grant access to the given ring_mfn to the peer of the given device.  Return
 * 0 on success, or -errno on error.  On error, the device will switch to
 * XenbusStateClosing, and the error will be saved in the store.
 */
int xenbus_grant_ring(struct xenbus_device *dev, unsigned long ring_mfn);


/**
 * Allocate an event channel for the given xenbus_device, assigning the newly
 * created local port to *port.  Return 0 on success, or -errno on error.  On
 * error, the device will switch to XenbusStateClosing, and the error will be
 * saved in the store.
 */
int xenbus_alloc_evtchn(struct xenbus_device *dev, int *port);


/**
 * Free an existing event channel. Returns 0 on success or -errno on error.
 */
int xenbus_free_evtchn(struct xenbus_device *dev, int port);


/**
 * Return the state of the driver rooted at the given store path, or
 * XenbusStateClosed if no state can be read.
 */
XenbusState xenbus_read_driver_state(const char *path);


/***
 * Report the given negative errno into the store, along with the given
 * formatted message.
 */
void xenbus_dev_error(struct xenbus_device *dev, int err, const char *fmt,
		      ...);


/***
 * Equivalent to xenbus_dev_error(dev, err, fmt, args), followed by
 * xenbus_switch_state(dev, NULL, XenbusStateClosing) to schedule an orderly
 * closedown of this driver and its peer.
 */
void xenbus_dev_fatal(struct xenbus_device *dev, int err, const char *fmt,
		      ...);

int xenbus_dev_init(void);

const char *xenbus_strstate(enum xenbus_state state);
int xenbus_dev_is_online(struct xenbus_device *dev);
int xenbus_frontend_closed(struct xenbus_device *dev);

#endif /* _ASM_XEN_XENBUS_H */

/*
 * Local variables:
 *  c-file-style: "bsd"
 *  indent-tabs-mode: t
 *  c-indent-level: 4
 *  c-basic-offset: 8
 *  tab-width: 4
 * End:
 */
