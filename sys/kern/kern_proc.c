/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)kern_proc.c	8.4 (Berkeley) 1/4/94
 * $Id: kern_proc.c,v 1.10 1995/05/30 08:05:37 rgrimes Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/acct.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <ufs/ufs/quota.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/signalvar.h>

struct prochd qs[NQS];		/* as good a place as any... */
struct prochd rtqs[NQS];	/* Space for REALTIME queues too */
struct prochd idqs[NQS];	/* Space for IDLE queues too */

volatile struct proc *allproc;	/* all processes */
struct proc *zombproc;		/* just zombies */

void pgdelete	__P((struct pgrp *));

/*
 * Structure associated with user cacheing.
 */
struct uidinfo {
	struct	uidinfo *ui_next;
	struct	uidinfo **ui_prev;
	uid_t	ui_uid;
	long	ui_proccnt;
} **uihashtbl;
u_long	uihash;		/* size of hash table - 1 */
#define	UIHASH(uid)	((uid) & uihash)

/*
 * Allocate a hash table.
 */
void
usrinfoinit()
{

	uihashtbl = hashinit(maxproc / 16, M_PROC, &uihash);
}

/*
 * Change the count associated with number of processes
 * a given user is using.
 */
int
chgproccnt(uid, diff)
	uid_t	uid;
	int	diff;
{
	register struct uidinfo **uipp, *uip, *uiq;

	uipp = &uihashtbl[UIHASH(uid)];
	for (uip = *uipp; uip; uip = uip->ui_next)
		if (uip->ui_uid == uid)
			break;
	if (uip) {
		uip->ui_proccnt += diff;
		if (uip->ui_proccnt > 0)
			return (uip->ui_proccnt);
		if (uip->ui_proccnt < 0)
			panic("chgproccnt: procs < 0");
		if ((uiq = uip->ui_next))
			uiq->ui_prev = uip->ui_prev;
		*uip->ui_prev = uiq;
		FREE(uip, M_PROC);
		return (0);
	}
	if (diff <= 0) {
		if (diff == 0)
			return(0);
		panic("chgproccnt: lost user");
	}
	MALLOC(uip, struct uidinfo *, sizeof(*uip), M_PROC, M_WAITOK);
	if ((uiq = *uipp))
		uiq->ui_prev = &uip->ui_next;
	uip->ui_next = uiq;
	uip->ui_prev = uipp;
	*uipp = uip;
	uip->ui_uid = uid;
	uip->ui_proccnt = diff;
	return (diff);
}

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
pfind(pid)
	register pid_t pid;
{
	register struct proc *p;

	for (p = pidhash[PIDHASH(pid)]; p != NULL; p = p->p_hash)
		if (p->p_pid == pid)
			return (p);
	return (NULL);
}

/*
 * Locate a zombie process by number
 */
struct proc *
zpfind(pid)
	pid_t pid;
{
	struct proc *p;

	for (p = zombproc; p != NULL; p = p->p_next) 
		if (p->p_pid == pid)
			return (p);
	return (NULL);
}

/*
 * Locate a process group by number
 */
struct pgrp *
pgfind(pgid)
	register pid_t pgid;
{
	register struct pgrp *pgrp;

	for (pgrp = pgrphash[PIDHASH(pgid)];
	    pgrp != NULL; pgrp = pgrp->pg_hforw)
		if (pgrp->pg_id == pgid)
			return (pgrp);
	return (NULL);
}

/*
 * Move p to a new or existing process group (and session)
 */
int
enterpgrp(p, pgid, mksess)
	register struct proc *p;
	pid_t pgid;
	int mksess;
{
	register struct pgrp *pgrp = pgfind(pgid);
	register struct proc **pp;
	int n;

#ifdef DIAGNOSTIC
	if (pgrp != NULL && mksess)	/* firewalls */
		panic("enterpgrp: setsid into non-empty pgrp");
	if (SESS_LEADER(p))
		panic("enterpgrp: session leader attempted setpgrp");
#endif
	if (pgrp == NULL) {
		pid_t savepid = p->p_pid;
		struct proc *np;
		/*
		 * new process group
		 */
#ifdef DIAGNOSTIC
		if (p->p_pid != pgid)
			panic("enterpgrp: new pgrp and pid != pgid");
#endif
		MALLOC(pgrp, struct pgrp *, sizeof(struct pgrp), M_PGRP,
		       M_WAITOK);
		if ((np = pfind(savepid)) == NULL || np != p)
			return (ESRCH);
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
			p->p_flag &= ~P_CONTROLT;
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
		return (0);

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
	for (pp = &p->p_pgrp->pg_mem; *pp; pp = &(*pp)->p_pgrpnxt) {
		if (*pp == p) {
			*pp = p->p_pgrpnxt;
			break;
		}
	}
#ifdef DIAGNOSTIC
	if (pp == NULL)
		panic("enterpgrp: can't find p on old pgrp");
#endif
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
	return (0);
}

/*
 * remove process from process group
 */
int
leavepgrp(p)
	register struct proc *p;
{
	register struct proc **pp = &p->p_pgrp->pg_mem;

	for (; *pp; pp = &(*pp)->p_pgrpnxt) {
		if (*pp == p) {
			*pp = p->p_pgrpnxt;
			break;
		}
	}
#ifdef DIAGNOSTIC
	if (pp == NULL)
		panic("leavepgrp: can't find p in pgrp");
#endif
	if (!p->p_pgrp->pg_mem)
		pgdelete(p->p_pgrp);
	p->p_pgrp = 0;
	return (0);
}

/*
 * delete a process group
 */
void
pgdelete(pgrp)
	register struct pgrp *pgrp;
{
	register struct pgrp **pgp = &pgrphash[PIDHASH(pgrp->pg_id)];

	if (pgrp->pg_session->s_ttyp != NULL &&
	    pgrp->pg_session->s_ttyp->t_pgrp == pgrp)
		pgrp->pg_session->s_ttyp->t_pgrp = NULL;
	for (; *pgp; pgp = &(*pgp)->pg_hforw) {
		if (*pgp == pgrp) {
			*pgp = pgrp->pg_hforw;
			break;
		}
	}
#ifdef DIAGNOSTIC
	if (pgp == NULL)
		panic("pgdelete: can't find pgrp on hash chain");
#endif
	if (--pgrp->pg_session->s_count == 0)
		FREE(pgrp->pg_session, M_SESSION);
	FREE(pgrp, M_PGRP);
}

static void orphanpg();

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
