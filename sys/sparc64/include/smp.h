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

#define	CPU_CLKSYNC		1
#define	CPU_INIT		2
#define	CPU_BOOTSTRAP		3

#ifndef	LOCORE

#include <machine/intr_machdep.h>
#include <machine/tte.h>

#define	IDR_BUSY	(1<<0)
#define	IDR_NACK	(1<<1)

#define	IPI_AST		PIL_AST
#define	IPI_RENDEZVOUS	PIL_RENDEZVOUS
#define	IPI_STOP	PIL_STOP

#define	IPI_RETRIES	100

struct cpu_start_args {
	u_int	csa_count;
	u_int	csa_mid;
	u_int	csa_state;
	vm_offset_t csa_pcpu;
	u_long	csa_tick;
	u_long	csa_ver;
	struct	tte csa_ttes[PCPU_PAGES];
};

struct ipi_cache_args {
	u_int	ica_mask;
	u_long	ica_pa;
};

struct ipi_tlb_args {
	u_int	ita_mask;
	struct	pmap *ita_pmap;
	u_long	ita_start;
	u_long	ita_end;
};
#define	ita_va	ita_start

struct pcpu;

void	cpu_mp_bootstrap(struct pcpu *pc);
void	cpu_mp_shutdown(void);

void	cpu_ipi_selected(u_int cpus, u_long d0, u_long d1, u_long d2);
void	cpu_ipi_send(u_int mid, u_long d0, u_long d1, u_long d2);

void	ipi_selected(u_int cpus, u_int ipi);
void	ipi_all(u_int ipi);
void	ipi_all_but_self(u_int ipi);

vm_offset_t mp_tramp_alloc(void);

extern	struct	ipi_cache_args ipi_cache_args;
extern	struct	ipi_level_args ipi_level_args;
extern	struct	ipi_tlb_args ipi_tlb_args;

extern	vm_offset_t mp_tramp;
extern	char *mp_tramp_code;
extern	u_long mp_tramp_code_len;
extern	u_long mp_tramp_tlb_slots;
extern	u_long mp_tramp_func;

extern	void mp_startup(void);

extern	char tl_ipi_dcache_page_inval[];
extern	char tl_ipi_icache_page_inval[];
extern	char tl_ipi_level[];
extern	char tl_ipi_tlb_context_demap[];
extern	char tl_ipi_tlb_page_demap[];
extern	char tl_ipi_tlb_range_demap[];

#ifdef SMP

static __inline void *
ipi_dcache_page_inval(vm_offset_t pa)
{
	struct ipi_cache_args *ica;

	if (smp_cpus == 1)
		return (NULL);
	ica = &ipi_cache_args;
	ica->ica_mask = all_cpus;
	ica->ica_pa = pa;
	cpu_ipi_selected(PCPU_GET(other_cpus), 0,
	    (u_long)tl_ipi_dcache_page_inval, (u_long)ica);
	return (&ica->ica_mask);
}

static __inline void *
ipi_icache_page_inval(vm_offset_t pa)
{
	struct ipi_cache_args *ica;

	if (smp_cpus == 1)
		return (NULL);
	ica = &ipi_cache_args;
	ica->ica_mask = all_cpus;
	ica->ica_pa = pa;
	cpu_ipi_selected(PCPU_GET(other_cpus), 0,
	    (u_long)tl_ipi_icache_page_inval, (u_long)ica);
	return (&ica->ica_mask);
}

#ifdef _MACHINE_PMAP_H_

static __inline void *
ipi_tlb_context_demap(struct pmap *pm)
{
	struct ipi_tlb_args *ita;
	u_int cpus;

	if (smp_cpus == 1)
		return (NULL);
	if ((cpus = (pm->pm_active & PCPU_GET(other_cpus))) == 0)
		return (NULL);
	ita = &ipi_tlb_args;
	ita->ita_mask = cpus | PCPU_GET(cpumask);
	ita->ita_pmap = pm;
	cpu_ipi_selected(cpus, 0, (u_long)tl_ipi_tlb_context_demap,
	    (u_long)ita);
	return (&ita->ita_mask);
}

static __inline void *
ipi_tlb_page_demap(struct pmap *pm, vm_offset_t va)
{
	struct ipi_tlb_args *ita;
	u_int cpus;

	if (smp_cpus == 1)
		return (NULL);
	if ((cpus = (pm->pm_active & PCPU_GET(other_cpus))) == 0)
		return (NULL);
	ita = &ipi_tlb_args;
	ita->ita_mask = cpus | PCPU_GET(cpumask);
	ita->ita_pmap = pm;
	ita->ita_va = va;
	cpu_ipi_selected(cpus, 0, (u_long)tl_ipi_tlb_page_demap, (u_long)ita);
	return (&ita->ita_mask);
}

static __inline void *
ipi_tlb_range_demap(struct pmap *pm, vm_offset_t start, vm_offset_t end)
{
	struct ipi_tlb_args *ita;
	u_int cpus;

	if (smp_cpus == 1)
		return (NULL);
	if ((cpus = (pm->pm_active & PCPU_GET(other_cpus))) == 0)
		return (NULL);
	ita = &ipi_tlb_args;
	ita->ita_mask = cpus | PCPU_GET(cpumask);
	ita->ita_pmap = pm;
	ita->ita_start = start;
	ita->ita_end = end;
	cpu_ipi_selected(cpus, 0, (u_long)tl_ipi_tlb_range_demap, (u_long)ita);
	return (&ita->ita_mask);
}

static __inline void
ipi_wait(void *cookie)
{
	u_int *volatile mask;

	if ((mask = cookie) != NULL) {
		atomic_clear_int(mask, PCPU_GET(cpumask));
		while (*mask != 0)
			;
	}
}

#endif

#else

static __inline void *
ipi_dcache_page_inval(vm_offset_t pa)
{
	return (NULL);
}

static __inline void *
ipi_icache_page_inval(vm_offset_t pa)
{
	return (NULL);
}

static __inline void *
ipi_tlb_context_demap(struct pmap *pm)
{
	return (NULL);
}

static __inline void *
ipi_tlb_page_demap(struct pmap *pm, vm_offset_t va)
{
	return (NULL);
}

static __inline void *
ipi_tlb_range_demap(struct pmap *pm, vm_offset_t start, vm_offset_t end)
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
