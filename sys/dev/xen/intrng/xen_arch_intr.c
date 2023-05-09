/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2014,2015 Julien Grall
 * Copyright © 2021,2022 Elliott Mitchell
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/intr.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/pcpu.h>
#include <sys/rman.h>
#include <sys/smp.h>
#include <sys/syslog.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/fdt_common.h>

#include <xen/xen-os.h>
#include <xen/xen_intr.h>
#include <xen/hypervisor.h>
#include <contrib/xen/vcpu.h>
#include <xen/features.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/xen/arch-intr.h>

DPCPU_DEFINE(struct vcpu_info, vcpu_local_info);
DPCPU_DEFINE(struct vcpu_info *, vcpu_info);

int
xen_dt_probe(void)
{
	/*
	 * Short-circuit extra attempts at looking for Xen
	 */
	if (xen_domain())
		return (1);

	/*
	 * The device tree contains the node /hypervisor with the compatible
	 * string "xen,xen" when the OS will run on top of Xen.
	 */
	if (ofw_bus_node_is_compatible(OF_finddevice("/hypervisor"), "xen,xen") == 0)
		return (0);

	vm_guest = VM_GUEST_XEN;
	setup_xen_features();

/* Implementation for 126b73dd8bd, but no longer works
	if (xen_feature(XENFEAT_dom0))
		HYPERVISOR_start_info->flags |= SIF_INITDOMAIN|SIF_PRIVILEGED;
	else
		HYPERVISOR_start_info->flags &= ~(SIF_INITDOMAIN|SIF_PRIVILEGED)
;
*/

	return (1);
}

/* in case of console being disabled, probe again as a fallback */
C_SYSINIT(xen_probe_fdt, SI_SUB_HYPERVISOR, SI_ORDER_FIRST, (sysinit_cfunc_t)xen_dt_probe, NULL);


struct xen_softc {
	struct resource		*intr;
	void			*cookie;
	int			rid;
};

struct xen_softc *xen_sc = NULL;

static int
xen_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "xen,xen"))
		return (ENXIO);

	device_set_desc(dev, "Xen ARM device-tree");

	return (BUS_PROBE_DEFAULT);
}

static int
xen_attach(device_t dev)
{
	struct xen_softc *sc;
	struct vcpu_register_vcpu_info info;
	struct vcpu_info *vcpu_info;
	vm_paddr_t phys;
	int rc, cpu;

	if (xen_sc != NULL)
		return (ENXIO);

	sc = device_get_softc(dev);

	/* TODO: Move to a proper function */
	vcpu_info = DPCPU_PTR(vcpu_local_info);
	cpu = PCPU_GET(cpuid);
	phys = vtophys(vcpu_info);
	info.mfn = phys >> PAGE_SHIFT_4K;
	info.offset = phys & PAGE_MASK_4K;

	rc = HYPERVISOR_vcpu_op(VCPUOP_register_vcpu_info, cpu, &info);
	KASSERT(rc == 0, ("Unable to register cpu %u\n", cpu));
	DPCPU_SET(vcpu_info, vcpu_info);


	/* Resources */
	sc->intr = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->rid, RF_ACTIVE);
	if (sc->intr == NULL) {
		panic("Unable to retrieve Xen event channel interrupt");
		return (ENXIO);
	}

	xen_sc = sc;

	/* Setup and enable the event channel interrupt */
	if (bus_setup_intr(dev, sc->intr, INTR_TYPE_MISC|INTR_MPSAFE,
		    xen_intr_handle_upcall, NULL, sc, &sc->cookie)) {
		panic("Could not setup event channel interrupt");
		return (ENXIO);
	}

	return (0);
}

static device_method_t xen_methods[] = {
	DEVMETHOD(device_probe,		xen_probe),
	DEVMETHOD(device_attach,	xen_attach),
	{ 0, 0 }
};

static driver_t xen_driver = {
	"xen",
	xen_methods,
	sizeof(struct xen_softc),
};

DRIVER_MODULE(xen, simplebus, xen_driver, 0, 0);
DRIVER_MODULE(xen, ofwbus, xen_driver, 0, 0);








static void
xen_intr_arch_disable_source(void *arg)
{
	struct xenisrc *isrc;

	isrc = arg;
	xen_intr_disable_source(isrc);
}

static void
xen_intr_arch_enable_source(void *arg)
{
	struct xenisrc *isrc;

	isrc = arg;
	xen_intr_enable_source(isrc);
}

static void
xen_intr_arch_eoi_source(void *arg)
{
	/* Nothing to do */
}

static int
xen_intr_arch_assign_cpu(void *arg, int cpuid)
{
	struct xenisrc *isrc;

	isrc = arg;
	return (xen_intr_assign_cpu(isrc, cpuid));
}

int
xen_arch_intr_setup(struct xenisrc *isrc)
{

	return (intr_event_create(&isrc->xi_arch, isrc, 0,
	    isrc->xi_vector /* IRQ */,
	    xen_intr_arch_disable_source /* mask */,
	    xen_intr_arch_enable_source /* unmask */,
	    xen_intr_arch_eoi_source /* EOI */,
	    xen_intr_arch_assign_cpu /* cpu assign */,
	    "xen%u", isrc->xi_port));
}

u_long
xen_arch_intr_execute_handlers(struct xenisrc *isrc, struct trapframe *frame)
{
	u_long strays;

	strays = intr_event_handle(isrc->xi_arch, frame);
	if (strays != 0) {
		xen_intr_disable_source(isrc);
		if (strays < INTR_STRAY_LOG_MAX)
			log(LOG_ERR, "stray evtchn %u: (%lu seen)\n",
			    isrc->xi_port, strays);
		else if (strays == INTR_STRAY_LOG_MAX)
			log(LOG_CRIT,
			    "too many stray evtchn %u: not logging anymore\n",
			   isrc->xi_port);
	}
	return (strays);
}

int
xen_arch_intr_add_handler(const char *name, driver_filter_t filter,
    driver_intr_t handler, void *arg, enum intr_type flags,
    struct xenisrc *isrc, void **cookiep)
{
	int error;

	error = intr_event_add_handler(isrc->xi_arch, name,
	    filter, handler, arg, intr_priority(flags), flags, cookiep);
	if (error != 0)
		return (error);

	/* Enable the event channel */
	xen_intr_enable_intr(isrc);
	xen_intr_enable_source(isrc);

	return (0);
}
