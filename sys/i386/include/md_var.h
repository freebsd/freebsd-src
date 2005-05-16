/*-
 * Copyright (c) 1995 Bruce D. Evans.
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
 * 3. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#ifndef _MACHINE_MD_VAR_H_
#define	_MACHINE_MD_VAR_H_

/*
 * Miscellaneous machine-dependent declarations.
 */

extern	void	(*bcopy_vector)(const void *from, void *to, size_t len);
extern	void	(*bzero_vector)(void *buf, size_t len);
extern	int	(*copyin_vector)(const void *udaddr, void *kaddr, size_t len);
extern	int	(*copyout_vector)(const void *kaddr, void *udaddr, size_t len);

extern	long	Maxmem;
extern	u_int	basemem;	/* PA of original top of base memory */
extern	int	busdma_swi_pending;
extern	u_int	cpu_exthigh;
extern	u_int	cpu_feature, cpu_feature2;
extern	u_int	cpu_fxsr;
extern	u_int	cpu_high;
extern	u_int	cpu_id;
extern	u_int	cpu_procinfo;
extern	char	cpu_vendor[];
extern	u_int	cyrix_did;
extern	char	kstack[];
extern	char	sigcode[];
extern	int	szsigcode;
#ifdef COMPAT_FREEBSD4
extern	int	szfreebsd4_sigcode;
#endif
#ifdef COMPAT_43
extern	int	szosigcode;
#endif

typedef void alias_for_inthand_t(u_int cs, u_int ef, u_int esp, u_int ss);
struct	thread;
struct	reg;
struct	fpreg;
struct  dbreg;

void	bcopyb(const void *from, void *to, size_t len);
void	busdma_swi(void);
void	cpu_setregs(void);
void	cpu_switch_load_gs(void) __asm(__STRING(cpu_switch_load_gs));
void	doreti_iret(void) __asm(__STRING(doreti_iret));
void	doreti_iret_fault(void) __asm(__STRING(doreti_iret_fault));
void	doreti_popl_ds(void) __asm(__STRING(doreti_popl_ds));
void	doreti_popl_ds_fault(void) __asm(__STRING(doreti_popl_ds_fault));
void	doreti_popl_es(void) __asm(__STRING(doreti_popl_es));
void	doreti_popl_es_fault(void) __asm(__STRING(doreti_popl_es_fault));
void	doreti_popl_fs(void) __asm(__STRING(doreti_popl_fs));
void	doreti_popl_fs_fault(void) __asm(__STRING(doreti_popl_fs_fault));
void	enable_sse(void);
void	fillw(int /*u_short*/ pat, void *base, size_t cnt);
void	i486_bzero(void *buf, size_t len);
void	i586_bcopy(const void *from, void *to, size_t len);
void	i586_bzero(void *buf, size_t len);
int	i586_copyin(const void *udaddr, void *kaddr, size_t len);
int	i586_copyout(const void *kaddr, void *udaddr, size_t len);
void	i686_pagezero(void *addr);
void	sse2_pagezero(void *addr);
void	init_AMD_Elan_sc520(void);
int	is_physical_memory(vm_paddr_t addr);
int	isa_nmi(int cd);
vm_paddr_t kvtop(void *addr);
void	setidt(int idx, alias_for_inthand_t *func, int typ, int dpl, int selec);
int     user_dbreg_trap(void);

#endif /* !_MACHINE_MD_VAR_H_ */
