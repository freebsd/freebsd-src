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
 *	@(#)seq.h	8.9 (Berkeley) 3/16/94
 */

/*
 * Map and abbreviation structures.
 *
 * The map structure is doubly linked list, sorted by input string and by
 * input length within the string.  (The latter is necessary so that short
 * matches will happen before long matches when the list is searched.)
 * Additionally, there is a bitmap which has bits set if there are entries
 * starting with the corresponding character.  This keeps us from walking
 * the list unless it's necessary.
 *
 * The name and the output fields of a SEQ can be empty, i.e. NULL.
 * Only the input field is required.
 *
 * XXX
 * The fast-lookup bits are never turned off -- users don't usually unmap
 * things, though, so it's probably not a big deal.
 */
					/* Sequence type. */
enum seqtype { SEQ_ABBREV, SEQ_COMMAND, SEQ_INPUT };

struct _seq {
	LIST_ENTRY(_seq) q;		/* Linked list of all sequences. */
	enum seqtype stype;		/* Sequence type. */
	char	*name;			/* Sequence name (if any). */
	size_t	 nlen;			/* Name length. */
	char	*input;			/* Sequence input keys. */
	size_t	 ilen;			/* Input keys length. */
	char	*output;		/* Sequence output keys. */
	size_t	 olen;			/* Output keys length. */

#define	S_USERDEF	0x01		/* If sequence user defined. */
	u_char	 flags;
};

int	 abbr_save __P((SCR *, FILE *));
int	 map_save __P((SCR *, FILE *));
int	 seq_delete __P((SCR *, char *, size_t, enum seqtype));
int	 seq_dump __P((SCR *, enum seqtype, int));
SEQ	*seq_find __P((SCR *, SEQ **, char *, size_t, enum seqtype, int *));
void	 seq_init __P((SCR *));
int	 seq_save __P((SCR *, FILE *, char *, enum seqtype));
int	 seq_set __P((SCR *, char *, size_t,
	    char *, size_t, char *, size_t, enum seqtype, int));
