/*	$Id: term.c,v 1.201 2011/09/21 09:57:13 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010, 2011 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "term.h"
#include "main.h"

static	void		 adjbuf(struct termp *p, int);
static	void		 bufferc(struct termp *, char);
static	void		 encode(struct termp *, const char *, size_t);
static	void		 encode1(struct termp *, int);

void
term_free(struct termp *p)
{

	if (p->buf)
		free(p->buf);
	if (p->symtab)
		mchars_free(p->symtab);

	free(p);
}


void
term_begin(struct termp *p, term_margin head, 
		term_margin foot, const void *arg)
{

	p->headf = head;
	p->footf = foot;
	p->argf = arg;
	(*p->begin)(p);
}


void
term_end(struct termp *p)
{

	(*p->end)(p);
}

/*
 * Flush a line of text.  A "line" is loosely defined as being something
 * that should be followed by a newline, regardless of whether it's
 * broken apart by newlines getting there.  A line can also be a
 * fragment of a columnar list (`Bl -tag' or `Bl -column'), which does
 * not have a trailing newline.
 *
 * The following flags may be specified:
 *
 *  - TERMP_NOBREAK: this is the most important and is used when making
 *    columns.  In short: don't print a newline and instead expect the
 *    next call to do the padding up to the start of the next column.
 *
 *  - TERMP_TWOSPACE: make sure there is room for at least two space
 *    characters of padding.  Otherwise, rather break the line.
 *
 *  - TERMP_DANGLE: don't newline when TERMP_NOBREAK is specified and
 *    the line is overrun, and don't pad-right if it's underrun.
 *
 *  - TERMP_HANG: like TERMP_DANGLE, but doesn't newline when
 *    overrunning, instead save the position and continue at that point
 *    when the next invocation.
 *
 *  In-line line breaking:
 *
 *  If TERMP_NOBREAK is specified and the line overruns the right
 *  margin, it will break and pad-right to the right margin after
 *  writing.  If maxrmargin is violated, it will break and continue
 *  writing from the right-margin, which will lead to the above scenario
 *  upon exit.  Otherwise, the line will break at the right margin.
 */
void
term_flushln(struct termp *p)
{
	int		 i;     /* current input position in p->buf */
	size_t		 vis;   /* current visual position on output */
	size_t		 vbl;   /* number of blanks to prepend to output */
	size_t		 vend;	/* end of word visual position on output */
	size_t		 bp;    /* visual right border position */
	size_t		 dv;    /* temporary for visual pos calculations */
	int		 j;     /* temporary loop index for p->buf */
	int		 jhy;	/* last hyph before overflow w/r/t j */
	size_t		 maxvis; /* output position of visible boundary */
	size_t		 mmax; /* used in calculating bp */

	/*
	 * First, establish the maximum columns of "visible" content.
	 * This is usually the difference between the right-margin and
	 * an indentation, but can be, for tagged lists or columns, a
	 * small set of values. 
	 */
	assert  (p->rmargin >= p->offset);
	dv     = p->rmargin - p->offset;
	maxvis = (int)dv > p->overstep ? dv - (size_t)p->overstep : 0;
	dv     = p->maxrmargin - p->offset;
	mmax   = (int)dv > p->overstep ? dv - (size_t)p->overstep : 0;

	bp = TERMP_NOBREAK & p->flags ? mmax : maxvis;

	/*
	 * Calculate the required amount of padding.
	 */
	vbl = p->offset + p->overstep > p->viscol ?
	      p->offset + p->overstep - p->viscol : 0;

	vis = vend = 0;
	i = 0;

	while (i < p->col) {
		/*
		 * Handle literal tab characters: collapse all
		 * subsequent tabs into a single huge set of spaces.
		 */
		while (i < p->col && '\t' == p->buf[i]) {
			vend = (vis / p->tabwidth + 1) * p->tabwidth;
			vbl += vend - vis;
			vis = vend;
			i++;
		}

		/*
		 * Count up visible word characters.  Control sequences
		 * (starting with the CSI) aren't counted.  A space
		 * generates a non-printing word, which is valid (the
		 * space is printed according to regular spacing rules).
		 */

		for (j = i, jhy = 0; j < p->col; j++) {
			if ((j && ' ' == p->buf[j]) || '\t' == p->buf[j])
				break;

			/* Back over the the last printed character. */
			if (8 == p->buf[j]) {
				assert(j);
				vend -= (*p->width)(p, p->buf[j - 1]);
				continue;
			}

			/* Regular word. */
			/* Break at the hyphen point if we overrun. */
			if (vend > vis && vend < bp && 
					ASCII_HYPH == p->buf[j])
				jhy = j;

			vend += (*p->width)(p, p->buf[j]);
		}

		/*
		 * Find out whether we would exceed the right margin.
		 * If so, break to the next line.
		 */
		if (vend > bp && 0 == jhy && vis > 0) {
			vend -= vis;
			(*p->endline)(p);
			p->viscol = 0;
			if (TERMP_NOBREAK & p->flags) {
				vbl = p->rmargin;
				vend += p->rmargin - p->offset;
			} else
				vbl = p->offset;

			/* Remove the p->overstep width. */

			bp += (size_t)p->overstep;
			p->overstep = 0;
		}

		/* Write out the [remaining] word. */
		for ( ; i < p->col; i++) {
			if (vend > bp && jhy > 0 && i > jhy)
				break;
			if ('\t' == p->buf[i])
				break;
			if (' ' == p->buf[i]) {
				j = i;
				while (' ' == p->buf[i])
					i++;
				dv = (size_t)(i - j) * (*p->width)(p, ' ');
				vbl += dv;
				vend += dv;
				break;
			}
			if (ASCII_NBRSP == p->buf[i]) {
				vbl += (*p->width)(p, ' ');
				continue;
			}

			/*
			 * Now we definitely know there will be
			 * printable characters to output,
			 * so write preceding white space now.
			 */
			if (vbl) {
				(*p->advance)(p, vbl);
				p->viscol += vbl;
				vbl = 0;
			}

			if (ASCII_HYPH == p->buf[i]) {
				(*p->letter)(p, '-');
				p->viscol += (*p->width)(p, '-');
				continue;
			}

			(*p->letter)(p, p->buf[i]);
			if (8 == p->buf[i])
				p->viscol -= (*p->width)(p, p->buf[i-1]);
			else 
				p->viscol += (*p->width)(p, p->buf[i]);
		}
		vis = vend;
	}

	/*
	 * If there was trailing white space, it was not printed;
	 * so reset the cursor position accordingly.
	 */
	if (vis)
		vis -= vbl;

	p->col = 0;
	p->overstep = 0;

	if ( ! (TERMP_NOBREAK & p->flags)) {
		p->viscol = 0;
		(*p->endline)(p);
		return;
	}

	if (TERMP_HANG & p->flags) {
		/* We need one blank after the tag. */
		p->overstep = (int)(vis - maxvis + (*p->width)(p, ' '));

		/*
		 * Behave exactly the same way as groff:
		 * If we have overstepped the margin, temporarily move
		 * it to the right and flag the rest of the line to be
		 * shorter.
		 * If we landed right at the margin, be happy.
		 * If we are one step before the margin, temporarily
		 * move it one step LEFT and flag the rest of the line
		 * to be longer.
		 */
		if (p->overstep < -1)
			p->overstep = 0;
		return;

	} else if (TERMP_DANGLE & p->flags)
		return;

	/* If the column was overrun, break the line. */
	if (maxvis <= vis +
	    ((TERMP_TWOSPACE & p->flags) ? (*p->width)(p, ' ') : 0)) {
		(*p->endline)(p);
		p->viscol = 0;
	}
}


/* 
 * A newline only breaks an existing line; it won't assert vertical
 * space.  All data in the output buffer is flushed prior to the newline
 * assertion.
 */
void
term_newln(struct termp *p)
{

	p->flags |= TERMP_NOSPACE;
	if (p->col || p->viscol)
		term_flushln(p);
}


/*
 * Asserts a vertical space (a full, empty line-break between lines).
 * Note that if used twice, this will cause two blank spaces and so on.
 * All data in the output buffer is flushed prior to the newline
 * assertion.
 */
void
term_vspace(struct termp *p)
{

	term_newln(p);
	p->viscol = 0;
	(*p->endline)(p);
}

void
term_fontlast(struct termp *p)
{
	enum termfont	 f;

	f = p->fontl;
	p->fontl = p->fontq[p->fonti];
	p->fontq[p->fonti] = f;
}


void
term_fontrepl(struct termp *p, enum termfont f)
{

	p->fontl = p->fontq[p->fonti];
	p->fontq[p->fonti] = f;
}


void
term_fontpush(struct termp *p, enum termfont f)
{

	assert(p->fonti + 1 < 10);
	p->fontl = p->fontq[p->fonti];
	p->fontq[++p->fonti] = f;
}


const void *
term_fontq(struct termp *p)
{

	return(&p->fontq[p->fonti]);
}


enum termfont
term_fonttop(struct termp *p)
{

	return(p->fontq[p->fonti]);
}


void
term_fontpopq(struct termp *p, const void *key)
{

	while (p->fonti >= 0 && key != &p->fontq[p->fonti])
		p->fonti--;
	assert(p->fonti >= 0);
}


void
term_fontpop(struct termp *p)
{

	assert(p->fonti);
	p->fonti--;
}

/*
 * Handle pwords, partial words, which may be either a single word or a
 * phrase that cannot be broken down (such as a literal string).  This
 * handles word styling.
 */
void
term_word(struct termp *p, const char *word)
{
	const char	*seq, *cp;
	char		 c;
	int		 sz, uc;
	size_t		 ssz;
	enum mandoc_esc	 esc;

	if ( ! (TERMP_NOSPACE & p->flags)) {
		if ( ! (TERMP_KEEP & p->flags)) {
			if (TERMP_PREKEEP & p->flags)
				p->flags |= TERMP_KEEP;
			bufferc(p, ' ');
			if (TERMP_SENTENCE & p->flags)
				bufferc(p, ' ');
		} else
			bufferc(p, ASCII_NBRSP);
	}

	if ( ! (p->flags & TERMP_NONOSPACE))
		p->flags &= ~TERMP_NOSPACE;
	else
		p->flags |= TERMP_NOSPACE;

	p->flags &= ~(TERMP_SENTENCE | TERMP_IGNDELIM);

	while ('\0' != *word) {
		if ((ssz = strcspn(word, "\\")) > 0)
			encode(p, word, ssz);

		word += (int)ssz;
		if ('\\' != *word)
			continue;

		word++;
		esc = mandoc_escape(&word, &seq, &sz);
		if (ESCAPE_ERROR == esc)
			break;

		if (TERMENC_ASCII != p->enc)
			switch (esc) {
			case (ESCAPE_UNICODE):
				uc = mchars_num2uc(seq + 1, sz - 1);
				if ('\0' == uc)
					break;
				encode1(p, uc);
				continue;
			case (ESCAPE_SPECIAL):
				uc = mchars_spec2cp(p->symtab, seq, sz);
				if (uc <= 0)
					break;
				encode1(p, uc);
				continue;
			default:
				break;
			}

		switch (esc) {
		case (ESCAPE_UNICODE):
			encode1(p, '?');
			break;
		case (ESCAPE_NUMBERED):
			c = mchars_num2char(seq, sz);
			if ('\0' != c)
				encode(p, &c, 1);
			break;
		case (ESCAPE_SPECIAL):
			cp = mchars_spec2str(p->symtab, seq, sz, &ssz);
			if (NULL != cp) 
				encode(p, cp, ssz);
			else if (1 == ssz)
				encode(p, seq, sz);
			break;
		case (ESCAPE_FONTBOLD):
			term_fontrepl(p, TERMFONT_BOLD);
			break;
		case (ESCAPE_FONTITALIC):
			term_fontrepl(p, TERMFONT_UNDER);
			break;
		case (ESCAPE_FONT):
			/* FALLTHROUGH */
		case (ESCAPE_FONTROMAN):
			term_fontrepl(p, TERMFONT_NONE);
			break;
		case (ESCAPE_FONTPREV):
			term_fontlast(p);
			break;
		case (ESCAPE_NOSPACE):
			if ('\0' == *word)
				p->flags |= TERMP_NOSPACE;
			break;
		default:
			break;
		}
	}
}

static void
adjbuf(struct termp *p, int sz)
{

	if (0 == p->maxcols)
		p->maxcols = 1024;
	while (sz >= p->maxcols)
		p->maxcols <<= 2;

	p->buf = mandoc_realloc
		(p->buf, sizeof(int) * (size_t)p->maxcols);
}

static void
bufferc(struct termp *p, char c)
{

	if (p->col + 1 >= p->maxcols)
		adjbuf(p, p->col + 1);

	p->buf[p->col++] = c;
}

/*
 * See encode().
 * Do this for a single (probably unicode) value.
 * Does not check for non-decorated glyphs.
 */
static void
encode1(struct termp *p, int c)
{
	enum termfont	  f;

	if (p->col + 4 >= p->maxcols)
		adjbuf(p, p->col + 4);

	f = term_fonttop(p);

	if (TERMFONT_NONE == f) {
		p->buf[p->col++] = c;
		return;
	} else if (TERMFONT_UNDER == f) {
		p->buf[p->col++] = '_';
	} else
		p->buf[p->col++] = c;

	p->buf[p->col++] = 8;
	p->buf[p->col++] = c;
}

static void
encode(struct termp *p, const char *word, size_t sz)
{
	enum termfont	  f;
	int		  i, len;

	/* LINTED */
	len = sz;

	/*
	 * Encode and buffer a string of characters.  If the current
	 * font mode is unset, buffer directly, else encode then buffer
	 * character by character.
	 */

	if (TERMFONT_NONE == (f = term_fonttop(p))) {
		if (p->col + len >= p->maxcols) 
			adjbuf(p, p->col + len);
		for (i = 0; i < len; i++)
			p->buf[p->col++] = word[i];
		return;
	}

	/* Pre-buffer, assuming worst-case. */

	if (p->col + 1 + (len * 3) >= p->maxcols)
		adjbuf(p, p->col + 1 + (len * 3));

	for (i = 0; i < len; i++) {
		if (ASCII_HYPH != word[i] &&
		    ! isgraph((unsigned char)word[i])) {
			p->buf[p->col++] = word[i];
			continue;
		}

		if (TERMFONT_UNDER == f)
			p->buf[p->col++] = '_';
		else if (ASCII_HYPH == word[i])
			p->buf[p->col++] = '-';
		else
			p->buf[p->col++] = word[i];

		p->buf[p->col++] = 8;
		p->buf[p->col++] = word[i];
	}
}

size_t
term_len(const struct termp *p, size_t sz)
{

	return((*p->width)(p, ' ') * sz);
}


size_t
term_strlen(const struct termp *p, const char *cp)
{
	size_t		 sz, rsz, i;
	int		 ssz, c;
	const char	*seq, *rhs;
	enum mandoc_esc	 esc;
	static const char rej[] = { '\\', ASCII_HYPH, ASCII_NBRSP, '\0' };

	/*
	 * Account for escaped sequences within string length
	 * calculations.  This follows the logic in term_word() as we
	 * must calculate the width of produced strings.
	 */

	sz = 0;
	while ('\0' != *cp) {
		rsz = strcspn(cp, rej);
		for (i = 0; i < rsz; i++)
			sz += (*p->width)(p, *cp++);

		c = 0;
		switch (*cp) {
		case ('\\'):
			cp++;
			esc = mandoc_escape(&cp, &seq, &ssz);
			if (ESCAPE_ERROR == esc)
				return(sz);

			if (TERMENC_ASCII != p->enc)
				switch (esc) {
				case (ESCAPE_UNICODE):
					c = mchars_num2uc
						(seq + 1, ssz - 1);
					if ('\0' == c)
						break;
					sz += (*p->width)(p, c);
					continue;
				case (ESCAPE_SPECIAL):
					c = mchars_spec2cp
						(p->symtab, seq, ssz);
					if (c <= 0)
						break;
					sz += (*p->width)(p, c);
					continue;
				default:
					break;
				}

			rhs = NULL;

			switch (esc) {
			case (ESCAPE_UNICODE):
				sz += (*p->width)(p, '?');
				break;
			case (ESCAPE_NUMBERED):
				c = mchars_num2char(seq, ssz);
				if ('\0' != c)
					sz += (*p->width)(p, c);
				break;
			case (ESCAPE_SPECIAL):
				rhs = mchars_spec2str
					(p->symtab, seq, ssz, &rsz);

				if (ssz != 1 || rhs)
					break;

				rhs = seq;
				rsz = ssz;
				break;
			default:
				break;
			}

			if (NULL == rhs)
				break;

			for (i = 0; i < rsz; i++)
				sz += (*p->width)(p, *rhs++);
			break;
		case (ASCII_NBRSP):
			sz += (*p->width)(p, ' ');
			cp++;
			break;
		case (ASCII_HYPH):
			sz += (*p->width)(p, '-');
			cp++;
			break;
		default:
			break;
		}
	}

	return(sz);
}

/* ARGSUSED */
size_t
term_vspan(const struct termp *p, const struct roffsu *su)
{
	double		 r;

	switch (su->unit) {
	case (SCALE_CM):
		r = su->scale * 2;
		break;
	case (SCALE_IN):
		r = su->scale * 6;
		break;
	case (SCALE_PC):
		r = su->scale;
		break;
	case (SCALE_PT):
		r = su->scale / 8;
		break;
	case (SCALE_MM):
		r = su->scale / 1000;
		break;
	case (SCALE_VS):
		r = su->scale;
		break;
	default:
		r = su->scale - 1;
		break;
	}

	if (r < 0.0)
		r = 0.0;
	return(/* LINTED */(size_t)
			r);
}

size_t
term_hspan(const struct termp *p, const struct roffsu *su)
{
	double		 v;

	v = ((*p->hspan)(p, su));
	if (v < 0.0)
		v = 0.0;
	return((size_t) /* LINTED */
			v);
}
