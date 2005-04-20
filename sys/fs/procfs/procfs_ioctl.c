/*-
 * Copyright (c) 2001 Dag-Erling Coïdan Smørgrav
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
 *
 *      $FreeBSD$
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/procfs/procfs.h>

/*
 * Process ioctls
 */
int
procfs_ioctl(PFS_IOCTL_ARGS)
{
	struct procfs_status *ps;
	int error, flags, sig;

	PROC_LOCK(p);
	error = 0;
	switch (cmd) {
	case PIOCBIS:
		p->p_stops |= *(uintptr_t *)data;
		break;
	case PIOCBIC:
		p->p_stops &= ~*(uintptr_t *)data;
		break;
	case PIOCSFL:
		flags = *(uintptr_t *)data;
		if (flags & PF_ISUGID && (error = suser(td)) != 0)
			break;
		p->p_pfsflags = flags;
		break;
	case PIOCGFL:
		*(unsigned int *)data = p->p_pfsflags;
		break;
	case PIOCWAIT:
		while (p->p_step == 0) {
			/* sleep until p stops */
			error = msleep(&p->p_stype, &p->p_mtx,
			    PWAIT|PCATCH, "pioctl", 0);
			if (error != 0)
				break;
		}
		/* fall through to PIOCSTATUS */
	case PIOCSTATUS:
		ps = (struct procfs_status *)data;
		ps->state = (p->p_step == 0);
		ps->flags = 0; /* nope */
		ps->events = p->p_stops;
		ps->why = p->p_step ? p->p_stype : 0;
		ps->val = p->p_step ? p->p_xstat : 0;
		break;
	case PIOCCONT:
		if (p->p_step == 0)
			break;
		sig = *(uintptr_t *)data;
		if (sig != 0 && !_SIG_VALID(sig)) {
			error = EINVAL;
			break;
		}
#if 0
		p->p_step = 0;
		if (P_SHOULDSTOP(p)) {
			p->p_xstat = sig;
			p->p_flag &= ~(P_STOPPED_TRACE|P_STOPPED_SIG);
			mtx_lock_spin(&sched_lock);
			thread_unsuspend(p);
			mtx_unlock_spin(&sched_lock);
		} else if (sig)
			psignal(p, sig);
#else
		if (sig)
			psignal(p, sig);
		p->p_step = 0;
		wakeup(&p->p_step);
#endif
		break;
	default:
		error = (ENOTTY);
	}
	PROC_UNLOCK(p);

	return (error);
}

/*
 * Clean up on last close
 */
int
procfs_close(PFS_CLOSE_ARGS)
{
	if (p != NULL && (p->p_pfsflags & PF_LINGER) == 0) {
		PROC_LOCK_ASSERT(p, MA_OWNED);
		p->p_pfsflags = 0;
		p->p_stops = 0;
		p->p_step = 0;
		wakeup(&p->p_step);
	}
	return (0);
}
