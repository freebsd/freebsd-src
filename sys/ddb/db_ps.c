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
 *	$Id: db_ps.c,v 1.4 1995/03/28 23:29:52 davidg Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <ddb/ddb.h>
#include <machine/cons.h>

void
db_ps() {
	int np;
	int nl = 0;
	volatile struct proc *ap, *p, *pp;
	np = nprocs;
	p = ap = allproc;

	db_printf("  pid   proc     addr    uid  ppid  pgrp  flag stat wmesg   wchan   cmd\n");
	while (--np >= 0) {
		/*
		 * XXX just take 20 for now...
		 */
		if (nl++ == 20) {
			db_printf("--More--");
			cngetc();
			db_printf("\r");
			nl = 0;
		}
		pp = p->p_pptr;
		if (pp == 0)
			pp = p;
		if (p->p_stat) {
			db_printf("%5d %06x %06x %4d %5d %5d %06x  %d",
			    p->p_pid, ap, p->p_addr, p->p_cred->p_ruid, pp->p_pid, 
			    p->p_pgrp->pg_id, p->p_flag, p->p_stat);
			if (p->p_wchan) {
				db_printf("  %6s %08x %s\n", p->p_wmesg, p->p_wchan, p->p_comm);
			} else {
				db_printf("                  %s\n", p->p_comm);
			}
		}
		ap = p->p_next;
		if (ap == 0 && np > 0)
			ap = zombproc;
		p = ap;
    	}
}
