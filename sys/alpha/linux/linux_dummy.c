/*-
 * Copyright (c) 1994-1995 Søren Schmidt
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
 *    derived from this software withough specific prior written permission
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
 *
 *  $Id: linux_dummy.c,v 1.1 1995/06/25 17:32:33 sos Exp $
 */

#include <sys/param.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>

#include <i386/linux/sysproto.h>

int
linux_setup(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): setup() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_break(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): break() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_stat(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): stat() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_fstat(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): fstat() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_mount(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): mount() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_umount(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): umount() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_stime(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): stime() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_ptrace(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): ptrace() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_pause(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): pause() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_stty(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): stty() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_gtty(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): gtty() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_nice(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): nice() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_ftime(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): ftime() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_prof(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): prof() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_signal(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): signal() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_phys(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): phys() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_lock(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): lock() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_mpx(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): mpx() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_ulimit(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): ulimit() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_olduname(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): olduname() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_ustat(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): ustat() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_ioperm(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): ioperm() not supported\n", p->p_pid);
    return 0; /* EINVAL SOS XXX */
}

int
linux_syslog(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): syslog() not supported (BSD sigreturn)\n",p->p_pid);
    return sigreturn(p, args, retval);
}

int
linux_iopl(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): iopl() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_vhangup(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): vhangup() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_idle(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): idle() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_vm86(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): vm86() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_swapoff(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): swapoff() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_sysinfo(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): sysinfo() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_sigreturn(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): sigreturn() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_clone(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): clone() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_uname(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): uname() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_modify_ldt(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): modify_ldt() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_adjtimex(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): adjtimex() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_create_module(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): create_module() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_init_module(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): init_module() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_delete_module(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): delete_module() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_get_kernel_syms(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): get_kernel_syms() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_quotactl(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): quotactl() not supported\n", p->p_pid);
    return ENOSYS;
}

int
linux_bdflush(struct proc *p, void *args, int *retval)
{
    printf("Linux-emul(%d): bdflush() not supported\n", p->p_pid);
    return ENOSYS;
}
