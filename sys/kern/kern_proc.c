/*
 * Copyright (c) 1982, 1986, 1989, 1991 Regents of the University of California.
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
 *	from: @(#)kern_proc.c	7.16 (Berkeley) 6/28/91
 *	$Id: kern_proc.c,v 1.4 1993/12/19 00:51:28 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "kernel.h"
#include "proc.h"
#include "buf.h"
#include "acct.h"
#include "wait.h"
#include "file.h"
#include "../ufs/quota.h"
#include "uio.h"
#include "malloc.h"
#include "mbuf.h"
#include "ioctl.h"
#include "tty.h"

struct prochd qs[NQS];		/* as good a place as any... */
struct proc *zombproc;
struct proc *allproc;

static void pgdelete(struct pgrp *);
static void orphanpg(struct pgrp *);

/*
 * Is p an inferior of the current process?
 */
int
inferior(p)
	register struct proc *p;
{

	for (; p != curproc; p = p->p_pptr)
		if (p->p_pid == 0)
			return (0);
	return (1);
}

/*
 * Locate a process by number
 */
struct proc *
pfind(int pid)
{
	register struct proc *p = pidhash[PIDHASH(pid)];

	for (; p; p = p->p_hash)
		if (p->p_pid == pid)
			return (p);
	return ((struct proc *)0);
}

/*
 * Locate a process group by number
 */
struct pgrp *
pgfind(pid_t pgid)
{
	register struct pgrp *pgrp = pgrphash[PIDHASH(pgid)];

	for (; pgrp; pgrp = pgrp->pg_hforw)
		if (pgrp->pg_id == pgid)
			return (pgrp);
	return ((struct pgrp *)0);
}

/*
 * Move p to a new or existing process group (and session)
 */
void
enterpgrp(p, pgid, mksess)
	register struct proc *p;
	pid_t pgid;
	int mksess;
{
	register struct pgrp *pgrp = pgfind(pgid);
	register struct proc **pp;
	register struct proc *cp;
	int n;

#ifdef DIAGNOSTIC
	if (pgrp && mksess)	/* firewalls */
		panic("enterpgrp: setsid into non-empty pgrp");
	if (SESS_LEADER(p))
		panic("enterpgrp: session leader attempted setpgrp");
#endif
	if (pgrp == NULL) {
		/*
		 * new process group
		 */
#ifdef DIAGNOSTIC
		if (p->p_pid != pgid)
			panic("enterpgrp: new pgrp and pid != pgid");
#endif
		MALLOC(pgrp, struct pgrp *, sizeof(struct pgrp), M_PGRP,
		       M_WAITOK);
		if (mksess) {
			register struct session *sess;

			/*
			 * new session
			 */
			MALLOC(sess, struct session *, sizeof(struct session),
				M_SESSION, M_WAITOK);
			sess->s_leader = p;
			sess->s_count = 1;
			sess->s_ttyvp = NULL;
			sess->s_ttyp = NULL;
			bcopy(p->p_session->s_login, sess->s_login,
			    sizeof(sess->s_login));
			p->p_flag &= ~SCTTY;
			pgrp->pg_session = sess;
#ifdef DIAGNOSTIC
			if (p != curproc)
				panic("enterpgrp: mksession and p != curproc");
#endif
		} else {
			pgrp->pg_session = p->p_session;
			pgrp->pg_session->s_count++;
		}
		pgrp->pg_id = pgid;
		pgrp->pg_hforw = pgrphash[n = PIDHASH(pgid)];
		pgrphash[n] = pgrp;
		pgrp->pg_jobc = 0;
		pgrp->pg_mem = NULL;
	} else if (pgrp == p->p_pgrp)
		return;

	/*
	 * Adjust eligibility of affected pgrps to participate in job control.
	 * Increment eligibility counts before decrementing, otherwise we
	 * could reach 0 spuriously during the first call.
	 */
	fixjobc(p, pgrp, 1);
	fixjobc(p, p->p_pgrp, 0);

	/*
	 * unlink p from old process group
	 */
	for (pp = &p->p_pgrp->pg_mem; *pp; pp = &(*pp)->p_pgrpnxt)
		if (*pp == p) {
			*pp = p->p_pgrpnxt;
			goto done;
		}
	panic("enterpgrp: can't find p on old pgrp");
done:
	/*
	 * delete old if empty
	 */
	if (p->p_pgrp->pg_mem == 0)
		pgdelete(p->p_pgrp);
	/*
	 * link into new one
	 */
	p->p_pgrp = pgrp;
	p->p_pgrpnxt = pgrp->pg_mem;
	pgrp->pg_mem = p;
}

/*
 * remove process from process group
 */
void
leavepgrp(p)
	register struct proc *p;
{
	register struct proc **pp = &p->p_pgrp->pg_mem;

	for (; *pp; pp = &(*pp)->p_pgrpnxt)
		if (*pp == p) {
			*pp = p->p_pgrpnxt;
			goto done;
		}
	panic("leavepgrp: can't find p in pgrp");
done:
	if (!p->p_pgrp->pg_mem)
		pgdelete(p->p_pgrp);
	p->p_pgrp = 0;
}

/*
 * delete a process group
 */
static void
pgdelete(pgrp)
	register struct pgrp *pgrp;
{
	register struct pgrp **pgp = &pgrphash[PIDHASH(pgrp->pg_id)];

	if (pgrp->pg_session->s_ttyp != NULL && 
	    pgrp->pg_session->s_ttyp->t_pgrp == pgrp)
		pgrp->pg_session->s_ttyp->t_pgrp = NULL;
	for (; *pgp; pgp = &(*pgp)->pg_hforw)
		if (*pgp == pgrp) {
			*pgp = pgrp->pg_hforw;
			goto done;
		}
	panic("pgdelete: can't find pgrp on hash chain");
done:
	if (--pgrp->pg_session->s_count == 0)
		FREE(pgrp->pg_session, M_SESSION);
	FREE(pgrp, M_PGRP);
}

/*
 * Adjust pgrp jobc counters when specified process changes process group.
 * We count the number of processes in each process group that "qualify"
 * the group for terminal job control (those with a parent in a different
 * process group of the same session).  If that count reaches zero, the
 * process group becomes orphaned.  Check both the specified process'
 * process group and that of its children.
 * entering == 0 => p is leaving specified group.
 * entering == 1 => p is entering specified group.
 */
void
fixjobc(p, pgrp, entering)
	register struct proc *p;
	register struct pgrp *pgrp;
	int entering;
{
	register struct pgrp *hispgrp;
	register struct session *mysession = pgrp->pg_session;

	/*
	 * Check p's parent to see whether p qualifies its own process
	 * group; if so, adjust count for p's process group.
	 */
	if ((hispgrp = p->p_pptr->p_pgrp) != pgrp &&
	    hispgrp->pg_session == mysession)
		if (entering)
			pgrp->pg_jobc++;
		else if (--pgrp->pg_jobc == 0)
			orphanpg(pgrp);

	/*
	 * Check this process' children to see whether they qualify
	 * their process groups; if so, adjust counts for children's
	 * process groups.
	 */
	for (p = p->p_cptr; p; p = p->p_osptr)
		if ((hispgrp = p->p_pgrp) != pgrp &&
		    hispgrp->pg_session == mysession &&
		    p->p_stat != SZOMB)
			if (entering)
				hispgrp->pg_jobc++;
			else if (--hispgrp->pg_jobc == 0)
				orphanpg(hispgrp);
}

/* 
 * A process group has become orphaned;
 * if there are any stopped processes in the group,
 * hang-up all process in that group.
 */
static void
orphanpg(pg)
	struct pgrp *pg;
{
	register struct proc *p;

	for (p = pg->pg_mem; p; p = p->p_pgrpnxt) {
		if (p->p_stat == SSTOP) {
			for (p = pg->pg_mem; p; p = p->p_pgrpnxt) {
				psignal(p, SIGHUP);
				psignal(p, SIGCONT);
			}
			return;
		}
	}
}

#ifdef debug
/* DEBUG */
pgrpdump()
{
	register struct pgrp *pgrp;
	register struct proc *p;
	register i;

	for (i=0; i<PIDHSZ; i++) {
		if (pgrphash[i]) {
		  printf("\tindx %d\n", i);
		  for (pgrp=pgrphash[i]; pgrp; pgrp=pgrp->pg_hforw) {
		    printf("\tpgrp %x, pgid %d, sess %x, sesscnt %d, mem %x\n",
			pgrp, pgrp->pg_id, pgrp->pg_session,
			pgrp->pg_session->s_count, pgrp->pg_mem);
		    for (p=pgrp->pg_mem; p; p=p->p_pgrpnxt) {
			printf("\t\tpid %d addr %x pgrp %x\n", 
				p->p_pid, p, p->p_pgrp);
		    }
		  }

		}
	}
}
#endif /* debug */
