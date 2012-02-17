/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
static char sccsid[] = "@(#)compress.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "ww.h"
#include "tt.h"

	/* special */
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
int cc_trace = 0;
FILE *cc_trace_fp;

	/* tunable parameters */

int cc_reverse = 1;
int cc_sort = 0;
int cc_chop = 0;

int cc_token_max = 8;		/* <= TOKEN_MAX */
int cc_token_min = 2;		/* > tt.tt_put_token_cost */
int cc_npass0 = 1;
int cc_npass1 = 1;

int cc_bufsize = 1024 * 3;	/* XXX, or 80 * 24 * 2 */

int cc_ntoken = 8192;

#define cc_weight XXX
#ifndef cc_weight
int cc_weight = 0;
#endif

#define TOKEN_MAX 16

struct cc {
	char string[TOKEN_MAX];
	char length;
	char flag;
#ifndef cc_weight
	short weight;
#endif
	long time;		/* time last seen */
	short bcount;		/* count in this buffer */
	short ccount;		/* count in compression */
	short places;		/* places in the buffer */
	short code;		/* token code */
	struct cc *qforw, *qback;
	struct cc *hforw, **hback;
};

short cc_thresholds[TOKEN_MAX + 1];
#define thresh(length) (cc_thresholds[length])
#define threshp(code, count, length) \
	((code) >= 0 || (short) (count) >= cc_thresholds[length])

#ifndef cc_weight
short cc_wthresholds[TOKEN_MAX + 1];
#define wthresh(length) (cc_wthresholds[length])
#define wthreshp(weight, length) ((short) (weight) >= cc_wthresholds[length])
#else
#define wthreshp(weight, length) (0)
#endif

#ifndef cc_weight
short cc_wlimits[TOKEN_MAX + 1];
#define wlimit(length) (cc_wlimits[length])
#endif

#define put_token_score(length) ((length) - tt.tt_put_token_cost)

int cc_score_adjustments[TOKEN_MAX + 1][8]; /* XXX, 8 > max of cc_thresholds */
#define score_adjust(score, p) \
	do { \
		int length = (p)->length; \
		int ccount = (p)->ccount; \
		if (threshp((p)->code, ccount, length) || \
		    wthreshp((p)->weight, length)) /* XXX */ \
			(score) -= length - tt.tt_put_token_cost; \
		else \
			(score) += cc_score_adjustments[length][ccount]; \
	} while (0)

int cc_initial_scores[TOKEN_MAX + 1][8]; /* XXX, 8 > max of cc_thresholds */

struct cc cc_q0a, cc_q0b, cc_q1a, cc_q1b;

#define qinsert(p1, p2) \
	do { \
		register struct cc *forw = (p1)->qforw; \
		register struct cc *back = (p1)->qback; \
		back->qforw = forw; \
		forw->qback = back; \
		forw = (p2)->qforw; \
		(p1)->qforw = forw; \
		forw->qback = (p1); \
		(p2)->qforw = (p1); \
		(p1)->qback = (p2); \
	} while (0)

#define qinsertq(q, p) \
	((q)->qforw == (q) ? 0 : \
	 ((q)->qback->qforw = (p)->qforw, \
	  (p)->qforw->qback = (q)->qback, \
	  (q)->qforw->qback = (p), \
	  (p)->qforw = (q)->qforw, \
	  (q)->qforw = (q), \
	  (q)->qback = (q)))

#define H		(14)
#define HSIZE		(1 << H)
#define hash(h, c)	((((h) >> H - 8 | (h) << 8) ^ (c)) & HSIZE - 1)

char *cc_buffer;
struct cc **cc_output;			/* the output array */
short *cc_places[TOKEN_MAX + 1];
short *cc_hashcodes;			/* for computing hashcodes */
struct cc **cc_htab;			/* the hash table */
struct cc **cc_tokens;			/* holds all the active tokens */
struct cc_undo {
	struct cc **pos;
	struct cc *val;
} *cc_undo;

long cc_time, cc_time0;

char *cc_tt_ob, *cc_tt_obe;

ccinit()
{
	register i, j;
	register struct cc *p;

	if (tt.tt_token_max > cc_token_max)
		tt.tt_token_max = cc_token_max;
	if (tt.tt_token_min < cc_token_min)
		tt.tt_token_min = cc_token_min;
	if (tt.tt_token_min > tt.tt_token_max) {
		tt.tt_ntoken = 0;
		return 0;
	}
	if (tt.tt_ntoken > cc_ntoken / 2)	/* not likely */
		tt.tt_ntoken = cc_ntoken / 2;
#define C(x) (sizeof (x) / sizeof *(x))
	for (i = 0; i < C(cc_thresholds); i++) {
		int h = i - tt.tt_put_token_cost;
		if (h > 0)
			cc_thresholds[i] =
				(tt.tt_set_token_cost + 1 + h - 1) / h + 1;
		else
			cc_thresholds[i] = 0;
	}
	for (i = 0; i < C(cc_score_adjustments); i++) {
		int t = cc_thresholds[i];
		for (j = 0; j < C(*cc_score_adjustments); j++) {
			if (j >= t)
				cc_score_adjustments[i][j] =
					- (i - tt.tt_put_token_cost);
			else if (j < t - 1)
				cc_score_adjustments[i][j] = 0;
			else
				/*
				 * cost now is
				 *	length * (ccount + 1)		a
				 * cost before was
				 *	set-token-cost + length +
				 *		ccount * put-token-cost	b
				 * the score adjustment is (b - a)
				 */
				cc_score_adjustments[i][j] =
					tt.tt_set_token_cost + i +
						j * tt.tt_put_token_cost -
							i * (j + 1);
			if (j >= t)
				cc_initial_scores[i][j] = 0;
			else
				/*
				 * - (set-token-cost +
				 *	(length - put-token-cost) -
				 *	(length - put-token-cost) * ccount)
				 */
				cc_initial_scores[i][j] =
					- (tt.tt_set_token_cost +
					   (i - tt.tt_put_token_cost) -
					   (i - tt.tt_put_token_cost) * j);
		}
	}
#ifndef cc_weight
	for (i = 1; i < C(cc_wthresholds); i++) {
		cc_wthresholds[i] =
			((tt.tt_set_token_cost + tt.tt_put_token_cost) / i +
				i / 5 + 1) *
				cc_weight + 1;
		cc_wlimits[i] = cc_wthresholds[i] + cc_weight;
	}
#endif
#undef C
	if ((cc_output = (struct cc **)
	     malloc((unsigned) cc_bufsize * sizeof *cc_output)) == 0)
		goto nomem;
	if ((cc_hashcodes = (short *)
	     malloc((unsigned) cc_bufsize * sizeof *cc_hashcodes)) == 0)
		goto nomem;
	if ((cc_htab = (struct cc **) malloc(HSIZE * sizeof *cc_htab)) == 0)
		goto nomem;
	if ((cc_tokens = (struct cc **)
	     malloc((unsigned)
	            (cc_ntoken + tt.tt_token_max - tt.tt_token_min + 1) *
		    sizeof *cc_tokens)) == 0)
		goto nomem;
	if ((cc_undo = (struct cc_undo *)
	     malloc((unsigned) cc_bufsize * sizeof *cc_undo)) == 0)
		goto nomem;
	for (i = tt.tt_token_min; i <= tt.tt_token_max; i++)
		if ((cc_places[i] = (short *)
		     malloc((unsigned) cc_bufsize * sizeof **cc_places)) == 0)
			goto nomem;
	cc_q0a.qforw = cc_q0a.qback = &cc_q0a;
	cc_q0b.qforw = cc_q0b.qback = &cc_q0b;
	cc_q1a.qforw = cc_q1a.qback = &cc_q1a;
	cc_q1b.qforw = cc_q1b.qback = &cc_q1b;
	if ((p = (struct cc *) malloc((unsigned) cc_ntoken * sizeof *p)) == 0)
		goto nomem;
	for (i = 0; i < tt.tt_ntoken; i++) {
		p->code = i;
		p->time = -1;
		p->qback = cc_q0a.qback;
		p->qforw = &cc_q0a;
		p->qback->qforw = p;
		cc_q0a.qback = p;
		p++;
	}
	for (; i < cc_ntoken; i++) {
		p->code = -1;
		p->time = -1;
		p->qback = cc_q1a.qback;
		p->qforw = &cc_q1a;
		p->qback->qforw = p;
		cc_q1a.qback = p;
		p++;
	}
	cc_tt_ob = tt_ob;
	cc_tt_obe = tt_obe;
	if ((cc_buffer = malloc((unsigned) cc_bufsize)) == 0)
		goto nomem;
	return 0;
nomem:
	wwerrno = WWE_NOMEM;
	return -1;
}

ccstart()
{
	int ccflush();

	ttflush();
	tt_obp = tt_ob = cc_buffer;
	tt_obe = tt_ob + cc_bufsize;
	tt.tt_flush = ccflush;
	if (cc_trace) {
		cc_trace_fp = fopen("window-trace", "a");
		(void) fcntl(fileno(cc_trace_fp), F_SETFD, 1);
	}
	ccreset();
}

ccreset()
{
	register struct cc *p;

	bzero((char *) cc_htab, HSIZE * sizeof *cc_htab);
	for (p = cc_q0a.qforw; p != &cc_q0a; p = p->qforw)
		p->hback = 0;
	for (p = cc_q1a.qforw; p != &cc_q1a; p = p->qforw)
		p->hback = 0;
}

ccend()
{

	ttflush();
	tt_obp = tt_ob = cc_tt_ob;
	tt_obe = cc_tt_obe;
	tt.tt_flush = 0;
	if (cc_trace_fp != NULL) {
		(void) fclose(cc_trace_fp);
		cc_trace_fp = NULL;
	}
}

ccflush()
{
	int bufsize = tt_obp - tt_ob;
	int n;

	if (tt_ob != cc_buffer)
		abort();
	if (cc_trace_fp != NULL) {
		(void) fwrite(tt_ob, 1, bufsize, cc_trace_fp);
		(void) putc(-1, cc_trace_fp);
	}
	tt.tt_flush = 0;
	(*tt.tt_compress)(1);
	if (bufsize < tt.tt_token_min) {
		ttflush();
		goto out;
	}
	tt_obp = tt_ob = cc_tt_ob;
	tt_obe = cc_tt_obe;
	cc_time0 = cc_time;
	cc_time += bufsize;
	n = cc_sweep_phase(cc_buffer, bufsize, cc_tokens);
	cc_compress_phase(cc_output, bufsize, cc_tokens, n);
	cc_output_phase(cc_buffer, cc_output, bufsize);
	ttflush();
	tt_obp = tt_ob = cc_buffer;
	tt_obe = cc_buffer + cc_bufsize;
out:
	(*tt.tt_compress)(0);
	tt.tt_flush = ccflush;
}

cc_sweep_phase(buffer, bufsize, tokens)
	char *buffer;
	struct cc **tokens;
{
	register struct cc **pp = tokens;
	register i, n;
#ifdef STATS
	int nn, ii;
#endif

#ifdef STATS
	if (verbose >= 0)
		time_begin();
	if (verbose > 0)
		printf("Sweep:");
#endif
	cc_sweep0(buffer, bufsize, tt.tt_token_min - 1);
#ifdef STATS
	ntoken_stat = 0;
	nn = 0;
	ii = 0;
#endif
	for (i = tt.tt_token_min; i <= tt.tt_token_max; i++) {
#ifdef STATS
		if (verbose > 0) {
			if (ii > 7) {
				printf("\n      ");
				ii = 0;
			}
			ii++;
			printf(" (%d", i);
			(void) fflush(stdout);
		}
#endif
		n = cc_sweep(buffer, bufsize, pp, i);
		pp += n;
#ifdef STATS
		if (verbose > 0) {
			if (--n > 0) {
				printf(" %d", n);
				nn += n;
			}
			putchar(')');
		}
#endif
	}
	qinsertq(&cc_q1b, &cc_q1a);
#ifdef STATS
	if (verbose > 0)
		printf("\n       %d tokens, %d candidates\n",
			ntoken_stat, nn);
	if (verbose >= 0)
		time_end();
#endif
	return pp - tokens;
}

cc_sweep0(buffer, n, length)
	char *buffer;
{
	register char *p;
	register short *hc;
	register i;
	register short c;
	register short pc = tt.tt_padc;

	/* n and length are at least 1 */
	p = buffer++;
	hc = cc_hashcodes;
	i = n;
	do {
		if ((*hc++ = *p++) == pc)
			hc[-1] = -1;
	} while (--i);
	while (--length) {
		p = buffer++;
		hc = cc_hashcodes;
		for (i = n--; --i;) {
			if ((c = *p++) == pc || *hc < 0)
				c = -1;
			else
				c = hash(*hc, c);
			*hc++ = c;
		}
	}
}

cc_sweep(buffer, bufsize, tokens, length)
	char *buffer;
	struct cc **tokens;
	register length;
{
	register struct cc *p;
	register char *cp;
	register i;
	short *hc;
	short *places = cc_places[length];
	struct cc **pp = tokens;
	short threshold = thresh(length);
#ifndef cc_weight
	short wthreshold = wthresh(length);
	short limit = wlimit(length);
#endif
	int time;
	short pc = tt.tt_padc;

	i = length - 1;
	bufsize -= i;
	cp = buffer + i;
	hc = cc_hashcodes;
	time = cc_time0;
	for (i = 0; i < bufsize; i++, time++) {
		struct cc **h;

		{
			register short *hc1 = hc;
			register short c = *cp++;
			register short hh;
			if ((hh = *hc1) < 0 || c == pc) {
				*hc1++ = -1;
				hc = hc1;
				continue;
			}
			h = cc_htab + (*hc1++ = hash(hh, c));
			hc = hc1;
		}
		for (p = *h; p != 0; p = p->hforw)
			if (p->length == (char) length) {
				register char *p1 = p->string;
				register char *p2 = cp - length;
				register n = length;
				do
					if (*p1++ != *p2++)
						goto fail;
				while (--n);
				break;
			fail:;
			}
		if (p == 0) {
			p = cc_q1a.qback;
			if (p == &cc_q1a ||
			    p->time >= cc_time0 && p->length == (char) length)
				continue;
			if (p->hback != 0)
				if ((*p->hback = p->hforw) != 0)
					p->hforw->hback = p->hback;
			{
				register char *p1 = p->string;
				register char *p2 = cp - length;
				register n = length;
				do
					*p1++ = *p2++;
				while (--n);
			}
			p->length = length;
#ifndef cc_weight
			p->weight = cc_weight;
#endif
			p->time = time;
			p->bcount = 1;
			p->ccount = 0;
			p->flag = 0;
			if ((p->hforw = *h) != 0)
				p->hforw->hback = &p->hforw;
			*h = p;
			p->hback = h;
			qinsert(p, &cc_q1a);
			places[i] = -1;
			p->places = i;
#ifdef STATS
			ntoken_stat++;
#endif
		} else if (p->time < cc_time0) {
#ifndef cc_weight
			if ((p->weight += p->time - time) < 0)
				p->weight = cc_weight;
			else if ((p->weight += cc_weight) > limit)
				p->weight = limit;
#endif
			p->time = time;
			p->bcount = 1;
			p->ccount = 0;
			if (p->code >= 0) {
				p->flag = 1;
				*pp++ = p;
			} else
#ifndef cc_weight
			if (p->weight >= wthreshold) {
				p->flag = 1;
				*pp++ = p;
				qinsert(p, &cc_q1b);
			} else
#endif
			{
				p->flag = 0;
				qinsert(p, &cc_q1a);
			}
			places[i] = -1;
			p->places = i;
#ifdef STATS
			ntoken_stat++;
#endif
		} else if (p->time + length > time) {
			/*
			 * overlapping token, don't count as two and
			 * don't update time, but do adjust weight to offset
			 * the difference
			 */
#ifndef cc_weight
			if (cc_weight != 0) {	/* XXX */
				p->weight += time - p->time;
				if (!p->flag && p->weight >= wthreshold) {
					p->flag = 1;
					*pp++ = p;
					qinsert(p, &cc_q1b);
				}
			}
#endif
			places[i] = p->places;
			p->places = i;
		} else {
#ifndef cc_weight
			if ((p->weight += p->time - time) < 0)
				p->weight = cc_weight;
			else if ((p->weight += cc_weight) > limit)
				p->weight = limit;
#endif
			p->time = time;
			p->bcount++;
			if (!p->flag &&
			    /* code must be < 0 if flag false here */
			    (p->bcount >= threshold
#ifndef cc_weight
			     || p->weight >= wthreshold
#endif
			     )) {
				p->flag = 1;
				*pp++ = p;
				qinsert(p, &cc_q1b);
			}
			places[i] = p->places;
			p->places = i;
		}
	}
	if ((i = pp - tokens) > 0) {
		*pp = 0;
		if (cc_reverse)
			cc_sweep_reverse(tokens, places);
		if (cc_sort && i > 1) {
			int cc_token_compare();
			qsort((char *) tokens, i, sizeof *tokens,
			      cc_token_compare);
		}
		if (cc_chop) {
			if ((i = i * cc_chop / 100) == 0)
				i = 1;
			tokens[i] = 0;
		}
		i++;
	}
	return i;
}

cc_sweep_reverse(pp, places)
	register struct cc **pp;
	register short *places;
{
	register struct cc *p;
	register short front, back, t;

	while ((p = *pp++) != 0) {
		back = -1;
		t = p->places;
		/* the list is never empty */
		do {
			front = places[t];
			places[t] = back;
			back = t;
		} while ((t = front) >= 0);
		p->places = back;
	}
}

cc_compress_phase(output, bufsize, tokens, ntoken)
	struct cc **output;
	struct cc **tokens;
{
	register i;

	bzero((char *) output, bufsize * sizeof *output);
	for (i = 0; i < cc_npass0; i++)
		cc_compress_phase1(output, tokens, ntoken, 0);
	for (i = 0; i < cc_npass1; i++)
		cc_compress_phase1(output, tokens, ntoken, 1);
	cc_compress_cleanup(output, bufsize);
}

cc_compress_phase1(output, tokens, ntoken, flag)
	register struct cc **output;
	struct cc **tokens;
{
	register struct cc **pp;
#ifdef STATS
	register int i = 0;
	int nt = 0, cc = 0, nc = 0;
#endif

#ifdef STATS
	if (verbose >= 0)
		time_begin();
	if (verbose > 0)
		printf("Compress:");
#endif
	pp = tokens;
	while (pp < tokens + ntoken) {
#ifdef STATS
		if (verbose > 0) {
			ntoken_stat = 0;
			ccount_stat = 0;
			ncover_stat = 0;
			if (i > 2) {
				printf("\n         ");
				i = 0;
			}
			i++;
			printf(" (%d", (*pp)->length);
			(void) fflush(stdout);
		}
#endif
		pp += cc_compress(output, pp, flag);
#ifdef STATS
		if (verbose > 0) {
			printf(" %dt %du %dc)", ntoken_stat, ccount_stat,
			       ncover_stat);
			nt += ntoken_stat;
			cc += ccount_stat;
			nc += ncover_stat;
		}
#endif
	}
#ifdef STATS
	if (verbose > 0)
		printf("\n   total: (%dt %du %dc)\n", nt, cc, nc);
	if (verbose >= 0)
		time_end();
#endif
}

cc_compress_cleanup(output, bufsize)
	register struct cc **output;
{
	register struct cc **end;

	/* the previous output phase may have been interrupted */
	qinsertq(&cc_q0b, &cc_q0a);
	for (end = output + bufsize; output < end;) {
		register struct cc *p;
		register length;
		if ((p = *output) == 0) {
			output++;
			continue;
		}
		length = p->length;
		if (!p->flag) {
		} else if (p->code >= 0) {
			qinsert(p, &cc_q0b);
			p->flag = 0;
		} else if (p->ccount == 0) {
			*output = 0;
		} else if (p->ccount >= thresh(length)
#ifndef cc_weight
			   || wthreshp(p->weight, length)
#endif
			   ) {
			p->flag = 0;
		} else {
			p->ccount = 0;
			*output = 0;
		}
		output += length;
	}
}

cc_compress(output, tokens, flag)
	struct cc **output;
	struct cc **tokens;
	char flag;
{
	struct cc **pp = tokens;
	register struct cc *p = *pp++;
	int length = p->length;
	int threshold = thresh(length);
#ifndef cc_weight
	short wthreshold = wthresh(length);
#endif
	short *places = cc_places[length];
	int *initial_scores = cc_initial_scores[length];
	int initial_score0 = put_token_score(length);

	do {
		int score;
		register struct cc_undo *undop;
		int ccount;
#ifdef STATS
		int ncover;
#endif
		int i;

		ccount = p->ccount;
		if ((short) ccount >= p->bcount)
			continue;
		if (p->code >= 0 || ccount >= threshold)
			score = 0;
#ifndef cc_weight
		else if (p->weight >= wthreshold)
			/* allow one fewer match than normal */
			/* XXX, should adjust for ccount */
			score = - tt.tt_set_token_cost;
#endif
		else
			score = initial_scores[ccount];
		undop = cc_undo;
#ifdef STATS
		ncover = 0;
#endif
		for (i = p->places; i >= 0; i = places[i]) {
			register struct cc **jp;
			register struct cc *x;
			register struct cc **ip = output + i;
			register score0 = initial_score0;
			struct cc **iip = ip + length;
			struct cc_undo *undop1 = undop;

			if ((x = *(jp = ip)) != 0)
				goto z;
			while (--jp >= output)
				if ((x = *jp) != 0) {
					if (jp + x->length > ip)
						goto z;
					break;
				}
			jp = ip + 1;
			while (jp < iip) {
				if ((x = *jp) == 0) {
					jp++;
					continue;
				}
			z:
				if (x == p)
					goto undo;
#ifdef STATS
				ncover++;
#endif
				undop->pos = jp;
				undop->val = x;
				undop++;
				*jp = 0;
				x->ccount--;
				score_adjust(score0, x);
				if (score0 < 0 && flag)
					goto undo;
				jp += x->length;
			}
			undop->pos = ip;
			undop->val = 0;
			undop++;
			*ip = p;
			ccount++;
			score += score0;
			continue;
		undo:
			while (--undop >= undop1)
				if (*undop->pos = x = undop->val)
					x->ccount++;
			undop++;
		}
		if (score > 0) {
#ifdef STATS
			ccount_stat += ccount - p->ccount;
			ntoken_stat++;
			ncover_stat += ncover;
#endif
			p->ccount = ccount;
		} else {
			register struct cc_undo *u = cc_undo;
			while (--undop >= u) {
				register struct cc *x;
				if (*undop->pos = x = undop->val)
					x->ccount++;
			}
		}
	} while ((p = *pp++) != 0);
	return pp - tokens;
}

cc_output_phase(buffer, output, bufsize)
	register char *buffer;
	register struct cc **output;
	register bufsize;
{
	register i;
	register struct cc *p, *p1;

	for (i = 0; i < bufsize;) {
		if ((p = output[i]) == 0) {
			ttputc(buffer[i]);
			i++;
		} else if (p->code >= 0) {
			if (--p->ccount == 0)
				qinsert(p, &cc_q0a);
			(*tt.tt_put_token)(p->code, p->string, p->length);
			wwntokuse++;
			wwntoksave += put_token_score(p->length);
			i += p->length;
		} else if ((p1 = cc_q0a.qback) != &cc_q0a) {
			p->code = p1->code;
			p1->code = -1;
			qinsert(p1, &cc_q1a);
			if (--p->ccount == 0)
				qinsert(p, &cc_q0a);
			else
				qinsert(p, &cc_q0b);
			(*tt.tt_set_token)(p->code, p->string, p->length);
			wwntokdef++;
			wwntoksave -= tt.tt_set_token_cost;
			i += p->length;
		} else {
			p->ccount--;
			ttwrite(p->string, p->length);
			wwntokbad++;
			i += p->length;
		}
	}
	wwntokc += bufsize;
}

cc_token_compare(p1, p2)
	struct cc **p1, **p2;
{
	return (*p2)->bcount - (*p1)->bcount;
}
