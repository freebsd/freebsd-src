/*
 * re.c - compile regular expressions.
 */

/* 
 * Copyright (C) 1991, 1992 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Progamming Language.
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
 * along with GAWK; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "awk.h"

/* Generate compiled regular expressions */

Regexp *
make_regexp(s, len, ignorecase, dfa)
char *s;
int len;
int ignorecase;
int dfa;
{
	Regexp *rp;
	char *err;
	char *src = s;
	char *temp;
	char *end = s + len;
	register char *dest;
	register int c;

	/* Handle escaped characters first. */

	/* Build a copy of the string (in dest) with the
	   escaped characters translated,  and generate the regex
	   from that.  
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
				c = parse_escape(&src);
				if (c < 0)
					cant_happen();
				*dest++ = (char)c;
				break;
			default:
				*dest++ = '\\';
				*dest++ = (char)c;
				src++;
				break;
			} /* switch */
		} else {
			*dest++ = *src++;	/* not '\\' */
		}
	} /* for */

	*dest = '\0' ;	/* Only necessary if we print dest ? */
	emalloc(rp, Regexp *, sizeof(*rp), "make_regexp");
	memset((char *) rp, 0, sizeof(*rp));
	emalloc(rp->pat.buffer, char *, 16, "make_regexp");
	rp->pat.allocated = 16;
	emalloc(rp->pat.fastmap, char *, 256, "make_regexp");

	if (ignorecase)
		rp->pat.translate = casetable;
	else
		rp->pat.translate = NULL;
	len = dest - temp;
	if ((err = re_compile_pattern(temp, (size_t) len, &(rp->pat))) != NULL)
		fatal("%s: /%s/", err, temp);
	if (dfa && !ignorecase) {
		regcompile(temp, len, &(rp->dfareg), 1);
		rp->dfa = 1;
	} else
		rp->dfa = 0;
	free(temp);
	return rp;
}

int
research(rp, str, start, len, need_start)
Regexp *rp;
register char *str;
int start;
register int len;
int need_start;
{
	char *ret = str;

	if (rp->dfa) {
		char save1;
		char save2;
		int count = 0;
		int try_backref;

		save1 = str[start+len];
		str[start+len] = '\n';
		save2 = str[start+len+1];
		ret = regexecute(&(rp->dfareg), str+start, str+start+len+1, 1,
					&count, &try_backref);
		str[start+len] = save1;
		str[start+len+1] = save2;
	}
	if (ret) {
		if (need_start || rp->dfa == 0)
			return re_search(&(rp->pat), str, start+len, start,
					len, &(rp->regs));
		else
			return 1;
	 } else
		return -1;
}

void
refree(rp)
Regexp *rp;
{
	free(rp->pat.buffer);
	free(rp->pat.fastmap);
	if (rp->dfa)
		reg_free(&(rp->dfareg));
	free(rp);
}

void
reg_error(s)
const char *s;
{
	fatal(s);
}

Regexp *
re_update(t)
NODE *t;
{
	NODE *t1;

#	define	CASE	1
	if ((t->re_flags & CASE) == IGNORECASE) {
		if (t->re_flags & CONST)
			return t->re_reg;
		t1 = force_string(tree_eval(t->re_exp));
		if (t->re_text) {
			if (cmp_nodes(t->re_text, t1) == 0) {
				free_temp(t1);
				return t->re_reg;
			}
			unref(t->re_text);
		}
		t->re_text = dupnode(t1);
		free_temp(t1);
	}
	if (t->re_reg)
		refree(t->re_reg);
	if (t->re_cnt)
		t->re_cnt++;
	if (t->re_cnt > 10)
		t->re_cnt = 0;
	if (!t->re_text) {
		t1 = force_string(tree_eval(t->re_exp));
		t->re_text = dupnode(t1);
		free_temp(t1);
	}
	t->re_reg = make_regexp(t->re_text->stptr, t->re_text->stlen, IGNORECASE, t->re_cnt);
	t->re_flags &= ~CASE;
	t->re_flags |= IGNORECASE;
	return t->re_reg;
}

void
resetup()
{
	(void) re_set_syntax(RE_SYNTAX_AWK);
	regsyntax(RE_SYNTAX_AWK, 0);
}
