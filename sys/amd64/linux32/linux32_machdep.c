/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2004 Tim J. Robbins
 * Copyright (c) 2002 Doug Rabson
 * Copyright (c) 2000 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/syscallsubr.h>

#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <x86/ifunc.h>

#include <vm/pmap.h>
#include <vm/vm.h>
#include <vm/vm_map.h>

#include <security/audit/audit.h>

#include <compat/freebsd32/freebsd32_util.h>
#include <amd64/linux32/linux.h>
#include <amd64/linux32/linux32_proto.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_fork.h>
#include <compat/linux/linux_ipc.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_mmap.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>

static void	bsd_to_linux_rusage(struct rusage *ru, struct l_rusage *lru);

struct l_old_select_argv {
	l_int		nfds;
	l_uintptr_t	readfds;
	l_uintptr_t	writefds;
	l_uintptr_t	exceptfds;
	l_uintptr_t	timeout;
} __packed;

static void
bsd_to_linux_rusage(struct rusage *ru, struct l_rusage *lru)
{

	lru->ru_utime.tv_sec = ru->ru_utime.tv_sec;
	lru->ru_utime.tv_usec = ru->ru_utime.tv_usec;
	lru->ru_stime.tv_sec = ru->ru_stime.tv_sec;
	lru->ru_stime.tv_usec = ru->ru_stime.tv_usec;
	lru->ru_maxrss = ru->ru_maxrss;
	lru->ru_ixrss = ru->ru_ixrss;
	lru->ru_idrss = ru->ru_idrss;
	lru->ru_isrss = ru->ru_isrss;
	lru->ru_minflt = ru->ru_minflt;
	lru->ru_majflt = ru->ru_majflt;
	lru->ru_nswap = ru->ru_nswap;
	lru->ru_inblock = ru->ru_inblock;
	lru->ru_oublock = ru->ru_oublock;
	lru->ru_msgsnd = ru->ru_msgsnd;
	lru->ru_msgrcv = ru->ru_msgrcv;
	lru->ru_nsignals = ru->ru_nsignals;
	lru->ru_nvcsw = ru->ru_nvcsw;
	lru->ru_nivcsw = ru->ru_nivcsw;
}

int
linux_copyout_rusage(struct rusage *ru, void *uaddr)
{
	struct l_rusage lru;

	bsd_to_linux_rusage(ru, &lru);

	return (copyout(&lru, uaddr, sizeof(struct l_rusage)));
}

int
linux_readv(struct thread *td, struct linux_readv_args *uap)
{
	struct uio *auio;
	int error;

	error = freebsd32_copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_readv(td, uap->fd, auio);
	free(auio, M_IOV);
	return (error);
}

struct l_ipc_kludge {
	l_uintptr_t msgp;
	l_long msgtyp;
} __packed;

int
linux_ipc(struct thread *td, struct linux_ipc_args *args)
{

	switch (args->what & 0xFFFF) {
	case LINUX_SEMOP: {

		return (kern_semop(td, args->arg1, PTRIN(args->ptr),
		    args->arg2, NULL));
	}
	case LINUX_SEMGET: {
		struct linux_semget_args a;

		a.key = args->arg1;
		a.nsems = args->arg2;
		a.semflg = args->arg3;
		return (linux_semget(td, &a));
	}
	case LINUX_SEMCTL: {
		struct linux_semctl_args a;
		int error;

		a.semid = args->arg1;
		a.semnum = args->arg2;
		a.cmd = args->arg3;
		error = copyin(PTRIN(args->ptr), &a.arg, sizeof(a.arg));
		if (error)
			return (error);
		return (linux_semctl(td, &a));
	}
	case LINUX_SEMTIMEDOP: {
		struct linux_semtimedop_args a;

		a.semid = args->arg1;
		a.tsops = PTRIN(args->ptr);
		a.nsops = args->arg2;
		a.timeout = PTRIN(args->arg5);
		return (linux_semtimedop(td, &a));
	}
	case LINUX_MSGSND: {
		struct linux_msgsnd_args a;

		a.msqid = args->arg1;
		a.msgp = PTRIN(args->ptr);
		a.msgsz = args->arg2;
		a.msgflg = args->arg3;
		return (linux_msgsnd(td, &a));
	}
	case LINUX_MSGRCV: {
		struct linux_msgrcv_args a;

		a.msqid = args->arg1;
		a.msgsz = args->arg2;
		a.msgflg = args->arg3;
		if ((args->what >> 16) == 0) {
			struct l_ipc_kludge tmp;
			int error;

			if (args->ptr == 0)
				return (EINVAL);
			error = copyin(PTRIN(args->ptr), &tmp, sizeof(tmp));
			if (error)
				return (error);
			a.msgp = PTRIN(tmp.msgp);
			a.msgtyp = tmp.msgtyp;
		} else {
			a.msgp = PTRIN(args->ptr);
			a.msgtyp = args->arg5;
		}
		return (linux_msgrcv(td, &a));
	}
	case LINUX_MSGGET: {
		struct linux_msgget_args a;

		a.key = args->arg1;
		a.msgflg = args->arg2;
		return (linux_msgget(td, &a));
	}
	case LINUX_MSGCTL: {
		struct linux_msgctl_args a;

		a.msqid = args->arg1;
		a.cmd = args->arg2;
		a.buf = PTRIN(args->ptr);
		return (linux_msgctl(td, &a));
	}
	case LINUX_SHMAT: {
		struct linux_shmat_args a;
		l_uintptr_t addr;
		int error;

		a.shmid = args->arg1;
		a.shmaddr = PTRIN(args->ptr);
		a.shmflg = args->arg2;
		error = linux_shmat(td, &a);
		if (error != 0)
			return (error);
		addr = td->td_retval[0];
		error = copyout(&addr, PTRIN(args->arg3), sizeof(addr));
		td->td_retval[0] = 0;
		return (error);
	}
	case LINUX_SHMDT: {
		struct linux_shmdt_args a;

		a.shmaddr = PTRIN(args->ptr);
		return (linux_shmdt(td, &a));
	}
	case LINUX_SHMGET: {
		struct linux_shmget_args a;

		a.key = args->arg1;
		a.size = args->arg2;
		a.shmflg = args->arg3;
		return (linux_shmget(td, &a));
	}
	case LINUX_SHMCTL: {
		struct linux_shmctl_args a;

		a.shmid = args->arg1;
		a.cmd = args->arg2;
		a.buf = PTRIN(args->ptr);
		return (linux_shmctl(td, &a));
	}
	default:
		break;
	}

	return (EINVAL);
}

int
linux_old_select(struct thread *td, struct linux_old_select_args *args)
{
	struct l_old_select_argv linux_args;
	struct linux_select_args newsel;
	int error;

	error = copyin(args->ptr, &linux_args, sizeof(linux_args));
	if (error)
		return (error);

	newsel.nfds = linux_args.nfds;
	newsel.readfds = PTRIN(linux_args.readfds);
	newsel.writefds = PTRIN(linux_args.writefds);
	newsel.exceptfds = PTRIN(linux_args.exceptfds);
	newsel.timeout = PTRIN(linux_args.timeout);
	return (linux_select(td, &newsel));
}

int
linux_set_cloned_tls(struct thread *td, void *desc)
{
	struct l_user_desc info;
	struct pcb *pcb;
	int error;

	error = copyin(desc, &info, sizeof(struct l_user_desc));
	if (error) {
		linux_msg(td, "set_cloned_tls copyin info failed!");
	} else {
		/* We might copy out the entry_number as GUGS32_SEL. */
		info.entry_number = GUGS32_SEL;
		error = copyout(&info, desc, sizeof(struct l_user_desc));
		if (error)
			linux_msg(td, "set_cloned_tls copyout info failed!");

		pcb = td->td_pcb;
		update_pcb_bases(pcb);
		pcb->pcb_gsbase = (register_t)info.base_addr;
		td->td_frame->tf_gs = GSEL(GUGS32_SEL, SEL_UPL);
	}

	return (error);
}

int
linux_set_upcall(struct thread *td, register_t stack)
{

	if (stack)
		td->td_frame->tf_rsp = stack;

	/*
	 * The newly created Linux thread returns
	 * to the user space by the same path that a parent do.
	 */
	td->td_frame->tf_rax = 0;
	return (0);
}

int
linux_mmap2(struct thread *td, struct linux_mmap2_args *args)
{

	return (linux_mmap_common(td, PTROUT(args->addr), args->len, args->prot,
		args->flags, args->fd, (uint64_t)(uint32_t)args->pgoff *
		PAGE_SIZE));
}

int
linux_mmap(struct thread *td, struct linux_mmap_args *args)
{
	int error;
	struct l_mmap_argv linux_args;

	error = copyin(args->ptr, &linux_args, sizeof(linux_args));
	if (error)
		return (error);

	return (linux_mmap_common(td, linux_args.addr, linux_args.len,
	    linux_args.prot, linux_args.flags, linux_args.fd,
	    (uint32_t)linux_args.pgoff));
}

int
linux_mprotect(struct thread *td, struct linux_mprotect_args *uap)
{

	return (linux_mprotect_common(td, PTROUT(uap->addr), uap->len, uap->prot));
}

int
linux_madvise(struct thread *td, struct linux_madvise_args *uap)
{

	return (linux_madvise_common(td, PTROUT(uap->addr), uap->len, uap->behav));
}

int
linux_iopl(struct thread *td, struct linux_iopl_args *args)
{
	int error;

	if (args->level < 0 || args->level > 3)
		return (EINVAL);
	if ((error = priv_check(td, PRIV_IO)) != 0)
		return (error);
	if ((error = securelevel_gt(td->td_ucred, 0)) != 0)
		return (error);
	td->td_frame->tf_rflags = (td->td_frame->tf_rflags & ~PSL_IOPL) |
	    (args->level * (PSL_IOPL / 3));

	return (0);
}

int
linux_sigaction(struct thread *td, struct linux_sigaction_args *args)
{
	l_osigaction_t osa;
	l_sigaction_t act, oact;
	int error;

	if (args->nsa != NULL) {
		error = copyin(args->nsa, &osa, sizeof(l_osigaction_t));
		if (error)
			return (error);
		act.lsa_handler = osa.lsa_handler;
		act.lsa_flags = osa.lsa_flags;
		act.lsa_restorer = osa.lsa_restorer;
		LINUX_SIGEMPTYSET(act.lsa_mask);
		act.lsa_mask.__mask = osa.lsa_mask;
	}

	error = linux_do_sigaction(td, args->sig, args->nsa ? &act : NULL,
	    args->osa ? &oact : NULL);

	if (args->osa != NULL && !error) {
		osa.lsa_handler = oact.lsa_handler;
		osa.lsa_flags = oact.lsa_flags;
		osa.lsa_restorer = oact.lsa_restorer;
		osa.lsa_mask = oact.lsa_mask.__mask;
		error = copyout(&osa, args->osa, sizeof(l_osigaction_t));
	}

	return (error);
}

/*
 * Linux has two extra args, restart and oldmask.  We don't use these,
 * but it seems that "restart" is actually a context pointer that
 * enables the signal to happen with a different register set.
 */
int
linux_sigsuspend(struct thread *td, struct linux_sigsuspend_args *args)
{
	sigset_t sigmask;
	l_sigset_t mask;

	LINUX_SIGEMPTYSET(mask);
	mask.__mask = args->mask;
	linux_to_bsd_sigset(&mask, &sigmask);
	return (kern_sigsuspend(td, sigmask));
}

int
linux_pause(struct thread *td, struct linux_pause_args *args)
{
	struct proc *p = td->td_proc;
	sigset_t sigmask;

	PROC_LOCK(p);
	sigmask = td->td_sigmask;
	PROC_UNLOCK(p);
	return (kern_sigsuspend(td, sigmask));
}

int
linux_gettimeofday(struct thread *td, struct linux_gettimeofday_args *uap)
{
	struct timeval atv;
	l_timeval atv32;
	struct timezone rtz;
	int error = 0;

	if (uap->tp) {
		microtime(&atv);
		atv32.tv_sec = atv.tv_sec;
		atv32.tv_usec = atv.tv_usec;
		error = copyout(&atv32, uap->tp, sizeof(atv32));
	}
	if (error == 0 && uap->tzp != NULL) {
		rtz.tz_minuteswest = 0;
		rtz.tz_dsttime = 0;
		error = copyout(&rtz, uap->tzp, sizeof(rtz));
	}
	return (error);
}

int
linux_settimeofday(struct thread *td, struct linux_settimeofday_args *uap)
{
	l_timeval atv32;
	struct timeval atv, *tvp;
	struct timezone atz, *tzp;
	int error;

	if (uap->tp) {
		error = copyin(uap->tp, &atv32, sizeof(atv32));
		if (error)
			return (error);
		atv.tv_sec = atv32.tv_sec;
		atv.tv_usec = atv32.tv_usec;
		tvp = &atv;
	} else
		tvp = NULL;
	if (uap->tzp) {
		error = copyin(uap->tzp, &atz, sizeof(atz));
		if (error)
			return (error);
		tzp = &atz;
	} else
		tzp = NULL;
	return (kern_settimeofday(td, tvp, tzp));
}

int
linux_getrusage(struct thread *td, struct linux_getrusage_args *uap)
{
	struct rusage s;
	int error;

	error = kern_getrusage(td, uap->who, &s);
	if (error != 0)
		return (error);
	if (uap->rusage != NULL)
		error = linux_copyout_rusage(&s, uap->rusage);
	return (error);
}

int
linux_set_thread_area(struct thread *td,
    struct linux_set_thread_area_args *args)
{
	struct l_user_desc info;
	struct pcb *pcb;
	int error;

	error = copyin(args->desc, &info, sizeof(struct l_user_desc));
	if (error)
		return (error);

	/*
	 * Semantics of Linux version: every thread in the system has array
	 * of three TLS descriptors. 1st is GLIBC TLS, 2nd is WINE, 3rd unknown.
	 * This syscall loads one of the selected TLS descriptors with a value
	 * and also loads GDT descriptors 6, 7 and 8 with the content of
	 * the per-thread descriptors.
	 *
	 * Semantics of FreeBSD version: I think we can ignore that Linux has
	 * three per-thread descriptors and use just the first one.
	 * The tls_array[] is used only in [gs]et_thread_area() syscalls and
	 * for loading the GDT descriptors. We use just one GDT descriptor
	 * for TLS, so we will load just one.
	 *
	 * XXX: This doesn't work when a user space process tries to use more
	 * than one TLS segment. Comment in the Linux source says wine might
	 * do this.
	 */

	/*
	 * GLIBC reads current %gs and call set_thread_area() with it.
	 * We should let GUDATA_SEL and GUGS32_SEL proceed as well because
	 * we use these segments.
	 */
	switch (info.entry_number) {
	case GUGS32_SEL:
	case GUDATA_SEL:
	case 6:
	case -1:
		info.entry_number = GUGS32_SEL;
		break;
	default:
		return (EINVAL);
	}

	/*
	 * We have to copy out the GDT entry we use.
	 *
	 * XXX: What if a user space program does not check the return value
	 * and tries to use 6, 7 or 8?
	 */
	error = copyout(&info, args->desc, sizeof(struct l_user_desc));
	if (error)
		return (error);

	pcb = td->td_pcb;
	update_pcb_bases(pcb);
	pcb->pcb_gsbase = (register_t)info.base_addr;
	update_gdt_gsbase(td, info.base_addr);

	return (0);
}

void
bsd_to_linux_regset32(const struct reg32 *b_reg,
    struct linux_pt_regset32 *l_regset)
{

	l_regset->ebx = b_reg->r_ebx;
	l_regset->ecx = b_reg->r_ecx;
	l_regset->edx = b_reg->r_edx;
	l_regset->esi = b_reg->r_esi;
	l_regset->edi = b_reg->r_edi;
	l_regset->ebp = b_reg->r_ebp;
	l_regset->eax = b_reg->r_eax;
	l_regset->ds = b_reg->r_ds;
	l_regset->es = b_reg->r_es;
	l_regset->fs = b_reg->r_fs;
	l_regset->gs = b_reg->r_gs;
	l_regset->orig_eax = b_reg->r_eax;
	l_regset->eip = b_reg->r_eip;
	l_regset->cs = b_reg->r_cs;
	l_regset->eflags = b_reg->r_eflags;
	l_regset->esp = b_reg->r_esp;
	l_regset->ss = b_reg->r_ss;
}

int futex_xchgl_nosmap(int oparg, uint32_t *uaddr, int *oldval);
int futex_xchgl_smap(int oparg, uint32_t *uaddr, int *oldval);
DEFINE_IFUNC(, int, futex_xchgl, (int, uint32_t *, int *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    futex_xchgl_smap : futex_xchgl_nosmap);
}

int futex_addl_nosmap(int oparg, uint32_t *uaddr, int *oldval);
int futex_addl_smap(int oparg, uint32_t *uaddr, int *oldval);
DEFINE_IFUNC(, int, futex_addl, (int, uint32_t *, int *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    futex_addl_smap : futex_addl_nosmap);
}

int futex_orl_nosmap(int oparg, uint32_t *uaddr, int *oldval);
int futex_orl_smap(int oparg, uint32_t *uaddr, int *oldval);
DEFINE_IFUNC(, int, futex_orl, (int, uint32_t *, int *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    futex_orl_smap : futex_orl_nosmap);
}

int futex_andl_nosmap(int oparg, uint32_t *uaddr, int *oldval);
int futex_andl_smap(int oparg, uint32_t *uaddr, int *oldval);
DEFINE_IFUNC(, int, futex_andl, (int, uint32_t *, int *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    futex_andl_smap : futex_andl_nosmap);
}

int futex_xorl_nosmap(int oparg, uint32_t *uaddr, int *oldval);
int futex_xorl_smap(int oparg, uint32_t *uaddr, int *oldval);
DEFINE_IFUNC(, int, futex_xorl, (int, uint32_t *, int *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    futex_xorl_smap : futex_xorl_nosmap);
}

int
linux_ptrace_peekuser(struct thread *td, pid_t pid, void *addr, void *data)
{

	LINUX_RATELIMIT_MSG_OPT1("PTRACE_PEEKUSER offset %ld not implemented; "
	    "returning EINVAL", (uintptr_t)addr);
	return (EINVAL);
}

int
linux_ptrace_pokeuser(struct thread *td, pid_t pid, void *addr, void *data)
{

	LINUX_RATELIMIT_MSG_OPT1("PTRACE_POKEUSER offset %ld "
	    "not implemented; returning EINVAL", (uintptr_t)addr);
	return (EINVAL);
}
