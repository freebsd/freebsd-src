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
 *	$Id: genassym.c,v 1.55 1998/05/17 18:53:11 tegge Exp $
 */

#include "opt_vm86.h"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <machine/frame.h>
#include <machine/bootinfo.h>
#include <machine/tss.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#define KERNEL /* XXX avoid user headers */
#include <sys/user.h>
#undef KERNEL
#include <net/if.h>
#include <netinet/in.h>
#include <nfs/nfsv2.h>
#include <nfs/rpcv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsdiskless.h>
#ifdef SMP
#include <machine/apic.h>
#endif
#ifdef VM86
#include <machine/segments.h>
#endif
#include <machine/globaldata.h>

int	main __P((void));
int	printf __P((const char *, ...));

int
main()
{
	struct proc *p = (struct proc *)0;
	struct vmmeter *vm = (struct vmmeter *)0;
	struct user *up = (struct user *)0;
	struct rusage *rup = (struct rusage *)0;
	struct uprof *uprof = (struct uprof *)0;
	struct vmspace *vms = (struct vmspace *)0;
	struct pcb *pcb = (struct pcb *)0;
	struct i386tss *tss = (struct i386tss *)0;
	struct trapframe *tf = (struct trapframe *)0;
	struct sigframe *sigf = (struct sigframe *)0;
	struct bootinfo *bootinfo = (struct bootinfo *)0;
	struct globaldata *globaldata = (struct globaldata *)0;
#ifdef SMP
	struct privatespace *privatespace = (struct privatespace *)0;
#endif

	printf("#define\tP_FORW %p\n", &p->p_procq.tqe_next);
	printf("#define\tP_BACK %p\n", &p->p_procq.tqe_prev);
	printf("#define\tP_VMSPACE %p\n", &p->p_vmspace);
	printf("#define\tVM_PMAP %p\n", &vms->vm_pmap);
	printf("#define\tP_ADDR %p\n", &p->p_addr);
	printf("#define\tP_PRI %p\n", &p->p_priority);
	printf("#define\tP_RTPRIO_TYPE %p\n", &p->p_rtprio.type);
	printf("#define\tP_RTPRIO_PRIO %p\n", &p->p_rtprio.prio);
	printf("#define\tP_STAT %p\n", &p->p_stat);
	printf("#define\tP_WCHAN %p\n", &p->p_wchan);
	printf("#define\tP_FLAG %p\n", &p->p_flag);
	printf("#define\tP_PID %p\n", &p->p_pid);
	printf("#define\tP_RUNTIME %p\n", &p->p_runtime);
#ifdef SMP
	printf("#define\tP_ONCPU %p\n", &p->p_oncpu);
	printf("#define\tP_LASTCPU %p\n", &p->p_lastcpu);
#endif
	printf("#define\tSSLEEP %d\n", SSLEEP);
	printf("#define\tSRUN %d\n", SRUN);
	printf("#define\tV_TRAP %p\n", &vm->v_trap);
	printf("#define\tV_SYSCALL %p\n", &vm->v_syscall);
	printf("#define\tV_INTR %p\n", &vm->v_intr);
	printf("#define\tUPAGES %d\n", UPAGES);
	printf("#define\tPAGE_SIZE %d\n", PAGE_SIZE);
	printf("#define\tNPTEPG %d\n", NPTEPG);
	printf("#define\tNPDEPG %d\n", NPDEPG);
	printf("#define\tPDESIZE %d\n", PDESIZE);
	printf("#define\tPTESIZE %d\n", PTESIZE);
	printf("#define\tNKPDE %d\n", NKPDE);
	printf("#define\tNKPT %d\n", NKPT);
	printf("#define\tPAGE_SHIFT %d\n", PAGE_SHIFT);
	printf("#define\tPAGE_MASK %d\n", PAGE_MASK);
	printf("#define\tPDRSHIFT %d\n", PDRSHIFT);
	printf("#define\tUSRSTACK 0x%lx\n", USRSTACK);
	printf("#define\tVM_MAXUSER_ADDRESS 0x%lx\n", VM_MAXUSER_ADDRESS);
	printf("#define\tKERNBASE 0x%x\n", KERNBASE);
	printf("#define\tMCLBYTES %d\n", MCLBYTES);
	printf("#define\tPCB_CR3 %p\n", &pcb->pcb_cr3);
	printf("#define\tPCB_EDI %p\n", &pcb->pcb_edi);
	printf("#define\tPCB_ESI %p\n", &pcb->pcb_esi);
	printf("#define\tPCB_EBP %p\n", &pcb->pcb_ebp);
	printf("#define\tPCB_ESP %p\n", &pcb->pcb_esp);
	printf("#define\tPCB_EBX %p\n", &pcb->pcb_ebx);
	printf("#define\tPCB_EIP %p\n", &pcb->pcb_eip);
	printf("#define\tTSS_ESP0 %p\n", &tss->tss_esp0);
	printf("#define\tPCB_USERLDT %p\n", &pcb->pcb_ldt);
	printf("#define\tPCB_FS %p\n", &pcb->pcb_fs);
	printf("#define\tPCB_GS %p\n", &pcb->pcb_gs);
#ifdef VM86
	printf("#define\tPCB_EXT %p\n", &pcb->pcb_ext);
#endif
#ifdef SMP
	printf("#define\tPCB_MPNEST %p\n", &pcb->pcb_mpnest);
#endif
	printf("#define\tU_PROF %p\n", &up->u_stats.p_prof);
	printf("#define\tU_PROFSCALE %p\n", &up->u_stats.p_prof.pr_scale);
	printf("#define\tPR_BASE %p\n", &uprof->pr_base);
	printf("#define\tPR_SIZE %p\n", &uprof->pr_size);
	printf("#define\tPR_OFF %p\n", &uprof->pr_off);
	printf("#define\tPR_SCALE %p\n", &uprof->pr_scale);
	printf("#define\tRU_MINFLT %p\n", &rup->ru_minflt);
	printf("#define\tPCB_FLAGS %p\n", &pcb->pcb_flags);
	printf("#define\tPCB_SAVEFPU %p\n", &pcb->pcb_savefpu);
	printf("#define\tPCB_SAVEFPU_SIZE %d\n", sizeof pcb->pcb_savefpu);
	printf("#define\tPCB_ONFAULT %p\n", &pcb->pcb_onfault);
#ifdef SMP
	printf("#define\tPCB_SIZE %d\n", sizeof(struct pcb));
#endif

	printf("#define\tTF_ES %p\n", &tf->tf_es);
	printf("#define\tTF_DS %p\n", &tf->tf_ds);
	printf("#define\tTF_EDI %p\n", &tf->tf_edi);
	printf("#define\tTF_ESI %p\n", &tf->tf_esi);
	printf("#define\tTF_EBP %p\n", &tf->tf_ebp);
	printf("#define\tTF_ISP %p\n", &tf->tf_isp);
	printf("#define\tTF_EBX %p\n", &tf->tf_ebx);
	printf("#define\tTF_EDX %p\n", &tf->tf_edx);
	printf("#define\tTF_ECX %p\n", &tf->tf_ecx);
	printf("#define\tTF_EAX %p\n", &tf->tf_eax);
	printf("#define\tTF_TRAPNO %p\n", &tf->tf_trapno);
	printf("#define\tTF_ERR %p\n", &tf->tf_err);
	printf("#define\tTF_EIP %p\n", &tf->tf_eip);
	printf("#define\tTF_CS %p\n", &tf->tf_cs);
	printf("#define\tTF_EFLAGS %p\n", &tf->tf_eflags);
	printf("#define\tTF_ESP %p\n", &tf->tf_esp);
	printf("#define\tTF_SS %p\n", &tf->tf_ss);

	printf("#define\tSIGF_SIGNUM %p\n", &sigf->sf_signum);
	printf("#define\tSIGF_CODE %p\n", &sigf->sf_code);
	printf("#define\tSIGF_SCP %p\n", &sigf->sf_scp);
	printf("#define\tSIGF_HANDLER %p\n", &sigf->sf_handler);
	printf("#define\tSIGF_SC %p\n", &sigf->sf_sc);

	printf("#define\tB_READ %d\n", B_READ);
	printf("#define\tENOENT %d\n", ENOENT);
	printf("#define\tEFAULT %d\n", EFAULT);
	printf("#define\tENAMETOOLONG %d\n", ENAMETOOLONG);
	printf("#define\tMAXPATHLEN %d\n", MAXPATHLEN);

	printf("#define\tBOOTINFO_SIZE %d\n", sizeof *bootinfo);
	printf("#define\tBI_VERSION %p\n", &bootinfo->bi_version);
	printf("#define\tBI_KERNELNAME %p\n", &bootinfo->bi_kernelname);
	printf("#define\tBI_NFS_DISKLESS %p\n", &bootinfo->bi_nfs_diskless);
	printf("#define\tBI_ENDCOMMON %p\n", &bootinfo->bi_endcommon);
	printf("#define\tNFSDISKLESS_SIZE %d\n", sizeof(struct nfs_diskless));
	printf("#define\tBI_SIZE %p\n", &bootinfo->bi_size);
	printf("#define\tBI_SYMTAB %p\n", &bootinfo->bi_symtab);
	printf("#define\tBI_ESYMTAB %p\n", &bootinfo->bi_esymtab);

	printf("#define\tGD_SIZEOF %d\n", sizeof(struct globaldata));
	printf("#define\tGD_CURPROC %d\n", &globaldata->curproc);
	printf("#define\tGD_NPXPROC %d\n", &globaldata->npxproc);
	printf("#define\tGD_CURPCB %d\n", &globaldata->curpcb);
	printf("#define\tGD_COMMON_TSS %d\n", &globaldata->common_tss);
#ifdef VM86
	printf("#define\tGD_COMMON_TSSD %d\n", &globaldata->common_tssd);
	printf("#define\tGD_PRIVATE_TSS %d\n", &globaldata->private_tss);
	printf("#define\tGD_MY_TR %d\n", &globaldata->my_tr);
#endif
#ifdef SMP
	printf("#define\tGD_CPUID %d\n", &globaldata->cpuid);
	printf("#define\tGD_CPU_LOCKID %d\n", &globaldata->cpu_lockid);
	printf("#define\tGD_OTHER_CPUS %d\n", &globaldata->other_cpus);
	printf("#define\tGD_MY_IDLEPTD %d\n", &globaldata->my_idlePTD);
	printf("#define\tGD_SS_TPR %d\n", &globaldata->ss_tpr);
	printf("#define\tGD_PRV_CMAP1 %d\n", &globaldata->prv_CMAP1);
	printf("#define\tGD_PRV_CMAP2 %d\n", &globaldata->prv_CMAP2);
	printf("#define\tGD_PRV_CMAP3 %d\n", &globaldata->prv_CMAP3);
	printf("#define\tGD_PRV_PMAP1 %d\n", &globaldata->prv_PMAP1);
	printf("#define\tGD_INSIDE_INTR %d\n", &globaldata->inside_intr);
	printf("#define\tPS_GLOBALDATA 0x%x\n", &privatespace->globaldata);
	printf("#define\tPS_PRVPT 0x%x\n", &privatespace->prvpt);
	printf("#define\tPS_LAPIC 0x%x\n", &privatespace->lapic);
	printf("#define\tPS_IDLESTACK 0x%x\n", &privatespace->idlestack);
	printf("#define\tPS_IDLESTACK_TOP 0x%x\n", &privatespace->CPAGE1);
	printf("#define\tPS_CPAGE1 0x%x\n", &privatespace->CPAGE1);
	printf("#define\tPS_CPAGE2 0x%x\n", &privatespace->CPAGE2);
	printf("#define\tPS_CPAGE3 0x%x\n", &privatespace->CPAGE3);
	printf("#define\tPS_PPAGE1 0x%x\n", &privatespace->PPAGE1);
	printf("#define\tPS_IOAPICS 0x%x\n", &privatespace->ioapics);
#endif

	return (0);
}
