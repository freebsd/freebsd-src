/*-
 * Copyright (c) 1992 Diomidis Spinellis.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Diomidis Spinellis of Imperial College, University of London.
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
/* from: static char sccsid[] = "@(#)compile.c	8.1 (Berkeley) 6/6/93"; */
static char *rcsid = "$Id: compile.c,v 1.5 1994/04/17 09:41:36 alm Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "extern.h"

#define LHSZ	128
#define	LHMASK	(LHSZ - 1)
static struct labhash {
	struct	labhash *lh_next;
	u_int	lh_hash;
	struct	s_command *lh_cmd;
	int	lh_ref;
} *labels[LHSZ];

static char	 *compile_addr __P((char *, struct s_addr *));
static char	 *compile_ccl __P((char **, char *));
static char	 *compile_delimited __P((char *, char *));
static char	 *compile_flags __P((char *, struct s_subst *));
static char	 *compile_re __P((char *, regex_t **));
static char	 *compile_subst __P((char *, struct s_subst *));
static char	 *compile_text __P((void));
static char	 *compile_tr __P((char *, char **));
static struct s_command
		**compile_stream __P((char *, struct s_command **, char *));
static char	 *duptoeol __P((char *, char *));
static void	  enterlabel __P((struct s_command *));
static struct s_command
		 *findlabel __P((char *));
static void	  fixuplabel __P((struct s_command *, struct s_command *));
static void	  uselabel __P((void));

/*
 * Command specification.  This is used to drive the command parser.
 */
struct s_format {
	char code;				/* Command code */
	int naddr;				/* Number of address args */
	enum e_args args;			/* Argument type */
};

static struct s_format cmd_fmts[] = {
	{'{', 2, GROUP},
	{'a', 1, TEXT},
	{'b', 2, BRANCH},
	{'c', 2, TEXT},
	{'d', 2, EMPTY},
	{'D', 2, EMPTY},
	{'g', 2, EMPTY},
	{'G', 2, EMPTY},
	{'h', 2, EMPTY},
	{'H', 2, EMPTY},
	{'i', 1, TEXT},
	{'l', 2, EMPTY},
	{'n', 2, EMPTY},
	{'N', 2, EMPTY},
	{'p', 2, EMPTY},
	{'P', 2, EMPTY},
	{'q', 1, EMPTY},
	{'r', 1, RFILE},
	{'s', 2, SUBST},
	{'t', 2, BRANCH},
	{'w', 2, WFILE},
	{'x', 2, EMPTY},
	{'y', 2, TR},
	{'!', 2, NONSEL},
	{':', 0, LABEL},
	{'#', 0, COMMENT},
	{'=', 1, EMPTY},
	{'\0', 0, COMMENT},
};

/* The compiled program. */
struct s_command *prog;

/*
 * Compile the program into prog.
 * Initialise appends.
 */
void
compile()
{
	*compile_stream(NULL, &prog, NULL) = NULL;
	fixuplabel(prog, NULL);
	uselabel();
	appends = xmalloc(sizeof(struct s_appends) * appendnum);
	match = xmalloc((maxnsub + 1) * sizeof(regmatch_t));
}

#define EATSPACE() do {							\
	if (p)								\
		while (*p && isascii(*p) && isspace(*p))		\
			p++;						\
	} while (0)

static struct s_command **
compile_stream(terminator, link, p)
	char *terminator;
	struct s_command **link;
	register char *p;
{
	static char lbuf[_POSIX2_LINE_MAX + 1];	/* To save stack */
	struct s_command *cmd, *cmd2;
	struct s_format *fp;
	int naddr;				/* Number of addresses */

	if (p != NULL)
		goto semicolon;
	for (;;) {
		if ((p = cu_fgets(lbuf, sizeof(lbuf))) == NULL) {
			if (terminator != NULL)
				err(COMPILE, "unexpected EOF (pending }'s)");
			return (link);
		}

semicolon:	EATSPACE();
		if (p && (*p == '#' || *p == '\0'))
			continue;
		if (*p == '}') {
			if (terminator == NULL)
				err(COMPILE, "unexpected }");
			return (link);
		}
		*link = cmd = xmalloc(sizeof(struct s_command));
		link = &cmd->next;
		cmd->nonsel = cmd->inrange = 0;
		/* First parse the addresses */
		naddr = 0;
		cmd->a1 = cmd->a2 = NULL;

/* Valid characters to start an address */
#define	addrchar(c)	(strchr("0123456789/\\$", (c)))
		if (addrchar(*p)) {
			naddr++;
			cmd->a1 = xmalloc(sizeof(struct s_addr));
			p = compile_addr(p, cmd->a1);
			EATSPACE();				/* EXTENSION */
			if (*p == ',') {
				naddr++;
				p++;
				EATSPACE();			/* EXTENSION */
				cmd->a2 = xmalloc(sizeof(struct s_addr));
				p = compile_addr(p, cmd->a2);
			}
		}

nonsel:		/* Now parse the command */
		EATSPACE();
		if (!*p)
			err(COMPILE, "command expected");
		cmd->code = *p;
		for (fp = cmd_fmts; fp->code; fp++)
			if (fp->code == *p)
				break;
		if (!fp->code)
			err(COMPILE, "invalid command code %c", *p);
		if (naddr > fp->naddr)
			err(COMPILE,
"command %c expects up to %d address(es), found %d", *p, fp->naddr, naddr);
		switch (fp->args) {
		case NONSEL:			/* ! */
			cmd->nonsel = ! cmd->nonsel;
			p++;
			goto nonsel;
		case GROUP:			/* { */
			p++;
			EATSPACE();
			if (!*p)
				p = NULL;
			cmd2 = xmalloc(sizeof(struct s_command));
			cmd2->code = '}';
			*compile_stream("}", &cmd->u.c, p) = cmd2;
			cmd->next = cmd2;
			link = &cmd2->next;
			break;
		case EMPTY:		/* d D g G h H l n N p P q x = \0 */
			p++;
			EATSPACE();
			if (*p == ';') {
				p++;
				link = &cmd->next;
				goto semicolon;
			}
			if (*p)
				err(COMPILE,
"extra characters at the end of %c command", cmd->code);
			break;
		case TEXT:			/* a c i */
			p++;
			EATSPACE();
			if (*p != '\\')
				err(COMPILE,
"command %c expects \\ followed by text", cmd->code);
			p++;
			EATSPACE();
			if (*p)
				err(COMPILE,
"extra characters after \\ at the end of %c command", cmd->code);
			cmd->t = compile_text();
			break;
		case COMMENT:			/* \0 # */
			break;
		case WFILE:			/* w */
			p++;
			EATSPACE();
			if (*p == '\0')
				err(COMPILE, "filename expected");
			cmd->t = duptoeol(p, "w command");
			if (aflag)
				cmd->u.fd = -1;
			else if ((cmd->u.fd = open(p, 
			    O_WRONLY|O_APPEND|O_CREAT|O_TRUNC,
			    DEFFILEMODE)) == -1)
				err(FATAL, "%s: %s\n", p, strerror(errno));
			break;
		case RFILE:			/* r */
			p++;
			EATSPACE();
			if (*p == '\0')
				err(COMPILE, "filename expected");
			else
				cmd->t = duptoeol(p, "read command");
			break;
		case BRANCH:			/* b t */
			p++;
			EATSPACE();
			if (*p == '\0')
				cmd->t = NULL;
			else
				cmd->t = duptoeol(p, "branch");
			break;
		case LABEL:			/* : */
			p++;
			EATSPACE();
			cmd->t = duptoeol(p, "label");
			if (strlen(p) == 0)
				err(COMPILE, "empty label");
			enterlabel(cmd);
			break;
		case SUBST:			/* s */
			p++;
			if (*p == '\0' || *p == '\\')
				err(COMPILE,
"substitute pattern can not be delimited by newline or backslash");
			cmd->u.s = xmalloc(sizeof(struct s_subst));
			p = compile_re(p, &cmd->u.s->re);
			if (p == NULL)
				err(COMPILE, "unterminated substitute pattern");
			--p;
			p = compile_subst(p, cmd->u.s);
			p = compile_flags(p, cmd->u.s);
			EATSPACE();
			if (*p == ';') {
				p++;
				link = &cmd->next;
				goto semicolon;
			}
			break;
		case TR:			/* y */
			p++;
			p = compile_tr(p, (char **)&cmd->u.y);
			EATSPACE();
			if (*p == ';') {
				p++;
				link = &cmd->next;
				goto semicolon;
			}
			if (*p)
				err(COMPILE,
"extra text at the end of a transform command");
			break;
		}
	}
}

/*
 * Get a delimited string.  P points to the delimeter of the string; d points
 * to a buffer area.  Newline and delimiter escapes are processed; other
 * escapes are ignored.
 *
 * Returns a pointer to the first character after the final delimiter or NULL
 * in the case of a non-terminated string.  The character array d is filled
 * with the processed string.
 */
static char *
compile_delimited(p, d)
	char *p, *d;
{
	char c;

	c = *p++;
	if (c == '\0')
		return (NULL);
	else if (c == '\\')
		err(COMPILE, "\\ can not be used as a string delimiter");
	else if (c == '\n')
		err(COMPILE, "newline can not be used as a string delimiter");
	while (*p) {
		if (*p == '[') {
			if ((d = compile_ccl(&p, d)) == NULL)
				err(COMPILE, "unbalanced brackets ([])");
			continue;
		} else if (*p == '\\' && p[1] == '[') {
			*d++ = *p++;
		} else if (*p == '\\' && p[1] == c)
			p++;
		else if (*p == '\\' && p[1] == 'n') {
			*d++ = '\n';
			p += 2;
			continue;
		} else if (*p == '\\' && p[1] == '\\')
			*d++ = *p++;
		else if (*p == c) {
			*d = '\0';
			return (p + 1);
		}
		*d++ = *p++;
	}
	return (NULL);
}


/* compile_ccl: expand a POSIX character class */
static char *
compile_ccl(sp, t)
	char **sp;
	char *t;
{
	int c, d;
	char *s = *sp;

	*t++ = *s++;
	if (*s == '^')
		*t++ = *s++;
	if (*s == ']')
		*t++ = *s++;
	for (; *s && (*t = *s) != ']'; s++, t++)
		if (*s == '[' && ((d = *(s+1)) == '.' || d == ':' || d == '=')) {
			*++t = *++s, t++, s++;
			for (c = *s; (*t = *s) != ']' || c != d; s++, t++)
				if ((c = *s) == '\0')
					return NULL;
		} else if (*s == '\\' && s[1] == 'n')
			    *t = '\n', s++;
	return (*s == ']') ? *sp = ++s, ++t : NULL;
}

/*
 * Get a regular expression.  P points to the delimiter of the regular
 * expression; repp points to the address of a regexp pointer.  Newline
 * and delimiter escapes are processed; other escapes are ignored.
 * Returns a pointer to the first character after the final delimiter
 * or NULL in the case of a non terminated regular expression.  The regexp
 * pointer is set to the compiled regular expression.
 * Cflags are passed to regcomp.
 */
static char *
compile_re(p, repp)
	char *p;
	regex_t **repp;
{
	int eval;
	char re[_POSIX2_LINE_MAX + 1];

	p = compile_delimited(p, re);
	if (p && strlen(re) == 0) {
		*repp = NULL;
		return (p);
	}
	*repp = xmalloc(sizeof(regex_t));
	if (p && (eval = regcomp(*repp, re, 0)) != 0)
		err(COMPILE, "RE error: %s", strregerror(eval, *repp));
	if (maxnsub < (*repp)->re_nsub)
		maxnsub = (*repp)->re_nsub;
	return (p);
}

/*
 * Compile the substitution string of a regular expression and set res to
 * point to a saved copy of it.  Nsub is the number of parenthesized regular
 * expressions.
 */
static char *
compile_subst(p, s)
	char *p;
	struct s_subst *s;
{
	static char lbuf[_POSIX2_LINE_MAX + 1];
	int asize, ref, size;
	char c, *text, *op, *sp;

	c = *p++;			/* Terminator character */
	if (c == '\0')
		return (NULL);

	s->maxbref = 0;
	s->linenum = linenum;
	asize = 2 * _POSIX2_LINE_MAX + 1;
	text = xmalloc(asize);
	size = 0;
	do {
		op = sp = text + size;
		for (; *p; p++) {
			if (*p == '\\') {
				p++;
				if (strchr("123456789", *p) != NULL) {
					*sp++ = '\\';
					ref = *p - '0';
					if (s->re != NULL &&
					    ref > s->re->re_nsub)
						err(COMPILE,
"\\%c not defined in the RE", *p);
					if (s->maxbref < ref)
						s->maxbref = ref;
				} else if (*p == '&' || *p == '\\')
					*sp++ = '\\';
			} else if (*p == c) {
				p++;
				*sp++ = '\0';
				size += sp - op;
				s->new = xrealloc(text, size);
				return (p);
			} else if (*p == '\n') {
				err(COMPILE,
"unescaped newline inside substitute pattern");
				/* NOTREACHED */
			}
			*sp++ = *p;
		}
		size += sp - op;
		if (asize - size < _POSIX2_LINE_MAX + 1) {
			asize *= 2;
			text = xmalloc(asize);
		}
	} while (cu_fgets(p = lbuf, sizeof(lbuf)));
	err(COMPILE, "unterminated substitute in regular expression");
	/* NOTREACHED */
}

/*
 * Compile the flags of the s command
 */
static char *
compile_flags(p, s)
	char *p;
	struct s_subst *s;
{
	int gn;			/* True if we have seen g or n */
	char wfile[_POSIX2_LINE_MAX + 1], *q;

	s->n = 1;				/* Default */
	s->p = 0;
	s->wfile = NULL;
	s->wfd = -1;
	for (gn = 0;;) {
		EATSPACE();			/* EXTENSION */
		switch (*p) {
		case 'g':
			if (gn)
				err(COMPILE,
"more than one number or 'g' in substitute flags");
			gn = 1;
			s->n = 0;
			break;
		case '\0':
		case '\n':
		case ';':
			return (p);
		case 'p':
			s->p = 1;
			break;
		case '1': case '2': case '3':
		case '4': case '5': case '6':
		case '7': case '8': case '9':
			if (gn)
				err(COMPILE,
"more than one number or 'g' in substitute flags");
			gn = 1;
			/* XXX Check for overflow */
			s->n = (int)strtol(p, &p, 10);
			break;
		case 'w':
			p++;
#ifdef HISTORIC_PRACTICE
			if (*p != ' ') {
				err(WARNING, "space missing before w wfile");
				return (p);
			}
#endif
			EATSPACE();
			q = wfile;
			while (*p) {
				if (*p == '\n')
					break;
				*q++ = *p++;
			}
			*q = '\0';
			if (q == wfile)
				err(COMPILE, "no wfile specified");
			s->wfile = strdup(wfile);
			if (!aflag && (s->wfd = open(wfile,
			    O_WRONLY|O_APPEND|O_CREAT|O_TRUNC,
			    DEFFILEMODE)) == -1)
				err(FATAL, "%s: %s\n", wfile, strerror(errno));
			return (p);
		default:
			err(COMPILE,
			    "bad flag in substitute command: '%c'", *p);
			break;
		}
		p++;
	}
}

/*
 * Compile a translation set of strings into a lookup table.
 */
static char *
compile_tr(p, transtab)
	char *p;
	char **transtab;
{
	int i;
	char *lt, *op, *np;
	char old[_POSIX2_LINE_MAX + 1];
	char new[_POSIX2_LINE_MAX + 1];

	if (*p == '\0' || *p == '\\')
		err(COMPILE,
"transform pattern can not be delimited by newline or backslash");
	p = compile_delimited(p, old);
	if (p == NULL) {
		err(COMPILE, "unterminated transform source string");
		return (NULL);
	}
	p = compile_delimited(--p, new);
	if (p == NULL) {
		err(COMPILE, "unterminated transform target string");
		return (NULL);
	}
	EATSPACE();
	if (strlen(new) != strlen(old)) {
		err(COMPILE, "transform strings are not the same length");
		return (NULL);
	}
	/* We assume characters are 8 bits */
	lt = xmalloc(UCHAR_MAX);
	for (i = 0; i <= UCHAR_MAX; i++)
		lt[i] = (char)i;
	for (op = old, np = new; *op; op++, np++)
		lt[(u_char)*op] = *np;
	*transtab = lt;
	return (p);
}

/*
 * Compile the text following an a or i command.
 */
static char *
compile_text()
{
	int asize, size;
	char *text, *p, *op, *s;
	char lbuf[_POSIX2_LINE_MAX + 1];

	asize = 2 * _POSIX2_LINE_MAX + 1;
	text = xmalloc(asize);
	size = 0;
	while (cu_fgets(lbuf, sizeof(lbuf))) {
		op = s = text + size;
		p = lbuf;
		EATSPACE();
		for (; *p; p++) {
			if (*p == '\\')
				p++;
			*s++ = *p;
		}
		size += s - op;
		if (p[-2] != '\\') {
			*s = '\0';
			break;
		}
		if (asize - size < _POSIX2_LINE_MAX + 1) {
			asize *= 2;
			text = xmalloc(asize);
		}
	}
	return (xrealloc(text, size + 1));
}

/*
 * Get an address and return a pointer to the first character after
 * it.  Fill the structure pointed to according to the address.
 */
static char *
compile_addr(p, a)
	char *p;
	struct s_addr *a;
{
	char *end;

	switch (*p) {
	case '\\':				/* Context address */
		++p;
		/* FALLTHROUGH */
	case '/':				/* Context address */
		p = compile_re(p, &a->u.r);
		if (p == NULL)
			err(COMPILE, "unterminated regular expression");
		a->type = AT_RE;
		return (p);

	case '$':				/* Last line */
		a->type = AT_LAST;
		return (p + 1);
						/* Line number */
	case '0': case '1': case '2': case '3': case '4': 
	case '5': case '6': case '7': case '8': case '9':
		a->type = AT_LINE;
		a->u.l = strtol(p, &end, 10);
		return (end);
	default:
		err(COMPILE, "expected context address");
		return (NULL);
	}
}

/*
 * duptoeol --
 *	Return a copy of all the characters up to \n or \0.
 */
static char *
duptoeol(s, ctype)
	register char *s;
	char *ctype;
{
	size_t len;
	int ws;
	char *start;

	ws = 0;
	for (start = s; *s != '\0' && *s != '\n'; ++s)
		ws = isspace(*s);
	*s = '\0';
	if (ws)
		err(WARNING, "whitespace after %s", ctype);
	len = s - start + 1;
	return (memmove(xmalloc(len), start, len));
}

/*
 * Convert goto label names to addresses, and count a and r commands, in
 * the given subset of the script.  Free the memory used by labels in b
 * and t commands (but not by :).
 *
 * TODO: Remove } nodes
 */
static void
fixuplabel(cp, end)
	struct s_command *cp, *end;
{

	for (; cp != end; cp = cp->next)
		switch (cp->code) {
		case 'a':
		case 'r':
			appendnum++;
			break;
		case 'b':
		case 't':
			/* Resolve branch target. */
			if (cp->t == NULL) {
				cp->u.c = NULL;
				break;
			}
			if ((cp->u.c = findlabel(cp->t)) == NULL)
				err(COMPILE2, "undefined label '%s'", cp->t);
			free(cp->t);
			break;
		case '{':
			/* Do interior commands. */
			fixuplabel(cp->u.c, cp->next);
			break;
		}
}

/*
 * Associate the given command label for later lookup.
 */
static void
enterlabel(cp)
	struct s_command *cp;
{
	register struct labhash **lhp, *lh;
	register u_char *p;
	register u_int h, c;

	for (h = 0, p = (u_char *)cp->t; (c = *p) != 0; p++)
		h = (h << 5) + h + c;
	lhp = &labels[h & LHMASK];
	for (lh = *lhp; lh != NULL; lh = lh->lh_next)
		if (lh->lh_hash == h && strcmp(cp->t, lh->lh_cmd->t) == 0)
			err(COMPILE2, "duplicate label '%s'", cp->t);
	lh = xmalloc(sizeof *lh);
	lh->lh_next = *lhp;
	lh->lh_hash = h;
	lh->lh_cmd = cp;
	lh->lh_ref = 0;
	*lhp = lh;
}

/*
 * Find the label contained in the command l in the command linked
 * list cp.  L is excluded from the search.  Return NULL if not found.
 */
static struct s_command *
findlabel(name)
	char *name;
{
	register struct labhash *lh;
	register u_char *p;
	register u_int h, c;

	for (h = 0, p = (u_char *)name; (c = *p) != 0; p++)
		h = (h << 5) + h + c;
	for (lh = labels[h & LHMASK]; lh != NULL; lh = lh->lh_next) {
		if (lh->lh_hash == h && strcmp(name, lh->lh_cmd->t) == 0) {
			lh->lh_ref = 1;
			return (lh->lh_cmd);
		}
	}
	return (NULL);
}

/* 
 * Warn about any unused labels.  As a side effect, release the label hash
 * table space.
 */
static void
uselabel()
{
	register struct labhash *lh, *next;
	register int i;

	for (i = 0; i < LHSZ; i++) {
		for (lh = labels[i]; lh != NULL; lh = next) {
			next = lh->lh_next;
			if (!lh->lh_ref)
				err(WARNING, "unused label '%s'",
				    lh->lh_cmd->t);
			free(lh);
		}
	}
}
