/*
 *
 * Portions of this code was derived from the file kern_fork.c and as such
 * is subject to the copyrights below.
 *
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
 * Copyright (c) 1996 Douglas Santry
 *
 * This code is subject to the beer copyright.  If I chance to meet you in a
 * bar and this code helped you in some way, you owe me a beer.  Only
 * in Germany will I accept domestic beer.  This code may or may not work
 * and I certainly make no claims as to its fitness for *any* purpose.
 * 
 * $Id$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/acct.h>
#include <sys/ktrace.h>
#include <sys/sysproto.h>
#include <sys/unistd.h>

#include <vm/vm.h>

#include <machine/cpu.h>

/*
 * Low level support for sleep/wakeup paradigm
 * If a timeout is specified:
 *	returns 0 if wakeup
 *	returns EAGAIN if timed out
 *	returns EINVAL if error
 *
 * If a timeout is not specified:
 *
 *	returns time waiting in ticks.
 */
int
thr_sleep(struct proc *p, struct thr_sleep_args *uap, int *retval) {
	int sleepStart;
	long long sleeptime;
	int sleepclocks;
	struct timespec ts;
	int error;

	sleepclocks = 0;
	if (uap->timeout != 0) {
		/*
		 * Get timespec struct
		 */
		if (error = copyin((caddr_t) uap->timeout, (caddr_t) &ts, sizeof ts)) {
			p->p_wakeup = 0;
			return error;
		}
		sleeptime = (long long) (hz * ts.tv_nsec);
		sleeptime /= 1000000000LL;
		sleeptime += ts.tv_sec * hz;
		sleepclocks = sleeptime;
		if (sleepclocks != sleeptime) {
			p->p_wakeup = 0;
			retval[0] = EINVAL;
			return 0;
		}
		if (sleepclocks == 0)
			sleepclocks = 1;
	}

	retval[0] = 0;
	if (p->p_wakeup == 0) {
		sleepStart = ticks;
		p->p_flag |= P_SINTR;
		error = tsleep(p, PUSER, "thrslp", sleepclocks);
		p->p_flag &= ~P_SINTR;
		if (error == EWOULDBLOCK) {
			p->p_wakeup = 0;
			retval[0] = EAGAIN;
			return 0;
		}
		if (uap->timeout == 0)
			retval[0] = ticks - sleepStart;
	}
	p->p_wakeup = 0;
	return (0);
}

int
thr_wakeup(struct proc *p, struct thr_wakeup_args *uap, int *retval) {
	struct proc *pSlave = p->p_leader;

	while(pSlave && (pSlave->p_pid != uap->pid))
		pSlave = pSlave->p_peers;

	if(pSlave == 0) {
		retval[0] = ESRCH;
		return(0);
	}

	pSlave->p_wakeup++;
	if((pSlave->p_stat == SSLEEP) && (pSlave->p_wchan == pSlave)) {
		wakeup(pSlave);
		return(0);
	}

	retval[0] = EAGAIN;
	return 0;
}

/*
 * General purpose yield system call
 */
int
yield(struct proc *p, struct yield_args *uap, int *retval) {
	int s;

	retval[0] = 0;

	s = splhigh();
	p->p_priority = MAXPRI;
	setrunqueue(p);
	mi_switch();
	splx(s);

	return(0);
}

