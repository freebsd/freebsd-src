/*-
 * Copyright (c) 2012-2014 Jakub Wojciech Klama <jceel@FreeBSD.org>.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h> 
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/conf.h>
#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/cpu.h>
#include <machine/smp.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "pic_if.h"

#define	INTRNAME_LEN	(MAXCOMLEN + 1)


#define MAXINTRS 1024 // XXX Need this passed in to pic registration

//#define DEBUG
#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

typedef void (*mask_fn)(void *);

struct arm_intr_controller {
	device_t		ic_dev;
	phandle_t		ic_node;
	u_int			ic_idx;
	u_int			ic_maxintrs;
	struct arm_intr_handler	**ic_ih_by_hwirq;
	u_int			ic_ih_count;
	SLIST_HEAD(, arm_intr_handler)
				ic_ih_list;
};

struct arm_intr_handler {
	SLIST_ENTRY(arm_intr_handler) ih_next_entry;
	u_int			ih_intrcnt_idx;
	u_int			ih_resirq;
	u_int			ih_hwirq;
	u_int			ih_ncells;
	enum intr_trigger	ih_trig;
	enum intr_polarity	ih_pol;
	struct intr_event *	ih_event;
	struct arm_intr_controller *ih_ic;
	pcell_t			ih_cells[];
};

static u_int resirq_encode(u_int picidx, u_int irqidx);


static void arm_mask_irq(void *);
static void arm_unmask_irq(void *);
static void arm_eoi(void *);

static struct arm_intr_controller arm_pics[NPIC];
static struct arm_intr_controller *arm_ipi_pic;

static int intrcnt_index;
static int intrcnt_last_printed;

MALLOC_DECLARE(M_INTRNG);
MALLOC_DEFINE(M_INTRNG, "intrng", "ARM interrupt handling");

static const char *ipi_names[] = {
	"IPI:AST",
	"IPI:PREEMPT",
	"IPI:RENDEZVOUS",
	"IPI:STOP",
	"IPI:HARDCLOCK",
	"IPI:TLB"
};
CTASSERT(ARM_IPI_COUNT == nitems(ipi_names));

static struct arm_intr_handler * ipi_handlers[ARM_IPI_COUNT];

/* Data for statistics reporting. */
u_long intrcnt[NIRQ];
char intrnames[NIRQ * INTRNAME_LEN];
size_t sintrcnt = sizeof(intrcnt);
size_t sintrnames = sizeof(intrnames);
int (*arm_config_irq)(int irq, enum intr_trigger trig,
    enum intr_polarity pol) = NULL;

static inline struct arm_intr_controller *
ic_from_dev(device_t dev)
{
	struct arm_intr_controller *ic;
	u_int i;

	for (i = 0, ic = arm_pics; i < nitems(arm_pics); i++, ic++) {
		if (dev == ic->ic_dev)
			return (ic);
	}
	return (NULL);
}

static inline struct arm_intr_controller *
ic_from_node(phandle_t node)
{
	struct arm_intr_controller *ic;
	u_int i;

	for (i = 0, ic = arm_pics; i < nitems(arm_pics); i++, ic++) {
		if (node == ic->ic_node)
			return (ic);
	}
	return (NULL);
}

static struct arm_intr_controller *
ic_create(phandle_t node)
{
	struct arm_intr_controller *ic;
	u_int i;

	for (i = 0, ic = arm_pics; i < nitems(arm_pics); i++, ic++) {
		if (ic->ic_node == 0)
			break;
	}
	if (i == nitems(arm_pics))
		panic("no room to add interrupt controller");

	bzero(ic, sizeof(ic));
	ic->ic_idx = i;
	ic->ic_node = node;
	SLIST_INIT(&ic->ic_ih_list);

	debugf("allocated new interrupt controller at index %d ptr %p for node %d\n", i, ic, node);
	return (ic);
}

static void
ic_setup_dev(struct arm_intr_controller *ic, device_t dev, u_int maxintrs)
{
	struct arm_intr_handler *ih;

	ic->ic_dev = dev;
	ic->ic_maxintrs = maxintrs;
	ic->ic_ih_by_hwirq = malloc(maxintrs * sizeof(struct arm_intr_handler *),
	    M_INTRNG, M_WAITOK | M_ZERO);
	SLIST_FOREACH(ih, &ic->ic_ih_list, ih_next_entry) {
		PIC_TRANSLATE(ic->ic_dev, ih->ih_cells, &ih->ih_hwirq,
		    &ih->ih_trig, &ih->ih_pol);
		ic->ic_ih_by_hwirq[ih->ih_hwirq] = ih;
	}
}

static struct arm_intr_handler *
ic_add_ih(struct arm_intr_controller *ic, pcell_t *cells, u_int ncells)
{
	struct arm_intr_handler *ih;
	u_int cellsize;

	cellsize = ncells * sizeof(*cells);
	ih = malloc(sizeof(*ih) + cellsize, M_INTRNG, M_WAITOK | M_ZERO);
	memcpy(ih->ih_cells, cells, cellsize);
	ih->ih_ncells = ncells;
	ih->ih_ic = ic;
	ih->ih_resirq = resirq_encode(ic->ic_idx, ic->ic_ih_count++);
	SLIST_INSERT_HEAD(&ic->ic_ih_list, ih, ih_next_entry);
	return (ih);
}

static void
ic_index_ih_by_hwirq(struct arm_intr_controller *ic, 
    struct arm_intr_handler *ih)
{

	KASSERT(ih->ih_hwirq < ic->ic_maxintrs, ("%s irq %u too large", 
	    device_get_nameunit(ic->ic_dev), ih->ih_hwirq));
	KASSERT(ic->ic_ih_by_hwirq[ih->ih_hwirq] == NULL, 
	    ("%s irq %u already registered", device_get_nameunit(ic->ic_dev),
	    ih->ih_hwirq));
	ic->ic_ih_by_hwirq[ih->ih_hwirq] = ih;
}

static struct arm_intr_handler *
ih_from_fdtcells(struct arm_intr_controller *ic, pcell_t *cells,  u_int ncells)
{
	struct arm_intr_handler *ih;

	SLIST_FOREACH(ih, &ic->ic_ih_list, ih_next_entry) {
		if (ncells == ih->ih_ncells && memcmp(cells, ih->ih_cells, 
		    ncells * sizeof(*cells)) == 0)
			return (ih);
	}
	return (NULL);
}

static struct arm_intr_handler *
ih_from_resirq(struct arm_intr_controller *ic, u_int resirq)
{
	struct arm_intr_handler *ih;

	SLIST_FOREACH(ih, &ic->ic_ih_list, ih_next_entry) {
		if (resirq == ih->ih_resirq)
			return (ih);
	}
	return (NULL);
}

static struct arm_intr_handler *
ih_from_hwirq(struct arm_intr_controller *ic, u_int hwirq)
{

	return (ic->ic_ih_by_hwirq[hwirq]);
}

static u_int
resirq_encode(u_int picidx, u_int irqidx)
{

	return((picidx << 16) | (irqidx & 0xffff));
}

static u_int
resirq_decode(int resirq, struct arm_intr_controller **pic, 
   struct arm_intr_handler **pih)
{
	struct arm_intr_controller *ic;
	u_int irqidx, picidx;

	picidx = resirq >> 16;
	KASSERT(picidx < nitems(arm_pics), ("bad pic index %u", picidx));
	ic = &arm_pics[picidx];

	irqidx = resirq & 0xffff;
	KASSERT(irqidx < ic->ic_ih_count, ("bad irq index %u for pic %u",
	    irqidx, picidx));

	if (pic != NULL)
		*pic = ic;
	if (pih != NULL && ic != NULL)
		*pih = ih_from_resirq(ic, resirq);

	return (irqidx);
}

void
arm_intrnames_init(void)
{
	/* nothing... */
}

void
arm_dispatch_irq(device_t dev, struct trapframe *tf, int irq)
{
	struct arm_intr_controller *ic;
	struct arm_intr_handler *ih = NULL;

//	debugf("pic %s, tf %p, irq %d\n", device_get_nameunit(dev), tf, irq);

	/*
	 * If we got null trapframe argument, that probably means
	 * a call from non-root interrupt controller. In that case,
	 * we'll just use the saved one.
	 */
	if (tf == NULL)
		tf = PCPU_GET(curthread)->td_intr_frame;

	ic = ic_from_dev(dev);
	KASSERT(ic != NULL, ("%s: interrupt controller for %s not found", 
	    __FUNCTION__, device_get_nameunit(dev)));

	ih = ih_from_hwirq(ic, irq);
	KASSERT(ih != NULL, ("%s: interrupt handler for %s irq %d not found",
	    __FUNCTION__, device_get_nameunit(dev), irq));

	intrcnt[ih->ih_intrcnt_idx]++;
	if (intr_event_handle(ih->ih_event, tf) != 0) {
		device_printf(dev, "stray irq %d; disabled", irq);
		arm_mask_irq(ih);
	}

//	debugf("done\n");
}

int
arm_fdt_map_irq(phandle_t icnode, pcell_t *cells, int ncells)
{
	struct arm_intr_controller *ic;
	struct arm_intr_handler *ih;

	debugf("map icnode %08x cells <%*D>\n", icnode, ncells * sizeof(pcell_t),
	    (char *)cells, ",");

	icnode = OF_node_from_xref(icnode);

	ic = ic_from_node(icnode);
	if (ic == NULL)
		ic = ic_create(icnode);

	ih = ih_from_fdtcells(ic, cells, ncells);
	if (ih == NULL) {
		ih = ic_add_ih(ic, cells, ncells);
		if (ic->ic_dev != NULL) {
			PIC_TRANSLATE(ic->ic_dev, ih->ih_cells, &ih->ih_hwirq,
			    &ih->ih_trig, &ih->ih_pol);
			ic_index_ih_by_hwirq(ic, ih);
		}
	}
	return (ih->ih_resirq);
}

const char *
arm_describe_irq(int resirq)
{
	struct arm_intr_controller *ic;
	struct arm_intr_handler *ih;
	int irqidx;
	static char buffer[INTRNAME_LEN];

	/* XXX static buffer, can this be called after APs released? */

	irqidx = resirq_decode(resirq, &ic, &ih);
	KASSERT(ic != NULL, ("%s: bad resirq 0x%08x", resirq));
	if (ic->ic_dev == NULL) {
		/*
		 * Interrupt controller not attached yet. We don't know the
		 * IC device name nor interrupt number. All we can do is to
		 * use its index (fdt names are unbounded length).
		 */
		snprintf(buffer, sizeof(buffer), "ic%d.%d", ic->ic_idx, irqidx);
	} else {
		KASSERT(ih != NULL, ("%s: no handler for resirq 0x%08x\n", resirq));
		snprintf(buffer, sizeof(buffer), "%s.%d", 
		    device_get_nameunit(ih->ih_ic->ic_dev), ih->ih_hwirq);
	}
	return (buffer);
}

void
arm_register_pic(device_t dev, int flags)
{
	struct arm_intr_controller *ic;
	struct arm_intr_handler *ih;
	phandle_t node;

	node = ofw_bus_get_node(dev);
	ic = ic_from_node(node);
	if (ic == NULL)
		ic = ic_create(node);

	ic_setup_dev(ic, dev, MAXINTRS);

	/*
	 * The nexus root usually isn't described by fdt data.  If the node is
	 * -1 and the number of interrupts added is zero and the device's
	 * name is "nexus", allocate a single entry for irq 0.
	 */
	if (node == -1 && ic->ic_ih_count == 0 &&
	    strcmp(device_get_name(dev), "nexus") == 0) {
		ih = ic_add_ih(ic, NULL, 0);
		ih->ih_hwirq = 0;
		ic_index_ih_by_hwirq(ic, ih);
	}

	debugf("device %s node %08x slot %d\n", device_get_nameunit(dev),
	    ic->ic_node, ic->ic_idx);

	if (flags & PIC_FEATURE_IPI) {
		KASSERT(arm_ipi_pic == NULL, 
		    ("controller for IPIs is already registered"));
		arm_ipi_pic = ic;
	}

	/*
	 * arm_describe_irq() has to print fake names earlier when the device
	 * issn't registered yet, emit a string that has the same fake name in
	 * it, so that earlier output links to this device.
	 */
	device_printf(dev, "registered as interrupt controller ic%d\n", 
	    ic->ic_idx);
}

void
arm_setup_irqhandler(device_t dev, driver_filter_t *filt, 
    void (*hand)(void*), void *arg, int resirq, int flags, void **cookiep)
{
	struct arm_intr_controller *ic;
	struct arm_intr_handler *ih;
	const char *name;
	int error;
	int irqidx;

	if (flags & INTR_IPI) {
		ic = arm_ipi_pic;
		irqidx = resirq; /* resirq is the same as hwirq for IPIs */
		KASSERT(ic != NULL, ("%s: no interrupt controller for IPIs",
		    __FUNCTION__));
		ih = ipi_handlers[irqidx];
		KASSERT(ih != NULL, 
		    ("%s: interrupt handler for %s IPI %u not found",
		    __FUNCTION__, device_get_nameunit(ic->ic_dev),
		    irqidx));
		KASSERT(irqidx < ARM_IPI_COUNT, ("IPI number too big: %u",
		    irqidx));
		name = ipi_names[irqidx];
		debugf("setup ipi %u (%s)\n", irqidx, name);
	} else {
		irqidx = resirq_decode(resirq, &ic, &ih);
		name = device_get_nameunit(dev);
		debugf("setup irq %s.%d on %s\n",
		    device_get_nameunit(ic->ic_dev), ih->ih_hwirq, name);
	}

	if (ih->ih_event == NULL) {
		error = intr_event_create(&ih->ih_event, ih, 0, resirq,
		    (mask_fn)arm_mask_irq, (mask_fn)arm_unmask_irq,
		    arm_eoi, NULL, "ic%d.%d:", ic->ic_idx, ih->ih_hwirq);
		if (error) {
			device_printf(dev, "intr_event_create() failed "
			    "for irq %s.%u\n", device_get_nameunit(ic->ic_dev),
			     ih->ih_hwirq);
			return;
		}
		intrcnt_last_printed += 1 +
		    snprintf(intrnames + intrcnt_last_printed,
		    INTRNAME_LEN, "%s:%d: %s",
		    device_get_nameunit(ic->ic_dev), irqidx, name);

		ih->ih_intrcnt_idx = intrcnt_index++;
	}

	if (!TAILQ_EMPTY(&ih->ih_event->ie_handlers))
		arm_mask_irq(ih);
	intr_event_add_handler(ih->ih_event, name, filt, hand, arg,
	    intr_priority(flags), flags, cookiep);
	arm_unmask_irq(ih);
}

int
arm_remove_irqhandler(int resirq, void *cookie)
{
	struct arm_intr_controller *ic;
	struct arm_intr_handler *ih;
	int error;

	resirq_decode(resirq, &ic, &ih);
	arm_mask_irq(ih);
	error = intr_event_remove_handler(cookie);
	if (!TAILQ_EMPTY(&ih->ih_event->ie_handlers))
		arm_unmask_irq(ih);
	return (error);
}

static void
arm_mask_irq(void *arg)
{
	struct arm_intr_handler *ih = (struct arm_intr_handler *)arg;

	PIC_MASK(ih->ih_ic->ic_dev, ih->ih_hwirq);
}

static void
arm_unmask_irq(void *arg)
{
	struct arm_intr_handler *ih = (struct arm_intr_handler *)arg;

	PIC_UNMASK(ih->ih_ic->ic_dev, ih->ih_hwirq);
}

static void
arm_eoi(void *arg)
{
	struct arm_intr_handler *ih = (struct arm_intr_handler *)arg;

	PIC_EOI(ih->ih_ic->ic_dev, ih->ih_hwirq);
}

int
arm_intrng_config_irq(int resirq, enum intr_trigger trig, enum intr_polarity pol)
{
	struct arm_intr_controller *ic;
	struct arm_intr_handler *ih;

	resirq_decode(resirq, &ic, &ih);

	return PIC_CONFIG(ic->ic_dev, ih->ih_hwirq, trig, pol);
}

#ifdef SMP

void
arm_ipi_map_irq(device_t dev, u_int ipi, u_int hwirq)
{
	struct arm_intr_controller *ic;
	struct arm_intr_handler *ih;

	ic = ic_from_dev(dev);
	KASSERT(ic != NULL, ("ipi controller not registered"));

	ih = ih_from_hwirq(ic, hwirq);
	KASSERT(ih == NULL, ("handler already registered for IPI %u", ipi));

	ih = ic_add_ih(ic, NULL, 0);
	ih->ih_hwirq = hwirq;
	ic_index_ih_by_hwirq(ic, ih);
	ipi_handlers[ipi] = ih;
	debugf("ipi %u mapped to %s.%u\n", ipi, device_get_nameunit(dev), hwirq);
}

void
arm_init_secondary_ic(void)
{

	KASSERT(arm_ipi_pic != NULL, ("%s: no IPI PIC attached", __FUNCTION__));
	PIC_INIT_SECONDARY(arm_ipi_pic->ic_dev);
}

void
pic_ipi_send(cpuset_t cpus, u_int ipi)
{

	KASSERT(ipi < ARM_IPI_COUNT, ("invalid IPI %u", ipi));
	KASSERT(ipi_handlers[ipi] != NULL, ("no handler for IPI %u", ipi));
	PIC_IPI_SEND(ipi_handlers[ipi]->ih_ic->ic_dev, cpus, ipi);
}

void
pic_ipi_clear(int ipi)
{

	KASSERT(ipi < ARM_IPI_COUNT, ("invalid IPI %u", ipi));
	KASSERT(ipi_handlers[ipi] != NULL, ("no handler for IPI %u", ipi));
	PIC_IPI_CLEAR(ipi_handlers[ipi]->ih_ic->ic_dev, ipi);
}

int
pic_ipi_read(int ipi)
{

	KASSERT(arm_ipi_pic != NULL, ("no IPI interrupt controller"));
	return (PIC_IPI_READ(arm_ipi_pic->ic_dev, ipi));
}

void
arm_unmask_ipi(int ipi)
{

	KASSERT(ipi < ARM_IPI_COUNT, ("invalid IPI %u", ipi));
	KASSERT(ipi_handlers[ipi] != NULL, ("no handler for IPI %u", ipi));
	PIC_UNMASK(ipi_handlers[ipi]->ih_ic->ic_dev, ipi);
}

void
arm_mask_ipi(int ipi)
{

	KASSERT(ipi < ARM_IPI_COUNT, ("invalid IPI %u", ipi));
	KASSERT(ipi_handlers[ipi] != NULL, ("no handler for IPI %u", ipi));
	PIC_MASK(ipi_handlers[ipi]->ih_ic->ic_dev, ipi);
}
#endif

void dosoftints(void);
void
dosoftints(void)
{
}

/*
 * arm_irq_memory_barrier()
 *
 * Ensure all writes to device memory have reached devices before proceeding.
 *
 * This is intended to be called from the post-filter and post-thread routines
 * of an interrupt controller implementation.  A peripheral device driver should
 * use bus_space_barrier() if it needs to ensure a write has reached the
 * hardware for some reason other than clearing interrupt conditions.
 *
 * The need for this function arises from the ARM weak memory ordering model.
 * Writes to locations mapped with the Device attribute bypass any caches, but
 * are buffered.  Multiple writes to the same device will be observed by that
 * device in the order issued by the cpu.  Writes to different devices may
 * appear at those devices in a different order than issued by the cpu.  That
 * is, if the cpu writes to device A then device B, the write to device B could
 * complete before the write to device A.
 *
 * Consider a typical device interrupt handler which services the interrupt and
 * writes to a device status-acknowledge register to clear the interrupt before
 * returning.  That write is posted to the L2 controller which "immediately"
 * places it in a store buffer and automatically drains that buffer.  This can
 * be less immediate than you'd think... There may be no free slots in the store
 * buffers, so an existing buffer has to be drained first to make room.  The
 * target bus may be busy with other traffic (such as DMA for various devices),
 * delaying the drain of the store buffer for some indeterminate time.  While
 * all this delay is happening, execution proceeds on the CPU, unwinding its way
 * out of the interrupt call stack to the point where the interrupt driver code
 * is ready to EOI and unmask the interrupt.  The interrupt controller may be
 * accessed via a faster bus than the hardware whose handler just ran; the write
 * to unmask and EOI the interrupt may complete quickly while the device write
 * to ack and clear the interrupt source is still lingering in a store buffer
 * waiting for access to a slower bus.  With the interrupt unmasked at the
 * interrupt controller but still active at the device, as soon as interrupts
 * are enabled on the core the device re-interrupts immediately: now you've got
 * a spurious interrupt on your hands.
 *
 * The right way to fix this problem is for every device driver to use the
 * proper bus_space_barrier() calls in its interrupt handler.  For ARM a single
 * barrier call at the end of the handler would work.  This would have to be
 * done to every driver in the system, not just arm-specific drivers.
 *
 * Another potential fix is to map all device memory as Strongly-Ordered rather
 * than Device memory, which takes the store buffers out of the picture.  This
 * has a pretty big impact on overall system performance, because each strongly
 * ordered memory access causes all L2 store buffers to be drained.
 *
 * A compromise solution is to have the interrupt controller implementation call
 * this function to establish a barrier between writes to the interrupt-source
 * device and writes to the interrupt controller device.
 *
 * This takes the interrupt number as an argument, and currently doesn't use it.
 * The plan is that maybe some day there is a way to flag certain interrupts as
 * "memory barrier safe" and we can avoid this overhead with them.
 */
void
arm_irq_memory_barrier(uintptr_t irq)
{

	dsb();
	cpu_l2cache_drain_writebuf();
}

