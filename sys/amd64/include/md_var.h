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

extern	long	Maxmem;
extern	u_int	basemem;
extern	int	busdma_swi_pending;
extern	u_int	cpu_exthigh;
extern	u_int	cpu_feature;
extern	u_int	cpu_feature2;
extern	u_int	amd_feature;
extern	u_int	amd_feature2;
extern	u_int	amd_pminfo;
extern	u_int	via_feature_rng;
extern	u_int	via_feature_xcrypt;
extern	u_int	cpu_clflush_line_size;
extern	u_int	cpu_stdext_feature;
extern	u_int	cpu_stdext_feature2;
extern	u_int	cpu_fxsr;
extern	u_int	cpu_high;
extern	u_int	cpu_id;
extern	u_int	cpu_max_ext_state_size;
extern	u_int	cpu_mxcsr_mask;
extern	u_int	cpu_procinfo;
extern	u_int	cpu_procinfo2;
extern	char	cpu_vendor[];
extern	u_int	cpu_vendor_id;
extern	u_int	cpu_mon_mwait_flags;
extern	u_int	cpu_mon_min_size;
extern	u_int	cpu_mon_max_size;
extern	u_int	cpu_maxphyaddr;
extern	char	ctx_switch_xsave[];
extern	u_int	hv_high;
extern	char	hv_vendor[];
extern	char	kstack[];
extern	char	sigcode[];
extern	int	szsigcode;
extern	uint64_t *vm_page_dump;
extern	int	vm_page_dump_size;
extern	int	workaround_erratum383;
extern	int	_udatasel;
extern	int	_ucodesel;
extern	int	_ucode32sel;
extern	int	_ufssel;
extern	int	_ugssel;
extern	int	use_xsave;
extern	uint64_t xsave_mask;

typedef void alias_for_inthand_t(u_int cs, u_int ef, u_int esp, u_int ss);
struct	pcb;
struct	savefpu;
struct	thread;
struct	reg;
struct	fpreg;
struct  dbreg;
struct	dumperinfo;

void	*alloc_fpusave(int flags);
void	amd64_syscall(struct thread *td, int traced);
void	busdma_swi(void);
void	cpu_setregs(void);
void	doreti_iret(void) __asm(__STRING(doreti_iret));
void	doreti_iret_fault(void) __asm(__STRING(doreti_iret_fault));
void	ld_ds(void) __asm(__STRING(ld_ds));
void	ld_es(void) __asm(__STRING(ld_es));
void	ld_fs(void) __asm(__STRING(ld_fs));
void	ld_gs(void) __asm(__STRING(ld_gs));
void	ld_fsbase(void) __asm(__STRING(ld_fsbase));
void	ld_gsbase(void) __asm(__STRING(ld_gsbase));
void	ds_load_fault(void) __asm(__STRING(ds_load_fault));
void	es_load_fault(void) __asm(__STRING(es_load_fault));
void	fs_load_fault(void) __asm(__STRING(fs_load_fault));
void	gs_load_fault(void) __asm(__STRING(gs_load_fault));
void	fsbase_load_fault(void) __asm(__STRING(fsbase_load_fault));
void	gsbase_load_fault(void) __asm(__STRING(gsbase_load_fault));
void	dump_add_page(vm_paddr_t);
void	dump_drop_page(vm_paddr_t);
void	identify_cpu(void);
void	initializecpu(void);
void	initializecpucache(void);
bool	intel_fix_cpuid(void);
void	fillw(int /*u_short*/ pat, void *base, size_t cnt);
void	fpstate_drop(struct thread *td);
int	is_physical_memory(vm_paddr_t addr);
int	isa_nmi(int cd);
void	panicifcpuunsupported(void);
void	pagecopy(void *from, void *to);
void	pagezero(void *addr);
void	printcpuinfo(void);
void	setidt(int idx, alias_for_inthand_t *func, int typ, int dpl, int ist);
int	user_dbreg_trap(void);
void	minidumpsys(struct dumperinfo *);
struct savefpu *get_pcb_user_save_td(struct thread *td);
struct savefpu *get_pcb_user_save_pcb(struct pcb *pcb);
struct pcb *get_pcb_td(struct thread *td);
void	amd64_db_resume_dbreg(void);

#endif /* !_MACHINE_MD_VAR_H_ */
