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

#include "opt_platform.h"

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
#include <machine/smp.h>
#include <machine/trap.h>

#include "pic_if.h"

#define	MAX_STRAY_LOG	5

MALLOC_DEFINE(M_INTR, "intr", "interrupt handler data");

struct powerpc_intr {
	struct intr_event *event;
	long	*cntp;
	u_int	irq;
	device_t pic;
	u_int	intline;
	u_int	vector;
	enum intr_trigger trig;
	enum intr_polarity pol;
};

struct pic {
	device_t pic;
	uint32_t pic_id;
	int ipi_irq;
};

static struct mtx intr_table_lock;
static struct powerpc_intr *powerpc_intrs[INTR_VECTORS];
static struct pic piclist[MAX_PICS];
static u_int nvectors;		/* Allocated vectors */
static u_int npics;		/* PICs registered */
static u_int stray_count;

device_t root_pic;

#ifdef SMP
static void *ipi_cookie;
#endif

static void
intr_init(void *dummy __unused)
{

	mtx_init(&intr_table_lock, "intr sources lock", NULL, MTX_DEF);
}
SYSINIT(intr_init, SI_SUB_INTR, SI_ORDER_FIRST, intr_init, NULL);

static void
intrcnt_setname(const char *name, int index)
{

	snprintf(intrnames + (MAXCOMLEN + 1) * index, MAXCOMLEN + 1, "%-*s",
	    MAXCOMLEN, name);
}

static struct powerpc_intr *
intr_lookup(u_int irq)
{
	char intrname[8];
	struct powerpc_intr *i, *iscan;
	int vector;

	mtx_lock(&intr_table_lock);
	for (vector = 0; vector < nvectors; vector++) {
		i = powerpc_intrs[vector];
		if (i != NULL && i->irq == irq) {
			mtx_unlock(&intr_table_lock);
			return (i);
		}
	}
	mtx_unlock(&intr_table_lock);

	i = malloc(sizeof(*i), M_INTR, M_NOWAIT);
	if (i == NULL)
		return (NULL);

	i->event = NULL;
	i->cntp = NULL;
	i->trig = INTR_TRIGGER_CONFORM;
	i->pol = INTR_POLARITY_CONFORM;
	i->irq = irq;
	i->pic = NULL;
	i->vector = -1;

	mtx_lock(&intr_table_lock);
	for (vector = 0; vector < INTR_VECTORS && vector <= nvectors;
	    vector++) {
		iscan = powerpc_intrs[vector];
		if (iscan != NULL && iscan->irq == irq)
			break;
		if (iscan == NULL && i->vector == -1)
			i->vector = vector;
		iscan = NULL;
	}

	if (iscan == NULL && i->vector != -1) {
		powerpc_intrs[i->vector] = i;
		sprintf(intrname, "irq%u:", i->irq);
		intrcnt_setname(intrname, i->vector);
		nvectors++;
	}
	mtx_unlock(&intr_table_lock);

	if (iscan != NULL || i->vector == -1) {
		free(i, M_INTR);
		i = iscan;
	}

	return (i);
}

static int
powerpc_map_irq(struct powerpc_intr *i)
{

	i->intline = INTR_INTLINE(i->irq);
	i->pic = piclist[INTR_IGN(i->irq)].pic;

	/* Try a best guess if that failed */
	if (i->pic == NULL)
		i->pic = root_pic;

	return (0);
}

static void
powerpc_intr_eoi(void *arg)
{
	struct powerpc_intr *i = arg;

	PIC_EOI(i->pic, i->intline);
}

static void
powerpc_intr_mask(void *arg)
{
	struct powerpc_intr *i = arg;

	PIC_MASK(i->pic, i->intline);
}

static void
powerpc_intr_unmask(void *arg)
{
	struct powerpc_intr *i = arg;

	PIC_UNMASK(i->pic, i->intline);
}

void
powerpc_register_pic(device_t dev, u_int ipi)
{
	int i;

	mtx_lock(&intr_table_lock);

	for (i = 0; i < npics; i++) {
		if (piclist[i].pic_id == PIC_ID(dev))
			break;
	}
	piclist[i].pic = dev;
	piclist[i].pic_id = PIC_ID(dev);
	piclist[i].ipi_irq = ipi;
	if (i == npics)
		npics++;

	mtx_unlock(&intr_table_lock);
}

int
powerpc_ign_lookup(uint32_t pic_id)
{
	int i;

	mtx_lock(&intr_table_lock);

	for (i = 0; i < npics; i++) {
		if (piclist[i].pic_id == pic_id) {
			mtx_unlock(&intr_table_lock);
			return (i);
		}
	}
	piclist[i].pic = NULL;
	piclist[i].pic_id = pic_id;
	piclist[i].ipi_irq = 0;
	npics++;

	mtx_unlock(&intr_table_lock);

	return (i);
}

int
powerpc_enable_intr(void)
{
	struct powerpc_intr *i;
	int error, vector;
#ifdef SMP
	int n;
#endif

	if (npics == 0)
		panic("no PIC detected\n");

#ifdef SMP
	/* Install an IPI handler. */

	for (n = 0; n < npics; n++) {
		if (piclist[n].pic != root_pic)
			continue;

		error = powerpc_setup_intr("IPI",
		    INTR_VEC(piclist[n].pic_id, piclist[n].ipi_irq),
		    powerpc_ipi_handler, NULL, NULL,
		    INTR_TYPE_MISC | INTR_EXCL | INTR_FAST, &ipi_cookie);
		if (error) {
			printf("unable to setup IPI handler\n");
			return (error);
		}
	}
#endif

	for (vector = 0; vector < nvectors; vector++) {
		i = powerpc_intrs[vector];
		if (i == NULL)
			continue;

		error = powerpc_map_irq(i);
		if (error)
			continue;

		if (i->trig != INTR_TRIGGER_CONFORM ||
		    i->pol != INTR_POLARITY_CONFORM)
			PIC_CONFIG(i->pic, i->intline, i->trig, i->pol);

		if (i->event != NULL)
			PIC_ENABLE(i->pic, i->intline, vector);
	}

	return (0);
}

int
powerpc_setup_intr(const char *name, u_int irq, driver_filter_t filter,
    driver_intr_t handler, void *arg, enum intr_type flags, void **cookiep)
{
	struct powerpc_intr *i;
	int error, enable = 0;

	i = intr_lookup(irq);
	if (i == NULL)
		return (ENOMEM);

	if (i->event == NULL) {
		error = intr_event_create(&i->event, (void *)i, 0, irq,
		    powerpc_intr_mask, powerpc_intr_unmask, powerpc_intr_eoi,
		    NULL, "irq%u:", irq);
		if (error)
			return (error);

		i->cntp = &intrcnt[i->vector];

		enable = 1;
	}

	error = intr_event_add_handler(i->event, name, filter, handler, arg,
	    intr_priority(flags), flags, cookiep);

	mtx_lock(&intr_table_lock);
	intrcnt_setname(i->event->ie_fullname, i->vector);
	mtx_unlock(&intr_table_lock);

	if (!cold) {
		error = powerpc_map_irq(i);

		if (!error && (i->trig != INTR_TRIGGER_CONFORM ||
		    i->pol != INTR_POLARITY_CONFORM))
			PIC_CONFIG(i->pic, i->intline, i->trig, i->pol);

		if (!error && enable)
			PIC_ENABLE(i->pic, i->intline, i->vector);
	}
	return (error);
}

int
powerpc_teardown_intr(void *cookie)
{

	return (intr_event_remove_handler(cookie));
}

int
powerpc_config_intr(int irq, enum intr_trigger trig, enum intr_polarity pol)
{
	struct powerpc_intr *i;

	i = intr_lookup(irq);
	if (i == NULL)
		return (ENOMEM);

	i->trig = trig;
	i->pol = pol;

	if (!cold && i->pic != NULL)
		PIC_CONFIG(i->pic, i->intline, trig, pol);

	return (0);
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
		PIC_MASK(i->pic, i->intline);
}
