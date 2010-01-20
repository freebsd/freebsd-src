/******************************************************************************
 * xenbus_comms.c
 *
 * Low level code to talks to Xen Store: ringbuffer and event channel.
 *
 * Copyright (C) 2005 Rusty Russell, IBM Corporation
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>

#include <machine/xen/xen-os.h>
#include <xen/hypervisor.h>

#include <xen/xen_intr.h>
#include <xen/evtchn.h>
#include <xen/interface/io/xs_wire.h>
#include <xen/xenbus/xenbus_comms.h>

static unsigned int xenstore_irq;

static inline struct xenstore_domain_interface *
xenstore_domain_interface(void)
{

	return (struct xenstore_domain_interface *)xen_store;
}

static void
xb_intr(void * arg __attribute__((unused)))
{

	wakeup(xen_store);
}

static int
xb_check_indexes(XENSTORE_RING_IDX cons, XENSTORE_RING_IDX prod)
{

	return ((prod - cons) <= XENSTORE_RING_SIZE);
}

static void *
xb_get_output_chunk(XENSTORE_RING_IDX cons, XENSTORE_RING_IDX prod,
    char *buf, uint32_t *len)
{

	*len = XENSTORE_RING_SIZE - MASK_XENSTORE_IDX(prod);
	if ((XENSTORE_RING_SIZE - (prod - cons)) < *len)
		*len = XENSTORE_RING_SIZE - (prod - cons);
	return (buf + MASK_XENSTORE_IDX(prod));
}

static const void *
xb_get_input_chunk(XENSTORE_RING_IDX cons, XENSTORE_RING_IDX prod,
    const char *buf, uint32_t *len)
{

	*len = XENSTORE_RING_SIZE - MASK_XENSTORE_IDX(cons);
	if ((prod - cons) < *len)
		*len = prod - cons;
	return (buf + MASK_XENSTORE_IDX(cons));
}

int
xb_write(const void *tdata, unsigned len, struct lock_object *lock)
{
	struct xenstore_domain_interface *intf = xenstore_domain_interface();
	XENSTORE_RING_IDX cons, prod;
	const char *data = (const char *)tdata;
	int error;

	while (len != 0) {
		void *dst;
		unsigned int avail;

		while ((intf->req_prod - intf->req_cons)
		    == XENSTORE_RING_SIZE) {
			error = _sleep(intf,
			    lock,
			    PCATCH, "xbwrite", hz/10);
			if (error && error != EWOULDBLOCK)
				return (error);
		}

		/* Read indexes, then verify. */
		cons = intf->req_cons;
		prod = intf->req_prod;
		mb();
		if (!xb_check_indexes(cons, prod)) {
			intf->req_cons = intf->req_prod = 0;
			return (EIO);
		}

		dst = xb_get_output_chunk(cons, prod, intf->req, &avail);
		if (avail == 0)
			continue;
		if (avail > len)
			avail = len;
		mb();
				
		memcpy(dst, data, avail);
		data += avail;
		len -= avail;

		/* Other side must not see new header until data is there. */
		wmb();
		intf->req_prod += avail;

		/* This implies mb() before other side sees interrupt. */
		notify_remote_via_evtchn(xen_store_evtchn);
	}

	return (0);
}

int
xb_read(void *tdata, unsigned len, struct lock_object *lock)
{
	struct xenstore_domain_interface *intf = xenstore_domain_interface();
	XENSTORE_RING_IDX cons, prod;
	char *data = (char *)tdata;
	int error;

	while (len != 0) {
		unsigned int avail;
		const char *src;

		while (intf->rsp_cons == intf->rsp_prod) {
			error = _sleep(intf, lock,
			    PCATCH, "xbread", hz/10);
			if (error && error != EWOULDBLOCK)
				return (error);
		}
			
		/* Read indexes, then verify. */
		cons = intf->rsp_cons;
		prod = intf->rsp_prod;
		if (!xb_check_indexes(cons, prod)) {
			intf->rsp_cons = intf->rsp_prod = 0;
			return (EIO);
		}
				
		src = xb_get_input_chunk(cons, prod, intf->rsp, &avail);
		if (avail == 0)
			continue;
		if (avail > len)
			avail = len;

		/* We must read header before we read data. */
		rmb();

		memcpy(data, src, avail);
		data += avail;
		len -= avail;

		/* Other side must not see free space until we've copied out */
		mb();
		intf->rsp_cons += avail;

		/* Implies mb(): they will see new header. */
		notify_remote_via_evtchn(xen_store_evtchn);
	}

	return (0);
}

/* Set up interrupt handler off store event channel. */
int
xb_init_comms(void)
{
	struct xenstore_domain_interface *intf = xenstore_domain_interface();
	int error;

	if (intf->rsp_prod != intf->rsp_cons) {
		log(LOG_WARNING, "XENBUS response ring is not quiescent "
		    "(%08x:%08x): fixing up\n",
		    intf->rsp_cons, intf->rsp_prod);
		intf->rsp_cons = intf->rsp_prod;
	}

	if (xenstore_irq)
		unbind_from_irqhandler(xenstore_irq);

	error = bind_caller_port_to_irqhandler(
		xen_store_evtchn, "xenbus",
		    xb_intr, NULL, INTR_TYPE_NET, &xenstore_irq);
	if (error) {
		log(LOG_WARNING, "XENBUS request irq failed %i\n", error);
		return (error);
	}

	return (0);
}
