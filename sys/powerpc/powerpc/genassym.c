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
 *	$Id: genassym.c,v 1.2 1998/06/14 13:44:43 dfr Exp $
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <machine/frame.h>
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

int	main __P((void));
int	printf __P((const char *, ...));

#define BIG(val)	((val) > 999LL || (val) < -999LL)

#define P(name, val) \
	printf(BIG(val) ? "#define\t%s 0x%qx\n" : "#define\t%s %qd\n", name, val)

/* XXX Danger Will Robinson */ 
struct	prochd {
	struct	proc *ph_link;		/* Linked list of running processes. */
	struct	proc *ph_rlink;
};

#define OFF(name, type, elem)	P(#name, (long long) &((type*)0)->elem)
#define CONST2(name, val)	P(#name, (long long) val)
#define CONST1(name)		P(#name, (long long) name)

int
main()
{
	OFF(P_FORW,		struct proc,	p_procq.tqe_next);
	OFF(P_BACK,		struct proc,	p_procq.tqe_prev);
	OFF(P_VMSPACE,		struct proc,	p_vmspace);
	OFF(P_ADDR,		struct proc,	p_addr);
	OFF(P_PRI,		struct proc,	p_priority);
	OFF(P_RTPRIO_TYPE,	struct proc,	p_rtprio.type);
	OFF(P_RTPRIO_PRIO,	struct proc,	p_rtprio.prio);
	OFF(P_STAT,		struct proc,	p_stat);
	OFF(P_WCHAN,		struct proc,	p_wchan);
	OFF(P_FLAG,		struct proc,	p_flag);
	OFF(P_PID,		struct proc,	p_pid);
	OFF(P_SWITCHTIME,	struct proc,	p_switchtime);
	OFF(P_RUNTIME,		struct proc,	p_runtime);
	OFF(P_MD_PCBPADDR,	struct proc,	p_md.md_pcbpaddr);

	OFF(PH_LINK,		struct prochd,	ph_link);
	OFF(PH_RLINK,		struct prochd,	ph_rlink);

	CONST1(SSLEEP);
	CONST1(SRUN);

	OFF(VM_PMAP,		struct vmspace,	vm_pmap);
	OFF(V_TRAP,		struct vmmeter,	v_trap);
	OFF(V_SYSCALL,		struct vmmeter,	v_trap);
	OFF(V_INTR,		struct vmmeter,	v_trap);

	CONST1(UPAGES);
	CONST1(PAGE_SIZE);
	CONST1(PAGE_SHIFT);
	CONST1(PAGE_MASK);
	CONST1(USRSTACK);
	CONST1(VM_MAXUSER_ADDRESS);
	CONST1(KERNBASE);
	CONST1(PTLEV1I);
	CONST1(PTESIZE);

	OFF(U_PCB_ONFAULT,	struct user,	u_pcb.pcb_onfault);
	OFF(U_PCB_HWPCB_KSP,	struct user,	u_pcb.pcb_hw.apcb_ksp);
	OFF(U_PCB_CONTEXT,	struct user,	u_pcb.pcb_context);
	OFF(U_PROFSCALE,	struct user,	u_stats.p_prof.pr_scale);

	OFF(PR_BASE,		struct uprof,	pr_base);
	OFF(PR_SIZE,		struct uprof,	pr_size);
	OFF(PR_OFF,		struct uprof,	pr_off);
	OFF(PR_SCALE,		struct uprof,	pr_scale);

	OFF(RU_MINFLT,		struct rusage,	ru_minflt);
	
	OFF(PCB_HW,		struct pcb,	pcb_hw);
	OFF(PCB_CONTEXT,	struct pcb,	pcb_context);
	OFF(PCB_FP,		struct pcb,	pcb_fp);
	OFF(PCB_ONFAULT,	struct pcb,	pcb_onfault);
	OFF(PCB_ACCESSADDR,	struct pcb,	pcb_accessaddr);

	OFF(FPREG_FPR_REGS,	struct fpreg,	fpr_regs);
	OFF(FPREG_FPR_CR,	struct fpreg,	fpr_cr);

	CONST1(B_READ);
	CONST1(ENOENT);
	CONST1(EFAULT);
	CONST1(ENAMETOOLONG);
	CONST1(MAXPATHLEN);

	/* Register offsets, for stack frames. */
	CONST1(FRAME_V0),
	CONST1(FRAME_T0),
	CONST1(FRAME_T1),
	CONST1(FRAME_T2),
	CONST1(FRAME_T3),
	CONST1(FRAME_T4),
	CONST1(FRAME_T5),
	CONST1(FRAME_T6),
	CONST1(FRAME_T7),
	CONST1(FRAME_S0),
	CONST1(FRAME_S1),
	CONST1(FRAME_S2),
	CONST1(FRAME_S3),
	CONST1(FRAME_S4),
	CONST1(FRAME_S5),
	CONST1(FRAME_S6),
	CONST1(FRAME_A3),
	CONST1(FRAME_A4),
	CONST1(FRAME_A5),
	CONST1(FRAME_T8),
	CONST1(FRAME_T9),
	CONST1(FRAME_T10),
	CONST1(FRAME_T11),
	CONST1(FRAME_RA),
	CONST1(FRAME_T12),
	CONST1(FRAME_AT),
	CONST1(FRAME_SP),

	CONST1(FRAME_SW_SIZE),

	CONST1(FRAME_PS),
	CONST1(FRAME_PC),
	CONST1(FRAME_GP),
	CONST1(FRAME_A0),
	CONST1(FRAME_A1),
	CONST1(FRAME_A2),

	CONST1(FRAME_SIZE),

	/* bits of the PS register */
	CONST1(ALPHA_PSL_USERMODE);
	CONST1(ALPHA_PSL_IPL_MASK);
	CONST1(ALPHA_PSL_IPL_0);
	CONST1(ALPHA_PSL_IPL_SOFT);
	CONST1(ALPHA_PSL_IPL_HIGH);

	/* pte bits */
	CONST1(ALPHA_L1SHIFT);
	CONST1(ALPHA_L2SHIFT);
	CONST1(ALPHA_L3SHIFT);
	CONST1(ALPHA_K1SEG_BASE);
	CONST1(ALPHA_PTE_VALID);
	CONST1(ALPHA_PTE_ASM);
	CONST1(ALPHA_PTE_KR);
	CONST1(ALPHA_PTE_KW);

	/* Kernel entries */
	CONST1(ALPHA_KENTRY_ARITH);
	CONST1(ALPHA_KENTRY_MM);

	CONST1(ALPHA_KENTRY_IF);
	CONST1(ALPHA_KENTRY_UNA);

	CONST1(VPTBASE);

	return (0);
}
