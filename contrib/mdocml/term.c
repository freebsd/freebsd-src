/*	$Id: term.c,v 1.256 2016/01/07 21:03:54 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010-2015 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "out.h"
#include "term.h"
#include "main.h"

static	size_t		 cond_width(const struct termp *, int, int *);
static	void		 adjbuf(struct termp *p, size_t);
static	void		 bufferc(struct termp *, char);
static	void		 encode(struct termp *, const char *, size_t);
static	void		 encode1(struct termp *, int);


void
term_free(struct termp *p)
{

	free(p->buf);
	free(p->fontq);
	free(p);
}

void
term_begin(struct termp *p, term_margin head,
		term_margin foot, const struct roff_meta *arg)
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
 * Flush a chunk of text.  By default, break the output line each time
 * the right margin is reached, and continue output on the next line
 * at the same offset as the chunk itself.  By default, also break the
 * output line at the end of the chunk.
 * The following flags may be specified:
 *
 *  - TERMP_NOBREAK: Do not break the output line at the right margin,
 *    but only at the max right margin.  Also, do not break the output
 *    line at the end of the chunk, such that the next call can pad to
 *    the next column.  However, if less than p->trailspace blanks,
 *    which can be 0, 1, or 2, remain to the right margin, the line
 *    will be broken.
 *  - TERMP_BRTRSP: Consider trailing whitespace significant
 *    when deciding whether the chunk fits or not.
 *  - TERMP_BRIND: If the chunk does not fit and the output line has
 *    to be broken, start the next line at the right margin instead
 *    of at the offset.  Used together with TERMP_NOBREAK for the tags
 *    in various kinds of tagged lists.
 *  - TERMP_DANGLE: Do not break the output line at the right margin,
 *    append the next chunk after it even if this one is too long.
 *    To be used together with TERMP_NOBREAK.
 *  - TERMP_HANG: Like TERMP_DANGLE, and also suppress padding before
 *    the next chunk if this column is not full.
 */
void
term_flushln(struct termp *p)
{
	size_t		 i;     /* current input position in p->buf */
	int		 ntab;	/* number of tabs to prepend */
	size_t		 vis;   /* current visual position on output */
	size_t		 vbl;   /* number of blanks to prepend to output */
	size_t		 vend;	/* end of word visual position on output */
	size_t		 bp;    /* visual right border position */
	size_t		 dv;    /* temporary for visual pos calculations */
	size_t		 j;     /* temporary loop index for p->buf */
	size_t		 jhy;	/* last hyph before overflow w/r/t j */
	size_t		 maxvis; /* output position of visible boundary */

	/*
	 * First, establish the maximum columns of "visible" content.
	 * This is usually the difference between the right-margin and
	 * an indentation, but can be, for tagged lists or columns, a
	 * small set of values.
	 *
	 * The following unsigned-signed subtractions look strange,
	 * but they are actually correct.  If the int p->overstep
	 * is negative, it gets sign extended.  Subtracting that
	 * very large size_t effectively adds a small number to dv.
	 */
	dv = p->rmargin > p->offset ? p->rmargin - p->offset : 0;
	maxvis = (int)dv > p->overstep ? dv - (size_t)p->overstep : 0;

	if (p->flags & TERMP_NOBREAK) {
		dv = p->maxrmargin > p->offset ?
		     p->maxrmargin - p->offset : 0;
		bp = (int)dv > p->overstep ?
		     dv - (size_t)p->overstep : 0;
	} else
		bp = maxvis;

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
		ntab = 0;
		while (i < p->col && '\t' == p->buf[i]) {
			vend = (vis / p->tabwidth + 1) * p->tabwidth;
			vbl += vend - vis;
			vis = vend;
			ntab++;
			i++;
		}

		/*
		 * Count up visible word characters.  Control sequences
		 * (starting with the CSI) aren't counted.  A space
		 * generates a non-printing word, which is valid (the
		 * space is printed according to regular spacing rules).
		 */

		for (j = i, jhy = 0; j < p->col; j++) {
			if (' ' == p->buf[j] || '\t' == p->buf[j])
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
			    (ASCII_HYPH == p->buf[j] ||
			     ASCII_BREAK == p->buf[j]))
				jhy = j;

			/*
			 * Hyphenation now decided, put back a real
			 * hyphen such that we get the correct width.
			 */
			if (ASCII_HYPH == p->buf[j])
				p->buf[j] = '-';

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
			if (TERMP_BRIND & p->flags) {
				vbl = p->rmargin;
				vend += p->rmargin;
				vend -= p->offset;
			} else
				vbl = p->offset;

			/* use pending tabs on the new line */

			if (0 < ntab)
				vbl += ntab * p->tabwidth;

			/*
			 * Remove the p->overstep width.
			 * Again, if p->overstep is negative,
			 * sign extension does the right thing.
			 */

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
				while (i < p->col && ' ' == p->buf[i])
					i++;
				dv = (i - j) * (*p->width)(p, ' ');
				vbl += dv;
				vend += dv;
				break;
			}
			if (ASCII_NBRSP == p->buf[i]) {
				vbl += (*p->width)(p, ' ');
				continue;
			}
			if (ASCII_BREAK == p->buf[i])
				continue;

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
	if (vis > vbl)
		vis -= vbl;
	else
		vis = 0;

	p->col = 0;
	p->overstep = 0;
	p->flags &= ~(TERMP_BACKAFTER | TERMP_BACKBEFORE);

	if ( ! (TERMP_NOBREAK & p->flags)) {
		p->viscol = 0;
		(*p->endline)(p);
		return;
	}

	if (TERMP_HANG & p->flags) {
		p->overstep += (int)(p->offset + vis - p->rmargin +
		    p->trailspace * (*p->width)(p, ' '));

		/*
		 * If we have overstepped the margin, temporarily move
		 * it to the right and flag the rest of the line to be
		 * shorter.
		 * If there is a request to keep the columns together,
		 * allow negative overstep when the column is not full.
		 */
		if (p->trailspace && p->overstep < 0)
			p->overstep = 0;
		return;

	} else if (TERMP_DANGLE & p->flags)
		return;

	/* Trailing whitespace is significant in some columns. */
	if (vis && vbl && (TERMP_BRTRSP & p->flags))
		vis += vbl;

	/* If the column was overrun, break the line. */
	if (maxvis < vis + p->trailspace * (*p->width)(p, ' ')) {
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
	if (0 < p->skipvsp)
		p->skipvsp--;
	else
		(*p->endline)(p);
}

/* Swap current and previous font; for \fP and .ft P */
void
term_fontlast(struct termp *p)
{
	enum termfont	 f;

	f = p->fontl;
	p->fontl = p->fontq[p->fonti];
	p->fontq[p->fonti] = f;
}

/* Set font, save current, discard previous; for \f, .ft, .B etc. */
void
term_fontrepl(struct termp *p, enum termfont f)
{

	p->fontl = p->fontq[p->fonti];
	p->fontq[p->fonti] = f;
}

/* Set font, save previous. */
void
term_fontpush(struct termp *p, enum termfont f)
{

	p->fontl = p->fontq[p->fonti];
	if (++p->fonti == p->fontsz) {
		p->fontsz += 8;
		p->fontq = mandoc_reallocarray(p->fontq,
		    p->fontsz, sizeof(*p->fontq));
	}
	p->fontq[p->fonti] = f;
}

/* Flush to make the saved pointer current again. */
void
term_fontpopq(struct termp *p, int i)
{

	assert(i >= 0);
	if (p->fonti > i)
		p->fonti = i;
}

/* Pop one font off the stack. */
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
	const char	 nbrsp[2] = { ASCII_NBRSP, 0 };
	const char	*seq, *cp;
	int		 sz, uc;
	size_t		 ssz;
	enum mandoc_esc	 esc;

	if ( ! (TERMP_NOSPACE & p->flags)) {
		if ( ! (TERMP_KEEP & p->flags)) {
			bufferc(p, ' ');
			if (TERMP_SENTENCE & p->flags)
				bufferc(p, ' ');
		} else
			bufferc(p, ASCII_NBRSP);
	}
	if (TERMP_PREKEEP & p->flags)
		p->flags |= TERMP_KEEP;

	if ( ! (p->flags & TERMP_NONOSPACE))
		p->flags &= ~TERMP_NOSPACE;
	else
		p->flags |= TERMP_NOSPACE;

	p->flags &= ~(TERMP_SENTENCE | TERMP_NONEWLINE);
	p->skipvsp = 0;

	while ('\0' != *word) {
		if ('\\' != *word) {
			if (TERMP_NBRWORD & p->flags) {
				if (' ' == *word) {
					encode(p, nbrsp, 1);
					word++;
					continue;
				}
				ssz = strcspn(word, "\\ ");
			} else
				ssz = strcspn(word, "\\");
			encode(p, word, ssz);
			word += (int)ssz;
			continue;
		}

		word++;
		esc = mandoc_escape(&word, &seq, &sz);
		if (ESCAPE_ERROR == esc)
			continue;

		switch (esc) {
		case ESCAPE_UNICODE:
			uc = mchars_num2uc(seq + 1, sz - 1);
			break;
		case ESCAPE_NUMBERED:
			uc = mchars_num2char(seq, sz);
			if (uc < 0)
				continue;
			break;
		case ESCAPE_SPECIAL:
			if (p->enc == TERMENC_ASCII) {
				cp = mchars_spec2str(seq, sz, &ssz);
				if (cp != NULL)
					encode(p, cp, ssz);
			} else {
				uc = mchars_spec2cp(seq, sz);
				if (uc > 0)
					encode1(p, uc);
			}
			continue;
		case ESCAPE_FONTBOLD:
			term_fontrepl(p, TERMFONT_BOLD);
			continue;
		case ESCAPE_FONTITALIC:
			term_fontrepl(p, TERMFONT_UNDER);
			continue;
		case ESCAPE_FONTBI:
			term_fontrepl(p, TERMFONT_BI);
			continue;
		case ESCAPE_FONT:
		case ESCAPE_FONTROMAN:
			term_fontrepl(p, TERMFONT_NONE);
			continue;
		case ESCAPE_FONTPREV:
			term_fontlast(p);
			continue;
		case ESCAPE_NOSPACE:
			if (p->flags & TERMP_BACKAFTER)
				p->flags &= ~TERMP_BACKAFTER;
			else if (*word == '\0')
				p->flags |= (TERMP_NOSPACE | TERMP_NONEWLINE);
			continue;
		case ESCAPE_SKIPCHAR:
			p->flags |= TERMP_BACKAFTER;
			continue;
		case ESCAPE_OVERSTRIKE:
			cp = seq + sz;
			while (seq < cp) {
				if (*seq == '\\') {
					mandoc_escape(&seq, NULL, NULL);
					continue;
				}
				encode1(p, *seq++);
				if (seq < cp) {
					if (p->flags & TERMP_BACKBEFORE)
						p->flags |= TERMP_BACKAFTER;
					else
						p->flags |= TERMP_BACKBEFORE;
				}
			}
			/* Trim trailing backspace/blank pair. */
			if (p->col > 2 && p->buf[p->col - 1] == ' ')
				p->col -= 2;
			continue;
		default:
			continue;
		}

		/*
		 * Common handling for Unicode and numbered
		 * character escape sequences.
		 */

		if (p->enc == TERMENC_ASCII) {
			cp = ascii_uc2str(uc);
			encode(p, cp, strlen(cp));
		} else {
			if ((uc < 0x20 && uc != 0x09) ||
			    (uc > 0x7E && uc < 0xA0))
				uc = 0xFFFD;
			encode1(p, uc);
		}
	}
	p->flags &= ~TERMP_NBRWORD;
}

static void
adjbuf(struct termp *p, size_t sz)
{

	if (0 == p->maxcols)
		p->maxcols = 1024;
	while (sz >= p->maxcols)
		p->maxcols <<= 2;

	p->buf = mandoc_reallocarray(p->buf, p->maxcols, sizeof(int));
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

	if (p->col + 7 >= p->maxcols)
		adjbuf(p, p->col + 7);

	f = (c == ASCII_HYPH || c > 127 || isgraph(c)) ?
	    p->fontq[p->fonti] : TERMFONT_NONE;

	if (p->flags & TERMP_BACKBEFORE) {
		if (p->buf[p->col - 1] == ' ')
			p->col--;
		else
			p->buf[p->col++] = 8;
		p->flags &= ~TERMP_BACKBEFORE;
	}
	if (TERMFONT_UNDER == f || TERMFONT_BI == f) {
		p->buf[p->col++] = '_';
		p->buf[p->col++] = 8;
	}
	if (TERMFONT_BOLD == f || TERMFONT_BI == f) {
		if (ASCII_HYPH == c)
			p->buf[p->col++] = '-';
		else
			p->buf[p->col++] = c;
		p->buf[p->col++] = 8;
	}
	p->buf[p->col++] = c;
	if (p->flags & TERMP_BACKAFTER) {
		p->flags |= TERMP_BACKBEFORE;
		p->flags &= ~TERMP_BACKAFTER;
	}
}

static void
encode(struct termp *p, const char *word, size_t sz)
{
	size_t		  i;

	if (p->col + 2 + (sz * 5) >= p->maxcols)
		adjbuf(p, p->col + 2 + (sz * 5));

	for (i = 0; i < sz; i++) {
		if (ASCII_HYPH == word[i] ||
		    isgraph((unsigned char)word[i]))
			encode1(p, word[i]);
		else
			p->buf[p->col++] = word[i];
	}
}

void
term_setwidth(struct termp *p, const char *wstr)
{
	struct roffsu	 su;
	int		 iop, width;

	iop = 0;
	width = 0;
	if (NULL != wstr) {
		switch (*wstr) {
		case '+':
			iop = 1;
			wstr++;
			break;
		case '-':
			iop = -1;
			wstr++;
			break;
		default:
			break;
		}
		if (a2roffsu(wstr, &su, SCALE_MAX))
			width = term_hspan(p, &su);
		else
			iop = 0;
	}
	(*p->setwidth)(p, iop, width);
}

size_t
term_len(const struct termp *p, size_t sz)
{

	return (*p->width)(p, ' ') * sz;
}

static size_t
cond_width(const struct termp *p, int c, int *skip)
{

	if (*skip) {
		(*skip) = 0;
		return 0;
	} else
		return (*p->width)(p, c);
}

size_t
term_strlen(const struct termp *p, const char *cp)
{
	size_t		 sz, rsz, i;
	int		 ssz, skip, uc;
	const char	*seq, *rhs;
	enum mandoc_esc	 esc;
	static const char rej[] = { '\\', ASCII_NBRSP, ASCII_HYPH,
			ASCII_BREAK, '\0' };

	/*
	 * Account for escaped sequences within string length
	 * calculations.  This follows the logic in term_word() as we
	 * must calculate the width of produced strings.
	 */

	sz = 0;
	skip = 0;
	while ('\0' != *cp) {
		rsz = strcspn(cp, rej);
		for (i = 0; i < rsz; i++)
			sz += cond_width(p, *cp++, &skip);

		switch (*cp) {
		case '\\':
			cp++;
			esc = mandoc_escape(&cp, &seq, &ssz);
			if (ESCAPE_ERROR == esc)
				continue;

			rhs = NULL;

			switch (esc) {
			case ESCAPE_UNICODE:
				uc = mchars_num2uc(seq + 1, ssz - 1);
				break;
			case ESCAPE_NUMBERED:
				uc = mchars_num2char(seq, ssz);
				if (uc < 0)
					continue;
				break;
			case ESCAPE_SPECIAL:
				if (p->enc == TERMENC_ASCII) {
					rhs = mchars_spec2str(seq, ssz, &rsz);
					if (rhs != NULL)
						break;
				} else {
					uc = mchars_spec2cp(seq, ssz);
					if (uc > 0)
						sz += cond_width(p, uc, &skip);
				}
				continue;
			case ESCAPE_SKIPCHAR:
				skip = 1;
				continue;
			case ESCAPE_OVERSTRIKE:
				rsz = 0;
				rhs = seq + ssz;
				while (seq < rhs) {
					if (*seq == '\\') {
						mandoc_escape(&seq, NULL, NULL);
						continue;
					}
					i = (*p->width)(p, *seq++);
					if (rsz < i)
						rsz = i;
				}
				sz += rsz;
				continue;
			default:
				continue;
			}

			/*
			 * Common handling for Unicode and numbered
			 * character escape sequences.
			 */

			if (rhs == NULL) {
				if (p->enc == TERMENC_ASCII) {
					rhs = ascii_uc2str(uc);
					rsz = strlen(rhs);
				} else {
					if ((uc < 0x20 && uc != 0x09) ||
					    (uc > 0x7E && uc < 0xA0))
						uc = 0xFFFD;
					sz += cond_width(p, uc, &skip);
					continue;
				}
			}

			if (skip) {
				skip = 0;
				break;
			}

			/*
			 * Common handling for all escape sequences
			 * printing more than one character.
			 */

			for (i = 0; i < rsz; i++)
				sz += (*p->width)(p, *rhs++);
			break;
		case ASCII_NBRSP:
			sz += cond_width(p, ' ', &skip);
			cp++;
			break;
		case ASCII_HYPH:
			sz += cond_width(p, '-', &skip);
			cp++;
			break;
		default:
			break;
		}
	}

	return sz;
}

int
term_vspan(const struct termp *p, const struct roffsu *su)
{
	double		 r;
	int		 ri;

	switch (su->unit) {
	case SCALE_BU:
		r = su->scale / 40.0;
		break;
	case SCALE_CM:
		r = su->scale * 6.0 / 2.54;
		break;
	case SCALE_FS:
		r = su->scale * 65536.0 / 40.0;
		break;
	case SCALE_IN:
		r = su->scale * 6.0;
		break;
	case SCALE_MM:
		r = su->scale * 0.006;
		break;
	case SCALE_PC:
		r = su->scale;
		break;
	case SCALE_PT:
		r = su->scale / 12.0;
		break;
	case SCALE_EN:
	case SCALE_EM:
		r = su->scale * 0.6;
		break;
	case SCALE_VS:
		r = su->scale;
		break;
	default:
		abort();
	}
	ri = r > 0.0 ? r + 0.4995 : r - 0.4995;
	return ri < 66 ? ri : 1;
}

/*
 * Convert a scaling width to basic units, rounding down.
 */
int
term_hspan(const struct termp *p, const struct roffsu *su)
{

	return (*p->hspan)(p, su);
}
