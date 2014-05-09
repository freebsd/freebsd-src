/*-
 * Copyright (c) 2007-2010 Marcel Moolenaar
 * Copyright (c) 1998 Doug Rabson
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

#ifndef _MACHINE_INTR_H_
#define	_MACHINE_INTR_H_

#define	IA64_NXIVS		256	/* External Interrupt Vectors */
#define	IA64_MIN_XIV		16

#define	IA64_MAX_HWPRIO		14

struct pcpu;
struct sapic;
struct thread;
struct trapframe;

/*
 * Layout of the Processor Interrupt Block.
 */
struct ia64_pib
{
	uint64_t	ib_ipi[65536][2];	/* 64K-way IPIs (1MB area). */
	uint8_t		_rsvd1[0xe0000];
	uint8_t		ib_inta;		/* Generate INTA cycle. */
	uint8_t		_rsvd2[7];
	uint8_t		ib_xtp;			/* External Task Priority. */
	uint8_t		_rsvd3[7];
	uint8_t		_rsvd4[0x1fff0];
};

enum ia64_xiv_use {
	IA64_XIV_FREE,
	IA64_XIV_ARCH,		/* Architecturally defined. */
	IA64_XIV_PLAT,		/* Platform defined. */
	IA64_XIV_IPI,		/* Used for IPIs. */
	IA64_XIV_IRQ		/* Used for external interrupts. */
};

typedef u_int (ia64_ihtype)(struct thread *, u_int, struct trapframe *);

extern struct ia64_pib *ia64_pib;

void	ia64_bind_intr(void);
void	ia64_handle_intr(struct trapframe *);
int	ia64_setup_intr(const char *, int, driver_filter_t, driver_intr_t,
	    void *, enum intr_type, void **);
int	ia64_teardown_intr(void *);

void	ia64_xiv_init(void);
u_int	ia64_xiv_alloc(u_int, enum ia64_xiv_use, ia64_ihtype);
int	ia64_xiv_free(u_int, enum ia64_xiv_use);
int	ia64_xiv_reserve(u_int, enum ia64_xiv_use, ia64_ihtype);

int	sapic_bind_intr(u_int, struct pcpu *);
int	sapic_config_intr(u_int, enum intr_trigger, enum intr_polarity);
struct sapic *sapic_create(u_int, u_int, uint64_t);
int	sapic_enable(struct sapic *, u_int, u_int);
void	sapic_eoi(struct sapic *, u_int);
struct sapic *sapic_lookup(u_int, u_int *);
void	sapic_mask(struct sapic *, u_int);
void	sapic_unmask(struct sapic *, u_int);

#ifdef DDB
void	sapic_print(struct sapic *, u_int);
#endif

#endif /* !_MACHINE_INTR_H_ */
