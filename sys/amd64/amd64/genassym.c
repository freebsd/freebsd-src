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
 *	$Id: genassym.c,v 1.58 1998/05/28 09:29:55 phk Exp $
 */

#include "opt_vm86.h"

#include <stddef.h>

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

#define	OS(s, m)	((u_int)offsetof(struct s, m))

int	main __P((void));
int	printf __P((const char *, ...));

int
main()
{

	printf("#define\tP_FORW %#x\n", OS(proc, p_procq.tqe_next));
	printf("#define\tP_BACK %#x\n", OS(proc, p_procq.tqe_prev));
	printf("#define\tP_VMSPACE %#x\n", OS(proc, p_vmspace));
	printf("#define\tVM_PMAP %#x\n", OS(vmspace, vm_pmap));
	printf("#define\tP_ADDR %#x\n", OS(proc, p_addr));
	printf("#define\tP_PRI %#x\n", OS(proc, p_priority));
	printf("#define\tP_RTPRIO_TYPE %#x\n", OS(proc, p_rtprio.type));
	printf("#define\tP_RTPRIO_PRIO %#x\n", OS(proc, p_rtprio.prio));
	printf("#define\tP_STAT %#x\n", OS(proc, p_stat));
	printf("#define\tP_WCHAN %#x\n", OS(proc, p_wchan));
	printf("#define\tP_FLAG %#x\n", OS(proc, p_flag));
	printf("#define\tP_PID %#x\n", OS(proc, p_pid));
	printf("#define\tP_SWITCHTIME %#x\n", OS(proc, p_switchtime));
#ifdef SMP
	printf("#define\tP_ONCPU %#x\n", OS(proc, p_oncpu));
	printf("#define\tP_LASTCPU %#x\n", OS(proc, p_lastcpu));
#endif
	printf("#define\tSSLEEP %d\n", SSLEEP);
	printf("#define\tSRUN %d\n", SRUN);
	printf("#define\tV_TRAP %#x\n", OS(vmmeter, v_trap));
	printf("#define\tV_SYSCALL %#x\n", OS(vmmeter, v_syscall));
	printf("#define\tV_INTR %#x\n", OS(vmmeter, v_intr));
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
	printf("#define\tUSRSTACK %#x\n", USRSTACK);
	printf("#define\tVM_MAXUSER_ADDRESS %#x\n", VM_MAXUSER_ADDRESS);
	printf("#define\tKERNBASE %#x\n", KERNBASE);
	printf("#define\tMCLBYTES %d\n", MCLBYTES);
	printf("#define\tPCB_CR3 %#x\n", OS(pcb, pcb_cr3));
	printf("#define\tPCB_EDI %#x\n", OS(pcb, pcb_edi));
	printf("#define\tPCB_ESI %#x\n", OS(pcb, pcb_esi));
	printf("#define\tPCB_EBP %#x\n", OS(pcb, pcb_ebp));
	printf("#define\tPCB_ESP %#x\n", OS(pcb, pcb_esp));
	printf("#define\tPCB_EBX %#x\n", OS(pcb, pcb_ebx));
	printf("#define\tPCB_EIP %#x\n", OS(pcb, pcb_eip));
	printf("#define\tTSS_ESP0 %#x\n", OS(i386tss, tss_esp0));
	printf("#define\tPCB_USERLDT %#x\n", OS(pcb, pcb_ldt));
	printf("#define\tPCB_FS %#x\n", OS(pcb, pcb_fs));
	printf("#define\tPCB_GS %#x\n", OS(pcb, pcb_gs));
#ifdef VM86
	printf("#define\tPCB_EXT %#x\n", OS(pcb, pcb_ext));
#endif
#ifdef SMP
	printf("#define\tPCB_MPNEST %#x\n", OS(pcb, pcb_mpnest));
#endif
	printf("#define\tU_PROF %#x\n", OS(user, u_stats.p_prof));
	printf("#define\tU_PROFSCALE %#x\n",
	    OS(user, u_stats.p_prof.pr_scale));
	printf("#define\tPR_BASE %#x\n", OS(uprof, pr_base));
	printf("#define\tPR_SIZE %#x\n", OS(uprof, pr_size));
	printf("#define\tPR_OFF %#x\n", OS(uprof, pr_off));
	printf("#define\tPR_SCALE %#x\n", OS(uprof, pr_scale));
	printf("#define\tRU_MINFLT %#x\n", OS(rusage, ru_minflt));
	printf("#define\tPCB_FLAGS %#x\n", OS(pcb, pcb_flags));
	printf("#define\tPCB_SAVEFPU %#x\n", OS(pcb, pcb_savefpu));
	printf("#define\tPCB_SAVEFPU_SIZE %u\n", sizeof(struct save87));
	printf("#define\tPCB_ONFAULT %#x\n", OS(pcb, pcb_onfault));
#ifdef SMP
	printf("#define\tPCB_SIZE %u\n", sizeof(struct pcb));
#endif

	printf("#define\tTF_ES %#x\n", OS(trapframe, tf_es));
	printf("#define\tTF_DS %#x\n", OS(trapframe, tf_ds));
	printf("#define\tTF_EDI %#x\n", OS(trapframe, tf_edi));
	printf("#define\tTF_ESI %#x\n", OS(trapframe, tf_esi));
	printf("#define\tTF_EBP %#x\n", OS(trapframe, tf_ebp));
	printf("#define\tTF_ISP %#x\n", OS(trapframe, tf_isp));
	printf("#define\tTF_EBX %#x\n", OS(trapframe, tf_ebx));
	printf("#define\tTF_EDX %#x\n", OS(trapframe, tf_edx));
	printf("#define\tTF_ECX %#x\n", OS(trapframe, tf_ecx));
	printf("#define\tTF_EAX %#x\n", OS(trapframe, tf_eax));
	printf("#define\tTF_TRAPNO %#x\n", OS(trapframe, tf_trapno));
	printf("#define\tTF_ERR %#x\n", OS(trapframe, tf_err));
	printf("#define\tTF_EIP %#x\n", OS(trapframe, tf_eip));
	printf("#define\tTF_CS %#x\n", OS(trapframe, tf_cs));
	printf("#define\tTF_EFLAGS %#x\n", OS(trapframe, tf_eflags));
	printf("#define\tTF_ESP %#x\n", OS(trapframe, tf_esp));
	printf("#define\tTF_SS %#x\n", OS(trapframe, tf_ss));

	printf("#define\tSIGF_SIGNUM %#x\n", OS(sigframe, sf_signum));
	printf("#define\tSIGF_CODE %#x\n", OS(sigframe, sf_code));
	printf("#define\tSIGF_SCP %#x\n", OS(sigframe, sf_scp));
	printf("#define\tSIGF_HANDLER %#x\n", OS(sigframe, sf_handler));
	printf("#define\tSIGF_SC %#x\n", OS(sigframe, sf_sc));

	printf("#define\tB_READ %#x\n", B_READ);
	printf("#define\tENOENT %d\n", ENOENT);
	printf("#define\tEFAULT %d\n", EFAULT);
	printf("#define\tENAMETOOLONG %d\n", ENAMETOOLONG);
	printf("#define\tMAXPATHLEN %d\n", MAXPATHLEN);

	printf("#define\tBOOTINFO_SIZE %u\n", sizeof(struct bootinfo));
	printf("#define\tBI_VERSION %#x\n", OS(bootinfo, bi_version));
	printf("#define\tBI_KERNELNAME %#x\n", OS(bootinfo, bi_kernelname));
	printf("#define\tBI_NFS_DISKLESS %#x\n", OS(bootinfo, bi_nfs_diskless));
	printf("#define\tBI_ENDCOMMON %#x\n", OS(bootinfo, bi_endcommon));
	printf("#define\tNFSDISKLESS_SIZE %u\n", sizeof(struct nfs_diskless));
	printf("#define\tBI_SIZE %#x\n", OS(bootinfo, bi_size));
	printf("#define\tBI_SYMTAB %#x\n", OS(bootinfo, bi_symtab));
	printf("#define\tBI_ESYMTAB %#x\n", OS(bootinfo, bi_esymtab));

	printf("#define\tGD_SIZEOF %u\n", sizeof(struct globaldata));
	printf("#define\tGD_CURPROC %#x\n", OS(globaldata, curproc));
	printf("#define\tGD_NPXPROC %#x\n", OS(globaldata, npxproc));
	printf("#define\tGD_CURPCB %#x\n", OS(globaldata, curpcb));
	printf("#define\tGD_COMMON_TSS %#x\n", OS(globaldata, common_tss));
	printf("#define\tGD_SWITCHTIME %#x\n", OS(globaldata, switchtime));
#ifdef VM86
	printf("#define\tGD_COMMON_TSSD %#x\n", OS(globaldata, common_tssd));
	printf("#define\tGD_PRIVATE_TSS %#x\n", OS(globaldata, private_tss));
	printf("#define\tGD_MY_TR %#x\n", OS(globaldata, my_tr));
#endif
#ifdef SMP
	printf("#define\tGD_CPUID %#x\n", OS(globaldata, cpuid));
	printf("#define\tGD_CPU_LOCKID %#x\n", OS(globaldata, cpu_lockid));
	printf("#define\tGD_OTHER_CPUS %#x\n", OS(globaldata, other_cpus));
	printf("#define\tGD_MY_IDLEPTD %#x\n", OS(globaldata, my_idlePTD));
	printf("#define\tGD_SS_EFLAGS %#x\n", OS(globaldata, ss_eflags));
	printf("#define\tGD_PRV_CMAP1 %#x\n", OS(globaldata, prv_CMAP1));
	printf("#define\tGD_PRV_CMAP2 %#x\n", OS(globaldata, prv_CMAP2));
	printf("#define\tGD_PRV_CMAP3 %#x\n", OS(globaldata, prv_CMAP3));
	printf("#define\tGD_PRV_PMAP1 %#x\n", OS(globaldata, prv_PMAP1));
	printf("#define\tGD_INSIDE_INTR %#x\n", OS(globaldata, inside_intr));
	printf("#define\tPS_GLOBALDATA %#x\n", OS(privatespace, globaldata));
	printf("#define\tPS_PRVPT %#x\n", OS(privatespace, prvpt));
	printf("#define\tPS_LAPIC %#x\n", OS(privatespace, lapic));
	printf("#define\tPS_IDLESTACK %#x\n", OS(privatespace, idlestack));
	printf("#define\tPS_IDLESTACK_TOP %#x\n", OS(privatespace, CPAGE1));
	printf("#define\tPS_CPAGE1 %#x\n", OS(privatespace, CPAGE1));
	printf("#define\tPS_CPAGE2 %#x\n", OS(privatespace, CPAGE2));
	printf("#define\tPS_CPAGE3 %#x\n", OS(privatespace, CPAGE3));
	printf("#define\tPS_PPAGE1 %#x\n", OS(privatespace, PPAGE1));
	printf("#define\tPS_IOAPICS %#x\n", OS(privatespace, ioapics));
#endif

	return (0);
}
