/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)isa.c	7.2 (Berkeley) 5/13/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_isa.h"

#include <sys/param.h>
#include <sys/bus.h> 
#include <sys/errno.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/unistd.h>

#include <machine/md_var.h>
#include <machine/segments.h>

#include <amd64/isa/isa.h>
#include <amd64/isa/icu.h>

#ifdef DEV_ISA
#include <isa/isavar.h>
#endif
#include <amd64/isa/intr_machdep.h>
#include <sys/interrupt.h>

/*
 * Per-interrupt data.
 */
u_long	*intr_countp[ICU_LEN];		/* pointers to interrupt counters */
driver_intr_t	*intr_handler[ICU_LEN];	/* first level interrupt handler */
struct	 ithd *ithds[ICU_LEN];		/* real interrupt handler */
void	*intr_unit[ICU_LEN];

static struct	mtx ithds_table_lock;	/* protect the ithds table */

static inthand_t *fastintr[ICU_LEN] = {
	IDTVEC(fastintr0), IDTVEC(fastintr1),
	IDTVEC(fastintr2), IDTVEC(fastintr3),
	IDTVEC(fastintr4), IDTVEC(fastintr5),
	IDTVEC(fastintr6), IDTVEC(fastintr7),
	IDTVEC(fastintr8), IDTVEC(fastintr9),
	IDTVEC(fastintr10), IDTVEC(fastintr11),
	IDTVEC(fastintr12), IDTVEC(fastintr13),
	IDTVEC(fastintr14), IDTVEC(fastintr15),
};

static inthand_t *slowintr[ICU_LEN] = {
	IDTVEC(intr0), IDTVEC(intr1), IDTVEC(intr2), IDTVEC(intr3),
	IDTVEC(intr4), IDTVEC(intr5), IDTVEC(intr6), IDTVEC(intr7),
	IDTVEC(intr8), IDTVEC(intr9), IDTVEC(intr10), IDTVEC(intr11),
	IDTVEC(intr12), IDTVEC(intr13), IDTVEC(intr14), IDTVEC(intr15),
};

static driver_intr_t isa_strayintr;

static void	ithds_init(void *dummy);
static void	ithread_enable(int vector);
static void	ithread_disable(int vector);
static void	init_i8259(void);

#define NMI_PARITY (1 << 7)
#define NMI_IOCHAN (1 << 6)
#define ENMI_WATCHDOG (1 << 7)
#define ENMI_BUSTIMER (1 << 6)
#define ENMI_IOSTATUS (1 << 5)

#ifdef DEV_ISA
/*
 * Bus attachment for the ISA PIC.
 */
static struct isa_pnp_id atpic_ids[] = {
	{ 0x0000d041 /* PNP0000 */, "AT interrupt controller" },
	{ 0 }
};

static int
atpic_probe(device_t dev)
{
	int result;
	
	if ((result = ISA_PNP_PROBE(device_get_parent(dev), dev, atpic_ids)) <= 0)
		device_quiet(dev);
	return(result);
}

/*
 * The generic ISA attachment code will handle allocating any other resources
 * that we don't explicitly claim here.
 */
static int
atpic_attach(device_t dev)
{
	return(0);
}

static device_method_t atpic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		atpic_probe),
	DEVMETHOD(device_attach,	atpic_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	{ 0, 0 }
};

static driver_t atpic_driver = {
	"atpic",
	atpic_methods,
	1,		/* no softc */
};

static devclass_t atpic_devclass;

DRIVER_MODULE(atpic, isa, atpic_driver, atpic_devclass, 0, 0);
DRIVER_MODULE(atpic, acpi, atpic_driver, atpic_devclass, 0, 0);
#endif /* DEV_ISA */

/*
 * Handle a NMI, possibly a machine check.
 * return true to panic system, false to ignore.
 */
int
isa_nmi(cd)
	int cd;
{
	int retval = 0;
	int isa_port = inb(0x61);

	log(LOG_CRIT, "NMI ISA STATUS 0x%02x", isa_port);
	
	if (isa_port & NMI_PARITY) {
		log(LOG_CRIT, "RAM parity error, likely hardware failure.");
		retval = 1;
	}

	if (isa_port & NMI_IOCHAN) {
		log(LOG_CRIT, "I/O channel check, likely hardware failure.");
		retval = 1;
	}

	return (retval);
}

/*
 *  ICU reinitialize when ICU configuration has lost.
 */
void icu_reinit()
{
	int i;
	register_t crit;

	crit = intr_disable();
	mtx_lock_spin(&icu_lock);
	init_i8259();
	for(i=0;i<ICU_LEN;i++)
		if(intr_handler[i] != isa_strayintr)
			INTREN(1<<i);
	mtx_unlock_spin(&icu_lock);
	intr_restore(crit);
}

/*
 * Create a default interrupt table to avoid problems caused by
 * spurious interrupts during configuration of kernel, then setup
 * interrupt control unit.
 */
void
isa_defaultirq()
{
	int i;
	register_t crit;

	/* icu vectors */
	for (i = 0; i < ICU_LEN; i++)
		icu_unset(i, (driver_intr_t *)NULL);
	crit = intr_disable();
	mtx_lock_spin(&icu_lock);
	init_i8259();
	mtx_unlock_spin(&icu_lock);
	intr_restore(crit);
}


/* 
 *initialize 8259's
 */
static void init_i8259()
{

	outb(IO_ICU1, ICW1_RESET | ICW1_IC4);	/* reset; program device, four bytes */

	outb(IO_ICU1+ICU_IMR_OFFSET, NRSVIDT);	/* starting at this vector index */
	outb(IO_ICU1+ICU_IMR_OFFSET, IRQ_SLAVE);/* slave on line 2 */
	outb(IO_ICU1+ICU_IMR_OFFSET, ICW4_8086);/* 8086 mode */
	outb(IO_ICU1+ICU_IMR_OFFSET, 0xff);	/* leave interrupts masked */
	outb(IO_ICU1, OCW3_SEL | OCW3_RR);	/* default to IRR on read */
	outb(IO_ICU1, ICU_SETPRI | 0x2);/* pri order 3-7, 0-2 (com2 first) */

	outb(IO_ICU2, ICW1_RESET | ICW1_IC4);	/* reset; program device, four bytes */

	outb(IO_ICU2+ICU_IMR_OFFSET, NRSVIDT+8); /* staring at this vector index */
	outb(IO_ICU2+ICU_IMR_OFFSET, ICU_SLAVEID); /* my slave id is 2 */
	outb(IO_ICU2+ICU_IMR_OFFSET, ICW4_8086); /* 8086 mode */
	outb(IO_ICU2+ICU_IMR_OFFSET, 0xff);     /* leave interrupts masked */
	outb(IO_ICU2, OCW3_SEL | OCW3_RR);	/* default to IRR on read */
}

/*
 * Caught a stray interrupt, notify
 */
static int isaglitch7;
static int isaglitch15;

static void
isa_strayintr(vcookiep)
	void *vcookiep;
{
	int intr = (void **)vcookiep - &intr_unit[0];
	int isr;

	/* Determine if it is a stray interrupt or simply a glitch */
	if (intr == 7) {
		outb(IO_ICU1, OCW3_SEL);	/* select IS register */
		isr = inb(IO_ICU1);
		outb(IO_ICU1, OCW3_SEL | OCW3_RR | OCW3_RIS); /* reselect IIR */
		if ((isr & 0x80) == 0) {
			isaglitch7++;
			return;
		}
	}
	if (intr == 15) {
		outb(IO_ICU2, OCW3_SEL);	/* select IS register */
		isr = inb(IO_ICU2);
		outb(IO_ICU2, OCW3_SEL | OCW3_RR); /* reselect IIR */
		if ((isr & 0x80) == 0) {
			isaglitch15++;
			return;
		}
	}
	if (intrcnt[1 + intr] <= 5)
		log(LOG_ERR, "stray irq %d\n", intr);
	if (intrcnt[1 + intr] == 5)
		log(LOG_CRIT,
		    "too many stray irq %d's; not logging any more\n", intr);
}

#ifdef DEV_ISA
/*
 * Return a bitmap of the current interrupt requests.  This is 8259-specific
 * and is only suitable for use at probe time.
 */
intrmask_t
isa_irq_pending()
{
	u_char irr1;
	u_char irr2;

	irr1 = inb(IO_ICU1);
	irr2 = inb(IO_ICU2);
	return ((irr2 << 8) | irr1);
}
#endif

/*
 * Update intrnames array with the specified name.  This is used by
 * vmstat(8) and the like.
 */
static void
update_intrname(int intr, const char *name)
{
	char buf[32];
	char *cp;
	int name_index, off, strayintr;

	/*
	 * Initialise strings for bitbucket and stray interrupt counters.
	 * These have statically allocated indices 0 and 1 through ICU_LEN.
	 */
	if (intrnames[0] == '\0') {
		off = sprintf(intrnames, "???") + 1;
		for (strayintr = 0; strayintr < ICU_LEN; strayintr++)
			off += sprintf(intrnames + off, "stray irq%d",
			    strayintr) + 1;
	}

	if (name == NULL)
		name = "???";
	if (snprintf(buf, sizeof(buf), "%s irq%d", name, intr) >= sizeof(buf))
		goto use_bitbucket;

	/*
	 * Search for `buf' in `intrnames'.  In the usual case when it is
	 * not found, append it to the end if there is enough space (the \0
	 * terminator for the previous string, if any, becomes a separator).
	 */
	for (cp = intrnames, name_index = 0;
	    cp != eintrnames && name_index < NR_INTRNAMES;
	    cp += strlen(cp) + 1, name_index++) {
		if (*cp == '\0') {
			if (strlen(buf) >= eintrnames - cp)
				break;
			strcpy(cp, buf);
			goto found;
		}
		if (strcmp(cp, buf) == 0)
			goto found;
	}

use_bitbucket:
	printf("update_intrname: counting %s irq%d as %s\n", name, intr,
	    intrnames);
	name_index = 0;
found:
	intr_countp[intr] = &intrcnt[name_index];
}

int
icu_setup(int intr, driver_intr_t *handler, void *arg, int flags)
{
	register_t	crit;

	if ((u_int)intr >= ICU_LEN || intr == ICU_SLAVEID)
		return (EINVAL);
#if 0
	if (intr_handler[intr] != isa_strayintr)
		return (EBUSY);
#endif

	crit = intr_disable();
	mtx_lock_spin(&icu_lock);
	intr_handler[intr] = handler;
	intr_unit[intr] = arg;
	setidt(ICU_OFFSET + intr,
	       flags & INTR_FAST ? fastintr[intr] : slowintr[intr],
	       SDT_SYSIGT, SEL_KPL, 0);
	INTREN(1 << intr);
	mtx_unlock_spin(&icu_lock);
	intr_restore(crit);
	return (0);
}

/*
 * Dissociate an interrupt handler from an IRQ and set the handler to
 * the stray interrupt handler.  The 'handler' parameter is used only
 * for consistency checking.
 */
int
icu_unset(intr, handler)
	int	intr;
	driver_intr_t *handler;
{
	register_t crit;

	if ((u_int)intr >= ICU_LEN || handler != intr_handler[intr])
		return (EINVAL);

	crit = intr_disable();
	mtx_lock_spin(&icu_lock);
	INTRDIS(1 << intr);
	intr_countp[intr] = &intrcnt[1 + intr];
	intr_handler[intr] = isa_strayintr;
	intr_unit[intr] = &intr_unit[intr];
	setidt(ICU_OFFSET + intr, slowintr[intr], SDT_SYSIGT, SEL_KPL, 0);
	mtx_unlock_spin(&icu_lock);
	intr_restore(crit);
	return (0);
}

static void
ithds_init(void *dummy)
{

	mtx_init(&ithds_table_lock, "ithread table lock", NULL, MTX_SPIN);
}
SYSINIT(ithds_init, SI_SUB_INTR, SI_ORDER_SECOND, ithds_init, NULL);

static void
ithread_enable(int vector)
{
	register_t crit;

	crit = intr_disable();
	mtx_lock_spin(&icu_lock);
	INTREN(1 << vector);
	mtx_unlock_spin(&icu_lock);
	intr_restore(crit);
}

static void
ithread_disable(int vector)
{
	register_t crit;

	crit = intr_disable();
	mtx_lock_spin(&icu_lock);
	INTRDIS(1 << vector);
	mtx_unlock_spin(&icu_lock);
	intr_restore(crit);
}

int
inthand_add(const char *name, int irq, driver_intr_t handler, void *arg,
    enum intr_type flags, void **cookiep)
{
	struct ithd *ithd;		/* descriptor for the IRQ */
	int errcode = 0;
	int created_ithd = 0;

	/*
	 * Work around a race where more than one CPU may be registering
	 * handlers on the same IRQ at the same time.
	 */
	mtx_lock_spin(&ithds_table_lock);
	ithd = ithds[irq];
	mtx_unlock_spin(&ithds_table_lock);
	if (ithd == NULL) {
		errcode = ithread_create(&ithd, irq, 0, ithread_disable,
		    ithread_enable, "irq%d:", irq);
		if (errcode)
			return (errcode);
		mtx_lock_spin(&ithds_table_lock);
		if (ithds[irq] == NULL) {
			ithds[irq] = ithd;
			created_ithd++;
			mtx_unlock_spin(&ithds_table_lock);
		} else {
			struct ithd *orphan;

			orphan = ithd;
			ithd = ithds[irq];
			mtx_unlock_spin(&ithds_table_lock);
			ithread_destroy(orphan);
		}
	}

	errcode = ithread_add_handler(ithd, name, handler, arg,
	    ithread_priority(flags), flags, cookiep);
	
	if ((flags & INTR_FAST) == 0 || errcode)
		/*
		 * The interrupt process must be in place, but
		 * not necessarily schedulable, before we
		 * initialize the ICU, since it may cause an
		 * immediate interrupt.
		 */
		if (icu_setup(irq, sched_ithd, arg, flags) != 0)
			panic("inthand_add: Can't initialize ICU");

	if (errcode)
		return (errcode);
	
	if (flags & INTR_FAST) {
		errcode = icu_setup(irq, handler, arg, flags);
		if (errcode && bootverbose)
			printf("\tinthand_add(irq%d) failed, result=%d\n", 
			       irq, errcode);
		if (errcode)
			return (errcode);
	}

	update_intrname(irq, name);
	return (0);
}

/*
 * Deactivate and remove linked list the interrupt handler descriptor
 * data connected created by an earlier call of inthand_add(), then
 * adjust the interrupt masks if necessary.
 *
 * Return the memory held by the interrupt handler descriptor data
 * structure to the system.  First ensure the handler is not actively
 * in use.
 */
int
inthand_remove(void *cookie)
{

	return (ithread_remove_handler(cookie));
}
