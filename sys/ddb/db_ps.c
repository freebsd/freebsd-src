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
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/cons.h>

#include <ddb/ddb.h>

void
db_ps(dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{
	int np;
	int nl = 0;
	volatile struct proc *p, *pp;
	volatile struct thread *td;
	char *state;

	np = nprocs;

	/* sx_slock(&allproc_lock); */
	if (!LIST_EMPTY(&allproc))
		p = LIST_FIRST(&allproc);
	else
		p = &proc0;

	db_printf("  pid   proc     addr    uid  ppid  pgrp  flag   stat  wmesg    wchan  cmd\n");
	while (--np >= 0) {
		/*
		 * XXX just take 20 for now...
		 */
		if (nl++ == 20) {
			int c;

			db_printf("--More--");
			c = cngetc();
			db_printf("\r");
			/*
			 * A whole screenfull or just one line?
			 */
			switch (c) {
			case '\n':		/* just one line */
				nl = 20;
				break;
			case ' ':
				nl = 0;		/* another screenfull */
				break;
			default:		/* exit */
				db_printf("\n");
				return;
			}
		}
		if (p == NULL) {
			printf("oops, ran out of processes early!\n");
			break;
		}
		/* PROC_LOCK(p); */
		pp = p->p_pptr;
		if (pp == NULL)
			pp = p;


		switch(p->p_state) {
		case PRS_NORMAL:
			if (P_SHOULDSTOP(p))
				state = "stop";
			else
				state = "norm";
			break;
		case PRS_NEW:
			state = "new ";
			break;
		case PRS_WAIT:
			state = "wait";
			break;
		case PRS_ZOMBIE:
			state = "zomp";
			break;
		default:
			state = "Unkn";
			break;
		}
		db_printf("%5d %8p %8p %4d %5d %5d %07x %-4s",
		    p->p_pid, (volatile void *)p, (void *)p->p_uarea, 
		    p->p_ucred != NULL ? p->p_ucred->cr_ruid : 0, pp->p_pid,
		    p->p_pgrp != NULL ? p->p_pgrp->pg_id : 0, p->p_flag,
		    state);
		if (p->p_flag & P_KSES) {
			db_printf("(threaded)  %s\n", p->p_comm);
			FOREACH_THREAD_IN_PROC(p, td) {
				db_printf( ".  .  .  .  .  .  .  "
				           ".  thread %p   .  .  .  ", td);
				if (td->td_wchan != NULL) {
					db_printf("SLP %6s %8p\n", td->td_wmesg,
					    (void *)td->td_wchan);
				} else if (td->td_state == TDS_MTX) {
					db_printf("MTX %6s %8p\n", td->td_mtxname,
					    (void *)td->td_blocked);
				} else {
					db_printf("--not blocked--\n");
				}
			}
		} else {
			td = FIRST_THREAD_IN_PROC(p);
			if (td != NULL && td->td_wchan != NULL) {
				db_printf("  %-6s %8p", td->td_wmesg,
				    (void *)td->td_wchan);
			} else if (td != NULL && td->td_state == TDS_MTX) {
				db_printf("  %6s %8p", td->td_mtxname,
				    (void *)td->td_blocked);
			} else {
				db_printf("                 ");
			}
			db_printf(" %s\n", p->p_comm);
		}
		/* PROC_UNLOCK(p); */

		p = LIST_NEXT(p, p_list);
		if (p == NULL && np > 0)
			p = LIST_FIRST(&zombproc);
    	}
	/* sx_sunlock(&allproc_lock); */
}
