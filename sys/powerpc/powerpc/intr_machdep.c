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
 */
/*-
 * Copyright (c) 2002 Benno Rice.
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
 *
 *	from: @(#)isa.c	7.2 (Berkeley) 5/13/91
 *	form: src/sys/i386/isa/intr_machdep.c,v 1.57 2001/07/20
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/syslog.h>
#include <sys/vmmeter.h>
#include <sys/proc.h>

#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/trap.h>

#include "pic_if.h"

#define	MAX_STRAY_LOG	5

MALLOC_DEFINE(M_INTR, "intr", "interrupt handler data");

struct powerpc_intr {
	struct intr_event *event;
	long	*cntp;
	u_int	irq;
};

static struct powerpc_intr *powerpc_intrs[INTR_VECTORS];
static u_int nvectors;		/* Allocated vectors */
static u_int stray_count;

device_t pic;

static void
intrcnt_setname(const char *name, int index)
{
	snprintf(intrnames + (MAXCOMLEN + 1) * index, MAXCOMLEN + 1, "%-*s",
	    MAXCOMLEN, name);
}

static void
powerpc_intr_eoi(void *arg)
{
	u_int irq = (uintptr_t)arg;

	PIC_EOI(pic, irq);
}

static void
powerpc_intr_mask(void *arg)
{
	u_int irq = (uintptr_t)arg;

	PIC_MASK(pic, irq);
}

static void
powerpc_intr_unmask(void *arg)
{
	u_int irq = (uintptr_t)arg;

	PIC_UNMASK(pic, irq);
}

void
powerpc_register_pic(device_t dev)
{

	pic = dev;
}

int
powerpc_enable_intr(void)
{
	struct powerpc_intr *i;
	int vector;

	for (vector = 0; vector < nvectors; vector++) {
		i = powerpc_intrs[vector];
		if (i == NULL)
			continue;

		PIC_ENABLE(pic, i->irq, vector);
	}

	return (0);
}

int
powerpc_setup_intr(const char *name, u_int irq, driver_filter_t filter, 
    driver_intr_t handler, void *arg, enum intr_type flags, void **cookiep)
{
	struct powerpc_intr *i;
	u_int vector;
	int error;

	/* XXX lock */

	i = NULL;
	for (vector = 0; vector < nvectors; vector++) {
		i = powerpc_intrs[vector];
		if (i == NULL)
			continue;
		if (i->irq == irq)
			break;
		i = NULL;
	}

	if (i == NULL) {
		if (nvectors >= INTR_VECTORS) {
			/* XXX unlock */
			return (ENOENT);
		}

		i = malloc(sizeof(*i), M_INTR, M_NOWAIT);
		if (i == NULL) {
			/* XXX unlock */
			return (ENOMEM);
		}
		error = intr_event_create(&i->event, (void *)irq, 0,
		    powerpc_intr_mask, powerpc_intr_unmask, powerpc_intr_eoi,
		    NULL, "irq%u:", irq);
		if (error) {
			/* XXX unlock */
			free(i, M_INTR);
			return (error);
		}

		vector = nvectors++;
		powerpc_intrs[vector] = i;

		i->irq = irq;

		/* XXX unlock */

		i->cntp = &intrcnt[vector];
		intrcnt_setname(i->event->ie_fullname, vector);

		if (!cold)
			PIC_ENABLE(pic, i->irq, vector);
	} else {
		/* XXX unlock */
	}

	error = intr_event_add_handler(i->event, name, filter, handler, arg,
	    intr_priority(flags), flags, cookiep);
	if (!error)
		intrcnt_setname(i->event->ie_fullname, vector);
	return (error);
}

int
powerpc_teardown_intr(void *cookie)
{

	return (intr_event_remove_handler(cookie));
}

void
powerpc_dispatch_intr(u_int vector, struct trapframe *tf)
{
	struct powerpc_intr *i;
	struct intr_event *ie;

	i = powerpc_intrs[vector];
	if (i == NULL)
		goto stray;

	(*i->cntp)++;

	ie = i->event;
	KASSERT(ie != NULL, ("%s: interrupt without an event", __func__));

	if (intr_event_handle(ie, tf) != 0) {
		goto stray;
	}
	return;

stray:
	stray_count++;
	if (stray_count <= MAX_STRAY_LOG) {
		printf("stray irq %d\n", i ? i->irq : -1);
		if (stray_count >= MAX_STRAY_LOG) {
			printf("got %d stray interrupts, not logging anymore\n",
			    MAX_STRAY_LOG);
		}
	}
	if (i != NULL)
		PIC_MASK(pic, i->irq);
}
