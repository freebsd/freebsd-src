#ifndef _LINUX_PRCTL_H
#define _LINUX_PRCTL_H

/* Values to pass as first argument to prctl() */

#define PR_SET_PDEATHSIG  1  /* Second arg is a signal */
#define PR_GET_PDEATHSIG  2  /* Second arg is a ptr to return the signal */

/* Get/set current->mm->dumpable */
#define PR_GET_DUMPABLE   3
#define PR_SET_DUMPABLE   4

/* Get/set unaligned access control bits (if meaningful) */
#define PR_GET_UNALIGN	  5
#define PR_SET_UNALIGN	  6
# define PR_UNALIGN_NOPRINT	1	/* silently fix up unaligned user accesses */
# define PR_UNALIGN_SIGBUS	2	/* generate SIGBUS on unaligned user access */

/* Get/set whether or not to drop capabilities on setuid() away from uid 0 */
#define PR_GET_KEEPCAPS   7
#define PR_SET_KEEPCAPS   8

/* Get/set floating-point emulation control bits (if meaningful) */
#define PR_GET_FPEMU  9
#define PR_SET_FPEMU 10
# define PR_FPEMU_NOPRINT	1	/* silently emulate fp operations accesses */
# define PR_FPEMU_SIGFPE	2	/* don't emulate fp operations, send SIGFPE instead */

/* Get/set floating-point exception mode (if meaningful) */
#define PR_GET_FPEXC	11
#define PR_SET_FPEXC	12
# define PR_FP_EXC_DISABLED	0	/* FP exceptions disabled */
# define PR_FP_EXC_NONRECOV	1	/* async non-recoverable exc. mode */
# define PR_FP_EXC_ASYNC	2	/* async recoverable exception mode */
# define PR_FP_EXC_PRECISE	3	/* precise exception mode */

#endif /* _LINUX_PRCTL_H */
