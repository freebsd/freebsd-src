/*-
 * Copyright (c) 1993 The Regents of the University of California.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/ddb/db_ps.c,v 1.66 2007/09/17 05:27:19 jeff Exp $");

#include <sys/param.h>
#include <sys/cons.h>
#include <sys/jail.h>
#include <sys/kdb.h>
#include <sys/linker_set.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <ddb/ddb.h>

static void	dumpthread(volatile struct proc *p, volatile struct thread *td,
		    int all);

/*
 * Layout:
 * - column counts
 * - header
 * - single-threaded process
 * - multi-threaded process
 * - thread in a MT process
 *
 *          1         2         3         4         5         6         7
 * 1234567890123456789012345678901234567890123456789012345678901234567890
 *   pid  ppid  pgrp   uid   state   wmesg     wchan    cmd
 * <pid> <ppi> <pgi> <uid>  <stat> < wmesg > < wchan  > <name>
 * <pid> <ppi> <pgi> <uid>  <stat>  (threaded)          <command>
 * <tid >                   <stat> < wmesg > < wchan  > <name>
 *
 * For machines with 64-bit pointers, we expand the wchan field 8 more
 * characters.
 */
void
db_ps(db_expr_t addr, boolean_t hasaddr, db_expr_t count, char *modif)
{
	volatile struct proc *p, *pp;
	volatile struct thread *td;
	struct ucred *cred;
	struct pgrp *pgrp;
	char state[9];
	int np, rflag, sflag, dflag, lflag, wflag;

	np = nprocs;

	if (!LIST_EMPTY(&allproc))
		p = LIST_FIRST(&allproc);
	else
		p = &proc0;

#ifdef __LP64__
	db_printf("  pid  ppid  pgrp   uid   state   wmesg         wchan        cmd\n");
#else
	db_printf("  pid  ppid  pgrp   uid   state   wmesg     wchan    cmd\n");
#endif
	while (--np >= 0 && !db_pager_quit) {
		if (p == NULL) {
			db_printf("oops, ran out of processes early!\n");
			break;
		}
		pp = p->p_pptr;
		if (pp == NULL)
			pp = p;

		cred = p->p_ucred;
		pgrp = p->p_pgrp;
		db_printf("%5d %5d %5d %5d ", p->p_pid, pp->p_pid,
		    pgrp != NULL ? pgrp->pg_id : 0,
		    cred != NULL ? cred->cr_ruid : 0);

		/* Determine our primary process state. */
		switch (p->p_state) {
		case PRS_NORMAL:
			if (P_SHOULDSTOP(p))
				state[0] = 'T';
			else {
				/*
				 * One of D, L, R, S, W.  For a
				 * multithreaded process we will use
				 * the state of the thread with the
				 * highest precedence.  The
				 * precendence order from high to low
				 * is R, L, D, S, W.  If no thread is
				 * in a sane state we use '?' for our
				 * primary state.
				 */
				rflag = sflag = dflag = lflag = wflag = 0;
				FOREACH_THREAD_IN_PROC(p, td) {
					if (td->td_state == TDS_RUNNING ||
					    td->td_state == TDS_RUNQ ||
					    td->td_state == TDS_CAN_RUN)
						rflag++;
					if (TD_ON_LOCK(td))
						lflag++;
					if (TD_IS_SLEEPING(td)) {
						if (!td->td_flags & TDF_SINTR)
							dflag++;
						else
							sflag++;
					}
					if (TD_AWAITING_INTR(td))
						wflag++;
				}
				if (rflag)
					state[0] = 'R';
				else if (lflag)
					state[0] = 'L';
				else if (dflag)
					state[0] = 'D';
				else if (sflag)
					state[0] = 'S';
				else if (wflag)
					state[0] = 'W';
				else
					state[0] = '?';				
			}
			break;
		case PRS_NEW:
			state[0] = 'N';
			break;
		case PRS_ZOMBIE:
			state[0] = 'Z';
			break;
		default:
			state[0] = 'U';
			break;
		}
		state[1] = '\0';

		/* Additional process state flags. */
		if (!p->p_flag & P_INMEM)
			strlcat(state, "W", sizeof(state));
		if (p->p_flag & P_TRACED)
			strlcat(state, "X", sizeof(state));
		if (p->p_flag & P_WEXIT && p->p_state != PRS_ZOMBIE)
			strlcat(state, "E", sizeof(state));
		if (p->p_flag & P_PPWAIT)
			strlcat(state, "V", sizeof(state));
		if (p->p_flag & P_SYSTEM || p->p_lock > 0)
			strlcat(state, "L", sizeof(state));
		if (p->p_session != NULL && SESS_LEADER(p))
			strlcat(state, "s", sizeof(state));
		/* Cheated here and didn't compare pgid's. */
		if (p->p_flag & P_CONTROLT)
			strlcat(state, "+", sizeof(state));
		if (cred != NULL && jailed(cred))
			strlcat(state, "J", sizeof(state));
		db_printf(" %-6.6s ", state);
		if (p->p_flag & P_HADTHREADS)
#ifdef __LP64__
			db_printf(" (threaded)                  %s\n",
			    p->p_comm);
#else
			db_printf(" (threaded)          %s\n", p->p_comm);
#endif
		FOREACH_THREAD_IN_PROC(p, td) {
			dumpthread(p, td, p->p_flag & P_HADTHREADS);
			if (db_pager_quit)
				break;
		}

		p = LIST_NEXT(p, p_list);
		if (p == NULL && np > 0)
			p = LIST_FIRST(&zombproc);
    	}
}

static void
dumpthread(volatile struct proc *p, volatile struct thread *td, int all)
{
	char state[9], wprefix;
	const char *wmesg;
	void *wchan;
	
	if (all) {
		db_printf("%6d                  ", td->td_tid);
		switch (td->td_state) {
		case TDS_RUNNING:
			snprintf(state, sizeof(state), "Run");
			break;
		case TDS_RUNQ:
			snprintf(state, sizeof(state), "RunQ");
			break;
		case TDS_CAN_RUN:
			snprintf(state, sizeof(state), "CanRun");
			break;
		case TDS_INACTIVE:
			snprintf(state, sizeof(state), "Inactv");
			break;
		case TDS_INHIBITED:
			state[0] = '\0';
			if (TD_ON_LOCK(td))
				strlcat(state, "L", sizeof(state));
			if (TD_IS_SLEEPING(td)) {
				if (td->td_flags & TDF_SINTR)
					strlcat(state, "S", sizeof(state));
				else
					strlcat(state, "D", sizeof(state));
			}
			if (TD_IS_SWAPPED(td))
				strlcat(state, "W", sizeof(state));
			if (TD_AWAITING_INTR(td))
				strlcat(state, "I", sizeof(state));
			if (TD_IS_SUSPENDED(td))
				strlcat(state, "s", sizeof(state));
			if (state[0] != '\0')
				break;
		default:
			snprintf(state, sizeof(state), "???");
		}			
		db_printf(" %-6.6s ", state);
	}
	wprefix = ' ';
	if (TD_ON_LOCK(td)) {
		wprefix = '*';
		wmesg = td->td_lockname;
		wchan = td->td_blocked;
	} else if (TD_ON_SLEEPQ(td)) {
		wmesg = td->td_wmesg;
		wchan = td->td_wchan;
	} else if (TD_IS_RUNNING(td)) {
		snprintf(state, sizeof(state), "CPU %d", td->td_oncpu);
		wmesg = state;
		wchan = NULL;
	} else {
		wmesg = "";
		wchan = NULL;
	}
	db_printf("%c%-8.8s ", wprefix, wmesg);
	if (wchan == NULL)
#ifdef __LP64__
		db_printf("%18s ", "");
#else
		db_printf("%10s ", "");
#endif
	else
		db_printf("%p ", wchan);
	if (p->p_flag & P_SYSTEM)
		db_printf("[");
	if (td->td_name[0] != '\0')
		db_printf("%s", td->td_name);
	else
		db_printf("%s", td->td_proc->p_comm);
	if (p->p_flag & P_SYSTEM)
		db_printf("]");
	db_printf("\n");
}

DB_SHOW_COMMAND(thread, db_show_thread)
{
	struct thread *td;
	boolean_t comma;

	/* Determine which thread to examine. */
	if (have_addr)
		td = db_lookup_thread(addr, FALSE);
	else
		td = kdb_thread;

	db_printf("Thread %d at %p:\n", td->td_tid, td);
	db_printf(" proc (pid %d): %p\n", td->td_proc->p_pid, td->td_proc);
	if (td->td_name[0] != '\0')
		db_printf(" name: %s\n", td->td_name);
	db_printf(" flags: %#x ", td->td_flags);
	db_printf(" pflags: %#x\n", td->td_pflags);
	db_printf(" state: ");
	switch (td->td_state) {
	case TDS_INACTIVE:
		db_printf("INACTIVE\n");
		break;
	case TDS_CAN_RUN:
		db_printf("CAN RUN\n");
		break;
	case TDS_RUNQ:
		db_printf("RUNQ\n");
		break;
	case TDS_RUNNING:
		db_printf("RUNNING (CPU %d)\n", td->td_oncpu);
		break;
	case TDS_INHIBITED:
		db_printf("INHIBITED: {");
		comma = FALSE;
		if (TD_IS_SLEEPING(td)) {
			db_printf("SLEEPING");
			comma = TRUE;
		}
		if (TD_IS_SUSPENDED(td)) {
			if (comma)
				db_printf(", ");
			db_printf("SUSPENDED");
			comma = TRUE;
		}
		if (TD_IS_SWAPPED(td)) {
			if (comma)
				db_printf(", ");
			db_printf("SWAPPED");
			comma = TRUE;
		}
		if (TD_ON_LOCK(td)) {
			if (comma)
				db_printf(", ");
			db_printf("LOCK");
			comma = TRUE;
		}
		if (TD_AWAITING_INTR(td)) {
			if (comma)
				db_printf(", ");
			db_printf("IWAIT");
		}
		db_printf("}\n");
		break;
	default:
		db_printf("??? (%#x)\n", td->td_state);
		break;
	}
	if (TD_ON_LOCK(td))
		db_printf(" lock: %s  turnstile: %p\n", td->td_lockname,
		    td->td_blocked);
	if (TD_ON_SLEEPQ(td))
		db_printf(" wmesg: %s  wchan: %p\n", td->td_wmesg,
		    td->td_wchan);
	db_printf(" priority: %d\n", td->td_priority);
}

DB_SHOW_COMMAND(proc, db_show_proc)
{
	struct thread *td;
	struct proc *p;
	int i;

	/* Determine which process to examine. */
	if (have_addr)
		p = db_lookup_proc(addr);
	else
		p = kdb_thread->td_proc;

	db_printf("Process %d (%s) at %p:\n", p->p_pid, p->p_comm, p);
	db_printf(" state: ");
	switch (p->p_state) {
	case PRS_NEW:
		db_printf("NEW\n");
		break;
	case PRS_NORMAL:
		db_printf("NORMAL\n");
		break;
	case PRS_ZOMBIE:
		db_printf("ZOMBIE\n");
		break;
	default:
		db_printf("??? (%#x)\n", p->p_state);
	}
	if (p->p_ucred != NULL) {
		db_printf(" uid: %d  gids: ", p->p_ucred->cr_uid);
		for (i = 0; i < p->p_ucred->cr_ngroups; i++) {
			db_printf("%d", p->p_ucred->cr_groups[i]);
			if (i < (p->p_ucred->cr_ngroups - 1))
				db_printf(", ");
		}
		db_printf("\n");
	}
	if (p->p_pptr != NULL)
		db_printf(" parent: pid %d at %p\n", p->p_pptr->p_pid,
		    p->p_pptr);
	if (p->p_leader != NULL && p->p_leader != p)
		db_printf(" leader: pid %d at %p\n", p->p_leader->p_pid,
		    p->p_leader);
	if (p->p_sysent != NULL)
		db_printf(" ABI: %s\n", p->p_sysent->sv_name);
	if (p->p_args != NULL)
		db_printf(" arguments: %.*s\n", (int)p->p_args->ar_length,
		    p->p_args->ar_args);
	db_printf(" threads: %d\n", p->p_numthreads);
	FOREACH_THREAD_IN_PROC(p, td) {
		dumpthread(p, td, 1);
		if (db_pager_quit)
			break;
	}
}
