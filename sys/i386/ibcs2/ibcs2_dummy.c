/*-
 * Copyright (c) 1994 Søren Schmidt
 * Copyright (c) 1994 Sean Eric Fagan
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
 *	$Id: ibcs2_dummy.c,v 1.1 1994/10/14 08:52:59 sos Exp $
 */

#include <i386/ibcs2/ibcs2.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/errno.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/reg.h>

int
ibcs2_nosys(struct proc *p, void *args, int *retval)
{
 	printf("IBCS2: no such syscall eax = %d(0x%x)\n",
 	       ((struct trapframe*)p->p_md.md_regs)->tf_eax,
 	       ((struct trapframe*)p->p_md.md_regs)->tf_eax);
 	return ENOSYS;
}

int
ibcs2_clocal(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'clocal' not implemented yet\n");
	return EINVAL;
}

int
ibcs2_sysfs(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'sysfs' not implemented yet\n");
	return EINVAL;
}

int
ibcs2_uadmin(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'uadmin' not implemented yet\n");
	return EINVAL;
}

int
ibcs2_advfs(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'advfs' not implemented yet\n");
	return EINVAL;
}

int
ibcs2_unadvfs(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'unadvfs' not implemented yet\n");
	return EINVAL;
}

int
ibcs2_libattach(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'libattach' obsolete\n");
	return EINVAL;
}

int
ibcs2_libdetach(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'libdetach' obsolete\n");
	return EINVAL;
}

int
ibcs2_plock(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'plock' not supported\n");
	return 0;
}


/*
 * getmsg/putmsg are STREAMS related system calls
 * We don't have STREAMS (yet??) but fake it anyways
 */
int
ibcs2_getmsg(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'getmsg' not supported\n");
	return 0;
}

int
ibcs2_putmsg(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'putmsg' not supported\n");
	return 0;
}

/*
 * The following are RFS system calls
 * We don't have RFS.
 */
int
ibcs2_rfdebug(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'rdebug' not supported\n");
	return EINVAL;
}

int
ibcs2_rfstart(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'rfstart' not supported\n");
	return EINVAL;
}

int
ibcs2_rfstop(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'rfstop' not supported\n");
	return EINVAL;
}

int
ibcs2_rfsys(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'rfsys' not supported\n");
	return EINVAL;
}

int
ibcs2_rmount(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'rmount' not supported\n");
	return EINVAL;
}

int
ibcs2_rumount(struct proc *p, void *args, int *retval)
{
	printf("IBCS2: 'rumount' not supported\n");
	return EINVAL;
}
