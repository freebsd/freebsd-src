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
 *	$Id: db_ps.c,v 1.15 1997/10/09 00:20:00 bde Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/cons.h>

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

	np = nprocs;

	if (allproc.lh_first != NULL)
		p = allproc.lh_first;
	else
		p = &proc0;

	db_printf("  pid   proc     addr    uid  ppid  pgrp  flag stat wmesg   wchan   cmd\n");
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
		pp = p->p_pptr;
		if (pp == NULL)
			pp = p;

		db_printf("%5d %8p %8p %4d %5d %5d %06x  %d",
		    p->p_pid, (void *)p, (void *)p->p_addr,
		    p->p_cred ? p->p_cred->p_ruid : 0, pp->p_pid,
		    p->p_pgrp ? p->p_pgrp->pg_id : 0, p->p_flag, p->p_stat);
		if (p->p_wchan) {
			db_printf("  %6s %8p", p->p_wmesg, (void *)p->p_wchan);
		} else {
			db_printf("                 ");
		}
		db_printf(" %s\n", p->p_comm ? p->p_comm : "");

		p = p->p_list.le_next;
		if (p == NULL && np > 0)
			p = zombproc.lh_first;
    	}
}
