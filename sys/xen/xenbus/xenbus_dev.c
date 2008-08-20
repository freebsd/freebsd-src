/*
 * xenbus_dev.c
 * 
 * Driver giving user-space access to the kernel's xenbus connection
 * to xenstore.
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


#include <machine/xen/xen-os.h>
#include <machine/xen/hypervisor.h>
#include <machine/xen/xenbus.h>
#include <machine/xen/hypervisor.h>
#include <xen/xenbus/xenbus_comms.h>




#define kmalloc(size, unused) malloc(size, M_DEVBUF, M_WAITOK)
#define BUG_ON        PANIC_IF
#define semaphore     sema
#define rw_semaphore  sema
#define DEFINE_SPINLOCK(lock) struct mtx lock
#define DECLARE_MUTEX(lock) struct sema lock
#define u32           uint32_t
#define simple_strtoul strtoul

struct xenbus_dev_transaction {
	LIST_ENTRY(xenbus_dev_transaction) list;
	struct xenbus_transaction handle;
};

struct xenbus_dev_data {
	/* In-progress transaction. */
	LIST_HEAD(xdd_list_head, xenbus_dev_transaction) transactions;

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
	int read_waitq;
};
#if 0
static struct proc_dir_entry *xenbus_dev_intf;
#endif
static int 
xenbus_dev_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	int i = 0;
	struct xenbus_dev_data *u = dev->si_drv1;

	if (wait_event_interruptible(&u->read_waitq,
				     u->read_prod != u->read_cons))
		return EINTR;

	for (i = 0; i < uio->uio_iov[0].iov_len; i++) {
		if (u->read_cons == u->read_prod)
			break;
		copyout(&u->read_buffer[MASK_READ_IDX(u->read_cons)], (char *)uio->uio_iov[0].iov_base+i, 1);
		u->read_cons++;
		uio->uio_resid--;
	}
	return 0;
}

static void queue_reply(struct xenbus_dev_data *u,
			char *data, unsigned int len)
{
	int i;

	for (i = 0; i < len; i++, u->read_prod++)
		u->read_buffer[MASK_READ_IDX(u->read_prod)] = data[i];

	BUG_ON((u->read_prod - u->read_cons) > sizeof(u->read_buffer));

	wakeup(&u->read_waitq);
}

static int 
xenbus_dev_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	int err = 0;
	struct xenbus_dev_data *u = dev->si_drv1;
	struct xenbus_dev_transaction *trans;
	void *reply;
	int len = uio->uio_iov[0].iov_len;

	if ((len + u->len) > sizeof(u->u.buffer))
		return EINVAL;

	if (copyin(u->u.buffer + u->len, uio->uio_iov[0].iov_base, len) != 0)
		return EFAULT;

	u->len += len;
	if (u->len < (sizeof(u->u.msg) + u->u.msg.len))
		return len;

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
		reply = xenbus_dev_request_and_reply(&u->u.msg);
		if (IS_ERR(reply)) {
			err = PTR_ERR(reply);
		} else {
			if (u->u.msg.type == XS_TRANSACTION_START) {
				trans = kmalloc(sizeof(*trans), GFP_KERNEL);
				trans->handle.id = simple_strtoul(reply, NULL, 0);
				LIST_INSERT_HEAD(&u->transactions, trans, list);
			} else if (u->u.msg.type == XS_TRANSACTION_END) {
				LIST_FOREACH(trans, &u->transactions,
					     list)
					if (trans->handle.id ==
					    u->u.msg.tx_id)
						break;
#if 0 /* XXX does this mean the list is empty? */
				BUG_ON(&trans->list == &u->transactions);
#endif
				LIST_REMOVE(trans, list);
				kfree(trans);
			}
			queue_reply(u, (char *)&u->u.msg, sizeof(u->u.msg));
			queue_reply(u, (char *)reply, u->u.msg.len);
			kfree(reply);
		}
		break;

	default:
		err = EINVAL;
		break;
	}

	if (err == 0) {
		u->len = 0;
		err = len;
	}

	return err;
}

static int xenbus_dev_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct xenbus_dev_data *u;

	if (xen_start_info->store_evtchn == 0)
		return ENOENT;
#if 0 /* XXX figure out if equiv needed */
	nonseekable_open(inode, filp);
#endif
	u = kmalloc(sizeof(*u), GFP_KERNEL);
	if (u == NULL)
		return ENOMEM;

	memset(u, 0, sizeof(*u));
	LIST_INIT(&u->transactions);

        dev->si_drv1 = u;

	return 0;
}

static int xenbus_dev_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct xenbus_dev_data *u = dev->si_drv1;
	struct xenbus_dev_transaction *trans, *tmp;

	LIST_FOREACH_SAFE(trans, &u->transactions, list, tmp) {
		xenbus_transaction_end(trans->handle, 1);
		LIST_REMOVE(trans, list);
		kfree(trans);
	}

	kfree(u);
	return 0;
}

static struct cdevsw xenbus_dev_cdevsw = {
	.d_version = D_VERSION,	
	.d_read = xenbus_dev_read,
	.d_write = xenbus_dev_write,
	.d_open = xenbus_dev_open,
	.d_close = xenbus_dev_close,
	.d_name = "xenbus_dev",
};

static int
xenbus_dev_sysinit(void)
{
	make_dev(&xenbus_dev_cdevsw, 0, UID_ROOT, GID_WHEEL, 0400, "xenbus");

	return 0;
}
SYSINIT(xenbus_dev_sysinit, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, xenbus_dev_sysinit, NULL);
/* SYSINIT NEEDED XXX */



/*
 * Local variables:
 *  c-file-style: "linux"
 *  indent-tabs-mode: t
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
