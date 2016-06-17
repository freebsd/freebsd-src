/*
 * Utility to generate asm-ia64/offsets.h.
 *
 * Copyright (C) 2002-2003 Fenghua Yu <fenghua.yu@intel.com>
 * Copyright (C) 1999-2001 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * Note that this file has dual use: when building the kernel
 * natively, the file is translated into a binary and executed.  When
 * building the kernel in a cross-development environment, this file
 * gets translated into an assembly file which, in turn, is processed
 * by awk to generate offsets.h.  So if you make any changes to this
 * file, be sure to verify that the awk procedure still works (see
 * print_offsets.awk).
 */
#include <linux/config.h>

#include <linux/sched.h>

#include <asm-ia64/processor.h>
#include <asm-ia64/ptrace.h>
#include <asm-ia64/siginfo.h>
#include <asm-ia64/sigcontext.h>
#include <asm-ia64/mca.h>

#include "../kernel/sigframe.h"

#ifdef offsetof
# undef offsetof
#endif

/*
 * We _can't_ include the host's standard header file, as those are in
 *  potential conflict with the what the Linux kernel declares for the
 *  target system.
 */
extern int printf (const char *, ...);

#define offsetof(type,field)	((char *) &((type *) 0)->field - (char *) 0)

struct
  {
    const char name[256];
    unsigned long value;
  }
tab[] =
  {
    { "IA64_TASK_SIZE",			sizeof (struct task_struct) },
    { "IA64_PT_REGS_SIZE",		sizeof (struct pt_regs) },
    { "IA64_SWITCH_STACK_SIZE",		sizeof (struct switch_stack) },
    { "IA64_SIGINFO_SIZE",		sizeof (struct siginfo) },
    { "IA64_CPU_SIZE",			sizeof (struct cpuinfo_ia64) },
    { "SIGFRAME_SIZE",			sizeof (struct sigframe) },
    { "UNW_FRAME_INFO_SIZE",		sizeof (struct unw_frame_info) },
    { "", 0 },			/* spacer */
    { "IA64_TASK_PTRACE_OFFSET",	offsetof (struct task_struct, ptrace) },
    { "IA64_TASK_SIGPENDING_OFFSET",	offsetof (struct task_struct, sigpending) },
    { "IA64_TASK_NEED_RESCHED_OFFSET",	offsetof (struct task_struct, need_resched) },
    { "IA64_TASK_PROCESSOR_OFFSET",	offsetof (struct task_struct, processor) },
    { "IA64_TASK_THREAD_OFFSET",	offsetof (struct task_struct, thread) },
    { "IA64_TASK_THREAD_KSP_OFFSET",	offsetof (struct task_struct, thread.ksp) },
#ifdef CONFIG_PERFMON
    { "IA64_TASK_PFM_OVFL_BLOCK_RESET_OFFSET",
					offsetof(struct task_struct, thread.pfm_ovfl_block_reset) },
#endif
    { "IA64_TASK_PID_OFFSET",		offsetof (struct task_struct, pid) },
    { "IA64_TASK_MM_OFFSET",		offsetof (struct task_struct, mm) },
    { "IA64_PT_REGS_B6_OFFSET",		offsetof (struct pt_regs, b6) },
    { "IA64_PT_REGS_B7_OFFSET",		offsetof (struct pt_regs, b7) },
    { "IA64_PT_REGS_AR_CSD_OFFSET",	offsetof (struct pt_regs, ar_csd) },
    { "IA64_PT_REGS_AR_SSD_OFFSET",	offsetof (struct pt_regs, ar_ssd) },
    { "IA64_PT_REGS_R8_OFFSET",		offsetof (struct pt_regs, r8) },
    { "IA64_PT_REGS_R9_OFFSET",		offsetof (struct pt_regs, r9) },
    { "IA64_PT_REGS_R10_OFFSET",	offsetof (struct pt_regs, r10) },
    { "IA64_PT_REGS_R11_OFFSET",	offsetof (struct pt_regs, r11) },
    { "IA64_PT_REGS_CR_IPSR_OFFSET",	offsetof (struct pt_regs, cr_ipsr) },
    { "IA64_PT_REGS_CR_IIP_OFFSET",	offsetof (struct pt_regs, cr_iip) },
    { "IA64_PT_REGS_CR_IFS_OFFSET",	offsetof (struct pt_regs, cr_ifs) },
    { "IA64_PT_REGS_AR_UNAT_OFFSET",	offsetof (struct pt_regs, ar_unat) },
    { "IA64_PT_REGS_AR_PFS_OFFSET",	offsetof (struct pt_regs, ar_pfs) },
    { "IA64_PT_REGS_AR_RSC_OFFSET",	offsetof (struct pt_regs, ar_rsc) },
    { "IA64_PT_REGS_AR_RNAT_OFFSET",	offsetof (struct pt_regs, ar_rnat) },
    { "IA64_PT_REGS_AR_BSPSTORE_OFFSET",offsetof (struct pt_regs, ar_bspstore) },
    { "IA64_PT_REGS_PR_OFFSET",		offsetof (struct pt_regs, pr) },
    { "IA64_PT_REGS_B0_OFFSET",		offsetof (struct pt_regs, b0) },
    { "IA64_PT_REGS_LOADRS_OFFSET",	offsetof (struct pt_regs, loadrs) },
    { "IA64_PT_REGS_R1_OFFSET",		offsetof (struct pt_regs, r1) },
    { "IA64_PT_REGS_R12_OFFSET",	offsetof (struct pt_regs, r12) },
    { "IA64_PT_REGS_R13_OFFSET",	offsetof (struct pt_regs, r13) },
    { "IA64_PT_REGS_AR_FPSR_OFFSET",	offsetof (struct pt_regs, ar_fpsr) },
    { "IA64_PT_REGS_R15_OFFSET",	offsetof (struct pt_regs, r15) },
    { "IA64_PT_REGS_R14_OFFSET",	offsetof (struct pt_regs, r14) },
    { "IA64_PT_REGS_R2_OFFSET",		offsetof (struct pt_regs, r2) },
    { "IA64_PT_REGS_R3_OFFSET",		offsetof (struct pt_regs, r3) },
    { "IA64_PT_REGS_R16_OFFSET",	offsetof (struct pt_regs, r16) },
    { "IA64_PT_REGS_R17_OFFSET",	offsetof (struct pt_regs, r17) },
    { "IA64_PT_REGS_R18_OFFSET",	offsetof (struct pt_regs, r18) },
    { "IA64_PT_REGS_R19_OFFSET",	offsetof (struct pt_regs, r19) },
    { "IA64_PT_REGS_R20_OFFSET",	offsetof (struct pt_regs, r20) },
    { "IA64_PT_REGS_R21_OFFSET",	offsetof (struct pt_regs, r21) },
    { "IA64_PT_REGS_R22_OFFSET",	offsetof (struct pt_regs, r22) },
    { "IA64_PT_REGS_R23_OFFSET",	offsetof (struct pt_regs, r23) },
    { "IA64_PT_REGS_R24_OFFSET",	offsetof (struct pt_regs, r24) },
    { "IA64_PT_REGS_R25_OFFSET",	offsetof (struct pt_regs, r25) },
    { "IA64_PT_REGS_R26_OFFSET",	offsetof (struct pt_regs, r26) },
    { "IA64_PT_REGS_R27_OFFSET",	offsetof (struct pt_regs, r27) },
    { "IA64_PT_REGS_R28_OFFSET",	offsetof (struct pt_regs, r28) },
    { "IA64_PT_REGS_R29_OFFSET",	offsetof (struct pt_regs, r29) },
    { "IA64_PT_REGS_R30_OFFSET",	offsetof (struct pt_regs, r30) },
    { "IA64_PT_REGS_R31_OFFSET",	offsetof (struct pt_regs, r31) },
    { "IA64_PT_REGS_AR_CCV_OFFSET",	offsetof (struct pt_regs, ar_ccv) },
    { "IA64_PT_REGS_F6_OFFSET",		offsetof (struct pt_regs, f6) },
    { "IA64_PT_REGS_F7_OFFSET",		offsetof (struct pt_regs, f7) },
    { "IA64_PT_REGS_F8_OFFSET",		offsetof (struct pt_regs, f8) },
    { "IA64_PT_REGS_F9_OFFSET",		offsetof (struct pt_regs, f9) },
    { "IA64_PT_REGS_F10_OFFSET",	offsetof (struct pt_regs, f10) },
    { "IA64_PT_REGS_F11_OFFSET",	offsetof (struct pt_regs, f11) },
    { "IA64_SWITCH_STACK_CALLER_UNAT_OFFSET",	offsetof (struct switch_stack, caller_unat) },
    { "IA64_SWITCH_STACK_AR_FPSR_OFFSET",	offsetof (struct switch_stack, ar_fpsr) },
    { "IA64_SWITCH_STACK_F2_OFFSET",		offsetof (struct switch_stack, f2) },
    { "IA64_SWITCH_STACK_F3_OFFSET",		offsetof (struct switch_stack, f3) },
    { "IA64_SWITCH_STACK_F4_OFFSET",		offsetof (struct switch_stack, f4) },
    { "IA64_SWITCH_STACK_F5_OFFSET",		offsetof (struct switch_stack, f5) },
    { "IA64_SWITCH_STACK_F12_OFFSET",		offsetof (struct switch_stack, f12) },
    { "IA64_SWITCH_STACK_F13_OFFSET",		offsetof (struct switch_stack, f13) },
    { "IA64_SWITCH_STACK_F14_OFFSET",		offsetof (struct switch_stack, f14) },
    { "IA64_SWITCH_STACK_F15_OFFSET",		offsetof (struct switch_stack, f15) },
    { "IA64_SWITCH_STACK_F16_OFFSET",		offsetof (struct switch_stack, f16) },
    { "IA64_SWITCH_STACK_F17_OFFSET",		offsetof (struct switch_stack, f17) },
    { "IA64_SWITCH_STACK_F18_OFFSET",		offsetof (struct switch_stack, f18) },
    { "IA64_SWITCH_STACK_F19_OFFSET",		offsetof (struct switch_stack, f19) },
    { "IA64_SWITCH_STACK_F20_OFFSET",		offsetof (struct switch_stack, f20) },
    { "IA64_SWITCH_STACK_F21_OFFSET",		offsetof (struct switch_stack, f21) },
    { "IA64_SWITCH_STACK_F22_OFFSET",		offsetof (struct switch_stack, f22) },
    { "IA64_SWITCH_STACK_F23_OFFSET",		offsetof (struct switch_stack, f23) },
    { "IA64_SWITCH_STACK_F24_OFFSET",		offsetof (struct switch_stack, f24) },
    { "IA64_SWITCH_STACK_F25_OFFSET",		offsetof (struct switch_stack, f25) },
    { "IA64_SWITCH_STACK_F26_OFFSET",		offsetof (struct switch_stack, f26) },
    { "IA64_SWITCH_STACK_F27_OFFSET",		offsetof (struct switch_stack, f27) },
    { "IA64_SWITCH_STACK_F28_OFFSET",		offsetof (struct switch_stack, f28) },
    { "IA64_SWITCH_STACK_F29_OFFSET",		offsetof (struct switch_stack, f29) },
    { "IA64_SWITCH_STACK_F30_OFFSET",		offsetof (struct switch_stack, f30) },
    { "IA64_SWITCH_STACK_F31_OFFSET",		offsetof (struct switch_stack, f31) },
    { "IA64_SWITCH_STACK_R4_OFFSET",		offsetof (struct switch_stack, r4) },
    { "IA64_SWITCH_STACK_R5_OFFSET",		offsetof (struct switch_stack, r5) },
    { "IA64_SWITCH_STACK_R6_OFFSET",		offsetof (struct switch_stack, r6) },
    { "IA64_SWITCH_STACK_R7_OFFSET",		offsetof (struct switch_stack, r7) },
    { "IA64_SWITCH_STACK_B0_OFFSET",		offsetof (struct switch_stack, b0) },
    { "IA64_SWITCH_STACK_B1_OFFSET",		offsetof (struct switch_stack, b1) },
    { "IA64_SWITCH_STACK_B2_OFFSET",		offsetof (struct switch_stack, b2) },
    { "IA64_SWITCH_STACK_B3_OFFSET",		offsetof (struct switch_stack, b3) },
    { "IA64_SWITCH_STACK_B4_OFFSET",		offsetof (struct switch_stack, b4) },
    { "IA64_SWITCH_STACK_B5_OFFSET",		offsetof (struct switch_stack, b5) },
    { "IA64_SWITCH_STACK_AR_PFS_OFFSET",	offsetof (struct switch_stack, ar_pfs) },
    { "IA64_SWITCH_STACK_AR_LC_OFFSET",		offsetof (struct switch_stack, ar_lc) },
    { "IA64_SWITCH_STACK_AR_UNAT_OFFSET",	offsetof (struct switch_stack, ar_unat) },
    { "IA64_SWITCH_STACK_AR_RNAT_OFFSET",	offsetof (struct switch_stack, ar_rnat) },
    { "IA64_SWITCH_STACK_AR_BSPSTORE_OFFSET",	offsetof (struct switch_stack, ar_bspstore) },
    { "IA64_SWITCH_STACK_PR_OFFSET",	offsetof (struct switch_stack, pr) },
    { "IA64_SIGCONTEXT_IP_OFFSET",	offsetof (struct sigcontext, sc_ip) },
    { "IA64_SIGCONTEXT_AR_BSP_OFFSET",	offsetof (struct sigcontext, sc_ar_bsp) },
    { "IA64_SIGCONTEXT_AR_FPSR_OFFSET", offsetof (struct sigcontext, sc_ar_fpsr) },
    { "IA64_SIGCONTEXT_AR_RNAT_OFFSET",	offsetof (struct sigcontext, sc_ar_rnat) },
    { "IA64_SIGCONTEXT_AR_UNAT_OFFSET", offsetof (struct sigcontext, sc_ar_unat) },
    { "IA64_SIGCONTEXT_B0_OFFSET",	offsetof (struct sigcontext, sc_br[0]) },
    { "IA64_SIGCONTEXT_CFM_OFFSET",	offsetof (struct sigcontext, sc_cfm) },
    { "IA64_SIGCONTEXT_FLAGS_OFFSET",	offsetof (struct sigcontext, sc_flags) },
    { "IA64_SIGCONTEXT_FR6_OFFSET",	offsetof (struct sigcontext, sc_fr[6]) },
    { "IA64_SIGCONTEXT_PR_OFFSET",	offsetof (struct sigcontext, sc_pr) },
    { "IA64_SIGCONTEXT_R12_OFFSET",	offsetof (struct sigcontext, sc_gr[12]) },
    { "IA64_SIGCONTEXT_RBS_BASE_OFFSET",offsetof (struct sigcontext, sc_rbs_base) },
    { "IA64_SIGCONTEXT_LOADRS_OFFSET",	offsetof (struct sigcontext, sc_loadrs) },
    { "IA64_SIGFRAME_ARG0_OFFSET",		offsetof (struct sigframe, arg0) },
    { "IA64_SIGFRAME_ARG1_OFFSET",		offsetof (struct sigframe, arg1) },
    { "IA64_SIGFRAME_ARG2_OFFSET",		offsetof (struct sigframe, arg2) },
    { "IA64_SIGFRAME_HANDLER_OFFSET",		offsetof (struct sigframe, handler) },
    { "IA64_SIGFRAME_SIGCONTEXT_OFFSET",	offsetof (struct sigframe, sc) },
    { "IA64_CLONE_VFORK",		CLONE_VFORK },
    { "IA64_CLONE_VM",			CLONE_VM },
    { "IA64_CPU_IRQ_COUNT_OFFSET",	offsetof (struct cpuinfo_ia64, irq_stat.f.irq_count) },
    { "IA64_CPU_BH_COUNT_OFFSET",	offsetof (struct cpuinfo_ia64, irq_stat.f.bh_count) },
    { "IA64_CPU_PHYS_STACKED_SIZE_P8_OFFSET",offsetof (struct cpuinfo_ia64, phys_stacked_size_p8)},
    { "IA64_MCA_TLB_INFO_SIZE",		sizeof (struct ia64_mca_tlb_info) },
};

static const char *tabs = "\t\t\t\t\t\t\t\t\t\t";

int
main (int argc, char **argv)
{
  const char *space;
  int i, num_tabs;
  size_t len;

  printf ("#ifndef _ASM_IA64_OFFSETS_H\n");
  printf ("#define _ASM_IA64_OFFSETS_H\n\n");

  printf ("/*\n * DO NOT MODIFY\n *\n * This file was generated by "
	  "arch/ia64/tools/print_offsets.\n *\n */\n\n");

  /* This is stretching things a bit, but entry.S needs the bit number
     for PT_PTRACED and it can't include <linux/sched.h> so this seems
     like a reasonably solution.  At least the code won't break in
     subtle ways should PT_PTRACED ever change.  Ditto for
     PT_TRACESYS_BIT. */
  printf ("#define PT_PTRACED_BIT\t\t\t%u\n", ffs (PT_PTRACED) - 1);
  printf ("#define PT_TRACESYS_BIT\t\t\t%u\n\n", ffs (PT_TRACESYS) - 1);

  for (i = 0; i < sizeof (tab) / sizeof (tab[0]); ++i)
    {
      if (tab[i].name[0] == '\0')
	printf ("\n");
      else
	{
	  len = strlen (tab[i].name);

	  num_tabs = (40 - len) / 8;
	  if (num_tabs <= 0)
	    space = " ";
	  else
	    space = strchr(tabs, '\0') - (40 - len) / 8;

	  printf ("#define %s%s%lu\t/* 0x%lx */\n",
		  tab[i].name, space, tab[i].value, tab[i].value);
	}
    }

  printf ("\n#endif /* _ASM_IA64_OFFSETS_H */\n");
  return 0;
}
