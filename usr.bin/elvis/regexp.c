/* regexp.c */

/* This file contains the code that compiles regular expressions and executes
 * them.  It supports the same syntax and features as vi's regular expression
 * code.  Specifically, the meta characters are:
 *	^	matches the beginning of a line
 *	$	matches the end of a line
 *	\<	matches the beginning of a word
 *	\>	matches the end of a word
 *	.	matches any single character
 *	[]	matches any character in a character class
 *	\(	delimits the start of a subexpression
 *	\)	delimits the end of a subexpression
 *	*	repeats the preceding 0 or more times
 * NOTE: You cannot follow a \) with a *.
 *
 * The physical structure of a compiled RE is as follows:
 *	- First, there is a one-byte value that says how many character classes
 *	  are used in this regular expression
 *	- Next, each character class is stored as a bitmap that is 256 bits
 *	  (32 bytes) long.
 *	- A mixture of literal characters and compiled meta characters follows.
 *	  This begins with M_BEGIN(0) and ends with M_END(0).  All meta chars
 *	  are stored as a \n followed by a one-byte code, so they take up two
 *	  bytes apiece.  Literal characters take up one byte apiece.  \n can't
 *	  be used as a literal character.
 *
 * If NO_MAGIC is defined, then a different set of functions is used instead.
 * That right, this file contains TWO versions of the code.
 */

#include <setjmp.h>
#include "config.h"
#include "ctype.h"
#include "vi.h"
#ifdef REGEX
# include <regex.h>
#else
# include "regexp.h"
#endif


#ifdef REGEX
extern int patlock;		/* from cmd_substitute() module */

static regex_t	*previous = NULL;	/* the previous regexp, used when null regexp is given */

regex_t *
optpat(s)
	char *s;
{
	char *neuter();
	char *expand_tilde();

	int n;
	if (*s == '\0') {
		if (!previous) regerr("no previous pattern");
		return previous;
	} else if (previous && !patlock)
		regfree(previous);
	else if ((previous = (regex_t *) malloc(sizeof(regex_t))) == NULL) {
		regerr("out of memory");
		return previous;
	}
	patlock = 0;
	if ((s = *o_magic ? expand_tilde(s) : neuter(s)) == NULL) {
		free(previous);
		return previous = NULL;
	} else if (n = regcomp(previous, s, *o_ignorecase ? REG_ICASE : 0)) {
		regerr("%d", n);
		free(previous);
		return previous = NULL;
	}
	return previous;
}

extern char *last_repl; 	/* replacement text from previous substitute */

/* expand_tilde: expand ~'s in a BRE */
char *
expand_tilde(s)
	char *s;
{
	char *literalize();
	static char *hd = NULL;

	char *t, *repl;
	int size;
	int offset;
	int m;

	free(hd);
	hd = t = malloc(size = strlen(s) + 1);
	while (*s)
		if (*s == '\\' && *(s + 1) == '~') {
			*t++ = *s++;
			*t++ = *s++;
		} else if (*s != '~')
			*t++ = *s++;
		else {
			if (!last_repl) {
				regerr("no previous replacement");
				return NULL;
			} else if ((repl = literalize(last_repl)) == NULL)
				return NULL;
			m = strlen(repl);
			offset = t - hd;
			if ((hd = realloc(hd, size += m)) == NULL) {
				regerr("out of memory");
				return NULL;
			}
			t = hd + offset;
			strcpy(t, repl);
			t += m;
			s++;
		}
	*t = '\0';
	return hd;
}


/* escape BRE meta-characters and expand ~'s in a string */
char *
neuter(s)
	char *s;
{
	char *literalize();
	static char *hd = NULL;

	char *t, *repl;
	int size;
	int offset;
	int m;

	free(hd);
	if ((hd = t = (char *) malloc(size = 2 * strlen(s) + 1)) == NULL) {
		regerr("out of memory");
		return NULL;
	}
	if (*s == '^')
		*t++ = *s++;
	while (*s) {
		if (*s == '\\' && (*(s + 1) == '.' || *(s + 1) == '*' || 
		    *(s + 1) == '[')) {
		    	s++;
		    	*t++ = *s++;
		    	continue;
		} else if (*s == '\\' && *(s + 1) == '~') {
			if (!last_repl) {
				regerr("no previous replacement");
				return NULL;
			} else if ((repl = literalize(last_repl)) == NULL)
				return NULL;
			m = strlen(repl);
			offset = t - hd;
			if ((hd = realloc(hd, size += m)) == NULL) {
				regerr("out of memory");
				return NULL;
			}
			t = hd + (offset);
			strcpy(t, repl);
			t += m;
			s += 2;
			continue;
		} else if (*s == '.' || *s == '\\' || *s == '[' || *s == '*')
			*t++ = '\\';
		*t++ = *s++;
	}
	*t = '\0';
	return hd;
}


/* escape BRE meta-characters in a string */
char *
literalize(s)
	char *s;
{
	static char *hd = NULL;

	char *t;
	int size;
	int offset;
	int m;

	free(hd);
	if ((hd = t = (char *) malloc(size = 2 * strlen(s) + 1)) == NULL) {
		regerr("out of memory");
		return NULL;
	}
	if (*o_magic)
		while (*s) {
			if (*s == '~' || *s == '&') {
				regerr("can't use ~ or & in pattern");
				return NULL;
			} else if (*s == '\\') {
				s++;
				if (isdigit(*s)) {
					regerr("can't use \\d in pattern");
					return NULL;
				} else if (*s == '&' || *s == '~') {
					*t++ = '\\';
					*t++ = *s++;
				}
			} else if (*s == '^' || *s == '$' || *s == '.' || 
			    *s == '[' || *s == '*') {
				*t++ = '\\';
				*t++ = *s++;
			} else
				*t++ = *s++;
		}
	else
		while (*s) {
			if (*s == '\\') {
				s++;
				if (*s == '&' || *s == '~') {
					regerr("can't use \\~ or \\& in pattern");
					return NULL;
				} else if (isdigit(*s)) {
					regerr("can't use \\d in pattern");
					return NULL;
				}
			} else
				*t++ = *s++;
		}
	*t = '\0';
	return hd;
}

#else
static char	*previous;	/* the previous regexp, used when null regexp is given */


#ifndef NO_MAGIC
/* THE REAL REGEXP PACKAGE IS USED UNLESS "NO_MAGIC" IS DEFINED */

/* These are used to classify or recognize meta-characters */
#define META		'\0'
#define BASE_META(m)	((m) - 256)
#define INT_META(c)	((c) + 256)
#define IS_META(m)	((m) >= 256)
#define IS_CLASS(m)	((m) >= M_CLASS(0) && (m) <= M_CLASS(9))
#define IS_START(m)	((m) >= M_START(0) && (m) <= M_START(9))
#define IS_END(m)	((m) >= M_END(0) && (m) <= M_END(9))
#define IS_CLOSURE(m)	((m) >= M_SPLAT && (m) <= M_RANGE)
#define ADD_META(s,m)	(*(s)++ = META, *(s)++ = BASE_META(m))
#define GET_META(s)	(*(s) == META ? INT_META(*++(s)) : *s)

/* These are the internal codes used for each type of meta-character */
#define M_BEGLINE	256		/* internal code for ^ */
#define M_ENDLINE	257		/* internal code for $ */
#define M_BEGWORD	258		/* internal code for \< */
#define M_ENDWORD	259		/* internal code for \> */
#define M_ANY		260		/* internal code for . */
#define M_SPLAT		261		/* internal code for * */
#define M_PLUS		262		/* internal code for \+ */
#define M_QMARK		263		/* internal code for \? */
#define M_RANGE		264		/* internal code for \{ */
#define M_CLASS(n)	(265+(n))	/* internal code for [] */
#define M_START(n)	(275+(n))	/* internal code for \( */
#define M_END(n)	(285+(n))	/* internal code for \) */

/* These are used during compilation */
static int	class_cnt;	/* used to assign class IDs */
static int	start_cnt;	/* used to assign start IDs */
static int	end_stk[NSUBEXP];/* used to assign end IDs */
static int	end_sp;
static char	*retext;	/* points to the text being compiled */

/* error-handling stuff */
jmp_buf	errorhandler;
#define FAIL(why)	regerr(why); longjmp(errorhandler, 1)





/* This function builds a bitmap for a particular class */
static char *makeclass(text, bmap)
	REG char	*text;	/* start of the class */
	REG char	*bmap;	/* the bitmap */
{
	REG int		i;
	int		complement = 0;


	checkmem();

	/* zero the bitmap */
	for (i = 0; bmap && i < 32; i++)
	{
		bmap[i] = 0;
	}

	/* see if we're going to complement this class */
	if (*text == '^')
	{
		text++;
		complement = 1;
	}

	/* add in the characters */
	while (*text && *text != ']')
	{
		/* is this a span of characters? */
		if (text[1] == '-' && text[2])
		{
			/* spans can't be backwards */
			if (text[0] > text[2])
			{
				FAIL("Backwards span in []");
			}

			/* add each character in the span to the bitmap */
			for (i = UCHAR(text[0]); bmap && (unsigned)i <= UCHAR(text[2]); i++)
			{
				bmap[i >> 3] |= (1 << (i & 7));
			}

			/* move past this span */
			text += 3;
		}
		else
		{
			/* add this single character to the span */
			i = *text++;
			if (bmap)
			{
				bmap[UCHAR(i) >> 3] |= (1 << (UCHAR(i) & 7));
			}
		}
	}

	/* make sure the closing ] is missing */
	if (*text++ != ']')
	{
		FAIL("] missing");
	}

	/* if we're supposed to complement this class, then do so */
	if (complement && bmap)
	{
		for (i = 0; i < 32; i++)
		{
			bmap[i] = ~bmap[i];
		}
	}

	checkmem();

	return text;
}




/* This function gets the next character or meta character from a string.
 * The pointer is incremented by 1, or by 2 for \-quoted characters.  For [],
 * a bitmap is generated via makeclass() (if re is given), and the
 * character-class text is skipped.
 */
static int gettoken(sptr, re)
	char	**sptr;
	regexp	*re;
{
	int	c;

	c = **sptr;
	if (!c)
	{
		return c;
	}
	++*sptr;
	if (c == '\\')
	{
		c = **sptr;
		++*sptr;
		switch (c)
		{
		  case '<':
			return M_BEGWORD;

		  case '>':
			return M_ENDWORD;

		  case '(':
			if (start_cnt >= NSUBEXP)
			{
				FAIL("Too many \\(s");
			}
			end_stk[end_sp++] = start_cnt;
			return M_START(start_cnt++);

		  case ')':
			if (end_sp <= 0)
			{
				FAIL("Mismatched \\)");
			}
			return M_END(end_stk[--end_sp]);

		  case '*':
			return (*o_magic ? c : M_SPLAT);

		  case '.':
			return (*o_magic ? c : M_ANY);

		  case '+':
			return M_PLUS;

		  case '?':
			return M_QMARK;
#ifndef CRUNCH
		  case '{':
			return M_RANGE;
#endif
		  default:
			return c;
		}
	}
	else if (*o_magic)
	{
		switch (c)
		{
		  case '^':
			if (*sptr == retext + 1)
			{
				return M_BEGLINE;
			}
			return c;

		  case '$':
			if (!**sptr)
			{
				return M_ENDLINE;
			}
			return c;

		  case '.':
			return M_ANY;

		  case '*':
			return M_SPLAT;

		  case '[':
			/* make sure we don't have too many classes */
			if (class_cnt >= 10)
			{
				FAIL("Too many []s");
			}

			/* process the character list for this class */
			if (re)
			{
				/* generate the bitmap for this class */
				*sptr = makeclass(*sptr, re->program + 1 + 32 * class_cnt);
			}
			else
			{
				/* skip to end of the class */
				*sptr = makeclass(*sptr, (char *)0);
			}
			return M_CLASS(class_cnt++);

		  default:
			return c;
		}
	}
	else	/* unquoted nomagic */
	{
		switch (c)
		{
		  case '^':
			if (*sptr == retext + 1)
			{
				return M_BEGLINE;
			}
			return c;

		  case '$':
			if (!**sptr)
			{
				return M_ENDLINE;
			}
			return c;

		  default:
			return c;
		}
	}
	/*NOTREACHED*/
}




/* This function calculates the number of bytes that will be needed for a
 * compiled RE.  Its argument is the uncompiled version.  It is not clever
 * about catching syntax errors; that is done in a later pass.
 */
static unsigned calcsize(text)
	char		*text;
{
	unsigned	size;
	int		token;

	retext = text;
	class_cnt = 0;
	start_cnt = 1;
	end_sp = 0;
	size = 5;
	while ((token = gettoken(&text, (regexp *)0)) != 0)
	{
		if (IS_CLASS(token))
		{
			size += 34;
		}
#ifndef CRUNCH
		else if (token == M_RANGE)
		{
			size += 4;
			while ((token = gettoken(&text, (regexp *)0)) != 0
			    && token != '}')
			{
			}
			if (!token)
			{
				return size;
			}
		}
#endif
		else if (IS_META(token))
		{
			size += 2;
		}
		else
		{
			size++;
		}
	}

	return size;
}



/* This function compiles a regexp. */
regexp *regcomp(exp)
	char		*exp;
{
	int		needfirst;
	unsigned	size;
	int		token;
	int		peek;
	char		*build;
#if __STDC__
    volatile
#endif
	regexp		*re;
#ifndef CRUNCH
	int		from;
	int		to;
	int		digit;
#endif
#ifdef DEBUG
	int		calced;
#endif


	checkmem();

	/* prepare for error handling */
	re = (regexp *)0;
	if (setjmp(errorhandler))
	{
		checkmem();
		if (re)
		{
			_free_(re);
		}
		return (regexp *)0;
	}

	/* if an empty regexp string was given, use the previous one */
	if (*exp == 0)
	{
		if (!previous)
		{
			FAIL("No previous RE");
		}
		exp = previous;
	}
	else /* non-empty regexp given, so remember it */
	{
		if (previous)
			_free_(previous);
		previous = (char *)malloc((unsigned)(strlen(exp) + 1));
		if (previous)
			strcpy(previous, exp);
	}

	/* allocate memory */
	checkmem();
	class_cnt = 0;
	start_cnt = 1;
	end_sp = 0;
	retext = exp;
#ifdef DEBUG
	calced = calcsize(exp);
	size = calced + sizeof(regexp);
#else
	size = calcsize(exp) + sizeof(regexp) + 10; /* !!! 10 bytes for slop */
#endif
#ifdef lint
	re = (regexp *)0;
#else
	re = (regexp *)malloc((unsigned)size);
#endif
	if (!re)
	{
		FAIL("Not enough memory for this RE");
	}
	checkmem();

	/* compile it */
	build = &re->program[1 + 32 * class_cnt];
	re->program[0] = class_cnt;
	for (token = 0; token < NSUBEXP; token++)
	{
		re->startp[token] = re->endp[token] = (char *)0;
	}
	re->first = 0;
	re->bol = 0;
	re->minlen = 0;
	needfirst = 1;
	class_cnt = 0;
	start_cnt = 1;
	end_sp = 0;
	retext = exp;
	for (token = M_START(0), peek = gettoken(&exp, re);
	     token;
	     token = peek, peek = gettoken(&exp, re))
	{
		/* special processing for the closure operator */
		if (IS_CLOSURE(peek))
		{
			/* detect misuse of closure operator */
			if (IS_START(token))
			{
				FAIL("Closure operator follows nothing");
			}
			else if (IS_META(token) && token != M_ANY && !IS_CLASS(token))
			{
				FAIL("Closure operators can only follow a normal character or . or []");
			}

#ifndef CRUNCH
			/* if \{ \} then read the range */
			if (peek == M_RANGE)
			{
				from = 0;
				for (digit = gettoken(&exp, re);
				     !IS_META(digit) && isdigit(digit);
				     digit = gettoken(&exp, re))
				{
					from = from * 10 + digit - '0';
				}
				if (digit == '}')
				{
					to = from;
				}
				else if (digit == ',')
				{
					to = 0;
					for (digit = gettoken(&exp, re);
					     !IS_META(digit) && isdigit(digit);
					     digit = gettoken(&exp, re))
					{
						to = to * 10 + digit - '0';
					}
					if (to == 0)
					{
						to = 255;
					}
				}
				if (digit != '}')
				{
					FAIL("Bad characters after \\{");
				}
				else if (to < from || to == 0 || from >= 255)
				{
					FAIL("Invalid range for \\{ \\}");
				}
				re->minlen += from;
			}
			else
#endif
			if (peek != M_SPLAT)
			{
				re->minlen++;
			}

			/* it is okay -- make it prefix instead of postfix */
			ADD_META(build, peek);
#ifndef CRUNCH
			if (peek == M_RANGE)
			{
				*build++ = from;
				*build++ = (to < 255 ? to : 255);
			}
#endif
			

			/* take care of "needfirst" - is this the first char? */
			if (needfirst && peek == M_PLUS && !IS_META(token))
			{
				re->first = token;
			}
			needfirst = 0;

			/* we used "peek" -- need to refill it */
			peek = gettoken(&exp, re);
			if (IS_CLOSURE(peek))
			{
				FAIL("* or \\+ or \\? doubled up");
			}
		}
		else if (!IS_META(token))
		{
			/* normal char is NOT argument of closure */
			if (needfirst)
			{
				re->first = token;
				needfirst = 0;
			}
			re->minlen++;
		}
		else if (token == M_ANY || IS_CLASS(token))
		{
			/* . or [] is NOT argument of closure */
			needfirst = 0;
			re->minlen++;
		}

		/* the "token" character is not closure -- process it normally */
		if (token == M_BEGLINE)
		{
			/* set the BOL flag instead of storing M_BEGLINE */
			re->bol = 1;
		}
		else if (IS_META(token))
		{
			ADD_META(build, token);
		}
		else
		{
			*build++ = token;
		}
	}
	checkmem();

	/* end it with a \) which MUST MATCH the opening \( */
	ADD_META(build, M_END(0));
	if (end_sp > 0)
	{
		FAIL("Not enough \\)s");
	}

#ifdef DEBUG
	if ((int)(build - re->program) != calced)
	{
		msg("regcomp error: calced=%d, actual=%d", calced, (int)(build - re->program));
		getkey(0);
	}
#endif

	checkmem();
	return re;
}



/*---------------------------------------------------------------------------*/


/* This function checks for a match between a character and a token which is
 * known to represent a single character.  It returns 0 if they match, or
 * 1 if they don't.
 */
int match1(re, ch, token)
	regexp		*re;
	REG char	ch;
	REG int		token;
{
	if (!ch)
	{
		/* the end of a line can't match any RE of width 1 */
		return 1;
	}
	if (token == M_ANY)
	{
		return 0;
	}
	else if (IS_CLASS(token))
	{
		if (re->program[1 + 32 * (token - M_CLASS(0)) + (UCHAR(ch) >> 3)] & (1 << (UCHAR(ch) & 7)))
			return 0;
	}
	else if (ch == token || *o_ignorecase && tolower(ch) == tolower(token))
	{
		return 0;
	}
	return 1;
}



/* This function checks characters up to and including the next closure, at
 * which point it does a recursive call to check the rest of it.  This function
 * returns 0 if everything matches, or 1 if something doesn't match.
 */
int match(re, str, prog, here)
	regexp		*re;	/* the regular expression */
	char		*str;	/* the string */
	REG char	*prog;	/* a portion of re->program, an compiled RE */
	REG char	*here;	/* a portion of str, the string to compare it to */
{
	REG int		token;	/* the roken pointed to by prog */
	REG int		nmatched;/* counter, used during closure matching */ 
	REG int		closure;/* the token denoting the type of closure */
	int		from;	/* minimum number of matches in closure */
	int		to;	/* maximum number of matches in closure */

	for (token = GET_META(prog); !IS_CLOSURE(token); prog++, token = GET_META(prog))
	{
		switch (token)
		{
		/*case M_BEGLINE: can't happen; re->bol is used instead */
		  case M_ENDLINE:
			if (*here)
				return 1;
			break;

		  case M_BEGWORD:
			if (here != str &&
			   (here[-1] == '_' || isalnum(here[-1])))
				return 1;
			break;

		  case M_ENDWORD:
			if (here[0] == '_' || isalnum(here[0]))
				return 1;
			break;

		  case M_START(0):
		  case M_START(1):
		  case M_START(2):
		  case M_START(3):
		  case M_START(4):
		  case M_START(5):
		  case M_START(6):
		  case M_START(7):
		  case M_START(8):
		  case M_START(9):
			re->startp[token - M_START(0)] = (char *)here;
			break;

		  case M_END(0):
		  case M_END(1):
		  case M_END(2):
		  case M_END(3):
		  case M_END(4):
		  case M_END(5):
		  case M_END(6):
		  case M_END(7):
		  case M_END(8):
		  case M_END(9):
			re->endp[token - M_END(0)] = (char *)here;
			if (token == M_END(0))
			{
				return 0;
			}
			break;

		  default: /* literal, M_CLASS(n), or M_ANY */
			if (match1(re, *here, token) != 0)
				return 1;
			here++;
		}
	}

	/* C L O S U R E */

	/* step 1: see what we have to match against, and move "prog" to point
	 * to the remainder of the compiled RE.
	 */
	closure = token;
	prog++;
	switch (closure)
	{
	  case M_SPLAT:
		from = 0;
		to = strlen(str);	/* infinity */
		break;

	  case M_PLUS:
		from = 1;
		to = strlen(str);	/* infinity */
		break;

	  case M_QMARK:
		from = 0;
		to = 1;
		break;

#ifndef CRUNCH
	  case M_RANGE:
		from = UCHAR(*prog++);
		to = UCHAR(*prog++);
		if (to == 255)
		{
			to = strlen(str); /* infinity */
		}
		break;
#endif
	}
	token = GET_META(prog);
	prog++;

	/* step 2: see how many times we can match that token against the string */
	for (nmatched = 0;
	     nmatched < to && *here && match1(re, *here, token) == 0;
	     nmatched++, here++)
	{
	}

	/* step 3: try to match the remainder, and back off if it doesn't */
	while (nmatched >= from && match(re, str, prog, here) != 0)
	{
		nmatched--;
		here--;
	}

	/* so how did it work out? */
	if (nmatched >= from)
		return 0;
	return 1;
}



/* This function searches through a string for text that matches an RE. */
int regexec(re, str, bol)
	regexp	*re;	/* the compiled regexp to search for */
	char	*str;	/* the string to search through */
	int	bol;	/* boolean: does str start at the beginning of a line? */
{
	char	*prog;	/* the entry point of re->program */
	int	len;	/* length of the string */
	REG char	*here;

	checkmem();

	/* if must start at the beginning of a line, and this isn't, then fail */
	if (re->bol && !bol)
	{
		return 0;
	}

	len = strlen(str);
	prog = re->program + 1 + 32 * re->program[0];

	/* search for the RE in the string */
	if (re->bol)
	{
		/* must occur at BOL */
		if ((re->first
			&& match1(re, *(char *)str, re->first))/* wrong first letter? */
		 || len < re->minlen			/* not long enough? */
		 || match(re, (char *)str, prog, str))	/* doesn't match? */
			return 0;			/* THEN FAIL! */
	}
#ifndef CRUNCH
	else if (!*o_ignorecase)
	{
		/* can occur anywhere in the line, noignorecase */
		for (here = (char *)str;
		     (re->first && re->first != *here)
			|| match(re, (char *)str, prog, here);
		     here++, len--)
		{
			if (len < re->minlen)
				return 0;
		}
	}
#endif
	else
	{
		/* can occur anywhere in the line, ignorecase */
		for (here = (char *)str;
		     (re->first && match1(re, *here, (int)re->first))
			|| match(re, (char *)str, prog, here);
		     here++, len--)
		{
			if (len < re->minlen)
				return 0;
		}
	}

	/* if we didn't fail, then we must have succeeded */
	checkmem();
	return 1;
}

/*============================================================================*/
#else /* NO_MAGIC */

regexp *regcomp(exp)
	char	*exp;
{
	char	*src;
	char	*dest;
	regexp	*re;
	int	i;

	/* allocate a big enough regexp structure */
#ifdef lint
	re = (regexp *)0;
#else
	re = (regexp *)malloc((unsigned)(strlen(exp) + 1 + sizeof(struct regexp)));
#endif
	if (!re)
	{
		regerr("Could not malloc a regexp structure");
		return (regexp *)0;
	}

	/* initialize all fields of the structure */
	for (i = 0; i < NSUBEXP; i++)
	{
		re->startp[i] = re->endp[i] = (char *)0;
	}
	re->minlen = 0;
	re->first = 0;
	re->bol = 0;

	/* copy the string into it, translating ^ and $ as needed */
	for (src = exp, dest = re->program + 1; *src; src++)
	{
		switch (*src)
		{
		  case '^':
			if (src == exp)
			{
				re->bol += 1;
			}
			else
			{
				*dest++ = '^';
				re->minlen++;
			}
			break;

		  case '$':
			if (!src[1])
			{
				re->bol += 2;
			}
			else
			{
				*dest++ = '$';
				re->minlen++;
			}
			break;

		  case '\\':
			if (src[1])
			{
				*dest++ = *++src;
				re->minlen++;
			}
			else
			{
				regerr("extra \\ at end of regular expression");
			}
			break;

		  default:
			*dest++ = *src;
			re->minlen++;
		}
	}
	*dest = '\0';

	return re;
}


/* This "helper" function checks for a match at a given location.  It returns
 * 1 if it matches, 0 if it doesn't match here but might match later on in the
 * string, or -1 if it could not possibly match
 */
static int reghelp(prog, string, bolflag)
	struct regexp	*prog;
	char		*string;
	int		bolflag;
{
	char		*scan;
	char		*str;

	/* if ^, then require bolflag */
	if ((prog->bol & 1) && !bolflag)
	{
		return -1;
	}

	/* if it matches, then it will start here */
	prog->startp[0] = string;

	/* compare, possibly ignoring case */
	if (*o_ignorecase)
	{
		for (scan = &prog->program[1]; *scan; scan++, string++)
			if (tolower(*scan) != tolower(*string))
				return *string ? 0 : -1;
	}
	else
	{
		for (scan = &prog->program[1]; *scan; scan++, string++)
			if (*scan != *string)
				return *string ? 0 : -1;
	}

	/* if $, then require string to end here, too */
	if ((prog->bol & 2) && *string)
	{
		return 0;
	}

	/* if we get to here, it matches */
	prog->endp[0] = string;
	return 1;
}



int regexec(prog, string, bolflag)
	struct regexp	*prog;
	char		*string;
	int		bolflag;
{
	int		rc;

	/* keep trying to match it */
	for (rc = reghelp(prog, string, bolflag); rc == 0; rc = reghelp(prog, string, 0))
	{
		string++;
	}

	/* did we match? */
	return rc == 1;
}
#endif
#endif /* !REGEX */
