/* 	$NetBSD: intr.h,v 1.7 2003/06/16 20:01:00 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 Mark Brinicombe.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef _MACHINE_INTR_H_
#define _MACHINE_INTR_H_

#ifdef FDT
#include <dev/ofw/openfirm.h>
#endif

#ifdef ARM_INTRNG

#ifndef NIRQ
#define	NIRQ		1024	/* XXX - It should be an option. */
#endif

#ifdef notyet
#define	INTR_SOLO	INTR_MD1
typedef int intr_irq_filter_t(void *arg, struct trapframe *tf);
#else
typedef int intr_irq_filter_t(void *arg);
#endif

#define INTR_ISRC_NAMELEN	(MAXCOMLEN + 1)

typedef void intr_ipi_filter_t(void *arg);

enum intr_isrc_type {
	INTR_ISRCT_NAMESPACE,
	INTR_ISRCT_FDT
};

#define INTR_ISRCF_REGISTERED	0x01	/* registered in a controller */
#define INTR_ISRCF_PERCPU	0x02	/* per CPU interrupt */
#define INTR_ISRCF_BOUND	0x04	/* bound to a CPU */

/* Interrupt source definition. */
struct intr_irqsrc {
	device_t		isrc_dev;	/* where isrc is mapped */
	intptr_t		isrc_xref;	/* device reference key */
	uintptr_t		isrc_data;	/* device data for isrc */
	u_int			isrc_irq;	/* unique identificator */
	enum intr_isrc_type	isrc_type;	/* how is isrc decribed */
	u_int			isrc_flags;
	char			isrc_name[INTR_ISRC_NAMELEN];
	uint16_t		isrc_nspc_type;
	uint16_t		isrc_nspc_num;
	enum intr_trigger	isrc_trig;
	enum intr_polarity	isrc_pol;
	cpuset_t		isrc_cpu;	/* on which CPUs is enabled */
	u_int			isrc_index;
	u_long *		isrc_count;
	u_int			isrc_handlers;
	struct intr_event *	isrc_event;
	intr_irq_filter_t *	isrc_filter;
	intr_ipi_filter_t *	isrc_ipifilter;
	void *			isrc_arg;
#ifdef FDT
	u_int			isrc_ncells;
	pcell_t			isrc_cells[];	/* leave it last */
#endif
};

void intr_irq_set_name(struct intr_irqsrc *isrc, const char *fmt, ...)
    __printflike(2, 3);

void intr_irq_dispatch(struct intr_irqsrc *isrc, struct trapframe *tf);

#define INTR_IRQ_NSPC_NONE	0
#define INTR_IRQ_NSPC_PLAIN	1
#define INTR_IRQ_NSPC_IRQ	2
#define INTR_IRQ_NSPC_IPI	3

u_int intr_namespace_map_irq(device_t dev, uint16_t type, uint16_t num);
#ifdef FDT
u_int intr_fdt_map_irq(phandle_t, pcell_t *, u_int);
#endif

int intr_pic_register(device_t dev, intptr_t xref);
int intr_pic_unregister(device_t dev, intptr_t xref);
int intr_pic_claim_root(device_t dev, intptr_t xref, intr_irq_filter_t *filter,
    void *arg, u_int ipicount);

int intr_irq_add_handler(device_t dev, driver_filter_t, driver_intr_t, void *,
    u_int, int, void **);
int intr_irq_remove_handler(device_t dev, u_int, void *);
int intr_irq_config(u_int, enum intr_trigger, enum intr_polarity);
int intr_irq_describe(u_int, void *, const char *);

u_int intr_irq_next_cpu(u_int current_cpu, cpuset_t *cpumask);

#ifdef SMP
int intr_irq_bind(u_int, int);

void intr_ipi_dispatch(struct intr_irqsrc *isrc, struct trapframe *tf);

#define AISHF_NOALLOC	0x0001

int intr_ipi_set_handler(u_int ipi, const char *name, intr_ipi_filter_t *filter,
    void *arg, u_int flags);

void intr_pic_init_secondary(void);
#endif

#else /* ARM_INTRNG */

/* XXX move to std.* files? */
#ifdef CPU_XSCALE_81342
#define NIRQ		128
#elif defined(CPU_XSCALE_PXA2X0)
#include <arm/xscale/pxa/pxareg.h>
#define	NIRQ		IRQ_GPIO_MAX
#elif defined(SOC_MV_DISCOVERY)
#define NIRQ		96
#elif defined(CPU_ARM9) || defined(SOC_MV_KIRKWOOD) || \
    defined(CPU_XSCALE_IXP435)
#define NIRQ		64
#elif defined(CPU_CORTEXA)
#define NIRQ		1020
#elif defined(CPU_KRAIT)
#define NIRQ		288
#elif defined(CPU_ARM1176)
#define NIRQ		128
#elif defined(SOC_MV_ARMADAXP)
#define MAIN_IRQ_NUM		116
#define ERR_IRQ_NUM		32
#define ERR_IRQ			(MAIN_IRQ_NUM)
#define MSI_IRQ_NUM		32
#define MSI_IRQ			(ERR_IRQ + ERR_IRQ_NUM)
#define NIRQ			(MAIN_IRQ_NUM + ERR_IRQ_NUM + MSI_IRQ_NUM)
#else
#define NIRQ		32
#endif

int arm_get_next_irq(int);
void arm_mask_irq(uintptr_t);
void arm_unmask_irq(uintptr_t);
void arm_intrnames_init(void);
void arm_setup_irqhandler(const char *, int (*)(void*), void (*)(void*),
    void *, int, int, void **);
int arm_remove_irqhandler(int, void *);
extern void (*arm_post_filter)(void *);
extern int (*arm_config_irq)(int irq, enum intr_trigger trig,
    enum intr_polarity pol);

void intr_pic_init_secondary(void);

#ifdef FDT
int gic_decode_fdt(phandle_t, pcell_t *, int *, int *, int *);
int intr_fdt_map_irq(phandle_t, pcell_t *, int);
#endif

#endif /* ARM_INTRNG */

void arm_irq_memory_barrier(uintptr_t);

#endif	/* _MACHINE_INTR_H */
