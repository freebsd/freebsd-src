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
static char sccsid[] = "@(#)v_filter.c	8.10 (Berkeley) 12/2/93";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "excmd.h"

/*
 * v_filter -- [count]!motion command(s)
 *	Run range through shell commands, replacing text.
 */
int
v_filter(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	EXCMDARG cmd;
	TEXT *tp;

	/*
	 * !!!
	 * Historical vi permitted "!!" in an empty file.  This is
	 * handled as a special case in the ex_bang routine.  Don't
	 * modify this setup without understanding that one.  In
	 * particular, note that we're manipulating the ex argument
	 * structures behind ex's back.
	 */
	SETCMDARG(cmd, C_BANG, 2, fm->lno, tm->lno, 0, NULL);
	EXP(sp)->argsoff = 0;			/* XXX */
	if (F_ISSET(vp,  VC_ISDOT)) {
		if (argv_exp1(sp, ep, &cmd, "!", 1, 1))
			return (1);
	} else {
		/* Get the command from the user. */
		if (sp->s_get(sp, ep, &sp->tiq,
		    '!', TXT_BS | TXT_CR | TXT_ESCAPE | TXT_PROMPT) != INP_OK)
			return (1);
		/*
		 * Len is 0 if backspaced over the prompt,
		 * 1 if only CR entered.
		 */
		tp = sp->tiq.cqh_first;
		if (tp->len <= 1)
			return (0);

		if (argv_exp1(sp, ep, &cmd, tp->lb + 1, tp->len - 1, 1))
			return (1);
	}
	cmd.argc = EXP(sp)->argsoff;		/* XXX */
	cmd.argv = EXP(sp)->args;		/* XXX */
	return (sp->s_ex_cmd(sp, ep, &cmd, rp));
}
