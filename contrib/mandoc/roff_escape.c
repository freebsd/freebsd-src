/* $Id: roff_escape.c,v 1.15 2024/05/16 21:23:00 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2012, 2013, 2014, 2015, 2017, 2018, 2020, 2022
 *               Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
 *
 * Parser for roff(7) escape sequences.
 * To be used by all mandoc(1) parsers and formatters.
 */
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "mandoc.h"
#include "roff.h"
#include "roff_int.h"

/*
 * Traditional escape sequence interpreter for general use
 * including in high-level formatters.  This function does not issue
 * diagnostics and is not usable for expansion in the roff(7) parser.
 * It is documented in the mandoc_escape(3) manual page.
 */
enum mandoc_esc
mandoc_escape(const char **rendarg, const char **rarg, int *rargl)
{
        int		 iarg, iendarg, iend;
        enum mandoc_esc  rval;

        rval = roff_escape(--*rendarg, 0, 0,
	    NULL, NULL, &iarg, &iendarg, &iend);
        assert(rval != ESCAPE_EXPAND);
        if (rarg != NULL)
	       *rarg = *rendarg + iarg;
        if (rargl != NULL)
	       *rargl = iendarg - iarg;
        *rendarg += iend;
        return rval;
}

/*
 * Full-featured escape sequence parser.
 * If it encounters a nested escape sequence that requires expansion
 * by the parser and re-parsing, the positions of that inner escape
 * sequence are returned in *resc ... *rend.
 * Otherwise, *resc is set to aesc and the positions of the escape
 * sequence starting at aesc are returned.
 * Diagnostic messages are generated if and only if ln != 0,
 * that is, if and only if called by roff_expand().
 */
enum mandoc_esc
roff_escape(const char *buf, const int ln, const int aesc,
    int *resc, int *rnam, int *rarg, int *rendarg, int *rend)
{
	int		 iesc;		/* index of leading escape char */
	int		 inam;		/* index of escape name */
	int		 iarg;		/* index beginning the argument */
	int		 iendarg;	/* index right after the argument */
	int		 iend;		/* index right after the sequence */
	int		 sesc, snam, sarg, sendarg, send; /* for sub-escape */
	int		 escterm;	/* whether term is escaped */
	int		 maxl;		/* expected length of the argument */
	int		 argl;		/* actual length of the argument */
	int		 c, i;		/* for \[char...] parsing */
	int 		 valid_A;	/* for \A parsing */
	enum mandoc_esc	 rval;		/* return value */
	enum mandoc_esc	 stype;		/* for sub-escape */
	enum mandocerr	 err;		/* diagnostic code */
	char		 term;		/* byte terminating the argument */

	/*
	 * Treat "\E" just like "\";
	 * it only makes a difference in copy mode.
	 */

	iesc = inam = aesc;
	do {
		inam++;
	} while (buf[inam] == 'E');

	/*
	 * Sort the following cases first by syntax category,
	 * then by escape sequence type, and finally by ASCII code.
	 */

	iarg = iendarg = iend = inam + 1;
	maxl = INT_MAX;
	term = '\0';
	err = MANDOCERR_OK;
	switch (buf[inam]) {

	/* Escape sequences taking no arguments at all. */

	case '!':
	case '?':
	case 'r':
		rval = ESCAPE_UNSUPP;
		goto out;

	case '%':
	case '&':
	case ')':
	case ',':
	case '/':
	case '^':
	case 'a':
	case 'd':
	case 't':
	case 'u':
	case '{':
	case '|':
	case '}':
		rval = ESCAPE_IGNORE;
		goto out;

	case '\0':
		iendarg = --iend;
		/* FALLTHROUGH */
	case '.':
	case '\\':
	default:
		iarg--;
		rval = ESCAPE_UNDEF;
		goto out;

	case ' ':
	case '\'':
	case '-':
	case '0':
	case ':':
	case '_':
	case '`':
	case 'e':
	case '~':
		iarg--;
		argl = 1;
		rval = ESCAPE_SPECIAL;
		goto out;
	case 'p':
		rval = ESCAPE_BREAK;
		goto out;
	case 'c':
		rval = ESCAPE_NOSPACE;
		goto out;
	case 'z':
		rval = ESCAPE_SKIPCHAR;
		goto out;

	/* Standard argument format. */

	case '$':
	case '*':
	case 'V':
	case 'g':
	case 'n':
		rval = ESCAPE_EXPAND;
		break;
	case 'F':
	case 'M':
	case 'O':
	case 'Y':
	case 'k':
	case 'm':
		rval = ESCAPE_IGNORE;
		break;
	case '(':
	case '[':
		rval = ESCAPE_SPECIAL;
		iendarg = iend = --iarg;
		break;
	case 'f':
		rval = ESCAPE_FONT;
		break;

	/* Quoted arguments */

	case 'A':
	case 'B':
	case 'w':
		rval = ESCAPE_EXPAND;
		term = '\b';
		break;
	case 'D':
	case 'H':
	case 'L':
	case 'R':
	case 'S':
	case 'X':
	case 'Z':
	case 'b':
	case 'v':
	case 'x':
		rval = ESCAPE_IGNORE;
		term = '\b';
		break;
	case 'C':
		rval = ESCAPE_SPECIAL;
		term = '\b';
		break;
	case 'N':
		rval = ESCAPE_NUMBERED;
		term = '\b';
		break;
	case 'h':
		rval = ESCAPE_HORIZ;
		term = '\b';
		break;
	case 'l':
		rval = ESCAPE_HLINE;
		term = '\b';
		break;
	case 'o':
		rval = ESCAPE_OVERSTRIKE;
		term = '\b';
		break;

	/* Sizes support both forms, with additional peculiarities. */

	case 's':
		rval = ESCAPE_IGNORE;
		if (buf[iarg] == '+' || buf[iarg] == '-'||
		    buf[iarg] == ASCII_HYPH)
			iarg++;
		switch (buf[iarg]) {
		case '(':
			maxl = 2;
			iarg++;
			break;
		case '[':
			term = ']';
			iarg++;
			break;
		case '\'':
			term = '\'';
			iarg++;
			break;
		case '1':
		case '2':
		case '3':
			if (buf[iarg - 1] == 's' &&
			    isdigit((unsigned char)buf[iarg + 1])) {
				maxl = 2;
				break;
			}
			/* FALLTHROUGH */
		default:
			maxl = 1;
			break;
		}
		iendarg = iend = iarg;
	}

	/* Decide how to end the argument. */

	escterm = 0;
	stype = ESCAPE_EXPAND;
	if ((term == '\b' || (term == '\0' && maxl == INT_MAX)) &&
	    buf[iarg] == buf[iesc]) {
		stype = roff_escape(buf, ln, iendarg,
		    &sesc, &snam, &sarg, &sendarg, &send);
		if (stype == ESCAPE_EXPAND)
			goto out_sub;
	}

	if (term == '\b') {
		if (stype == ESCAPE_UNDEF)
			iarg++;
		if (stype != ESCAPE_EXPAND && stype != ESCAPE_UNDEF) {
			if (strchr("BHLRSNhlvx", buf[inam]) != NULL &&
			    strchr(" ,.0DLOXYZ^abdhlortuvx|~",
			    buf[snam]) != NULL) {
				err = MANDOCERR_ESC_DELIM;
				iend = send;
				iarg = iendarg = sesc;
				goto out;
			}
			escterm = 1;
			iarg = send;
			term = buf[snam];
		} else if (strchr("BDHLRSvxNhl", buf[inam]) != NULL &&
		    strchr(" %&()*+-./0123456789:<=>", buf[iarg]) != NULL) {
			err = MANDOCERR_ESC_DELIM;
			if (rval != ESCAPE_EXPAND)
				rval = ESCAPE_ERROR;
			if (buf[inam] != 'D') {
				iendarg = iend = iarg + 1;
				goto out;
			}
		}
		if (term == '\b')
			term = buf[iarg++];
	} else if (term == '\0' && maxl == INT_MAX) {
		if (buf[inam] == 'n' && (buf[iarg] == '+' || buf[iarg] == '-'))
			iarg++;
		switch (buf[iarg]) {
		case '(':
			maxl = 2;
			iarg++;
			break;
		case '[':
			if (buf[++iarg] == ' ') {
				iendarg = iend = iarg + 1;
				err = MANDOCERR_ESC_ARG;
				rval = ESCAPE_ERROR;
				goto out;
			}
			term = ']';
			break;
		default:
			maxl = 1;
			break;
		}
	}

	/* Advance to the end of the argument. */

	valid_A = 1;
	iendarg = iarg;
	while (maxl > 0) {
		if (buf[iendarg] == '\0') {
			err = MANDOCERR_ESC_INCOMPLETE;
			if (rval != ESCAPE_EXPAND &&
			    rval != ESCAPE_OVERSTRIKE)
				rval = ESCAPE_ERROR;
			/* Usually, ignore an incomplete argument. */
			if (strchr("Aow", buf[inam]) == NULL)
				iendarg = iarg;
			break;
		}
		if (escterm == 0 && buf[iendarg] == term) {
			iend = iendarg + 1;
			break;
		}
		if (buf[iendarg] == buf[iesc]) {
			stype = roff_escape(buf, ln, iendarg,
			    &sesc, &snam, &sarg, &sendarg, &send);
			if (stype == ESCAPE_EXPAND)
				goto out_sub;
			iend = send;
			if (escterm == 1 &&
			    (buf[snam] == term || buf[inam] == 'N'))
				break;
			if (stype != ESCAPE_UNDEF)
				valid_A = 0;
			iendarg = send;
		} else if (buf[inam] == 'N' &&
		    isdigit((unsigned char)buf[iendarg]) == 0) {
			iend = iendarg + 1;
			break;
		} else {
			if (buf[iendarg] == ' ' || buf[iendarg] == '\t')
				valid_A = 0;
			if (maxl != INT_MAX)
				maxl--;
			iend = ++iendarg;
		}
	}

	/* Post-process depending on the content of the argument. */

	argl = iendarg - iarg;
	switch (buf[inam]) {
	case '*':
		if (resc == NULL && argl == 2 &&
		    buf[iarg] == '.' && buf[iarg + 1] == 'T')
			rval = ESCAPE_DEVICE;
		break;
	case 'A':
		if (valid_A == 0)
			iendarg = iarg;
		break;
	case 'O':
		switch (buf[iarg]) {
		case '0':
			rval = ESCAPE_UNSUPP;
			break;
		case '1':
		case '2':
		case '3':
		case '4':
			if (argl == 1)
				rval = ESCAPE_IGNORE;
			else {
				err = MANDOCERR_ESC_ARG;
				rval = ESCAPE_ERROR;
			}
			break;
		case '5':
			if (buf[iarg - 1] == '[')
				rval = ESCAPE_UNSUPP;
			else {
				err = MANDOCERR_ESC_ARG;
				rval = ESCAPE_ERROR;
			}
			break;
		default:
			err = MANDOCERR_ESC_ARG;
			rval = ESCAPE_ERROR;
			break;
		}
		break;
	default:
		break;
	}

	switch (rval) {
	case ESCAPE_FONT:
		rval = mandoc_font(buf + iarg, argl);
		if (rval == ESCAPE_ERROR)
			err = MANDOCERR_ESC_ARG;
		break;

	case ESCAPE_SPECIAL:
		if (argl == 0) {
			err = MANDOCERR_ESC_BADCHAR;
			rval = ESCAPE_ERROR;
			break;
		}

		/*
		 * The file chars.c only provides one common list of
		 * character names, but \[-] == \- is the only one of
		 * the characters with one-byte names that allows
		 * enclosing the name in brackets.
		 */

		if (term != '\0' && argl == 1 && buf[iarg] != '-') {
			err = MANDOCERR_ESC_BADCHAR;
			rval = ESCAPE_ERROR;
			break;
		}

		/* Treat \[char...] as an alias for \N'...'. */

		if (buf[iarg] == 'c') {
			if (argl < 6 || argl > 7 ||
			    strncmp(buf + iarg, "char", 4) != 0 ||
			    (int)strspn(buf + iarg + 4, "0123456789")
			     + 4 < argl)
				break;
			c = 0;
			for (i = iarg; i < iendarg; i++)
				c = 10 * c + (buf[i] - '0');
			if (c < 0x21 || (c > 0x7e && c < 0xa0) || c > 0xff) {
				err = MANDOCERR_ESC_BADCHAR;
				break;
			}
			iarg += 4;
			rval = ESCAPE_NUMBERED;
			break;
		}

		/*
		 * Unicode escapes are defined in groff as \[u0000]
		 * to \[u10FFFF], where the contained value must be
		 * a valid Unicode codepoint.
		 */

		if (buf[iarg] != 'u' || argl < 5 || argl > 7)
			break;
		if (argl == 7 &&  /* beyond the Unicode range */
		    (buf[iarg + 1] != '1' || buf[iarg + 2] != '0')) {
			err = MANDOCERR_ESC_BADCHAR;
			break;
		}
		if (argl == 6 && buf[iarg + 1] == '0') {
			err = MANDOCERR_ESC_BADCHAR;
			break;
		}
		if (argl == 5 &&  /* UTF-16 surrogate */
		    toupper((unsigned char)buf[iarg + 1]) == 'D' &&
		    strchr("89ABCDEFabcdef", buf[iarg + 2]) != NULL) {
			err = MANDOCERR_ESC_BADCHAR;
			break;
		}
		if ((int)strspn(buf + iarg + 1, "0123456789ABCDEFabcdef")
		    + 1 == argl)
			rval = ESCAPE_UNICODE;
		break;
	default:
		break;
	}
	goto out;

out_sub:
	iesc = sesc;
	inam = snam;
	iarg = sarg;
	iendarg = sendarg;
	iend = send;
	rval = ESCAPE_EXPAND;

out:
	if (resc != NULL)
		*resc = iesc;
	if (rnam != NULL)
		*rnam = inam;
	if (rarg != NULL)
		*rarg = iarg;
	if (rendarg != NULL)
		*rendarg = iendarg;
	if (rend != NULL)
		*rend = iend;
	if (ln == 0)
		return rval;

	/*
	 * Diagnostic messages are only issued when called
	 * from the parser, not when called from the formatters.
	 */

	switch (rval) {
	case ESCAPE_UNSUPP:
		err = MANDOCERR_ESC_UNSUPP;
		break;
	case ESCAPE_UNDEF:
		if (buf[inam] != '\\' && buf[inam] != '.')
			err = MANDOCERR_ESC_UNDEF;
		break;
	case ESCAPE_SPECIAL:
		if (mchars_spec2cp(buf + iarg, argl) >= 0)
			err = MANDOCERR_OK;
		else if (err == MANDOCERR_OK)
			err = MANDOCERR_ESC_UNKCHAR;
		break;
	default:
		break;
	}
	if (err != MANDOCERR_OK)
		mandoc_msg(err, ln, iesc, "%.*s", iend - iesc, buf + iesc);
	return rval;
}
