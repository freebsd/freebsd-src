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
 *	@(#)genassym.c	5.11 (Berkeley) 5/10/91
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00154
 * --------------------         -----   ----------------------
 *
 * 24 Apr 93	Bruce Evans/Dave Rivers	Npx-0.5 support
 *
 */
static char rcsid[] = "$Header: /usr/bill/working/sys/i386/i386/RCS/genassym.c,v 1.2 92/01/21 14:22:02 william Exp $";

#ifndef lint
static char sccsid[] = "@(#)genassym.c	5.11 (Berkeley) 5/10/91";
#endif /* not lint */

#include "sys/param.h"
#include "sys/buf.h"
#include "sys/vmmeter.h"
#include "sys/proc.h"
#include "sys/user.h"
#include "sys/mbuf.h"
#include "sys/msgbuf.h"
#include "sys/resourcevar.h"
#include "machine/cpu.h"
#include "machine/trap.h"
#include "machine/psl.h"
#include "machine/reg.h"
#include "sys/syscall.h"
#include "vm/vm_param.h"
#include "vm/vm_map.h"
#include "machine/pmap.h"

main()
{
	struct proc *p = (struct proc *)0;
	struct vmmeter *vm = (struct vmmeter *)0;
	struct user *up = (struct user *)0;
	struct rusage *rup = (struct rusage *)0;
	struct uprof *uprof = (struct uprof *)0;
	struct vmspace *vms = (struct vmspace *)0;
	vm_map_t map = (vm_map_t)0;
	pmap_t pmap = (pmap_t)0;
	struct pcb *pcb = (struct pcb *)0;
	register unsigned i;

	printf("#define\tI386_CR3PAT %d\n", I386_CR3PAT);
	printf("#define\tUDOT_SZ %d\n", sizeof(struct user));
	printf("#define\tP_LINK %d\n", &p->p_link);
	printf("#define\tP_RLINK %d\n", &p->p_rlink);
	printf("#define\tP_VMSPACE %d\n", &p->p_vmspace);
	printf("#define\tVM_PMAP %d\n", &vms->vm_pmap);
	printf("#define\tP_ADDR %d\n", &p->p_addr);
	printf("#define\tP_PRI %d\n", &p->p_pri);
	printf("#define\tP_STAT %d\n", &p->p_stat);
	printf("#define\tP_WCHAN %d\n", &p->p_wchan);
	printf("#define\tP_FLAG %d\n", &p->p_flag);
	printf("#define\tP_PID %d\n", &p->p_pid);
	printf("#define\tSSLEEP %d\n", SSLEEP);
	printf("#define\tSRUN %d\n", SRUN);
	printf("#define\tV_SWTCH %d\n", &vm->v_swtch);
	printf("#define\tV_TRAP %d\n", &vm->v_trap);
	printf("#define\tV_SYSCALL %d\n", &vm->v_syscall);
	printf("#define\tV_INTR %d\n", &vm->v_intr);
	printf("#define\tV_SOFT %d\n", &vm->v_soft);
	printf("#define\tV_PDMA %d\n", &vm->v_pdma);
	printf("#define\tV_FAULTS %d\n", &vm->v_faults);
	printf("#define\tV_PGREC %d\n", &vm->v_pgrec);
	printf("#define\tV_FASTPGREC %d\n", &vm->v_fastpgrec);
	printf("#define\tUPAGES %d\n", UPAGES);
	printf("#define\tHIGHPAGES %d\n", HIGHPAGES);
	printf("#define\tCLSIZE %d\n", CLSIZE);
	printf("#define\tNBPG %d\n", NBPG);
	printf("#define\tNPTEPG %d\n", NPTEPG);
	printf("#define\tPGSHIFT %d\n", PGSHIFT);
	printf("#define\tSYSPTSIZE %d\n", SYSPTSIZE);
	printf("#define\tUSRPTSIZE %d\n", USRPTSIZE);
	printf("#define\tUSRIOSIZE %d\n", USRIOSIZE);
#ifdef SYSVSHM
	printf("#define\tSHMMAXPGS %d\n", SHMMAXPGS);
#endif
	printf("#define\tUSRSTACK %d\n", USRSTACK);
	printf("#define\tMSGBUFPTECNT %d\n", btoc(sizeof (struct msgbuf)));
	printf("#define\tNMBCLUSTERS %d\n", NMBCLUSTERS);
	printf("#define\tMCLBYTES %d\n", MCLBYTES);
	printf("#define\tPCB_LINK %d\n", &pcb->pcb_tss.tss_link);
	printf("#define\tPCB_ESP0 %d\n", &pcb->pcb_tss.tss_esp0);
	printf("#define\tPCB_SS0 %d\n", &pcb->pcb_tss.tss_ss0);
	printf("#define\tPCB_ESP1 %d\n", &pcb->pcb_tss.tss_esp1);
	printf("#define\tPCB_SS1 %d\n", &pcb->pcb_tss.tss_ss1);
	printf("#define\tPCB_ESP2 %d\n", &pcb->pcb_tss.tss_esp2);
	printf("#define\tPCB_SS2 %d\n", &pcb->pcb_tss.tss_ss2);
	printf("#define\tPCB_CR3 %d\n", &pcb->pcb_tss.tss_cr3);
	printf("#define\tPCB_EIP %d\n", &pcb->pcb_tss.tss_eip);
	printf("#define\tPCB_EFLAGS %d\n", &pcb->pcb_tss.tss_eflags);
	printf("#define\tPCB_EAX %d\n", &pcb->pcb_tss.tss_eax);
	printf("#define\tPCB_ECX %d\n", &pcb->pcb_tss.tss_ecx);
	printf("#define\tPCB_EDX %d\n", &pcb->pcb_tss.tss_edx);
	printf("#define\tPCB_EBX %d\n", &pcb->pcb_tss.tss_ebx);
	printf("#define\tPCB_ESP %d\n", &pcb->pcb_tss.tss_esp);
	printf("#define\tPCB_EBP %d\n", &pcb->pcb_tss.tss_ebp);
	printf("#define\tPCB_ESI %d\n", &pcb->pcb_tss.tss_esi);
	printf("#define\tPCB_EDI %d\n", &pcb->pcb_tss.tss_edi);
	printf("#define\tPCB_ES %d\n", &pcb->pcb_tss.tss_es);
	printf("#define\tPCB_CS %d\n", &pcb->pcb_tss.tss_cs);
	printf("#define\tPCB_SS %d\n", &pcb->pcb_tss.tss_ss);
	printf("#define\tPCB_DS %d\n", &pcb->pcb_tss.tss_ds);
	printf("#define\tPCB_FS %d\n", &pcb->pcb_tss.tss_fs);
	printf("#define\tPCB_GS %d\n", &pcb->pcb_tss.tss_gs);
	printf("#define\tPCB_LDT %d\n", &pcb->pcb_tss.tss_ldt);
	printf("#define\tPCB_IOOPT %d\n", &pcb->pcb_tss.tss_ioopt);
	printf("#define\tNKMEMCLUSTERS %d\n", NKMEMCLUSTERS);
	printf("#define\tU_PROF %d\n", &up->u_stats.p_prof);
	printf("#define\tU_PROFSCALE %d\n", &up->u_stats.p_prof.pr_scale);
	printf("#define\tPR_BASE %d\n", &uprof->pr_base);
	printf("#define\tPR_SIZE %d\n", &uprof->pr_size);
	printf("#define\tPR_OFF %d\n", &uprof->pr_off);
	printf("#define\tPR_SCALE %d\n", &uprof->pr_scale);
	printf("#define\tRU_MINFLT %d\n", &rup->ru_minflt);
	printf("#define\tPCB_FLAGS %d\n", &pcb->pcb_flags);
	printf("#define\tPCB_SAVEFPU %d\n", &pcb->pcb_savefpu);
#ifdef notused
	printf("#define\tFP_WASUSED %d\n", FP_WASUSED);
	printf("#define\tFP_NEEDSSAVE %d\n", FP_NEEDSSAVE);
	printf("#define\tFP_NEEDSRESTORE %d\n", FP_NEEDSRESTORE);
#endif
	printf("#define\tFP_USESEMC %d\n", FP_USESEMC);
	printf("#define\tPCB_SAVEEMC %d\n", &pcb->pcb_saveemc);
	printf("#define\tPCB_CMAP2 %d\n", &pcb->pcb_cmap2);
	printf("#define\tPCB_SIGC %d\n", pcb->pcb_sigc);
	printf("#define\tPCB_IML %d\n", &pcb->pcb_iml);
	printf("#define\tPCB_ONFAULT %d\n", &pcb->pcb_onfault);

	printf("#define\tB_READ %d\n", B_READ);
	printf("#define\tENOENT %d\n", ENOENT);
	printf("#define\tEFAULT %d\n", EFAULT);
	printf("#define\tENAMETOOLONG %d\n", ENAMETOOLONG);
	exit(0);
}
