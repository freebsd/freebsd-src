/* dfa.c - deterministic extended regexp routines for GNU
   Copyright (C) 1988 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Written June, 1988 by Mike Haertel
   Modified July, 1988 by Arthur David Olson to assist BMG speedups  */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
#include <sys/types.h>
extern char *calloc(), *malloc(), *realloc();
extern void free();
#endif

#if defined(HAVE_STRING_H) || defined(STDC_HEADERS)
#include <string.h>
#undef index
#define index strchr
#else
#include <strings.h>
#endif

#ifndef isgraph
#define isgraph(C) (isprint(C) && !isspace(C))
#endif

#ifdef isascii
#define ISALPHA(C) (isascii(C) && isalpha(C))
#define ISUPPER(C) (isascii(C) && isupper(C))
#define ISLOWER(C) (isascii(C) && islower(C))
#define ISDIGIT(C) (isascii(C) && isdigit(C))
#define ISXDIGIT(C) (isascii(C) && isxdigit(C))
#define ISSPACE(C) (isascii(C) && isspace(C))
#define ISPUNCT(C) (isascii(C) && ispunct(C))
#define ISALNUM(C) (isascii(C) && isalnum(C))
#define ISPRINT(C) (isascii(C) && isprint(C))
#define ISGRAPH(C) (isascii(C) && isgraph(C))
#define ISCNTRL(C) (isascii(C) && iscntrl(C))
#else
#define ISALPHA(C) isalpha(C)
#define ISUPPER(C) isupper(C)
#define ISLOWER(C) islower(C)
#define ISDIGIT(C) isdigit(C)
#define ISXDIGIT(C) isxdigit(C)
#define ISSPACE(C) isspace(C)
#define ISPUNCT(C) ispunct(C)
#define ISALNUM(C) isalnum(C)
#define ISPRINT(C) isprint(C)
#define ISGRAPH(C) isgraph(C)
#define ISCNTRL(C) iscntrl(C)
#endif

#include "dfa.h"
#include "regex.h"

#if __STDC__
typedef void *ptr_t;
#else
typedef char *ptr_t;
#endif

static void	dfamust();

static ptr_t
xcalloc(n, s)
     int n;
     size_t s;
{
  ptr_t r = calloc(n, s);

  if (!r)
    dfaerror("Memory exhausted");
  return r;
}

static ptr_t
xmalloc(n)
     size_t n;
{
  ptr_t r = malloc(n);

  assert(n != 0);
  if (!r)
    dfaerror("Memory exhausted");
  return r;
}

static ptr_t
xrealloc(p, n)
     ptr_t p;
     size_t n;
{
  ptr_t r = realloc(p, n);

  assert(n != 0);
  if (!r)
    dfaerror("Memory exhausted");
  return r;
}

#define CALLOC(p, t, n) ((p) = (t *) xcalloc((n), sizeof (t)))
#define MALLOC(p, t, n) ((p) = (t *) xmalloc((n) * sizeof (t)))
#define REALLOC(p, t, n) ((p) = (t *) xrealloc((ptr_t) (p), (n) * sizeof (t)))

/* Reallocate an array of type t if nalloc is too small for index. */
#define REALLOC_IF_NECESSARY(p, t, nalloc, index) \
  if ((index) >= (nalloc))			  \
    {						  \
      while ((index) >= (nalloc))		  \
	(nalloc) *= 2;				  \
      REALLOC(p, t, nalloc);			  \
    }

#ifdef DEBUG

static void
prtok(t)
     token t;
{
  char *s;

  if (t < 0)
    fprintf(stderr, "END");
  else if (t < NOTCHAR)
    fprintf(stderr, "%c", t);
  else
    {
      switch (t)
	{
	case EMPTY: s = "EMPTY"; break;
	case BACKREF: s = "BACKREF"; break;
	case BEGLINE: s = "BEGLINE"; break;
	case ENDLINE: s = "ENDLINE"; break;
	case BEGWORD: s = "BEGWORD"; break;
	case ENDWORD: s = "ENDWORD"; break;
	case LIMWORD: s = "LIMWORD"; break;
	case NOTLIMWORD: s = "NOTLIMWORD"; break;
	case QMARK: s = "QMARK"; break;
	case STAR: s = "STAR"; break;
	case PLUS: s = "PLUS"; break;
	case CAT: s = "CAT"; break;
	case OR: s = "OR"; break;
	case ORTOP: s = "ORTOP"; break;
	case LPAREN: s = "LPAREN"; break;
	case RPAREN: s = "RPAREN"; break;
	default: s = "CSET"; break;
	}
      fprintf(stderr, "%s", s);
    }
}
#endif /* DEBUG */

/* Stuff pertaining to charclasses. */

static int
tstbit(b, c)
     int b;
     charclass c;
{
  return c[b / INTBITS] & 1 << b % INTBITS;
}

static void
setbit(b, c)
     int b;
     charclass c;
{
  c[b / INTBITS] |= 1 << b % INTBITS;
}

static void
clrbit(b, c)
     int b;
     charclass c;
{
  c[b / INTBITS] &= ~(1 << b % INTBITS);
}

static void
copyset(src, dst)
     charclass src;
     charclass dst;
{
  int i;

  for (i = 0; i < CHARCLASS_INTS; ++i)
    dst[i] = src[i];
}

static void
zeroset(s)
     charclass s;
{
  int i;

  for (i = 0; i < CHARCLASS_INTS; ++i)
    s[i] = 0;
}

static void
notset(s)
     charclass s;
{
  int i;

  for (i = 0; i < CHARCLASS_INTS; ++i)
    s[i] = ~s[i];
}

static int
equal(s1, s2)
     charclass s1;
     charclass s2;
{
  int i;

  for (i = 0; i < CHARCLASS_INTS; ++i)
    if (s1[i] != s2[i])
      return 0;
  return 1;
}

/* A pointer to the current dfa is kept here during parsing. */
static struct dfa *dfa;

/* Find the index of charclass s in dfa->charclasses, or allocate a new charclass. */
static int
charclass_index(s)
     charclass s;
{
  int i;

  for (i = 0; i < dfa->cindex; ++i)
    if (equal(s, dfa->charclasses[i]))
      return i;
  REALLOC_IF_NECESSARY(dfa->charclasses, charclass, dfa->calloc, dfa->cindex);
  ++dfa->cindex;
  copyset(s, dfa->charclasses[i]);
  return i;
}

/* Syntax bits controlling the behavior of the lexical analyzer. */
static int syntax_bits, syntax_bits_set;

/* Flag for case-folding letters into sets. */
static int case_fold;

/* Entry point to set syntax options. */
void
dfasyntax(bits, fold)
     int bits;
     int fold;
{
  syntax_bits_set = 1;
  syntax_bits = bits;
  case_fold = fold;
}

/* Lexical analyzer.  All the dross that deals with the obnoxious
   GNU Regex syntax bits is located here.  The poor, suffering
   reader is referred to the GNU Regex documentation for the
   meaning of the @#%!@#%^!@ syntax bits. */

static char *lexstart;		/* Pointer to beginning of input string. */
static char *lexptr;		/* Pointer to next input character. */
static lexleft;			/* Number of characters remaining. */
static token lasttok;		/* Previous token returned; initially END. */
static int laststart;		/* True if we're separated from beginning or (, |
				   only by zero-width characters. */
static int parens;		/* Count of outstanding left parens. */
static int minrep, maxrep;	/* Repeat counts for {m,n}. */

/* Note that characters become unsigned here. */
#define FETCH(c, eoferr)   	      \
  {			   	      \
    if (! lexleft)	   	      \
      if (eoferr != 0)	   	      \
	dfaerror(eoferr);  	      \
      else		   	      \
	return END;	   	      \
    (c) = (unsigned char) *lexptr++;  \
    --lexleft;		   	      \
  }

#define FUNC(F, P) static int F(c) int c; { return P(c); }

FUNC(is_alpha, ISALPHA)
FUNC(is_upper, ISUPPER)
FUNC(is_lower, ISLOWER)
FUNC(is_digit, ISDIGIT)
FUNC(is_xdigit, ISXDIGIT)
FUNC(is_space, ISSPACE)
FUNC(is_punct, ISPUNCT)
FUNC(is_alnum, ISALNUM)
FUNC(is_print, ISPRINT)
FUNC(is_graph, ISGRAPH)
FUNC(is_cntrl, ISCNTRL)

/* The following list maps the names of the Posix named character classes
   to predicate functions that determine whether a given character is in
   the class.  The leading [ has already been eaten by the lexical analyzer. */
static struct {
  char *name;
  int (*pred)();
} prednames[] = {
  ":alpha:]", is_alpha,
  ":upper:]", is_upper,
  ":lower:]", is_lower,
  ":digit:]", is_digit,
  ":xdigit:]", is_xdigit,
  ":space:]", is_space,
  ":punct:]", is_punct,
  ":alnum:]", is_alnum,
  ":print:]", is_print,
  ":graph:]", is_graph,
  ":cntrl:]", is_cntrl,
  0
};

static int
looking_at(s)
     char *s;
{
  int len;

  len = strlen(s);
  if (lexleft < len)
    return 0;
  return strncmp(s, lexptr, len) == 0;
}

static token
lex()
{
  token c, c1, c2;
  int backslash = 0, invert;
  charclass ccl;
  int i;

  /* Basic plan: We fetch a character.  If it's a backslash,
     we set the backslash flag and go through the loop again.
     On the plus side, this avoids having a duplicate of the
     main switch inside the backslash case.  On the minus side,
     it means that just about every case begins with
     "if (backslash) ...".  */
  for (i = 0; i < 2; ++i)
    {
      FETCH(c, 0);
      switch (c)
	{
	case '\\':
	  if (backslash)
	    goto normal_char;
	  if (lexleft == 0)
	    dfaerror("Unfinished \\ escape");
	  backslash = 1;
	  break;

	case '^':
	  if (backslash)
	    goto normal_char;
	  if (syntax_bits & RE_CONTEXT_INDEP_ANCHORS
	      || lasttok == END
	      || lasttok == LPAREN
	      || lasttok == OR)
	    return lasttok = BEGLINE;
	  goto normal_char;

	case '$':
	  if (backslash)
	    goto normal_char;
	  if (syntax_bits & RE_CONTEXT_INDEP_ANCHORS
	      || lexleft == 0
	      || (syntax_bits & RE_NO_BK_PARENS
		  ? lexleft > 0 && *lexptr == ')'
		  : lexleft > 1 && lexptr[0] == '\\' && lexptr[1] == ')')
	      || (syntax_bits & RE_NO_BK_VBAR
		  ? lexleft > 0 && *lexptr == '|'
		  : lexleft > 1 && lexptr[0] == '\\' && lexptr[1] == '|')
	      || ((syntax_bits & RE_NEWLINE_ALT)
	          && lexleft > 0 && *lexptr == '\n'))
	    return lasttok = ENDLINE;
	  goto normal_char;

	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  if (backslash && !(syntax_bits & RE_NO_BK_REFS))
	    {
	      laststart = 0;
	      return lasttok = BACKREF;
	    }
	  goto normal_char;

	case '<':
	  if (backslash)
	    return lasttok = BEGWORD;
	  goto normal_char;

	case '>':
	  if (backslash)
	    return lasttok = ENDWORD;
	  goto normal_char;

	case 'b':
	  if (backslash)
	    return lasttok = LIMWORD;
	  goto normal_char;

	case 'B':
	  if (backslash)
	    return lasttok = NOTLIMWORD;
	  goto normal_char;

	case '?':
	  if (syntax_bits & RE_LIMITED_OPS)
	    goto normal_char;
	  if (backslash != ((syntax_bits & RE_BK_PLUS_QM) != 0))
	    goto normal_char;
	  if (!(syntax_bits & RE_CONTEXT_INDEP_OPS) && laststart)
	    goto normal_char;
	  return lasttok = QMARK;

	case '*':
	  if (backslash)
	    goto normal_char;
	  if (!(syntax_bits & RE_CONTEXT_INDEP_OPS) && laststart)
	    goto normal_char;
	  return lasttok = STAR;

	case '+':
	  if (syntax_bits & RE_LIMITED_OPS)
	    goto normal_char;
	  if (backslash != ((syntax_bits & RE_BK_PLUS_QM) != 0))
	    goto normal_char;
	  if (!(syntax_bits & RE_CONTEXT_INDEP_OPS) && laststart)
	    goto normal_char;
	  return lasttok = PLUS;

	case '{':
	  if (!(syntax_bits & RE_INTERVALS))
	    goto normal_char;
	  if (backslash != ((syntax_bits & RE_NO_BK_BRACES) == 0))
	    goto normal_char;
	  minrep = maxrep = 0;
	  /* Cases:
	     {M} - exact count
	     {M,} - minimum count, maximum is infinity
	     {,M} - 0 through M
	     {M,N} - M through N */
	  FETCH(c, "unfinished repeat count");
	  if (ISDIGIT(c))
	    {
	      minrep = c - '0';
	      for (;;)
		{
		  FETCH(c, "unfinished repeat count");
		  if (!ISDIGIT(c))
		    break;
		  minrep = 10 * minrep + c - '0';
		}
	    }
	  else if (c != ',')
	    dfaerror("malformed repeat count");
	  if (c == ',')
	    for (;;)
	      {
		FETCH(c, "unfinished repeat count");
		if (!ISDIGIT(c))
		  break;
		maxrep = 10 * maxrep + c - '0';
	      }
	  else
	    maxrep = minrep;
	  if (!(syntax_bits & RE_NO_BK_BRACES))
	    {
	      if (c != '\\')
		dfaerror("malformed repeat count");
	      FETCH(c, "unfinished repeat count");
	    }
	  if (c != '}')
	    dfaerror("malformed repeat count");
	  laststart = 0;
	  return lasttok = REPMN;

	case '|':
	  if (syntax_bits & RE_LIMITED_OPS)
	    goto normal_char;
	  if (backslash != ((syntax_bits & RE_NO_BK_VBAR) == 0))
	    goto normal_char;
	  laststart = 1;
	  return lasttok = OR;

	case '\n':
	  if (syntax_bits & RE_LIMITED_OPS
	      || backslash
	      || !(syntax_bits & RE_NEWLINE_ALT))
	    goto normal_char;
	  laststart = 1;
	  return lasttok = OR;

	case '(':
	  if (backslash != ((syntax_bits & RE_NO_BK_PARENS) == 0))
	    goto normal_char;
	  ++parens;
	  laststart = 1;
	  return lasttok = LPAREN;

	case ')':
	  if (backslash != ((syntax_bits & RE_NO_BK_PARENS) == 0))
	    goto normal_char;
	  if (parens == 0 && syntax_bits & RE_UNMATCHED_RIGHT_PAREN_ORD)
	    goto normal_char;
	  --parens;
	  laststart = 0;
	  return lasttok = RPAREN;

	case '.':
	  if (backslash)
	    goto normal_char;
	  zeroset(ccl);
	  notset(ccl);
	  if (!(syntax_bits & RE_DOT_NEWLINE))
	    clrbit('\n', ccl);
	  if (syntax_bits & RE_DOT_NOT_NULL)
	    clrbit('\0', ccl);
	  laststart = 0;
	  return lasttok = CSET + charclass_index(ccl);

	case 'w':
	case 'W':
	  if (!backslash)
	    goto normal_char;
	  zeroset(ccl);
	  for (c2 = 0; c2 < NOTCHAR; ++c2)
	    if (ISALNUM(c2))
	      setbit(c2, ccl);
	  if (c == 'W')
	    notset(ccl);
	  laststart = 0;
	  return lasttok = CSET + charclass_index(ccl);
	
	case '[':
	  if (backslash)
	    goto normal_char;
	  zeroset(ccl);
	  FETCH(c, "Unbalanced [");
	  if (c == '^')
	    {
	      FETCH(c, "Unbalanced [");
	      invert = 1;
	    }
	  else
	    invert = 0;
	  do
	    {
	      /* Nobody ever said this had to be fast. :-)
		 Note that if we're looking at some other [:...:]
		 construct, we just treat it as a bunch of ordinary
		 characters.  We can do this because we assume
		 regex has checked for syntax errors before
		 dfa is ever called. */
	      if (c == '[' && (syntax_bits & RE_CHAR_CLASSES))
		for (c1 = 0; prednames[c1].name; ++c1)
		  if (looking_at(prednames[c1].name))
		    {
		      for (c2 = 0; c2 < NOTCHAR; ++c2)
			if ((*prednames[c1].pred)(c2))
			  setbit(c2, ccl);
		      lexptr += strlen(prednames[c1].name);
		      lexleft -= strlen(prednames[c1].name);
		      FETCH(c1, "Unbalanced [");
		      goto skip;
		    }
	      if (c == '\\' && (syntax_bits & RE_BACKSLASH_ESCAPE_IN_LISTS))
		FETCH(c, "Unbalanced [");
	      FETCH(c1, "Unbalanced [");
	      if (c1 == '-')
		{
		  FETCH(c2, "Unbalanced [");
		  if (c2 == ']')
		    {
		      /* In the case [x-], the - is an ordinary hyphen,
			 which is left in c1, the lookahead character. */
		      --lexptr;
		      ++lexleft;
		      c2 = c;
		    }
		  else
		    {
		      if (c2 == '\\'
			  && (syntax_bits & RE_BACKSLASH_ESCAPE_IN_LISTS))
			FETCH(c2, "Unbalanced [");
		      FETCH(c1, "Unbalanced [");
		    }
		}
	      else
		c2 = c;
	      while (c <= c2)
		{
		  setbit(c, ccl);
		  if (case_fold)
		    if (ISUPPER(c))
		      setbit(tolower(c), ccl);
		    else if (ISLOWER(c))
		      setbit(toupper(c), ccl);
		  ++c;
		}
	    skip:
	      ;
	    }
	  while ((c = c1) != ']');
	  if (invert)
	    {
	      notset(ccl);
	      if (syntax_bits & RE_HAT_LISTS_NOT_NEWLINE)
		clrbit('\n', ccl);
	    }
	  laststart = 0;
	  return lasttok = CSET + charclass_index(ccl);

	default:
	normal_char:
	  laststart = 0;
	  if (case_fold && ISALPHA(c))
	    {
	      zeroset(ccl);
	      setbit(c, ccl);
	      if (isupper(c))
		setbit(tolower(c), ccl);
	      else
		setbit(toupper(c), ccl);
	      return lasttok = CSET + charclass_index(ccl);
	    }
	  return c;
	}
    }

  /* The above loop should consume at most a backslash
     and some other character. */
  abort();
}

/* Recursive descent parser for regular expressions. */

static token tok;		/* Lookahead token. */
static depth;			/* Current depth of a hypothetical stack
				   holding deferred productions.  This is
				   used to determine the depth that will be
				   required of the real stack later on in
				   dfaanalyze(). */

/* Add the given token to the parse tree, maintaining the depth count and
   updating the maximum depth if necessary. */
static void
addtok(t)
     token t;
{
  REALLOC_IF_NECESSARY(dfa->tokens, token, dfa->talloc, dfa->tindex);
  dfa->tokens[dfa->tindex++] = t;

  switch (t)
    {
    case QMARK:
    case STAR:
    case PLUS:
      break;

    case CAT:
    case OR:
    case ORTOP:
      --depth;
      break;

    default:
      ++dfa->nleaves;
    case EMPTY:
      ++depth;
      break;
    }
  if (depth > dfa->depth)
    dfa->depth = depth;
}

/* The grammar understood by the parser is as follows.

   regexp:
     regexp OR branch
     branch

   branch:
     branch closure
     closure

   closure:
     closure QMARK
     closure STAR
     closure PLUS
     atom

   atom:
     <normal character>
     CSET
     BACKREF
     BEGLINE
     ENDLINE
     BEGWORD
     ENDWORD
     LIMWORD
     NOTLIMWORD
     <empty>

   The parser builds a parse tree in postfix form in an array of tokens. */

#if __STDC__
static void regexp(int);
#else
static void regexp();
#endif

static void
atom()
{
  if ((tok >= 0 && tok < NOTCHAR) || tok >= CSET || tok == BACKREF
      || tok == BEGLINE || tok == ENDLINE || tok == BEGWORD
      || tok == ENDWORD || tok == LIMWORD || tok == NOTLIMWORD)
    {
      addtok(tok);
      tok = lex();
    }
  else if (tok == LPAREN)
    {
      tok = lex();
      regexp(0);
      if (tok != RPAREN)
	dfaerror("Unbalanced (");
      tok = lex();
    }
  else
    addtok(EMPTY);
}

/* Return the number of tokens in the given subexpression. */
static int
nsubtoks(tindex)
{
  int ntoks1;

  switch (dfa->tokens[tindex - 1])
    {
    default:
      return 1;
    case QMARK:
    case STAR:
    case PLUS:
      return 1 + nsubtoks(tindex - 1);
    case CAT:
    case OR:
    case ORTOP:
      ntoks1 = nsubtoks(tindex - 1);
      return 1 + ntoks1 + nsubtoks(tindex - 1 - ntoks1);
    }
}

/* Copy the given subexpression to the top of the tree. */
static void
copytoks(tindex, ntokens)
     int tindex, ntokens;
{
  int i;

  for (i = 0; i < ntokens; ++i)
    addtok(dfa->tokens[tindex + i]);
}

static void
closure()
{
  int tindex, ntokens, i;

  atom();
  while (tok == QMARK || tok == STAR || tok == PLUS || tok == REPMN)
    if (tok == REPMN)
      {
	ntokens = nsubtoks(dfa->tindex);
	tindex = dfa->tindex - ntokens;
	if (maxrep == 0)
	  addtok(PLUS);
	if (minrep == 0)
	  addtok(QMARK);
	for (i = 1; i < minrep; ++i)
	  {
	    copytoks(tindex, ntokens);
	    addtok(CAT);
	  }
	for (; i < maxrep; ++i)
	  {
	    copytoks(tindex, ntokens);
	    addtok(QMARK);
	    addtok(CAT);
	  }
	tok = lex();
      }
    else
      {
	addtok(tok);
	tok = lex();
      }
}

static void
branch()
{
  closure();
  while (tok != RPAREN && tok != OR && tok >= 0)
    {
      closure();
      addtok(CAT);
    }
}

static void
regexp(toplevel)
     int toplevel;
{
  branch();
  while (tok == OR)
    {
      tok = lex();
      branch();
      if (toplevel)
	addtok(ORTOP);
      else
	addtok(OR);
    }
}

/* Main entry point for the parser.  S is a string to be parsed, len is the
   length of the string, so s can include NUL characters.  D is a pointer to
   the struct dfa to parse into. */
void
dfaparse(s, len, d)
     char *s;
     size_t len;
     struct dfa *d;

{
  dfa = d;
  lexstart = lexptr = s;
  lexleft = len;
  lasttok = END;
  laststart = 1;
  parens = 0;

  if (! syntax_bits_set)
    dfaerror("No syntax specified");

  tok = lex();
  depth = d->depth;

  regexp(1);

  if (tok != END)
    dfaerror("Unbalanced )");

  addtok(END - d->nregexps);
  addtok(CAT);

  if (d->nregexps)
    addtok(ORTOP);

  ++d->nregexps;
}

/* Some primitives for operating on sets of positions. */

/* Copy one set to another; the destination must be large enough. */
static void
copy(src, dst)
     position_set *src;
     position_set *dst;
{
  int i;

  for (i = 0; i < src->nelem; ++i)
    dst->elems[i] = src->elems[i];
  dst->nelem = src->nelem;
}

/* Insert a position in a set.  Position sets are maintained in sorted
   order according to index.  If position already exists in the set with
   the same index then their constraints are logically or'd together.
   S->elems must point to an array large enough to hold the resulting set. */
static void
insert(p, s)
     position p;
     position_set *s;
{
  int i;
  position t1, t2;

  for (i = 0; i < s->nelem && p.index < s->elems[i].index; ++i)
    ;
  if (i < s->nelem && p.index == s->elems[i].index)
    s->elems[i].constraint |= p.constraint;
  else
    {
      t1 = p;
      ++s->nelem;
      while (i < s->nelem)
	{
	  t2 = s->elems[i];
	  s->elems[i++] = t1;
	  t1 = t2;
	}
    }
}

/* Merge two sets of positions into a third.  The result is exactly as if
   the positions of both sets were inserted into an initially empty set. */
static void
merge(s1, s2, m)
     position_set *s1;
     position_set *s2;
     position_set *m;
{
  int i = 0, j = 0;

  m->nelem = 0;
  while (i < s1->nelem && j < s2->nelem)
    if (s1->elems[i].index > s2->elems[j].index)
      m->elems[m->nelem++] = s1->elems[i++];
    else if (s1->elems[i].index < s2->elems[j].index)
      m->elems[m->nelem++] = s2->elems[j++];
    else
      {
	m->elems[m->nelem] = s1->elems[i++];
	m->elems[m->nelem++].constraint |= s2->elems[j++].constraint;
      }
  while (i < s1->nelem)
    m->elems[m->nelem++] = s1->elems[i++];
  while (j < s2->nelem)
    m->elems[m->nelem++] = s2->elems[j++];
}

/* Delete a position from a set. */
static void
delete(p, s)
     position p;
     position_set *s;
{
  int i;

  for (i = 0; i < s->nelem; ++i)
    if (p.index == s->elems[i].index)
      break;
  if (i < s->nelem)
    for (--s->nelem; i < s->nelem; ++i)
      s->elems[i] = s->elems[i + 1];
}

/* Find the index of the state corresponding to the given position set with
   the given preceding context, or create a new state if there is no such
   state.  Newline and letter tell whether we got here on a newline or
   letter, respectively. */
static int
state_index(d, s, newline, letter)
     struct dfa *d;
     position_set *s;
     int newline;
     int letter;
{
  int hash = 0;
  int constraint;
  int i, j;

  newline = newline ? 1 : 0;
  letter = letter ? 1 : 0;

  for (i = 0; i < s->nelem; ++i)
    hash ^= s->elems[i].index + s->elems[i].constraint;

  /* Try to find a state that exactly matches the proposed one. */
  for (i = 0; i < d->sindex; ++i)
    {
      if (hash != d->states[i].hash || s->nelem != d->states[i].elems.nelem
	  || newline != d->states[i].newline || letter != d->states[i].letter)
	continue;
      for (j = 0; j < s->nelem; ++j)
	if (s->elems[j].constraint
	    != d->states[i].elems.elems[j].constraint
	    || s->elems[j].index != d->states[i].elems.elems[j].index)
	  break;
      if (j == s->nelem)
	return i;
    }

  /* We'll have to create a new state. */
  REALLOC_IF_NECESSARY(d->states, dfa_state, d->salloc, d->sindex);
  d->states[i].hash = hash;
  MALLOC(d->states[i].elems.elems, position, s->nelem);
  copy(s, &d->states[i].elems);
  d->states[i].newline = newline;
  d->states[i].letter = letter;
  d->states[i].backref = 0;
  d->states[i].constraint = 0;
  d->states[i].first_end = 0;
  for (j = 0; j < s->nelem; ++j)
    if (d->tokens[s->elems[j].index] < 0)
      {
	constraint = s->elems[j].constraint;
	if (SUCCEEDS_IN_CONTEXT(constraint, newline, 0, letter, 0)
	    || SUCCEEDS_IN_CONTEXT(constraint, newline, 0, letter, 1)
	    || SUCCEEDS_IN_CONTEXT(constraint, newline, 1, letter, 0)
	    || SUCCEEDS_IN_CONTEXT(constraint, newline, 1, letter, 1))
	  d->states[i].constraint |= constraint;
	if (! d->states[i].first_end)
	  d->states[i].first_end = d->tokens[s->elems[j].index];
      }
    else if (d->tokens[s->elems[j].index] == BACKREF)
      {
	d->states[i].constraint = NO_CONSTRAINT;
	d->states[i].backref = 1;
      }

  ++d->sindex;

  return i;
}

/* Find the epsilon closure of a set of positions.  If any position of the set
   contains a symbol that matches the empty string in some context, replace
   that position with the elements of its follow labeled with an appropriate
   constraint.  Repeat exhaustively until no funny positions are left.
   S->elems must be large enough to hold the result. */
void
epsclosure(s, d)
     position_set *s;
     struct dfa *d;
{
  int i, j;
  int *visited;
  position p, old;

  MALLOC(visited, int, d->tindex);
  for (i = 0; i < d->tindex; ++i)
    visited[i] = 0;

  for (i = 0; i < s->nelem; ++i)
    if (d->tokens[s->elems[i].index] >= NOTCHAR
	&& d->tokens[s->elems[i].index] != BACKREF
	&& d->tokens[s->elems[i].index] < CSET)
      {
	old = s->elems[i];
	p.constraint = old.constraint;
	delete(s->elems[i], s);
	if (visited[old.index])
	  {
	    --i;
	    continue;
	  }
	visited[old.index] = 1;
	switch (d->tokens[old.index])
	  {
	  case BEGLINE:
	    p.constraint &= BEGLINE_CONSTRAINT;
	    break;
	  case ENDLINE:
	    p.constraint &= ENDLINE_CONSTRAINT;
	    break;
	  case BEGWORD:
	    p.constraint &= BEGWORD_CONSTRAINT;
	    break;
	  case ENDWORD:
	    p.constraint &= ENDWORD_CONSTRAINT;
	    break;
	  case LIMWORD:
	    p.constraint &= LIMWORD_CONSTRAINT;
	    break;
	  case NOTLIMWORD:
	    p.constraint &= NOTLIMWORD_CONSTRAINT;
	    break;
	  default:
	    break;
	  }
	for (j = 0; j < d->follows[old.index].nelem; ++j)
	  {
	    p.index = d->follows[old.index].elems[j].index;
	    insert(p, s);
	  }
	/* Force rescan to start at the beginning. */
	i = -1;
      }

  free(visited);
}

/* Perform bottom-up analysis on the parse tree, computing various functions.
   Note that at this point, we're pretending constructs like \< are real
   characters rather than constraints on what can follow them.

   Nullable:  A node is nullable if it is at the root of a regexp that can
   match the empty string.
   *  EMPTY leaves are nullable.
   * No other leaf is nullable.
   * A QMARK or STAR node is nullable.
   * A PLUS node is nullable if its argument is nullable.
   * A CAT node is nullable if both its arguments are nullable.
   * An OR node is nullable if either argument is nullable.

   Firstpos:  The firstpos of a node is the set of positions (nonempty leaves)
   that could correspond to the first character of a string matching the
   regexp rooted at the given node.
   * EMPTY leaves have empty firstpos.
   * The firstpos of a nonempty leaf is that leaf itself.
   * The firstpos of a QMARK, STAR, or PLUS node is the firstpos of its
     argument.
   * The firstpos of a CAT node is the firstpos of the left argument, union
     the firstpos of the right if the left argument is nullable.
   * The firstpos of an OR node is the union of firstpos of each argument.

   Lastpos:  The lastpos of a node is the set of positions that could
   correspond to the last character of a string matching the regexp at
   the given node.
   * EMPTY leaves have empty lastpos.
   * The lastpos of a nonempty leaf is that leaf itself.
   * The lastpos of a QMARK, STAR, or PLUS node is the lastpos of its
     argument.
   * The lastpos of a CAT node is the lastpos of its right argument, union
     the lastpos of the left if the right argument is nullable.
   * The lastpos of an OR node is the union of the lastpos of each argument.

   Follow:  The follow of a position is the set of positions that could
   correspond to the character following a character matching the node in
   a string matching the regexp.  At this point we consider special symbols
   that match the empty string in some context to be just normal characters.
   Later, if we find that a special symbol is in a follow set, we will
   replace it with the elements of its follow, labeled with an appropriate
   constraint.
   * Every node in the firstpos of the argument of a STAR or PLUS node is in
     the follow of every node in the lastpos.
   * Every node in the firstpos of the second argument of a CAT node is in
     the follow of every node in the lastpos of the first argument.

   Because of the postfix representation of the parse tree, the depth-first
   analysis is conveniently done by a linear scan with the aid of a stack.
   Sets are stored as arrays of the elements, obeying a stack-like allocation
   scheme; the number of elements in each set deeper in the stack can be
   used to determine the address of a particular set's array. */
void
dfaanalyze(d, searchflag)
     struct dfa *d;
     int searchflag;
{
  int *nullable;		/* Nullable stack. */
  int *nfirstpos;		/* Element count stack for firstpos sets. */
  position *firstpos;		/* Array where firstpos elements are stored. */
  int *nlastpos;		/* Element count stack for lastpos sets. */
  position *lastpos;		/* Array where lastpos elements are stored. */
  int *nalloc;			/* Sizes of arrays allocated to follow sets. */
  position_set tmp;		/* Temporary set for merging sets. */
  position_set merged;		/* Result of merging sets. */
  int wants_newline;		/* True if some position wants newline info. */
  int *o_nullable;
  int *o_nfirst, *o_nlast;
  position *o_firstpos, *o_lastpos;
  int i, j;
  position *pos;

#ifdef DEBUG
  fprintf(stderr, "dfaanalyze:\n");
  for (i = 0; i < d->tindex; ++i)
    {
      fprintf(stderr, " %d:", i);
      prtok(d->tokens[i]);
    }
  putc('\n', stderr);
#endif

  d->searchflag = searchflag;

  MALLOC(nullable, int, d->depth);
  o_nullable = nullable;
  MALLOC(nfirstpos, int, d->depth);
  o_nfirst = nfirstpos;
  MALLOC(firstpos, position, d->nleaves);
  o_firstpos = firstpos, firstpos += d->nleaves;
  MALLOC(nlastpos, int, d->depth);
  o_nlast = nlastpos;
  MALLOC(lastpos, position, d->nleaves);
  o_lastpos = lastpos, lastpos += d->nleaves;
  MALLOC(nalloc, int, d->tindex);
  for (i = 0; i < d->tindex; ++i)
    nalloc[i] = 0;
  MALLOC(merged.elems, position, d->nleaves);

  CALLOC(d->follows, position_set, d->tindex);

  for (i = 0; i < d->tindex; ++i)
#ifdef DEBUG
    {				/* Nonsyntactic #ifdef goo... */
#endif
    switch (d->tokens[i])
      {
      case EMPTY:
	/* The empty set is nullable. */
	*nullable++ = 1;

	/* The firstpos and lastpos of the empty leaf are both empty. */
	*nfirstpos++ = *nlastpos++ = 0;
	break;

      case STAR:
      case PLUS:
	/* Every element in the firstpos of the argument is in the follow
	   of every element in the lastpos. */
	tmp.nelem = nfirstpos[-1];
	tmp.elems = firstpos;
	pos = lastpos;
	for (j = 0; j < nlastpos[-1]; ++j)
	  {
	    merge(&tmp, &d->follows[pos[j].index], &merged);
	    REALLOC_IF_NECESSARY(d->follows[pos[j].index].elems, position,
				 nalloc[pos[j].index], merged.nelem - 1);
	    copy(&merged, &d->follows[pos[j].index]);
	  }

      case QMARK:
	/* A QMARK or STAR node is automatically nullable. */
	if (d->tokens[i] != PLUS)
	  nullable[-1] = 1;
	break;

      case CAT:
	/* Every element in the firstpos of the second argument is in the
	   follow of every element in the lastpos of the first argument. */
	tmp.nelem = nfirstpos[-1];
	tmp.elems = firstpos;
	pos = lastpos + nlastpos[-1];
	for (j = 0; j < nlastpos[-2]; ++j)
	  {
	    merge(&tmp, &d->follows[pos[j].index], &merged);
	    REALLOC_IF_NECESSARY(d->follows[pos[j].index].elems, position,
				 nalloc[pos[j].index], merged.nelem - 1);
	    copy(&merged, &d->follows[pos[j].index]);
	  }

	/* The firstpos of a CAT node is the firstpos of the first argument,
	   union that of the second argument if the first is nullable. */
	if (nullable[-2])
	  nfirstpos[-2] += nfirstpos[-1];
	else
	  firstpos += nfirstpos[-1];
	--nfirstpos;

	/* The lastpos of a CAT node is the lastpos of the second argument,
	   union that of the first argument if the second is nullable. */
	if (nullable[-1])
	  nlastpos[-2] += nlastpos[-1];
	else
	  {
	    pos = lastpos + nlastpos[-2];
	    for (j = nlastpos[-1] - 1; j >= 0; --j)
	      pos[j] = lastpos[j];
	    lastpos += nlastpos[-2];
	    nlastpos[-2] = nlastpos[-1];
	  }
	--nlastpos;

	/* A CAT node is nullable if both arguments are nullable. */
	nullable[-2] = nullable[-1] && nullable[-2];
	--nullable;
	break;

      case OR:
      case ORTOP:
	/* The firstpos is the union of the firstpos of each argument. */
	nfirstpos[-2] += nfirstpos[-1];
	--nfirstpos;

	/* The lastpos is the union of the lastpos of each argument. */
	nlastpos[-2] += nlastpos[-1];
	--nlastpos;

	/* An OR node is nullable if either argument is nullable. */
	nullable[-2] = nullable[-1] || nullable[-2];
	--nullable;
	break;

      default:
	/* Anything else is a nonempty position.  (Note that special
	   constructs like \< are treated as nonempty strings here;
	   an "epsilon closure" effectively makes them nullable later.
	   Backreferences have to get a real position so we can detect
	   transitions on them later.  But they are nullable. */
	*nullable++ = d->tokens[i] == BACKREF;

	/* This position is in its own firstpos and lastpos. */
	*nfirstpos++ = *nlastpos++ = 1;
	--firstpos, --lastpos;
	firstpos->index = lastpos->index = i;
	firstpos->constraint = lastpos->constraint = NO_CONSTRAINT;

	/* Allocate the follow set for this position. */
	nalloc[i] = 1;
	MALLOC(d->follows[i].elems, position, nalloc[i]);
	break;
      }
#ifdef DEBUG
    /* ... balance the above nonsyntactic #ifdef goo... */
      fprintf(stderr, "node %d:", i);
      prtok(d->tokens[i]);
      putc('\n', stderr);
      fprintf(stderr, nullable[-1] ? " nullable: yes\n" : " nullable: no\n");
      fprintf(stderr, " firstpos:");
      for (j = nfirstpos[-1] - 1; j >= 0; --j)
	{
	  fprintf(stderr, " %d:", firstpos[j].index);
	  prtok(d->tokens[firstpos[j].index]);
	}
      fprintf(stderr, "\n lastpos:");
      for (j = nlastpos[-1] - 1; j >= 0; --j)
	{
	  fprintf(stderr, " %d:", lastpos[j].index);
	  prtok(d->tokens[lastpos[j].index]);
	}
      putc('\n', stderr);
    }
#endif

  /* For each follow set that is the follow set of a real position, replace
     it with its epsilon closure. */
  for (i = 0; i < d->tindex; ++i)
    if (d->tokens[i] < NOTCHAR || d->tokens[i] == BACKREF
	|| d->tokens[i] >= CSET)
      {
#ifdef DEBUG
	fprintf(stderr, "follows(%d:", i);
	prtok(d->tokens[i]);
	fprintf(stderr, "):");
	for (j = d->follows[i].nelem - 1; j >= 0; --j)
	  {
	    fprintf(stderr, " %d:", d->follows[i].elems[j].index);
	    prtok(d->tokens[d->follows[i].elems[j].index]);
	  }
	putc('\n', stderr);
#endif
	copy(&d->follows[i], &merged);
	epsclosure(&merged, d);
	if (d->follows[i].nelem < merged.nelem)
	  REALLOC(d->follows[i].elems, position, merged.nelem);
	copy(&merged, &d->follows[i]);
      }

  /* Get the epsilon closure of the firstpos of the regexp.  The result will
     be the set of positions of state 0. */
  merged.nelem = 0;
  for (i = 0; i < nfirstpos[-1]; ++i)
    insert(firstpos[i], &merged);
  epsclosure(&merged, d);

  /* Check if any of the positions of state 0 will want newline context. */
  wants_newline = 0;
  for (i = 0; i < merged.nelem; ++i)
    if (PREV_NEWLINE_DEPENDENT(merged.elems[i].constraint))
      wants_newline = 1;

  /* Build the initial state. */
  d->salloc = 1;
  d->sindex = 0;
  MALLOC(d->states, dfa_state, d->salloc);
  state_index(d, &merged, wants_newline, 0);

  free(o_nullable);
  free(o_nfirst);
  free(o_firstpos);
  free(o_nlast);
  free(o_lastpos);
  free(nalloc);
  free(merged.elems);
}

/* Find, for each character, the transition out of state s of d, and store
   it in the appropriate slot of trans.

   We divide the positions of s into groups (positions can appear in more
   than one group).  Each group is labeled with a set of characters that
   every position in the group matches (taking into account, if necessary,
   preceding context information of s).  For each group, find the union
   of the its elements' follows.  This set is the set of positions of the
   new state.  For each character in the group's label, set the transition
   on this character to be to a state corresponding to the set's positions,
   and its associated backward context information, if necessary.

   If we are building a searching matcher, we include the positions of state
   0 in every state.

   The collection of groups is constructed by building an equivalence-class
   partition of the positions of s.

   For each position, find the set of characters C that it matches.  Eliminate
   any characters from C that fail on grounds of backward context.

   Search through the groups, looking for a group whose label L has nonempty
   intersection with C.  If L - C is nonempty, create a new group labeled
   L - C and having the same positions as the current group, and set L to
   the intersection of L and C.  Insert the position in this group, set
   C = C - L, and resume scanning.

   If after comparing with every group there are characters remaining in C,
   create a new group labeled with the characters of C and insert this
   position in that group. */
void
dfastate(s, d, trans)
     int s;
     struct dfa *d;
     int trans[];
{
  position_set grps[NOTCHAR];	/* As many as will ever be needed. */
  charclass labels[NOTCHAR];	/* Labels corresponding to the groups. */
  int ngrps = 0;		/* Number of groups actually used. */
  position pos;			/* Current position being considered. */
  charclass matches;		/* Set of matching characters. */
  int matchesf;			/* True if matches is nonempty. */
  charclass intersect;		/* Intersection with some label set. */
  int intersectf;		/* True if intersect is nonempty. */
  charclass leftovers;		/* Stuff in the label that didn't match. */
  int leftoversf;		/* True if leftovers is nonempty. */
  static charclass letters;	/* Set of characters considered letters. */
  static charclass newline;	/* Set of characters that aren't newline. */
  position_set follows;		/* Union of the follows of some group. */
  position_set tmp;		/* Temporary space for merging sets. */
  int state;			/* New state. */
  int wants_newline;		/* New state wants to know newline context. */
  int state_newline;		/* New state on a newline transition. */
  int wants_letter;		/* New state wants to know letter context. */
  int state_letter;		/* New state on a letter transition. */
  static initialized;		/* Flag for static initialization. */
  int i, j, k;

  /* Initialize the set of letters, if necessary. */
  if (! initialized)
    {
      initialized = 1;
      for (i = 0; i < NOTCHAR; ++i)
	if (ISALNUM(i))
	  setbit(i, letters);
      setbit('\n', newline);
    }

  zeroset(matches);

  for (i = 0; i < d->states[s].elems.nelem; ++i)
    {
      pos = d->states[s].elems.elems[i];
      if (d->tokens[pos.index] >= 0 && d->tokens[pos.index] < NOTCHAR)
	setbit(d->tokens[pos.index], matches);
      else if (d->tokens[pos.index] >= CSET)
	copyset(d->charclasses[d->tokens[pos.index] - CSET], matches);
      else
	continue;

      /* Some characters may need to be eliminated from matches because
	 they fail in the current context. */
      if (pos.constraint != 0xFF)
	{
	  if (! MATCHES_NEWLINE_CONTEXT(pos.constraint,
					 d->states[s].newline, 1))
	    clrbit('\n', matches);
	  if (! MATCHES_NEWLINE_CONTEXT(pos.constraint,
					 d->states[s].newline, 0))
	    for (j = 0; j < CHARCLASS_INTS; ++j)
	      matches[j] &= newline[j];
	  if (! MATCHES_LETTER_CONTEXT(pos.constraint,
					d->states[s].letter, 1))
	    for (j = 0; j < CHARCLASS_INTS; ++j)
	      matches[j] &= ~letters[j];
	  if (! MATCHES_LETTER_CONTEXT(pos.constraint,
					d->states[s].letter, 0))
	    for (j = 0; j < CHARCLASS_INTS; ++j)
	      matches[j] &= letters[j];

	  /* If there are no characters left, there's no point in going on. */
	  for (j = 0; j < CHARCLASS_INTS && !matches[j]; ++j)
	    ;
	  if (j == CHARCLASS_INTS)
	    continue;
	}

      for (j = 0; j < ngrps; ++j)
	{
	  /* If matches contains a single character only, and the current
	     group's label doesn't contain that character, go on to the
	     next group. */
	  if (d->tokens[pos.index] >= 0 && d->tokens[pos.index] < NOTCHAR
	      && !tstbit(d->tokens[pos.index], labels[j]))
	    continue;

	  /* Check if this group's label has a nonempty intersection with
	     matches. */
	  intersectf = 0;
	  for (k = 0; k < CHARCLASS_INTS; ++k)
	    (intersect[k] = matches[k] & labels[j][k]) ? intersectf = 1 : 0;
	  if (! intersectf)
	    continue;

	  /* It does; now find the set differences both ways. */
	  leftoversf = matchesf = 0;
	  for (k = 0; k < CHARCLASS_INTS; ++k)
	    {
	      /* Even an optimizing compiler can't know this for sure. */
	      int match = matches[k], label = labels[j][k];

	      (leftovers[k] = ~match & label) ? leftoversf = 1 : 0;
	      (matches[k] = match & ~label) ? matchesf = 1 : 0;
	    }

	  /* If there were leftovers, create a new group labeled with them. */
	  if (leftoversf)
	    {
	      copyset(leftovers, labels[ngrps]);
	      copyset(intersect, labels[j]);
	      MALLOC(grps[ngrps].elems, position, d->nleaves);
	      copy(&grps[j], &grps[ngrps]);
	      ++ngrps;
	    }

	  /* Put the position in the current group.  Note that there is no
	     reason to call insert() here. */
	  grps[j].elems[grps[j].nelem++] = pos;

	  /* If every character matching the current position has been
	     accounted for, we're done. */
	  if (! matchesf)
	    break;
	}

      /* If we've passed the last group, and there are still characters
	 unaccounted for, then we'll have to create a new group. */
      if (j == ngrps)
	{
	  copyset(matches, labels[ngrps]);
	  zeroset(matches);
	  MALLOC(grps[ngrps].elems, position, d->nleaves);
	  grps[ngrps].nelem = 1;
	  grps[ngrps].elems[0] = pos;
	  ++ngrps;
	}
    }

  MALLOC(follows.elems, position, d->nleaves);
  MALLOC(tmp.elems, position, d->nleaves);

  /* If we are a searching matcher, the default transition is to a state
     containing the positions of state 0, otherwise the default transition
     is to fail miserably. */
  if (d->searchflag)
    {
      wants_newline = 0;
      wants_letter = 0;
      for (i = 0; i < d->states[0].elems.nelem; ++i)
	{
	  if (PREV_NEWLINE_DEPENDENT(d->states[0].elems.elems[i].constraint))
	    wants_newline = 1;
	  if (PREV_LETTER_DEPENDENT(d->states[0].elems.elems[i].constraint))
	    wants_letter = 1;
	}
      copy(&d->states[0].elems, &follows);
      state = state_index(d, &follows, 0, 0);
      if (wants_newline)
	state_newline = state_index(d, &follows, 1, 0);
      else
	state_newline = state;
      if (wants_letter)
	state_letter = state_index(d, &follows, 0, 1);
      else
	state_letter = state;
      for (i = 0; i < NOTCHAR; ++i)
	if (i == '\n')
	  trans[i] = state_newline;
	else if (ISALNUM(i))
	  trans[i] = state_letter;
	else
	  trans[i] = state;
    }
  else
    for (i = 0; i < NOTCHAR; ++i)
      trans[i] = -1;

  for (i = 0; i < ngrps; ++i)
    {
      follows.nelem = 0;

      /* Find the union of the follows of the positions of the group.
	 This is a hideously inefficient loop.  Fix it someday. */
      for (j = 0; j < grps[i].nelem; ++j)
	for (k = 0; k < d->follows[grps[i].elems[j].index].nelem; ++k)
	  insert(d->follows[grps[i].elems[j].index].elems[k], &follows);

      /* If we are building a searching matcher, throw in the positions
	 of state 0 as well. */
      if (d->searchflag)
	for (j = 0; j < d->states[0].elems.nelem; ++j)
	  insert(d->states[0].elems.elems[j], &follows);

      /* Find out if the new state will want any context information. */
      wants_newline = 0;
      if (tstbit('\n', labels[i]))
	for (j = 0; j < follows.nelem; ++j)
	  if (PREV_NEWLINE_DEPENDENT(follows.elems[j].constraint))
	    wants_newline = 1;

      wants_letter = 0;
      for (j = 0; j < CHARCLASS_INTS; ++j)
	if (labels[i][j] & letters[j])
	  break;
      if (j < CHARCLASS_INTS)
	for (j = 0; j < follows.nelem; ++j)
	  if (PREV_LETTER_DEPENDENT(follows.elems[j].constraint))
	    wants_letter = 1;

      /* Find the state(s) corresponding to the union of the follows. */
      state = state_index(d, &follows, 0, 0);
      if (wants_newline)
	state_newline = state_index(d, &follows, 1, 0);
      else
	state_newline = state;
      if (wants_letter)
	state_letter = state_index(d, &follows, 0, 1);
      else
	state_letter = state;

      /* Set the transitions for each character in the current label. */
      for (j = 0; j < CHARCLASS_INTS; ++j)
	for (k = 0; k < INTBITS; ++k)
	  if (labels[i][j] & 1 << k)
	    {
	      int c = j * INTBITS + k;

	      if (c == '\n')
		trans[c] = state_newline;
	      else if (ISALNUM(c))
		trans[c] = state_letter;
	      else if (c < NOTCHAR)
		trans[c] = state;
	    }
    }

  for (i = 0; i < ngrps; ++i)
    free(grps[i].elems);
  free(follows.elems);
  free(tmp.elems);
}

/* Some routines for manipulating a compiled dfa's transition tables.
   Each state may or may not have a transition table; if it does, and it
   is a non-accepting state, then d->trans[state] points to its table.
   If it is an accepting state then d->fails[state] points to its table.
   If it has no table at all, then d->trans[state] is NULL.
   TODO: Improve this comment, get rid of the unnecessary redundancy. */

static void
build_state(s, d)
     int s;
     struct dfa *d;
{
  int *trans;			/* The new transition table. */
  int i;

  /* Set an upper limit on the number of transition tables that will ever
     exist at once.  1024 is arbitrary.  The idea is that the frequently
     used transition tables will be quickly rebuilt, whereas the ones that
     were only needed once or twice will be cleared away. */
  if (d->trcount >= 1024)
    {
      for (i = 0; i < d->tralloc; ++i)
	if (d->trans[i])
	  {
	    free((ptr_t) d->trans[i]);
	    d->trans[i] = NULL;
	  }
	else if (d->fails[i])
	  {
	    free((ptr_t) d->fails[i]);
	    d->fails[i] = NULL;
	  }
      d->trcount = 0;
    }

  ++d->trcount;

  /* Set up the success bits for this state. */
  d->success[s] = 0;
  if (ACCEPTS_IN_CONTEXT(d->states[s].newline, 1, d->states[s].letter, 0,
      s, *d))
    d->success[s] |= 4;
  if (ACCEPTS_IN_CONTEXT(d->states[s].newline, 0, d->states[s].letter, 1,
      s, *d))
    d->success[s] |= 2;
  if (ACCEPTS_IN_CONTEXT(d->states[s].newline, 0, d->states[s].letter, 0,
      s, *d))
    d->success[s] |= 1;

  MALLOC(trans, int, NOTCHAR);
  dfastate(s, d, trans);

  /* Now go through the new transition table, and make sure that the trans
     and fail arrays are allocated large enough to hold a pointer for the
     largest state mentioned in the table. */
  for (i = 0; i < NOTCHAR; ++i)
    if (trans[i] >= d->tralloc)
      {
	int oldalloc = d->tralloc;

	while (trans[i] >= d->tralloc)
	  d->tralloc *= 2;
	REALLOC(d->realtrans, int *, d->tralloc + 1);
	d->trans = d->realtrans + 1;
	REALLOC(d->fails, int *, d->tralloc);
	REALLOC(d->success, int, d->tralloc);
	REALLOC(d->newlines, int, d->tralloc);
	while (oldalloc < d->tralloc)
	  {
	    d->trans[oldalloc] = NULL;
	    d->fails[oldalloc++] = NULL;
	  }
      }

  /* Keep the newline transition in a special place so we can use it as
     a sentinel. */
  d->newlines[s] = trans['\n'];
  trans['\n'] = -1;

  if (ACCEPTING(s, *d))
    d->fails[s] = trans;
  else
    d->trans[s] = trans;
}

static void
build_state_zero(d)
     struct dfa *d;
{
  d->tralloc = 1;
  d->trcount = 0;
  CALLOC(d->realtrans, int *, d->tralloc + 1);
  d->trans = d->realtrans + 1;
  CALLOC(d->fails, int *, d->tralloc);
  MALLOC(d->success, int, d->tralloc);
  MALLOC(d->newlines, int, d->tralloc);
  build_state(0, d);
}

/* Search through a buffer looking for a match to the given struct dfa.
   Find the first occurrence of a string matching the regexp in the buffer,
   and the shortest possible version thereof.  Return a pointer to the first
   character after the match, or NULL if none is found.  Begin points to
   the beginning of the buffer, and end points to the first character after
   its end.  We store a newline in *end to act as a sentinel, so end had
   better point somewhere valid.  Newline is a flag indicating whether to
   allow newlines to be in the matching string.  If count is non-
   NULL it points to a place we're supposed to increment every time we
   see a newline.  Finally, if backref is non-NULL it points to a place
   where we're supposed to store a 1 if backreferencing happened and the
   match needs to be verified by a backtracking matcher.  Otherwise
   we store a 0 in *backref. */
char *
dfaexec(d, begin, end, newline, count, backref)
     struct dfa *d;
     char *begin;
     char *end;
     int newline;
     int *count;
     int *backref;
{
  register s, s1, tmp;		/* Current state. */
  register unsigned char *p;	/* Current input character. */
  register **trans, *t;		/* Copy of d->trans so it can be optimized
				   into a register. */
  static sbit[NOTCHAR];	/* Table for anding with d->success. */
  static sbit_init;

  if (! sbit_init)
    {
      int i;

      sbit_init = 1;
      for (i = 0; i < NOTCHAR; ++i)
	if (i == '\n')
	  sbit[i] = 4;
	else if (ISALNUM(i))
	  sbit[i] = 2;
	else
	  sbit[i] = 1;
    }

  if (! d->tralloc)
    build_state_zero(d);

  s = s1 = 0;
  p = (unsigned char *) begin;
  trans = d->trans;
  *end = '\n';

  for (;;)
    {
      /* The dreaded inner loop. */
      if ((t = trans[s]) != 0)
	do
	  {
	    s1 = t[*p++];
	    if (! (t = trans[s1]))
	      goto last_was_s;
	    s = t[*p++];
	  }
        while ((t = trans[s]) != 0);
      goto last_was_s1;
    last_was_s:
      tmp = s, s = s1, s1 = tmp;
    last_was_s1:

      if (s >= 0 && p <= (unsigned char *) end && d->fails[s])
	{
	  if (d->success[s] & sbit[*p])
	    {
	      if (backref)
		if (d->states[s].backref)
		  *backref = 1;
		else
		  *backref = 0;
	      return (char *) p;
	    }

	  s1 = s;
	  s = d->fails[s][*p++];
	  continue;
	}

      /* If the previous character was a newline, count it. */
      if (count && (char *) p <= end && p[-1] == '\n')
	++*count;

      /* Check if we've run off the end of the buffer. */
      if ((char *) p > end)
	return NULL;

      if (s >= 0)
	{
	  build_state(s, d);
	  trans = d->trans;
	  continue;
	}

      if (p[-1] == '\n' && newline)
	{
	  s = d->newlines[s1];
	  continue;
	}

      s = 0;
    }
}

/* Initialize the components of a dfa that the other routines don't
   initialize for themselves. */
void
dfainit(d)
     struct dfa *d;
{
  d->calloc = 1;
  MALLOC(d->charclasses, charclass, d->calloc);
  d->cindex = 0;

  d->talloc = 1;
  MALLOC(d->tokens, token, d->talloc);
  d->tindex = d->depth = d->nleaves = d->nregexps = 0;

  d->searchflag = 0;
  d->tralloc = 0;

  d->musts = 0;
}

/* Parse and analyze a single string of the given length. */
void
dfacomp(s, len, d, searchflag)
     char *s;
     size_t len;
     struct dfa *d;
     int searchflag;
{
  if (case_fold)	/* dummy folding in service of dfamust() */
    {
      char *copy;
      int i;

      copy = malloc(len);
      if (!copy)
	dfaerror("out of memory");
      
      /* This is a kludge. */
      case_fold = 0;
      for (i = 0; i < len; ++i)
	if (ISUPPER(s[i]))
	  copy[i] = tolower(s[i]);
	else
	  copy[i] = s[i];

      dfainit(d);
      dfaparse(copy, len, d);
      free(copy);
      dfamust(d);
      d->cindex = d->tindex = d->depth = d->nleaves = d->nregexps = 0;
      case_fold = 1;
      dfaparse(s, len, d);
      dfaanalyze(d, searchflag);
    }
  else
    {
        dfainit(d);
        dfaparse(s, len, d);
	dfamust(d);
        dfaanalyze(d, searchflag);
    }
}

/* Free the storage held by the components of a dfa. */
void
dfafree(d)
     struct dfa *d;
{
  int i;
  struct dfamust *dm, *ndm;

  free((ptr_t) d->charclasses);
  free((ptr_t) d->tokens);
  for (i = 0; i < d->sindex; ++i)
    free((ptr_t) d->states[i].elems.elems);
  free((ptr_t) d->states);
  for (i = 0; i < d->tindex; ++i)
    if (d->follows[i].elems)
      free((ptr_t) d->follows[i].elems);
  free((ptr_t) d->follows);
  for (i = 0; i < d->tralloc; ++i)
    if (d->trans[i])
      free((ptr_t) d->trans[i]);
    else if (d->fails[i])
      free((ptr_t) d->fails[i]);
  free((ptr_t) d->realtrans);
  free((ptr_t) d->fails);
  free((ptr_t) d->newlines);
  for (dm = d->musts; dm; dm = ndm)
    {
      ndm = dm->next;
      free(dm->must);
      free((ptr_t) dm);
    }
}

/* Having found the postfix representation of the regular expression,
   try to find a long sequence of characters that must appear in any line
   containing the r.e.
   Finding a "longest" sequence is beyond the scope here;
   we take an easy way out and hope for the best.
   (Take "(ab|a)b"--please.)

   We do a bottom-up calculation of sequences of characters that must appear
   in matches of r.e.'s represented by trees rooted at the nodes of the postfix
   representation:
	sequences that must appear at the left of the match ("left")
	sequences that must appear at the right of the match ("right")
	lists of sequences that must appear somewhere in the match ("in")
	sequences that must constitute the match ("is")

   When we get to the root of the tree, we use one of the longest of its
   calculated "in" sequences as our answer.  The sequence we find is returned in
   d->must (where "d" is the single argument passed to "dfamust");
   the length of the sequence is returned in d->mustn.

   The sequences calculated for the various types of node (in pseudo ANSI c)
   are shown below.  "p" is the operand of unary operators (and the left-hand
   operand of binary operators); "q" is the right-hand operand of binary
   operators.

   "ZERO" means "a zero-length sequence" below.

	Type	left		right		is		in
	----	----		-----		--		--
	char c	# c		# c		# c		# c
	
	CSET	ZERO		ZERO		ZERO		ZERO
	
	STAR	ZERO		ZERO		ZERO		ZERO

	QMARK	ZERO		ZERO		ZERO		ZERO

	PLUS	p->left		p->right	ZERO		p->in

	CAT	(p->is==ZERO)?	(q->is==ZERO)?	(p->is!=ZERO &&	p->in plus
		p->left :	q->right :	q->is!=ZERO) ?	q->in plus
		p->is##q->left	p->right##q->is	p->is##q->is :	p->right##q->left
						ZERO
					
	OR	longest common	longest common	(do p->is and	substrings common to
		leading		trailing	q->is have same	p->in and q->in
		(sub)sequence	(sub)sequence	length and	
		of p->left	of p->right	content) ?	
		and q->left	and q->right	p->is : NULL	

   If there's anything else we recognize in the tree, all four sequences get set
   to zero-length sequences.  If there's something we don't recognize in the tree,
   we just return a zero-length sequence.

   Break ties in favor of infrequent letters (choosing 'zzz' in preference to
   'aaa')?

   And. . .is it here or someplace that we might ponder "optimizations" such as
	egrep 'psi|epsilon'	->	egrep 'psi'
	egrep 'pepsi|epsilon'	->	egrep 'epsi'
					(Yes, we now find "epsi" as a "string
					that must occur", but we might also
					simplify the *entire* r.e. being sought)
	grep '[c]'		->	grep 'c'
	grep '(ab|a)b'		->	grep 'ab'
	grep 'ab*'		->	grep 'a'
	grep 'a*b'		->	grep 'b'

   There are several issues:

   Is optimization easy (enough)?

   Does optimization actually accomplish anything,
   or is the automaton you get from "psi|epsilon" (for example)
   the same as the one you get from "psi" (for example)?
  
   Are optimizable r.e.'s likely to be used in real-life situations
   (something like 'ab*' is probably unlikely; something like is
   'psi|epsilon' is likelier)? */

static char *
icatalloc(old, new)
     char *old;
     char *new;
{
  char *result;
  int oldsize, newsize;

  newsize = (new == NULL) ? 0 : strlen(new);
  if (old == NULL)
    oldsize = 0;
  else if (newsize == 0)
    return old;
  else	oldsize = strlen(old);
  if (old == NULL)
    result = (char *) malloc(newsize + 1);
  else
    result = (char *) realloc((void *) old, oldsize + newsize + 1);
  if (result != NULL && new != NULL)
    (void) strcpy(result + oldsize, new);
  return result;
}

static char *
icpyalloc(string)
     char *string;
{
  return icatalloc((char *) NULL, string);
}

static char *
istrstr(lookin, lookfor)
     char *lookin;
     char *lookfor;
{
  char *cp;
  int len;

  len = strlen(lookfor);
  for (cp = lookin; *cp != '\0'; ++cp)
    if (strncmp(cp, lookfor, len) == 0)
      return cp;
  return NULL;
}

static void
ifree(cp)
     char *cp;
{
  if (cp != NULL)
    free(cp);
}

static void
freelist(cpp)
     char **cpp;
{
  int i;

  if (cpp == NULL)
    return;
  for (i = 0; cpp[i] != NULL; ++i)
    {
      free(cpp[i]);
      cpp[i] = NULL;
    }
}

static char **
enlist(cpp, new, len)
     char **cpp;
     char *new;
     int len;
{
  int i, j;

  if (cpp == NULL)
    return NULL;
  if ((new = icpyalloc(new)) == NULL)
    {
      freelist(cpp);
      return NULL;
    }
  new[len] = '\0';
  /* Is there already something in the list that's new (or longer)? */
  for (i = 0; cpp[i] != NULL; ++i)
    if (istrstr(cpp[i], new) != NULL)
      {
	free(new);
	return cpp;
      }
  /* Eliminate any obsoleted strings. */
  j = 0;
  while (cpp[j] != NULL)
    if (istrstr(new, cpp[j]) == NULL)
      ++j;
    else
      {
	free(cpp[j]);
	if (--i == j)
	  break;
	cpp[j] = cpp[i];
	cpp[i] = NULL;
      }
  /* Add the new string. */
  cpp = (char **) realloc((char *) cpp, (i + 2) * sizeof *cpp);
  if (cpp == NULL)
    return NULL;
  cpp[i] = new;
  cpp[i + 1] = NULL;
  return cpp;
}

/* Given pointers to two strings, return a pointer to an allocated
   list of their distinct common substrings. Return NULL if something
   seems wild. */
static char **
comsubs(left, right)
     char *left;
     char *right;
{
  char **cpp;
  char *lcp;
  char *rcp;
  int i, len;

  if (left == NULL || right == NULL)
    return NULL;
  cpp = (char **) malloc(sizeof *cpp);
  if (cpp == NULL)
    return NULL;
  cpp[0] = NULL;
  for (lcp = left; *lcp != '\0'; ++lcp)
    {
      len = 0;
      rcp = index(right, *lcp);
      while (rcp != NULL)
	{
	  for (i = 1; lcp[i] != '\0' && lcp[i] == rcp[i]; ++i)
	    ;
	  if (i > len)
	    len = i;
	  rcp = index(rcp + 1, *lcp);
	}
      if (len == 0)
	continue;
      if ((cpp = enlist(cpp, lcp, len)) == NULL)
	break;
    }
  return cpp;
}

static char **
addlists(old, new)
char **old;
char **new;
{
  int i;

  if (old == NULL || new == NULL)
    return NULL;
  for (i = 0; new[i] != NULL; ++i)
    {
      old = enlist(old, new[i], strlen(new[i]));
      if (old == NULL)
	break;
    }
  return old;
}

/* Given two lists of substrings, return a new list giving substrings
   common to both. */
static char **
inboth(left, right)
     char **left;
     char **right;
{
  char **both;
  char **temp;
  int lnum, rnum;

  if (left == NULL || right == NULL)
    return NULL;
  both = (char **) malloc(sizeof *both);
  if (both == NULL)
    return NULL;
  both[0] = NULL;
  for (lnum = 0; left[lnum] != NULL; ++lnum)
    {
      for (rnum = 0; right[rnum] != NULL; ++rnum)
	{
	  temp = comsubs(left[lnum], right[rnum]);
	  if (temp == NULL)
	    {
	      freelist(both);
	      return NULL;
	    }
	  both = addlists(both, temp);
	  freelist(temp);
	  if (both == NULL)
	    return NULL;
	}
    }
  return both;
}

typedef struct
{
  char **in;
  char *left;
  char *right;
  char *is;
} must;

static void
resetmust(mp)
must *mp;
{
  mp->left[0] = mp->right[0] = mp->is[0] = '\0';
  freelist(mp->in);
}

static void
dfamust(dfa)
struct dfa *dfa;
{
  must *musts;
  must *mp;
  char *result;
  int ri;
  int i;
  int exact;
  token t;
  static must must0;
  struct dfamust *dm;

  result = "";
  exact = 0;
  musts = (must *) malloc((dfa->tindex + 1) * sizeof *musts);
  if (musts == NULL)
    return;
  mp = musts;
  for (i = 0; i <= dfa->tindex; ++i)
    mp[i] = must0;
  for (i = 0; i <= dfa->tindex; ++i)
    {
      mp[i].in = (char **) malloc(sizeof *mp[i].in);
      mp[i].left = malloc(2);
      mp[i].right = malloc(2);
      mp[i].is = malloc(2);
      if (mp[i].in == NULL || mp[i].left == NULL ||
	  mp[i].right == NULL || mp[i].is == NULL)
	goto done;
      mp[i].left[0] = mp[i].right[0] = mp[i].is[0] = '\0';
      mp[i].in[0] = NULL;
    }
#ifdef DEBUG
  fprintf(stderr, "dfamust:\n");
  for (i = 0; i < dfa->tindex; ++i)
    {
      fprintf(stderr, " %d:", i);
      prtok(dfa->tokens[i]);
    }
  putc('\n', stderr);
#endif
  for (ri = 0; ri < dfa->tindex; ++ri)
    {
      switch (t = dfa->tokens[ri])
	{
	case LPAREN:
	case RPAREN:
	  goto done;		/* "cannot happen" */
	case EMPTY:
	case BEGLINE:
	case ENDLINE:
	case BEGWORD:
	case ENDWORD:
	case LIMWORD:
	case NOTLIMWORD:
	case BACKREF:
	  resetmust(mp);
	  break;
	case STAR:
	case QMARK:
	  if (mp <= musts)
	    goto done;		/* "cannot happen" */
	  --mp;
	  resetmust(mp);
	  break;
	case OR:
	case ORTOP:
	  if (mp < &musts[2])
	    goto done;		/* "cannot happen" */
	  {
	    char **new;
	    must *lmp;
	    must *rmp;
	    int j, ln, rn, n;

	    rmp = --mp;
	    lmp = --mp;
	    /* Guaranteed to be.  Unlikely, but. . . */
	    if (strcmp(lmp->is, rmp->is) != 0)
	      lmp->is[0] = '\0';
	    /* Left side--easy */
	    i = 0;
	    while (lmp->left[i] != '\0' && lmp->left[i] == rmp->left[i])
	      ++i;
	    lmp->left[i] = '\0';
	    /* Right side */
	    ln = strlen(lmp->right);
	    rn = strlen(rmp->right);
	    n = ln;
	    if (n > rn)
	      n = rn;
	    for (i = 0; i < n; ++i)
	      if (lmp->right[ln - i - 1] != rmp->right[rn - i - 1])
		break;
	    for (j = 0; j < i; ++j)
	      lmp->right[j] = lmp->right[(ln - i) + j];
	    lmp->right[j] = '\0';
	    new = inboth(lmp->in, rmp->in);
	    if (new == NULL)
	      goto done;
	    freelist(lmp->in);
	    free((char *) lmp->in);
	    lmp->in = new;
	  }
	  break;
	case PLUS:
	  if (mp <= musts)
	    goto done;		/* "cannot happen" */
	  --mp;
	  mp->is[0] = '\0';
	  break;
	case END:
	  if (mp != &musts[1])
	    goto done;		/* "cannot happen" */
	  for (i = 0; musts[0].in[i] != NULL; ++i)
	    if (strlen(musts[0].in[i]) > strlen(result))
	      result = musts[0].in[i];
	  if (strcmp(result, musts[0].is) == 0)
	    exact = 1;
	  goto done;
	case CAT:
	  if (mp < &musts[2])
	    goto done;		/* "cannot happen" */
	  {
	    must *lmp;
	    must *rmp;

	    rmp = --mp;
	    lmp = --mp;
	    /* In.  Everything in left, plus everything in
	       right, plus catenation of
	       left's right and right's left. */
	    lmp->in = addlists(lmp->in, rmp->in);
	    if (lmp->in == NULL)
	      goto done;
	    if (lmp->right[0] != '\0' &&
		rmp->left[0] != '\0')
	      {
		char *tp;

		tp = icpyalloc(lmp->right);
		if (tp == NULL)
		  goto done;
		tp = icatalloc(tp, rmp->left);
		if (tp == NULL)
		  goto done;
		lmp->in = enlist(lmp->in, tp,
				 strlen(tp));
		free(tp);
		if (lmp->in == NULL)
		  goto done;
	      }
	    /* Left-hand */
	    if (lmp->is[0] != '\0')
	      {
		lmp->left = icatalloc(lmp->left,
				      rmp->left);
		if (lmp->left == NULL)
		  goto done;
	      }
	    /* Right-hand */
	    if (rmp->is[0] == '\0')
	      lmp->right[0] = '\0';
	    lmp->right = icatalloc(lmp->right, rmp->right);
	    if (lmp->right == NULL)
	      goto done;
	    /* Guaranteed to be */
	    if (lmp->is[0] != '\0' && rmp->is[0] != '\0')
	      {
		lmp->is = icatalloc(lmp->is, rmp->is);
		if (lmp->is == NULL)
		  goto done;
	      }
	    else
	      lmp->is[0] = '\0';
	  }
	  break;
	default:
	  if (t < END)
	    {
	      /* "cannot happen" */
	      goto done;
	    }
	  else if (t == '\0')
	    {
	      /* not on *my* shift */
	      goto done;
	    }
	  else if (t >= CSET)
	    {
	      /* easy enough */
	      resetmust(mp);
	    }
	  else
	    {
	      /* plain character */
	      resetmust(mp);
	      mp->is[0] = mp->left[0] = mp->right[0] = t;
	      mp->is[1] = mp->left[1] = mp->right[1] = '\0';
	      mp->in = enlist(mp->in, mp->is, 1);
	      if (mp->in == NULL)
		goto done;
	    }
	  break;
	}
#ifdef DEBUG
      fprintf(stderr, " node: %d:", ri);
      prtok(dfa->tokens[ri]);
      fprintf(stderr, "\n  in:");
      for (i = 0; mp->in[i]; ++i)
	fprintf(stderr, " \"%s\"", mp->in[i]);
      fprintf(stderr, "\n  is: \"%s\"\n", mp->is);
      fprintf(stderr, "  left: \"%s\"\n", mp->left);
      fprintf(stderr, "  right: \"%s\"\n", mp->right);
#endif
      ++mp;
    }
 done:
  if (strlen(result))
    {
      dm = (struct dfamust *) malloc(sizeof (struct dfamust));
      dm->exact = exact;
      dm->must = malloc(strlen(result) + 1);
      strcpy(dm->must, result);
      dm->next = dfa->musts;
      dfa->musts = dm;
    }
  mp = musts;
  for (i = 0; i <= dfa->tindex; ++i)
    {
      freelist(mp[i].in);
      ifree((char *) mp[i].in);
      ifree(mp[i].left);
      ifree(mp[i].right);
      ifree(mp[i].is);
    }
  free((char *) mp);
}
