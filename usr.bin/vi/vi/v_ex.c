/*-
 * Copyright (c) 1992, 1993, 1994
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
static char sccsid[] = "@(#)v_ex.c	8.12 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

static void excmd __P((EXCMDARG *,
    int, int, recno_t, recno_t, int, ARGS *[], ARGS *, char *));

/*
 * v_again -- &
 *	Repeat the previous substitution.
 */
int
v_again(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

	excmd(&cmd, C_SUBAGAIN,
	    2, vp->m_start.lno, vp->m_start.lno, 1, ap, &a, "");
	return (sp->s_ex_cmd(sp, ep, &cmd, &vp->m_final));
}

/*
 * v_at -- @
 *	Execute a buffer.
 */
int
v_at(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

        excmd(&cmd, C_AT, 0, OOBLNO, OOBLNO, 0, ap, &a, NULL);
	if (F_ISSET(vp, VC_BUFFER)) {
		F_SET(&cmd, E_BUFFER);
		cmd.buffer = vp->buffer;
	}
        return (sp->s_ex_cmd(sp, ep, &cmd, &vp->m_final));
}

/*
 * v_ex -- :
 *	Execute a colon command line.
 */
int
v_ex(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (sp->s_ex_run(sp, ep, &vp->m_final));
}

/*
 * v_exmode -- Q
 *	Switch the editor into EX mode.
 */
int
v_exmode(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	/* Save the current line/column number. */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	/* Switch to ex mode. */
	sp->saved_vi_mode = F_ISSET(sp, S_VI_CURSES | S_VI_XAW);
	F_CLR(sp, S_SCREENS);
	F_SET(sp, S_EX);
	return (0);
}

/*
 * v_filter -- [count]!motion command(s)
 *	Run range through shell commands, replacing text.
 */
int
v_filter(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;
	TEXT *tp;

	/*
	 * !!!
	 * Historical vi permitted "!!" in an empty file, and it's handled
	 * as a special case in the ex_bang routine.  Don't modify this setup
	 * without understanding that one.  In particular, note that we're
	 * manipulating the ex argument structures behind ex's back.
	 *
	 * !!!
	 * Historical vi did not permit the '!' command to be associated with
	 * a non-line oriented motion command, in general, although it did
	 * with search commands.  So, !f; and !w would fail, but !/;<CR>
	 * would succeed, even if they all moved to the same location in the
	 * current line.  I don't see any reason to disallow '!' using any of
	 * the possible motion commands.
	 */
	excmd(&cmd, C_BANG,
	    2, vp->m_start.lno, vp->m_stop.lno, 0, ap, &a, NULL);
	EXP(sp)->argsoff = 0;			/* XXX */
	if (F_ISSET(vp,  VC_ISDOT)) {
		if (argv_exp1(sp, ep, &cmd, "!", 1, 1))
			return (1);
	} else {
		/* Get the command from the user. */
		if (sp->s_get(sp, ep, sp->tiqp,
		    '!', TXT_BS | TXT_CR | TXT_ESCAPE | TXT_PROMPT) != INP_OK)
			return (1);
		/*
		 * Len is 0 if backspaced over the prompt,
		 * 1 if only CR entered.
		 */
		tp = sp->tiqp->cqh_first;
		if (tp->len <= 1)
			return (0);

		if (argv_exp1(sp, ep, &cmd, tp->lb + 1, tp->len - 1, 1))
			return (1);
	}
	cmd.argc = EXP(sp)->argsoff;		/* XXX */
	cmd.argv = EXP(sp)->args;		/* XXX */
	return (sp->s_ex_cmd(sp, ep, &cmd, &vp->m_final));
}

/*
 * v_join -- [count]J
 *	Join lines together.
 */
int
v_join(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;
	int lno;

	/*
	 * YASC.
	 * The general rule is that '#J' joins # lines, counting the current
	 * line.  However, 'J' and '1J' are the same as '2J', i.e. join the
	 * current and next lines.  This doesn't map well into the ex command
	 * (which takes two line numbers), so we handle it here.  Note that
	 * we never test for EOF -- historically going past the end of file
	 * worked just fine.
	 */
	lno = vp->m_start.lno + 1;
	if (F_ISSET(vp, VC_C1SET) && vp->count > 2)
		lno = vp->m_start.lno + (vp->count - 1);

	excmd(&cmd, C_JOIN, 2, vp->m_start.lno, lno, 0, ap, &a, NULL);
	return (sp->s_ex_cmd(sp, ep, &cmd, &vp->m_final));
}

/*
 * v_shiftl -- [count]<motion
 *	Shift lines left.
 */
int
v_shiftl(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

	excmd(&cmd, C_SHIFTL,
	    2, vp->m_start.lno, vp->m_stop.lno, 0, ap, &a, "<");
	return (sp->s_ex_cmd(sp, ep, &cmd, &vp->m_final));
}

/*
 * v_shiftr -- [count]>motion
 *	Shift lines right.
 */
int
v_shiftr(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

	excmd(&cmd, C_SHIFTR,
	    2, vp->m_start.lno, vp->m_stop.lno, 0, ap, &a, ">");
	return (sp->s_ex_cmd(sp, ep, &cmd, &vp->m_final));
}

/*
 * v_switch -- ^^
 *	Switch to the previous file.
 */
int
v_switch(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;
	char *name;

	/*
	 * Try the alternate file name, then the previous file
	 * name.  Use the real name, not the user's current name.
	 */
	if ((name = sp->alt_name) == NULL) {
		msgq(sp, M_ERR, "No previous file to edit");
		return (1);
	}

	/* If autowrite is set, write out the file. */
	if (file_m1(sp, ep, 0, FS_ALL))
		return (1);

	excmd(&cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0, ap, &a, name);
	return (sp->s_ex_cmd(sp, ep, &cmd, &vp->m_final));
}

/*
 * v_tagpush -- ^[
 *	Do a tag search on a the cursor keyword.
 */
int
v_tagpush(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

	excmd(&cmd, C_TAG, 0, OOBLNO, 0, 0, ap, &a, vp->keyword);
	return (sp->s_ex_cmd(sp, ep, &cmd, &vp->m_final));
}

/*
 * v_tagpop -- ^T
 *	Pop the tags stack.
 */
int
v_tagpop(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	ARGS *ap[2], a;
	EXCMDARG cmd;

	excmd(&cmd, C_TAGPOP, 0, OOBLNO, 0, 0, ap, &a, NULL);
	return (sp->s_ex_cmd(sp, ep, &cmd, &vp->m_final));
}

/*
 * excmd --
 *	Build an EX command structure.
 */
static void
excmd(cmdp, cmd_id, naddr, lno1, lno2, force, ap, a, arg)
	EXCMDARG *cmdp;
	int cmd_id, force, naddr;
	recno_t lno1, lno2;
	ARGS *ap[2], *a;
	char *arg;
{
	memset(cmdp, 0, sizeof(EXCMDARG));
	cmdp->cmd = &cmds[cmd_id];
	cmdp->addrcnt = naddr;
	cmdp->addr1.lno = lno1;
	cmdp->addr2.lno = lno2;
	cmdp->addr1.cno = cmdp->addr2.cno = 1;
	if (force)
		cmdp->flags |= E_FORCE;
	if ((a->bp = arg) == NULL) {
		cmdp->argc = 0;
		a->len = 0;
	} else {
		cmdp->argc = 1;
		a->len = strlen(arg);
	}
	ap[0] = a;
	ap[1] = NULL;
	cmdp->argv = ap;
}
