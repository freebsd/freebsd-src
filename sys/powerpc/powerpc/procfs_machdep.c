/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993 Jan-Simon Pendry
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_machdep.c	8.3 (Berkeley) 1/27/94
 *
 * $FreeBSD$
 */

/*
 * Functions to be implemented here are:
 *
 * procfs_read_regs(proc, regs)
 *	Get the current user-visible register set from the process
 *	and copy it into the regs structure (<machine/reg.h>).
 *	The process is stopped at the time read_regs is called.
 *
 * procfs_write_regs(proc, regs)
 *	Update the current register set from the passed in regs
 *	structure.  Take care to avoid clobbering special CPU
 *	registers or privileged bits in the PSL.
 *	Depending on the architecture this may have fix-up work to do,
 *	especially if the IAR or PCW are modified.
 *	The process is stopped at the time write_regs is called.
 *
 * procfs_read_fpregs, procfs_write_fpregs
 *	deal with the floating point register set, otherwise as above.
 *
 * procfs_sstep(proc)
 *	Arrange for the process to trap after executing a single instruction.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/vnode.h>

#include <machine/md_var.h>
#include <machine/reg.h>
#include <fs/procfs/procfs.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <sys/user.h>

int
procfs_read_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	if ((p->p_flag & PS_INMEM) == 0) {
		return (EIO);
	}

	return (fill_regs(p, regs));
}

int
procfs_write_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	if ((p->p_flag & PS_INMEM) == 0) {
		return (EIO);
	}

	return (set_regs(p, regs));
}

/*
 * Ptrace doesn't support fpregs at all, and there are no security holes
 * or translations for fpregs, so we can just copy them.
 */

int
procfs_read_fpregs(p, fpregs)
	struct proc *p;
	struct fpreg *fpregs;
{
	if ((p->p_flag & PS_INMEM) == 0) {
		return (EIO);
	}

	return (fill_fpregs(p, fpregs));
}

int
procfs_write_fpregs(p, fpregs)
	struct proc *p;
	struct fpreg *fpregs;
{
	if ((p->p_flag & PS_INMEM) == 0) {
		return (EIO);
	}

	return (set_fpregs(p, fpregs));
}

int
procfs_sstep(p)
	struct proc *p;
{
	return (EINVAL);
}

/*
 * Placeholders
 */
int
procfs_read_dbregs(p, dbregs)
	struct proc *p;
	struct dbreg *dbregs;
{
	return (EIO);
}

int
procfs_write_dbregs(p, dbregs)
	struct proc *p;
	struct dbreg *dbregs;
{
	return (EIO);
}
