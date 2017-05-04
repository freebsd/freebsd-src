/*-
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)kern_sig.c	8.7 (Berkeley) 4/18/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* XXX-BD: way to many includes... */
#include <sys/param.h>
#include <sys/ctype.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/acct.h>
#include <sys/bus.h>
#include <sys/capsicum.h>
#include <sys/condvar.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/ktrace.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/refcount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/procdesc.h>
#include <sys/posix4.h>
#include <sys/pioctl.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/sdt.h>
#include <sys/sbuf.h>
#include <sys/sleepqueue.h>
#include <sys/smp.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/timers.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <cheri/cheri.h>
#include <compat/cheriabi/cheriabi_proto.h>
#include <compat/cheriabi/cheriabi_signal.h>
#include <compat/cheriabi/cheriabi_syscall.h>
#include <compat/cheriabi/cheriabi_util.h>

#ifndef _SYS_SYSPROTO_H_
struct cheriabi_sigqueue_args {
	pid_t pid;
	int signum;
	/* union sigval_c */ void *value;
};
#endif
int
cheriabi_sigqueue(struct thread *td, struct cheriabi_sigqueue_args *uap)
{
	union sigval_c value_union;
	ksiginfo_t ksi;
	struct proc *p;
	int error;

	if ((u_int)uap->signum > _SIG_MAXSIG)
		return (EINVAL);

	/*
	 * Specification says sigqueue can only send signal to
	 * single process.
	 */
	if (uap->pid <= 0)
		return (EINVAL);

	if ((p = pfind(uap->pid)) == NULL) {
		if ((p = zpfind(uap->pid)) == NULL)
			return (ESRCH);
	}
	error = p_cansignal(td, p, uap->signum);
	if (error == 0 && uap->signum != 0) {
		ksiginfo_init(&ksi);
		ksi.ksi_flags = KSI_SIGQ;
		ksi.ksi_signo = uap->signum;
		ksi.ksi_code = SI_QUEUE;
		ksi.ksi_pid = td->td_proc->p_pid;
		ksi.ksi_uid = td->td_ucred->cr_ruid;
		cheriabi_fetch_syscall_arg(td, &value_union.sival_ptr,
		    CHERIABI_SYS_cheriabi_sigqueue, 2);
		if (td->td_proc == p) {
			ksi.ksi_value.sival_ptr = malloc(sizeof(value_union),
			    M_TEMP, M_WAITOK);
			cheri_capability_copy(ksi.ksi_value.sival_ptr,
			    &value_union.sival_ptr);
			ksi.ksi_flags |= KSI_CHERI;
		} else {
			/*
			 * Cowardly refuse to send capabilities to other
			 * processes.
			 *
			 * XXX-BD: allow untagged capablities between
			 * CheriABI processess?
			 * XXX-BD: check for tag and return EPROT if found?
			 * XXX-BD: this type punning is dubiously legal.
			 * Do we need to hide it better?
			 */
			ksi.ksi_value.sival_int = value_union.sival_int;
		}
		error = pksignal(p, ksi.ksi_signo, &ksi);
	}
	PROC_UNLOCK(p);
	return (error);
}
