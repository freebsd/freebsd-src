/*-
 * Copyright (c) 2004 Juli Mallett <jmallett@FreeBSD.org>
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

#ifndef	_MACHINE_INTR_MACHDEP_H_
#define	_MACHINE_INTR_MACHDEP_H_

#ifdef TARGET_XLR_XLS
/*
 * XLR/XLS uses its own intr_machdep.c and has
 * a different number of interupts. This probably
 * should be placed somewhere else.
 */

struct mips_intrhand {
        struct  intr_event *mih_event;
        driver_intr_t      *mih_disable;
        volatile long       *cntp;  /* interrupt counter */
};

extern struct mips_intrhand mips_intr_handlers[];
#define XLR_MAX_INTR 64 

#else
#define NHARD_IRQS	6
#define NSOFT_IRQS	2
#endif

struct trapframe;

void cpu_init_interrupts(void);
void cpu_establish_hardintr(const char *, driver_filter_t *, driver_intr_t *, 
    void *, int, int, void **);
void cpu_establish_softintr(const char *, driver_filter_t *, void (*)(void*), 
    void *, int, int, void **);
void cpu_intr(struct trapframe *);

/*
 * Opaque datatype that represents intr counter
 */
typedef unsigned long* mips_intrcnt_t;

mips_intrcnt_t mips_intrcnt_create(const char *);
void mips_intrcnt_setname(mips_intrcnt_t, const char *);

static __inline void
mips_intrcnt_inc(mips_intrcnt_t counter)
{
	if (counter)
		atomic_add_long(counter, 1);
}
#endif /* !_MACHINE_INTR_MACHDEP_H_ */
