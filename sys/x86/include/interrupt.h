/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
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

#ifndef __X86_INTR_MACHDEP_H__
#define	__X86_INTR_MACHDEP_H__

#ifdef _KERNEL

/* FreeBSD standard interrupt controller interface */

typedef struct intsrc interrupt_t;

/* FreeBSD standard interrupt controller interface */

#include <sys/_cpuset.h>
#include <sys/_interrupt.h>
#include <sys/kobj.h>
#include <sys/types.h>

DECLARE_CLASS(pic_base_class);

typedef struct { } pic_base_softc_t;

/*
 * Values used in determining the allocation of IRQ values among
 * different types of I/O interrupts.  These values are used as
 * indices into a interrupt source array to map I/O interrupts to a
 * device interrupt source whether it be a pin on an interrupt
 * controller or an MSI interrupt.  The 16 ISA IRQs are assigned fixed
 * IDT vectors, but all other device interrupts allocate IDT vectors
 * on demand.  Currently we have 191 IDT vectors available for device
 * interrupts on each CPU.  On many systems with I/O APICs, a lot of
 * the IRQs are not used, so the total number of IRQ values reserved
 * can exceed the number of available IDT slots.
 *
 * The first 16 IRQs (0 - 15) are reserved for ISA IRQs.  Interrupt
 * pins on I/O APICs for non-ISA interrupts use IRQ values starting at
 * IRQ 17.  This layout matches the GSI numbering used by ACPI so that
 * IRQ values returned by ACPI methods such as _CRS can be used
 * directly by the ACPI bus driver.
 *
 * MSI interrupts allocate a block of interrupts starting at the end
 * of the I/O APIC range.  When running under the Xen Hypervisor, an
 * additional range of IRQ values are available for binding to event
 * channel events.
 */
extern u_int first_msi_irq;
extern u_int num_io_irqs;
extern u_int num_msi_irqs;

/*
 * Default base address for MSI messages on x86 platforms.
 */
#define	MSI_INTEL_ADDR_BASE		0xfee00000

typedef void inthand_t(void);

#define	IDTVEC(name)	__CONCAT(X,name)

typedef	struct pic		*x86pic_t, x86pics_t;

/*
 * Methods that a PIC provides to mask/unmask a given interrupt source,
 * "turn on" the interrupt on the CPU side by setting up an IDT entry, and
 * return the vector associated with this source.
 */
struct pic {
	void (*pic_register_sources)(x86pic_t);
	void (*pic_enable_source)(x86pic_t, struct intsrc *);
	void (*pic_disable_source)(x86pic_t, struct intsrc *, int);
	void (*pic_eoi_source)(x86pic_t, struct intsrc *);
	void (*pic_enable_intr)(x86pic_t, struct intsrc *);
	void (*pic_disable_intr)(x86pic_t, struct intsrc *);
	int (*pic_source_pending)(x86pic_t, struct intsrc *);
	void (*pic_suspend)(x86pic_t);
	void (*pic_resume)(x86pic_t, bool suspend_cancelled);
	int (*pic_config_intr)(x86pic_t, struct intsrc *, enum intr_trigger,
	    enum intr_polarity);
	int (*pic_assign_cpu)(x86pic_t, struct intsrc *, u_int apic_id);
	void (*pic_reprogram_pin)(x86pic_t, struct intsrc *);
};

/* Wrappers for transition to kobj/devices */
#define	PIC_REGISTER_SOURCES(pic) \
		do {							\
			if ((pic)->pic_register_sources != NULL)	\
				((pic)->pic_register_sources(pic));	\
		} while(0)
#define	PIC_ENABLE_SOURCE(pic, isrc) \
		((pic)->pic_enable_source((pic), (isrc)))
#define	PIC_DISABLE_SOURCE(pic, isrc, eoi) \
		((pic)->pic_disable_source((pic), (isrc), (eoi)))
#define	PIC_EOI_SOURCE(pic, isrc)	((pic)->pic_eoi_source((pic), (isrc)))
#define	PIC_ENABLE_INTR(pic, isrc)	((pic)->pic_enable_intr((pic), (isrc)))
#define	PIC_DISABLE_INTR(pic, isrc)	((pic)->pic_disable_intr((pic), (isrc)))
#define	PIC_SUSPEND(pic) \
		do {							\
			if ((pic)->pic_suspend != NULL)			\
				((pic)->pic_suspend(pic));		\
		} while(0)
#define	PIC_RESUME(pic, cancel) \
		do {							\
			if ((pic)->pic_resume != NULL)			\
				((pic)->pic_resume((pic), (cancel)));	\
		} while(0)
#define	PIC_CONFIG_INTR(pic, isrc, trigger, polarity) \
		((pic)->pic_config_intr != NULL ? (pic)->pic_config_intr((pic),\
		    (isrc), (trigger), (polarity)) : ENODEV)
#define	PIC_ASSIGN_CPU(pic, isrc, apic_id) \
		((pic)->pic_assign_cpu((pic), (isrc), (apic_id)))
#define	PIC_REPROGRAM_PIN(pic, isrc) \
		do {							\
			if ((pic)->pic_reprogram_pin != NULL)		\
				((pic)->pic_reprogram_pin((pic), (isrc))); \
		} while(0)

#define	X86PIC_FUNC(func, impl)	.func = impl
#define	X86PIC_END
#define	X86PIC_PIC(type, pic)	((struct type *)(pic))
#define	X86PIC_PTR(pic)		(&(pic))

/* Flags for pic_disable_source() */
enum {
	PIC_EOI,
	PIC_NO_EOI,
};

/*
 * An interrupt source.  The upper-layer code uses the PIC methods to
 * control a given source.  The lower-layer PIC drivers can store additional
 * private data in a given interrupt source such as an interrupt pin number
 * or an I/O APIC pointer.
 */
struct intsrc {
	x86pic_t is_pic;
	struct intr_event *is_event;
	u_long *is_count;
	u_long *is_straycount;
	u_int is_index;
	u_int is_handlers;
	u_int is_domain;
	u_int is_cpu;
};

struct trapframe;
struct intr_handler;

#ifdef SMP
extern cpuset_t intr_cpus;
#endif
extern struct mtx icu_lock;
extern int elcr_found;
#ifdef SMP
extern int msix_disable_migration;
#endif

#ifndef DEV_ATPIC
void	atpic_reset(void);
#endif
/* XXX: The elcr_* prototypes probably belong somewhere else. */
int	elcr_probe(void);
enum intr_trigger elcr_read_trigger(u_int irq);
void	elcr_resume(void);
void	elcr_write_trigger(u_int irq, enum intr_trigger trigger);
#ifdef SMP
void	intr_add_cpu(u_int cpu);
#endif
int	intr_add_handler(struct intsrc *isrc, const char *name,
     driver_filter_t filter, driver_intr_t handler, void *arg,
     enum intr_type flags, void **cookiep, int domain);
int	intr_config_intr(struct intsrc *isrc, enum intr_trigger trig,
    enum intr_polarity pol);
int	intr_describe(struct intsrc *isrc, void *ih, const char *descr);
void	intr_disable_all(void);
void	intr_execute_handlers(struct intsrc *isrc, struct trapframe *frame);
u_int	intr_next_cpu(int domain);
struct intsrc *intr_lookup_source(int vector);
device_t	intr_create_pic(const char *name, u_int unit,
		    struct kobj_class *driver /* driver_t * */);
void	intr_register_pic(x86pic_t pic);
int	intr_register_source(unsigned int vector, struct intsrc *isrc);
int	intr_remove_handler(struct intsrc *isrc, struct intr_handler *handler);
void	intr_resume(bool suspend_cancelled);
void	intr_suspend(void);
void	intr_enable_src(u_int irq);
void	intr_reprogram(void);
void	intrcnt_add(const char *name, u_long **countp);
void	nexus_add_irq(u_long irq);
int	msi_alloc(device_t dev, int count, int maxcount, int *irqs);
void	msi_init(void);
int	msi_map(int irq, uint64_t *addr, uint32_t *data);
int	msi_release(int *irqs, int count);
int	msix_alloc(device_t dev, int *irq);
int	msix_release(int irq);
#ifdef XENHVM
void	xen_intr_alloc_irqs(void);
#endif

#endif	/* _KERNEL */
#endif	/* !__X86_INTR_MACHDEP_H__ */
