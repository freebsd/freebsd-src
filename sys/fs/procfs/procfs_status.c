/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)procfs_status.c	8.4 (Berkeley) 6/15/94
 *
 * From:
 *	$Id: procfs_status.c,v 3.1 1993/12/15 09:40:17 jsp Exp $
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/jail.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sbuf.h>
#include <sys/sysent.h>
#include <sys/tty.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/procfs/procfs.h>

int
procfs_doprocstatus(PFS_FILL_ARGS)
{
	struct session *sess;
	struct thread *tdfirst;
	struct tty *tp;
	struct ucred *cr;
	const char *wmesg;
	char *pc;
	char *sep;
	int pid, ppid, pgid, sid;
	int i;

	pid = p->p_pid;
	PROC_LOCK(p);
	ppid = p->p_pptr ? p->p_pptr->p_pid : 0;
	pgid = p->p_pgrp->pg_id;
	sess = p->p_pgrp->pg_session;
	SESS_LOCK(sess);
	sid = sess->s_leader ? sess->s_leader->p_pid : 0;

/* comm pid ppid pgid sid maj,min ctty,sldr start ut st wmsg 
                                euid ruid rgid,egid,groups[1 .. NGROUPS]
*/

	pc = p->p_comm;
	do {
		if (*pc < 33 || *pc > 126 || *pc == '\\')
			sbuf_printf(sb, "\\%03o", *pc);
		else
			sbuf_putc(sb, *pc);
	} while (*++pc);
	sbuf_printf(sb, " %d %d %d %d ", pid, ppid, pgid, sid);
	if ((p->p_flag & P_CONTROLT) && (tp = sess->s_ttyp))
		sbuf_printf(sb, "%d,%d ", major(tp->t_dev), minor(tp->t_dev));
	else
		sbuf_printf(sb, "%d,%d ", -1, -1);

	sep = "";
	if (sess->s_ttyvp) {
		sbuf_printf(sb, "%sctty", sep);
		sep = ",";
	}
	if (SESS_LEADER(p)) {
		sbuf_printf(sb, "%ssldr", sep);
		sep = ",";
	}
	SESS_UNLOCK(sess);
	if (*sep != ',') {
		sbuf_printf(sb, "noflags");
	}

	mtx_lock_spin(&sched_lock);
	if (p->p_flag & P_SA)
		wmesg = "-kse- ";
	else {
		tdfirst = FIRST_THREAD_IN_PROC(p);
		if (tdfirst->td_wchan != NULL) {
			KASSERT(tdfirst->td_wmesg != NULL,
			    ("wchan %p has no wmesg", tdfirst->td_wchan));
			wmesg = tdfirst->td_wmesg;
		} else
			wmesg = "nochan";
	}

	if (p->p_sflag & PS_INMEM) {
		struct timeval start, ut, st;

		calcru(p, &ut, &st, (struct timeval *) NULL);
		mtx_unlock_spin(&sched_lock);
		start = p->p_stats->p_start;
		timevaladd(&start, &boottime);
		sbuf_printf(sb, " %ld,%ld %ld,%ld %ld,%ld",
		    start.tv_sec, start.tv_usec,
		    ut.tv_sec, ut.tv_usec,
		    st.tv_sec, st.tv_usec);
	} else {
		mtx_unlock_spin(&sched_lock);
		sbuf_printf(sb, " -1,-1 -1,-1 -1,-1");
	}

	sbuf_printf(sb, " %s", wmesg);

	cr = p->p_ucred;

	sbuf_printf(sb, " %lu %lu %lu",
		(u_long)cr->cr_uid,
		(u_long)cr->cr_ruid,
		(u_long)cr->cr_rgid);

	/* egid (cr->cr_svgid) is equal to cr_ngroups[0] 
	   see also getegid(2) in /sys/kern/kern_prot.c */

	for (i = 0; i < cr->cr_ngroups; i++) {
		sbuf_printf(sb, ",%lu", (u_long)cr->cr_groups[i]);
	}

	if (jailed(p->p_ucred)) {
		mtx_lock(&p->p_ucred->cr_prison->pr_mtx);
		sbuf_printf(sb, " %s", p->p_ucred->cr_prison->pr_host);
		mtx_unlock(&p->p_ucred->cr_prison->pr_mtx);
	} else {
		sbuf_printf(sb, " -");
	}
	PROC_UNLOCK(p);
	sbuf_printf(sb, "\n");

	return (0);
}

int
procfs_doproccmdline(PFS_FILL_ARGS)
{
	struct ps_strings pstr;
	int error, i;

	/*
	 * If we are using the ps/cmdline caching, use that.  Otherwise
	 * revert back to the old way which only implements full cmdline
	 * for the currept process and just p->p_comm for all other
	 * processes.
	 * Note that if the argv is no longer available, we deliberately
	 * don't fall back on p->p_comm or return an error: the authentic
	 * Linux behaviour is to return zero-length in this case.
	 */

	PROC_LOCK(p);
	if (p->p_args && (ps_argsopen || !p_cansee(td, p))) {
		sbuf_bcpy(sb, p->p_args->ar_args, p->p_args->ar_length);
		PROC_UNLOCK(p);
		return (0);
	}
	PROC_UNLOCK(p);
	if (p != td->td_proc) {
		sbuf_printf(sb, "%.*s", MAXCOMLEN, p->p_comm);
	} else {
		error = copyin((void *)p->p_sysent->sv_psstrings, &pstr,
		    sizeof(pstr));
		if (error)
			return (error);
		for (i = 0; i < pstr.ps_nargvstr; i++) {
			sbuf_copyin(sb, pstr.ps_argvstr[i], 0);
			sbuf_printf(sb, "%c", '\0');
		}
	}

	return (0);
}
