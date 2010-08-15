/*-
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *
 *	from: @(#)genassym.c	5.11 (Berkeley) 5/10/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_hwpmc_hooks.h"
#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/assym.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>
#ifdef	HWPMC_HOOKS
#include <sys/pmckern.h>
#endif
#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/resourcevar.h>
#include <sys/ucontext.h>
#include <machine/tss.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/proc.h>
#include <net/if.h>
#include <netinet/in.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsdiskless.h>
#include <machine/apicreg.h>
#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/sigframe.h>
#include <machine/proc.h>
#include <machine/segments.h>

ASSYM(P_VMSPACE, offsetof(struct proc, p_vmspace));
ASSYM(VM_PMAP, offsetof(struct vmspace, vm_pmap));
ASSYM(PM_ACTIVE, offsetof(struct pmap, pm_active));

ASSYM(P_MD, offsetof(struct proc, p_md));
ASSYM(MD_LDT, offsetof(struct mdproc, md_ldt));
ASSYM(MD_LDT_SD, offsetof(struct mdproc, md_ldt_sd));

ASSYM(TD_LOCK, offsetof(struct thread, td_lock));
ASSYM(TD_FLAGS, offsetof(struct thread, td_flags));
ASSYM(TD_PCB, offsetof(struct thread, td_pcb));
ASSYM(TD_PFLAGS, offsetof(struct thread, td_pflags));
ASSYM(TD_PROC, offsetof(struct thread, td_proc));
ASSYM(TD_TID, offsetof(struct thread, td_tid));

ASSYM(TDF_ASTPENDING, TDF_ASTPENDING);
ASSYM(TDF_NEEDRESCHED, TDF_NEEDRESCHED);

ASSYM(TDP_CALLCHAIN, TDP_CALLCHAIN);
ASSYM(TDP_KTHREAD, TDP_KTHREAD);

ASSYM(V_TRAP, offsetof(struct vmmeter, v_trap));
ASSYM(V_SYSCALL, offsetof(struct vmmeter, v_syscall));
ASSYM(V_INTR, offsetof(struct vmmeter, v_intr));
ASSYM(KSTACK_PAGES, KSTACK_PAGES);
ASSYM(PAGE_SIZE, PAGE_SIZE);
ASSYM(NPTEPG, NPTEPG);
ASSYM(NPDEPG, NPDEPG);
ASSYM(addr_PTmap, addr_PTmap);
ASSYM(addr_PDmap, addr_PDmap);
ASSYM(addr_PDPmap, addr_PDPmap);
ASSYM(addr_PML4map, addr_PML4map);
ASSYM(addr_PML4pml4e, addr_PML4pml4e);
ASSYM(PDESIZE, sizeof(pd_entry_t));
ASSYM(PTESIZE, sizeof(pt_entry_t));
ASSYM(PTESHIFT, PTESHIFT);
ASSYM(PAGE_SHIFT, PAGE_SHIFT);
ASSYM(PAGE_MASK, PAGE_MASK);
ASSYM(PDRSHIFT, PDRSHIFT);
ASSYM(PDPSHIFT, PDPSHIFT);
ASSYM(PML4SHIFT, PML4SHIFT);
ASSYM(val_KPDPI, KPDPI);
ASSYM(val_KPML4I, KPML4I);
ASSYM(val_PML4PML4I, PML4PML4I);
ASSYM(USRSTACK, USRSTACK);
ASSYM(VM_MAXUSER_ADDRESS, VM_MAXUSER_ADDRESS);
ASSYM(KERNBASE, KERNBASE);
ASSYM(DMAP_MIN_ADDRESS, DMAP_MIN_ADDRESS);
ASSYM(DMAP_MAX_ADDRESS, DMAP_MAX_ADDRESS);
ASSYM(MCLBYTES, MCLBYTES);

ASSYM(PCB_R15, offsetof(struct pcb, pcb_r15));
ASSYM(PCB_R14, offsetof(struct pcb, pcb_r14));
ASSYM(PCB_R13, offsetof(struct pcb, pcb_r13));
ASSYM(PCB_R12, offsetof(struct pcb, pcb_r12));
ASSYM(PCB_RBP, offsetof(struct pcb, pcb_rbp));
ASSYM(PCB_RSP, offsetof(struct pcb, pcb_rsp));
ASSYM(PCB_RBX, offsetof(struct pcb, pcb_rbx));
ASSYM(PCB_RIP, offsetof(struct pcb, pcb_rip));
ASSYM(PCB_FSBASE, offsetof(struct pcb, pcb_fsbase));
ASSYM(PCB_GSBASE, offsetof(struct pcb, pcb_gsbase));
ASSYM(PCB_KGSBASE, offsetof(struct pcb, pcb_kgsbase));
ASSYM(PCB_CR0, offsetof(struct pcb, pcb_cr0));
ASSYM(PCB_CR2, offsetof(struct pcb, pcb_cr2));
ASSYM(PCB_CR3, offsetof(struct pcb, pcb_cr3));
ASSYM(PCB_CR4, offsetof(struct pcb, pcb_cr4));
ASSYM(PCB_DR0, offsetof(struct pcb, pcb_dr0));
ASSYM(PCB_DR1, offsetof(struct pcb, pcb_dr1));
ASSYM(PCB_DR2, offsetof(struct pcb, pcb_dr2));
ASSYM(PCB_DR3, offsetof(struct pcb, pcb_dr3));
ASSYM(PCB_DR6, offsetof(struct pcb, pcb_dr6));
ASSYM(PCB_DR7, offsetof(struct pcb, pcb_dr7));
ASSYM(PCB_FLAGS, offsetof(struct pcb, pcb_flags));
ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));
ASSYM(PCB_GS32SD, offsetof(struct pcb, pcb_gs32sd));
ASSYM(PCB_TSSP, offsetof(struct pcb, pcb_tssp));
ASSYM(PCB_SAVEFPU, offsetof(struct pcb, pcb_save));
ASSYM(PCB_SAVEFPU_SIZE, sizeof(struct savefpu));
ASSYM(PCB_FULL_IRET, offsetof(struct pcb, pcb_full_iret));
ASSYM(PCB_GDT, offsetof(struct pcb, pcb_gdt));
ASSYM(PCB_IDT, offsetof(struct pcb, pcb_idt));
ASSYM(PCB_LDT, offsetof(struct pcb, pcb_ldt));
ASSYM(PCB_TR, offsetof(struct pcb, pcb_tr));
ASSYM(PCB_USERFPU, offsetof(struct pcb, pcb_user_save));
ASSYM(PCB_SIZE, sizeof(struct pcb));
ASSYM(PCB_DBREGS, PCB_DBREGS);
ASSYM(PCB_32BIT, PCB_32BIT);
ASSYM(PCB_GS32BIT, PCB_GS32BIT);
ASSYM(PCB_FULLCTX, PCB_FULLCTX);

ASSYM(COMMON_TSS_RSP0, offsetof(struct amd64tss, tss_rsp0));

ASSYM(TF_R15, offsetof(struct trapframe, tf_r15));
ASSYM(TF_R14, offsetof(struct trapframe, tf_r14));
ASSYM(TF_R13, offsetof(struct trapframe, tf_r13));
ASSYM(TF_R12, offsetof(struct trapframe, tf_r12));
ASSYM(TF_R11, offsetof(struct trapframe, tf_r11));
ASSYM(TF_R10, offsetof(struct trapframe, tf_r10));
ASSYM(TF_R9, offsetof(struct trapframe, tf_r9));
ASSYM(TF_R8, offsetof(struct trapframe, tf_r8));
ASSYM(TF_RDI, offsetof(struct trapframe, tf_rdi));
ASSYM(TF_RSI, offsetof(struct trapframe, tf_rsi));
ASSYM(TF_RBP, offsetof(struct trapframe, tf_rbp));
ASSYM(TF_RBX, offsetof(struct trapframe, tf_rbx));
ASSYM(TF_RDX, offsetof(struct trapframe, tf_rdx));
ASSYM(TF_RCX, offsetof(struct trapframe, tf_rcx));
ASSYM(TF_RAX, offsetof(struct trapframe, tf_rax));
ASSYM(TF_TRAPNO, offsetof(struct trapframe, tf_trapno));
ASSYM(TF_ADDR, offsetof(struct trapframe, tf_addr));
ASSYM(TF_ERR, offsetof(struct trapframe, tf_err));
ASSYM(TF_RIP, offsetof(struct trapframe, tf_rip));
ASSYM(TF_CS, offsetof(struct trapframe, tf_cs));
ASSYM(TF_RFLAGS, offsetof(struct trapframe, tf_rflags));
ASSYM(TF_RSP, offsetof(struct trapframe, tf_rsp));
ASSYM(TF_SS, offsetof(struct trapframe, tf_ss));
ASSYM(TF_DS, offsetof(struct trapframe, tf_ds));
ASSYM(TF_ES, offsetof(struct trapframe, tf_es));
ASSYM(TF_FS, offsetof(struct trapframe, tf_fs));
ASSYM(TF_GS, offsetof(struct trapframe, tf_gs));
ASSYM(TF_FLAGS, offsetof(struct trapframe, tf_flags));
ASSYM(TF_SIZE, sizeof(struct trapframe));
ASSYM(TF_HASSEGS, TF_HASSEGS);

ASSYM(SIGF_HANDLER, offsetof(struct sigframe, sf_ahu.sf_handler));
ASSYM(SIGF_UC, offsetof(struct sigframe, sf_uc));
ASSYM(UC_EFLAGS, offsetof(ucontext_t, uc_mcontext.mc_rflags));
ASSYM(ENOENT, ENOENT);
ASSYM(EFAULT, EFAULT);
ASSYM(ENAMETOOLONG, ENAMETOOLONG);
ASSYM(MAXCPU, MAXCPU);
ASSYM(MAXCOMLEN, MAXCOMLEN);
ASSYM(MAXPATHLEN, MAXPATHLEN);
ASSYM(PC_SIZEOF, sizeof(struct pcpu));
ASSYM(PC_PRVSPACE, offsetof(struct pcpu, pc_prvspace));
ASSYM(PC_CURTHREAD, offsetof(struct pcpu, pc_curthread));
ASSYM(PC_FPCURTHREAD, offsetof(struct pcpu, pc_fpcurthread));
ASSYM(PC_IDLETHREAD, offsetof(struct pcpu, pc_idlethread));
ASSYM(PC_CURPCB, offsetof(struct pcpu, pc_curpcb));
ASSYM(PC_CPUID, offsetof(struct pcpu, pc_cpuid));
ASSYM(PC_SCRATCH_RSP, offsetof(struct pcpu, pc_scratch_rsp));
ASSYM(PC_CURPMAP, offsetof(struct pcpu, pc_curpmap));
ASSYM(PC_TSSP, offsetof(struct pcpu, pc_tssp));
ASSYM(PC_RSP0, offsetof(struct pcpu, pc_rsp0));
ASSYM(PC_FS32P, offsetof(struct pcpu, pc_fs32p));
ASSYM(PC_GS32P, offsetof(struct pcpu, pc_gs32p));
ASSYM(PC_LDT, offsetof(struct pcpu, pc_ldt));
ASSYM(PC_COMMONTSSP, offsetof(struct pcpu, pc_commontssp));
ASSYM(PC_TSS, offsetof(struct pcpu, pc_tss));
 
ASSYM(LA_VER, offsetof(struct LAPIC, version));
ASSYM(LA_TPR, offsetof(struct LAPIC, tpr));
ASSYM(LA_EOI, offsetof(struct LAPIC, eoi));
ASSYM(LA_SVR, offsetof(struct LAPIC, svr));
ASSYM(LA_ICR_LO, offsetof(struct LAPIC, icr_lo));
ASSYM(LA_ICR_HI, offsetof(struct LAPIC, icr_hi));
ASSYM(LA_ISR, offsetof(struct LAPIC, isr0));

ASSYM(KCSEL, GSEL(GCODE_SEL, SEL_KPL));
ASSYM(KDSEL, GSEL(GDATA_SEL, SEL_KPL));
ASSYM(KUCSEL, GSEL(GUCODE_SEL, SEL_UPL));
ASSYM(KUDSEL, GSEL(GUDATA_SEL, SEL_UPL));
ASSYM(KUC32SEL, GSEL(GUCODE32_SEL, SEL_UPL));
ASSYM(KUF32SEL, GSEL(GUFS32_SEL, SEL_UPL));
ASSYM(KUG32SEL, GSEL(GUGS32_SEL, SEL_UPL));
ASSYM(TSSSEL, GSEL(GPROC0_SEL, SEL_KPL));
ASSYM(LDTSEL, GSEL(GUSERLDT_SEL, SEL_KPL));
ASSYM(SEL_RPL_MASK, SEL_RPL_MASK);

#ifdef	HWPMC_HOOKS
ASSYM(PMC_FN_USER_CALLCHAIN, PMC_FN_USER_CALLCHAIN);
#endif
