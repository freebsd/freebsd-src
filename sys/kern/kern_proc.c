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
 *	@(#)kern_proc.c	8.7 (Berkeley) 2/14/95
 * $Id: kern_proc.c,v 1.28 1997/08/02 14:31:33 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <vm/vm.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>

MALLOC_DEFINE(M_PGRP, "pgrp", "process group header");

struct prochd qs[NQS];		/* as good a place as any... */
struct prochd rtqs[NQS];	/* Space for REALTIME queues too */
struct prochd idqs[NQS];	/* Space for IDLE queues too */

static void pgdelete	__P((struct pgrp *));

/*
 * Structure associated with user cacheing.
 */
struct uidinfo {
	LIST_ENTRY(uidinfo) ui_hash;
	uid_t	ui_uid;
	long	ui_proccnt;
};
#define	UIHASH(uid)	(&uihashtbl[(uid) & uihash])
LIST_HEAD(uihashhead, uidinfo) *uihashtbl;
static u_long uihash;		/* size of hash table - 1 */

static void	orphanpg __P((struct pgrp *pg));

/*
 * Other process lists
 */
struct pidhashhead *pidhashtbl;
u_long pidhash;
struct pgrphashhead *pgrphashtbl;
u_long pgrphash;
struct proclist allproc;
struct proclist zombproc;

/*
 * Initialize global process hashing structures.
 */
void
procinit()
{

	LIST_INIT(&allproc);
	LIST_INIT(&zombproc);
	pidhashtbl = hashinit(maxproc / 4, M_PROC, &pidhash);
	pgrphashtbl = hashinit(maxproc / 4, M_PROC, &pgrphash);
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
	register struct uidinfo *uip;
	register struct uihashhead *uipp;

	uipp = UIHASH(uid);
	for (uip = uipp->lh_first; uip != 0; uip = uip->ui_hash.le_next)
		if (uip->ui_uid == uid)
			break;
	if (uip) {
		uip->ui_proccnt += diff;
		if (uip->ui_proccnt > 0)
			return (uip->ui_proccnt);
		if (uip->ui_proccnt < 0)
			panic("chgproccnt: procs < 0");
		LIST_REMOVE(uip, ui_hash);
		FREE(uip, M_PROC);
		return (0);
	}
	if (diff <= 0) {
		if (diff == 0)
			return(0);
		panic("chgproccnt: lost user");
	}
	MALLOC(uip, struct uidinfo *, sizeof(*uip), M_PROC, M_WAITOK);
	LIST_INSERT_HEAD(uipp, uip, ui_hash);
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

	for (p = PIDHASH(pid)->lh_first; p != 0; p = p->p_hash.le_next)
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

	for (pgrp = PGRPHASH(pgid)->lh_first; pgrp != 0;
	     pgrp = pgrp->pg_hash.le_next)
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
		LIST_INIT(&pgrp->pg_members);
		LIST_INSERT_HEAD(PGRPHASH(pgid), pgrp, pg_hash);
		pgrp->pg_jobc = 0;
	} else if (pgrp == p->p_pgrp)
		return (0);

	/*
	 * Adjust eligibility of affected pgrps to participate in job control.
	 * Increment eligibility counts before decrementing, otherwise we
	 * could reach 0 spuriously during the first call.
	 */
	fixjobc(p, pgrp, 1);
	fixjobc(p, p->p_pgrp, 0);

	LIST_REMOVE(p, p_pglist);
	if (p->p_pgrp->pg_members.lh_first == 0)
		pgdelete(p->p_pgrp);
	p->p_pgrp = pgrp;
	LIST_INSERT_HEAD(&pgrp->pg_members, p, p_pglist);
	return (0);
}

/*
 * remove process from process group
 */
int
leavepgrp(p)
	register struct proc *p;
{

	LIST_REMOVE(p, p_pglist);
	if (p->p_pgrp->pg_members.lh_first == 0)
		pgdelete(p->p_pgrp);
	p->p_pgrp = 0;
	return (0);
}

/*
 * delete a process group
 */
static void
pgdelete(pgrp)
	register struct pgrp *pgrp;
{

	if (pgrp->pg_session->s_ttyp != NULL &&
	    pgrp->pg_session->s_ttyp->t_pgrp == pgrp)
		pgrp->pg_session->s_ttyp->t_pgrp = NULL;
	LIST_REMOVE(pgrp, pg_hash);
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
	for (p = p->p_children.lh_first; p != 0; p = p->p_sibling.le_next)
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

	for (p = pg->pg_members.lh_first; p != 0; p = p->p_pglist.le_next) {
		if (p->p_stat == SSTOP) {
			for (p = pg->pg_members.lh_first; p != 0;
			    p = p->p_pglist.le_next) {
				psignal(p, SIGHUP);
				psignal(p, SIGCONT);
			}
			return;
		}
	}
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(pgrpdump, pgrpdump)
{
	register struct pgrp *pgrp;
	register struct proc *p;
	register i;

	for (i = 0; i <= pgrphash; i++) {
		if (pgrp = pgrphashtbl[i].lh_first) {
			printf("\tindx %d\n", i);
			for (; pgrp != 0; pgrp = pgrp->pg_hash.le_next) {
				printf("\tpgrp %x, pgid %d, sess %x, sesscnt %d, mem %x\n",
				    pgrp, pgrp->pg_id, pgrp->pg_session,
				    pgrp->pg_session->s_count,
				    pgrp->pg_members.lh_first);
				for (p = pgrp->pg_members.lh_first; p != 0;
				    p = p->p_pglist.le_next) {
					printf("\t\tpid %d addr %x pgrp %x\n", 
					    p->p_pid, p, p->p_pgrp);
				}
			}
		}
	}
}
#endif /* DDB */

/*
 * Fill in an eproc structure for the specified process.
 */
void
fill_eproc(p, ep)
	register struct proc *p;
	register struct eproc *ep;
{
	register struct tty *tp;

	bzero(ep, sizeof(*ep));

	ep->e_paddr = p;
	if (p->p_cred) {
		ep->e_pcred = *p->p_cred;
		if (p->p_ucred)
			ep->e_ucred = *p->p_ucred;
	}
	if (p->p_stat != SIDL && p->p_stat != SZOMB && p->p_vmspace != NULL) {
		register struct vmspace *vm = p->p_vmspace;

#ifdef pmap_resident_count
		ep->e_vm.vm_rssize = pmap_resident_count(&vm->vm_pmap); /*XXX*/
#else
		ep->e_vm.vm_rssize = vm->vm_rssize;
#endif
		ep->e_vm.vm_tsize = vm->vm_tsize;
		ep->e_vm.vm_dsize = vm->vm_dsize;
		ep->e_vm.vm_ssize = vm->vm_ssize;
		ep->e_vm.vm_taddr = vm->vm_taddr;
		ep->e_vm.vm_daddr = vm->vm_daddr;
		ep->e_vm.vm_minsaddr = vm->vm_minsaddr;
		ep->e_vm.vm_maxsaddr = vm->vm_maxsaddr;
#ifndef sparc
		ep->e_vm.vm_pmap = vm->vm_pmap;
#endif
	}
	if (p->p_pptr)
		ep->e_ppid = p->p_pptr->p_pid;
	if (p->p_pgrp) {
		ep->e_pgid = p->p_pgrp->pg_id;
		ep->e_jobc = p->p_pgrp->pg_jobc;
		ep->e_sess = p->p_pgrp->pg_session;

		if (ep->e_sess) {
			bcopy(ep->e_sess->s_login, ep->e_login, sizeof(ep->e_login));
			if (ep->e_sess->s_ttyvp)
				ep->e_flag = EPROC_CTTY;
			if (p->p_session && SESS_LEADER(p))
				ep->e_flag |= EPROC_SLEADER;
		}
	}
	if ((p->p_flag & P_CONTROLT) &&
	    (ep->e_sess != NULL) &&
	    ((tp = ep->e_sess->s_ttyp) != NULL)) {
		ep->e_tdev = tp->t_dev;
		ep->e_tpgid = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		ep->e_tsess = tp->t_session;
	} else
		ep->e_tdev = NODEV;
	if (p->p_wmesg) {
		strncpy(ep->e_wmesg, p->p_wmesg, WMESGLEN);
		ep->e_wmesg[WMESGLEN] = 0;
	}
}

static struct proc *
zpfind(pid_t pid)
{
	struct proc *p;

	for (p = zombproc.lh_first; p != 0; p = p->p_list.le_next)
		if (p->p_pid == pid)
			return (p);
	return (NULL);
}


static int
sysctl_out_proc(struct proc *p, struct sysctl_req *req, int doingzomb)
{
	struct eproc eproc;
	int error;
	pid_t pid = p->p_pid;

	fill_eproc(p, &eproc);
	error = SYSCTL_OUT(req,(caddr_t)p, sizeof(struct proc));
	if (error)
		return (error);
	error = SYSCTL_OUT(req,(caddr_t)&eproc, sizeof(eproc));
	if (error)
		return (error);
	if (!doingzomb && pid && (pfind(pid) != p))
		return EAGAIN;
	if (doingzomb && zpfind(pid) != p)
		return EAGAIN;
	return (0);
}

static int
sysctl_kern_proc SYSCTL_HANDLER_ARGS
{
	int *name = (int*) arg1;
	u_int namelen = arg2;
	struct proc *p;
	int doingzomb;
	int error = 0;

	if (oidp->oid_number == KERN_PROC_PID) {
		if (namelen != 1) 
			return (EINVAL);
		p = pfind((pid_t)name[0]);
		if (!p)
			return (0);
		error = sysctl_out_proc(p, req, 0);
		return (error);
	}
	if (oidp->oid_number == KERN_PROC_ALL && !namelen)
		;
	else if (oidp->oid_number != KERN_PROC_ALL && namelen == 1)
		;
	else
		return (EINVAL);
	
	if (!req->oldptr) {
		/* overestimate by 5 procs */
		error = SYSCTL_OUT(req, 0, sizeof (struct kinfo_proc) * 5);
		if (error)
			return (error);
	}
	for (doingzomb=0 ; doingzomb < 2 ; doingzomb++) {
		if (!doingzomb)
			p = allproc.lh_first;
		else
			p = zombproc.lh_first;
		for (; p != 0; p = p->p_list.le_next) {
			/*
			 * Skip embryonic processes.
			 */
			if (p->p_stat == SIDL)
				continue;
			/*
			 * TODO - make more efficient (see notes below).
			 * do by session.
			 */
			switch (oidp->oid_number) {

			case KERN_PROC_PGRP:
				/* could do this by traversing pgrp */
				if (p->p_pgrp == NULL || 
				    p->p_pgrp->pg_id != (pid_t)name[0])
					continue;
				break;

			case KERN_PROC_TTY:
				if ((p->p_flag & P_CONTROLT) == 0 ||
				    p->p_session == NULL ||
				    p->p_session->s_ttyp == NULL ||
				    p->p_session->s_ttyp->t_dev != (dev_t)name[0])
					continue;
				break;

			case KERN_PROC_UID:
				if (p->p_ucred == NULL || 
				    p->p_ucred->cr_uid != (uid_t)name[0])
					continue;
				break;

			case KERN_PROC_RUID:
				if (p->p_ucred == NULL || 
				    p->p_cred->p_ruid != (uid_t)name[0])
					continue;
				break;
			}

			error = sysctl_out_proc(p, req, doingzomb);
			if (error)
				return (error);
		}
	}
	return (0);
}


SYSCTL_NODE(_kern, KERN_PROC, proc, CTLFLAG_RD,  0, "Process table");

SYSCTL_PROC(_kern_proc, KERN_PROC_ALL, all, CTLFLAG_RD|CTLTYPE_STRUCT,
	0, 0, sysctl_kern_proc, "S,proc", "");

SYSCTL_NODE(_kern_proc, KERN_PROC_PGRP, pgrp, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_TTY, tty, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_UID, uid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_RUID, ruid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_PID, pid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");
