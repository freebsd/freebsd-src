/*
 * re.c - compile regular expressions.
 */

/* 
 * Copyright (C) 1991-1996 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Programming Language.
 * 
 * GAWK is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GAWK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 */

#include "awk.h"

static reg_syntax_t syn;

/* make_regexp --- generate compiled regular expressions */

Regexp *
make_regexp(s, len, ignorecase, dfa)
char *s;
size_t len;
int ignorecase;
int dfa;
{
	Regexp *rp;
	const char *rerr;
	char *src = s;
	char *temp;
	char *end = s + len;
	register char *dest;
	register int c, c2;

	/* Handle escaped characters first. */

	/*
	 * Build a copy of the string (in dest) with the
	 * escaped characters translated, and generate the regex
	 * from that.  
	 */
	emalloc(dest, char *, len + 2, "make_regexp");
	temp = dest;

	while (src < end) {
		if (*src == '\\') {
			c = *++src;
			switch (c) {
			case 'a':
			case 'b':
			case 'f':
			case 'n':
			case 'r':
			case 't':
			case 'v':
			case 'x':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				c2 = parse_escape(&src);
				if (c2 < 0)
					cant_happen();
				/*
				 * Unix awk treats octal (and hex?) chars
				 * literally in re's, so escape regexp
				 * metacharacters.
				 */
				if (do_traditional && ! do_posix && (isdigit(c) || c == 'x')
				    && strchr("()|*+?.^$\\[]", c2) != NULL)
					*dest++ = '\\';
				*dest++ = (char) c2;
				break;
			case '8':
			case '9':	/* a\9b not valid */
				*dest++ = c;
				src++;
				break;
			case 'y':	/* normally \b */
				/* gnu regex op */
				if (! do_traditional) {
					*dest++ = '\\';
					*dest++ = 'b';
					src++;
					break;
				}
				/* else, fall through */
			default:
				*dest++ = '\\';
				*dest++ = (char) c;
				src++;
				break;
			} /* switch */
		} else
			*dest++ = *src++;	/* not '\\' */
	} /* for */

	*dest = '\0' ;	/* Only necessary if we print dest ? */
	emalloc(rp, Regexp *, sizeof(*rp), "make_regexp");
	memset((char *) rp, 0, sizeof(*rp));
	rp->pat.allocated = 0;	/* regex will allocate the buffer */
	emalloc(rp->pat.fastmap, char *, 256, "make_regexp");

	if (ignorecase)
		rp->pat.translate = casetable;
	else
		rp->pat.translate = NULL;
	len = dest - temp;
	if ((rerr = re_compile_pattern(temp, len, &(rp->pat))) != NULL)
		fatal("%s: /%s/", rerr, temp);

	/* gack. this must be done *after* re_compile_pattern */
	rp->pat.newline_anchor = FALSE; /* don't get \n in middle of string */
	if (dfa && ! ignorecase) {
		dfacomp(temp, len, &(rp->dfareg), TRUE);
		rp->dfa = TRUE;
	} else
		rp->dfa = FALSE;

	free(temp);
	return rp;
}

/* research --- do a regexp search. use dfa if possible */

int
research(rp, str, start, len, need_start)
Regexp *rp;
register char *str;
int start;
register size_t len;
int need_start;
{
	char *ret = str;
	int try_backref;

	/*
	 * Always do dfa search if can; if it fails, then even if
	 * need_start is true, we won't bother with the regex search.
	 */
	if (rp->dfa) {
		char save;
		int count = 0;

		/*
		 * dfa likes to stick a '\n' right after the matched
		 * text.  So we just save and restore the character.
		 */
		save = str[start+len];
		ret = dfaexec(&(rp->dfareg), str+start, str+start+len, TRUE,
					&count, &try_backref);
		str[start+len] = save;
	}
	if (ret) {
		if (need_start || rp->dfa == FALSE || try_backref) {
			int result = re_search(&(rp->pat), str, start+len,
					start, len, &(rp->regs));
			/* recover any space from C based alloca */
#ifdef C_ALLOCA
			(void) alloca(0);
#endif
			return result;
		} else
			return 1;
	} else
		return -1;
}

/* refree --- free up the dynamic memory used by a compiled regexp */

void
refree(rp)
Regexp *rp;
{
	free(rp->pat.buffer);
	free(rp->pat.fastmap);
	if (rp->regs.start)
		free(rp->regs.start);
	if (rp->regs.end)
		free(rp->regs.end);
	if (rp->dfa)
		dfafree(&(rp->dfareg));
	free(rp);
}

/* dfaerror --- print an error message for the dfa routines */

void
dfaerror(s)
const char *s;
{
	fatal("%s", s);
}

/* re_update --- recompile a dynamic regexp */

Regexp *
re_update(t)
NODE *t;
{
	NODE *t1;

/* #	define	CASE	1 */
	if ((t->re_flags & CASE) == IGNORECASE) {
		if ((t->re_flags & CONST) != 0)
			return t->re_reg;
		t1 = force_string(tree_eval(t->re_exp));
		if (t->re_text != NULL) {
			if (cmp_nodes(t->re_text, t1) == 0) {
				free_temp(t1);
				return t->re_reg;
			}
			unref(t->re_text);
		}
		t->re_text = dupnode(t1);
		free_temp(t1);
	}
	if (t->re_reg != NULL)
		refree(t->re_reg);
	if (t->re_cnt > 0)
		t->re_cnt++;
	if (t->re_cnt > 10)
		t->re_cnt = 0;
	if (t->re_text == NULL) {
		t1 = force_string(tree_eval(t->re_exp));
		t->re_text = dupnode(t1);
		free_temp(t1);
	}
	t->re_reg = make_regexp(t->re_text->stptr, t->re_text->stlen,
				IGNORECASE, t->re_cnt);
	t->re_flags &= ~CASE;
	t->re_flags |= IGNORECASE;
	return t->re_reg;
}

/* resetup --- choose what kind of regexps we match */

void
resetup()
{
	if (do_posix)
		syn = RE_SYNTAX_POSIX_AWK;	/* strict POSIX re's */
	else if (do_traditional)
		syn = RE_SYNTAX_AWK;		/* traditional Unix awk re's */
	else
		syn = RE_SYNTAX_GNU_AWK;	/* POSIX re's + GNU ops */

	/*
	 * Interval expressions are off by default, since it's likely to
	 * break too many old programs to have them on.
	 */
	if (do_intervals)
		syn |= RE_INTERVALS;

	(void) re_set_syntax(syn);
	dfasyntax(syn, FALSE);
}

/* avoid_dfa --- FIXME: temporary kludge function until we have a new dfa.c */

int
avoid_dfa(re, str, len)
NODE *re;
char *str;
size_t len;
{
	char *restr;
	int relen;
	int anchor, i;
	char *end;

	if ((re->re_flags & CONST) != 0) {
		restr = re->re_exp->stptr;
		relen = re->re_exp->stlen;
	} else {
		restr = re->re_text->stptr;
		relen = re->re_text->stlen;
	}

	for (anchor = FALSE, i = 0; i < relen; i++) {
		if (restr[i] == '^' || restr[i] == '$') {
			anchor = TRUE;
			break;
		}
	}
	if (! anchor)
		return FALSE;

	for (end = str + len; str < end; str++)
		if (*str == '\n')
			return TRUE;

	return FALSE;
}
