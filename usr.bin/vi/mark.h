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
 *
 *	@(#)mark.h	8.8 (Berkeley) 3/16/94
 */

/*
 * The MARK and LMARK structures define positions in the file.  There are
 * two structures because the mark subroutines are the only places where
 * anything cares about something other than line and column.
 *
 * Because of the different interfaces used by the db(3) package, curses,
 * and users, the line number is 1 based and the column number is 0 based.
 * Additionally, it is known that the out-of-band line number is less than
 * any legal line number.  The line number is of type recno_t, as that's
 * the underlying type of the database.  The column number is of type size_t,
 * guaranteeing that we can malloc a line.
 */
struct _mark {
#define	OOBLNO		0		/* Out-of-band line number. */
	recno_t	lno;			/* Line number. */
	size_t	cno;			/* Column number. */
};

struct _lmark {
	LIST_ENTRY(_lmark) q;		/* Linked list of marks. */
	recno_t	lno;			/* Line number. */
	size_t	cno;			/* Column number. */
	CHAR_T	name;			/* Mark name. */

#define	MARK_DELETED	0x01		/* Mark was deleted. */
#define	MARK_USERSET	0x02		/* User set this mark. */
	u_char	flags;
};

#define	ABSMARK1	'\''		/* Absolute mark name. */
#define	ABSMARK2	'`'		/* Absolute mark name. */

/* Mark routines. */
int	mark_end __P((SCR *, EXF *));
int	mark_get __P((SCR *, EXF *, ARG_CHAR_T, MARK *));
int	mark_init __P((SCR *, EXF *));
void	mark_insdel __P((SCR *, EXF *, enum operation, recno_t));
int	mark_set __P((SCR *, EXF *, ARG_CHAR_T, MARK *, int));
