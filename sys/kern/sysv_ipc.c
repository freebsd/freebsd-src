/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department. Originally from University of Wisconsin.
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
 * from: Utah $Hdr: uipc_shm.c 1.9 89/08/14$
 *
 *	@(#)sysv_shm.c	7.15 (Berkeley) 5/13/91
 */

/*
 * IPC routines shared by some or all of the SYSV IPC system calls.
 */

#ifndef SYSVIPC

#ifdef SYSVSHM
#define SYSVIPC
#endif

#ifdef SYSVSEM
#define SYSVIPC
#endif

#ifdef SYSVMSG
#define SYSVIPC
#endif

#endif

#ifdef SYSVIPC

#include "param.h"
#include "systm.h"
#include "kernel.h"
#include "proc.h"
#include "ipc.h"
#include "shm.h"

/*
 * Perform access checking.
 */

int
ipcaccess(ipc, mode, cred)
	register struct ipc_perm *ipc;
	int mode;
	register struct ucred *cred;
{
	register int m;

	if (cred->cr_uid == 0)
		return(0);
	/*
	 * Access check is based on only one of owner, group, public.
	 * If not owner, then check group.
	 * If not a member of the group, then check public access.
	 */
	mode &= 0700;
	m = ipc->mode;
	if (cred->cr_uid != ipc->uid && cred->cr_uid != ipc->cuid) {
		m <<= 3;
		if (!groupmember(ipc->gid, cred) &&
		    !groupmember(ipc->cgid, cred))
			m <<= 3;
	}
	if ((mode&m) == mode)
		return (0);
	return (EACCES);
}
#endif /* SYSVSHM */
