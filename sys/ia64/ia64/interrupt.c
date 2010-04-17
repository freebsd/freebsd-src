/* $FreeBSD$ */
/* $NetBSD: interrupt.c,v 1.23 1998/02/24 07:38:01 thorpej Exp $ */

/*-
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Keith Bostic, Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*-
 * Additional Copyright (c) 1997 by Matthew Jacob for NASA/Ames Research Center.
 * Redistribute and modify at will, leaving only this additional copyright
 * notice.
 */

#include "opt_ddb.h"

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/malloc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <machine/cpu.h>
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/intrcnt.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/reg.h>
#include <machine/smp.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

struct ia64_intr {
	struct intr_event *event;	/* interrupt event */
	volatile long *cntp;		/* interrupt counter */
	struct sapic *sapic;
	u_int	irq;
};

ia64_ihtype *ia64_handler[IA64_NXIVS];

static enum ia64_xiv_use ia64_xiv[IA64_NXIVS];
static struct ia64_intr *ia64_intrs[IA64_NXIVS];

static ia64_ihtype ia64_ih_invalid;
static ia64_ihtype ia64_ih_irq;

void
ia64_xiv_init(void)
{
	u_int xiv;

	for (xiv = 0; xiv < IA64_NXIVS; xiv++) {
		ia64_handler[xiv] = ia64_ih_invalid;
		ia64_xiv[xiv] = IA64_XIV_FREE;
		ia64_intrs[xiv] = NULL;
	}
	(void)ia64_xiv_reserve(15, IA64_XIV_ARCH, NULL);
}

int
ia64_xiv_free(u_int xiv, enum ia64_xiv_use what)
{

	if (xiv >= IA64_NXIVS)
		return (EINVAL);
	if (what == IA64_XIV_FREE || what == IA64_XIV_ARCH)
		return (EINVAL);
	if (ia64_xiv[xiv] != what)
		return (ENXIO);
	ia64_xiv[xiv] = IA64_XIV_FREE;
	ia64_handler[xiv] = ia64_ih_invalid;
	return (0);
}

int
ia64_xiv_reserve(u_int xiv, enum ia64_xiv_use what, ia64_ihtype ih)
{

	if (xiv >= IA64_NXIVS)
		return (EINVAL);
	if (what == IA64_XIV_FREE)
		return (EINVAL);
	if (ia64_xiv[xiv] != IA64_XIV_FREE)
		return (EBUSY);
	ia64_xiv[xiv] = what;
	ia64_handler[xiv] = (ih == NULL) ? ia64_ih_invalid: ih;
	if (bootverbose)
		printf("XIV %u: use=%u, IH=%p\n", xiv, what, ih);
	return (0);
}

u_int
ia64_xiv_alloc(u_int prio, enum ia64_xiv_use what, ia64_ihtype ih)
{
	u_int hwprio;
	u_int xiv0, xiv;

	hwprio = prio >> 2;
	if (hwprio > IA64_MAX_HWPRIO)
		hwprio = IA64_MAX_HWPRIO;

	xiv0 = IA64_NXIVS - (hwprio + 1) * 16;

	KASSERT(xiv0 >= IA64_MIN_XIV, ("%s: min XIV", __func__));
	KASSERT(xiv0 < IA64_NXIVS, ("%s: max XIV", __func__));

	xiv = xiv0;
	while (xiv < IA64_NXIVS && ia64_xiv_reserve(xiv, what, ih))
		xiv++;

	if (xiv < IA64_NXIVS)
		return (xiv);

	xiv = xiv0;
	while (xiv >= IA64_MIN_XIV && ia64_xiv_reserve(xiv, what, ih))
		xiv--;

	return ((xiv >= IA64_MIN_XIV) ? xiv : 0);
}

static void
ia64_intr_eoi(void *arg)
{
	u_int xiv = (uintptr_t)arg;
	struct ia64_intr *i;

	i = ia64_intrs[xiv];
	KASSERT(i != NULL, ("%s", __func__));
	sapic_eoi(i->sapic, xiv);
}

static void
ia64_intr_mask(void *arg)
{
	u_int xiv = (uintptr_t)arg;
	struct ia64_intr *i;

	i = ia64_intrs[xiv];
	KASSERT(i != NULL, ("%s", __func__));
	sapic_mask(i->sapic, i->irq);
	sapic_eoi(i->sapic, xiv);
}

static void
ia64_intr_unmask(void *arg)
{
	u_int xiv = (uintptr_t)arg;
	struct ia64_intr *i;

	i = ia64_intrs[xiv];
	KASSERT(i != NULL, ("%s", __func__));
	sapic_unmask(i->sapic, i->irq);
}

int
ia64_setup_intr(const char *name, int irq, driver_filter_t filter,
    driver_intr_t handler, void *arg, enum intr_type flags, void **cookiep)
{
	struct ia64_intr *i;
	struct sapic *sa;
	char *intrname;
	u_int prio, xiv;
	int error;

	prio = intr_priority(flags);
	if (prio > PRI_MAX_ITHD)
		return (EINVAL);

	/* XXX lock */

	/* Get the I/O SAPIC and XIV that corresponds to the IRQ. */
	sa = sapic_lookup(irq, &xiv);
	if (sa == NULL) {
		/* XXX unlock */
		printf("XXX %s: no I/O SAPIC -- can't setup IRQ %u\n",
		    __func__, irq);
		return (0);
	}

	if (xiv == 0) {
		/* XXX unlock */
		i = malloc(sizeof(struct ia64_intr), M_DEVBUF,
		    M_ZERO | M_WAITOK);
		/* XXX lock */
		sa = sapic_lookup(irq, &xiv);
		KASSERT(sa != NULL, ("sapic_lookup"));
		if (xiv != 0)
			free(i, M_DEVBUF);
	}

	/*
	 * If the IRQ has no XIV assigned to it yet, assign one based
	 * on the priority.
	 */
	if (xiv == 0) {
		xiv = ia64_xiv_alloc(prio, IA64_XIV_IRQ, ia64_ih_irq);
		if (xiv == 0) {
			/* XXX unlock */
			free(i, M_DEVBUF);
			return (ENOSPC);
		}

		error = intr_event_create(&i->event, (void *)(uintptr_t)xiv,
		    0, irq, ia64_intr_mask, ia64_intr_unmask, ia64_intr_eoi,
		    NULL, "irq%u:", irq);
		if (error) {
			ia64_xiv_free(xiv, IA64_XIV_IRQ);
			/* XXX unlock */
			free(i, M_DEVBUF);
			return (error);
		}

		i->sapic = sa;
		i->irq = irq;
		i->cntp = intrcnt + xiv;
		ia64_intrs[xiv] = i;

		/* XXX unlock */

		sapic_enable(sa, irq, xiv);

		if (name != NULL && *name != '\0') {
			/* XXX needs abstraction. Too error prone. */
			intrname = intrnames + xiv * INTRNAME_LEN;
			memset(intrname, ' ', INTRNAME_LEN - 1);
			bcopy(name, intrname, strlen(name));
		}
	} else {
		i = ia64_intrs[xiv];
		/* XXX unlock */
	}

	KASSERT(i != NULL, ("XIV mapping bug"));

	error = intr_event_add_handler(i->event, name, filter, handler, arg,
	    prio, flags, cookiep);
	return (error);
}

int
ia64_teardown_intr(void *cookie)
{

	return (intr_event_remove_handler(cookie));
}

void
ia64_bind_intr(void)
{
	struct ia64_intr *i;
	struct pcpu *pc;
	u_int xiv;
	int cpu;

	cpu = MAXCPU;
	for (xiv = IA64_NXIVS - 1; xiv >= IA64_MIN_XIV; xiv--) {
		if (ia64_xiv[xiv] != IA64_XIV_IRQ)
			continue;
		i = ia64_intrs[xiv];
		do {
			cpu = (cpu == 0) ? MAXCPU - 1 : cpu - 1;
			pc = cpuid_to_pcpu[cpu];
		} while (pc == NULL || !pc->pc_md.awake);
		sapic_bind_intr(i->irq, pc);
	}
}

/*
 * Interrupt handlers.
 */

void
ia64_handle_intr(struct trapframe *tf)
{
	struct thread *td;
	u_int xiv;

	td = curthread;
	ia64_set_fpsr(IA64_FPSR_DEFAULT);
	PCPU_INC(cnt.v_intr);

	xiv = ia64_get_ivr();
	ia64_srlz_d();
	if (xiv == 15) {
		PCPU_INC(md.stats.pcs_nstrays);
		goto out;
	}

	critical_enter();

	do {
		CTR2(KTR_INTR, "INTR: ITC=%u, XIV=%u",
		    (u_int)tf->tf_special.ifa, xiv);
		(ia64_handler[xiv])(td, xiv, tf);
		ia64_set_eoi(0);
		ia64_srlz_d();
		xiv = ia64_get_ivr();
		ia64_srlz_d();
	} while (xiv != 15);

	critical_exit();

 out:
	if (TRAPF_USERMODE(tf)) {
		while (td->td_flags & (TDF_ASTPENDING|TDF_NEEDRESCHED)) {
			ia64_enable_intr();
			ast(tf);
			ia64_disable_intr();
		}
	}
}

static u_int
ia64_ih_invalid(struct thread *td, u_int xiv, struct trapframe *tf)
{

	panic("invalid XIV: %u", xiv);
	return (0);
}

static u_int
ia64_ih_irq(struct thread *td, u_int xiv, struct trapframe *tf)
{
	struct ia64_intr *i;
	struct intr_event *ie;			/* our interrupt event */

	PCPU_INC(md.stats.pcs_nhwints);

	/* Find the interrupt thread for this XIV. */
	i = ia64_intrs[xiv];
	KASSERT(i != NULL, ("%s: unassigned XIV", __func__));

	(*i->cntp)++;

	ie = i->event;
	KASSERT(ie != NULL, ("%s: interrupt without event", __func__));

	if (intr_event_handle(ie, tf) != 0) {
		ia64_intr_mask((void *)(uintptr_t)xiv);
		log(LOG_ERR, "stray irq%u\n", i->irq);
	}

	return (0);
}

#ifdef DDB

static void
db_print_xiv(u_int xiv, int always)
{
	struct ia64_intr *i;

	i = ia64_intrs[xiv];
	if (i != NULL) {
		db_printf("XIV %u (%p): ", xiv, i);
		sapic_print(i->sapic, i->irq);
	} else if (always)
		db_printf("XIV %u: unassigned\n", xiv);
}

DB_SHOW_COMMAND(xiv, db_show_xiv)
{
	u_int xiv;

	if (have_addr) {
		xiv = ((addr >> 4) % 16) * 10 + (addr % 16);
		if (xiv >= IA64_NXIVS)
			db_printf("error: XIV %u not in range [0..%u]\n",
			    xiv, IA64_NXIVS - 1);
		else
			db_print_xiv(xiv, 1);
	} else {
		for (xiv = 0; xiv < IA64_NXIVS; xiv++)
			db_print_xiv(xiv, 0);
	}
}

#endif
