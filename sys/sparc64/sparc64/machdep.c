/*-
 * Copyright (c) 2001 Jake Burkholder.
 * All rights reserved.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>
#include <sys/sysproto.h>

#include <dev/ofw/openfirm.h>

#include <machine/md_var.h>
#include <machine/reg.h>

void sparc64_init(ofw_vec_t *ofw_vec);

int cold = 1;
long dumplo;
int Maxmem = 0;

struct mtx Giant;
struct mtx sched_lock;

struct user *proc0paddr;

void
sparc64_init(ofw_vec_t *ofw_vec)
{
	OF_init(ofw_vec);
	cninit();
	printf("hello world!!\n");
}

void
sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	TODO;
}

#ifndef	_SYS_SYSPROTO_H_
struct	sigreturn_args {
	ucontext_t *ucp;
};
#endif

int
sigreturn(struct proc *p, struct sigreturn_args *uap)
{
	TODO;
	return (0);
}

void
cpu_halt(void)
{
	TODO;
}

int
ptrace_read_u_check(struct proc *p, vm_offset_t addr, size_t len)
{
	TODO;
	return (0);
}

int
ptrace_write_u(struct proc *p, vm_offset_t off, long data)
{
	TODO;
	return (0);
}

int
ptrace_set_pc(struct proc *p, u_long addr)
{
	TODO;
	return (0);
}

int
ptrace_single_step(struct proc *p)
{
	TODO;
	return (0);
}

void
setregs(struct proc *p, u_long entry, u_long stack, u_long ps_strings)
{
	TODO;
}

void
Debugger(const char *msg)
{
	TODO;
}

int
fill_dbregs(struct proc *p, struct dbreg *dbregs)
{
	TODO;
	return (0);
}

int
set_dbregs(struct proc *p, struct dbreg *dbregs)
{
	TODO;
	return (0);
}

int
fill_regs(struct proc *p, struct reg *regs)
{
	TODO;
	return (0);
}

int
set_regs(struct proc *p, struct reg *regs)
{
	TODO;
	return (0);
}

int
fill_fpregs(struct proc *p, struct fpreg *fpregs)
{
	TODO;
	return (0);
}

int
set_fpregs(struct proc *p, struct fpreg *fpregs)
{
	TODO;
	return (0);
}
