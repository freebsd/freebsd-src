/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * Copyright (c) 2002 Benno Rice.
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
 * 
 * This code is derived from software contributed by
 * William Jolitz (Berkeley) and Benno Rice.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	form: src/sys/powerpc/powerpc/intr_machdep.c, r271712 2014/09/17
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/cpuset.h>
#include <sys/interrupt.h>
#include <sys/queue.h>
#include <sys/smp.h>

#include <machine/cpufunc.h>
#include <machine/intr.h>

#ifdef SMP
#include <machine/smp.h>
#endif

#include "pic_if.h"

#define	MAX_STRAY_LOG	5
#define	INTRNAME_LEN	(MAXCOMLEN + 1)

#define	NIRQS	1024	/* Maximum number of interrupts in the system */

static MALLOC_DEFINE(M_INTR, "intr", "Interrupt Services");

/*
 * Linked list of interrupts that have been set-up.
 * Each element holds the interrupt description
 * and has to be allocated and freed dynamically.
 */
static SLIST_HEAD(, arm64_intr_entry) irq_slist_head =
    SLIST_HEAD_INITIALIZER(irq_slist_head);

struct arm64_intr_entry {
	SLIST_ENTRY(arm64_intr_entry) entries;
	struct intr_event	*i_event;

	enum intr_trigger	i_trig;
	enum intr_polarity	i_pol;

	u_int			i_hw_irq;	/* Physical interrupt number */
	u_int			i_cntidx;	/* Index in intrcnt table */
	u_int			i_handlers;	/* Allocated handlers */
	u_long			*i_cntp;	/* Interrupt hit counter */
};

/* Counts and names for statistics - see sys/sys/interrupt.h */
/* Tables are indexed by i_cntidx */
u_long intrcnt[NIRQS];
char intrnames[NIRQS * INTRNAME_LEN];
size_t sintrcnt = sizeof(intrcnt);
size_t sintrnames = sizeof(intrnames);

static u_int intrcntidx;	/* Current index into intrcnt table */
static u_int arm64_nintrs;	/* Max interrupts number of the root PIC */
static u_int arm64_nstray;	/* Number of received stray interrupts */
static device_t root_pic;	/* PIC device for all incoming interrupts */
static device_t msi_pic;	/* Device which handles MSI/MSI-X interrupts */
static struct mtx intr_list_lock;

static void
intr_init(void *dummy __unused)
{

	mtx_init(&intr_list_lock, "intr sources lock", NULL, MTX_DEF);
}
SYSINIT(intr_init, SI_SUB_INTR, SI_ORDER_FIRST, intr_init, NULL);

/*
 * Helper routines.
 */

/* Set interrupt name for statistics */
static void
intrcnt_setname(const char *name, u_int idx)
{

	snprintf(&intrnames[idx * INTRNAME_LEN], INTRNAME_LEN, "%-*s",
	    INTRNAME_LEN - 1, name);
}

/*
 * Get intr structure for the given interrupt number.
 * Allocate one if this is the first time.
 * (Similar to ppc's intr_lookup() but without actual
 * lookup since irq number is an index in arm64_intrs[]).
 */
static struct arm64_intr_entry *
intr_acquire(u_int hw_irq)
{
	struct arm64_intr_entry *intr;

	mtx_lock(&intr_list_lock);

	SLIST_FOREACH(intr, &irq_slist_head, entries) {
		if (intr->i_hw_irq == hw_irq) {
			break;
		}
	}
	if (intr != NULL)
		goto out;

	/* Do not alloc another intr when max number of IRQs has been reached */
	if (intrcntidx >= NIRQS)
		goto out;

	intr = malloc(sizeof(*intr), M_INTR, M_NOWAIT);
	if (intr == NULL)
		goto out;

	intr->i_event = NULL;
	intr->i_handlers = 0;
	intr->i_trig = INTR_TRIGGER_CONFORM;
	intr->i_pol = INTR_POLARITY_CONFORM;
	intr->i_cntidx = atomic_fetchadd_int(&intrcntidx, 1);
	intr->i_cntp = &intrcnt[intr->i_cntidx];
	intr->i_hw_irq = hw_irq;
	SLIST_INSERT_HEAD(&irq_slist_head, intr, entries);
out:
	mtx_unlock(&intr_list_lock);
	return intr;
}

static void
intr_pre_ithread(void *arg)
{
	struct arm64_intr_entry *intr = arg;

	PIC_PRE_ITHREAD(root_pic, intr->i_hw_irq);
}

static void
intr_post_ithread(void *arg)
{
	struct arm64_intr_entry *intr = arg;

	PIC_POST_ITHREAD(root_pic, intr->i_hw_irq);
}

static void
intr_post_filter(void *arg)
{
	struct arm64_intr_entry *intr = arg;

	PIC_POST_FILTER(root_pic, intr->i_hw_irq);
}

/*
 * Register PIC driver.
 * This is intended to be called by the very first PIC driver
 * at the end of the successful attach.
 * Note that during boot this can be called after first references
 * to bus_setup_intr() so it is required to not use root_pic if it
 * is not 100% safe.
 */
void
arm_register_root_pic(device_t dev, u_int nirq)
{

	KASSERT(root_pic == NULL, ("Unable to set the pic twice"));
	KASSERT(nirq <= NIRQS, ("PIC is trying to handle too many IRQs"));

	arm64_nintrs = NIRQS; /* Number of IRQs limited only by array size */
	root_pic = dev;
}

/* Register device which allocates MSI interrupts */
void
arm_register_msi_pic(device_t dev)
{

	KASSERT(msi_pic == NULL, ("Unable to set msi_pic twice"));
	msi_pic = dev;
}

int
arm_alloc_msi(device_t pci_dev, int count, int *irqs)
{

	return PIC_ALLOC_MSI(msi_pic, pci_dev, count, irqs);
}

int
arm_release_msi(device_t pci_dev, int count, int *irqs)
{

	return PIC_RELEASE_MSI(msi_pic, pci_dev, count, irqs);
}

int
arm_map_msi(device_t pci_dev, int irq, uint64_t *addr, uint32_t *data)
{

	return PIC_MAP_MSI(msi_pic, pci_dev, irq, addr, data);
}

int
arm_alloc_msix(device_t pci_dev, int *irq)
{

	return PIC_ALLOC_MSIX(msi_pic, pci_dev, irq);
}

int
arm_release_msix(device_t pci_dev, int irq)
{

	return PIC_RELEASE_MSIX(msi_pic, pci_dev, irq);
}


int
arm_map_msix(device_t pci_dev, int irq, uint64_t *addr, uint32_t *data)
{

	return PIC_MAP_MSIX(msi_pic, pci_dev, irq, addr, data);
}

/*
 * Finalize interrupts bring-up (should be called from configure_final()).
 * Enables all interrupts registered by bus_setup_intr() during boot
 * as well as unlocks interrups reception on primary CPU.
 */
int
arm_enable_intr(void)
{
	struct arm64_intr_entry *intr;

	if (root_pic == NULL)
		panic("Cannot enable interrupts. No PIC configured");

	/*
	 * Iterate through all possible interrupts and perform
	 * configuration if the interrupt is registered.
	 */
	SLIST_FOREACH(intr, &irq_slist_head, entries) {
		/*
		 * XXX: In case we allowed to set up interrupt whose number
		 *	exceeds maximum number of interrupts for the root PIC
		 *	disable it and print proper error message.
		 *
		 *	This can happen only when calling bus_setup_intr()
		 *	before the interrupt controller is attached.
		 */
		if (intr->i_cntidx >= arm64_nintrs) {
			/* Better fail when IVARIANTS enabled */
			KASSERT(0, ("%s: Interrupt %u cannot be handled by the "
			    "registered PIC. Max interrupt number: %u", __func__,
			    intr->i_cntidx, arm64_nintrs - 1));
			/* Print message and disable otherwise */
			printf("ERROR: Cannot enable irq %u. Disabling.\n",
			    intr->i_cntidx);
			PIC_MASK(root_pic, intr->i_hw_irq);
		}

		if (intr->i_trig != INTR_TRIGGER_CONFORM ||
		    intr->i_pol != INTR_POLARITY_CONFORM) {
			PIC_CONFIG(root_pic, intr->i_hw_irq,
			    intr->i_trig, intr->i_pol);
		}

		if (intr->i_handlers > 0)
			PIC_UNMASK(root_pic, intr->i_hw_irq);

	}
	/* Enable interrupt reception on this CPU */
	intr_enable();

	return (0);
}

int
arm_setup_intr(const char *name, driver_filter_t *filt, driver_intr_t handler,
    void *arg, u_int hw_irq, enum intr_type flags, void **cookiep)
{
	struct arm64_intr_entry *intr;
	int error;

	intr = intr_acquire(hw_irq);
	if (intr == NULL)
		return (ENOMEM);

	/*
	 * Watch out for interrupts' numbers.
	 * If this is a system boot then don't allow to overfill interrupts
	 * table (the interrupts will be deconfigured in arm_enable_intr()).
	 */
	if (intr->i_cntidx >= NIRQS)
		return (EINVAL);

	if (intr->i_event == NULL) {
		error = intr_event_create(&intr->i_event, (void *)intr, 0,
		    hw_irq, intr_pre_ithread, intr_post_ithread,
		    intr_post_filter, NULL, "irq%u", hw_irq);
		if (error)
			return (error);
	}

	error = intr_event_add_handler(intr->i_event, name, filt, handler, arg,
	    intr_priority(flags), flags, cookiep);

	if (!error) {
		mtx_lock(&intr_list_lock);
		intrcnt_setname(intr->i_event->ie_fullname, intr->i_cntidx);
		intr->i_handlers++;

		if (!cold && intr->i_handlers == 1) {
			if (intr->i_trig != INTR_TRIGGER_CONFORM ||
			    intr->i_pol != INTR_POLARITY_CONFORM) {
				PIC_CONFIG(root_pic, intr->i_hw_irq, intr->i_trig,
				    intr->i_pol);
			}

			PIC_UNMASK(root_pic, intr->i_hw_irq);
		}
		mtx_unlock(&intr_list_lock);
	}

	return (error);
}

int
arm_teardown_intr(void *cookie)
{
	struct arm64_intr_entry *intr;
	int error;

	intr = intr_handler_source(cookie);
	error = intr_event_remove_handler(cookie);
	if (!error) {
		mtx_lock(&intr_list_lock);
		intr->i_handlers--;
		if (intr->i_handlers == 0)
			PIC_MASK(root_pic, intr->i_hw_irq);
		intrcnt_setname(intr->i_event->ie_fullname, intr->i_cntidx);
		mtx_unlock(&intr_list_lock);
	}

	return (error);
}

int
arm_config_intr(u_int hw_irq, enum intr_trigger trig, enum intr_polarity pol)
{
	struct arm64_intr_entry *intr;

	intr = intr_acquire(hw_irq);
	if (intr == NULL)
		return (ENOMEM);

	intr->i_trig = trig;
	intr->i_pol = pol;

	if (!cold && root_pic != NULL)
		PIC_CONFIG(root_pic, intr->i_hw_irq, trig, pol);

	return (0);
}

void
arm_dispatch_intr(u_int hw_irq, struct trapframe *tf)
{
	struct arm64_intr_entry *intr;

	SLIST_FOREACH(intr, &irq_slist_head, entries) {
		if (intr->i_hw_irq == hw_irq) {
			break;
		}
	}

	if (intr == NULL)
		goto stray;

	(*intr->i_cntp)++;

	if (!intr_event_handle(intr->i_event, tf))
		return;

stray:
	if (arm64_nstray < MAX_STRAY_LOG) {
		arm64_nstray++;
		printf("Stray IRQ %u\n", hw_irq);
		if (arm64_nstray >= MAX_STRAY_LOG) {
			printf("Got %d stray IRQs. Not logging anymore.\n",
			    MAX_STRAY_LOG);
		}
	}

	if (intr != NULL)
		PIC_MASK(root_pic, intr->i_hw_irq);
#ifdef HWPMC_HOOKS
	if (pmc_hook && (PCPU_GET(curthread)->td_pflags & TDP_CALLCHAIN))
		pmc_hook(PCPU_GET(curthread), PMC_FN_USER_CALLCHAIN, tf);
#endif
}

void
arm_cpu_intr(struct trapframe *tf)
{

	critical_enter();
	PIC_DISPATCH(root_pic, tf);
	critical_exit();
}

#ifdef SMP
void
arm_setup_ipihandler(driver_filter_t *filt, u_int ipi)
{

	/* ARM64TODO: The hard coded 16 will be fixed with am_intrng */
	arm_setup_intr("ipi", filt, NULL, (void *)((uintptr_t)ipi | 1<<16), ipi + 16,
	    INTR_TYPE_MISC | INTR_EXCL, NULL);
	arm_unmask_ipi(ipi);
}

void
arm_unmask_ipi(u_int ipi)
{
	PIC_UNMASK(root_pic, ipi + 16);
}

void
arm_init_secondary(void)
{

	PIC_INIT_SECONDARY(root_pic);
}

/* Sending IPI */
void
ipi_all_but_self(u_int ipi)
{
	cpuset_t other_cpus;

	other_cpus = all_cpus;
	CPU_CLR(PCPU_GET(cpuid), &other_cpus);

	/* ARM64TODO: This will be fixed with arm_intrng */
	ipi += 16;

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	PIC_IPI_SEND(root_pic, other_cpus, ipi);
}

void
ipi_cpu(int cpu, u_int ipi)
{
	cpuset_t cpus;

	CPU_ZERO(&cpus);
	CPU_SET(cpu, &cpus);

	/* ARM64TODO: This will be fixed with arm_intrng */
	ipi += 16;

	CTR2(KTR_SMP, "ipi_cpu: cpu: %d, ipi: %x", cpu, ipi);
	PIC_IPI_SEND(root_pic, cpus, ipi);
}

void
ipi_selected(cpuset_t cpus, u_int ipi)
{

	/* ARM64TODO: This will be fixed with arm_intrng */
	ipi += 16;

	CTR1(KTR_SMP, "ipi_selected: ipi: %x", ipi);
	PIC_IPI_SEND(root_pic, cpus, ipi);
}

#endif
