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

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/kernel.h>



#include <machine/xen/xen-os.h>
#include <machine/xen/hypervisor.h>
#include <machine/xen/evtchn.h>
#include <machine/xen/xenbus.h>
#include <machine/xen/xen_intr.h>
#include <xen/xenbus/xenbus_comms.h>

static int xenbus_irq;

extern void xenbus_probe(void *); 
extern int xenstored_ready; 
#if 0
static DECLARE_WORK(probe_work, xenbus_probe, NULL);
#endif
int xb_wait;
extern char *xen_store;
#define wake_up wakeup
#define xb_waitq xb_wait
#define pr_debug(a,b,c)

static inline struct xenstore_domain_interface *xenstore_domain_interface(void)
{
		return (struct xenstore_domain_interface *)xen_store;
}

static void
wake_waiting(void * arg __attribute__((unused)))
{
#if 0
		if (unlikely(xenstored_ready == 0)) {
				xenstored_ready = 1; 
				schedule_work(&probe_work); 
		} 
#endif
		wakeup(&xb_wait);
}

static int check_indexes(XENSTORE_RING_IDX cons, XENSTORE_RING_IDX prod)
{
		return ((prod - cons) <= XENSTORE_RING_SIZE);
}

static void *get_output_chunk(XENSTORE_RING_IDX cons,
							  XENSTORE_RING_IDX prod,
							  char *buf, uint32_t *len)
{
		*len = XENSTORE_RING_SIZE - MASK_XENSTORE_IDX(prod);
		if ((XENSTORE_RING_SIZE - (prod - cons)) < *len)
				*len = XENSTORE_RING_SIZE - (prod - cons);
		return buf + MASK_XENSTORE_IDX(prod);
}

static const void *get_input_chunk(XENSTORE_RING_IDX cons,
								   XENSTORE_RING_IDX prod,
								   const char *buf, uint32_t *len)
{
		*len = XENSTORE_RING_SIZE - MASK_XENSTORE_IDX(cons);
		if ((prod - cons) < *len)
				*len = prod - cons;
		return buf + MASK_XENSTORE_IDX(cons);
}

int xb_write(const void *tdata, unsigned len)
{
		struct xenstore_domain_interface *intf = xenstore_domain_interface();
		XENSTORE_RING_IDX cons, prod;
		const char *data = (const char *)tdata;

		while (len != 0) {
				void *dst;
				unsigned int avail;

				wait_event_interruptible(&xb_waitq,
										 (intf->req_prod - intf->req_cons) !=
										 XENSTORE_RING_SIZE);

				/* Read indexes, then verify. */
				cons = intf->req_cons;
				prod = intf->req_prod;
				mb();
				if (!check_indexes(cons, prod)) {
						intf->req_cons = intf->req_prod = 0;
 						return -EIO;
				}

				dst = get_output_chunk(cons, prod, intf->req, &avail);
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
				notify_remote_via_evtchn(xen_start_info->store_evtchn);
		}

		return 0;
}

#ifdef notyet
int xb_data_to_read(void)
{
	struct xenstore_domain_interface *intf = xen_store_interface;
	return (intf->rsp_cons != intf->rsp_prod);
}

int xb_wait_for_data_to_read(void)
{
	return wait_event_interruptible(xb_waitq, xb_data_to_read());
}
#endif


int xb_read(void *tdata, unsigned len)
{
		struct xenstore_domain_interface *intf = xenstore_domain_interface();
		XENSTORE_RING_IDX cons, prod;
		char *data = (char *)tdata;

		while (len != 0) {
				unsigned int avail;
				const char *src;

				wait_event_interruptible(&xb_waitq,
										 intf->rsp_cons != intf->rsp_prod);

				/* Read indexes, then verify. */
				cons = intf->rsp_cons;
				prod = intf->rsp_prod;
				if (!check_indexes(cons, prod)) {
						intf->rsp_cons = intf->rsp_prod = 0;
						return -EIO;
				}
				
				src = get_input_chunk(cons, prod, intf->rsp, &avail);
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

				pr_debug("Finished read of %i bytes (%i to go)\n", avail, len);

				/* Implies mb(): they will see new header. */
				notify_remote_via_evtchn(xen_start_info->store_evtchn);
		}

		return 0;
}

/* Set up interrupt handler off store event channel. */
int xb_init_comms(void)
{
		struct xenstore_domain_interface *intf = xenstore_domain_interface();
		int err;

		if (intf->rsp_prod != intf->rsp_cons) {
				log(LOG_WARNING, "XENBUS response ring is not quiescent "
					   "(%08x:%08x): fixing up\n",
					   intf->rsp_cons, intf->rsp_prod);
				intf->rsp_cons = intf->rsp_prod;
		}

		if (xenbus_irq)
				unbind_from_irqhandler(xenbus_irq, &xb_waitq);

		err = bind_caller_port_to_irqhandler(
				xen_start_info->store_evtchn,
				"xenbus", wake_waiting, NULL, INTR_TYPE_NET, NULL);
		if (err <= 0) {
				log(LOG_WARNING, "XENBUS request irq failed %i\n", err);
				return err;
		}

		xenbus_irq = err;

		return 0;
}

/*
 * Local variables:
 *  c-file-style: "bsd"
 *  indent-tabs-mode: t
 *  c-indent-level: 4
 *  c-basic-offset: 8
 *  tab-width: 4
 * End:
 */
