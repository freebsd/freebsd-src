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
 *	from: @(#)genassym.c	5.11 (Berkeley) 5/10/91
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/assym.h>
#include <sys/errno.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/signal.h>
#include <sys/smp.h>
#include <sys/systm.h>
#include <sys/ucontext.h>
#include <sys/user.h>
#include <sys/ucontext.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>

#include <machine/asi.h>
#include <machine/cache.h>
#include <machine/vmparam.h>
#include <machine/cpufunc.h>
#include <machine/fp.h>
#include <machine/frame.h>
#include <machine/fsr.h>
#include <machine/intr_machdep.h>
#include <machine/lsu.h>
#include <machine/pcb.h>
#include <machine/pstate.h>
#include <machine/setjmp.h>
#include <machine/sigframe.h>
#include <machine/smp.h>
#include <machine/tte.h>
#include <machine/tlb.h>
#include <machine/tsb.h>
#include <machine/tstate.h>
#include <machine/upa.h>
#include <machine/utrap.h>

ASSYM(KERNBASE, KERNBASE);

ASSYM(EFAULT, EFAULT);
ASSYM(ENAMETOOLONG, ENAMETOOLONG);

ASSYM(KSTACK_PAGES, KSTACK_PAGES);
ASSYM(KSTACK_GUARD_PAGES, KSTACK_GUARD_PAGES);
ASSYM(PCPU_PAGES, PCPU_PAGES);
ASSYM(UAREA_PAGES, UAREA_PAGES);
ASSYM(PAGE_SIZE, PAGE_SIZE);

ASSYM(PIL_TICK, PIL_TICK);

ASSYM(FPRS_DL, FPRS_DL);
ASSYM(FPRS_DU, FPRS_DU);
ASSYM(FPRS_FEF, FPRS_FEF);

ASSYM(LSU_VW, LSU_VW);

ASSYM(TAR_VPN_SHIFT, TAR_VPN_SHIFT);

ASSYM(TLB_DEMAP_NUCLEUS, TLB_DEMAP_NUCLEUS);
ASSYM(TLB_DEMAP_PRIMARY, TLB_DEMAP_PRIMARY);
ASSYM(TLB_DEMAP_CONTEXT, TLB_DEMAP_CONTEXT);
ASSYM(TLB_DEMAP_PAGE, TLB_DEMAP_PAGE);

ASSYM(TSB_BUCKET_ADDRESS_BITS, TSB_BUCKET_ADDRESS_BITS);
ASSYM(TSB_BUCKET_SHIFT, TSB_BUCKET_SHIFT);
ASSYM(TSB_KERNEL_MASK, TSB_KERNEL_MASK);

ASSYM(INT_SHIFT, INT_SHIFT);
ASSYM(PTR_SHIFT, PTR_SHIFT);

ASSYM(PAGE_SHIFT, PAGE_SHIFT);
ASSYM(PAGE_MASK, PAGE_MASK);

ASSYM(CPU_CLKSYNC, CPU_CLKSYNC);
ASSYM(CPU_INIT, CPU_INIT);

ASSYM(CSA_MID, offsetof(struct cpu_start_args, csa_mid));
ASSYM(CSA_PCPU, offsetof(struct cpu_start_args, csa_pcpu));
ASSYM(CSA_STATE, offsetof(struct cpu_start_args, csa_state));
ASSYM(CSA_TICK, offsetof(struct cpu_start_args, csa_tick));
ASSYM(CSA_VER, offsetof(struct cpu_start_args, csa_ver));
ASSYM(CSA_TTES, offsetof(struct cpu_start_args, csa_ttes));

ASSYM(DC_TAG_SHIFT, DC_TAG_SHIFT);
ASSYM(DC_TAG_MASK, DC_TAG_MASK);
ASSYM(DC_VALID_SHIFT, DC_VALID_SHIFT);
ASSYM(DC_VALID_MASK, DC_VALID_MASK);
ASSYM(IC_TAG_SHIFT, IC_TAG_SHIFT);
ASSYM(IC_TAG_MASK, IC_TAG_MASK);
ASSYM(IC_VALID_SHIFT, IC_VALID_SHIFT);
ASSYM(IC_VALID_MASK, IC_VALID_MASK);

ASSYM(DC_SIZE, offsetof(struct cacheinfo, dc_size));
ASSYM(DC_LINESIZE, offsetof(struct cacheinfo, dc_linesize));
ASSYM(IC_SIZE, offsetof(struct cacheinfo, ic_size));
ASSYM(IC_LINESIZE, offsetof(struct cacheinfo, ic_linesize));

ASSYM(ICA_PA, offsetof(struct ipi_cache_args, ica_pa));

ASSYM(KTR_PROC, KTR_PROC);
ASSYM(KTR_TRAP, KTR_TRAP);
ASSYM(KTR_SMP, KTR_SMP);
ASSYM(KTR_SYSC, KTR_SYSC);
ASSYM(KTR_INTR, KTR_INTR);
ASSYM(KTR_CT1, KTR_CT1);
ASSYM(KTR_CT2, KTR_CT2);
ASSYM(KTR_CT3, KTR_CT3);
ASSYM(KTR_CT4, KTR_CT4);
ASSYM(KTR_CT5, KTR_CT5);
ASSYM(KTR_CT6, KTR_CT6);
ASSYM(KTR_CT7, KTR_CT7);
ASSYM(KTR_CT8, KTR_CT8);

ASSYM(KTR_SIZEOF, sizeof(struct ktr_entry));
ASSYM(KTR_LINE, offsetof(struct ktr_entry, ktr_line));
ASSYM(KTR_FILE, offsetof(struct ktr_entry, ktr_file));
ASSYM(KTR_DESC, offsetof(struct ktr_entry, ktr_desc));
ASSYM(KTR_CPU, offsetof(struct ktr_entry, ktr_cpu));
ASSYM(KTR_TIMESTAMP, offsetof(struct ktr_entry, ktr_timestamp));
ASSYM(KTR_PARM1, offsetof(struct ktr_entry, ktr_parms[0]));
ASSYM(KTR_PARM2, offsetof(struct ktr_entry, ktr_parms[1]));
ASSYM(KTR_PARM3, offsetof(struct ktr_entry, ktr_parms[2]));
ASSYM(KTR_PARM4, offsetof(struct ktr_entry, ktr_parms[3]));
ASSYM(KTR_PARM5, offsetof(struct ktr_entry, ktr_parms[4]));
ASSYM(KTR_PARM6, offsetof(struct ktr_entry, ktr_parms[5]));

ASSYM(TTE_VPN, offsetof(struct tte, tte_vpn));
ASSYM(TTE_DATA, offsetof(struct tte, tte_data));
ASSYM(TTE_SHIFT, TTE_SHIFT);

ASSYM(TD_EXEC, TD_EXEC);
ASSYM(TD_REF, TD_REF);
ASSYM(TD_SW, TD_SW);
ASSYM(TD_L, TD_L);
ASSYM(TD_W, TD_W);

ASSYM(V_INTR, offsetof(struct vmmeter, v_intr));

ASSYM(PC_CURTHREAD, offsetof(struct pcpu, pc_curthread));
ASSYM(PC_CURPCB, offsetof(struct pcpu, pc_curpcb));
ASSYM(PC_CPUID, offsetof(struct pcpu, pc_cpuid));
ASSYM(PC_CPUMASK, offsetof(struct pcpu, pc_cpumask));
ASSYM(PC_IRHEAD, offsetof(struct pcpu, pc_irhead));
ASSYM(PC_IRTAIL, offsetof(struct pcpu, pc_irtail));
ASSYM(PC_IRFREE, offsetof(struct pcpu, pc_irfree));
ASSYM(PC_MID, offsetof(struct pcpu, pc_mid));
ASSYM(PC_TLB_CTX, offsetof(struct pcpu, pc_tlb_ctx));
ASSYM(PC_TLB_CTX_MAX, offsetof(struct pcpu, pc_tlb_ctx_max));
ASSYM(PC_TLB_CTX_MIN, offsetof(struct pcpu, pc_tlb_ctx_min));
ASSYM(PC_VMSPACE, offsetof(struct pcpu, pc_vmspace));
ASSYM(PC_SIZEOF, sizeof(struct pcpu));

ASSYM(IH_SHIFT, IH_SHIFT);

ASSYM(IRSR_BUSY, IRSR_BUSY);

ASSYM(IR_NEXT, offsetof(struct intr_request, ir_next));
ASSYM(IR_FUNC, offsetof(struct intr_request, ir_func));
ASSYM(IR_ARG, offsetof(struct intr_request, ir_arg));
ASSYM(IR_PRI, offsetof(struct intr_request, ir_pri));
ASSYM(IR_VEC, offsetof(struct intr_request, ir_vec));

ASSYM(ITA_MASK, offsetof(struct ipi_tlb_args, ita_mask));
ASSYM(ITA_PMAP, offsetof(struct ipi_tlb_args, ita_pmap));
ASSYM(ITA_START, offsetof(struct ipi_tlb_args, ita_start));
ASSYM(ITA_END, offsetof(struct ipi_tlb_args, ita_end));
ASSYM(ITA_VA, offsetof(struct ipi_tlb_args, ita_va));

ASSYM(IV_SHIFT, IV_SHIFT);
ASSYM(IV_FUNC, offsetof(struct intr_vector, iv_func));
ASSYM(IV_ARG, offsetof(struct intr_vector, iv_arg));
ASSYM(IV_PRI, offsetof(struct intr_vector, iv_pri));

ASSYM(IV_MAX, IV_MAX);

ASSYM(KEF_ASTPENDING, KEF_ASTPENDING);
ASSYM(KEF_NEEDRESCHED, KEF_NEEDRESCHED);

ASSYM(MD_UTRAP, offsetof(struct mdproc, md_utrap));

ASSYM(MTX_LOCK, offsetof(struct mtx, mtx_lock));

ASSYM(P_COMM, offsetof(struct proc, p_comm));
ASSYM(P_MD, offsetof(struct proc, p_md));
ASSYM(P_PID, offsetof(struct proc, p_pid));
ASSYM(P_SFLAG, offsetof(struct proc, p_sflag));
ASSYM(P_VMSPACE, offsetof(struct proc, p_vmspace));

ASSYM(RW_SHIFT, RW_SHIFT);

ASSYM(KE_FLAGS, offsetof(struct kse, ke_flags));

ASSYM(TD_FRAME, offsetof(struct thread, td_frame));
ASSYM(TD_KSE, offsetof(struct thread, td_kse));
ASSYM(TD_KSTACK, offsetof(struct thread, td_kstack));
ASSYM(TD_PCB, offsetof(struct thread, td_pcb));
ASSYM(TD_PROC, offsetof(struct thread, td_proc));

ASSYM(PCB_SIZEOF, sizeof(struct pcb));
ASSYM(PCB_FPSTATE, offsetof(struct pcb, pcb_fpstate));
ASSYM(PCB_FP, offsetof(struct pcb, pcb_fp));
ASSYM(PCB_PC, offsetof(struct pcb, pcb_pc));
ASSYM(PCB_Y, offsetof(struct pcb, pcb_pc));
ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));
ASSYM(PCB_NSAVED, offsetof(struct pcb, pcb_nsaved));
ASSYM(PCB_RWSP, offsetof(struct pcb, pcb_rwsp));
ASSYM(PCB_RW, offsetof(struct pcb, pcb_rw));

ASSYM(VM_PMAP, offsetof(struct vmspace, vm_pmap));
ASSYM(PM_ACTIVE, offsetof(struct pmap, pm_active));
ASSYM(PM_CONTEXT, offsetof(struct pmap, pm_context));
ASSYM(PM_TSB, offsetof(struct pmap, pm_tsb));

ASSYM(FP_FB0, offsetof(struct fpstate, fp_fb[0]));
ASSYM(FP_FB1, offsetof(struct fpstate, fp_fb[1]));
ASSYM(FP_FB2, offsetof(struct fpstate, fp_fb[2]));
ASSYM(FP_FB3, offsetof(struct fpstate, fp_fb[3]));

ASSYM(CCFSZ, sizeof(struct frame));
ASSYM(SPOFF, SPOFF);

ASSYM(SF_UC, offsetof(struct sigframe, sf_uc));

ASSYM(_JB_FP, offsetof(struct _jmp_buf, _jb[_JB_FP]));
ASSYM(_JB_PC, offsetof(struct _jmp_buf, _jb[_JB_PC]));
ASSYM(_JB_SP, offsetof(struct _jmp_buf, _jb[_JB_SP]));
ASSYM(_JB_SIGFLAG, offsetof(struct _jmp_buf, _jb[_JB_SIGFLAG]));
ASSYM(_JB_SIGMASK, offsetof(struct _jmp_buf, _jb[_JB_SIGMASK]));

ASSYM(TF_G0, offsetof(struct trapframe, tf_global[0]));
ASSYM(TF_G1, offsetof(struct trapframe, tf_global[1]));
ASSYM(TF_G2, offsetof(struct trapframe, tf_global[2]));
ASSYM(TF_G3, offsetof(struct trapframe, tf_global[3]));
ASSYM(TF_G4, offsetof(struct trapframe, tf_global[4]));
ASSYM(TF_G5, offsetof(struct trapframe, tf_global[5]));
ASSYM(TF_G6, offsetof(struct trapframe, tf_global[6]));
ASSYM(TF_G7, offsetof(struct trapframe, tf_global[7]));
ASSYM(TF_O0, offsetof(struct trapframe, tf_out[0]));
ASSYM(TF_O1, offsetof(struct trapframe, tf_out[1]));
ASSYM(TF_O2, offsetof(struct trapframe, tf_out[2]));
ASSYM(TF_O3, offsetof(struct trapframe, tf_out[3]));
ASSYM(TF_O4, offsetof(struct trapframe, tf_out[4]));
ASSYM(TF_O5, offsetof(struct trapframe, tf_out[5]));
ASSYM(TF_O6, offsetof(struct trapframe, tf_out[6]));
ASSYM(TF_O7, offsetof(struct trapframe, tf_out[7]));
ASSYM(TF_FSR, offsetof(struct trapframe, tf_fsr));
ASSYM(TF_SFAR, offsetof(struct trapframe, tf_sfar));
ASSYM(TF_LEVEL, offsetof(struct trapframe, tf_level));
ASSYM(TF_TAR, offsetof(struct trapframe, tf_tar));
ASSYM(TF_TNPC, offsetof(struct trapframe, tf_tnpc));
ASSYM(TF_TPC, offsetof(struct trapframe, tf_tpc));
ASSYM(TF_TSTATE, offsetof(struct trapframe, tf_tstate));
ASSYM(TF_SFSR, offsetof(struct trapframe, tf_sfsr));
ASSYM(TF_TYPE, offsetof(struct trapframe, tf_type));
ASSYM(TF_Y, offsetof(struct trapframe, tf_y));
ASSYM(TF_FPRS, offsetof(struct trapframe, tf_fprs));
ASSYM(TF_PIL, offsetof(struct trapframe, tf_pil));
ASSYM(TF_WSTATE, offsetof(struct trapframe, tf_wstate));
ASSYM(TF_SIZEOF, sizeof(struct trapframe));

ASSYM(UT_MAX, UT_MAX);
