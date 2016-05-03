/*
 * xenstore_dev.c
 * 
 * Driver giving user-space access to the kernel's connection to the
 * XenStore service.
 * 
 * Copyright (c) 2005, Christian Limpach
 * Copyright (c) 2005, Rusty Russell, IBM Corporation
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


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/module.h>

#include <xen/xen-os.h>

#include <xen/hypervisor.h>
#include <xen/xenstore/xenstorevar.h>
#include <xen/xenstore/xenstore_internal.h>

struct xs_dev_transaction {
	LIST_ENTRY(xs_dev_transaction) list;
	struct xs_transaction handle;
};

struct xs_dev_data {
	/* In-progress transaction. */
	LIST_HEAD(xdd_list_head, xs_dev_transaction) transactions;

	/* Partial request. */
	unsigned int len;
	union {
		struct xsd_sockmsg msg;
		char buffer[PAGE_SIZE];
	} u;

	/* Response queue. */
#define MASK_READ_IDX(idx) ((idx)&(PAGE_SIZE-1))
	char read_buffer[PAGE_SIZE];
	unsigned int read_cons, read_prod;
};

static int 
xs_dev_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	int error;
	struct xs_dev_data *u;

	error = devfs_get_cdevpriv((void **)&u);
	if (error != 0)
		return (error);

	while (u->read_prod == u->read_cons) {
		error = tsleep(u, PCATCH, "xsdread", hz/10);
		if (error && error != EWOULDBLOCK)
			return (error);
	}

	while (uio->uio_resid > 0) {
		if (u->read_cons == u->read_prod)
			break;
		error = uiomove(&u->read_buffer[MASK_READ_IDX(u->read_cons)],
		    1, uio);
		if (error)
			return (error);
		u->read_cons++;
	}
	return (0);
}

static void
xs_queue_reply(struct xs_dev_data *u, char *data, unsigned int len)
{
	int i;

	for (i = 0; i < len; i++, u->read_prod++)
		u->read_buffer[MASK_READ_IDX(u->read_prod)] = data[i];

	KASSERT((u->read_prod - u->read_cons) <= sizeof(u->read_buffer),
	    ("xenstore reply too big"));

	wakeup(u);
}

static int 
xs_dev_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	int error;
	struct xs_dev_data *u;
	struct xs_dev_transaction *trans;
	void *reply;
	int len = uio->uio_resid;

	error = devfs_get_cdevpriv((void **)&u);
	if (error != 0)
		return (error);

	if ((len + u->len) > sizeof(u->u.buffer))
		return (EINVAL);

	error = uiomove(u->u.buffer + u->len, len, uio);
	if (error)
		return (error);

	u->len += len;
	if (u->len < (sizeof(u->u.msg) + u->u.msg.len))
		return (0);

	switch (u->u.msg.type) {
	case XS_TRANSACTION_START:
	case XS_TRANSACTION_END:
	case XS_DIRECTORY:
	case XS_READ:
	case XS_GET_PERMS:
	case XS_RELEASE:
	case XS_GET_DOMAIN_PATH:
	case XS_WRITE:
	case XS_MKDIR:
	case XS_RM:
	case XS_SET_PERMS:
		error = xs_dev_request_and_reply(&u->u.msg, &reply);
		if (!error) {
			if (u->u.msg.type == XS_TRANSACTION_START) {
				trans = malloc(sizeof(*trans), M_XENSTORE,
				    M_WAITOK);
				trans->handle.id = strtoul(reply, NULL, 0);
				LIST_INSERT_HEAD(&u->transactions, trans, list);
			} else if (u->u.msg.type == XS_TRANSACTION_END) {
				LIST_FOREACH(trans, &u->transactions, list)
					if (trans->handle.id == u->u.msg.tx_id)
						break;
#if 0 /* XXX does this mean the list is empty? */
				BUG_ON(&trans->list == &u->transactions);
#endif
				LIST_REMOVE(trans, list);
				free(trans, M_XENSTORE);
			}
			xs_queue_reply(u, (char *)&u->u.msg, sizeof(u->u.msg));
			xs_queue_reply(u, (char *)reply, u->u.msg.len);
			free(reply, M_XENSTORE);
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	if (error == 0)
		u->len = 0;

	return (error);
}

static void
xs_dev_dtor(void *arg)
{
	struct xs_dev_data *u = arg;
	struct xs_dev_transaction *trans, *tmp;

	LIST_FOREACH_SAFE(trans, &u->transactions, list, tmp) {
		xs_transaction_end(trans->handle, 1);
		LIST_REMOVE(trans, list);
		free(trans, M_XENSTORE);
	}

	free(u, M_XENSTORE);
}

static int
xs_dev_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct xs_dev_data *u;
	int error;

	u = malloc(sizeof(*u), M_XENSTORE, M_WAITOK|M_ZERO);
	LIST_INIT(&u->transactions);
	error = devfs_set_cdevpriv(u, xs_dev_dtor);
	if (error != 0)
		free(u, M_XENSTORE);

	return (error);
}

static struct cdevsw xs_dev_cdevsw = {
	.d_version = D_VERSION,	
	.d_read = xs_dev_read,
	.d_write = xs_dev_write,
	.d_open = xs_dev_open,
	.d_name = "xs_dev",
};

/*------------------ Private Device Attachment Functions  --------------------*/
/**
 * \brief Identify instances of this device type in the system.
 *
 * \param driver  The driver performing this identify action.
 * \param parent  The NewBus parent device for any devices this method adds.
 */
static void
xs_dev_identify(driver_t *driver __unused, device_t parent)
{
	/*
	 * A single device instance for our driver is always present
	 * in a system operating under Xen.
	 */
	BUS_ADD_CHILD(parent, 0, driver->name, 0);
}

/**
 * \brief Probe for the existence of the Xenstore device
 *
 * \param dev  NewBus device_t for this instance.
 *
 * \return  Always returns 0 indicating success.
 */
static int 
xs_dev_probe(device_t dev)
{

	device_set_desc(dev, "Xenstore user-space device");
	return (0);
}

/**
 * \brief Attach the Xenstore device.
 *
 * \param dev  NewBus device_t for this instance.
 *
 * \return  On success, 0. Otherwise an errno value indicating the
 *          type of failure.
 */
static int
xs_dev_attach(device_t dev)
{
	struct cdev *xs_cdev;

	xs_cdev = make_dev_credf(MAKEDEV_ETERNAL, &xs_dev_cdevsw, 0, NULL,
	    UID_ROOT, GID_WHEEL, 0400, "xen/xenstore");
	if (xs_cdev == NULL)
		return (EINVAL);

	return (0);
}

/*-------------------- Private Device Attachment Data  -----------------------*/
static device_method_t xs_dev_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	xs_dev_identify),
	DEVMETHOD(device_probe,         xs_dev_probe),
	DEVMETHOD(device_attach,        xs_dev_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(xs_dev, xs_dev_driver, xs_dev_methods, 0);
devclass_t xs_dev_devclass;

DRIVER_MODULE(xs_dev, xenstore, xs_dev_driver, xs_dev_devclass,
    NULL, NULL);
