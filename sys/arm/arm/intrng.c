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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h> 
#include <sys/malloc.h>
#include <sys/proc.h>
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

#define	IRQ_PIC_IDX(_irq)	((_irq >> 8) & 0xff)
#define	IRQ_VECTOR_IDX(_irq)	((_irq) & 0xff)
#define	IRQ_GEN(_pic, _irq)	(((_pic) << 8) | ((_irq) & 0xff))

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
	int			ic_maxintrs;
	struct arm_intr_handler	*ic_intrs;
};

struct arm_intr_handler {
	device_t		ih_dev;
	const char *		ih_ipi_name;
	int			ih_intrcnt_idx;
	int			ih_irq;
	pcell_t			ih_cells[8];
	int			ih_ncells;
	enum intr_trigger	ih_trig;
	enum intr_polarity	ih_pol;
	struct intr_event *	ih_event;
	struct arm_intr_controller *ih_pic;
};

static void arm_mask_irq(void *);
static void arm_unmask_irq(void *);
static void arm_eoi(void *);

static struct arm_intr_controller arm_pics[NPIC];
static struct arm_intr_controller *arm_ipi_pic;

static int intrcnt_index = 0;
static int last_printed = 0;

MALLOC_DECLARE(M_INTRNG);
MALLOC_DEFINE(M_INTRNG, "intrng", "ARM interrupt handling");

/* Data for statistics reporting. */
u_long intrcnt[NIRQ];
char intrnames[NIRQ * INTRNAME_LEN];
size_t sintrcnt = sizeof(intrcnt);
size_t sintrnames = sizeof(intrnames);
int (*arm_config_irq)(int irq, enum intr_trigger trig,
    enum intr_polarity pol) = NULL;

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
	int i;

	debugf("pic %s, tf %p, irq %d\n", device_get_nameunit(dev), tf, irq);

	/*
	 * If we got null trapframe argument, that probably means
	 * a call from non-root interrupt controller. In that case,
	 * we'll just use the saved one.
	 */
	if (tf == NULL)
		tf = PCPU_GET(curthread)->td_intr_frame;

	for (ic = &arm_pics[0]; ic->ic_dev != NULL; ic++) {
		if (ic->ic_dev == dev) {
			for (i = 0; i < ic->ic_maxintrs; i++) {
				ih = &ic->ic_intrs[i];
				if (irq == ih->ih_irq)
					break;
			}
		}
	}

	if (ih == NULL)
		panic("arm_dispatch_irq: unknown irq");

	debugf("requested by %s\n", ih->ih_ipi_name != NULL
	    ? ih->ih_ipi_name
	    : device_get_nameunit(ih->ih_dev));

	intrcnt[ih->ih_intrcnt_idx]++;
	if (intr_event_handle(ih->ih_event, tf) != 0) {
		/* Stray IRQ */
		arm_mask_irq(ih);
	}

	debugf("done\n");
}

static struct arm_intr_handler *
arm_lookup_intr_handler(device_t pic, int idx)
{
	struct arm_intr_controller *ic;

	for (ic = &arm_pics[0]; ic->ic_dev != NULL; ic++) {
		if (ic->ic_dev != NULL && ic->ic_dev == pic)
			return (&ic->ic_intrs[idx]);
	}

	return (NULL);
}


int
arm_fdt_map_irq(phandle_t ic, pcell_t *cells, int ncells)
{
	struct arm_intr_controller *pic;
	struct arm_intr_handler *ih;
	int i, j;

	ic = OF_xref_phandle(ic);

	debugf("ic %08x cells <%*D>\n", ic, ncells * sizeof(pcell_t),
	    (char *)cells, ",");

	for (i = 0; arm_pics[i].ic_node != 0; i++) {
		pic = &arm_pics[i];
		if (pic->ic_node == ic) {
			for (j = 0; j < pic->ic_maxintrs; j++) {
				ih = &pic->ic_intrs[j];

				/* Compare pcell contents */
				if (!memcmp(cells, ih->ih_cells, ncells))
					return (IRQ_GEN(i, j));
			}

			/* Not found - new entry required */
			pic->ic_maxintrs++;
			pic->ic_intrs = realloc(pic->ic_intrs,
			    pic->ic_maxintrs * sizeof(struct arm_intr_handler),
			    M_INTRNG, M_WAITOK | M_ZERO);

			ih = &pic->ic_intrs[pic->ic_maxintrs - 1];
			ih->ih_pic = pic;
			ih->ih_ncells = ncells;
			memcpy(ih->ih_cells, cells, ncells);

			if (pic->ic_dev != NULL) {
				/* Map IRQ number */
				PIC_TRANSLATE(pic->ic_dev, cells, &ih->ih_irq,
				    &ih->ih_trig, &ih->ih_pol);

				debugf(pic->ic_dev, "translated to irq %d\n",
				    ih->ih_irq);
			}

			return (IRQ_GEN(i, pic->ic_maxintrs - 1));
		};
	}

	/* 
	 * Interrupt controller is not registered yet, so
	 * we map a stub for it. 'i' is pointing to free
	 * first slot in arm_pics table.
	 */
	debugf("allocating new ic at index %d\n", i);

	pic = &arm_pics[i];
	pic->ic_node = ic;
	pic->ic_maxintrs = 1;
	pic->ic_intrs = malloc(sizeof(struct arm_intr_handler), M_INTRNG,
	    M_WAITOK | M_ZERO);

	ih = &pic->ic_intrs[0];
	ih->ih_pic = pic;
	ih->ih_ncells = ncells;
	memcpy(ih->ih_cells, cells, ncells);

	return (IRQ_GEN(i, 0));
}

const char *
arm_describe_irq(int irq)
{
	struct arm_intr_controller *pic;
	struct arm_intr_handler *ih;
	static char buffer[INTRNAME_LEN];
	static char name[INTRNAME_LEN];

	pic = &arm_pics[IRQ_PIC_IDX(irq)];

	if (pic == NULL)
		return ("<unknown ic>");

	if (IRQ_VECTOR_IDX(irq) > pic->ic_maxintrs)
		return ("<unknown irq>");

	ih = &pic->ic_intrs[IRQ_VECTOR_IDX(irq)];

	if (pic->ic_dev == NULL) {
		/*
		 * Interrupt controller not attached yet. We don't know the
		 * IC device name nor interrupt number. All we can do is to
		 * use FDT 'name' property.
		 */
		OF_getprop(ih->ih_pic->ic_node, "name", name, sizeof(name));
		snprintf(buffer, sizeof(buffer), "%s.?", name);
		return (buffer);
	}

	snprintf(buffer, sizeof(buffer), "%s.%d",
	    device_get_nameunit(ih->ih_pic->ic_dev), ih->ih_irq);

	return (buffer);
}

void
arm_register_pic(device_t dev, int flags)
{
	struct arm_intr_controller *ic = NULL;
	struct arm_intr_handler *ih;
	phandle_t node;
	int i;

	node = ofw_bus_get_node(dev);

	/* Find room for IC */
	for (i = 0; i < NPIC; i++) {
		if (arm_pics[i].ic_dev != NULL)
			continue;

		if (arm_pics[i].ic_node == node) {
			ic = &arm_pics[i];
			break;
		}

		if (arm_pics[i].ic_node == 0) {
			ic = &arm_pics[i];
			break;
		}
	}

	if (ic == NULL)
		panic("not enough room to register interrupt controller");

	ic->ic_dev = dev;
	ic->ic_node = node;

	/*
	 * Normally ic_intrs is allocated by arm_fdt_map_irq(), but the nexus
	 * root isn't described by fdt data.  If the node is -1 and the ic_intrs
	 * array hasn't yet been allocated, we're dealing with nexus, allocate a
	 * single entry for irq 0.
	 */
	if (node == -1 && ic->ic_intrs == NULL) {
	       ic->ic_intrs = malloc(sizeof(struct arm_intr_handler), M_INTRNG,
		   M_WAITOK | M_ZERO);
	       ic->ic_maxintrs = 1;
	       ih = &ic->ic_intrs[0];
	       ih->ih_pic = ic;
	       ih->ih_ncells = 0;
       }

	debugf("device %s node %08x slot %d\n", device_get_nameunit(dev),
	    ic->ic_node, i);

	if (flags & PIC_FEATURE_IPI) {
		if (arm_ipi_pic != NULL)
			panic("there's already registered interrupt "
			    "controller for serving IPIs");

		arm_ipi_pic = ic;
	}

	/* Resolve IRQ numbers for interrupt handlers added earlier */
	for (i = 0; i < ic->ic_maxintrs; i++) {
		ih = &ic->ic_intrs[i];

		/* Map IRQ number */
		PIC_TRANSLATE(ic->ic_dev, ih->ih_cells, &ih->ih_irq,
		    &ih->ih_trig, &ih->ih_pol);

		debugf(ic->ic_dev, "translated to irq %d\n", ih->ih_irq);
	}

	device_printf(dev, "registered as interrupt controller\n");
}

void
arm_setup_irqhandler(device_t dev, driver_filter_t *filt, 
    void (*hand)(void*), void *arg, int irq, int flags, void **cookiep)
{
	struct arm_intr_controller *pic;
	struct arm_intr_handler *ih;
	const char *name;
	int error;
	int ipi;

	if (irq < 0)
		return;

	ipi = (flags & INTR_IPI) != 0;
	pic = ipi ? arm_ipi_pic : &arm_pics[IRQ_PIC_IDX(irq)];
	ih = arm_lookup_intr_handler(pic->ic_dev, IRQ_VECTOR_IDX(irq));

	if (ipi) {
		name = (const char *)dev;
		debugf("setup ipi %d\n", irq);
	} else {
		name = device_get_nameunit(dev);
		debugf("setup irq %d on %s\n", IRQ_VECTOR_IDX(irq),
		    device_get_nameunit(pic->ic_dev));
	}

	debugf("pic %p, ih %p\n", pic, ih);

	if (ih->ih_event == NULL) {
		error = intr_event_create(&ih->ih_event, (void *)ih, 0, irq,
		    (mask_fn)arm_mask_irq, (mask_fn)arm_unmask_irq,
		    arm_eoi, NULL, "intr%d.%d:", IRQ_PIC_IDX(irq),
		    IRQ_VECTOR_IDX(irq));
		
		if (error)
			return;

		ih->ih_dev = dev;
		ih->ih_ipi_name = ipi ? name : NULL;
		ih->ih_pic = pic;

		arm_unmask_irq(ih);

		last_printed += 
		    snprintf(intrnames + last_printed,
		    INTRNAME_LEN, "%s:%d: %s",
		    device_get_nameunit(pic->ic_dev),
		    ih->ih_irq, name);
		
		last_printed++;
		ih->ih_intrcnt_idx = intrcnt_index;
		intrcnt_index++;
		
	}

	intr_event_add_handler(ih->ih_event, name, filt, hand, arg,
	    intr_priority(flags), flags, cookiep);

	/* Unmask IPIs immediately */
	if (ipi)
		arm_unmask_irq(ih);
}

int
arm_remove_irqhandler(int irq, void *cookie)
{
	struct arm_intr_controller *pic;
	struct arm_intr_handler *ih;
	int error;

	if (irq < 0)
		return (ENXIO);

	pic = &arm_pics[IRQ_PIC_IDX(irq)];
	ih = arm_lookup_intr_handler(pic->ic_dev, IRQ_VECTOR_IDX(irq));

	if (ih->ih_event == NULL)
		return (ENXIO);

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

	PIC_MASK(ih->ih_pic->ic_dev, ih->ih_irq);
}

static void
arm_unmask_irq(void *arg)
{
	struct arm_intr_handler *ih = (struct arm_intr_handler *)arg;

	PIC_UNMASK(ih->ih_pic->ic_dev, ih->ih_irq);
}

static void
arm_eoi(void *arg)
{
	struct arm_intr_handler *ih = (struct arm_intr_handler *)arg;

	PIC_EOI(ih->ih_pic->ic_dev, ih->ih_irq);
}

int
arm_intrng_config_irq(int irq, enum intr_trigger trig, enum intr_polarity pol)
{
	struct arm_intr_controller *pic;
	struct arm_intr_handler *ih;

	pic = &arm_pics[IRQ_PIC_IDX(irq)];
	ih = arm_lookup_intr_handler(pic->ic_dev, IRQ_VECTOR_IDX(irq));

	if (ih == NULL)
		return (ENXIO);

	return PIC_CONFIG(pic->ic_dev, ih->ih_irq, trig, pol);
}

#ifdef SMP
void
arm_init_secondary_ic(void)
{

	KASSERT(arm_ipi_pic != NULL, ("no IPI PIC attached"));
	PIC_INIT_SECONDARY(arm_ipi_pic->ic_dev);
}

void
pic_ipi_send(cpuset_t cpus, u_int ipi)
{

	KASSERT(arm_ipi_pic != NULL, ("no IPI PIC attached"));
	PIC_IPI_SEND(arm_ipi_pic->ic_dev, cpus, ipi);
}

void
pic_ipi_clear(int ipi)
{
	
	KASSERT(arm_ipi_pic != NULL, ("no IPI PIC attached"));
	PIC_IPI_CLEAR(arm_ipi_pic->ic_dev, ipi);
}

int
pic_ipi_read(int ipi)
{

	KASSERT(arm_ipi_pic != NULL, ("no IPI PIC attached"));
	return (PIC_IPI_READ(arm_ipi_pic->ic_dev, ipi));
}

void
arm_unmask_ipi(int ipi)
{

	KASSERT(arm_ipi_pic != NULL, ("no IPI PIC attached"));
	PIC_UNMASK(arm_ipi_pic->ic_dev, ipi);
}

void
arm_mask_ipi(int ipi)
{

	KASSERT(arm_ipi_pic != NULL, ("no IPI PIC attached"));
	PIC_MASK(arm_ipi_pic->ic_dev, ipi);
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

