#ifndef _LINUX_PTRACE_H
#define _LINUX_PTRACE_H
/* ptrace.h */
/* structs and defines to help the user use the ptrace system call. */

/* has the defines to get at the registers. */

#define PTRACE_TRACEME		   0
#define PTRACE_PEEKTEXT		   1
#define PTRACE_PEEKDATA		   2
#define PTRACE_PEEKUSR		   3
#define PTRACE_POKETEXT		   4
#define PTRACE_POKEDATA		   5
#define PTRACE_POKEUSR		   6
#define PTRACE_CONT		   7
#define PTRACE_KILL		   8
#define PTRACE_SINGLESTEP	   9

#define PTRACE_ATTACH		0x10
#define PTRACE_DETACH		0x11

#define PTRACE_SYSCALL		  24

#include <asm/ptrace.h>

#ifdef __KERNEL__

#ifndef force_successful_syscall_return
/*
 * System call handlers that, upon successful completion, need to return a
 * negative value should call force_successful_syscall_return() right before
 * returning.  On architectures where the syscall convention provides for a
 * separate error flag (e.g., alpha, ia64, ppc{,64}, sparc{,64}, possibly
 * others), this macro can be used to ensure that the error flag will not get
 * set.  On architectures which do not support a separate error flag, the macro
 * is a no-op and the spurious error condition needs to be filtered out by some
 * other means (e.g., in user-level, by passing an extra argument to the
 * syscall handler, or something along those lines).
 */
#define force_successful_syscall_return() do { } while (0)
#endif
#endif

#endif
