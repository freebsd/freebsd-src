/*
 * Copyright (c) 2015 Roger Pau Monné <roger.pau@citrix.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_stack.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/stack.h>
#include <sys/sbuf.h>

#include <xen/xen-os.h>
#include <xen/xen_intr.h>
#include <xen/hypervisor.h>

/*
 * Xen debug device
 *
 * Handles the VIRQ_DEBUG interrupt and prints the backtrace of each
 * vCPU on the Xen console.
 */

DPCPU_DEFINE(xen_intr_handle_t, xendebug_handler);
static struct mtx lock;
static struct sbuf *buf;

static int
xendebug_drain(void *arg, const char *str, int len)
{
	/*
	 * Use xen_emergency_print() instead of xc_printf() to avoid the
	 * overhead of parsing a format string when it's not needed.
	 */
	xen_emergency_print(str, len);
	return (len);
}

extern void
stack_capture(struct stack *st, register_t rbp);

static int
xendebug_filter(void *arg __unused)
{
#if defined(STACK) && defined(DDB)
	struct stack st;

	stack_save(&st);

	mtx_lock_spin(&lock);
	xc_printf("Printing stack trace vCPU%u\n", XEN_VCPUID());
	stack_sbuf_print_ddb(buf, &st);
	sbuf_drain(buf);
	mtx_unlock_spin(&lock);
#endif

	return (FILTER_HANDLED);
}

static void
xendebug_identify(driver_t *driver, device_t parent)
{

	KASSERT(xen_domain(),
	    ("Trying to add Xen debug device to non-xen guest"));

	if (!xen_has_percpu_evtchn())
		return;

	if (BUS_ADD_CHILD(parent, 0, "debug", 0) == NULL)
		panic("Unable to add Xen debug device.");
}

static int
xendebug_probe(device_t dev)
{

	device_set_desc(dev, "Xen debug handler");
	return (BUS_PROBE_NOWILDCARD);
}

static int
xendebug_attach(device_t dev)
{
	int i, error;

	mtx_init(&lock, "xen-dbg", NULL, MTX_SPIN);
	buf = sbuf_new(NULL, NULL, 1024, SBUF_FIXEDLEN);
	if (buf == NULL)
		panic("Unable to create sbuf for stack dump");
	sbuf_set_drain(buf, xendebug_drain, NULL);

	/* Bind an event channel to a VIRQ on each VCPU. */
	CPU_FOREACH(i) {
		error = xen_intr_bind_virq(dev, VIRQ_DEBUG, i, xendebug_filter,
		    NULL, NULL, INTR_TYPE_TTY,
		    DPCPU_ID_PTR(i, xendebug_handler));
		if (error != 0) {
			printf("Failed to bind VIRQ_DEBUG to vCPU %d: %d",
			    i, error);
			continue;
		}
		xen_intr_describe(DPCPU_ID_GET(i, xendebug_handler), "d%d", i);
	}

	return (0);
}

static device_method_t xendebug_methods[] = {
	DEVMETHOD(device_identify, xendebug_identify),
	DEVMETHOD(device_probe, xendebug_probe),
	DEVMETHOD(device_attach, xendebug_attach),

	DEVMETHOD_END
};

static driver_t xendebug_driver = {
	"debug",
	xendebug_methods,
	0,
};

DRIVER_MODULE(xendebug, xenpv, xendebug_driver, 0, 0);
MODULE_DEPEND(xendebug, xenpv, 1, 1, 1);
