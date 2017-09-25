/*-
 * Copyright (c) 2002 Doug Rabson
 * Copyright (c) 2015-2017 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/capsicum.h>
#include <sys/clock.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/file.h>		/* Must come after sys/malloc.h */
#include <sys/imgact.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/procctl.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/selinfo.h>
#include <sys/eventvar.h>	/* Must come after sys/selinfo.h */
#include <sys/pipe.h>		/* Must come after sys/selinfo.h */
#include <sys/signal.h>
#include <sys/ktrace.h>		/* Must come after sys/signal.h */
#include <sys/signalvar.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/thr.h>
#include <sys/unistd.h>
#include <sys/ucontext.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/vdso.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#ifdef INET
#include <netinet/in.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/elf.h>

#include <security/audit/audit.h>

#include <cheri/cheri.h>

#include <compat/cheriabi/cheriabi.h>
#include <compat/cheriabi/cheriabi_util.h>
#if 0
#include <compat/cheriabi/cheriabi_ipc.h>
#include <compat/cheriabi/cheriabi_misc.h>
#endif
#include <compat/cheriabi/cheriabi_signal.h>
#include <compat/cheriabi/cheriabi_proto.h>
#include <compat/cheriabi/cheriabi_syscall.h>
#include <compat/cheriabi/cheriabi_sysargmap.h>

#include <sys/cheriabi.h>

int
cheriabi_quotactl(struct thread *td, struct cheriabi_quotactl_args *uap)
{

	return (kern_quotactl(td, uap->path, uap->cmd, uap->uid, uap->arg));
}

int
cheriabi_chdir(struct thread *td, struct cheriabi_chdir_args *uap)
{

	return (kern_chdir(td, uap->path, UIO_USERSPACE));
}

int
cheriabi_open(struct thread *td, struct cheriabi_open_args *uap)
{

	return (kern_openat_c(td, AT_FDCWD, uap->path, UIO_USERSPACE,
	    uap->flags, uap->mode));
}

int
cheriabi_openat(struct thread *td, struct cheriabi_openat_args *uap)
{

	return (kern_openat_c(td, uap->fd, uap->path, UIO_USERSPACE, uap->flag,
	    uap->mode));
}

int
cheriabi_link(struct thread *td, struct cheriabi_link_args *uap)
{

	return (kern_linkat_c(td, AT_FDCWD, AT_FDCWD, uap->path, uap->to,
	    UIO_USERSPACE, FOLLOW));
}

int
cheriabi_linkat(struct thread *td, struct cheriabi_linkat_args *uap)
{
	int flag;

	flag = uap->flag;
	if (flag & ~AT_SYMLINK_FOLLOW)
		return (EINVAL);

	return (kern_linkat_c(td, uap->fd1, uap->fd2, uap->path1, uap->path2,
	    UIO_USERSPACE, (flag & AT_SYMLINK_FOLLOW) ? FOLLOW : NOFOLLOW));
}

int
cheriabi_unlink(struct thread *td, struct cheriabi_unlink_args *uap)
{

	return (kern_unlinkat_c(td, AT_FDCWD, uap->path, UIO_USERSPACE, 0));
}

int
cheriabi_unlinkat(struct thread *td, struct cheriabi_unlinkat_args *uap)
{
	int flag = uap->flag;

	if (flag & ~AT_REMOVEDIR)
		return (EINVAL);

	if (flag & AT_REMOVEDIR)
		return (kern_rmdirat_c(td, uap->fd, uap->path, UIO_USERSPACE));
	else
		return (kern_unlinkat_c(td, uap->fd, uap->path, UIO_USERSPACE,
		    0));
}

int
cheriabi_rmdir(struct thread *td, struct cheriabi_rmdir_args *uap)
{

	return (kern_rmdirat_c(td, AT_FDCWD, uap->path, UIO_USERSPACE));
}
