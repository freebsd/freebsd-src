/*	$Id: term.c,v 1.281 2019/06/03 20:23:41 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010-2019 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "out.h"
#include "term.h"
#include "main.h"

static	size_t		 cond_width(const struct termp *, int, int *);
static	void		 adjbuf(struct termp_col *, size_t);
static	void		 bufferc(struct termp *, char);
static	void		 encode(struct termp *, const char *, size_t);
static	void		 encode1(struct termp *, int);
static	void		 endline(struct termp *);
static	void		 term_field(struct termp *, size_t, size_t,
				size_t, size_t);
static	void		 term_fill(struct termp *, size_t *, size_t *,
				size_t);


void
term_setcol(struct termp *p, size_t maxtcol)
{
	if (maxtcol > p->maxtcol) {
		p->tcols = mandoc_recallocarray(p->tcols,
		    p->maxtcol, maxtcol, sizeof(*p->tcols));
		p->maxtcol = maxtcol;
	}
	p->lasttcol = maxtcol - 1;
	p->tcol = p->tcols;
}

void
term_free(struct termp *p)
{
	for (p->tcol = p->tcols; p->tcol < p->tcols + p->maxtcol; p->tcol++)
		free(p->tcol->buf);
	free(p->tcols);
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
 * output line at the end of the chunk.  There are many flags modifying
 * this behaviour, see the comments in the body of the function.
 */
void
term_flushln(struct termp *p)
{
	size_t	 vbl;      /* Number of blanks to prepend to the output. */
	size_t	 vbr;      /* Actual visual position of the end of field. */
	size_t	 vfield;   /* Desired visual field width. */
	size_t	 vtarget;  /* Desired visual position of the right margin. */
	size_t	 ic;       /* Character position in the input buffer. */
	size_t	 nbr;      /* Number of characters to print in this field. */

	/*
	 * Normally, start writing at the left margin, but with the
	 * NOPAD flag, start writing at the current position instead.
	 */

	vbl = (p->flags & TERMP_NOPAD) || p->tcol->offset < p->viscol ?
	    0 : p->tcol->offset - p->viscol;
	if (p->minbl && vbl < p->minbl)
		vbl = p->minbl;

	if ((p->flags & TERMP_MULTICOL) == 0)
		p->tcol->col = 0;

	/* Loop over output lines. */

	for (;;) {
		vfield = p->tcol->rmargin > p->viscol + vbl ?
		    p->tcol->rmargin - p->viscol - vbl : 0;

		/*
		 * Normally, break the line at the the right margin
		 * of the field, but with the NOBREAK flag, only
		 * break it at the max right margin of the screen,
		 * and with the BRNEVER flag, never break it at all.
		 */

		vtarget = p->flags & TERMP_BRNEVER ? SIZE_MAX :
		    (p->flags & TERMP_NOBREAK) == 0 ? vfield :
		    p->maxrmargin > p->viscol + vbl ?
		    p->maxrmargin - p->viscol - vbl : 0;

		/*
		 * Figure out how much text will fit in the field.
		 * If there is whitespace only, print nothing.
		 */

		term_fill(p, &nbr, &vbr, vtarget);
		if (nbr == 0)
			break;

		/*
		 * With the CENTER or RIGHT flag, increase the indentation
		 * to center the text between the left and right margins
		 * or to adjust it to the right margin, respectively.
		 */

		if (vbr < vtarget) {
			if (p->flags & TERMP_CENTER)
				vbl += (vtarget - vbr) / 2;
			else if (p->flags & TERMP_RIGHT)
				vbl += vtarget - vbr;
		}

		/* Finally, print the field content. */

		term_field(p, vbl, nbr, vbr, vtarget);

		/*
		 * If there is no text left in the field, exit the loop.
		 * If the BRTRSP flag is set, consider trailing
		 * whitespace significant when deciding whether
		 * the field fits or not.
		 */

		for (ic = p->tcol->col; ic < p->tcol->lastcol; ic++) {
			switch (p->tcol->buf[ic]) {
			case '\t':
				if (p->flags & TERMP_BRTRSP)
					vbr = term_tab_next(vbr);
				continue;
			case ' ':
				if (p->flags & TERMP_BRTRSP)
					vbr += (*p->width)(p, ' ');
				continue;
			case '\n':
			case ASCII_BREAK:
				continue;
			default:
				break;
			}
			break;
		}
		if (ic == p->tcol->lastcol)
			break;

		/*
		 * At the location of an automtic line break, input
		 * space characters are consumed by the line break.
		 */

		while (p->tcol->col < p->tcol->lastcol &&
		    p->tcol->buf[p->tcol->col] == ' ')
			p->tcol->col++;

		/*
		 * In multi-column mode, leave the rest of the text
		 * in the buffer to be handled by a subsequent
		 * invocation, such that the other columns of the
		 * table can be handled first.
		 * In single-column mode, simply break the line.
		 */

		if (p->flags & TERMP_MULTICOL)
			return;

		endline(p);
		p->viscol = 0;

		/*
		 * Normally, start the next line at the same indentation
		 * as this one, but with the BRIND flag, start it at the
		 * right margin instead.  This is used together with
		 * NOBREAK for the tags in various kinds of tagged lists.
		 */

		vbl = p->flags & TERMP_BRIND ?
		    p->tcol->rmargin : p->tcol->offset;
	}

	/* Reset output state in preparation for the next field. */

	p->col = p->tcol->col = p->tcol->lastcol = 0;
	p->minbl = p->trailspace;
	p->flags &= ~(TERMP_BACKAFTER | TERMP_BACKBEFORE | TERMP_NOPAD);

	if (p->flags & TERMP_MULTICOL)
		return;

	/*
	 * The HANG flag means that the next field
	 * always follows on the same line.
	 * The NOBREAK flag means that the next field
	 * follows on the same line unless the field was overrun.
	 * Normally, break the line at the end of each field.
	 */

	if ((p->flags & TERMP_HANG) == 0 &&
	    ((p->flags & TERMP_NOBREAK) == 0 ||
	     vbr + term_len(p, p->trailspace) > vfield))
		endline(p);
}

/*
 * Store the number of input characters to print in this field in *nbr
 * and their total visual width to print in *vbr.
 * If there is only whitespace in the field, both remain zero.
 * The desired visual width of the field is provided by vtarget.
 * If the first word is longer, the field will be overrun.
 */
static void
term_fill(struct termp *p, size_t *nbr, size_t *vbr, size_t vtarget)
{
	size_t	 ic;        /* Character position in the input buffer. */
	size_t	 vis;       /* Visual position of the current character. */
	size_t	 vn;        /* Visual position of the next character. */
	int	 breakline; /* Break at the end of this word. */
	int	 graph;     /* Last character was non-blank. */

	*nbr = *vbr = vis = 0;
	breakline = graph = 0;
	for (ic = p->tcol->col; ic < p->tcol->lastcol; ic++) {
		switch (p->tcol->buf[ic]) {
		case '\b':  /* Escape \o (overstrike) or backspace markup. */
			assert(ic > 0);
			vis -= (*p->width)(p, p->tcol->buf[ic - 1]);
			continue;

		case '\t':  /* Normal ASCII whitespace. */
		case ' ':
		case ASCII_BREAK:  /* Escape \: (breakpoint). */
			switch (p->tcol->buf[ic]) {
			case '\t':
				vn = term_tab_next(vis);
				break;
			case ' ':
				vn = vis + (*p->width)(p, ' ');
				break;
			case ASCII_BREAK:
				vn = vis;
				break;
			default:
				abort();
			}
			/* Can break at the end of a word. */
			if (breakline || vn > vtarget)
				break;
			if (graph) {
				*nbr = ic;
				*vbr = vis;
				graph = 0;
			}
			vis = vn;
			continue;

		case '\n':  /* Escape \p (break at the end of the word). */
			breakline = 1;
			continue;

		case ASCII_HYPH:  /* Breakable hyphen. */
			graph = 1;
			/*
			 * We are about to decide whether to break the
			 * line or not, so we no longer need this hyphen
			 * to be marked as breakable.  Put back a real
			 * hyphen such that we get the correct width.
			 */
			p->tcol->buf[ic] = '-';
			vis += (*p->width)(p, '-');
			if (vis > vtarget) {
				ic++;
				break;
			}
			*nbr = ic + 1;
			*vbr = vis;
			continue;

		case ASCII_NBRSP:  /* Non-breakable space. */
			p->tcol->buf[ic] = ' ';
			/* FALLTHROUGH */
		default:  /* Printable character. */
			graph = 1;
			vis += (*p->width)(p, p->tcol->buf[ic]);
			if (vis > vtarget && *nbr > 0)
				return;
			continue;
		}
		break;
	}

	/*
	 * If the last word extends to the end of the field without any
	 * trailing whitespace, the loop could not check yet whether it
	 * can remain on this line.  So do the check now.
	 */

	if (graph && (vis <= vtarget || *nbr == 0)) {
		*nbr = ic;
		*vbr = vis;
	}
}

/*
 * Print the contents of one field
 * with an indentation of	 vbl	  visual columns,
 * an input string length of	 nbr	  characters,
 * an output width of		 vbr	  visual columns,
 * and a desired field width of	 vtarget  visual columns.
 */
static void
term_field(struct termp *p, size_t vbl, size_t nbr, size_t vbr, size_t vtarget)
{
	size_t	 ic;	/* Character position in the input buffer. */
	size_t	 vis;	/* Visual position of the current character. */
	size_t	 dv;	/* Visual width of the current character. */
	size_t	 vn;	/* Visual position of the next character. */

	vis = 0;
	for (ic = p->tcol->col; ic < nbr; ic++) {

		/*
		 * To avoid the printing of trailing whitespace,
		 * do not print whitespace right away, only count it.
		 */

		switch (p->tcol->buf[ic]) {
		case '\n':
		case ASCII_BREAK:
			continue;
		case '\t':
			vn = term_tab_next(vis);
			vbl += vn - vis;
			vis = vn;
			continue;
		case ' ':
		case ASCII_NBRSP:
			dv = (*p->width)(p, ' ');
			vbl += dv;
			vis += dv;
			continue;
		default:
			break;
		}

		/*
		 * We found a non-blank character to print,
		 * so write preceding white space now.
		 */

		if (vbl > 0) {
			(*p->advance)(p, vbl);
			p->viscol += vbl;
			vbl = 0;
		}

		/* Print the character and adjust the visual position. */

		(*p->letter)(p, p->tcol->buf[ic]);
		if (p->tcol->buf[ic] == '\b') {
			dv = (*p->width)(p, p->tcol->buf[ic - 1]);
			p->viscol -= dv;
			vis -= dv;
		} else {
			dv = (*p->width)(p, p->tcol->buf[ic]);
			p->viscol += dv;
			vis += dv;
		}
	}
	p->tcol->col = nbr;
}

static void
endline(struct termp *p)
{
	if ((p->flags & (TERMP_NEWMC | TERMP_ENDMC)) == TERMP_ENDMC) {
		p->mc = NULL;
		p->flags &= ~TERMP_ENDMC;
	}
	if (p->mc != NULL) {
		if (p->viscol && p->maxrmargin >= p->viscol)
			(*p->advance)(p, p->maxrmargin - p->viscol + 1);
		p->flags |= TERMP_NOBUF | TERMP_NOSPACE;
		term_word(p, p->mc);
		p->flags &= ~(TERMP_NOBUF | TERMP_NEWMC);
	}
	p->viscol = 0;
	p->minbl = 0;
	(*p->endline)(p);
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
	if (p->tcol->lastcol || p->viscol)
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
	p->minbl = 0;
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
	struct roffsu	 su;
	const char	 nbrsp[2] = { ASCII_NBRSP, 0 };
	const char	*seq, *cp;
	int		 sz, uc;
	size_t		 csz, lsz, ssz;
	enum mandoc_esc	 esc;

	if ((p->flags & TERMP_NOBUF) == 0) {
		if ((p->flags & TERMP_NOSPACE) == 0) {
			if ((p->flags & TERMP_KEEP) == 0) {
				bufferc(p, ' ');
				if (p->flags & TERMP_SENTENCE)
					bufferc(p, ' ');
			} else
				bufferc(p, ASCII_NBRSP);
		}
		if (p->flags & TERMP_PREKEEP)
			p->flags |= TERMP_KEEP;
		if (p->flags & TERMP_NONOSPACE)
			p->flags |= TERMP_NOSPACE;
		else
			p->flags &= ~TERMP_NOSPACE;
		p->flags &= ~(TERMP_SENTENCE | TERMP_NONEWLINE);
		p->skipvsp = 0;
	}

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
		case ESCAPE_UNDEF:
			uc = *seq;
			break;
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
		case ESCAPE_FONTCW:
		case ESCAPE_FONTROMAN:
			term_fontrepl(p, TERMFONT_NONE);
			continue;
		case ESCAPE_FONTPREV:
			term_fontlast(p);
			continue;
		case ESCAPE_BREAK:
			bufferc(p, '\n');
			continue;
		case ESCAPE_NOSPACE:
			if (p->flags & TERMP_BACKAFTER)
				p->flags &= ~TERMP_BACKAFTER;
			else if (*word == '\0')
				p->flags |= (TERMP_NOSPACE | TERMP_NONEWLINE);
			continue;
		case ESCAPE_DEVICE:
			if (p->type == TERMTYPE_PDF)
				encode(p, "pdf", 3);
			else if (p->type == TERMTYPE_PS)
				encode(p, "ps", 2);
			else if (p->enc == TERMENC_ASCII)
				encode(p, "ascii", 5);
			else
				encode(p, "utf8", 4);
			continue;
		case ESCAPE_HORIZ:
			if (*seq == '|') {
				seq++;
				uc = -p->col;
			} else
				uc = 0;
			if (a2roffsu(seq, &su, SCALE_EM) == NULL)
				continue;
			uc += term_hen(p, &su);
			if (uc > 0)
				while (uc-- > 0)
					bufferc(p, ASCII_NBRSP);
			else if (p->col > (size_t)(-uc))
				p->col += uc;
			else {
				uc += p->col;
				p->col = 0;
				if (p->tcol->offset > (size_t)(-uc)) {
					p->ti += uc;
					p->tcol->offset += uc;
				} else {
					p->ti -= p->tcol->offset;
					p->tcol->offset = 0;
				}
			}
			continue;
		case ESCAPE_HLINE:
			if ((cp = a2roffsu(seq, &su, SCALE_EM)) == NULL)
				continue;
			uc = term_hen(p, &su);
			if (uc <= 0) {
				if (p->tcol->rmargin <= p->tcol->offset)
					continue;
				lsz = p->tcol->rmargin - p->tcol->offset;
			} else
				lsz = uc;
			if (*cp == seq[-1])
				uc = -1;
			else if (*cp == '\\') {
				seq = cp + 1;
				esc = mandoc_escape(&seq, &cp, &sz);
				switch (esc) {
				case ESCAPE_UNICODE:
					uc = mchars_num2uc(cp + 1, sz - 1);
					break;
				case ESCAPE_NUMBERED:
					uc = mchars_num2char(cp, sz);
					break;
				case ESCAPE_SPECIAL:
					uc = mchars_spec2cp(cp, sz);
					break;
				case ESCAPE_UNDEF:
					uc = *seq;
					break;
				default:
					uc = -1;
					break;
				}
			} else
				uc = *cp;
			if (uc < 0x20 || (uc > 0x7E && uc < 0xA0))
				uc = '_';
			if (p->enc == TERMENC_ASCII) {
				cp = ascii_uc2str(uc);
				csz = term_strlen(p, cp);
				ssz = strlen(cp);
			} else
				csz = (*p->width)(p, uc);
			while (lsz >= csz) {
				if (p->enc == TERMENC_ASCII)
					encode(p, cp, ssz);
				else
					encode1(p, uc);
				lsz -= csz;
			}
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
			if (p->tcol->lastcol > 2 &&
			    (p->tcol->buf[p->tcol->lastcol - 1] == ' ' ||
			     p->tcol->buf[p->tcol->lastcol - 1] == '\t'))
				p->tcol->lastcol -= 2;
			if (p->col > p->tcol->lastcol)
				p->col = p->tcol->lastcol;
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
adjbuf(struct termp_col *c, size_t sz)
{
	if (c->maxcols == 0)
		c->maxcols = 1024;
	while (c->maxcols <= sz)
		c->maxcols <<= 2;
	c->buf = mandoc_reallocarray(c->buf, c->maxcols, sizeof(*c->buf));
}

static void
bufferc(struct termp *p, char c)
{
	if (p->flags & TERMP_NOBUF) {
		(*p->letter)(p, c);
		return;
	}
	if (p->col + 1 >= p->tcol->maxcols)
		adjbuf(p->tcol, p->col + 1);
	if (p->tcol->lastcol <= p->col || (c != ' ' && c != ASCII_NBRSP))
		p->tcol->buf[p->col] = c;
	if (p->tcol->lastcol < ++p->col)
		p->tcol->lastcol = p->col;
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

	if (p->flags & TERMP_NOBUF) {
		(*p->letter)(p, c);
		return;
	}

	if (p->col + 7 >= p->tcol->maxcols)
		adjbuf(p->tcol, p->col + 7);

	f = (c == ASCII_HYPH || c > 127 || isgraph(c)) ?
	    p->fontq[p->fonti] : TERMFONT_NONE;

	if (p->flags & TERMP_BACKBEFORE) {
		if (p->tcol->buf[p->col - 1] == ' ' ||
		    p->tcol->buf[p->col - 1] == '\t')
			p->col--;
		else
			p->tcol->buf[p->col++] = '\b';
		p->flags &= ~TERMP_BACKBEFORE;
	}
	if (f == TERMFONT_UNDER || f == TERMFONT_BI) {
		p->tcol->buf[p->col++] = '_';
		p->tcol->buf[p->col++] = '\b';
	}
	if (f == TERMFONT_BOLD || f == TERMFONT_BI) {
		if (c == ASCII_HYPH)
			p->tcol->buf[p->col++] = '-';
		else
			p->tcol->buf[p->col++] = c;
		p->tcol->buf[p->col++] = '\b';
	}
	if (p->tcol->lastcol <= p->col || (c != ' ' && c != ASCII_NBRSP))
		p->tcol->buf[p->col] = c;
	if (p->tcol->lastcol < ++p->col)
		p->tcol->lastcol = p->col;
	if (p->flags & TERMP_BACKAFTER) {
		p->flags |= TERMP_BACKBEFORE;
		p->flags &= ~TERMP_BACKAFTER;
	}
}

static void
encode(struct termp *p, const char *word, size_t sz)
{
	size_t		  i;

	if (p->flags & TERMP_NOBUF) {
		for (i = 0; i < sz; i++)
			(*p->letter)(p, word[i]);
		return;
	}

	if (p->col + 2 + (sz * 5) >= p->tcol->maxcols)
		adjbuf(p->tcol, p->col + 2 + (sz * 5));

	for (i = 0; i < sz; i++) {
		if (ASCII_HYPH == word[i] ||
		    isgraph((unsigned char)word[i]))
			encode1(p, word[i]);
		else {
			if (p->tcol->lastcol <= p->col ||
			    (word[i] != ' ' && word[i] != ASCII_NBRSP))
				p->tcol->buf[p->col] = word[i];
			p->col++;

			/*
			 * Postpone the effect of \z while handling
			 * an overstrike sequence from ascii_uc2str().
			 */

			if (word[i] == '\b' &&
			    (p->flags & TERMP_BACKBEFORE)) {
				p->flags &= ~TERMP_BACKBEFORE;
				p->flags |= TERMP_BACKAFTER;
			}
		}
	}
	if (p->tcol->lastcol < p->col)
		p->tcol->lastcol = p->col;
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
		if (a2roffsu(wstr, &su, SCALE_MAX) != NULL)
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
			rhs = NULL;
			esc = mandoc_escape(&cp, &seq, &ssz);
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
			case ESCAPE_UNDEF:
				uc = *seq;
				break;
			case ESCAPE_DEVICE:
				if (p->type == TERMTYPE_PDF) {
					rhs = "pdf";
					rsz = 3;
				} else if (p->type == TERMTYPE_PS) {
					rhs = "ps";
					rsz = 2;
				} else if (p->enc == TERMENC_ASCII) {
					rhs = "ascii";
					rsz = 5;
				} else {
					rhs = "utf8";
					rsz = 4;
				}
				break;
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
 * Convert a scaling width to basic units, rounding towards 0.
 */
int
term_hspan(const struct termp *p, const struct roffsu *su)
{

	return (*p->hspan)(p, su);
}

/*
 * Convert a scaling width to basic units, rounding to closest.
 */
int
term_hen(const struct termp *p, const struct roffsu *su)
{
	int bu;

	if ((bu = (*p->hspan)(p, su)) >= 0)
		return (bu + 11) / 24;
	else
		return -((-bu + 11) / 24);
}
