/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#ifndef	_MACHINE_SMP_H_
#define	_MACHINE_SMP_H_

#include <machine/intr_machdep.h>

#define	CPU_INITING		1
#define	CPU_INITED		2
#define	CPU_REJECT		3
#define	CPU_STARTING		4
#define	CPU_STARTED		5
#define	CPU_BOOTSTRAPING	6
#define	CPU_BOOTSTRAPPED	7

#ifndef	LOCORE

#define	IDR_BUSY	(1<<0)
#define	IDR_NACK	(1<<1)

#define	IPI_AST		PIL_AST
#define	IPI_RENDEZVOUS	PIL_RENDEZVOUS
#define	IPI_STOP	PIL_STOP

#define	IPI_RETRIES	100

struct cpu_start_args {
	u_int	csa_mid;
	u_int	csa_state;
	u_long	csa_data;
	vm_offset_t csa_va;
};

struct ipi_level_args {
	u_int	ila_count;
	u_int	ila_level;
};

struct ipi_tlb_args {
	u_int	ita_count;
	u_long	ita_tlb;
	u_long	ita_ctx;
	u_long	ita_start;
	u_long	ita_end;
};
#define	ita_va	ita_start

struct pcpu;

void	cpu_mp_bootstrap(struct pcpu *pc);

void	cpu_ipi_selected(u_int cpus, u_long d0, u_long d1, u_long d2);
void	cpu_ipi_send(u_int mid, u_long d0, u_long d1, u_long d2);

void	ipi_selected(u_int cpus, u_int ipi);
void	ipi_all(u_int ipi);
void	ipi_all_but_self(u_int ipi);

extern	struct	ipi_level_args ipi_level_args;
extern	struct	ipi_tlb_args ipi_tlb_args;

extern	int mp_ncpus;

extern	char tl_ipi_level[];
extern	char tl_ipi_test[];
extern	char tl_ipi_tlb_context_demap[];
extern	char tl_ipi_tlb_page_demap[];
extern	char tl_ipi_tlb_range_demap[];

#ifdef SMP

static __inline void *
ipi_tlb_context_demap(u_int ctx)
{
	struct ipi_tlb_args *ita;

	if (mp_ncpus == 1)
		return (NULL);
	ita = &ipi_tlb_args;
	ita->ita_count = mp_ncpus;
	ita->ita_ctx = ctx;
	cpu_ipi_selected(PCPU_GET(other_cpus), 0,
	    (u_long)tl_ipi_tlb_context_demap, (u_long)ita);
	return (&ita->ita_count);
}

static __inline void *
ipi_tlb_page_demap(u_int tlb, u_int ctx, vm_offset_t va)
{
	struct ipi_tlb_args *ita;

	if (mp_ncpus == 1)
		return (NULL);
	ita = &ipi_tlb_args;
	ita->ita_count = mp_ncpus;
	ita->ita_tlb = tlb;
	ita->ita_ctx = ctx;
	ita->ita_va = va;
	cpu_ipi_selected(PCPU_GET(other_cpus), 0,
	    (u_long)tl_ipi_tlb_page_demap, (u_long)ita);
	return (&ita->ita_count);
}

static __inline void *
ipi_tlb_range_demap(u_int ctx, vm_offset_t start, vm_offset_t end)
{
	struct ipi_tlb_args *ita;

	if (mp_ncpus == 1)
		return (NULL);
	ita = &ipi_tlb_args;
	ita->ita_count = mp_ncpus;
	ita->ita_ctx = ctx;
	ita->ita_start = start;
	ita->ita_end = end;
	cpu_ipi_selected(PCPU_GET(other_cpus), 0,
	    (u_long)tl_ipi_tlb_range_demap, (u_long)ita);
	return (&ita->ita_count);
}

static __inline void
ipi_wait(void *cookie)
{
	u_int *count;

	if ((count = cookie) != NULL) {
		atomic_subtract_int(count, 1);
		while (*count != 0)
			membar(LoadStore);
	}
}

#else

static __inline void *
ipi_tlb_context_demap(u_int ctx)
{
	return (NULL);
}

static __inline void *
ipi_tlb_page_demap(u_int tlb, u_int ctx, vm_offset_t va)
{
	return (NULL);
}

static __inline void *
ipi_tlb_range_demap(u_int ctx, vm_offset_t start, vm_offset_t end)
{
	return (NULL);
}

static __inline void
ipi_wait(void *cookie)
{
}

#endif /* SMP */

#endif /* !LOCORE */

#endif /* !_MACHINE_SMP_H_ */
