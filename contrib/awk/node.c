/*
 * node.c -- routines for node management
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991-1997 the Free Software Foundation, Inc.
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

/* r_force_number --- force a value to be numeric */

AWKNUM
r_force_number(n)
register NODE *n;
{
	register char *cp;
	register char *cpend;
	char save;
	char *ptr;
	unsigned int newflags;

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
	if (ISALPHA(*cp))
		return 0.0;

	cpend = cp + n->stlen;
	while (cp < cpend && isspace((unsigned char)*cp))
		cp++;
	if (cp == cpend || isalpha((unsigned char)*cp))
		return 0.0;

	if (n->flags & MAYBE_NUM) {
		newflags = NUMBER;
		n->flags &= ~MAYBE_NUM;
	} else
		newflags = 0;
	if (cpend - cp == 1) {
		if (ISDIGIT(*cp)) {
			n->numbr = (AWKNUM)(*cp - '0');
			n->flags |= newflags;
		}
		return n->numbr;
	}

#ifdef NONDECDATA
	errno = 0;
	if (! do_traditional && isnondecimal(cp)) {
		n->numbr = nondec2awknum(cp, cpend - cp);
		goto finish;
	}
#endif /* NONDECDATA */

	errno = 0;
	save = *cpend;
	*cpend = '\0';
	n->numbr = (AWKNUM) strtod((const char *) cp, &ptr);

	/* POSIX says trailing space is OK for NUMBER */
	while (ISSPACE(*ptr))
		ptr++;
	*cpend = save;
finish:
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
static const char *values[] = {
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

/* format_val --- format a numeric value based on format */

NODE *
format_val(format, index, s)
char *format;
int index;
register NODE *s;
{
	char buf[128];
	register char *sp = buf;
	double val;

	/* not an integral value, or out of range */
	if ((val = double_to_int(s->numbr)) != s->numbr
	    || val < LONG_MIN || val > LONG_MAX) {
#ifdef GFMT_WORKAROUND
		NODE *dummy, *r;
		unsigned short oflags;
		extern NODE *format_tree P((const char *, int, NODE *));
		extern NODE **fmt_list;          /* declared in eval.c */

		/* create dummy node for a sole use of format_tree */
		getnode(dummy);
		dummy->lnode = s;
		dummy->rnode = NULL;
		oflags = s->flags;
		s->flags |= PERM; /* prevent from freeing by format_tree() */
		r = format_tree(format, fmt_list[index]->stlen, dummy);
		s->flags = oflags;
		s->stfmt = (char) index;
		s->stlen = r->stlen;
		s->stptr = r->stptr;
		freenode(r);		/* Do not free_temp(r)!  We want */
		freenode(dummy);	/* to keep s->stptr == r->stpr.  */

		goto no_malloc;
#else
		/*
		 * no need for a "replacement" formatting by gawk,
		 * just use sprintf
		 */
		sprintf(sp, format, s->numbr);
		s->stlen = strlen(sp);
		s->stfmt = (char) index;
#endif /* GFMT_WORKAROUND */
	} else {
		/* integral value */
	        /* force conversion to long only once */
		register long num = (long) val;
		if (num < NVAL && num >= 0) {
			sp = (char *) values[num];
			s->stlen = 1;
		} else {
			(void) sprintf(sp, "%ld", num);
			s->stlen = strlen(sp);
		}
		s->stfmt = -1;
	}
	emalloc(s->stptr, char *, s->stlen + 2, "force_string");
	memcpy(s->stptr, sp, s->stlen+1);
#ifdef GFMT_WORKAROUND
no_malloc:
#endif /* GFMT_WORKAROUND */
	s->stref = 1;
	s->flags |= STR;
	return s;
}

/* r_force_string --- force a value to be a string */

NODE *
r_force_string(s)
register NODE *s;
{
#ifdef DEBUG
	if (s == NULL)
		cant_happen();
	if (s->type != Node_val)
		cant_happen();
	if ((s->flags & NUM) == 0)
		cant_happen();
	if (s->stref <= 0)
		cant_happen();
	if ((s->flags & STR) != 0
	    && (s->stfmt == -1 || s->stfmt == CONVFMTidx))
		return s;
#endif

	return format_val(CONVFMT, CONVFMTidx, s);
}

/*
 * dupnode:
 * Duplicate a node.  (For strings, "duplicate" means crank up the
 * reference count.)
 */

NODE *
dupnode(n)
NODE *n;
{
	register NODE *r;

	if ((n->flags & TEMP) != 0) {
		n->flags &= ~TEMP;
		n->flags |= MALLOC;
		return n;
	}
	if ((n->flags & (MALLOC|STR)) == (MALLOC|STR)) {
		if (n->stref < LONG_MAX)
			n->stref++;
		return n;
	}
	getnode(r);
	*r = *n;
	r->flags &= ~(PERM|TEMP);
	r->flags |= MALLOC;
	if (n->type == Node_val && (n->flags & STR) != 0) {
		r->stref = 1;
		emalloc(r->stptr, char *, r->stlen + 2, "dupnode");
		memcpy(r->stptr, n->stptr, r->stlen);
		r->stptr[r->stlen] = '\0';
	}
	return r;
}

/* mk_number --- allocate a node with defined number */

NODE *
mk_number(x, flags)
AWKNUM x;
unsigned int flags;
{
	register NODE *r;

	getnode(r);
	r->type = Node_val;
	r->numbr = x;
	r->flags = flags | SCALAR;
#ifdef DEBUG
	r->stref = 1;
	r->stptr = NULL;
	r->stlen = 0;
#endif
	return r;
}

/* make_str_node --- make a string node */

NODE *
make_str_node(s, len, flags)
char *s;
size_t len;
int flags;
{
	register NODE *r;

	getnode(r);
	r->type = Node_val;
	r->flags = (STRING|STR|MALLOC|SCALAR);
	if (flags & ALREADY_MALLOCED)
		r->stptr = s;
	else {
		emalloc(r->stptr, char *, len + 2, s);
		memcpy(r->stptr, s, len);
	}
	r->stptr[len] = '\0';
	       
	if ((flags & SCAN) != 0) {	/* scan for escape sequences */
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

/* tmp_string --- allocate a temporary string */

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

/* more_nodes --- allocate more nodes */

#define NODECHUNK	100

NODE *nextfree = NULL;

NODE *
more_nodes()
{
	register NODE *np;

	/* get more nodes and initialize list */
	emalloc(nextfree, NODE *, NODECHUNK * sizeof(NODE), "newnode");
	for (np = nextfree; np <= &nextfree[NODECHUNK - 1]; np++) {
		np->flags = 0;
		np->nextp = np + 1;
	}
	--np;
	np->nextp = NULL;
	np = nextfree;
	nextfree = nextfree->nextp;
	return np;
}

#ifdef DEBUG
/* freenode --- release a node back to the pool */

void
freenode(it)
NODE *it;
{
	it->flags &= ~SCALAR;
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

/* unref --- remove reference to a particular node */

void
unref(tmp)
register NODE *tmp;
{
	if (tmp == NULL)
		return;
	if ((tmp->flags & PERM) != 0)
		return;
	if ((tmp->flags & (MALLOC|TEMP)) != 0) {
		tmp->flags &= ~TEMP;
		if ((tmp->flags & STR) != 0) {
			if (tmp->stref > 1) {
				if (tmp->stref != LONG_MAX)
					tmp->stref--;
				return;
			}
			free(tmp->stptr);
		}
		freenode(tmp);
		return;
	}
	if ((tmp->flags & FIELD) != 0) {
		freenode(tmp);
		return;
	}
}

/*
 * parse_escape:
 *
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
			static int didwarn = FALSE;

			if (! didwarn) {
				didwarn = TRUE;
				warning("POSIX does not allow \"\\x\" escapes");
			}
		}
		if (do_posix)
			return ('x');
		if (! isxdigit((unsigned char)(*string_ptr)[0])) {
			warning("no hex digits in \\x escape sequence");
			return ('x');
		}
		i = 0;
		for (;;) {
			if (ISXDIGIT((c = *(*string_ptr)++))) {
				i *= 16;
				if (ISDIGIT(c))
					i += c - '0';
				else if (ISUPPER(c))
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
