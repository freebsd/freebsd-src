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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * $FreeBSD$
 */

#include "opt_user_ldt.h"

#include <stddef.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/assym.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/resourcevar.h>
/* XXX */
#ifdef KTR_PERCPU
#include <sys/ktr.h>
#endif
#include <machine/frame.h>
#include <machine/bootinfo.h>
#include <machine/tss.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <net/if.h>
#include <netinet/in.h>
#include <nfs/nfsv2.h>
#include <nfs/rpcv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsdiskless.h>
#ifdef SMP
#include <machine/apic.h>
#endif
#include <machine/segments.h>
#include <machine/sigframe.h>
#include <machine/globaldata.h>
#include <machine/vm86.h>
#include <machine/mutex.h>

ASSYM(P_VMSPACE, offsetof(struct proc, p_vmspace));
ASSYM(VM_PMAP, offsetof(struct vmspace, vm_pmap));
ASSYM(PM_ACTIVE, offsetof(struct pmap, pm_active));
ASSYM(P_ADDR, offsetof(struct proc, p_addr));
ASSYM(P_STAT, offsetof(struct proc, p_stat));
ASSYM(P_WCHAN, offsetof(struct proc, p_wchan));

#ifdef SMP
ASSYM(P_ONCPU, offsetof(struct proc, p_oncpu));
ASSYM(P_LASTCPU, offsetof(struct proc, p_lastcpu));
#endif

ASSYM(SSLEEP, SSLEEP);
ASSYM(SRUN, SRUN);
ASSYM(V_TRAP, offsetof(struct vmmeter, v_trap));
ASSYM(V_SYSCALL, offsetof(struct vmmeter, v_syscall));
ASSYM(V_INTR, offsetof(struct vmmeter, v_intr));
ASSYM(UPAGES, UPAGES);
ASSYM(PAGE_SIZE, PAGE_SIZE);
ASSYM(NPTEPG, NPTEPG);
ASSYM(NPDEPG, NPDEPG);
ASSYM(PDESIZE, PDESIZE);
ASSYM(PTESIZE, PTESIZE);
ASSYM(PAGE_SHIFT, PAGE_SHIFT);
ASSYM(PAGE_MASK, PAGE_MASK);
ASSYM(PDRSHIFT, PDRSHIFT);
ASSYM(USRSTACK, USRSTACK);
ASSYM(VM_MAXUSER_ADDRESS, VM_MAXUSER_ADDRESS);
ASSYM(KERNBASE, KERNBASE);
ASSYM(MCLBYTES, MCLBYTES);
ASSYM(PCB_CR3, offsetof(struct pcb, pcb_cr3));
ASSYM(PCB_EDI, offsetof(struct pcb, pcb_edi));
ASSYM(PCB_ESI, offsetof(struct pcb, pcb_esi));
ASSYM(PCB_EBP, offsetof(struct pcb, pcb_ebp));
ASSYM(PCB_ESP, offsetof(struct pcb, pcb_esp));
ASSYM(PCB_EBX, offsetof(struct pcb, pcb_ebx));
ASSYM(PCB_EIP, offsetof(struct pcb, pcb_eip));
ASSYM(TSS_ESP0, offsetof(struct i386tss, tss_esp0));

#ifdef USER_LDT
ASSYM(PCB_USERLDT, offsetof(struct pcb, pcb_ldt));
#endif

ASSYM(PCB_GS, offsetof(struct pcb, pcb_gs));
ASSYM(PCB_DR0, offsetof(struct pcb, pcb_dr0));
ASSYM(PCB_DR1, offsetof(struct pcb, pcb_dr1));
ASSYM(PCB_DR2, offsetof(struct pcb, pcb_dr2));
ASSYM(PCB_DR3, offsetof(struct pcb, pcb_dr3));
ASSYM(PCB_DR6, offsetof(struct pcb, pcb_dr6));
ASSYM(PCB_DR7, offsetof(struct pcb, pcb_dr7));
ASSYM(PCB_DBREGS, PCB_DBREGS);
ASSYM(PCB_EXT, offsetof(struct pcb, pcb_ext));

ASSYM(PCB_SCHEDNEST, offsetof(struct pcb, pcb_schednest));

ASSYM(PCB_SPARE, offsetof(struct pcb, __pcb_spare));
ASSYM(PCB_FLAGS, offsetof(struct pcb, pcb_flags));
ASSYM(PCB_SAVEFPU, offsetof(struct pcb, pcb_savefpu));
ASSYM(PCB_SAVEFPU_SIZE, sizeof(struct save87));
ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));

#ifdef SMP
ASSYM(PCB_SIZE, sizeof(struct pcb));
#endif

ASSYM(TF_TRAPNO, offsetof(struct trapframe, tf_trapno));
ASSYM(TF_ERR, offsetof(struct trapframe, tf_err));
ASSYM(TF_CS, offsetof(struct trapframe, tf_cs));
ASSYM(TF_EFLAGS, offsetof(struct trapframe, tf_eflags));
ASSYM(SIGF_HANDLER, offsetof(struct sigframe, sf_ahu.sf_handler));
ASSYM(SIGF_SC, offsetof(struct osigframe, sf_siginfo.si_sc));
ASSYM(SIGF_UC, offsetof(struct sigframe, sf_uc));
ASSYM(SC_PS, offsetof(struct osigcontext, sc_ps));
ASSYM(SC_FS, offsetof(struct osigcontext, sc_fs));
ASSYM(SC_GS, offsetof(struct osigcontext, sc_gs));
ASSYM(SC_TRAPNO, offsetof(struct osigcontext, sc_trapno));
ASSYM(UC_EFLAGS, offsetof(ucontext_t, uc_mcontext.mc_eflags));
ASSYM(UC_GS, offsetof(ucontext_t, uc_mcontext.mc_gs));
ASSYM(ENOENT, ENOENT);
ASSYM(EFAULT, EFAULT);
ASSYM(ENAMETOOLONG, ENAMETOOLONG);
ASSYM(MAXPATHLEN, MAXPATHLEN);
ASSYM(BOOTINFO_SIZE, sizeof(struct bootinfo));
ASSYM(BI_VERSION, offsetof(struct bootinfo, bi_version));
ASSYM(BI_KERNELNAME, offsetof(struct bootinfo, bi_kernelname));
ASSYM(BI_NFS_DISKLESS, offsetof(struct bootinfo, bi_nfs_diskless));
ASSYM(BI_ENDCOMMON, offsetof(struct bootinfo, bi_endcommon));
ASSYM(NFSDISKLESS_SIZE, sizeof(struct nfs_diskless));
ASSYM(BI_SIZE, offsetof(struct bootinfo, bi_size));
ASSYM(BI_SYMTAB, offsetof(struct bootinfo, bi_symtab));
ASSYM(BI_ESYMTAB, offsetof(struct bootinfo, bi_esymtab));
ASSYM(BI_KERNEND, offsetof(struct bootinfo, bi_kernend));
ASSYM(GD_SIZEOF, sizeof(struct globaldata));
ASSYM(GD_CURPROC, offsetof(struct globaldata, gd_curproc));
ASSYM(GD_NPXPROC, offsetof(struct globaldata, gd_npxproc));
ASSYM(GD_IDLEPROC, offsetof(struct globaldata, gd_idleproc));
ASSYM(GD_CURPCB, offsetof(struct globaldata, gd_curpcb));
ASSYM(GD_COMMON_TSS, offsetof(struct globaldata, gd_common_tss));
ASSYM(GD_SWITCHTIME, offsetof(struct globaldata, gd_switchtime));
ASSYM(GD_SWITCHTICKS, offsetof(struct globaldata, gd_switchticks));
ASSYM(GD_COMMON_TSSD, offsetof(struct globaldata, gd_common_tssd));
ASSYM(GD_TSS_GDT, offsetof(struct globaldata, gd_tss_gdt));
ASSYM(GD_ASTPENDING, offsetof(struct globaldata, gd_astpending));
ASSYM(GD_INTR_NESTING_LEVEL, offsetof(struct globaldata, gd_intr_nesting_level));

#ifdef USER_LDT
ASSYM(GD_CURRENTLDT, offsetof(struct globaldata, gd_currentldt));
#endif

ASSYM(GD_WITNESS_SPIN_CHECK, offsetof(struct globaldata, gd_witness_spin_check));

/* XXX */
#ifdef KTR_PERCPU
ASSYM(GD_KTR_IDX, offsetof(struct globaldata, gd_ktr_idx));
ASSYM(GD_KTR_BUF, offsetof(struct globaldata, gd_ktr_buf));
ASSYM(GD_KTR_BUF_DATA, offsetof(struct globaldata, gd_ktr_buf_data));
#endif

#ifdef SMP
ASSYM(GD_CPUID, offsetof(struct globaldata, gd_cpuid));
ASSYM(GD_CPU_LOCKID, offsetof(struct globaldata, gd_cpu_lockid));
ASSYM(GD_OTHER_CPUS, offsetof(struct globaldata, gd_other_cpus));
ASSYM(GD_SS_EFLAGS, offsetof(struct globaldata, gd_ss_eflags));
ASSYM(GD_INSIDE_INTR, offsetof(struct globaldata, gd_inside_intr));
ASSYM(GD_PRV_CMAP1, offsetof(struct globaldata, gd_prv_CMAP1));
ASSYM(GD_PRV_CMAP2, offsetof(struct globaldata, gd_prv_CMAP2));
ASSYM(GD_PRV_CMAP3, offsetof(struct globaldata, gd_prv_CMAP3));
ASSYM(GD_PRV_PMAP1, offsetof(struct globaldata, gd_prv_PMAP1));
ASSYM(GD_PRV_CADDR1, offsetof(struct globaldata, gd_prv_CADDR1));
ASSYM(GD_PRV_CADDR2, offsetof(struct globaldata, gd_prv_CADDR2));
ASSYM(GD_PRV_CADDR3, offsetof(struct globaldata, gd_prv_CADDR3));
ASSYM(GD_PRV_PADDR1, offsetof(struct globaldata, gd_prv_PADDR1));
ASSYM(PS_IDLESTACK, offsetof(struct privatespace, idlestack));
ASSYM(PS_IDLESTACK_TOP, sizeof(struct privatespace));
#endif

ASSYM(KCSEL, GSEL(GCODE_SEL, SEL_KPL));
ASSYM(KDSEL, GSEL(GDATA_SEL, SEL_KPL));

#ifdef SMP
ASSYM(KPSEL, GSEL(GPRIV_SEL, SEL_KPL));
#endif

ASSYM(BC32SEL, GSEL(GBIOSCODE32_SEL, SEL_KPL));
ASSYM(GPROC0_SEL, GPROC0_SEL);
ASSYM(VM86_FRAMESIZE, sizeof(struct vm86frame));

ASSYM(MTX_LOCK, offsetof(struct mtx, mtx_lock));
ASSYM(MTX_RECURSE, offsetof(struct mtx, mtx_recurse));
ASSYM(MTX_SAVEFL, offsetof(struct mtx, mtx_savefl));

ASSYM(MTX_UNOWNED, MTX_UNOWNED);
