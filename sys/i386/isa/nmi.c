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
 *	from: @(#)isa.c	7.2 (Berkeley) 5/13/91
 * $FreeBSD$
 */

#include "opt_auto_eoi.h"

#include "isa.h"

#include <sys/param.h>
#include <sys/bus.h>
#ifndef SMP
#include <machine/lock.h>
#endif
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/ipl.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/unistd.h>
#include <sys/errno.h>
#include <sys/interrupt.h>
#include <machine/md_var.h>
#include <machine/segments.h>
#include <sys/bus.h> 

#if defined(APIC_IO)
#include <machine/smp.h>
#include <machine/smptests.h>			/** FAST_HI */
#include <machine/resource.h>
#endif /* APIC_IO */
#ifdef PC98
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>
#include <pc98/pc98/epsonio.h>
#else
#include <i386/isa/isa.h>
#endif
#include <i386/isa/icu.h>

#if NISA > 0
#include <isa/isavar.h>
#endif
#include <i386/isa/intr_machdep.h>
#include <sys/interrupt.h>
#ifdef APIC_IO
#include <machine/clock.h>
#endif

#include "mca.h"
#if NMCA > 0
#include <i386/isa/mca_machdep.h>
#endif

/*
 * Per-interrupt data.
 */
u_long	*intr_countp[ICU_LEN];		/* pointers to interrupt counters */
driver_intr_t *intr_handler[ICU_LEN];	/* first level interrupt handler */
struct ithd *ithds[ICU_LEN];		/* real interrupt handler */
void	*intr_unit[ICU_LEN];

static inthand_t *fastintr[ICU_LEN] = {
	&IDTVEC(fastintr0), &IDTVEC(fastintr1),
	&IDTVEC(fastintr2), &IDTVEC(fastintr3),
	&IDTVEC(fastintr4), &IDTVEC(fastintr5),
	&IDTVEC(fastintr6), &IDTVEC(fastintr7),
	&IDTVEC(fastintr8), &IDTVEC(fastintr9),
	&IDTVEC(fastintr10), &IDTVEC(fastintr11),
	&IDTVEC(fastintr12), &IDTVEC(fastintr13),
	&IDTVEC(fastintr14), &IDTVEC(fastintr15),
#if defined(APIC_IO)
	&IDTVEC(fastintr16), &IDTVEC(fastintr17),
	&IDTVEC(fastintr18), &IDTVEC(fastintr19),
	&IDTVEC(fastintr20), &IDTVEC(fastintr21),
	&IDTVEC(fastintr22), &IDTVEC(fastintr23),
#endif /* APIC_IO */
};

static inthand_t *slowintr[ICU_LEN] = {
	&IDTVEC(intr0), &IDTVEC(intr1), &IDTVEC(intr2), &IDTVEC(intr3),
	&IDTVEC(intr4), &IDTVEC(intr5), &IDTVEC(intr6), &IDTVEC(intr7),
	&IDTVEC(intr8), &IDTVEC(intr9), &IDTVEC(intr10), &IDTVEC(intr11),
	&IDTVEC(intr12), &IDTVEC(intr13), &IDTVEC(intr14), &IDTVEC(intr15),
#if defined(APIC_IO)
	&IDTVEC(intr16), &IDTVEC(intr17), &IDTVEC(intr18), &IDTVEC(intr19),
	&IDTVEC(intr20), &IDTVEC(intr21), &IDTVEC(intr22), &IDTVEC(intr23),
#endif /* APIC_IO */
};

static driver_intr_t isa_strayintr;

#ifdef PC98
#define NMI_PARITY 0x04
#define NMI_EPARITY 0x02
#else
#define NMI_PARITY (1 << 7)
#define NMI_IOCHAN (1 << 6)
#define ENMI_WATCHDOG (1 << 7)
#define ENMI_BUSTIMER (1 << 6)
#define ENMI_IOSTATUS (1 << 5)
#endif

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
 * In the APIC_IO case we might be granted IRQ 2, as this is typically
 * consumed by chaining between the two PIC components.  If we're using
 * the APIC, however, this may not be the case, and as such we should
 * free the resource.  (XXX untested)
 *
 * The generic ISA attachment code will handle allocating any other resources
 * that we don't explicitly claim here.
 */
static int
atpic_attach(device_t dev)
{
#ifdef APIC_IO
	int		rid;
	struct resource *res;

	/* try to allocate our IRQ and then free it */
	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1, 0);
	if (res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, rid, res);
#endif
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

/*
 * Handle a NMI, possibly a machine check.
 * return true to panic system, false to ignore.
 */
int
isa_nmi(cd)
	int cd;
{
	int retval = 0;
#ifdef PC98
 	int port = inb(0x33);

	log(LOG_CRIT, "NMI PC98 port = %x\n", port);
	if (epson_machine_id == 0x20)
		epson_outb(0xc16, epson_inb(0xc16) | 0x1);
	if (port & NMI_PARITY) {
		log(LOG_CRIT, "BASE RAM parity error, likely hardware failure.");
		retval = 1;
	} else if (port & NMI_EPARITY) {
		log(LOG_CRIT, "EXTENDED RAM parity error, likely hardware failure.");
		retval = 1;
	} else {
		log(LOG_CRIT, "\nNMI Resume ??\n");
	}
#else /* IBM-PC */
	int isa_port = inb(0x61);
	int eisa_port = inb(0x461);

	log(LOG_CRIT, "NMI ISA %x, EISA %x\n", isa_port, eisa_port);
#if NMCA > 0
	if (MCA_system && mca_bus_nmi())
		return(0);
#endif
	
	if (isa_port & NMI_PARITY) {
		log(LOG_CRIT, "RAM parity error, likely hardware failure.");
		retval = 1;
	}

	if (isa_port & NMI_IOCHAN) {
		log(LOG_CRIT, "I/O channel check, likely hardware failure.");
		retval = 1;
	}

	/*
	 * On a real EISA machine, this will never happen.  However it can
	 * happen on ISA machines which implement XT style floating point
	 * error handling (very rare).  Save them from a meaningless panic.
	 */
	if (eisa_port == 0xff)
		return(retval);

	if (eisa_port & ENMI_WATCHDOG) {
		log(LOG_CRIT, "EISA watchdog timer expired, likely hardware failure.");
		retval = 1;
	}

	if (eisa_port & ENMI_BUSTIMER) {
		log(LOG_CRIT, "EISA bus timeout, likely hardware failure.");
		retval = 1;
	}

	if (eisa_port & ENMI_IOSTATUS) {
		log(LOG_CRIT, "EISA I/O port status error.");
		retval = 1;
	}
#endif
	return(retval);
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

	/* icu vectors */
	for (i = 0; i < ICU_LEN; i++)
		icu_unset(i, (driver_intr_t *)NULL);

	/* initialize 8259's */
#if NMCA > 0
	if (MCA_system)
		outb(IO_ICU1, 0x19);		/* reset; program device, four bytes */
	else
#endif
		outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */

	outb(IO_ICU1+ICU_IMR_OFFSET, NRSVIDT);	/* starting at this vector index */
	outb(IO_ICU1+ICU_IMR_OFFSET, IRQ_SLAVE);		/* slave on line 7 */
#ifdef PC98
#ifdef AUTO_EOI_1
	outb(IO_ICU1+ICU_IMR_OFFSET, 0x1f);		/* (master) auto EOI, 8086 mode */
#else
	outb(IO_ICU1+ICU_IMR_OFFSET, 0x1d);		/* (master) 8086 mode */
#endif
#else /* IBM-PC */
#ifdef AUTO_EOI_1
	outb(IO_ICU1+ICU_IMR_OFFSET, 2 | 1);		/* auto EOI, 8086 mode */
#else
	outb(IO_ICU1+ICU_IMR_OFFSET, 1);		/* 8086 mode */
#endif
#endif /* PC98 */
	outb(IO_ICU1+ICU_IMR_OFFSET, 0xff);		/* leave interrupts masked */
	outb(IO_ICU1, 0x0a);		/* default to IRR on read */
#ifndef PC98
	outb(IO_ICU1, 0xc0 | (3 - 1));	/* pri order 3-7, 0-2 (com2 first) */
#endif /* !PC98 */

#if NMCA > 0
	if (MCA_system)
		outb(IO_ICU2, 0x19);		/* reset; program device, four bytes */
	else
#endif
		outb(IO_ICU2, 0x11);		/* reset; program device, four bytes */

	outb(IO_ICU2+ICU_IMR_OFFSET, NRSVIDT+8); /* staring at this vector index */
	outb(IO_ICU2+ICU_IMR_OFFSET, ICU_SLAVEID);         /* my slave id is 7 */
#ifdef PC98
	outb(IO_ICU2+ICU_IMR_OFFSET,9);              /* 8086 mode */
#else /* IBM-PC */
#ifdef AUTO_EOI_2
	outb(IO_ICU2+ICU_IMR_OFFSET, 2 | 1);		/* auto EOI, 8086 mode */
#else
	outb(IO_ICU2+ICU_IMR_OFFSET,1);		/* 8086 mode */
#endif
#endif /* PC98 */
	outb(IO_ICU2+ICU_IMR_OFFSET, 0xff);          /* leave interrupts masked */
	outb(IO_ICU2, 0x0a);		/* default to IRR on read */
}

/*
 * Caught a stray interrupt, notify
 */
static void
isa_strayintr(vcookiep)
	void *vcookiep;
{
	int intr = (void **)vcookiep - &intr_unit[0];

	/*
	 * XXX TODO print a different message for #7 if it is for a
	 * glitch.  Glitches can be distinguished from real #7's by
	 * testing that the in-service bit is _not_ set.  The test
	 * must be done before sending an EOI so it can't be done if
	 * we are using AUTO_EOI_1.
	 */
	if (intrcnt[1 + intr] <= 5)
		log(LOG_ERR, "stray irq %d\n", intr);
	if (intrcnt[1 + intr] == 5)
		log(LOG_CRIT,
		    "too many stray irq %d's; not logging any more\n", intr);
}

#if NISA > 0
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
update_intrname(int intr, char *name)
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
#ifdef FAST_HI
	int		select;		/* the select register is 8 bits */
	int		vector;
	u_int32_t	value;		/* the window register is 32 bits */
#endif /* FAST_HI */
	u_long	ef;

#if defined(APIC_IO)
	if ((u_int)intr >= ICU_LEN)	/* no 8259 SLAVE to ignore */
#else
	if ((u_int)intr >= ICU_LEN || intr == ICU_SLAVEID)
#endif /* APIC_IO */
	if (intr_handler[intr] != isa_strayintr)
		return (EBUSY);

	ef = read_eflags();
	disable_intr();
	intr_handler[intr] = handler;
	intr_unit[intr] = arg;
#ifdef FAST_HI
	if (flags & INTR_FAST) {
		vector = TPR_FAST_INTS + intr;
		setidt(vector, fastintr[intr],
		       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	}
	else {
		vector = TPR_SLOW_INTS + intr;
#ifdef APIC_INTR_REORDER
#ifdef APIC_INTR_HIGHPRI_CLOCK
		/* XXX: Hack (kludge?) for more accurate clock. */
		if (intr == apic_8254_intr || intr == 8) {
			vector = TPR_FAST_INTS + intr;
		}
#endif
#endif
		setidt(vector, slowintr[intr],
		       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	}
#ifdef APIC_INTR_REORDER
	set_lapic_isrloc(intr, vector);
#endif
	/*
	 * Reprogram the vector in the IO APIC.
	 */
	if (int_to_apicintpin[intr].ioapic >= 0) {
		select = int_to_apicintpin[intr].redirindex;
		value = io_apic_read(int_to_apicintpin[intr].ioapic, 
				     select) & ~IOART_INTVEC;
		io_apic_write(int_to_apicintpin[intr].ioapic, 
			      select, value | vector);
	}
#else
	setidt(ICU_OFFSET + intr,
	       flags & INTR_FAST ? fastintr[intr] : slowintr[intr],
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#endif /* FAST_HI */
	INTREN(1 << intr);
	write_eflags(ef);
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
	u_long	ef;

	if ((u_int)intr >= ICU_LEN || handler != intr_handler[intr])
		return (EINVAL);

	INTRDIS(1 << intr);
	ef = read_eflags();
	disable_intr();
	intr_countp[intr] = &intrcnt[1 + intr];
	intr_handler[intr] = isa_strayintr;
	intr_unit[intr] = &intr_unit[intr];
#ifdef FAST_HI_XXX
	/* XXX how do I re-create dvp here? */
	setidt(flags & INTR_FAST ? TPR_FAST_INTS + intr : TPR_SLOW_INTS + intr,
	    slowintr[intr], SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#else /* FAST_HI */
#ifdef APIC_INTR_REORDER
	set_lapic_isrloc(intr, ICU_OFFSET + intr);
#endif
	setidt(ICU_OFFSET + intr, slowintr[intr], SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
#endif /* FAST_HI */
	write_eflags(ef);
	return (0);
}

struct intrec *
inthand_add(const char *name, int irq, driver_intr_t handler, void *arg,
	     int pri, int flags)
{
	struct ithd *ithd = ithds[irq];	/* descriptor for the IRQ */
	struct intrec *head;		/* chain of handlers for IRQ */
	struct intrec *idesc;		/* descriptor for this handler */
	struct proc *p;			/* interrupt thread */
	int errcode = 0;

	if (name == NULL)		/* no name? */
		panic ("anonymous interrupt");
	if (ithd == NULL || ithd->it_ih == NULL) {
		/* first handler for this irq. */
		if (ithd == NULL) {
			ithd = malloc(sizeof (struct ithd), M_DEVBUF, M_WAITOK);
			if (ithd == NULL)
				return (NULL);
			bzero(ithd, sizeof(struct ithd));	
			ithd->irq = irq;
			ithds[irq] = ithd;
		}
		/*
		 * If we have a fast interrupt, we need to set the
		 * handler address directly.  Do that below.  For a
		 * slow interrupt, we don't need to know more details,
		 * so do it here because it's tidier.
		 */
		if ((flags & INTR_FAST)	== 0) {
			/*
			 * Only create a kernel thread if we don't already
			 * have one.
			 */
			if (ithd->it_proc == NULL) {
				errcode = kthread_create(ithd_loop, NULL, &p,
				    RFSTOPPED | RFHIGHPID, "irq%d: %s", irq,
				    name);
				if (errcode)
					panic("inthand_add: Can't create "
					      "interrupt thread");
				p->p_rtprio.type = RTP_PRIO_ITHREAD;
				p->p_stat = SWAIT; /* we're idle */

				/* Put in linkages. */
				ithd->it_proc = p;
				p->p_ithd = ithd;
			} else
				snprintf(ithd->it_proc->p_comm, MAXCOMLEN,
				    "irq%d: %s", irq, name);
			p->p_rtprio.prio = pri;

			/*
			 * The interrupt process must be in place, but
			 * not necessarily schedulable, before we
			 * initialize the ICU, since it may cause an
			 * immediate interrupt.
			 */
			if (icu_setup(irq, &sched_ithd, arg, flags) != 0)
				panic("inthand_add: Can't initialize ICU");
		}
	} else if ((flags & INTR_EXCL) != 0
		   || (ithd->it_ih->flags & INTR_EXCL) != 0) {
		/*
		 * We can't append the new handler if either
		 * list ithd or new handler do not allow
		 * interrupts to be shared.
		 */
		if (bootverbose)
			printf("\tdevice combination %s and %s "
			       "doesn't support shared irq%d\n",
			       ithd->it_ih->name, name, irq);
		return(NULL);
	} else if (flags & INTR_FAST) {
		 /* We can only have one fast interrupt by itself. */
		if (bootverbose)
			printf("\tCan't add fast interrupt %s"
 			       " to normal interrupt %s on irq%d",
			       name, ithd->it_ih->name, irq);
		return (NULL);
	} else {			/* update p_comm */
		p = ithd->it_proc;
		if (strlen(p->p_comm) + strlen(name) < MAXCOMLEN) {
			strcat(p->p_comm, " ");
			strcat(p->p_comm, name);
		} else if (strlen(p->p_comm) == MAXCOMLEN)
			p->p_comm[MAXCOMLEN - 1] = '+';
		else
			strcat(p->p_comm, "+");
	}
	idesc = malloc(sizeof (struct intrec), M_DEVBUF, M_WAITOK);
	if (idesc == NULL)
		return (NULL);
	bzero(idesc, sizeof (struct intrec));

	idesc->handler	= handler;
	idesc->argument = arg;
	idesc->flags    = flags;
	idesc->ithd     = ithd;

	idesc->name     = malloc(strlen(name) + 1, M_DEVBUF, M_WAITOK);
	if (idesc->name == NULL) {
		free(idesc, M_DEVBUF);
		return (NULL);
	}
	strcpy(idesc->name, name);

	/* Slow interrupts got set up above. */
	if ((flags & INTR_FAST)
		&& (icu_setup(irq, idesc->handler, idesc->argument,
			      idesc->flags) != 0) ) {
		if (bootverbose)
				printf("\tinthand_add(irq%d) failed, result=%d\n", 
			       irq, errcode);
		free(idesc->name, M_DEVBUF);
		free(idesc, M_DEVBUF);
			return NULL;
	}
	head = ithd->it_ih;		/* look at chain of handlers */
	if (head) {
		while (head->next != NULL)
			head = head->next; /* find the end */
		head->next = idesc;	/* hook it in there */
	} else
		ithd->it_ih = idesc;	/* put it up front */
	update_intrname(irq, idesc->name);
	return (idesc);
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
inthand_remove(struct intrec *idesc)
{
	struct ithd *ithd;		/* descriptor for the IRQ */
	struct intrec *ih;		/* chain of handlers */

	if (idesc == NULL)
		return (-1);
	ithd = idesc->ithd;
	ih = ithd->it_ih;

	if (ih == idesc)		/* first in the chain */
		ithd->it_ih = idesc->next; /* unhook it */
	else {
		while ((ih != NULL)
			&& (ih->next != idesc) )
			ih = ih->next;
		if (ih->next != idesc)
		return (-1);
		ih->next = ih->next->next;
			}
	
	if (ithd->it_ih == NULL) {	/* no handlers left, */
		icu_unset(ithd->irq, idesc->handler);
		ithds[ithd->irq] = NULL;

		mtx_enter(&sched_lock, MTX_SPIN);
		if (ithd->it_proc->p_stat == SWAIT) {
			ithd->it_proc->p_stat = SRUN;
			setrunqueue(ithd->it_proc);
			/*
			 * We don't do an ast here because we really
			 * don't care when it runs next.
			 *
			 * XXX: should we lower the threads priority?
			 */
		}
		mtx_exit(&sched_lock, MTX_SPIN);
	}
	free(idesc, M_DEVBUF);
	return (0);
}
