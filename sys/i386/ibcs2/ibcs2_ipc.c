/*-
 * Copyright (c) 1994 Søren Schmidt
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
 *	$Id: ibcs2_ipc.c,v 1.1 1994/10/14 08:53:04 sos Exp $
 */

#include <i386/ibcs2/ibcs2.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>

int
ibcs2_msgsys(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC)
		printf("IBCS2: 'msgsys'\n");
#ifdef SYSVMSG
	return msgsys(p, args, retval);
#else
	printf("IBCS2: 'msgsys' not implemented yet\n");
	return EINVAL;
#endif
}

int
ibcs2_semsys(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC)
		printf("IBCS2: 'semsys'\n");
#ifdef SYSVSEM
	return semsys(p, args, retval);
#else
	printf("IBCS2: 'semsys' not implemented yet\n");
	return EINVAL;
#endif
}

int
ibcs2_shmsys(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_MISC)
		printf("IBCS2: 'shmsys'\n");
#ifdef SYSVSHM
	return shmsys(p, args, retval);
#else
	printf("IBCS2: 'shmsys' not implemented yet\n");
	return EINVAL;
#endif
}

