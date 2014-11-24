/*	$Id: roff.c,v 1.224 2014/08/01 17:27:44 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010-2014 Ingo Schwarze <schwarze@openbsd.org>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "libroff.h"
#include "libmandoc.h"

/* Maximum number of nested if-else conditionals. */
#define	RSTACK_MAX	128

/* Maximum number of string expansions per line, to break infinite loops. */
#define	EXPAND_LIMIT	1000

enum	rofft {
	ROFF_ad,
	ROFF_am,
	ROFF_ami,
	ROFF_am1,
	ROFF_as,
	ROFF_cc,
	ROFF_ce,
	ROFF_de,
	ROFF_dei,
	ROFF_de1,
	ROFF_ds,
	ROFF_el,
	ROFF_fam,
	ROFF_hw,
	ROFF_hy,
	ROFF_ie,
	ROFF_if,
	ROFF_ig,
	ROFF_it,
	ROFF_ne,
	ROFF_nh,
	ROFF_nr,
	ROFF_ns,
	ROFF_ps,
	ROFF_rm,
	ROFF_rr,
	ROFF_so,
	ROFF_ta,
	ROFF_tr,
	ROFF_Dd,
	ROFF_TH,
	ROFF_TS,
	ROFF_TE,
	ROFF_T_,
	ROFF_EQ,
	ROFF_EN,
	ROFF_cblock,
	ROFF_USERDEF,
	ROFF_MAX
};

/*
 * An incredibly-simple string buffer.
 */
struct	roffstr {
	char		*p; /* nil-terminated buffer */
	size_t		 sz; /* saved strlen(p) */
};

/*
 * A key-value roffstr pair as part of a singly-linked list.
 */
struct	roffkv {
	struct roffstr	 key;
	struct roffstr	 val;
	struct roffkv	*next; /* next in list */
};

/*
 * A single number register as part of a singly-linked list.
 */
struct	roffreg {
	struct roffstr	 key;
	int		 val;
	struct roffreg	*next;
};

struct	roff {
	struct mparse	*parse; /* parse point */
	struct roffnode	*last; /* leaf of stack */
	int		*rstack; /* stack of inverted `ie' values */
	struct roffreg	*regtab; /* number registers */
	struct roffkv	*strtab; /* user-defined strings & macros */
	struct roffkv	*xmbtab; /* multi-byte trans table (`tr') */
	struct roffstr	*xtab; /* single-byte trans table (`tr') */
	const char	*current_string; /* value of last called user macro */
	struct tbl_node	*first_tbl; /* first table parsed */
	struct tbl_node	*last_tbl; /* last table parsed */
	struct tbl_node	*tbl; /* current table being parsed */
	struct eqn_node	*last_eqn; /* last equation parsed */
	struct eqn_node	*first_eqn; /* first equation parsed */
	struct eqn_node	*eqn; /* current equation being parsed */
	int		 options; /* parse options */
	int		 rstacksz; /* current size limit of rstack */
	int		 rstackpos; /* position in rstack */
	char		 control; /* control character */
};

struct	roffnode {
	enum rofft	 tok; /* type of node */
	struct roffnode	*parent; /* up one in stack */
	int		 line; /* parse line */
	int		 col; /* parse col */
	char		*name; /* node name, e.g. macro name */
	char		*end; /* end-rules: custom token */
	int		 endspan; /* end-rules: next-line or infty */
	int		 rule; /* current evaluation rule */
};

#define	ROFF_ARGS	 struct roff *r, /* parse ctx */ \
			 enum rofft tok, /* tok of macro */ \
			 char **bufp, /* input buffer */ \
			 size_t *szp, /* size of input buffer */ \
			 int ln, /* parse line */ \
			 int ppos, /* original pos in buffer */ \
			 int pos, /* current pos in buffer */ \
			 int *offs /* reset offset of buffer data */

typedef	enum rofferr (*roffproc)(ROFF_ARGS);

struct	roffmac {
	const char	*name; /* macro name */
	roffproc	 proc; /* process new macro */
	roffproc	 text; /* process as child text of macro */
	roffproc	 sub; /* process as child of macro */
	int		 flags;
#define	ROFFMAC_STRUCT	(1 << 0) /* always interpret */
	struct roffmac	*next;
};

struct	predef {
	const char	*name; /* predefined input name */
	const char	*str; /* replacement symbol */
};

#define	PREDEF(__name, __str) \
	{ (__name), (__str) },

static	enum rofft	 roffhash_find(const char *, size_t);
static	void		 roffhash_init(void);
static	void		 roffnode_cleanscope(struct roff *);
static	void		 roffnode_pop(struct roff *);
static	void		 roffnode_push(struct roff *, enum rofft,
				const char *, int, int);
static	enum rofferr	 roff_block(ROFF_ARGS);
static	enum rofferr	 roff_block_text(ROFF_ARGS);
static	enum rofferr	 roff_block_sub(ROFF_ARGS);
static	enum rofferr	 roff_cblock(ROFF_ARGS);
static	enum rofferr	 roff_cc(ROFF_ARGS);
static	void		 roff_ccond(struct roff *, int, int);
static	enum rofferr	 roff_cond(ROFF_ARGS);
static	enum rofferr	 roff_cond_text(ROFF_ARGS);
static	enum rofferr	 roff_cond_sub(ROFF_ARGS);
static	enum rofferr	 roff_ds(ROFF_ARGS);
static	int		 roff_evalcond(const char *, int *);
static	int		 roff_evalnum(const char *, int *, int *, int);
static	int		 roff_evalpar(const char *, int *, int *);
static	int		 roff_evalstrcond(const char *, int *);
static	void		 roff_free1(struct roff *);
static	void		 roff_freereg(struct roffreg *);
static	void		 roff_freestr(struct roffkv *);
static	size_t		 roff_getname(struct roff *, char **, int, int);
static	int		 roff_getnum(const char *, int *, int *);
static	int		 roff_getop(const char *, int *, char *);
static	int		 roff_getregn(const struct roff *,
				const char *, size_t);
static	int		 roff_getregro(const char *name);
static	const char	*roff_getstrn(const struct roff *,
				const char *, size_t);
static	enum rofferr	 roff_it(ROFF_ARGS);
static	enum rofferr	 roff_line_ignore(ROFF_ARGS);
static	enum rofferr	 roff_nr(ROFF_ARGS);
static	void		 roff_openeqn(struct roff *, const char *,
				int, int, const char *);
static	enum rofft	 roff_parse(struct roff *, char *, int *,
				int, int);
static	enum rofferr	 roff_parsetext(char **, size_t *, int, int *);
static	enum rofferr	 roff_res(struct roff *,
				char **, size_t *, int, int);
static	enum rofferr	 roff_rm(ROFF_ARGS);
static	enum rofferr	 roff_rr(ROFF_ARGS);
static	void		 roff_setstr(struct roff *,
				const char *, const char *, int);
static	void		 roff_setstrn(struct roffkv **, const char *,
				size_t, const char *, size_t, int);
static	enum rofferr	 roff_so(ROFF_ARGS);
static	enum rofferr	 roff_tr(ROFF_ARGS);
static	enum rofferr	 roff_Dd(ROFF_ARGS);
static	enum rofferr	 roff_TH(ROFF_ARGS);
static	enum rofferr	 roff_TE(ROFF_ARGS);
static	enum rofferr	 roff_TS(ROFF_ARGS);
static	enum rofferr	 roff_EQ(ROFF_ARGS);
static	enum rofferr	 roff_EN(ROFF_ARGS);
static	enum rofferr	 roff_T_(ROFF_ARGS);
static	enum rofferr	 roff_userdef(ROFF_ARGS);

/* See roffhash_find() */

#define	ASCII_HI	 126
#define	ASCII_LO	 33
#define	HASHWIDTH	(ASCII_HI - ASCII_LO + 1)

static	struct roffmac	*hash[HASHWIDTH];

static	struct roffmac	 roffs[ROFF_MAX] = {
	{ "ad", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "am", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "ami", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "am1", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "as", roff_ds, NULL, NULL, 0, NULL },
	{ "cc", roff_cc, NULL, NULL, 0, NULL },
	{ "ce", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "de", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "dei", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "de1", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "ds", roff_ds, NULL, NULL, 0, NULL },
	{ "el", roff_cond, roff_cond_text, roff_cond_sub, ROFFMAC_STRUCT, NULL },
	{ "fam", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hw", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "hy", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "ie", roff_cond, roff_cond_text, roff_cond_sub, ROFFMAC_STRUCT, NULL },
	{ "if", roff_cond, roff_cond_text, roff_cond_sub, ROFFMAC_STRUCT, NULL },
	{ "ig", roff_block, roff_block_text, roff_block_sub, 0, NULL },
	{ "it", roff_it, NULL, NULL, 0, NULL },
	{ "ne", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "nh", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "nr", roff_nr, NULL, NULL, 0, NULL },
	{ "ns", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "ps", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "rm", roff_rm, NULL, NULL, 0, NULL },
	{ "rr", roff_rr, NULL, NULL, 0, NULL },
	{ "so", roff_so, NULL, NULL, 0, NULL },
	{ "ta", roff_line_ignore, NULL, NULL, 0, NULL },
	{ "tr", roff_tr, NULL, NULL, 0, NULL },
	{ "Dd", roff_Dd, NULL, NULL, 0, NULL },
	{ "TH", roff_TH, NULL, NULL, 0, NULL },
	{ "TS", roff_TS, NULL, NULL, 0, NULL },
	{ "TE", roff_TE, NULL, NULL, 0, NULL },
	{ "T&", roff_T_, NULL, NULL, 0, NULL },
	{ "EQ", roff_EQ, NULL, NULL, 0, NULL },
	{ "EN", roff_EN, NULL, NULL, 0, NULL },
	{ ".", roff_cblock, NULL, NULL, 0, NULL },
	{ NULL, roff_userdef, NULL, NULL, 0, NULL },
};

/* not currently implemented: Ds em Eq LP Me PP pp Or Rd Sf SH */
const	char *const __mdoc_reserved[] = {
	"Ac", "Ad", "An", "Ao", "Ap", "Aq", "Ar", "At",
	"Bc", "Bd", "Bf", "Bk", "Bl", "Bo", "Bq",
	"Brc", "Bro", "Brq", "Bsx", "Bt", "Bx",
	"Cd", "Cm", "Db", "Dc", "Dd", "Dl", "Do", "Dq",
	"Dt", "Dv", "Dx", "D1",
	"Ec", "Ed", "Ef", "Ek", "El", "Em",
	"En", "Eo", "Er", "Es", "Ev", "Ex",
	"Fa", "Fc", "Fd", "Fl", "Fn", "Fo", "Fr", "Ft", "Fx",
	"Hf", "Ic", "In", "It", "Lb", "Li", "Lk", "Lp",
	"Ms", "Mt", "Nd", "Nm", "No", "Ns", "Nx",
	"Oc", "Oo", "Op", "Os", "Ot", "Ox",
	"Pa", "Pc", "Pf", "Po", "Pp", "Pq",
	"Qc", "Ql", "Qo", "Qq", "Re", "Rs", "Rv",
	"Sc", "Sh", "Sm", "So", "Sq",
	"Ss", "St", "Sx", "Sy",
	"Ta", "Tn", "Ud", "Ux", "Va", "Vt", "Xc", "Xo", "Xr",
	"%A", "%B", "%C", "%D", "%I", "%J", "%N", "%O",
	"%P", "%Q", "%R", "%T", "%U", "%V",
	NULL
};

/* not currently implemented: BT DE DS ME MT PT SY TQ YS */
const	char *const __man_reserved[] = {
	"AT", "B", "BI", "BR", "DT",
	"EE", "EN", "EQ", "EX", "HP", "I", "IB", "IP", "IR",
	"LP", "OP", "P", "PD", "PP",
	"R", "RB", "RE", "RI", "RS", "SB", "SH", "SM", "SS",
	"TE", "TH", "TP", "TS", "T&", "UC", "UE", "UR",
	NULL
};

/* Array of injected predefined strings. */
#define	PREDEFS_MAX	 38
static	const struct predef predefs[PREDEFS_MAX] = {
#include "predefs.in"
};

/* See roffhash_find() */
#define	ROFF_HASH(p)	(p[0] - ASCII_LO)

static	int	 roffit_lines;  /* number of lines to delay */
static	char	*roffit_macro;  /* nil-terminated macro line */


static void
roffhash_init(void)
{
	struct roffmac	 *n;
	int		  buc, i;

	for (i = 0; i < (int)ROFF_USERDEF; i++) {
		assert(roffs[i].name[0] >= ASCII_LO);
		assert(roffs[i].name[0] <= ASCII_HI);

		buc = ROFF_HASH(roffs[i].name);

		if (NULL != (n = hash[buc])) {
			for ( ; n->next; n = n->next)
				/* Do nothing. */ ;
			n->next = &roffs[i];
		} else
			hash[buc] = &roffs[i];
	}
}

/*
 * Look up a roff token by its name.  Returns ROFF_MAX if no macro by
 * the nil-terminated string name could be found.
 */
static enum rofft
roffhash_find(const char *p, size_t s)
{
	int		 buc;
	struct roffmac	*n;

	/*
	 * libroff has an extremely simple hashtable, for the time
	 * being, which simply keys on the first character, which must
	 * be printable, then walks a chain.  It works well enough until
	 * optimised.
	 */

	if (p[0] < ASCII_LO || p[0] > ASCII_HI)
		return(ROFF_MAX);

	buc = ROFF_HASH(p);

	if (NULL == (n = hash[buc]))
		return(ROFF_MAX);
	for ( ; n; n = n->next)
		if (0 == strncmp(n->name, p, s) && '\0' == n->name[(int)s])
			return((enum rofft)(n - roffs));

	return(ROFF_MAX);
}

/*
 * Pop the current node off of the stack of roff instructions currently
 * pending.
 */
static void
roffnode_pop(struct roff *r)
{
	struct roffnode	*p;

	assert(r->last);
	p = r->last;

	r->last = r->last->parent;
	free(p->name);
	free(p->end);
	free(p);
}

/*
 * Push a roff node onto the instruction stack.  This must later be
 * removed with roffnode_pop().
 */
static void
roffnode_push(struct roff *r, enum rofft tok, const char *name,
		int line, int col)
{
	struct roffnode	*p;

	p = mandoc_calloc(1, sizeof(struct roffnode));
	p->tok = tok;
	if (name)
		p->name = mandoc_strdup(name);
	p->parent = r->last;
	p->line = line;
	p->col = col;
	p->rule = p->parent ? p->parent->rule : 0;

	r->last = p;
}

static void
roff_free1(struct roff *r)
{
	struct tbl_node	*tbl;
	struct eqn_node	*e;
	int		 i;

	while (NULL != (tbl = r->first_tbl)) {
		r->first_tbl = tbl->next;
		tbl_free(tbl);
	}
	r->first_tbl = r->last_tbl = r->tbl = NULL;

	while (NULL != (e = r->first_eqn)) {
		r->first_eqn = e->next;
		eqn_free(e);
	}
	r->first_eqn = r->last_eqn = r->eqn = NULL;

	while (r->last)
		roffnode_pop(r);

	free (r->rstack);
	r->rstack = NULL;
	r->rstacksz = 0;
	r->rstackpos = -1;

	roff_freereg(r->regtab);
	r->regtab = NULL;

	roff_freestr(r->strtab);
	roff_freestr(r->xmbtab);
	r->strtab = r->xmbtab = NULL;

	if (r->xtab)
		for (i = 0; i < 128; i++)
			free(r->xtab[i].p);
	free(r->xtab);
	r->xtab = NULL;
}

void
roff_reset(struct roff *r)
{

	roff_free1(r);
	r->control = 0;
}

void
roff_free(struct roff *r)
{

	roff_free1(r);
	free(r);
}

struct roff *
roff_alloc(struct mparse *parse, int options)
{
	struct roff	*r;

	r = mandoc_calloc(1, sizeof(struct roff));
	r->parse = parse;
	r->options = options;
	r->rstackpos = -1;

	roffhash_init();

	return(r);
}

/*
 * In the current line, expand escape sequences that tend to get
 * used in numerical expressions and conditional requests.
 * Also check the syntax of the remaining escape sequences.
 */
static enum rofferr
roff_res(struct roff *r, char **bufp, size_t *szp, int ln, int pos)
{
	char		 ubuf[24]; /* buffer to print the number */
	const char	*start;	/* start of the string to process */
	char		*stesc;	/* start of an escape sequence ('\\') */
	const char	*stnam;	/* start of the name, after "[(*" */
	const char	*cp;	/* end of the name, e.g. before ']' */
	const char	*res;	/* the string to be substituted */
	char		*nbuf;	/* new buffer to copy bufp to */
	size_t		 maxl;  /* expected length of the escape name */
	size_t		 naml;	/* actual length of the escape name */
	int		 expand_count;	/* to avoid infinite loops */
	int		 npos;	/* position in numeric expression */
	int		 arg_complete; /* argument not interrupted by eol */
	char		 term;	/* character terminating the escape */

	expand_count = 0;
	start = *bufp + pos;
	stesc = strchr(start, '\0') - 1;
	while (stesc-- > start) {

		/* Search backwards for the next backslash. */

		if ('\\' != *stesc)
			continue;

		/* If it is escaped, skip it. */

		for (cp = stesc - 1; cp >= start; cp--)
			if ('\\' != *cp)
				break;

		if (0 == (stesc - cp) % 2) {
			stesc = (char *)cp;
			continue;
		}

		/* Decide whether to expand or to check only. */

		term = '\0';
		cp = stesc + 1;
		switch (*cp) {
		case '*':
			res = NULL;
			break;
		case 'B':
			/* FALLTHROUGH */
		case 'w':
			term = cp[1];
			/* FALLTHROUGH */
		case 'n':
			res = ubuf;
			break;
		default:
			if (ESCAPE_ERROR == mandoc_escape(&cp, NULL, NULL))
				mandoc_vmsg(MANDOCERR_ESC_BAD,
				    r->parse, ln, (int)(stesc - *bufp),
				    "%.*s", (int)(cp - stesc), stesc);
			continue;
		}

		if (EXPAND_LIMIT < ++expand_count) {
			mandoc_msg(MANDOCERR_ROFFLOOP, r->parse,
			    ln, (int)(stesc - *bufp), NULL);
			return(ROFF_IGN);
		}

		/*
		 * The third character decides the length
		 * of the name of the string or register.
		 * Save a pointer to the name.
		 */

		if ('\0' == term) {
			switch (*++cp) {
			case '\0':
				maxl = 0;
				break;
			case '(':
				cp++;
				maxl = 2;
				break;
			case '[':
				cp++;
				term = ']';
				maxl = 0;
				break;
			default:
				maxl = 1;
				break;
			}
		} else {
			cp += 2;
			maxl = 0;
		}
		stnam = cp;

		/* Advance to the end of the name. */

		arg_complete = 1;
		for (naml = 0; 0 == maxl || naml < maxl; naml++, cp++) {
			if ('\0' == *cp) {
				mandoc_msg(MANDOCERR_ESC_BAD, r->parse,
				    ln, (int)(stesc - *bufp), stesc);
				arg_complete = 0;
				break;
			}
			if (0 == maxl && *cp == term) {
				cp++;
				break;
			}
		}

		/*
		 * Retrieve the replacement string; if it is
		 * undefined, resume searching for escapes.
		 */

		switch (stesc[1]) {
		case '*':
			if (arg_complete)
				res = roff_getstrn(r, stnam, naml);
			break;
		case 'B':
			npos = 0;
			ubuf[0] = arg_complete &&
			    roff_evalnum(stnam, &npos, NULL, 0) &&
			    stnam + npos + 1 == cp ? '1' : '0';
			ubuf[1] = '\0';
			break;
		case 'n':
			if (arg_complete)
				(void)snprintf(ubuf, sizeof(ubuf), "%d",
				    roff_getregn(r, stnam, naml));
			else
				ubuf[0] = '\0';
			break;
		case 'w':
			/* use even incomplete args */
			(void)snprintf(ubuf, sizeof(ubuf), "%d",
			    24 * (int)naml);
			break;
		}

		if (NULL == res) {
			mandoc_vmsg(MANDOCERR_STR_UNDEF,
			    r->parse, ln, (int)(stesc - *bufp),
			    "%.*s", (int)naml, stnam);
			res = "";
		}

		/* Replace the escape sequence by the string. */

		*stesc = '\0';
		*szp = mandoc_asprintf(&nbuf, "%s%s%s",
		    *bufp, res, cp) + 1;

		/* Prepare for the next replacement. */

		start = nbuf + pos;
		stesc = nbuf + (stesc - *bufp) + strlen(res);
		free(*bufp);
		*bufp = nbuf;
	}
	return(ROFF_CONT);
}

/*
 * Process text streams:
 * Convert all breakable hyphens into ASCII_HYPH.
 * Decrement and spring input line trap.
 */
static enum rofferr
roff_parsetext(char **bufp, size_t *szp, int pos, int *offs)
{
	size_t		 sz;
	const char	*start;
	char		*p;
	int		 isz;
	enum mandoc_esc	 esc;

	start = p = *bufp + pos;

	while ('\0' != *p) {
		sz = strcspn(p, "-\\");
		p += sz;

		if ('\0' == *p)
			break;

		if ('\\' == *p) {
			/* Skip over escapes. */
			p++;
			esc = mandoc_escape((const char **)&p, NULL, NULL);
			if (ESCAPE_ERROR == esc)
				break;
			continue;
		} else if (p == start) {
			p++;
			continue;
		}

		if (isalpha((unsigned char)p[-1]) &&
		    isalpha((unsigned char)p[1]))
			*p = ASCII_HYPH;
		p++;
	}

	/* Spring the input line trap. */
	if (1 == roffit_lines) {
		isz = mandoc_asprintf(&p, "%s\n.%s", *bufp, roffit_macro);
		free(*bufp);
		*bufp = p;
		*szp = isz + 1;
		*offs = 0;
		free(roffit_macro);
		roffit_lines = 0;
		return(ROFF_REPARSE);
	} else if (1 < roffit_lines)
		--roffit_lines;
	return(ROFF_CONT);
}

enum rofferr
roff_parseln(struct roff *r, int ln, char **bufp,
		size_t *szp, int pos, int *offs)
{
	enum rofft	 t;
	enum rofferr	 e;
	int		 ppos, ctl;

	/*
	 * Run the reserved-word filter only if we have some reserved
	 * words to fill in.
	 */

	e = roff_res(r, bufp, szp, ln, pos);
	if (ROFF_IGN == e)
		return(e);
	assert(ROFF_CONT == e);

	ppos = pos;
	ctl = roff_getcontrol(r, *bufp, &pos);

	/*
	 * First, if a scope is open and we're not a macro, pass the
	 * text through the macro's filter.  If a scope isn't open and
	 * we're not a macro, just let it through.
	 * Finally, if there's an equation scope open, divert it into it
	 * no matter our state.
	 */

	if (r->last && ! ctl) {
		t = r->last->tok;
		assert(roffs[t].text);
		e = (*roffs[t].text)(r, t, bufp, szp, ln, pos, pos, offs);
		assert(ROFF_IGN == e || ROFF_CONT == e);
		if (ROFF_CONT != e)
			return(e);
	}
	if (r->eqn)
		return(eqn_read(&r->eqn, ln, *bufp, ppos, offs));
	if ( ! ctl) {
		if (r->tbl)
			return(tbl_read(r->tbl, ln, *bufp, pos));
		return(roff_parsetext(bufp, szp, pos, offs));
	}

	/*
	 * If a scope is open, go to the child handler for that macro,
	 * as it may want to preprocess before doing anything with it.
	 * Don't do so if an equation is open.
	 */

	if (r->last) {
		t = r->last->tok;
		assert(roffs[t].sub);
		return((*roffs[t].sub)(r, t, bufp, szp,
		    ln, ppos, pos, offs));
	}

	/*
	 * Lastly, as we've no scope open, try to look up and execute
	 * the new macro.  If no macro is found, simply return and let
	 * the compilers handle it.
	 */

	if (ROFF_MAX == (t = roff_parse(r, *bufp, &pos, ln, ppos)))
		return(ROFF_CONT);

	assert(roffs[t].proc);
	return((*roffs[t].proc)(r, t, bufp, szp, ln, ppos, pos, offs));
}

void
roff_endparse(struct roff *r)
{

	if (r->last)
		mandoc_msg(MANDOCERR_BLK_NOEND, r->parse,
		    r->last->line, r->last->col,
		    roffs[r->last->tok].name);

	if (r->eqn) {
		mandoc_msg(MANDOCERR_BLK_NOEND, r->parse,
		    r->eqn->eqn.ln, r->eqn->eqn.pos, "EQ");
		eqn_end(&r->eqn);
	}

	if (r->tbl) {
		mandoc_msg(MANDOCERR_BLK_NOEND, r->parse,
		    r->tbl->line, r->tbl->pos, "TS");
		tbl_end(&r->tbl);
	}
}

/*
 * Parse a roff node's type from the input buffer.  This must be in the
 * form of ".foo xxx" in the usual way.
 */
static enum rofft
roff_parse(struct roff *r, char *buf, int *pos, int ln, int ppos)
{
	char		*cp;
	const char	*mac;
	size_t		 maclen;
	enum rofft	 t;

	cp = buf + *pos;

	if ('\0' == *cp || '"' == *cp || '\t' == *cp || ' ' == *cp)
		return(ROFF_MAX);

	mac = cp;
	maclen = roff_getname(r, &cp, ln, ppos);

	t = (r->current_string = roff_getstrn(r, mac, maclen))
	    ? ROFF_USERDEF : roffhash_find(mac, maclen);

	if (ROFF_MAX != t)
		*pos = cp - buf;

	return(t);
}

static enum rofferr
roff_cblock(ROFF_ARGS)
{

	/*
	 * A block-close `..' should only be invoked as a child of an
	 * ignore macro, otherwise raise a warning and just ignore it.
	 */

	if (NULL == r->last) {
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "..");
		return(ROFF_IGN);
	}

	switch (r->last->tok) {
	case ROFF_am:
		/* ROFF_am1 is remapped to ROFF_am in roff_block(). */
		/* FALLTHROUGH */
	case ROFF_ami:
		/* FALLTHROUGH */
	case ROFF_de:
		/* ROFF_de1 is remapped to ROFF_de in roff_block(). */
		/* FALLTHROUGH */
	case ROFF_dei:
		/* FALLTHROUGH */
	case ROFF_ig:
		break;
	default:
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "..");
		return(ROFF_IGN);
	}

	if ((*bufp)[pos])
		mandoc_vmsg(MANDOCERR_ARG_SKIP, r->parse, ln, pos,
		    ".. %s", *bufp + pos);

	roffnode_pop(r);
	roffnode_cleanscope(r);
	return(ROFF_IGN);

}

static void
roffnode_cleanscope(struct roff *r)
{

	while (r->last) {
		if (--r->last->endspan != 0)
			break;
		roffnode_pop(r);
	}
}

static void
roff_ccond(struct roff *r, int ln, int ppos)
{

	if (NULL == r->last) {
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "\\}");
		return;
	}

	switch (r->last->tok) {
	case ROFF_el:
		/* FALLTHROUGH */
	case ROFF_ie:
		/* FALLTHROUGH */
	case ROFF_if:
		break;
	default:
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "\\}");
		return;
	}

	if (r->last->endspan > -1) {
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "\\}");
		return;
	}

	roffnode_pop(r);
	roffnode_cleanscope(r);
	return;
}

static enum rofferr
roff_block(ROFF_ARGS)
{
	const char	*name;
	char		*iname, *cp;
	size_t		 namesz;

	/* Ignore groff compatibility mode for now. */

	if (ROFF_de1 == tok)
		tok = ROFF_de;
	else if (ROFF_am1 == tok)
		tok = ROFF_am;

	/* Parse the macro name argument. */

	cp = *bufp + pos;
	if (ROFF_ig == tok) {
		iname = NULL;
		namesz = 0;
	} else {
		iname = cp;
		namesz = roff_getname(r, &cp, ln, ppos);
		iname[namesz] = '\0';
	}

	/* Resolve the macro name argument if it is indirect. */

	if (namesz && (ROFF_dei == tok || ROFF_ami == tok)) {
		if (NULL == (name = roff_getstrn(r, iname, namesz))) {
			mandoc_vmsg(MANDOCERR_STR_UNDEF,
			    r->parse, ln, (int)(iname - *bufp),
			    "%.*s", (int)namesz, iname);
			namesz = 0;
		} else
			namesz = strlen(name);
	} else
		name = iname;

	if (0 == namesz && ROFF_ig != tok) {
		mandoc_msg(MANDOCERR_REQ_EMPTY, r->parse,
		    ln, ppos, roffs[tok].name);
		return(ROFF_IGN);
	}

	roffnode_push(r, tok, name, ln, ppos);

	/*
	 * At the beginning of a `de' macro, clear the existing string
	 * with the same name, if there is one.  New content will be
	 * appended from roff_block_text() in multiline mode.
	 */

	if (ROFF_de == tok || ROFF_dei == tok)
		roff_setstrn(&r->strtab, name, namesz, "", 0, 0);

	if ('\0' == *cp)
		return(ROFF_IGN);

	/* Get the custom end marker. */

	iname = cp;
	namesz = roff_getname(r, &cp, ln, ppos);

	/* Resolve the end marker if it is indirect. */

	if (namesz && (ROFF_dei == tok || ROFF_ami == tok)) {
		if (NULL == (name = roff_getstrn(r, iname, namesz))) {
			mandoc_vmsg(MANDOCERR_STR_UNDEF,
			    r->parse, ln, (int)(iname - *bufp),
			    "%.*s", (int)namesz, iname);
			namesz = 0;
		} else
			namesz = strlen(name);
	} else
		name = iname;

	if (namesz)
		r->last->end = mandoc_strndup(name, namesz);

	if ('\0' != *cp)
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, r->parse,
		    ln, pos, ".%s ... %s", roffs[tok].name, cp);

	return(ROFF_IGN);
}

static enum rofferr
roff_block_sub(ROFF_ARGS)
{
	enum rofft	t;
	int		i, j;

	/*
	 * First check whether a custom macro exists at this level.  If
	 * it does, then check against it.  This is some of groff's
	 * stranger behaviours.  If we encountered a custom end-scope
	 * tag and that tag also happens to be a "real" macro, then we
	 * need to try interpreting it again as a real macro.  If it's
	 * not, then return ignore.  Else continue.
	 */

	if (r->last->end) {
		for (i = pos, j = 0; r->last->end[j]; j++, i++)
			if ((*bufp)[i] != r->last->end[j])
				break;

		if ('\0' == r->last->end[j] &&
		    ('\0' == (*bufp)[i] ||
		     ' '  == (*bufp)[i] ||
		     '\t' == (*bufp)[i])) {
			roffnode_pop(r);
			roffnode_cleanscope(r);

			while (' ' == (*bufp)[i] || '\t' == (*bufp)[i])
				i++;

			pos = i;
			if (ROFF_MAX != roff_parse(r, *bufp, &pos, ln, ppos))
				return(ROFF_RERUN);
			return(ROFF_IGN);
		}
	}

	/*
	 * If we have no custom end-query or lookup failed, then try
	 * pulling it out of the hashtable.
	 */

	t = roff_parse(r, *bufp, &pos, ln, ppos);

	if (ROFF_cblock != t) {
		if (ROFF_ig != tok)
			roff_setstr(r, r->last->name, *bufp + ppos, 2);
		return(ROFF_IGN);
	}

	assert(roffs[t].proc);
	return((*roffs[t].proc)(r, t, bufp, szp, ln, ppos, pos, offs));
}

static enum rofferr
roff_block_text(ROFF_ARGS)
{

	if (ROFF_ig != tok)
		roff_setstr(r, r->last->name, *bufp + pos, 2);

	return(ROFF_IGN);
}

static enum rofferr
roff_cond_sub(ROFF_ARGS)
{
	enum rofft	 t;
	char		*ep;
	int		 rr;

	rr = r->last->rule;
	roffnode_cleanscope(r);
	t = roff_parse(r, *bufp, &pos, ln, ppos);

	/*
	 * Fully handle known macros when they are structurally
	 * required or when the conditional evaluated to true.
	 */

	if ((ROFF_MAX != t) &&
	    (rr || ROFFMAC_STRUCT & roffs[t].flags)) {
		assert(roffs[t].proc);
		return((*roffs[t].proc)(r, t, bufp, szp,
		    ln, ppos, pos, offs));
	}

	/*
	 * If `\}' occurs on a macro line without a preceding macro,
	 * drop the line completely.
	 */

	ep = *bufp + pos;
	if ('\\' == ep[0] && '}' == ep[1])
		rr = 0;

	/* Always check for the closing delimiter `\}'. */

	while (NULL != (ep = strchr(ep, '\\'))) {
		if ('}' == *(++ep)) {
			*ep = '&';
			roff_ccond(r, ln, ep - *bufp - 1);
		}
		++ep;
	}
	return(rr ? ROFF_CONT : ROFF_IGN);
}

static enum rofferr
roff_cond_text(ROFF_ARGS)
{
	char		*ep;
	int		 rr;

	rr = r->last->rule;
	roffnode_cleanscope(r);

	ep = *bufp + pos;
	while (NULL != (ep = strchr(ep, '\\'))) {
		if ('}' == *(++ep)) {
			*ep = '&';
			roff_ccond(r, ln, ep - *bufp - 1);
		}
		++ep;
	}
	return(rr ? ROFF_CONT : ROFF_IGN);
}

/*
 * Parse a single signed integer number.  Stop at the first non-digit.
 * If there is at least one digit, return success and advance the
 * parse point, else return failure and let the parse point unchanged.
 * Ignore overflows, treat them just like the C language.
 */
static int
roff_getnum(const char *v, int *pos, int *res)
{
	int	 myres, n, p;

	if (NULL == res)
		res = &myres;

	p = *pos;
	n = v[p] == '-';
	if (n)
		p++;

	for (*res = 0; isdigit((unsigned char)v[p]); p++)
		*res = 10 * *res + v[p] - '0';
	if (p == *pos + n)
		return 0;

	if (n)
		*res = -*res;

	*pos = p;
	return 1;
}

/*
 * Evaluate a string comparison condition.
 * The first character is the delimiter.
 * Succeed if the string up to its second occurrence
 * matches the string up to its third occurence.
 * Advance the cursor after the third occurrence
 * or lacking that, to the end of the line.
 */
static int
roff_evalstrcond(const char *v, int *pos)
{
	const char	*s1, *s2, *s3;
	int		 match;

	match = 0;
	s1 = v + *pos;		/* initial delimiter */
	s2 = s1 + 1;		/* for scanning the first string */
	s3 = strchr(s2, *s1);	/* for scanning the second string */

	if (NULL == s3)		/* found no middle delimiter */
		goto out;

	while ('\0' != *++s3) {
		if (*s2 != *s3) {  /* mismatch */
			s3 = strchr(s3, *s1);
			break;
		}
		if (*s3 == *s1) {  /* found the final delimiter */
			match = 1;
			break;
		}
		s2++;
	}

out:
	if (NULL == s3)
		s3 = strchr(s2, '\0');
	else
		s3++;
	*pos = s3 - v;
	return(match);
}

/*
 * Evaluate an optionally negated single character, numerical,
 * or string condition.
 */
static int
roff_evalcond(const char *v, int *pos)
{
	int	 wanttrue, number;

	if ('!' == v[*pos]) {
		wanttrue = 0;
		(*pos)++;
	} else
		wanttrue = 1;

	switch (v[*pos]) {
	case 'n':
		/* FALLTHROUGH */
	case 'o':
		(*pos)++;
		return(wanttrue);
	case 'c':
		/* FALLTHROUGH */
	case 'd':
		/* FALLTHROUGH */
	case 'e':
		/* FALLTHROUGH */
	case 'r':
		/* FALLTHROUGH */
	case 't':
		(*pos)++;
		return(!wanttrue);
	default:
		break;
	}

	if (roff_evalnum(v, pos, &number, 0))
		return((number > 0) == wanttrue);
	else
		return(roff_evalstrcond(v, pos) == wanttrue);
}

static enum rofferr
roff_line_ignore(ROFF_ARGS)
{

	return(ROFF_IGN);
}

static enum rofferr
roff_cond(ROFF_ARGS)
{

	roffnode_push(r, tok, NULL, ln, ppos);

	/*
	 * An `.el' has no conditional body: it will consume the value
	 * of the current rstack entry set in prior `ie' calls or
	 * defaults to DENY.
	 *
	 * If we're not an `el', however, then evaluate the conditional.
	 */

	r->last->rule = ROFF_el == tok ?
	    (r->rstackpos < 0 ? 0 : r->rstack[r->rstackpos--]) :
	    roff_evalcond(*bufp, &pos);

	/*
	 * An if-else will put the NEGATION of the current evaluated
	 * conditional into the stack of rules.
	 */

	if (ROFF_ie == tok) {
		if (r->rstackpos + 1 == r->rstacksz) {
			r->rstacksz += 16;
			r->rstack = mandoc_reallocarray(r->rstack,
			    r->rstacksz, sizeof(int));
		}
		r->rstack[++r->rstackpos] = !r->last->rule;
	}

	/* If the parent has false as its rule, then so do we. */

	if (r->last->parent && !r->last->parent->rule)
		r->last->rule = 0;

	/*
	 * Determine scope.
	 * If there is nothing on the line after the conditional,
	 * not even whitespace, use next-line scope.
	 */

	if ('\0' == (*bufp)[pos]) {
		r->last->endspan = 2;
		goto out;
	}

	while (' ' == (*bufp)[pos])
		pos++;

	/* An opening brace requests multiline scope. */

	if ('\\' == (*bufp)[pos] && '{' == (*bufp)[pos + 1]) {
		r->last->endspan = -1;
		pos += 2;
		goto out;
	}

	/*
	 * Anything else following the conditional causes
	 * single-line scope.  Warn if the scope contains
	 * nothing but trailing whitespace.
	 */

	if ('\0' == (*bufp)[pos])
		mandoc_msg(MANDOCERR_COND_EMPTY, r->parse,
		    ln, ppos, roffs[tok].name);

	r->last->endspan = 1;

out:
	*offs = pos;
	return(ROFF_RERUN);
}

static enum rofferr
roff_ds(ROFF_ARGS)
{
	char		*string;
	const char	*name;
	size_t		 namesz;

	/*
	 * The first word is the name of the string.
	 * If it is empty or terminated by an escape sequence,
	 * abort the `ds' request without defining anything.
	 */

	name = string = *bufp + pos;
	if ('\0' == *name)
		return(ROFF_IGN);

	namesz = roff_getname(r, &string, ln, pos);
	if ('\\' == name[namesz])
		return(ROFF_IGN);

	/* Read past the initial double-quote, if any. */
	if ('"' == *string)
		string++;

	/* The rest is the value. */
	roff_setstrn(&r->strtab, name, namesz, string, strlen(string),
	    ROFF_as == tok);
	return(ROFF_IGN);
}

/*
 * Parse a single operator, one or two characters long.
 * If the operator is recognized, return success and advance the
 * parse point, else return failure and let the parse point unchanged.
 */
static int
roff_getop(const char *v, int *pos, char *res)
{

	*res = v[*pos];

	switch (*res) {
	case '+':
		/* FALLTHROUGH */
	case '-':
		/* FALLTHROUGH */
	case '*':
		/* FALLTHROUGH */
	case '/':
		/* FALLTHROUGH */
	case '%':
		/* FALLTHROUGH */
	case '&':
		/* FALLTHROUGH */
	case ':':
		break;
	case '<':
		switch (v[*pos + 1]) {
		case '=':
			*res = 'l';
			(*pos)++;
			break;
		case '>':
			*res = '!';
			(*pos)++;
			break;
		case '?':
			*res = 'i';
			(*pos)++;
			break;
		default:
			break;
		}
		break;
	case '>':
		switch (v[*pos + 1]) {
		case '=':
			*res = 'g';
			(*pos)++;
			break;
		case '?':
			*res = 'a';
			(*pos)++;
			break;
		default:
			break;
		}
		break;
	case '=':
		if ('=' == v[*pos + 1])
			(*pos)++;
		break;
	default:
		return(0);
	}
	(*pos)++;

	return(*res);
}

/*
 * Evaluate either a parenthesized numeric expression
 * or a single signed integer number.
 */
static int
roff_evalpar(const char *v, int *pos, int *res)
{

	if ('(' != v[*pos])
		return(roff_getnum(v, pos, res));

	(*pos)++;
	if ( ! roff_evalnum(v, pos, res, 1))
		return(0);

	/*
	 * Omission of the closing parenthesis
	 * is an error in validation mode,
	 * but ignored in evaluation mode.
	 */

	if (')' == v[*pos])
		(*pos)++;
	else if (NULL == res)
		return(0);

	return(1);
}

/*
 * Evaluate a complete numeric expression.
 * Proceed left to right, there is no concept of precedence.
 */
static int
roff_evalnum(const char *v, int *pos, int *res, int skipwhite)
{
	int		 mypos, operand2;
	char		 operator;

	if (NULL == pos) {
		mypos = 0;
		pos = &mypos;
	}

	if (skipwhite)
		while (isspace((unsigned char)v[*pos]))
			(*pos)++;

	if ( ! roff_evalpar(v, pos, res))
		return(0);

	while (1) {
		if (skipwhite)
			while (isspace((unsigned char)v[*pos]))
				(*pos)++;

		if ( ! roff_getop(v, pos, &operator))
			break;

		if (skipwhite)
			while (isspace((unsigned char)v[*pos]))
				(*pos)++;

		if ( ! roff_evalpar(v, pos, &operand2))
			return(0);

		if (skipwhite)
			while (isspace((unsigned char)v[*pos]))
				(*pos)++;

		if (NULL == res)
			continue;

		switch (operator) {
		case '+':
			*res += operand2;
			break;
		case '-':
			*res -= operand2;
			break;
		case '*':
			*res *= operand2;
			break;
		case '/':
			*res /= operand2;
			break;
		case '%':
			*res %= operand2;
			break;
		case '<':
			*res = *res < operand2;
			break;
		case '>':
			*res = *res > operand2;
			break;
		case 'l':
			*res = *res <= operand2;
			break;
		case 'g':
			*res = *res >= operand2;
			break;
		case '=':
			*res = *res == operand2;
			break;
		case '!':
			*res = *res != operand2;
			break;
		case '&':
			*res = *res && operand2;
			break;
		case ':':
			*res = *res || operand2;
			break;
		case 'i':
			if (operand2 < *res)
				*res = operand2;
			break;
		case 'a':
			if (operand2 > *res)
				*res = operand2;
			break;
		default:
			abort();
		}
	}
	return(1);
}

void
roff_setreg(struct roff *r, const char *name, int val, char sign)
{
	struct roffreg	*reg;

	/* Search for an existing register with the same name. */
	reg = r->regtab;

	while (reg && strcmp(name, reg->key.p))
		reg = reg->next;

	if (NULL == reg) {
		/* Create a new register. */
		reg = mandoc_malloc(sizeof(struct roffreg));
		reg->key.p = mandoc_strdup(name);
		reg->key.sz = strlen(name);
		reg->val = 0;
		reg->next = r->regtab;
		r->regtab = reg;
	}

	if ('+' == sign)
		reg->val += val;
	else if ('-' == sign)
		reg->val -= val;
	else
		reg->val = val;
}

/*
 * Handle some predefined read-only number registers.
 * For now, return -1 if the requested register is not predefined;
 * in case a predefined read-only register having the value -1
 * were to turn up, another special value would have to be chosen.
 */
static int
roff_getregro(const char *name)
{

	switch (*name) {
	case 'A':  /* ASCII approximation mode is always off. */
		return(0);
	case 'g':  /* Groff compatibility mode is always on. */
		return(1);
	case 'H':  /* Fixed horizontal resolution. */
		return (24);
	case 'j':  /* Always adjust left margin only. */
		return(0);
	case 'T':  /* Some output device is always defined. */
		return(1);
	case 'V':  /* Fixed vertical resolution. */
		return (40);
	default:
		return (-1);
	}
}

int
roff_getreg(const struct roff *r, const char *name)
{
	struct roffreg	*reg;
	int		 val;

	if ('.' == name[0] && '\0' != name[1] && '\0' == name[2]) {
		val = roff_getregro(name + 1);
		if (-1 != val)
			return (val);
	}

	for (reg = r->regtab; reg; reg = reg->next)
		if (0 == strcmp(name, reg->key.p))
			return(reg->val);

	return(0);
}

static int
roff_getregn(const struct roff *r, const char *name, size_t len)
{
	struct roffreg	*reg;
	int		 val;

	if ('.' == name[0] && 2 == len) {
		val = roff_getregro(name + 1);
		if (-1 != val)
			return (val);
	}

	for (reg = r->regtab; reg; reg = reg->next)
		if (len == reg->key.sz &&
		    0 == strncmp(name, reg->key.p, len))
			return(reg->val);

	return(0);
}

static void
roff_freereg(struct roffreg *reg)
{
	struct roffreg	*old_reg;

	while (NULL != reg) {
		free(reg->key.p);
		old_reg = reg;
		reg = reg->next;
		free(old_reg);
	}
}

static enum rofferr
roff_nr(ROFF_ARGS)
{
	char		*key, *val;
	size_t		 keysz;
	int		 iv;
	char		 sign;

	key = val = *bufp + pos;
	if ('\0' == *key)
		return(ROFF_IGN);

	keysz = roff_getname(r, &val, ln, pos);
	if ('\\' == key[keysz])
		return(ROFF_IGN);
	key[keysz] = '\0';

	sign = *val;
	if ('+' == sign || '-' == sign)
		val++;

	if (roff_evalnum(val, NULL, &iv, 0))
		roff_setreg(r, key, iv, sign);

	return(ROFF_IGN);
}

static enum rofferr
roff_rr(ROFF_ARGS)
{
	struct roffreg	*reg, **prev;
	char		*name, *cp;
	size_t		 namesz;

	name = cp = *bufp + pos;
	if ('\0' == *name)
		return(ROFF_IGN);
	namesz = roff_getname(r, &cp, ln, pos);
	name[namesz] = '\0';

	prev = &r->regtab;
	while (1) {
		reg = *prev;
		if (NULL == reg || !strcmp(name, reg->key.p))
			break;
		prev = &reg->next;
	}
	if (NULL != reg) {
		*prev = reg->next;
		free(reg->key.p);
		free(reg);
	}
	return(ROFF_IGN);
}

static enum rofferr
roff_rm(ROFF_ARGS)
{
	const char	 *name;
	char		 *cp;
	size_t		  namesz;

	cp = *bufp + pos;
	while ('\0' != *cp) {
		name = cp;
		namesz = roff_getname(r, &cp, ln, (int)(cp - *bufp));
		roff_setstrn(&r->strtab, name, namesz, NULL, 0, 0);
		if ('\\' == name[namesz])
			break;
	}
	return(ROFF_IGN);
}

static enum rofferr
roff_it(ROFF_ARGS)
{
	char		*cp;
	size_t		 len;
	int		 iv;

	/* Parse the number of lines. */
	cp = *bufp + pos;
	len = strcspn(cp, " \t");
	cp[len] = '\0';
	if ((iv = mandoc_strntoi(cp, len, 10)) <= 0) {
		mandoc_msg(MANDOCERR_IT_NONUM, r->parse,
		    ln, ppos, *bufp + 1);
		return(ROFF_IGN);
	}
	cp += len + 1;

	/* Arm the input line trap. */
	roffit_lines = iv;
	roffit_macro = mandoc_strdup(cp);
	return(ROFF_IGN);
}

static enum rofferr
roff_Dd(ROFF_ARGS)
{
	const char *const	*cp;

	if (0 == ((MPARSE_MDOC | MPARSE_QUICK) & r->options))
		for (cp = __mdoc_reserved; *cp; cp++)
			roff_setstr(r, *cp, NULL, 0);

	return(ROFF_CONT);
}

static enum rofferr
roff_TH(ROFF_ARGS)
{
	const char *const	*cp;

	if (0 == (MPARSE_QUICK & r->options))
		for (cp = __man_reserved; *cp; cp++)
			roff_setstr(r, *cp, NULL, 0);

	return(ROFF_CONT);
}

static enum rofferr
roff_TE(ROFF_ARGS)
{

	if (NULL == r->tbl)
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "TE");
	else
		tbl_end(&r->tbl);

	return(ROFF_IGN);
}

static enum rofferr
roff_T_(ROFF_ARGS)
{

	if (NULL == r->tbl)
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse,
		    ln, ppos, "T&");
	else
		tbl_restart(ppos, ln, r->tbl);

	return(ROFF_IGN);
}

#if 0
static int
roff_closeeqn(struct roff *r)
{

	return(r->eqn && ROFF_EQN == eqn_end(&r->eqn) ? 1 : 0);
}
#endif

static void
roff_openeqn(struct roff *r, const char *name, int line,
		int offs, const char *buf)
{
	struct eqn_node *e;
	int		 poff;

	assert(NULL == r->eqn);
	e = eqn_alloc(name, offs, line, r->parse);

	if (r->last_eqn)
		r->last_eqn->next = e;
	else
		r->first_eqn = r->last_eqn = e;

	r->eqn = r->last_eqn = e;

	if (buf) {
		poff = 0;
		eqn_read(&r->eqn, line, buf, offs, &poff);
	}
}

static enum rofferr
roff_EQ(ROFF_ARGS)
{

	roff_openeqn(r, *bufp + pos, ln, ppos, NULL);
	return(ROFF_IGN);
}

static enum rofferr
roff_EN(ROFF_ARGS)
{

	mandoc_msg(MANDOCERR_BLK_NOTOPEN, r->parse, ln, ppos, "EN");
	return(ROFF_IGN);
}

static enum rofferr
roff_TS(ROFF_ARGS)
{
	struct tbl_node	*tbl;

	if (r->tbl) {
		mandoc_msg(MANDOCERR_BLK_BROKEN, r->parse,
		    ln, ppos, "TS breaks TS");
		tbl_end(&r->tbl);
	}

	tbl = tbl_alloc(ppos, ln, r->parse);

	if (r->last_tbl)
		r->last_tbl->next = tbl;
	else
		r->first_tbl = r->last_tbl = tbl;

	r->tbl = r->last_tbl = tbl;
	return(ROFF_IGN);
}

static enum rofferr
roff_cc(ROFF_ARGS)
{
	const char	*p;

	p = *bufp + pos;

	if ('\0' == *p || '.' == (r->control = *p++))
		r->control = 0;

	if ('\0' != *p)
		mandoc_msg(MANDOCERR_ARGCOUNT, r->parse, ln, ppos, NULL);

	return(ROFF_IGN);
}

static enum rofferr
roff_tr(ROFF_ARGS)
{
	const char	*p, *first, *second;
	size_t		 fsz, ssz;
	enum mandoc_esc	 esc;

	p = *bufp + pos;

	if ('\0' == *p) {
		mandoc_msg(MANDOCERR_ARGCOUNT, r->parse, ln, ppos, NULL);
		return(ROFF_IGN);
	}

	while ('\0' != *p) {
		fsz = ssz = 1;

		first = p++;
		if ('\\' == *first) {
			esc = mandoc_escape(&p, NULL, NULL);
			if (ESCAPE_ERROR == esc) {
				mandoc_msg(MANDOCERR_ESC_BAD, r->parse,
				    ln, (int)(p - *bufp), first);
				return(ROFF_IGN);
			}
			fsz = (size_t)(p - first);
		}

		second = p++;
		if ('\\' == *second) {
			esc = mandoc_escape(&p, NULL, NULL);
			if (ESCAPE_ERROR == esc) {
				mandoc_msg(MANDOCERR_ESC_BAD, r->parse,
				    ln, (int)(p - *bufp), second);
				return(ROFF_IGN);
			}
			ssz = (size_t)(p - second);
		} else if ('\0' == *second) {
			mandoc_msg(MANDOCERR_ARGCOUNT, r->parse,
			    ln, (int)(p - *bufp), NULL);
			second = " ";
			p--;
		}

		if (fsz > 1) {
			roff_setstrn(&r->xmbtab, first, fsz,
			    second, ssz, 0);
			continue;
		}

		if (NULL == r->xtab)
			r->xtab = mandoc_calloc(128,
			    sizeof(struct roffstr));

		free(r->xtab[(int)*first].p);
		r->xtab[(int)*first].p = mandoc_strndup(second, ssz);
		r->xtab[(int)*first].sz = ssz;
	}

	return(ROFF_IGN);
}

static enum rofferr
roff_so(ROFF_ARGS)
{
	char *name;

	name = *bufp + pos;
	mandoc_vmsg(MANDOCERR_SO, r->parse, ln, ppos, "so %s", name);

	/*
	 * Handle `so'.  Be EXTREMELY careful, as we shouldn't be
	 * opening anything that's not in our cwd or anything beneath
	 * it.  Thus, explicitly disallow traversing up the file-system
	 * or using absolute paths.
	 */

	if ('/' == *name || strstr(name, "../") || strstr(name, "/..")) {
		mandoc_vmsg(MANDOCERR_SO_PATH, r->parse, ln, ppos,
		    ".so %s", name);
		return(ROFF_ERR);
	}

	*offs = pos;
	return(ROFF_SO);
}

static enum rofferr
roff_userdef(ROFF_ARGS)
{
	const char	 *arg[9];
	char		 *cp, *n1, *n2;
	int		  i;

	/*
	 * Collect pointers to macro argument strings
	 * and NUL-terminate them.
	 */
	cp = *bufp + pos;
	for (i = 0; i < 9; i++)
		arg[i] = '\0' == *cp ? "" :
		    mandoc_getarg(r->parse, &cp, ln, &pos);

	/*
	 * Expand macro arguments.
	 */
	*szp = 0;
	n1 = cp = mandoc_strdup(r->current_string);
	while (NULL != (cp = strstr(cp, "\\$"))) {
		i = cp[2] - '1';
		if (0 > i || 8 < i) {
			/* Not an argument invocation. */
			cp += 2;
			continue;
		}
		*cp = '\0';
		*szp = mandoc_asprintf(&n2, "%s%s%s",
		    n1, arg[i], cp + 3) + 1;
		cp = n2 + (cp - n1);
		free(n1);
		n1 = n2;
	}

	/*
	 * Replace the macro invocation
	 * by the expanded macro.
	 */
	free(*bufp);
	*bufp = n1;
	if (0 == *szp)
		*szp = strlen(*bufp) + 1;

	return(*szp > 1 && '\n' == (*bufp)[(int)*szp - 2] ?
	   ROFF_REPARSE : ROFF_APPEND);
}

static size_t
roff_getname(struct roff *r, char **cpp, int ln, int pos)
{
	char	 *name, *cp;
	size_t	  namesz;

	name = *cpp;
	if ('\0' == *name)
		return(0);

	/* Read until end of name and terminate it with NUL. */
	for (cp = name; 1; cp++) {
		if ('\0' == *cp || ' ' == *cp) {
			namesz = cp - name;
			break;
		}
		if ('\\' != *cp)
			continue;
		namesz = cp - name;
		if ('{' == cp[1] || '}' == cp[1])
			break;
		cp++;
		if ('\\' == *cp)
			continue;
		mandoc_vmsg(MANDOCERR_NAMESC, r->parse, ln, pos,
		    "%.*s", (int)(cp - name + 1), name);
		mandoc_escape((const char **)&cp, NULL, NULL);
		break;
	}

	/* Read past spaces. */
	while (' ' == *cp)
		cp++;

	*cpp = cp;
	return(namesz);
}

/*
 * Store *string into the user-defined string called *name.
 * To clear an existing entry, call with (*r, *name, NULL, 0).
 * append == 0: replace mode
 * append == 1: single-line append mode
 * append == 2: multiline append mode, append '\n' after each call
 */
static void
roff_setstr(struct roff *r, const char *name, const char *string,
	int append)
{

	roff_setstrn(&r->strtab, name, strlen(name), string,
	    string ? strlen(string) : 0, append);
}

static void
roff_setstrn(struct roffkv **r, const char *name, size_t namesz,
		const char *string, size_t stringsz, int append)
{
	struct roffkv	*n;
	char		*c;
	int		 i;
	size_t		 oldch, newch;

	/* Search for an existing string with the same name. */
	n = *r;

	while (n && (namesz != n->key.sz ||
			strncmp(n->key.p, name, namesz)))
		n = n->next;

	if (NULL == n) {
		/* Create a new string table entry. */
		n = mandoc_malloc(sizeof(struct roffkv));
		n->key.p = mandoc_strndup(name, namesz);
		n->key.sz = namesz;
		n->val.p = NULL;
		n->val.sz = 0;
		n->next = *r;
		*r = n;
	} else if (0 == append) {
		free(n->val.p);
		n->val.p = NULL;
		n->val.sz = 0;
	}

	if (NULL == string)
		return;

	/*
	 * One additional byte for the '\n' in multiline mode,
	 * and one for the terminating '\0'.
	 */
	newch = stringsz + (1 < append ? 2u : 1u);

	if (NULL == n->val.p) {
		n->val.p = mandoc_malloc(newch);
		*n->val.p = '\0';
		oldch = 0;
	} else {
		oldch = n->val.sz;
		n->val.p = mandoc_realloc(n->val.p, oldch + newch);
	}

	/* Skip existing content in the destination buffer. */
	c = n->val.p + (int)oldch;

	/* Append new content to the destination buffer. */
	i = 0;
	while (i < (int)stringsz) {
		/*
		 * Rudimentary roff copy mode:
		 * Handle escaped backslashes.
		 */
		if ('\\' == string[i] && '\\' == string[i + 1])
			i++;
		*c++ = string[i++];
	}

	/* Append terminating bytes. */
	if (1 < append)
		*c++ = '\n';

	*c = '\0';
	n->val.sz = (int)(c - n->val.p);
}

static const char *
roff_getstrn(const struct roff *r, const char *name, size_t len)
{
	const struct roffkv *n;
	int i;

	for (n = r->strtab; n; n = n->next)
		if (0 == strncmp(name, n->key.p, len) &&
		    '\0' == n->key.p[(int)len])
			return(n->val.p);

	for (i = 0; i < PREDEFS_MAX; i++)
		if (0 == strncmp(name, predefs[i].name, len) &&
				'\0' == predefs[i].name[(int)len])
			return(predefs[i].str);

	return(NULL);
}

static void
roff_freestr(struct roffkv *r)
{
	struct roffkv	 *n, *nn;

	for (n = r; n; n = nn) {
		free(n->key.p);
		free(n->val.p);
		nn = n->next;
		free(n);
	}
}

const struct tbl_span *
roff_span(const struct roff *r)
{

	return(r->tbl ? tbl_span(r->tbl) : NULL);
}

const struct eqn *
roff_eqn(const struct roff *r)
{

	return(r->last_eqn ? &r->last_eqn->eqn : NULL);
}

/*
 * Duplicate an input string, making the appropriate character
 * conversations (as stipulated by `tr') along the way.
 * Returns a heap-allocated string with all the replacements made.
 */
char *
roff_strdup(const struct roff *r, const char *p)
{
	const struct roffkv *cp;
	char		*res;
	const char	*pp;
	size_t		 ssz, sz;
	enum mandoc_esc	 esc;

	if (NULL == r->xmbtab && NULL == r->xtab)
		return(mandoc_strdup(p));
	else if ('\0' == *p)
		return(mandoc_strdup(""));

	/*
	 * Step through each character looking for term matches
	 * (remember that a `tr' can be invoked with an escape, which is
	 * a glyph but the escape is multi-character).
	 * We only do this if the character hash has been initialised
	 * and the string is >0 length.
	 */

	res = NULL;
	ssz = 0;

	while ('\0' != *p) {
		if ('\\' != *p && r->xtab && r->xtab[(int)*p].p) {
			sz = r->xtab[(int)*p].sz;
			res = mandoc_realloc(res, ssz + sz + 1);
			memcpy(res + ssz, r->xtab[(int)*p].p, sz);
			ssz += sz;
			p++;
			continue;
		} else if ('\\' != *p) {
			res = mandoc_realloc(res, ssz + 2);
			res[ssz++] = *p++;
			continue;
		}

		/* Search for term matches. */
		for (cp = r->xmbtab; cp; cp = cp->next)
			if (0 == strncmp(p, cp->key.p, cp->key.sz))
				break;

		if (NULL != cp) {
			/*
			 * A match has been found.
			 * Append the match to the array and move
			 * forward by its keysize.
			 */
			res = mandoc_realloc(res,
			    ssz + cp->val.sz + 1);
			memcpy(res + ssz, cp->val.p, cp->val.sz);
			ssz += cp->val.sz;
			p += (int)cp->key.sz;
			continue;
		}

		/*
		 * Handle escapes carefully: we need to copy
		 * over just the escape itself, or else we might
		 * do replacements within the escape itself.
		 * Make sure to pass along the bogus string.
		 */
		pp = p++;
		esc = mandoc_escape(&p, NULL, NULL);
		if (ESCAPE_ERROR == esc) {
			sz = strlen(pp);
			res = mandoc_realloc(res, ssz + sz + 1);
			memcpy(res + ssz, pp, sz);
			break;
		}
		/*
		 * We bail out on bad escapes.
		 * No need to warn: we already did so when
		 * roff_res() was called.
		 */
		sz = (int)(p - pp);
		res = mandoc_realloc(res, ssz + sz + 1);
		memcpy(res + ssz, pp, sz);
		ssz += sz;
	}

	res[(int)ssz] = '\0';
	return(res);
}

/*
 * Find out whether a line is a macro line or not.
 * If it is, adjust the current position and return one; if it isn't,
 * return zero and don't change the current position.
 * If the control character has been set with `.cc', then let that grain
 * precedence.
 * This is slighly contrary to groff, where using the non-breaking
 * control character when `cc' has been invoked will cause the
 * non-breaking macro contents to be printed verbatim.
 */
int
roff_getcontrol(const struct roff *r, const char *cp, int *ppos)
{
	int		pos;

	pos = *ppos;

	if (0 != r->control && cp[pos] == r->control)
		pos++;
	else if (0 != r->control)
		return(0);
	else if ('\\' == cp[pos] && '.' == cp[pos + 1])
		pos += 2;
	else if ('.' == cp[pos] || '\'' == cp[pos])
		pos++;
	else
		return(0);

	while (' ' == cp[pos] || '\t' == cp[pos])
		pos++;

	*ppos = pos;
	return(1);
}
