/* $FreeBSD$ */
#ifndef __LP64__
#error "this file must be compiled for LP64."
#endif

#define __ELF_WORD_SIZE 32
#define _MACHINE_ELF_WANT_32BIT
#define	_WANT_LWPINFO32

#include <sys/procfs.h>

#define	ELFCORE_COMPAT_32	1
#include "elfcore.c"

static void
elf_convert_siginfo(struct siginfo32 *sid, siginfo_t *sis)
{

	bzero(sid, sizeof(*sid));
	sid->si_signo = sis->si_signo;
	sid->si_errno = sis->si_errno;
	sid->si_code = sis->si_code;
	sid->si_pid = sis->si_pid;
	sid->si_uid = sis->si_uid;
	sid->si_status = sis->si_status;
	sid->si_addr = (uintptr_t)sis->si_addr;
#if _BYTE_ORDER == _BIG_ENDIAN
	if (sis->si_value.sival_int == 0)
		sid->si_value.sival_ptr = (uintptr_t)sis->si_value.sival_ptr;
	else
#endif
		sid->si_value.sival_int = sis->si_value.sival_int;
	sid->si_timerid = sis->si_timerid;
	sid->si_overrun = sis->si_overrun;
}

static void
elf_convert_lwpinfo(struct ptrace_lwpinfo32 *pld, struct ptrace_lwpinfo *pls)
{

	pld->pl_lwpid = pls->pl_lwpid;
	pld->pl_event = pls->pl_event;
	pld->pl_flags = pls->pl_flags;
	pld->pl_sigmask = pls->pl_sigmask;
	pld->pl_siglist = pls->pl_siglist;
	elf_convert_siginfo(&pld->pl_siginfo, &pls->pl_siginfo);
	memcpy(pld->pl_tdname, pls->pl_tdname, sizeof(pld->pl_tdname));
	pld->pl_child_pid = pls->pl_child_pid;
	pld->pl_syscall_code = pls->pl_syscall_code;
	pld->pl_syscall_narg = pls->pl_syscall_narg;
}

