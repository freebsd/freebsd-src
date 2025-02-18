/*
 * System call prototypes.
 *
 * DO NOT EDIT-- this file is automatically @generated.
 */

#ifndef _TEST_SYSPROTO_H_
#define	_TEST_SYSPROTO_H_

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/cpuset.h>
#include <sys/domainset.h>
#include <sys/_ffcounter.h>
#include <sys/_semaphore.h>
#include <sys/ucontext.h>
#include <sys/wait.h>

#include <bsm/audit_kevents.h>

struct proc;

struct thread;

#define	PAD_(t)	(sizeof(syscallarg_t) <= sizeof(t) ? \
		0 : sizeof(syscallarg_t) - sizeof(t))

#if BYTE_ORDER == LITTLE_ENDIAN
#define	PADL_(t)	0
#define	PADR_(t)	PAD_(t)
#else
#define	PADL_(t)	PAD_(t)
#define	PADR_(t)	0
#endif

#ifdef PLATFORM_FOO
struct syscall1_args {
	char arg1_l_[PADL_(int)]; int arg1; char arg1_r_[PADR_(int)];
};
#else
#endif
#ifdef PLATFORM_FOO
#else
struct syscall2_args {
	syscallarg_t dummy;
};
#endif
#ifdef PLATFORM_FOO
int	sys_syscall1(struct thread *, struct syscall1_args *);
#else
#endif
#ifdef PLATFORM_FOO
#else
int	sys_syscall2(struct thread *, struct syscall2_args *);
#endif
#define	TEST_SYS_AUE_syscall1	AUE_NULL
#define	TEST_SYS_AUE_syscall2	AUE_NULL

#undef PAD_
#undef PADL_
#undef PADR_

#endif /* !_TEST_SYSPROTO_H_ */
