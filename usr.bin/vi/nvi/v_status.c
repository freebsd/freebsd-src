/*-
 * Copyright (c) 1992, 1993
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
 */

#ifndef lint
static char sccsid[] = "@(#)v_status.c	8.10 (Berkeley) 11/20/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <unistd.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_status -- ^G
 *	Show the file status.
 */
int
v_status(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{

	/*
	 * ^G in historic vi reset the cursor column to the first
	 * non-blank character in the line.  This doesn't seem of
	 * any usefulness whatsoever, so I don't bother.
	 */
	return (status(sp, ep, fm->lno, 1));
}

int
status(sp, ep, lno, showlast)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	int showlast;
{
	recno_t last;
	char *mo, *nc, *nf, *ro, *pid;
#ifdef DEBUG
	char pbuf[50];

	(void)snprintf(pbuf, sizeof(pbuf), " (pid %u)", getpid());
	pid = pbuf;
#else
	pid = "";
#endif
	/*
	 * See nvi/exf.c:file_init() for a description of how and
	 * when the read-only bit is set.  Possible displays are:
	 *
	 *	new file
	 *	new file, readonly
	 *	[un]modified
	 *	[un]modified, readonly
	 *	name changed, [un]modified
	 *	name changed, [un]modified, readonly
	 *
	 * !!!
	 * The historic display for "name changed" was "[Not edited]".
	 */
	if (F_ISSET(sp->frp, FR_NEWFILE)) {
		F_CLR(sp->frp, FR_NEWFILE);
		nf = "new file";
		mo = nc = "";
	} else {
		nf = "";
		if (sp->frp->cname != NULL) {
			nc = "name changed";
			mo = F_ISSET(ep, F_MODIFIED) ?
			    ", modified" : ", unmodified";
		} else {
			nc = "";
			mo = F_ISSET(ep, F_MODIFIED) ?
			    "modified" : "unmodified";
		}
	}
	ro = F_ISSET(sp->frp, FR_RDONLY) ? ", readonly" : "";
	if (showlast) {
		if (file_lline(sp, ep, &last))
			return (1);
		if (last >= 1)
			msgq(sp, M_INFO,
			    "%s: %s%s%s%s: line %lu of %lu [%ld%%]%s",
			    FILENAME(sp->frp), nf, nc, mo, ro, lno,
			    last, (lno * 100) / last, pid);
		else
			msgq(sp, M_INFO, "%s: %s%s%s%s: empty file%s",
			    FILENAME(sp->frp), nf, nc, mo, ro, pid);
	} else
		msgq(sp, M_INFO, "%s: %s%s%s%s: line %lu%s",
		    FILENAME(sp->frp), nf, nc, mo, ro, lno, pid);
	return (0);
}
