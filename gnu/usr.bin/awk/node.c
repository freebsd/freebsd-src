/*
 * node.c -- routines for node management
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991, 1992 the Free Software Foundation, Inc.
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

extern double strtod();

AWKNUM
r_force_number(n)
register NODE *n;
{
	register char *cp;
	register char *cpend;
	char save;
	char *ptr;
	unsigned int newflags = 0;

#ifdef DEBUG
	if (n == NULL)
		cant_happen();
	if (n->type != Node_val)
		cant_happen();
	if(n->flags == 0)
		cant_happen();
	if (n->flags & NUM)
		return n->numbr;
#endif

	/* all the conditionals are an attempt to avoid the expensive strtod */

	n->numbr = 0.0;
	n->flags |= NUM;

	if (n->stlen == 0)
		return 0.0;

	cp = n->stptr;
	if (isalpha(*cp))
		return 0.0;

	cpend = cp + n->stlen;
	while (cp < cpend && isspace(*cp))
		cp++;
	if (cp == cpend || isalpha(*cp))
		return 0.0;

	if (n->flags & MAYBE_NUM) {
		newflags = NUMBER;
		n->flags &= ~MAYBE_NUM;
	}
	if (cpend - cp == 1) {
		if (isdigit(*cp)) {
			n->numbr = (AWKNUM)(*cp - '0');
			n->flags |= newflags;
		}
		return n->numbr;
	}

	errno = 0;
	save = *cpend;
	*cpend = '\0';
	n->numbr = (AWKNUM) strtod((const char *)cp, &ptr);

	/* POSIX says trailing space is OK for NUMBER */
	while (isspace(*ptr))
		ptr++;
	*cpend = save;
	/* the >= should be ==, but for SunOS 3.5 strtod() */
	if (errno == 0 && ptr >= cpend)
		n->flags |= newflags;
	else
		errno = 0;

	return n->numbr;
}

/*
 * the following lookup table is used as an optimization in force_string
 * (more complicated) variations on this theme didn't seem to pay off, but 
 * systematic testing might be in order at some point
 */
static char *values[] = {
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
};
#define	NVAL	(sizeof(values)/sizeof(values[0]))

NODE *
r_force_string(s)
register NODE *s;
{
	char buf[128];
	register char *sp = buf;
	register long num = 0;

#ifdef DEBUG
	if (s == NULL) cant_happen();
	if (s->type != Node_val) cant_happen();
	if (s->flags & STR) return s;
	if (!(s->flags & NUM)) cant_happen();
	if (s->stref != 0) ; /*cant_happen();*/
#endif

        /* avoids floating point exception in DOS*/
        if ( s->numbr <= LONG_MAX && s->numbr >= -LONG_MAX)
		num = (long)s->numbr;
	if ((AWKNUM) num == s->numbr) {	/* integral value */
		if (num < NVAL && num >= 0) {
			sp = values[num];
			s->stlen = 1;
		} else {
			(void) sprintf(sp, "%ld", num);
			s->stlen = strlen(sp);
		}
		s->stfmt = -1;
	} else {
		(void) sprintf(sp, CONVFMT, s->numbr);
		s->stlen = strlen(sp);
		s->stfmt = (char)CONVFMTidx;
	}
	s->stref = 1;
	emalloc(s->stptr, char *, s->stlen + 2, "force_string");
	memcpy(s->stptr, sp, s->stlen+1);
	s->flags |= STR;
	return s;
}

/*
 * Duplicate a node.  (For strings, "duplicate" means crank up the
 * reference count.)
 */
NODE *
dupnode(n)
NODE *n;
{
	register NODE *r;

	if (n->flags & TEMP) {
		n->flags &= ~TEMP;
		n->flags |= MALLOC;
		return n;
	}
	if ((n->flags & (MALLOC|STR)) == (MALLOC|STR)) {
		if (n->stref < 255)
			n->stref++;
		return n;
	}
	getnode(r);
	*r = *n;
	r->flags &= ~(PERM|TEMP);
	r->flags |= MALLOC;
	if (n->type == Node_val && (n->flags & STR)) {
		r->stref = 1;
		emalloc(r->stptr, char *, r->stlen + 2, "dupnode");
		memcpy(r->stptr, n->stptr, r->stlen+1);
	}
	return r;
}

/* this allocates a node with defined numbr */
NODE *
mk_number(x, flags)
AWKNUM x;
unsigned int flags;
{
	register NODE *r;

	getnode(r);
	r->type = Node_val;
	r->numbr = x;
	r->flags = flags;
#ifdef DEBUG
	r->stref = 1;
	r->stptr = 0;
	r->stlen = 0;
#endif
	return r;
}

/*
 * Make a string node.
 */
NODE *
make_str_node(s, len, flags)
char *s;
size_t len;
int flags;
{
	register NODE *r;

	getnode(r);
	r->type = Node_val;
	r->flags = (STRING|STR|MALLOC);
	if (flags & ALREADY_MALLOCED)
		r->stptr = s;
	else {
		emalloc(r->stptr, char *, len + 2, s);
		memcpy(r->stptr, s, len);
	}
	r->stptr[len] = '\0';
	       
	if (flags & SCAN) {	/* scan for escape sequences */
		char *pf;
		register char *ptm;
		register int c;
		register char *end;

		end = &(r->stptr[len]);
		for (pf = ptm = r->stptr; pf < end;) {
			c = *pf++;
			if (c == '\\') {
				c = parse_escape(&pf);
				if (c < 0) {
					if (do_lint)
						warning("backslash at end of string");
					c = '\\';
				}
				*ptm++ = c;
			} else
				*ptm++ = c;
		}
		len = ptm - r->stptr;
		erealloc(r->stptr, char *, len + 1, "make_str_node");
		r->stptr[len] = '\0';
		r->flags |= PERM;
	}
	r->stlen = len;
	r->stref = 1;
	r->stfmt = -1;

	return r;
}

NODE *
tmp_string(s, len)
char *s;
size_t len;
{
	register NODE *r;

	r = make_string(s, len);
	r->flags |= TEMP;
	return r;
}


#define NODECHUNK	100

NODE *nextfree = NULL;

NODE *
more_nodes()
{
	register NODE *np;

	/* get more nodes and initialize list */
	emalloc(nextfree, NODE *, NODECHUNK * sizeof(NODE), "newnode");
	for (np = nextfree; np < &nextfree[NODECHUNK - 1]; np++)
		np->nextp = np + 1;
	np->nextp = NULL;
	np = nextfree;
	nextfree = nextfree->nextp;
	return np;
}

#ifdef DEBUG
void
freenode(it)
NODE *it;
{
#ifdef MPROF
	it->stref = 0;
	free((char *) it);
#else	/* not MPROF */
	/* add it to head of freelist */
	it->nextp = nextfree;
	nextfree = it;
#endif	/* not MPROF */
}
#endif	/* DEBUG */

void
unref(tmp)
register NODE *tmp;
{
	if (tmp == NULL)
		return;
	if (tmp->flags & PERM)
		return;
	if (tmp->flags & (MALLOC|TEMP)) {
		tmp->flags &= ~TEMP;
		if (tmp->flags & STR) {
			if (tmp->stref > 1) {
				if (tmp->stref != 255)
					tmp->stref--;
				return;
			}
			free(tmp->stptr);
		}
		freenode(tmp);
	}
}

/*
 * Parse a C escape sequence.  STRING_PTR points to a variable containing a
 * pointer to the string to parse.  That pointer is updated past the
 * characters we use.  The value of the escape sequence is returned. 
 *
 * A negative value means the sequence \ newline was seen, which is supposed to
 * be equivalent to nothing at all. 
 *
 * If \ is followed by a null character, we return a negative value and leave
 * the string pointer pointing at the null character. 
 *
 * If \ is followed by 000, we return 0 and leave the string pointer after the
 * zeros.  A value of 0 does not mean end of string.  
 *
 * Posix doesn't allow \x.
 */

int
parse_escape(string_ptr)
char **string_ptr;
{
	register int c = *(*string_ptr)++;
	register int i;
	register int count;

	switch (c) {
	case 'a':
		return BELL;
	case 'b':
		return '\b';
	case 'f':
		return '\f';
	case 'n':
		return '\n';
	case 'r':
		return '\r';
	case 't':
		return '\t';
	case 'v':
		return '\v';
	case '\n':
		return -2;
	case 0:
		(*string_ptr)--;
		return -1;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		i = c - '0';
		count = 0;
		while (++count < 3) {
			if ((c = *(*string_ptr)++) >= '0' && c <= '7') {
				i *= 8;
				i += c - '0';
			} else {
				(*string_ptr)--;
				break;
			}
		}
		return i;
	case 'x':
		if (do_lint) {
			static int didwarn;

			if (! didwarn) {
				didwarn = 1;
				warning("Posix does not allow \"\\x\" escapes");
			}
		}
		if (do_posix)
			return ('x');
		i = 0;
		while (1) {
			if (isxdigit((c = *(*string_ptr)++))) {
				i *= 16;
				if (isdigit(c))
					i += c - '0';
				else if (isupper(c))
					i += c - 'A' + 10;
				else
					i += c - 'a' + 10;
			} else {
				(*string_ptr)--;
				break;
			}
		}
		return i;
	default:
		return c;
	}
}
