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
static char sccsid[] = "@(#)seq.c	8.26 (Berkeley) 3/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "seq.h"
#include "excmd.h"

/*
 * seq_set --
 *	Internal version to enter a sequence.
 */
int
seq_set(sp, name, nlen, input, ilen, output, olen, stype, flags)
	SCR *sp;
	char *name, *input, *output;
	size_t nlen, ilen, olen;
	enum seqtype stype;
	int flags;
{
	SEQ *lastqp, *qp;
	CHAR_T *p;
	int sv_errno;

	/*
	 * An input string must always be present.  The output string
	 * can be NULL, when set internally, that's how we throw away
	 * input.
	 *
	 * Just replace the output field if the string already set.
	 */
	if ((qp = seq_find(sp, &lastqp, input, ilen, stype, NULL)) != NULL) {
		if (output == NULL || olen == 0) {
			p = NULL;
			olen = 0;
		} else if ((p = v_strdup(sp, output, olen)) == NULL) {
			sv_errno = errno;
			goto mem1;
		}
		if (qp->output != NULL)
			free(qp->output);
		qp->olen = olen;
		qp->output = p;
		return (0);
	}

	/* Allocate and initialize SEQ structure. */
	CALLOC(sp, qp, SEQ *, 1, sizeof(SEQ));
	if (qp == NULL) {
		sv_errno = errno;
		goto mem1;
	}

	/* Name. */
	if (name == NULL || nlen == 0)
		qp->name = NULL;
	else if ((qp->name = v_strdup(sp, name, nlen)) == NULL) {
		sv_errno = errno;
		goto mem2;
	}
	qp->nlen = nlen;

	/* Input. */
	if ((qp->input = v_strdup(sp, input, ilen)) == NULL) {
		sv_errno = errno;
		goto mem3;
	}
	qp->ilen = ilen;

	/* Output. */
	if (output == NULL) {
		qp->output = NULL;
		olen = 0;
	} else if ((qp->output = v_strdup(sp, output, olen)) == NULL) {
		sv_errno = errno;
		free(qp->input);
mem3:		if (qp->name != NULL)
			free(qp->name);
mem2:		FREE(qp, sizeof(SEQ));
mem1:		errno = sv_errno;
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	qp->olen = olen;

	/* Type, flags. */
	qp->stype = stype;
	qp->flags = flags;

	/* Link into the chain. */
	if (lastqp == NULL) {
		LIST_INSERT_HEAD(&sp->gp->seqq, qp, q);
	} else {
		LIST_INSERT_AFTER(lastqp, qp, q);
	}

	/* Set the fast lookup bit. */
	bit_set(sp->gp->seqb, qp->input[0]);

	return (0);
}

/*
 * seq_delete --
 *	Delete a sequence.
 */
int
seq_delete(sp, input, ilen, stype)
	SCR *sp;
	char *input;
	size_t ilen;
	enum seqtype stype;
{
	SEQ *qp;

	if ((qp = seq_find(sp, NULL, input, ilen, stype, NULL)) == NULL)
		return (1);

	LIST_REMOVE(qp, q);
	if (qp->name != NULL)
		free(qp->name);
	free(qp->input);
	if (qp->output != NULL)
		free(qp->output);
	FREE(qp, sizeof(SEQ));
	return (0);
}

/*
 * seq_find --
 *	Search the sequence list for a match to a buffer, if ispartial
 *	isn't NULL, partial matches count.
 */
SEQ *
seq_find(sp, lastqp, input, ilen, stype, ispartialp)
	SCR *sp;
	SEQ **lastqp;
	char *input;
	size_t ilen;
	enum seqtype stype;
	int *ispartialp;
{
	SEQ *lqp, *qp;
	int diff;

	/*
	 * Ispartialp is a location where we return if there was a
	 * partial match, i.e. if the string were extended it might
	 * match something.
	 *
	 * XXX
	 * Overload the meaning of ispartialp; only the terminal key
	 * search doesn't want the search limited to complete matches,
	 * i.e. ilen may be longer than the match.
	 */
	if (ispartialp != NULL)
		*ispartialp = 0;
	for (lqp = NULL, qp = sp->gp->seqq.lh_first;
	    qp != NULL; lqp = qp, qp = qp->q.le_next) {
		/* Fast checks on the first character and type. */
		if (qp->input[0] > input[0])
			break;
		if (qp->input[0] < input[0] || qp->stype != stype)
			continue;

		/* Check on the real comparison. */
		diff = memcmp(qp->input, input, MIN(qp->ilen, ilen));
		if (diff > 0)
			break;
		if (diff < 0)
			continue;
		/*
		 * If the entry is the same length as the string, return a
		 * match.  If the entry is shorter than the string, return a
		 * match if called from the terminal key routine.  Otherwise,
		 * keep searching for a complete match.
		 */
		if (qp->ilen <= ilen) {
			if (qp->ilen == ilen || ispartialp != NULL) {
				if (lastqp != NULL)
					*lastqp = lqp;
				return (qp);
			}
			continue;
		}
		/*
		 * If the entry longer than the string, return partial match
		 * if called from the terminal key routine.  Otherwise, no
		 * match.
		 */
		if (ispartialp != NULL)
			*ispartialp = 1;
		break;
	}
	if (lastqp != NULL)
		*lastqp = lqp;
	return (NULL);
}

/*
 * seq_dump --
 *	Display the sequence entries of a specified type.
 */
int
seq_dump(sp, stype, isname)
	SCR *sp;
	enum seqtype stype;
	int isname;
{
	CHNAME const *cname;
	SEQ *qp;
	int cnt, len, olen;
	char *p;

	cnt = 0;
	cname = sp->gp->cname;
	for (qp = sp->gp->seqq.lh_first; qp != NULL; qp = qp->q.le_next) {
		if (stype != qp->stype)
			continue;
		++cnt;
		for (p = qp->input,
		    olen = qp->ilen, len = 0; olen > 0; --olen)
			len += ex_printf(EXCOOKIE, "%s", cname[*p++].name);
		for (len = STANDARD_TAB - len % STANDARD_TAB; len > 0;)
			len -= ex_printf(EXCOOKIE, " ");

		if (qp->output != NULL)
			for (p = qp->output,
			    olen = qp->olen, len = 0; olen > 0; --olen)
				len +=
				    ex_printf(EXCOOKIE, "%s", cname[*p++].name);
		else
			len = 0;

		if (isname && qp->name != NULL) {
			for (len = STANDARD_TAB - len % STANDARD_TAB; len > 0;)
				len -= ex_printf(EXCOOKIE, " ");
			for (p = qp->name, olen = qp->nlen; olen > 0; --olen)
				(void)ex_printf(EXCOOKIE,
				    "%s", cname[*p++].name);
		}
		(void)ex_printf(EXCOOKIE, "\n");
	}
	return (cnt);
}

/*
 * seq_save --
 *	Save the sequence entries to a file.
 */
int
seq_save(sp, fp, prefix, stype)
	SCR *sp;
	FILE *fp;
	char *prefix;
	enum seqtype stype;
{
	SEQ *qp;
	size_t olen;
	int ch;
	char *p;

	/* Write a sequence command for all keys the user defined. */
	for (qp = sp->gp->seqq.lh_first; qp != NULL; qp = qp->q.le_next) {
		if (!F_ISSET(qp, S_USERDEF) || stype != qp->stype)
			continue;
		if (prefix)
			(void)fprintf(fp, "%s", prefix);
		for (p = qp->input, olen = qp->ilen; olen > 0; --olen) {
			ch = *p++;
			if (ch == LITERAL_CH || ch == '|' ||
			    isblank(ch) || term_key_val(sp, ch) == K_NL)
				(void)putc(LITERAL_CH, fp);
			(void)putc(ch, fp);
		}
		(void)putc(' ', fp);
		if (qp->output != NULL)
			for (p = qp->output,
			    olen = qp->olen; olen > 0; --olen) {
				ch = *p++;
				if (ch == LITERAL_CH || ch == '|' ||
				    term_key_val(sp, ch) == K_NL)
					(void)putc(LITERAL_CH, fp);
				(void)putc(ch, fp);
			}
		(void)putc('\n', fp);
	}
	return (0);
}
