/*-
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
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
 * $FreeBSD$
 */

#ifndef __MACHINE_INTR_MACHDEP_H__
#define	__MACHINE_INTR_MACHDEP_H__

#ifdef _KERNEL

/* With I/O APIC's we can have up to 191 interrupts. */
#define	NUM_IO_INTS	191
#define	INTRCNT_COUNT	(1 + NUM_IO_INTS * 2)

#ifndef LOCORE

typedef void inthand_t(u_int cs, u_int ef, u_int esp, u_int ss);

#define	IDTVEC(name)	__CONCAT(X,name)

struct intsrc;

/*
 * Methods that a PIC provides to mask/unmask a given interrupt source,
 * "turn on" the interrupt on the CPU side by setting up an IDT entry, and
 * return the vector associated with this source.
 */
struct pic {
	void (*pic_enable_source)(struct intsrc *);
	void (*pic_disable_source)(struct intsrc *, int);
	void (*pic_eoi_source)(struct intsrc *);
	void (*pic_enable_intr)(struct intsrc *);
	int (*pic_vector)(struct intsrc *);
	int (*pic_source_pending)(struct intsrc *);
	void (*pic_suspend)(struct intsrc *);
	void (*pic_resume)(struct intsrc *);
	int (*pic_config_intr)(struct intsrc *, enum intr_trigger,
	    enum intr_polarity);
};

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
	struct pic *is_pic;
	struct ithd *is_ithread;
	u_long *is_count;
	u_long *is_straycount;
	u_int is_index;
};

struct intrframe;

extern struct mtx icu_lock;
extern int elcr_found;

/* XXX: The elcr_* prototypes probably belong somewhere else. */
int	elcr_probe(void);
enum intr_trigger elcr_read_trigger(u_int irq);
void	elcr_resume(void);
void	elcr_write_trigger(u_int irq, enum intr_trigger trigger);
int	intr_add_handler(const char *name, int vector, driver_intr_t handler,
    void *arg, enum intr_type flags, void **cookiep);
int	intr_config_intr(int vector, enum intr_trigger trig,
    enum intr_polarity pol);
void	intr_execute_handlers(struct intsrc *isrc, struct intrframe *iframe);
struct intsrc *intr_lookup_source(int vector);
int	intr_register_source(struct intsrc *isrc);
int	intr_remove_handler(void *cookie);
void	intr_resume(void);
void	intr_suspend(void);
void	intrcnt_add(const char *name, u_long **countp);

#endif	/* !LOCORE */
#endif	/* _KERNEL */
#endif	/* !__MACHINE_INTR_MACHDEP_H__ */
