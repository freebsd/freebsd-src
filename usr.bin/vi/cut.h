/*-
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)cut.h	8.13 (Berkeley) 3/16/94
 */

typedef struct _texth TEXTH;		/* TEXT list head structure. */
CIRCLEQ_HEAD(_texth, _text);

/* Cut buffers. */
struct _cb {
	LIST_ENTRY(_cb) q;		/* Linked list of cut buffers. */
	TEXTH	 textq;			/* Linked list of TEXT structures. */
	CHAR_T	 name;			/* Cut buffer name. */
	size_t	 len;			/* Total length of cut text. */

#define	CB_LMODE	0x01		/* Cut was in line mode. */
	u_char	 flags;
};

/* Lines/blocks of text. */
struct _text {				/* Text: a linked list of lines. */
	CIRCLEQ_ENTRY(_text) q;		/* Linked list of text structures. */
	char	*lb;			/* Line buffer. */
	size_t	 lb_len;		/* Line buffer length. */
	size_t	 len;			/* Line length. */

	/* These fields are used by the vi text input routine. */
	recno_t	 lno;			/* 1-N: line number. */
	size_t	 ai;			/* 0-N: autoindent bytes. */
	size_t	 insert;		/* 0-N: bytes to insert (push). */
	size_t	 offset;		/* 0-N: initial, unerasable bytes. */
	size_t	 owrite;		/* 0-N: bytes to overwrite. */

	/* These fields are used by the ex text input routine. */
	u_char	*wd;			/* Width buffer. */
	size_t	 wd_len;		/* Width buffer length. */
};

/*
 * Get named buffer 'name'.
 * Translate upper-case buffer names to lower-case buffer names.
 */
#define	CBNAME(sp, cbp, name) {						\
	CHAR_T __name;							\
	__name = isupper(name) ? tolower(name) : (name);		\
	for (cbp = sp->gp->cutq.lh_first;				\
	    cbp != NULL; cbp = cbp->q.le_next)				\
		if (cbp->name == __name)				\
			break;						\
}

#define	CUT_DELETE	0x01		/* Delete (rotate numeric buffers). */
#define	CUT_LINEMODE	0x02		/* Cut in line mode. */
int	 cut __P((SCR *, EXF *, CB *, CHAR_T *, MARK *, MARK *, int));
int	 delete __P((SCR *, EXF *, MARK *, MARK *, int));
int	 put __P((SCR *, EXF *, CB *, CHAR_T *, MARK *, MARK *, int));

void	 text_free __P((TEXT *));
TEXT	*text_init __P((SCR *, const char *, size_t, size_t));
void	 text_lfree __P((TEXTH *));
