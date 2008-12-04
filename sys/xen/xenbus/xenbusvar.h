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

#ifndef _XEN_XENBUS_XENBUSVAR_H
#define _XEN_XENBUS_XENBUSVAR_H

#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <machine/xen/xen-os.h>
#include <xen/interface/io/xenbus.h>
#include <xen/interface/io/xs_wire.h>

#include "xenbus_if.h"

enum {
	/*
	 * Path of this device node.
	 */
	XENBUS_IVAR_NODE,

	/*
	 * The device type (e.g. vif, vbd).
	 */
	XENBUS_IVAR_TYPE,

	/*
	 * The state of this device (not the otherend's state).
	 */
	XENBUS_IVAR_STATE,

	/*
	 * Domain ID of the other end device.
	 */
	XENBUS_IVAR_OTHEREND_ID,

	/*
	 * Path of the other end device.
	 */
	XENBUS_IVAR_OTHEREND_PATH
};

/*
 * Simplified accessors for xenbus devices
 */
#define	XENBUS_ACCESSOR(var, ivar, type) \
	__BUS_ACCESSOR(xenbus, var, XENBUS, ivar, type)

XENBUS_ACCESSOR(node,		NODE,			const char *)
XENBUS_ACCESSOR(type,		TYPE,			const char *)
XENBUS_ACCESSOR(state,		STATE,			enum xenbus_state)
XENBUS_ACCESSOR(otherend_id,	OTHEREND_ID,		int)
XENBUS_ACCESSOR(otherend_path,	OTHEREND_PATH,		const char *)

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

typedef int (*xenstore_event_handler_t)(void *);

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
int xenbus_watch_path(device_t dev, char *path,
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
int xenbus_watch_path2(device_t dev, const char *path,
		       const char *path2, struct xenbus_watch *watch, 
		       void (*callback)(struct xenbus_watch *,
					const char **, unsigned int));


/**
 * Advertise in the store a change of the given driver to the given new_state.
 * which case this is performed inside its own transaction.  Return 0 on
 * success, or -errno on error.  On error, the device will switch to
 * XenbusStateClosing, and the error will be saved in the store.
 */
int xenbus_switch_state(device_t dev,
			XenbusState new_state);


/**
 * Grant access to the given ring_mfn to the peer of the given device.  Return
 * 0 on success, or -errno on error.  On error, the device will switch to
 * XenbusStateClosing, and the error will be saved in the store.
 */
int xenbus_grant_ring(device_t dev, unsigned long ring_mfn);


/**
 * Allocate an event channel for the given xenbus_device, assigning the newly
 * created local port to *port.  Return 0 on success, or -errno on error.  On
 * error, the device will switch to XenbusStateClosing, and the error will be
 * saved in the store.
 */
int xenbus_alloc_evtchn(device_t dev, int *port);


/**
 * Free an existing event channel. Returns 0 on success or -errno on error.
 */
int xenbus_free_evtchn(device_t dev, int port);


/**
 * Return the state of the driver rooted at the given store path, or
 * XenbusStateClosed if no state can be read.
 */
XenbusState xenbus_read_driver_state(const char *path);


/***
 * Report the given negative errno into the store, along with the given
 * formatted message.
 */
void xenbus_dev_error(device_t dev, int err, const char *fmt,
		      ...);


/***
 * Equivalent to xenbus_dev_error(dev, err, fmt, args), followed by
 * xenbus_switch_state(dev, NULL, XenbusStateClosing) to schedule an orderly
 * closedown of this driver and its peer.
 */
void xenbus_dev_fatal(device_t dev, int err, const char *fmt,
		      ...);

int xenbus_dev_init(void);

const char *xenbus_strstate(enum xenbus_state state);
int xenbus_dev_is_online(device_t dev);
int xenbus_frontend_closed(device_t dev);

#endif /* _XEN_XENBUS_XENBUSVAR_H */
