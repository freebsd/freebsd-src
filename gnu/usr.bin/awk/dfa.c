/* dfa.c - determinisitic extended regexp routines for GNU
   Copyright (C) 1988 Free Software Foundation, Inc.
                      Written June, 1988 by Mike Haertel
		      Modified July, 1988 by Arthur David Olson
			 to assist BMG speedups

		       NO WARRANTY

  BECAUSE THIS PROGRAM IS LICENSED FREE OF CHARGE, WE PROVIDE ABSOLUTELY
NO WARRANTY, TO THE EXTENT PERMITTED BY APPLICABLE STATE LAW.  EXCEPT
WHEN OTHERWISE STATED IN WRITING, FREE SOFTWARE FOUNDATION, INC,
RICHARD M. STALLMAN AND/OR OTHER PARTIES PROVIDE THIS PROGRAM "AS IS"
WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY
AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE PROGRAM PROVE
DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR OR
CORRECTION.

 IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW WILL RICHARD M.
STALLMAN, THE FREE SOFTWARE FOUNDATION, INC., AND/OR ANY OTHER PARTY
WHO MAY MODIFY AND REDISTRIBUTE THIS PROGRAM AS PERMITTED BELOW, BE
LIABLE TO YOU FOR DAMAGES, INCLUDING ANY LOST PROFITS, LOST MONIES, OR
OTHER SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE
USE OR INABILITY TO USE (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR
DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY THIRD PARTIES OR
A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS) THIS
PROGRAM, EVEN IF YOU HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES, OR FOR ANY CLAIM BY ANY OTHER PARTY.

		GENERAL PUBLIC LICENSE TO COPY

  1. You may copy and distribute verbatim copies of this source file
as you receive it, in any medium, provided that you conspicuously and
appropriately publish on each copy a valid copyright notice "Copyright
 (C) 1988 Free Software Foundation, Inc."; and include following the
copyright notice a verbatim copy of the above disclaimer of warranty
and of this License.  You may charge a distribution fee for the
physical act of transferring a copy.

  2. You may modify your copy or copies of this source file or
any portion of it, and copy and distribute such modifications under
the terms of Paragraph 1 above, provided that you also do the following:

    a) cause the modified files to carry prominent notices stating
    that you changed the files and the date of any change; and

    b) cause the whole of any work that you distribute or publish,
    that in whole or in part contains or is a derivative of this
    program or any part thereof, to be licensed at no charge to all
    third parties on terms identical to those contained in this
    License Agreement (except that you may choose to grant more extensive
    warranty protection to some or all third parties, at your option).

    c) You may charge a distribution fee for the physical act of
    transferring a copy, and you may at your option offer warranty
    protection in exchange for a fee.

Mere aggregation of another unrelated program with this program (or its
derivative) on a volume of a storage or distribution medium does not bring
the other program under the scope of these terms.

  3. You may copy and distribute this program or any portion of it in
compiled, executable or object code form under the terms of Paragraphs
1 and 2 above provided that you do the following:

    a) accompany it with the complete corresponding machine-readable
    source code, which must be distributed under the terms of
    Paragraphs 1 and 2 above; or,

    b) accompany it with a written offer, valid for at least three
    years, to give any third party free (except for a nominal
    shipping charge) a complete machine-readable copy of the
    corresponding source code, to be distributed under the terms of
    Paragraphs 1 and 2 above; or,

    c) accompany it with the information you received as to where the
    corresponding source code may be obtained.  (This alternative is
    allowed only for noncommercial distribution and only if you
    received the program in object code or executable form alone.)

For an executable file, complete source code means all the source code for
all modules it contains; but, as a special exception, it need not include
source code for modules which are standard libraries that accompany the
operating system on which the executable file runs.

  4. You may not copy, sublicense, distribute or transfer this program
except as expressly provided under this License Agreement.  Any attempt
otherwise to copy, sublicense, distribute or transfer this program is void and
your rights to use the program under this License agreement shall be
automatically terminated.  However, parties who have received computer
software programs from you with this License Agreement will not have
their licenses terminated so long as such parties remain in full compliance.

  5. If you wish to incorporate parts of this program into other free
programs whose distribution conditions are different, write to the Free
Software Foundation at 675 Mass Ave, Cambridge, MA 02139.  We have not yet
worked out a simple rule that can be stated here, but we will often permit
this.  We will be guided by the two goals of preserving the free status of
all derivatives our free software and of promoting the sharing and reuse of
software.


In other words, you are welcome to use, share and improve this program.
You are forbidden to forbid anyone else to use, share and improve
what you give them.   Help stamp out software-hoarding!  */

#include "awk.h"
#include <assert.h>

#ifdef setbit  /* surprise - setbit and clrbit are macros on NeXT */
#undef setbit
#endif
#ifdef clrbit
#undef clrbit
#endif

#ifdef __STDC__
typedef void *ptr_t;
#else
typedef char *ptr_t;
#endif

typedef struct {
	char **	in;
	char *	left;
	char *	right;
	char *	is;
} must;

static ptr_t xcalloc P((int n, size_t s));
static ptr_t xmalloc P((size_t n));
static ptr_t xrealloc P((ptr_t p, size_t n));
static int tstbit P((int b, _charset c));
static void setbit P((int b, _charset c));
static void clrbit P((int b, _charset c));
static void copyset P((const _charset src, _charset dst));
static void zeroset P((_charset s));
static void notset P((_charset s));
static int equal P((const _charset s1, const _charset s2));
static int charset_index P((const _charset s));
static _token lex P((void));
static void addtok P((_token t));
static void atom P((void));
static void closure P((void));
static void branch P((void));
static void regexp P((void));
static void copy P((const _position_set *src, _position_set *dst));
static void insert P((_position p, _position_set *s));
static void merge P((_position_set *s1, _position_set *s2, _position_set *m));
static void delete P((_position p, _position_set *s));
static int state_index P((struct regexp *r, _position_set *s,
			  int newline, int letter));
static void epsclosure P((_position_set *s, struct regexp *r));
static void build_state P((int s, struct regexp *r));
static void build_state_zero P((struct regexp *r));
static char *icatalloc P((char *old, const char *new));
static char *icpyalloc P((const char *string));
static char *istrstr P((char *lookin, char *lookfor));
static void ifree P((char *cp));
static void freelist P((char **cpp));
static char **enlist P((char **cpp, char *new, size_t len));
static char **comsubs P((char *left, char *right));
static char **addlists P((char **old, char **new));
static char **inboth P((char **left, char **right));
static void resetmust P((must *mp));
static void regmust P((struct regexp *r));

#undef P

static ptr_t
xcalloc(n, s)
     int n;
     size_t s;
{
  ptr_t r = calloc(n, s);

  if (NULL == r)
    reg_error("Memory exhausted");  /* reg_error does not return */
  return r;
}

static ptr_t
xmalloc(n)
     size_t n;
{
  ptr_t r = malloc(n);

  assert(n != 0);
  if (NULL == r)
    reg_error("Memory exhausted");
  return r;
}

static ptr_t
xrealloc(p, n)
     ptr_t p;
     size_t n;
{
  ptr_t r = realloc(p, n);

  assert(n != 0);
  if (NULL == r)
    reg_error("Memory exhausted");
  return r;
}

#define CALLOC(p, t, n) ((p) = (t *) xcalloc((n), sizeof (t)))
#undef MALLOC
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

/* Stuff pertaining to charsets. */

static int
tstbit(b, c)
     int b;
     _charset c;
{
  return c[b / INTBITS] & 1 << b % INTBITS;
}

static void
setbit(b, c)
     int b;
     _charset c;
{
  c[b / INTBITS] |= 1 << b % INTBITS;
}

static void
clrbit(b, c)
     int b;
     _charset c;
{
  c[b / INTBITS] &= ~(1 << b % INTBITS);
}

static void
copyset(src, dst)
     const _charset src;
     _charset dst;
{
  int i;

  for (i = 0; i < _CHARSET_INTS; ++i)
    dst[i] = src[i];
}

static void
zeroset(s)
     _charset s;
{
  int i;

  for (i = 0; i < _CHARSET_INTS; ++i)
    s[i] = 0;
}

static void
notset(s)
     _charset s;
{
  int i;

  for (i = 0; i < _CHARSET_INTS; ++i)
    s[i] = ~s[i];
}

static int
equal(s1, s2)
     const _charset s1;
     const _charset s2;
{
  int i;

  for (i = 0; i < _CHARSET_INTS; ++i)
    if (s1[i] != s2[i])
      return 0;
  return 1;
}

/* A pointer to the current regexp is kept here during parsing. */
static struct regexp *reg;

/* Find the index of charset s in reg->charsets, or allocate a new charset. */
static int
charset_index(s)
     const _charset s;
{
  int i;

  for (i = 0; i < reg->cindex; ++i)
    if (equal(s, reg->charsets[i]))
      return i;
  REALLOC_IF_NECESSARY(reg->charsets, _charset, reg->calloc, reg->cindex);
  ++reg->cindex;
  copyset(s, reg->charsets[i]);
  return i;
}

/* Syntax bits controlling the behavior of the lexical analyzer. */
static syntax_bits, syntax_bits_set;

/* Flag for case-folding letters into sets. */
static case_fold;

/* Entry point to set syntax options. */
void
regsyntax(bits, fold)
     long bits;
     int fold;
{
  syntax_bits_set = 1;
  syntax_bits = bits;
  case_fold = fold;
}

/* Lexical analyzer. */
static const char *lexstart;	/* Pointer to beginning of input string. */
static const char *lexptr;	/* Pointer to next input character. */
static lexleft;			/* Number of characters remaining. */
static caret_allowed;		/* True if backward context allows ^
				   (meaningful only if RE_CONTEXT_INDEP_OPS
				   is turned off). */
static closure_allowed;		/* True if backward context allows closures
				   (meaningful only if RE_CONTEXT_INDEP_OPS
				   is turned off). */

/* Note that characters become unsigned here. */
#define FETCH(c, eoferr)   	      \
  {			   	      \
    if (! lexleft)	   	      \
      if (eoferr != NULL)   	      \
	reg_error(eoferr);  	      \
      else		   	      \
	return _END;	   	      \
    (c) = (unsigned char) *lexptr++;  \
    --lexleft;		   	      \
  }

static _token
lex()
{
  _token c, c2;
  int invert;
  _charset cset;

  FETCH(c, (char *) 0);
  switch (c)
    {
    case '^':
      if (! (syntax_bits & RE_CONTEXT_INDEP_OPS)
	  && (!caret_allowed ||
	      ((syntax_bits & RE_TIGHT_VBAR) && lexptr - 1 != lexstart)))
	goto normal_char;
      caret_allowed = 0;
      return syntax_bits & RE_TIGHT_VBAR ? _ALLBEGLINE : _BEGLINE;

    case '$':
      if (syntax_bits & RE_CONTEXT_INDEP_OPS || !lexleft
	  || (! (syntax_bits & RE_TIGHT_VBAR)
	      && ((syntax_bits & RE_NO_BK_PARENS
		   ? lexleft > 0 && *lexptr == ')'
		   : lexleft > 1 && *lexptr == '\\' && lexptr[1] == ')')
		  || (syntax_bits & RE_NO_BK_VBAR
		      ? lexleft > 0 && *lexptr == '|'
		      : lexleft > 1 && *lexptr == '\\' && lexptr[1] == '|'))))
	return syntax_bits & RE_TIGHT_VBAR ? _ALLENDLINE : _ENDLINE;
      goto normal_char;

    case '\\':
      FETCH(c, "Unfinished \\ quote");
      switch (c)
	{
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  caret_allowed = 0;
	  closure_allowed = 1;
	  return _BACKREF;

	case '<':
	  caret_allowed = 0;
	  return _BEGWORD;

	case '>':
	  caret_allowed = 0;
	  return _ENDWORD;

	case 'b':
	  caret_allowed = 0;
	  return _LIMWORD;

	case 'B':
	  caret_allowed = 0;
	  return _NOTLIMWORD;

	case 'w':
	case 'W':
	  zeroset(cset);
	  for (c2 = 0; c2 < _NOTCHAR; ++c2)
	    if (ISALNUM(c2))
	      setbit(c2, cset);
	  if (c == 'W')
	    notset(cset);
	  caret_allowed = 0;
	  closure_allowed = 1;
	  return _SET + charset_index(cset);

	case '?':
	  if (syntax_bits & RE_BK_PLUS_QM)
	    goto qmark;
	  goto normal_char;

	case '+':
	  if (syntax_bits & RE_BK_PLUS_QM)
	    goto plus;
	  goto normal_char;

	case '|':
	  if (! (syntax_bits & RE_NO_BK_VBAR))
	    goto or;
	  goto normal_char;

	case '(':
	  if (! (syntax_bits & RE_NO_BK_PARENS))
	    goto lparen;
	  goto normal_char;

	case ')':
	  if (! (syntax_bits & RE_NO_BK_PARENS))
	    goto rparen;
	  goto normal_char;

	default:
	  goto normal_char;
	}

    case '?':
      if (syntax_bits & RE_BK_PLUS_QM)
	goto normal_char;
    qmark:
      if (! (syntax_bits & RE_CONTEXT_INDEP_OPS) && !closure_allowed)
	goto normal_char;
      return _QMARK;

    case '*':
      if (! (syntax_bits & RE_CONTEXT_INDEP_OPS) && !closure_allowed)
	goto normal_char;
      return _STAR;

    case '+':
      if (syntax_bits & RE_BK_PLUS_QM)
	goto normal_char;
    plus:
      if (! (syntax_bits & RE_CONTEXT_INDEP_OPS) && !closure_allowed)
	goto normal_char;
      return _PLUS;

    case '|':
      if (! (syntax_bits & RE_NO_BK_VBAR))
	goto normal_char;
    or:
      caret_allowed = 1;
      closure_allowed = 0;
      return _OR;

    case '\n':
      if (! (syntax_bits & RE_NEWLINE_OR))
	goto normal_char;
      goto or;

    case '(':
      if (! (syntax_bits & RE_NO_BK_PARENS))
	goto normal_char;
    lparen:
      caret_allowed = 1;
      closure_allowed = 0;
      return _LPAREN;

    case ')':
      if (! (syntax_bits & RE_NO_BK_PARENS))
	goto normal_char;
    rparen:
      caret_allowed = 0;
      closure_allowed = 1;
      return _RPAREN;

    case '.':
      zeroset(cset);
      notset(cset);
      clrbit('\n', cset);
      caret_allowed = 0;
      closure_allowed = 1;
      return _SET + charset_index(cset);

    case '[':
      zeroset(cset);
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
	  FETCH(c2, "Unbalanced [");
	  if ((syntax_bits & RE_AWK_CLASS_HACK) && c == '\\')
	    {
	      c = c2;
	      FETCH(c2, "Unbalanced [");
	    }
	  if (c2 == '-')
	    {
	      FETCH(c2, "Unbalanced [");
	      if (c2 == ']' && (syntax_bits & RE_AWK_CLASS_HACK))
		{
		  setbit(c, cset);
		  setbit('-', cset);
		  break;
	        }
	      while (c <= c2)
		  setbit(c++, cset);
	      FETCH(c, "Unbalanced [");
	    }
	  else
	    {
	      setbit(c, cset);
	      c = c2;
	    }
	}
      while (c != ']');
      if (invert)
	notset(cset);
      caret_allowed = 0;
      closure_allowed = 1;
      return _SET + charset_index(cset);

    default:
    normal_char:
      caret_allowed = 0;
      closure_allowed = 1;
      if (case_fold && ISALPHA(c))
	{
	  zeroset(cset);
	  if (isupper(c))
	    c = tolower(c);
	  setbit(c, cset);
	  setbit(toupper(c), cset);
	  return _SET + charset_index(cset);
	}
      return c;
    }
}

/* Recursive descent parser for regular expressions. */

static _token tok;		/* Lookahead token. */
static depth;			/* Current depth of a hypothetical stack
				   holding deferred productions.  This is
				   used to determine the depth that will be
				   required of the real stack later on in
				   reganalyze(). */

/* Add the given token to the parse tree, maintaining the depth count and
   updating the maximum depth if necessary. */
static void
addtok(t)
     _token t;
{
  REALLOC_IF_NECESSARY(reg->tokens, _token, reg->talloc, reg->tindex);
  reg->tokens[reg->tindex++] = t;

  switch (t)
    {
    case _QMARK:
    case _STAR:
    case _PLUS:
      break;

    case _CAT:
    case _OR:
      --depth;
      break;

    default:
      ++reg->nleaves;
    case _EMPTY:
      ++depth;
      break;
    }
  if (depth > reg->depth)
    reg->depth = depth;
}

/* The grammar understood by the parser is as follows.

   start:
     regexp
     _ALLBEGLINE regexp
     regexp _ALLENDLINE
     _ALLBEGLINE regexp _ALLENDLINE

   regexp:
     regexp _OR branch
     branch

   branch:
     branch closure
     closure

   closure:
     closure _QMARK
     closure _STAR
     closure _PLUS
     atom

   atom:
     <normal character>
     _SET
     _BACKREF
     _BEGLINE
     _ENDLINE
     _BEGWORD
     _ENDWORD
     _LIMWORD
     _NOTLIMWORD
     <empty>

   The parser builds a parse tree in postfix form in an array of tokens. */

#ifdef __STDC__
static void regexp(void);
#else
static void regexp();
#endif

static void
atom()
{
  if (tok >= 0 && (tok < _NOTCHAR || tok >= _SET || tok == _BACKREF
      || tok == _BEGLINE || tok == _ENDLINE || tok == _BEGWORD
      || tok == _ENDWORD || tok == _LIMWORD || tok == _NOTLIMWORD))
    {
      addtok(tok);
      tok = lex();
    }
  else if (tok == _LPAREN)
    {
      tok = lex();
      regexp();
      if (tok != _RPAREN)
	reg_error("Unbalanced (");
      tok = lex();
    }
  else
    addtok(_EMPTY);
}

static void
closure()
{
  atom();
  while (tok == _QMARK || tok == _STAR || tok == _PLUS)
    {
      addtok(tok);
      tok = lex();
    }
}

static void
branch()
{
  closure();
  while (tok != _RPAREN && tok != _OR && tok != _ALLENDLINE && tok >= 0)
    {
      closure();
      addtok(_CAT);
    }
}

static void
regexp()
{
  branch();
  while (tok == _OR)
    {
      tok = lex();
      branch();
      addtok(_OR);
    }
}

/* Main entry point for the parser.  S is a string to be parsed, len is the
   length of the string, so s can include NUL characters.  R is a pointer to
   the struct regexp to parse into. */
void
regparse(s, len, r)
     const char *s;
     size_t len;
     struct regexp *r;
{
  reg = r;
  lexstart = lexptr = s;
  lexleft = len;
  caret_allowed = 1;
  closure_allowed = 0;

  if (! syntax_bits_set)
    reg_error("No syntax specified");

  tok = lex();
  depth = r->depth;

  if (tok == _ALLBEGLINE)
    {
      addtok(_BEGLINE);
      tok = lex();
      regexp();
      addtok(_CAT);
    }
  else
    regexp();

  if (tok == _ALLENDLINE)
    {
      addtok(_ENDLINE);
      addtok(_CAT);
      tok = lex();
    }

  if (tok != _END)
    reg_error("Unbalanced )");

  addtok(_END - r->nregexps);
  addtok(_CAT);

  if (r->nregexps)
    addtok(_OR);

  ++r->nregexps;
}

/* Some primitives for operating on sets of positions. */

/* Copy one set to another; the destination must be large enough. */
static void
copy(src, dst)
     const _position_set *src;
     _position_set *dst;
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
     _position p;
     _position_set *s;
{
  int i;
  _position t1, t2;

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
     _position_set *s1;
     _position_set *s2;
     _position_set *m;
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
     _position p;
     _position_set *s;
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
state_index(r, s, newline, letter)
     struct regexp *r;
     _position_set *s;
     int newline;
     int letter;
{
  int lhash = 0;
  int constraint;
  int i, j;

  newline = newline ? 1 : 0;
  letter = letter ? 1 : 0;

  for (i = 0; i < s->nelem; ++i)
    lhash ^= s->elems[i].index + s->elems[i].constraint;

  /* Try to find a state that exactly matches the proposed one. */
  for (i = 0; i < r->sindex; ++i)
    {
      if (lhash != r->states[i].hash || s->nelem != r->states[i].elems.nelem
	  || newline != r->states[i].newline || letter != r->states[i].letter)
	continue;
      for (j = 0; j < s->nelem; ++j)
	if (s->elems[j].constraint
	    != r->states[i].elems.elems[j].constraint
	    || s->elems[j].index != r->states[i].elems.elems[j].index)
	  break;
      if (j == s->nelem)
	return i;
    }

  /* We'll have to create a new state. */
  REALLOC_IF_NECESSARY(r->states, _dfa_state, r->salloc, r->sindex);
  r->states[i].hash = lhash;
  MALLOC(r->states[i].elems.elems, _position, s->nelem);
  copy(s, &r->states[i].elems);
  r->states[i].newline = newline;
  r->states[i].letter = letter;
  r->states[i].backref = 0;
  r->states[i].constraint = 0;
  r->states[i].first_end = 0;
  for (j = 0; j < s->nelem; ++j)
    if (r->tokens[s->elems[j].index] < 0)
      {
	constraint = s->elems[j].constraint;
	if (_SUCCEEDS_IN_CONTEXT(constraint, newline, 0, letter, 0)
	    || _SUCCEEDS_IN_CONTEXT(constraint, newline, 0, letter, 1)
	    || _SUCCEEDS_IN_CONTEXT(constraint, newline, 1, letter, 0)
	    || _SUCCEEDS_IN_CONTEXT(constraint, newline, 1, letter, 1))
	  r->states[i].constraint |= constraint;
	if (! r->states[i].first_end)
	  r->states[i].first_end = r->tokens[s->elems[j].index];
      }
    else if (r->tokens[s->elems[j].index] == _BACKREF)
      {
	r->states[i].constraint = _NO_CONSTRAINT;
	r->states[i].backref = 1;
      }

  ++r->sindex;

  return i;
}

/* Find the epsilon closure of a set of positions.  If any position of the set
   contains a symbol that matches the empty string in some context, replace
   that position with the elements of its follow labeled with an appropriate
   constraint.  Repeat exhaustively until no funny positions are left.
   S->elems must be large enough to hold the result. */
static void
epsclosure(s, r)
     _position_set *s;
     struct regexp *r;
{
  int i, j;
  int *visited;
  _position p, old;

  MALLOC(visited, int, r->tindex);
  for (i = 0; i < r->tindex; ++i)
    visited[i] = 0;

  for (i = 0; i < s->nelem; ++i)
    if (r->tokens[s->elems[i].index] >= _NOTCHAR
	&& r->tokens[s->elems[i].index] != _BACKREF
	&& r->tokens[s->elems[i].index] < _SET)
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
	switch (r->tokens[old.index])
	  {
	  case _BEGLINE:
	    p.constraint &= _BEGLINE_CONSTRAINT;
	    break;
	  case _ENDLINE:
	    p.constraint &= _ENDLINE_CONSTRAINT;
	    break;
	  case _BEGWORD:
	    p.constraint &= _BEGWORD_CONSTRAINT;
	    break;
	  case _ENDWORD:
	    p.constraint &= _ENDWORD_CONSTRAINT;
	    break;
	  case _LIMWORD:
	    p.constraint &= _ENDWORD_CONSTRAINT;
	    break;
	  case _NOTLIMWORD:
	    p.constraint &= _NOTLIMWORD_CONSTRAINT;
	    break;
	  default:
	    break;
	  }
	for (j = 0; j < r->follows[old.index].nelem; ++j)
	  {
	    p.index = r->follows[old.index].elems[j].index;
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
   *  _EMPTY leaves are nullable.
   * No other leaf is nullable.
   * A _QMARK or _STAR node is nullable.
   * A _PLUS node is nullable if its argument is nullable.
   * A _CAT node is nullable if both its arguments are nullable.
   * An _OR node is nullable if either argument is nullable.

   Firstpos:  The firstpos of a node is the set of positions (nonempty leaves)
   that could correspond to the first character of a string matching the
   regexp rooted at the given node.
   * _EMPTY leaves have empty firstpos.
   * The firstpos of a nonempty leaf is that leaf itself.
   * The firstpos of a _QMARK, _STAR, or _PLUS node is the firstpos of its
     argument.
   * The firstpos of a _CAT node is the firstpos of the left argument, union
     the firstpos of the right if the left argument is nullable.
   * The firstpos of an _OR node is the union of firstpos of each argument.

   Lastpos:  The lastpos of a node is the set of positions that could
   correspond to the last character of a string matching the regexp at
   the given node.
   * _EMPTY leaves have empty lastpos.
   * The lastpos of a nonempty leaf is that leaf itself.
   * The lastpos of a _QMARK, _STAR, or _PLUS node is the lastpos of its
     argument.
   * The lastpos of a _CAT node is the lastpos of its right argument, union
     the lastpos of the left if the right argument is nullable.
   * The lastpos of an _OR node is the union of the lastpos of each argument.

   Follow:  The follow of a position is the set of positions that could
   correspond to the character following a character matching the node in
   a string matching the regexp.  At this point we consider special symbols
   that match the empty string in some context to be just normal characters.
   Later, if we find that a special symbol is in a follow set, we will
   replace it with the elements of its follow, labeled with an appropriate
   constraint.
   * Every node in the firstpos of the argument of a _STAR or _PLUS node is in
     the follow of every node in the lastpos.
   * Every node in the firstpos of the second argument of a _CAT node is in
     the follow of every node in the lastpos of the first argument.

   Because of the postfix representation of the parse tree, the depth-first
   analysis is conveniently done by a linear scan with the aid of a stack.
   Sets are stored as arrays of the elements, obeying a stack-like allocation
   scheme; the number of elements in each set deeper in the stack can be
   used to determine the address of a particular set's array. */
void
reganalyze(r, searchflag)
     struct regexp *r;
     int searchflag;
{
  int *nullable;		/* Nullable stack. */
  int *nfirstpos;		/* Element count stack for firstpos sets. */
  _position *firstpos;		/* Array where firstpos elements are stored. */
  int *nlastpos;		/* Element count stack for lastpos sets. */
  _position *lastpos;		/* Array where lastpos elements are stored. */
  int *nalloc;			/* Sizes of arrays allocated to follow sets. */
  _position_set tmp;		/* Temporary set for merging sets. */
  _position_set merged;		/* Result of merging sets. */
  int wants_newline;		/* True if some position wants newline info. */
  int *o_nullable;
  int *o_nfirst, *o_nlast;
  _position *o_firstpos, *o_lastpos;
  int i, j;
  _position *pos;

  r->searchflag = searchflag;

  MALLOC(nullable, int, r->depth);
  o_nullable = nullable;
  MALLOC(nfirstpos, int, r->depth);
  o_nfirst = nfirstpos;
  MALLOC(firstpos, _position, r->nleaves);
  o_firstpos = firstpos, firstpos += r->nleaves;
  MALLOC(nlastpos, int, r->depth);
  o_nlast = nlastpos;
  MALLOC(lastpos, _position, r->nleaves);
  o_lastpos = lastpos, lastpos += r->nleaves;
  MALLOC(nalloc, int, r->tindex);
  for (i = 0; i < r->tindex; ++i)
    nalloc[i] = 0;
  MALLOC(merged.elems, _position, r->nleaves);

  CALLOC(r->follows, _position_set, r->tindex);

  for (i = 0; i < r->tindex; ++i)
    switch (r->tokens[i])
      {
      case _EMPTY:
	/* The empty set is nullable. */
	*nullable++ = 1;

	/* The firstpos and lastpos of the empty leaf are both empty. */
	*nfirstpos++ = *nlastpos++ = 0;
	break;

      case _STAR:
      case _PLUS:
	/* Every element in the firstpos of the argument is in the follow
	   of every element in the lastpos. */
	tmp.nelem = nfirstpos[-1];
	tmp.elems = firstpos;
	pos = lastpos;
	for (j = 0; j < nlastpos[-1]; ++j)
	  {
	    merge(&tmp, &r->follows[pos[j].index], &merged);
	    REALLOC_IF_NECESSARY(r->follows[pos[j].index].elems, _position,
				 nalloc[pos[j].index], merged.nelem - 1);
	    copy(&merged, &r->follows[pos[j].index]);
	  }

      case _QMARK:
	/* A _QMARK or _STAR node is automatically nullable. */
	if (r->tokens[i] != _PLUS)
	  nullable[-1] = 1;
	break;

      case _CAT:
	/* Every element in the firstpos of the second argument is in the
	   follow of every element in the lastpos of the first argument. */
	tmp.nelem = nfirstpos[-1];
	tmp.elems = firstpos;
	pos = lastpos + nlastpos[-1];
	for (j = 0; j < nlastpos[-2]; ++j)
	  {
	    merge(&tmp, &r->follows[pos[j].index], &merged);
	    REALLOC_IF_NECESSARY(r->follows[pos[j].index].elems, _position,
				 nalloc[pos[j].index], merged.nelem - 1);
	    copy(&merged, &r->follows[pos[j].index]);
	  }

	/* The firstpos of a _CAT node is the firstpos of the first argument,
	   union that of the second argument if the first is nullable. */
	if (nullable[-2])
	  nfirstpos[-2] += nfirstpos[-1];
	else
	  firstpos += nfirstpos[-1];
	--nfirstpos;

	/* The lastpos of a _CAT node is the lastpos of the second argument,
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

	/* A _CAT node is nullable if both arguments are nullable. */
	nullable[-2] = nullable[-1] && nullable[-2];
	--nullable;
	break;

      case _OR:
	/* The firstpos is the union of the firstpos of each argument. */
	nfirstpos[-2] += nfirstpos[-1];
	--nfirstpos;

	/* The lastpos is the union of the lastpos of each argument. */
	nlastpos[-2] += nlastpos[-1];
	--nlastpos;

	/* An _OR node is nullable if either argument is nullable. */
	nullable[-2] = nullable[-1] || nullable[-2];
	--nullable;
	break;

      default:
	/* Anything else is a nonempty position.  (Note that special
	   constructs like \< are treated as nonempty strings here;
	   an "epsilon closure" effectively makes them nullable later.
	   Backreferences have to get a real position so we can detect
	   transitions on them later.  But they are nullable. */
	*nullable++ = r->tokens[i] == _BACKREF;

	/* This position is in its own firstpos and lastpos. */
	*nfirstpos++ = *nlastpos++ = 1;
	--firstpos, --lastpos;
	firstpos->index = lastpos->index = i;
	firstpos->constraint = lastpos->constraint = _NO_CONSTRAINT;

	/* Allocate the follow set for this position. */
	nalloc[i] = 1;
	MALLOC(r->follows[i].elems, _position, nalloc[i]);
	break;
      }

  /* For each follow set that is the follow set of a real position, replace
     it with its epsilon closure. */
  for (i = 0; i < r->tindex; ++i)
    if (r->tokens[i] < _NOTCHAR || r->tokens[i] == _BACKREF
	|| r->tokens[i] >= _SET)
      {
	copy(&r->follows[i], &merged);
	epsclosure(&merged, r);
	if (r->follows[i].nelem < merged.nelem)
	  REALLOC(r->follows[i].elems, _position, merged.nelem);
	copy(&merged, &r->follows[i]);
      }

  /* Get the epsilon closure of the firstpos of the regexp.  The result will
     be the set of positions of state 0. */
  merged.nelem = 0;
  for (i = 0; i < nfirstpos[-1]; ++i)
    insert(firstpos[i], &merged);
  epsclosure(&merged, r);

  /* Check if any of the positions of state 0 will want newline context. */
  wants_newline = 0;
  for (i = 0; i < merged.nelem; ++i)
    if (_PREV_NEWLINE_DEPENDENT(merged.elems[i].constraint))
      wants_newline = 1;

  /* Build the initial state. */
  r->salloc = 1;
  r->sindex = 0;
  MALLOC(r->states, _dfa_state, r->salloc);
  state_index(r, &merged, wants_newline, 0);

  free(o_nullable);
  free(o_nfirst);
  free(o_firstpos);
  free(o_nlast);
  free(o_lastpos);
  free(nalloc);
  free(merged.elems);
}

/* Find, for each character, the transition out of state s of r, and store
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
regstate(s, r, trans)
     int s;
     struct regexp *r;
     int trans[];
{
  _position_set grps[_NOTCHAR];	/* As many as will ever be needed. */
  _charset labels[_NOTCHAR];	/* Labels corresponding to the groups. */
  int ngrps = 0;		/* Number of groups actually used. */
  _position pos;		/* Current position being considered. */
  _charset matches;		/* Set of matching characters. */
  int matchesf;			/* True if matches is nonempty. */
  _charset intersect;		/* Intersection with some label set. */
  int intersectf;		/* True if intersect is nonempty. */
  _charset leftovers;		/* Stuff in the label that didn't match. */
  int leftoversf;		/* True if leftovers is nonempty. */
  static _charset letters;	/* Set of characters considered letters. */
  static _charset newline;	/* Set of characters that aren't newline. */
  _position_set follows;	/* Union of the follows of some group. */
  _position_set tmp;		/* Temporary space for merging sets. */
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
      for (i = 0; i < _NOTCHAR; ++i)
	if (ISALNUM(i))
	  setbit(i, letters);
      setbit('\n', newline);
    }

  zeroset(matches);

  for (i = 0; i < r->states[s].elems.nelem; ++i)
    {
      pos = r->states[s].elems.elems[i];
      if (r->tokens[pos.index] >= 0 && r->tokens[pos.index] < _NOTCHAR)
	setbit(r->tokens[pos.index], matches);
      else if (r->tokens[pos.index] >= _SET)
	copyset(r->charsets[r->tokens[pos.index] - _SET], matches);
      else
	continue;

      /* Some characters may need to be climinated from matches because
	 they fail in the current context. */
      if (pos.constraint != 0xff)
	{
	  if (! _MATCHES_NEWLINE_CONTEXT(pos.constraint,
					 r->states[s].newline, 1))
	    clrbit('\n', matches);
	  if (! _MATCHES_NEWLINE_CONTEXT(pos.constraint,
					 r->states[s].newline, 0))
	    for (j = 0; j < _CHARSET_INTS; ++j)
	      matches[j] &= newline[j];
	  if (! _MATCHES_LETTER_CONTEXT(pos.constraint,
					r->states[s].letter, 1))
	    for (j = 0; j < _CHARSET_INTS; ++j)
	      matches[j] &= ~letters[j];
	  if (! _MATCHES_LETTER_CONTEXT(pos.constraint,
					r->states[s].letter, 0))
	    for (j = 0; j < _CHARSET_INTS; ++j)
	      matches[j] &= letters[j];

	  /* If there are no characters left, there's no point in going on. */
	  for (j = 0; j < _CHARSET_INTS && !matches[j]; ++j)
	    ;
	  if (j == _CHARSET_INTS)
	    continue;
	}

      for (j = 0; j < ngrps; ++j)
	{
	  /* If matches contains a single character only, and the current
	     group's label doesn't contain that character, go on to the
	     next group. */
	  if (r->tokens[pos.index] >= 0 && r->tokens[pos.index] < _NOTCHAR
	      && !tstbit(r->tokens[pos.index], labels[j]))
	    continue;

	  /* Check if this group's label has a nonempty intersection with
	     matches. */
	  intersectf = 0;
	  for (k = 0; k < _CHARSET_INTS; ++k)
	    (intersect[k] = matches[k] & labels[j][k]) ? intersectf = 1 : 0;
	  if (! intersectf)
	    continue;

	  /* It does; now find the set differences both ways. */
	  leftoversf = matchesf = 0;
	  for (k = 0; k < _CHARSET_INTS; ++k)
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
	      MALLOC(grps[ngrps].elems, _position, r->nleaves);
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
	  MALLOC(grps[ngrps].elems, _position, r->nleaves);
	  grps[ngrps].nelem = 1;
	  grps[ngrps].elems[0] = pos;
	  ++ngrps;
	}
    }

  MALLOC(follows.elems, _position, r->nleaves);
  MALLOC(tmp.elems, _position, r->nleaves);

  /* If we are a searching matcher, the default transition is to a state
     containing the positions of state 0, otherwise the default transition
     is to fail miserably. */
  if (r->searchflag)
    {
      wants_newline = 0;
      wants_letter = 0;
      for (i = 0; i < r->states[0].elems.nelem; ++i)
	{
	  if (_PREV_NEWLINE_DEPENDENT(r->states[0].elems.elems[i].constraint))
	    wants_newline = 1;
	  if (_PREV_LETTER_DEPENDENT(r->states[0].elems.elems[i].constraint))
	    wants_letter = 1;
	}
      copy(&r->states[0].elems, &follows);
      state = state_index(r, &follows, 0, 0);
      if (wants_newline)
	state_newline = state_index(r, &follows, 1, 0);
      else
	state_newline = state;
      if (wants_letter)
	state_letter = state_index(r, &follows, 0, 1);
      else
	state_letter = state;
      for (i = 0; i < _NOTCHAR; ++i)
	trans[i] = (ISALNUM(i)) ? state_letter : state ;
      trans['\n'] = state_newline;
    }
  else
    for (i = 0; i < _NOTCHAR; ++i)
      trans[i] = -1;

  for (i = 0; i < ngrps; ++i)
    {
      follows.nelem = 0;

      /* Find the union of the follows of the positions of the group.
	 This is a hideously inefficient loop.  Fix it someday. */
      for (j = 0; j < grps[i].nelem; ++j)
	for (k = 0; k < r->follows[grps[i].elems[j].index].nelem; ++k)
	  insert(r->follows[grps[i].elems[j].index].elems[k], &follows);

      /* If we are building a searching matcher, throw in the positions
	 of state 0 as well. */
      if (r->searchflag)
	for (j = 0; j < r->states[0].elems.nelem; ++j)
	  insert(r->states[0].elems.elems[j], &follows);

      /* Find out if the new state will want any context information. */
      wants_newline = 0;
      if (tstbit('\n', labels[i]))
	for (j = 0; j < follows.nelem; ++j)
	  if (_PREV_NEWLINE_DEPENDENT(follows.elems[j].constraint))
	    wants_newline = 1;

      wants_letter = 0;
      for (j = 0; j < _CHARSET_INTS; ++j)
	if (labels[i][j] & letters[j])
	  break;
      if (j < _CHARSET_INTS)
	for (j = 0; j < follows.nelem; ++j)
	  if (_PREV_LETTER_DEPENDENT(follows.elems[j].constraint))
	    wants_letter = 1;

      /* Find the state(s) corresponding to the union of the follows. */
      state = state_index(r, &follows, 0, 0);
      if (wants_newline)
	state_newline = state_index(r, &follows, 1, 0);
      else
	state_newline = state;
      if (wants_letter)
	state_letter = state_index(r, &follows, 0, 1);
      else
	state_letter = state;

      /* Set the transitions for each character in the current label. */
      for (j = 0; j < _CHARSET_INTS; ++j)
	for (k = 0; k < INTBITS; ++k)
	  if (labels[i][j] & 1 << k)
	    {
	      int c = j * INTBITS + k;

	      if (c == '\n')
		trans[c] = state_newline;
	      else if (ISALNUM(c))
		trans[c] = state_letter;
	      else if (c < _NOTCHAR)
		trans[c] = state;
	    }
    }

  for (i = 0; i < ngrps; ++i)
    free(grps[i].elems);
  free(follows.elems);
  free(tmp.elems);
}

/* Some routines for manipulating a compiled regexp's transition tables.
   Each state may or may not have a transition table; if it does, and it
   is a non-accepting state, then r->trans[state] points to its table.
   If it is an accepting state then r->fails[state] points to its table.
   If it has no table at all, then r->trans[state] is NULL.
   TODO: Improve this comment, get rid of the unnecessary redundancy. */

static void
build_state(s, r)
     int s;
     struct regexp *r;
{
  int *trans;			/* The new transition table. */
  int i;

  /* Set an upper limit on the number of transition tables that will ever
     exist at once.  1024 is arbitrary.  The idea is that the frequently
     used transition tables will be quickly rebuilt, whereas the ones that
     were only needed once or twice will be cleared away. */
  if (r->trcount >= 1024)
    {
      for (i = 0; i < r->tralloc; ++i)
	if (r->trans[i])
	  {
	    free((ptr_t) r->trans[i]);
	    r->trans[i] = NULL;
	  }
	else if (r->fails[i])
	  {
	    free((ptr_t) r->fails[i]);
	    r->fails[i] = NULL;
	  }
      r->trcount = 0;
    }

  ++r->trcount;

  /* Set up the success bits for this state. */
  r->success[s] = 0;
  if (ACCEPTS_IN_CONTEXT(r->states[s].newline, 1, r->states[s].letter, 0,
      s, *r))
    r->success[s] |= 4;
  if (ACCEPTS_IN_CONTEXT(r->states[s].newline, 0, r->states[s].letter, 1,
      s, *r))
    r->success[s] |= 2;
  if (ACCEPTS_IN_CONTEXT(r->states[s].newline, 0, r->states[s].letter, 0,
      s, *r))
    r->success[s] |= 1;

  MALLOC(trans, int, _NOTCHAR);
  regstate(s, r, trans);

  /* Now go through the new transition table, and make sure that the trans
     and fail arrays are allocated large enough to hold a pointer for the
     largest state mentioned in the table. */
  for (i = 0; i < _NOTCHAR; ++i)
    if (trans[i] >= r->tralloc)
      {
	int oldalloc = r->tralloc;

	while (trans[i] >= r->tralloc)
	  r->tralloc *= 2;
	REALLOC(r->realtrans, int *, r->tralloc + 1);
	r->trans = r->realtrans + 1;
	REALLOC(r->fails, int *, r->tralloc);
	REALLOC(r->success, int, r->tralloc);
	REALLOC(r->newlines, int, r->tralloc);
	while (oldalloc < r->tralloc)
	  {
	    r->trans[oldalloc] = NULL;
	    r->fails[oldalloc++] = NULL;
	  }
      }

  /* Keep the newline transition in a special place so we can use it as
     a sentinel. */
  r->newlines[s] = trans['\n'];
  trans['\n'] = -1;

  if (ACCEPTING(s, *r))
    r->fails[s] = trans;
  else
    r->trans[s] = trans;
}

static void
build_state_zero(r)
     struct regexp *r;
{
  r->tralloc = 1;
  r->trcount = 0;
  CALLOC(r->realtrans, int *, r->tralloc + 1);
  r->trans = r->realtrans + 1;
  CALLOC(r->fails, int *, r->tralloc);
  MALLOC(r->success, int, r->tralloc);
  MALLOC(r->newlines, int, r->tralloc);
  build_state(0, r);
}

/* Search through a buffer looking for a match to the given struct regexp.
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
regexecute(r, begin, end, newline, count, backref)
     struct regexp *r;
     char *begin;
     char *end;
     int newline;
     int *count;
     int *backref;
{
  register s, s1, tmp;		/* Current state. */
  register unsigned char *p;	/* Current input character. */
  register **trans, *t;		/* Copy of r->trans so it can be optimized
				   into a register. */
  static sbit[_NOTCHAR];	/* Table for anding with r->success. */
  static sbit_init;

  if (! sbit_init)
    {
      int i;

      sbit_init = 1;
      for (i = 0; i < _NOTCHAR; ++i)
	sbit[i] = (ISALNUM(i)) ? 2 : 1;
      sbit['\n'] = 4;
    }

  if (! r->tralloc)
    build_state_zero(r);

  s = s1 = 0;
  p = (unsigned char *) begin;
  trans = r->trans;
  *end = '\n';

  for (;;)
    {
      while ((t = trans[s]) != 0) { /* hand-optimized loop */
	s1 = t[*p++];
        if ((t = trans[s1]) == 0) {
           tmp = s ; s = s1 ; s1 = tmp ; /* swap */
           break;
        }
	s = t[*p++];
      }

      if (s >= 0 && p <= (unsigned char *) end && r->fails[s])
	{
	  if (r->success[s] & sbit[*p])
	    {
	      if (backref)
		*backref = (r->states[s].backref != 0);
	      return (char *) p;
	    }

	  s1 = s;
	  s = r->fails[s][*p++];
	  continue;
	}

      /* If the previous character was a newline, count it. */
      if (count && (char *) p <= end && p[-1] == '\n')
	++*count;

      /* Check if we've run off the end of the buffer. */
      if ((char *) p >= end)
	return NULL;

      if (s >= 0)
	{
	  build_state(s, r);
	  trans = r->trans;
	  continue;
	}

      if (p[-1] == '\n' && newline)
	{
	  s = r->newlines[s1];
	  continue;
	}

      s = 0;
    }
}

/* Initialize the components of a regexp that the other routines don't
   initialize for themselves. */
void
reginit(r)
     struct regexp *r;
{
  r->calloc = 1;
  MALLOC(r->charsets, _charset, r->calloc);
  r->cindex = 0;

  r->talloc = 1;
  MALLOC(r->tokens, _token, r->talloc);
  r->tindex = r->depth = r->nleaves = r->nregexps = 0;

  r->searchflag = 0;
  r->tralloc = 0;
}

/* Parse and analyze a single string of the given length. */
void
regcompile(s, len, r, searchflag)
     const char *s;
     size_t len;
     struct regexp *r;
     int searchflag;
{
  if (case_fold)	/* dummy folding in service of regmust() */
    {
      char *regcopy;
      int i;

      regcopy = malloc(len);
      if (!regcopy)
	reg_error("out of memory");
      
      /* This is a complete kludge and could potentially break
	 \<letter> escapes . . . */
      case_fold = 0;
      for (i = 0; i < len; ++i)
	if (ISUPPER(s[i]))
	  regcopy[i] = tolower(s[i]);
	else
	  regcopy[i] = s[i];

      reginit(r);
      r->mustn = 0;
      r->must[0] = '\0';
      regparse(regcopy, len, r);
      free(regcopy);
      regmust(r);
      reganalyze(r, searchflag);
      case_fold = 1;
      reginit(r);
      regparse(s, len, r);
      reganalyze(r, searchflag);
    }
  else
    {
        reginit(r);
        regparse(s, len, r);
        regmust(r);
        reganalyze(r, searchflag);
    }
}

/* Free the storage held by the components of a regexp. */
void
reg_free(r)
     struct regexp *r;
{
  int i;

  free((ptr_t) r->charsets);
  free((ptr_t) r->tokens);
  for (i = 0; i < r->sindex; ++i)
    free((ptr_t) r->states[i].elems.elems);
  free((ptr_t) r->states);
  for (i = 0; i < r->tindex; ++i)
    if (r->follows[i].elems)
      free((ptr_t) r->follows[i].elems);
  free((ptr_t) r->follows);
  for (i = 0; i < r->tralloc; ++i)
    if (r->trans[i])
      free((ptr_t) r->trans[i]);
    else if (r->fails[i])
      free((ptr_t) r->fails[i]);
  if (r->realtrans)
    free((ptr_t) r->realtrans);
  if (r->fails)
    free((ptr_t) r->fails);
  if (r->newlines)
    free((ptr_t) r->newlines);
}

/*
Having found the postfix representation of the regular expression,
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
r->must (where "r" is the single argument passed to "regmust");
the length of the sequence is returned in r->mustn.

The sequences calculated for the various types of node (in pseudo ANSI c)
are shown below.  "p" is the operand of unary operators (and the left-hand
operand of binary operators); "q" is the right-hand operand of binary operators
.
"ZERO" means "a zero-length sequence" below.

Type	left		right		is		in
----	----		-----		--		--
char c	# c		# c		# c		# c

SET	ZERO		ZERO		ZERO		ZERO

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
					simplify the *entire* r.e. being sought
)
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
	'psi|epsilon' is likelier)?
*/

static char *
icatalloc(old, new)
char *	old;
const char *	new;
{
	register char *	result;
	register int	oldsize, newsize;

	newsize = (new == NULL) ? 0 : strlen(new);
	if (old == NULL)
		oldsize = 0;
	else if (newsize == 0)
		return old;
	else	oldsize = strlen(old);
	if (old == NULL)
		result = (char *) malloc(newsize + 1);
	else	result = (char *) realloc((void *) old, oldsize + newsize + 1);
	if (result != NULL && new != NULL)
		(void) strcpy(result + oldsize, new);
	return result;
}

static char *
icpyalloc(string)
const char *	string;
{
	return icatalloc((char *) NULL, string);
}

static char *
istrstr(lookin, lookfor)
char *		lookin;
register char *	lookfor;
{
	register char *	cp;
	register int	len;

	len = strlen(lookfor);
	for (cp = lookin; *cp != '\0'; ++cp)
		if (strncmp(cp, lookfor, len) == 0)
			return cp;
	return NULL;
}

static void
ifree(cp)
char *	cp;
{
	if (cp != NULL)
		free(cp);
}

static void
freelist(cpp)
register char **	cpp;
{
	register int	i;

	if (cpp == NULL)
		return;
	for (i = 0; cpp[i] != NULL; ++i) {
		free(cpp[i]);
		cpp[i] = NULL;
	}
}

static char **
enlist(cpp, new, len)
register char **	cpp;
register char *		new;
#ifdef __STDC__
size_t                  len;
#else
int                     len;
#endif
{
	register int	i, j;

	if (cpp == NULL)
		return NULL;
	if ((new = icpyalloc(new)) == NULL) {
		freelist(cpp);
		return NULL;
	}
	new[len] = '\0';
	/*
	** Is there already something in the list that's new (or longer)?
	*/
	for (i = 0; cpp[i] != NULL; ++i)
		if (istrstr(cpp[i], new) != NULL) {
			free(new);
			return cpp;
		}
	/*
	** Eliminate any obsoleted strings.
	*/
	j = 0;
	while (cpp[j] != NULL)
		if (istrstr(new, cpp[j]) == NULL)
			++j;
		else {
			free(cpp[j]);
			if (--i == j)
				break;
			cpp[j] = cpp[i];
		}
	/*
	** Add the new string.
	*/
	cpp = (char **) realloc((char *) cpp, (i + 2) * sizeof *cpp);
	if (cpp == NULL)
		return NULL;
	cpp[i] = new;
	cpp[i + 1] = NULL;
	return cpp;
}

/*
** Given pointers to two strings,
** return a pointer to an allocated list of their distinct common substrings.
** Return NULL if something seems wild.
*/

static char **
comsubs(left, right)
char *	left;
char *	right;
{
	register char **	cpp;
	register char *		lcp;
	register char *		rcp;
	register int		i, len;

	if (left == NULL || right == NULL)
		return NULL;
	cpp = (char **) malloc(sizeof *cpp);
	if (cpp == NULL)
		return NULL;
	cpp[0] = NULL;
	for (lcp = left; *lcp != '\0'; ++lcp) {
		len = 0;
		rcp = strchr(right, *lcp);
		while (rcp != NULL) {
			for (i = 1; lcp[i] != '\0' && lcp[i] == rcp[i]; ++i)
				;
			if (i > len)
				len = i;
			rcp = strchr(rcp + 1, *lcp);
		}
		if (len == 0)
			continue;
#ifdef __STDC__
		if ((cpp = enlist(cpp, lcp, (size_t)len)) == NULL)
#else
		if ((cpp = enlist(cpp, lcp, len)) == NULL)
#endif
			break;
	}
	return cpp;
}

static char **
addlists(old, new)
char **	old;
char **	new;
{
	register int	i;

	if (old == NULL || new == NULL)
		return NULL;
	for (i = 0; new[i] != NULL; ++i) {
		old = enlist(old, new[i], strlen(new[i]));
		if (old == NULL)
			break;
	}
	return old;
}

/*
** Given two lists of substrings,
** return a new list giving substrings common to both.
*/

static char **
inboth(left, right)
char **	left;
char **	right;
{
	register char **	both;
	register char **	temp;
	register int		lnum, rnum;

	if (left == NULL || right == NULL)
		return NULL;
	both = (char **) malloc(sizeof *both);
	if (both == NULL)
		return NULL;
	both[0] = NULL;
	for (lnum = 0; left[lnum] != NULL; ++lnum) {
		for (rnum = 0; right[rnum] != NULL; ++rnum) {
			temp = comsubs(left[lnum], right[rnum]);
			if (temp == NULL) {
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

/*
typedef struct {
	char **	in;
	char *	left;
	char *	right;
	char *	is;
} must;
 */
static void
resetmust(mp)
register must *	mp;
{
	mp->left[0] = mp->right[0] = mp->is[0] = '\0';
	freelist(mp->in);
}

static void
regmust(r)
register struct regexp *	r;
{
	register must *		musts;
	register must *		mp;
	register char *		result = "";
	register int		ri;
	register int		i;
	register _token		t;
	static must		must0;

	reg->mustn = 0;
	reg->must[0] = '\0';
	musts = (must *) malloc((reg->tindex + 1) * sizeof *musts);
	if (musts == NULL)
		return;
	mp = musts;
	for (i = 0; i <= reg->tindex; ++i)
		mp[i] = must0;
	for (i = 0; i <= reg->tindex; ++i) {
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
	for (ri = 0; ri < reg->tindex; ++ri) {
		switch (t = reg->tokens[ri]) {
		case _ALLBEGLINE:
		case _ALLENDLINE:
		case _LPAREN:
		case _RPAREN:
			goto done;		/* "cannot happen" */
		case _EMPTY:
		case _BEGLINE:
		case _ENDLINE:
		case _BEGWORD:
		case _ENDWORD:
		case _LIMWORD:
		case _NOTLIMWORD:
		case _BACKREF:
			resetmust(mp);
			break;
		case _STAR:
		case _QMARK:
			if (mp <= musts)
				goto done;	/* "cannot happen" */
			--mp;
			resetmust(mp);
			break;
		case _OR:
			if (mp < &musts[2])
				goto done;	/* "cannot happen" */
			{
				register char **	new;
				register must *		lmp;
				register must *		rmp;
				register int		j, ln, rn, n;

				rmp = --mp;
				lmp = --mp;
				/* Guaranteed to be.  Unlikely, but. . . */
				if (strcmp(lmp->is, rmp->is) != 0)
					lmp->is[0] = '\0';
				/* Left side--easy */
				i = 0;
				while (lmp->left[i] != '\0' &&
					lmp->left[i] == rmp->left[i])
						++i;
				lmp->left[i] = '\0';
				/* Right side */
				ln = strlen(lmp->right);
				rn = strlen(rmp->right);
				n = ln;
				if (n > rn)
					n = rn;
				for (i = 0; i < n; ++i)
					if (lmp->right[ln - i - 1] !=
					    rmp->right[rn - i - 1])
						break;
				for (j = 0; j < i; ++j)
					lmp->right[j] =
						lmp->right[(ln - i) + j];
				lmp->right[j] = '\0';
				new = inboth(lmp->in, rmp->in);
				if (new == NULL)
					goto done;
				freelist(lmp->in);
				free((char *) lmp->in);
				lmp->in = new;
			}
			break;
		case _PLUS:
			if (mp <= musts)
				goto done;	/* "cannot happen" */
			--mp;
			mp->is[0] = '\0';
			break;
		case _END:
			if (mp != &musts[1])
				goto done;	/* "cannot happen" */
			for (i = 0; musts[0].in[i] != NULL; ++i)
				if (strlen(musts[0].in[i]) > strlen(result))
					result = musts[0].in[i];
			goto done;
		case _CAT:
			if (mp < &musts[2])
				goto done;	/* "cannot happen" */
			{
				register must *	lmp;
				register must *	rmp;

				rmp = --mp;
				lmp = --mp;
				/*
				** In.  Everything in left, plus everything in
				** right, plus catenation of
				** left's right and right's left.
				*/
				lmp->in = addlists(lmp->in, rmp->in);
				if (lmp->in == NULL)
					goto done;
				if (lmp->right[0] != '\0' &&
					rmp->left[0] != '\0') {
						register char *	tp;

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
				if (lmp->is[0] != '\0') {
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
				if (lmp->is[0] != '\0' && rmp->is[0] != '\0') {
					lmp->is = icatalloc(lmp->is, rmp->is);
					if (lmp->is == NULL)
						goto done;
				}
			}
			break;
		default:
			if (t < _END) {
				/* "cannot happen" */
				goto done;
			} else if (t == '\0') {
				/* not on *my* shift */
				goto done;
			} else if (t >= _SET) {
				/* easy enough */
				resetmust(mp);
			} else {
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
		++mp;
	}
done:
	(void) strncpy(reg->must, result, MUST_MAX - 1);
	reg->must[MUST_MAX - 1] = '\0';
	reg->mustn = strlen(reg->must);
	mp = musts;
	for (i = 0; i <= reg->tindex; ++i) {
		freelist(mp[i].in);
		ifree((char *) mp[i].in);
		ifree(mp[i].left);
		ifree(mp[i].right);
		ifree(mp[i].is);
	}
	free((char *) mp);
}
