/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)v_match.c	10.8 (Berkeley) 3/6/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../common/common.h"
#include "vi.h"

/*
 * v_match -- %
 *	Search to matching character.
 *
 * PUBLIC: int v_match __P((SCR *, VICMD *));
 */
int
v_match(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	VCS cs;
	MARK *mp;
	size_t cno, len, off;
	int cnt, isempty, matchc, startc, (*gc)__P((SCR *, VCS *));
	char *p;

	/*
	 * !!!
	 * Historic practice; ignore the count.
	 *
	 * !!!
	 * Historical practice was to search for the initial character in the
	 * forward direction only.
	 */
	if (db_eget(sp, vp->m_start.lno, &p, &len, &isempty)) {
		if (isempty)
			goto nomatch;
		return (1);
	}
	for (off = vp->m_start.cno;; ++off) {
		if (off >= len) {
nomatch:		msgq(sp, M_BERR, "184|No match character on this line");
			return (1);
		}
		switch (startc = p[off]) {
		case '(':
			matchc = ')';
			gc = cs_next;
			break;
		case ')':
			matchc = '(';
			gc = cs_prev;
			break;
		case '[':
			matchc = ']';
			gc = cs_next;
			break;
		case ']':
			matchc = '[';
			gc = cs_prev;
			break;
		case '{':
			matchc = '}';
			gc = cs_next;
			break;
		case '}':
			matchc = '{';
			gc = cs_prev;
			break;
		case '<':
			matchc = '>';
			gc = cs_next;
			break;
		case '>':
			matchc = '<';
			gc = cs_prev;
			break;
		default:
			continue;
		}
		break;
	}

	cs.cs_lno = vp->m_start.lno;
	cs.cs_cno = off;
	if (cs_init(sp, &cs))
		return (1);
	for (cnt = 1;;) {
		if (gc(sp, &cs))
			return (1);
		if (cs.cs_flags != 0) {
			if (cs.cs_flags == CS_EOF || cs.cs_flags == CS_SOF)
				break;
			continue;
		}
		if (cs.cs_ch == startc)
			++cnt;
		else if (cs.cs_ch == matchc && --cnt == 0)
			break;
	}
	if (cnt) {
		msgq(sp, M_BERR, "185|Matching character not found");
		return (1);
	}

	vp->m_stop.lno = cs.cs_lno;
	vp->m_stop.cno = cs.cs_cno;

	/*
	 * If moving right, non-motion commands move to the end of the range.
	 * Delete and yank stay at the start.
	 *
	 * If moving left, all commands move to the end of the range.
	 *
	 * !!!
	 * Don't correct for leftward movement -- historic vi deleted the
	 * starting cursor position when deleting to a match.
	 */
	if (vp->m_start.lno < vp->m_stop.lno ||
	    vp->m_start.lno == vp->m_stop.lno &&
	    vp->m_start.cno < vp->m_stop.cno)
		vp->m_final = ISMOTION(vp) ? vp->m_start : vp->m_stop;
	else
		vp->m_final = vp->m_stop;

	/*
	 * !!!
	 * If the motion is across lines, and the earliest cursor position
	 * is at or before any non-blank characters in the line, i.e. the
	 * movement is cutting all of the line's text, and the later cursor
	 * position has nothing other than whitespace characters between it
	 * and the end of its line, the buffer is in line mode.
	 */
	if (!ISMOTION(vp) || vp->m_start.lno == vp->m_stop.lno)
		return (0);
	mp = vp->m_start.lno < vp->m_stop.lno ? &vp->m_start : &vp->m_stop;
	if (mp->cno != 0) {
		cno = 0;
		if (nonblank(sp, mp->lno, &cno))
			return (1);
		if (cno < mp->cno)
			return (0);
	}
	mp = vp->m_start.lno < vp->m_stop.lno ? &vp->m_stop : &vp->m_start;
	if (db_get(sp, mp->lno, DBG_FATAL, &p, &len))
		return (1);
	for (p += mp->cno + 1, len -= mp->cno; --len; ++p)
		if (!isblank(*p))
			return (0);
	F_SET(vp, VM_LMODE);
	return (0);
}
