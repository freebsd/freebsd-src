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
 *	$Id: procfs_status.c,v 1.14 1999/05/22 20:10:26 dt Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/jail.h>
#include <sys/vnode.h>
#include <sys/tty.h>
#include <sys/resourcevar.h>
#include <miscfs/procfs/procfs.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <sys/exec.h>

int
procfs_dostatus(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	struct session *sess;
	struct tty *tp;
	struct ucred *cr;
	char *ps;
	char *sep;
	int pid, ppid, pgid, sid;
	int i;
	int xlen;
	int error;
	char psbuf[256];		/* XXX - conservative */

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	pid = p->p_pid;
	ppid = p->p_pptr ? p->p_pptr->p_pid : 0,
	pgid = p->p_pgrp->pg_id;
	sess = p->p_pgrp->pg_session;
	sid = sess->s_leader ? sess->s_leader->p_pid : 0;

/* comm pid ppid pgid sid maj,min ctty,sldr start ut st wmsg 
                                euid ruid rgid,egid,groups[1 .. NGROUPS]
*/
	ps = psbuf;
	bcopy(p->p_comm, ps, MAXCOMLEN);
	ps[MAXCOMLEN] = '\0';
	ps += strlen(ps);
	ps += sprintf(ps, " %d %d %d %d ", pid, ppid, pgid, sid);

	if ((p->p_flag&P_CONTROLT) && (tp = sess->s_ttyp))
		ps += sprintf(ps, "%d,%d ", major(tp->t_dev), minor(tp->t_dev));
	else
		ps += sprintf(ps, "%d,%d ", -1, -1);

	sep = "";
	if (sess->s_ttyvp) {
		ps += sprintf(ps, "%sctty", sep);
		sep = ",";
	}
	if (SESS_LEADER(p)) {
		ps += sprintf(ps, "%ssldr", sep);
		sep = ",";
	}
	if (*sep != ',')
		ps += sprintf(ps, "noflags");

	if (p->p_flag & P_INMEM) {
		struct timeval ut, st;

		calcru(p, &ut, &st, (struct timeval *) NULL);
		ps += sprintf(ps, " %ld,%ld %ld,%ld %ld,%ld",
		    p->p_stats->p_start.tv_sec,
		    p->p_stats->p_start.tv_usec,
		    ut.tv_sec, ut.tv_usec,
		    st.tv_sec, st.tv_usec);
	} else
		ps += sprintf(ps, " -1,-1 -1,-1 -1,-1");

	ps += sprintf(ps, " %s",
		(p->p_wchan && p->p_wmesg) ? p->p_wmesg : "nochan");

	cr = p->p_ucred;

	ps += sprintf(ps, " %lu %lu %lu", 
		(u_long)cr->cr_uid,
		(u_long)p->p_cred->p_ruid,
		(u_long)p->p_cred->p_rgid);

	/* egid (p->p_cred->p_svgid) is equal to cr_ngroups[0] 
	   see also getegid(2) in /sys/kern/kern_prot.c */

	for (i = 0; i < cr->cr_ngroups; i++)
		ps += sprintf(ps, ",%lu", (u_long)cr->cr_groups[i]);

	if (p->p_prison)
		ps += sprintf(ps, " %s", p->p_prison->pr_host);
	else
		ps += sprintf(ps, " -");
	ps += sprintf(ps, "\n");

	xlen = ps - psbuf;
	xlen -= uio->uio_offset;
	ps = psbuf + uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	if (xlen <= 0)
		error = 0;
	else
		error = uiomove(ps, xlen, uio);

	return (error);
}

int
procfs_docmdline(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	char *ps;
	int xlen;
	int error;
	char psbuf[256];

	struct ps_strings pstr;
	int i;
	size_t bytes_left, done;

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	/*
	 * This is a hack: the correct behaviour is only implemented for
	 * the case of the current process enquiring about its own argv
	 * (due to the difficulty of accessing other processes' address space).
	 * For other cases, we cop out and just return argv[0] from p->p_comm.
	 * Note that if the argv is no longer available, we deliberately
	 * don't fall back on p->p_comm or return an error: the authentic
	 * Linux behaviour is to return zero-length in this case.
	 */

	if (curproc == p) {
		error = copyin((void*)PS_STRINGS, &pstr, sizeof(pstr));
		if (error)
			return (error);
		bytes_left = sizeof(psbuf);
		ps = psbuf;
		for (i = 0; bytes_left && (i < pstr.ps_nargvstr); i++) {
			error = copyinstr(pstr.ps_argvstr[i], ps,
					  bytes_left, &done);
			/* If too long or malformed, just truncate */
			if (error) {
				error = 0;
				break;
			}
			ps += done;
			bytes_left -= done;
		}
	}
	else {
		ps = psbuf;
		bcopy(p->p_comm, ps, MAXCOMLEN);
		ps[MAXCOMLEN] = '\0';
		ps += strlen(ps);
	}

	xlen = ps - psbuf;
	xlen -= uio->uio_offset;
	ps = psbuf + uio->uio_offset;
	xlen = min(xlen, uio->uio_resid);
	if (xlen <= 0)
		error = 0;
	else
		error = uiomove(ps, xlen, uio);
	return (error);
}
