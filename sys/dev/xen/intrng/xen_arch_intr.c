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

#include <contrib/xen/event_channel.h>
#include <contrib/xen/hvm/params.h>

#include "pic_if.h"

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

	if (xen_feature(XENFEAT_dom0))
		hvm_start_flags |= SIF_INITDOMAIN | SIF_PRIVILEGED;

	return (1);
}

/* in case of console being disabled, probe again as a fallback */
C_SYSINIT(xen_probe_fdt, SI_SUB_HYPERVISOR, SI_ORDER_FIRST, (sysinit_cfunc_t)xen_dt_probe, NULL);

static void
xen_map_shared_info(void)
{
	int rc;
	struct xen_add_to_physmap xatp;
	vm_page_t shared_info;
	vm_paddr_t shared_paddr;

	if (!xen_domain())
		return;

	shared_info = vm_page_alloc_noobj(VM_ALLOC_ZERO | VM_ALLOC_WIRED |
	    VM_ALLOC_WAITFAIL);
	KASSERT(shared_info != NULL, ("Unable to allocate shared page\n"));
	shared_paddr = VM_PAGE_TO_PHYS(shared_info);
	HYPERVISOR_shared_info = pmap_mapdev_attr(shared_paddr, PAGE_SIZE,
	    VM_MEMATTR_XEN);

	xatp.domid = DOMID_SELF;
	xatp.idx = 0;
	xatp.space = XENMAPSPACE_shared_info;
	xatp.gpfn = shared_paddr >> PAGE_SHIFT;
	rc = HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp);
	if (rc != 0)
		panic("Unable to map shared info error=%d\n", rc);
}

/* xen_map_shared_info() won't work during console probe, thus this */
C_SYSINIT(xen_shared_info, SI_SUB_HYPERVISOR, SI_ORDER_SECOND,
    (sysinit_cfunc_t)xen_map_shared_info, NULL);

#ifdef SMP
static void
setup_vcpu(const void *unused __unused)
{
	cpuset_t procs = all_cpus;

	CPU_CLR(0, &procs);
	if (xen_domain() && mp_ncpus > 1)
		smp_rendezvous_cpus(procs, smp_no_rendezvous_barrier,
		    (void (*)(void *))xen_setup_vcpu_info,
		    smp_no_rendezvous_barrier, NULL);
}
C_SYSINIT(setup_vcpu, SI_SUB_SMP, SI_ORDER_SECOND, setup_vcpu, NULL);
#endif

struct xen_softc {
	struct resource		*intr;
	void			*cookie;
	int			rid;
	device_t		dev;
	struct intr_pic		*pic;
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

	if (xen_sc != NULL)
		return (ENXIO);

	sc = device_get_softc(dev);

	sc->dev = dev;

	sc->pic = intr_pic_register(dev,
	    OF_xref_from_node(ofw_bus_get_node(dev)));
	if (sc->pic == NULL)
		return (ENXIO);

	/* setup vCPU #0 so events work on first processor */
	xen_setup_vcpu_info();


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

static void
xen_intrng_intr_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{

	xen_intr_disable_intr((struct xenisrc *)isrc);
}

static void
xen_intrng_intr_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{

	xen_intr_enable_intr((struct xenisrc *)isrc);
}

static void
xen_intrng_intr_post_filter(device_t dev, struct intr_irqsrc *isrc)
{

	/* should only clear the port now, but oh well for the moment */
}

static void
xen_intrng_intr_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{

	xen_intr_disable_source((struct xenisrc *)isrc);
}

static void
xen_intrng_intr_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{

	xen_intr_enable_source((struct xenisrc *)isrc);
}

static int
xen_intrng_intr_bind(device_t dev, struct intr_irqsrc *isrc)
{
	struct xenisrc *xsrc = (struct xenisrc *)isrc;
	u_int cpu;

	/* distinctly inspired by sys/arm64/arm64/gic_v3.c:gic_v3_bind_intr() */
	if (CPU_EMPTY(&isrc->isrc_cpu)) {
		cpu = xen_arch_intr_next_cpu(xsrc);
		CPU_SETOF(cpu, &isrc->isrc_cpu);
	} else {
		/*
		 * We can only bind to a single CPU so select
		 * the first CPU found.
		 */
		cpu = CPU_FFS(&isrc->isrc_cpu) - 1;
	}

	return (xen_intr_assign_cpu(xsrc, cpu));
}

static device_method_t xen_methods[] = {
	DEVMETHOD(device_probe,		xen_probe),
	DEVMETHOD(device_attach,	xen_attach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	xen_intrng_intr_disable_intr),
	DEVMETHOD(pic_enable_intr,	xen_intrng_intr_enable_intr),
	DEVMETHOD(pic_post_filter,	xen_intrng_intr_post_filter),
	DEVMETHOD(pic_pre_ithread,	xen_intrng_intr_pre_ithread),
	DEVMETHOD(pic_post_ithread,	xen_intrng_intr_post_ithread),
	DEVMETHOD(pic_bind_intr,	xen_intrng_intr_bind),

	DEVMETHOD_END
};

static driver_t xen_driver = {
	"xen",
	xen_methods,
	sizeof(struct xen_softc),
};

DRIVER_MODULE(xen, ofwbus, xen_driver, 0, 0);








static MALLOC_DEFINE(M_XENINTR, "xen_intr", "Xen Interrupt Services");

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

struct xenisrc *
xen_arch_intr_alloc(void)
{
	static u_int counter = 0;
	struct xenisrc *isrc;
	int error;

	if (!(isrc = malloc(sizeof(struct xenisrc), M_XENINTR, M_WAITOK | M_ZERO)))
		return (NULL);

	error = intr_isrc_register(&isrc->xi_arch, xen_sc->dev, 0, "xen%u",
	    ++counter);

	if (error) {
		free(isrc, M_XENINTR);
		isrc = NULL;
	}

	return (isrc);
}

void
xen_arch_intr_release(struct xenisrc *isrc)
{
	int error;

	KASSERT(isrc->xi_arch.isrc_event == NULL ||
	    CK_SLIST_EMPTY(&isrc->xi_arch.isrc_event->ie_handlers),
	    ("Release called, but xenisrc still in use"));

	if ((error = intr_isrc_deregister(&isrc->xi_arch)) != 0)
		printf("%s(): leaking isrc due to failure during release: %d",
		    __func__, error);
	else
		free(isrc, M_XENINTR);
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
