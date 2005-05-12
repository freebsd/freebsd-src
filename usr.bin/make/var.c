/*-
 * Copyright (c) 2002 Juli Mallett.
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *
 * @(#)var.c	8.3 (Berkeley) 3/19/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/**
 * var.c --
 *	Variable-handling functions
 *
 * Interface:
 *	Var_Set		Set the value of a variable in the given
 *			context. The variable is created if it doesn't
 *			yet exist. The value and variable name need not
 *			be preserved.
 *
 *	Var_Append	Append more characters to an existing variable
 *			in the given context. The variable needn't
 *			exist already -- it will be created if it doesn't.
 *			A space is placed between the old value and the
 *			new one.
 *
 *	Var_Exists	See if a variable exists.
 *
 *	Var_Value	Return the value of a variable in a context or
 *			NULL if the variable is undefined.
 *
 *	Var_Subst	Substitute named variable, or all variables if
 *			NULL in a string using
 *			the given context as the top-most one. If the
 *			third argument is non-zero, Parse_Error is
 *			called if any variables are undefined.
 *
 *	Var_Parse	Parse a variable expansion from a string and
 *			return the result and the number of characters
 *			consumed.
 *
 *	Var_Delete	Delete a variable in a context.
 *
 *	Var_Init	Initialize this module.
 *
 * Debugging:
 *	Var_Dump	Print out all variables defined in the given
 *			context.
 *
 * XXX: There's a lot of duplication in these functions.
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "buf.h"
#include "config.h"
#include "globals.h"
#include "GNode.h"
#include "job.h"
#include "make.h"
#include "parse.h"
#include "str.h"
#include "targ.h"
#include "util.h"
#include "var.h"

/**
 *
 */
typedef struct VarParser {
	const char	*const input;	/* pointer to input string */
	const char	*ptr;		/* current parser pos in input str */
	GNode		*ctxt;
	Boolean		err;
	Boolean		execute;
} VarParser;

typedef struct Var {
	char		*name;	/* the variable's name */
	struct Buffer	*val;	/* its value */
	int		flags;	/* miscellaneous status flags */

#define	VAR_IN_USE	1	/* Variable's value currently being used.
				 * Used to avoid recursion */

#define	VAR_JUNK	4	/* Variable is a junk variable that
				 * should be destroyed when done with
				 * it. Used by Var_Parse for undefined,
				 * modified variables */

#define	VAR_TO_ENV	8	/* Place variable in environment */
} Var;

typedef struct {
	struct Buffer	*lhs;	/* String to match */
	struct Buffer	*rhs;	/* Replacement string (w/ &'s removed) */

	regex_t			re;
	int			nsub;
	regmatch_t		*matches;

	int	flags;
#define	VAR_SUB_GLOBAL	0x01	/* Apply substitution globally */
#define	VAR_SUB_ONE	0x02	/* Apply substitution to one word */
#define	VAR_SUB_MATCHED	0x04	/* There was a match */
#define	VAR_MATCH_START	0x08	/* Match at start of word */
#define	VAR_MATCH_END	0x10	/* Match at end of word */
} VarPattern;

typedef Boolean VarModifyProc(const char *, Boolean, struct Buffer *, void *);

static char *VarParse(VarParser *, Boolean *);

/*
 * This is a harmless return value for Var_Parse that can be used by Var_Subst
 * to determine if there was an error in parsing -- easier than returning
 * a flag, as things outside this module don't give a hoot.
 */
char	var_Error[] = "";

/*
 * Similar to var_Error, but returned when the 'err' flag for Var_Parse is
 * set false. Why not just use a constant? Well, gcc likes to condense
 * identical string instances...
 */
static char	varNoError[] = "";

/*
 * Internally, variables are contained in four different contexts.
 *	1) the environment. They may not be changed. If an environment
 *	   variable is appended-to, the result is placed in the global
 *	   context.
 *	2) the global context. Variables set in the Makefile are located in
 *	   the global context. It is the penultimate context searched when
 *	   substituting.
 *	3) the command-line context. All variables set on the command line
 *	   are placed in this context. They are UNALTERABLE once placed here.
 *	4) the local context. Each target has associated with it a context
 *	   list. On this list are located the structures describing such
 *	   local variables as $(@) and $(*)
 * The four contexts are searched in the reverse order from which they are
 * listed.
 */
static GNode	*VAR_ENV;	/* variables from the environment */
GNode		*VAR_GLOBAL;	/* variables from the makefile */
GNode		*VAR_CMD;	/* variables defined on the command-line */

Boolean		oldVars;	/* variable substitution style */
Boolean		checkEnvFirst;	/* -e flag */

#define	OPEN_PAREN		'('
#define	CLOSE_PAREN		')'
#define	OPEN_BRACE		'{'
#define	CLOSE_BRACE		'}'

/**
 * Create a Var object.
 *
 * Params:
 *	name		Name of variable (copied).
 *	value		Value of variable (copied) or NULL.
 *	flags		Flags set on variable.
 *
 * Returns:
 *	New variable.
 */
static Var *
VarCreate(const char name[], const char value[], int flags)
{
	Var *v;

	v = emalloc(sizeof(Var));
	v->name = estrdup(name);
	v->val = Buf_Init(0);
	v->flags = flags;

	if (value != NULL) {
		Buf_Append(v->val, value);
	}
	return (v);
}

/**
 * Destroy a Var object.
 *
 * Params:
 *	v	Object to destroy.
 *	f	True if internal buffer in Buffer object is to be removed.
 */
static void
VarDestroy(Var *v, Boolean f)
{

	Buf_Destroy(v->val, f);
	free(v->name);
	free(v);
}

/**
 * Remove the tail of the given word and place the result in the given
 * buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 */
static Boolean
VarHead(const char *word, Boolean addSpace, Buffer *buf, void *dummy __unused)
{
	char *slash;

	slash = strrchr(word, '/');
	if (slash != NULL) {
		if (addSpace) {
			Buf_AddByte(buf, (Byte)' ');
		}
		Buf_AppendRange(buf, word, slash);
	} else {
		/*
		 * If no directory part, give . (q.v. the POSIX standard)
		 */
		if (addSpace) {
			Buf_Append(buf, " .");
		} else {
			Buf_AddByte(buf, (Byte)'.');
		}
	}
	return (TRUE);
}

/**
 * Remove the head of the given word and place the result in the given
 * buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 */
static Boolean
VarTail(const char *word, Boolean addSpace, Buffer *buf, void *dummy __unused)
{
	const char *slash;

	if (addSpace) {
		Buf_AddByte (buf, (Byte)' ');
	}

	slash = strrchr(word, '/');
	if (slash != NULL) {
		slash++;
		Buf_Append(buf, slash);
	} else {
		Buf_Append(buf, word);
	}
	return (TRUE);
}

/**
 * Place the suffix of the given word in the given buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The suffix from the word is placed in the buffer.
 */
static Boolean
VarSuffix(const char *word, Boolean addSpace, Buffer *buf, void *dummy __unused)
{
	const char *dot;

	dot = strrchr(word, '.');
	if (dot != NULL) {
		if (addSpace) {
			Buf_AddByte(buf, (Byte)' ');
		}
		dot++;
		Buf_Append(buf, dot);
		addSpace = TRUE;
	}
	return (addSpace);
}

/**
 * Remove the suffix of the given word and place the result in the
 * buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 */
static Boolean
VarRoot(const char *word, Boolean addSpace, Buffer *buf, void *dummy __unused)
{
	char *dot;

	if (addSpace) {
		Buf_AddByte(buf, (Byte)' ');
	}

	dot = strrchr(word, '.');
	if (dot != NULL) {
		Buf_AppendRange(buf, word, dot);
	} else {
		Buf_Append(buf, word);
	}
	return (TRUE);
}

/**
 * Place the word in the buffer if it matches the given pattern.
 * Callback function for VarModify to implement the :M modifier.
 * A space will be added if requested.  A pattern is supplied
 * which the word must match.
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 */
static Boolean
VarMatch(const char *word, Boolean addSpace, Buffer *buf, void *pattern)
{

	if (Str_Match(word, pattern)) {
		if (addSpace) {
			Buf_AddByte(buf, (Byte)' ');
		}
		addSpace = TRUE;
		Buf_Append(buf, word);
	}
	return (addSpace);
}

#ifdef SYSVVARSUB
/**
 * Place the word in the buffer if it matches the given pattern.
 * Callback function for VarModify to implement the System V %
 * modifiers.  A space is added if requested.
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 */
static Boolean
VarSYSVMatch(const char *word, Boolean addSpace, Buffer *buf, void *patp)
{
	int		len;
	const char	*ptr;
	VarPattern	*pat = (VarPattern *)patp;

	if (addSpace)
		Buf_AddByte(buf, (Byte)' ');

	addSpace = TRUE;

	if ((ptr = Str_SYSVMatch(word, Buf_Data(pat->lhs), &len)) != NULL)
		Str_SYSVSubst(buf, Buf_Data(pat->rhs), ptr, len);
	else
		Buf_Append(buf, word);

	return (addSpace);
}
#endif

/**
 * Place the word in the buffer if it doesn't match the given pattern.
 * Callback function for VarModify to implement the :N modifier.  A
 * space is added if requested.
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 */
static Boolean
VarNoMatch(const char *word, Boolean addSpace, Buffer *buf, void *pattern)
{

	if (!Str_Match(word, pattern)) {
		if (addSpace) {
			Buf_AddByte(buf, (Byte)' ');
		}
		addSpace = TRUE;
		Buf_Append(buf, word);
	}
	return (addSpace);
}

/**
 * Perform a string-substitution on the given word, placing the
 * result in the passed buffer.  A space is added if requested.
 *
 * Results:
 *	TRUE if a space is needed before more characters are added.
 */
static Boolean
VarSubstitute(const char *word, Boolean addSpace, Buffer *buf, void *patternp)
{
	size_t		wordLen;	/* Length of word */
	const char	*cp;		/* General pointer */
	VarPattern	*pattern = patternp;

	wordLen = strlen(word);
	if (1) { /* substitute in each word of the variable */
		/*
		 * Break substitution down into simple anchored cases
		 * and if none of them fits, perform the general substitution
		 * case.
		 */
		if ((pattern->flags & VAR_MATCH_START) &&
		   (strncmp(word, Buf_Data(pattern->lhs),
		    Buf_Size(pattern->lhs)) == 0)) {
			/*
			 * Anchored at start and beginning of word matches
			 * pattern.
			 */
			if ((pattern->flags & VAR_MATCH_END) &&
			    (wordLen == Buf_Size(pattern->lhs))) {
				/*
				 * Also anchored at end and matches to the end
				 * (word is same length as pattern) add space
				 * and rhs only if rhs is non-null.
				 */
				if (Buf_Size(pattern->rhs) != 0) {
					if (addSpace) {
						Buf_AddByte(buf, (Byte)' ');
					}
					addSpace = TRUE;
					Buf_AppendBuf(buf, pattern->rhs);
				}

			} else if (pattern->flags & VAR_MATCH_END) {
				/*
				 * Doesn't match to end -- copy word wholesale
				 */
				goto nosub;

			} else {
				/*
				 * Matches at start but need to copy in
				 * trailing characters.
				 */
				if ((Buf_Size(pattern->rhs) + wordLen -
				    Buf_Size(pattern->lhs)) != 0) {
					if (addSpace) {
						Buf_AddByte(buf, (Byte)' ');
					}
					addSpace = TRUE;
				}
				Buf_AppendBuf(buf, pattern->rhs);
				Buf_AddBytes(buf, wordLen -
				    Buf_Size(pattern->lhs),
				    (word + Buf_Size(pattern->lhs)));
			}

		} else if (pattern->flags & VAR_MATCH_START) {
			/*
			 * Had to match at start of word and didn't -- copy
			 * whole word.
			 */
			goto nosub;

		} else if (pattern->flags & VAR_MATCH_END) {
			/*
			 * Anchored at end, Find only place match could occur
			 * (leftLen characters from the end of the word) and
			 * see if it does. Note that because the $ will be
			 * left at the end of the lhs, we have to use strncmp.
			 */
			cp = word + (wordLen - Buf_Size(pattern->lhs));
			if ((cp >= word) && (strncmp(cp, Buf_Data(pattern->lhs),
			    Buf_Size(pattern->lhs)) == 0)) {
				/*
				 * Match found. If we will place characters in
				 * the buffer, add a space before hand as
				 * indicated by addSpace, then stuff in the
				 * initial, unmatched part of the word followed
				 * by the right-hand-side.
				 */
				if ((cp - word) + Buf_Size(pattern->rhs) != 0) {
					if (addSpace) {
						Buf_AddByte(buf, (Byte)' ');
					}
					addSpace = TRUE;
				}
				Buf_AppendRange(buf, word, cp);
				Buf_AppendBuf(buf, pattern->rhs);

			} else {
				/*
				 * Had to match at end and didn't. Copy entire
				 * word.
				 */
				goto nosub;
			}
		} else {
			/*
			 * Pattern is unanchored: search for the pattern in the
			 * word using strstr(3), copying unmatched portions and
			 * the right-hand-side for each match found, handling
			 * non-global substitutions correctly, etc. When the
			 * loop is done, any remaining part of the word (word
			 * and wordLen are adjusted accordingly through the
			 * loop) is copied straight into the buffer.
			 * addSpace is set FALSE as soon as a space is added
			 * to the buffer.
			 */
			Boolean done;
			size_t origSize;

			done = FALSE;
			origSize = Buf_Size(buf);
			while (!done) {
				cp = strstr(word, Buf_Data(pattern->lhs));
				if (cp != NULL) {
					if (addSpace && (((cp - word) +
					    Buf_Size(pattern->rhs)) != 0)) {
						Buf_AddByte(buf, (Byte)' ');
						addSpace = FALSE;
					}
					Buf_AppendRange(buf, word, cp);
					Buf_AppendBuf(buf, pattern->rhs);
					wordLen -= (cp - word) +
					    Buf_Size(pattern->lhs);
					word = cp + Buf_Size(pattern->lhs);
					if (wordLen == 0 || (pattern->flags &
					    VAR_SUB_GLOBAL) == 0) {
						done = TRUE;
					}
				} else {
					done = TRUE;
				}
			}
			if (wordLen != 0) {
				if (addSpace) {
					Buf_AddByte(buf, (Byte)' ');
				}
				Buf_AddBytes(buf, wordLen, (const Byte *)word);
			}

			/*
			 * If added characters to the buffer, need to add a
			 * space before we add any more. If we didn't add any,
			 * just return the previous value of addSpace.
			 */
			return ((Buf_Size(buf) != origSize) || addSpace);
		}
		/*
		 * Common code for anchored substitutions:
		 * addSpace was set TRUE if characters were added to the buffer.
		 */
		return (addSpace);
	}
  nosub:
	if (addSpace) {
		Buf_AddByte(buf, (Byte)' ');
	}
	Buf_AddBytes(buf, wordLen, (const Byte *)word);
	return (TRUE);
}

/**
 * Print the error caused by a regcomp or regexec call.
 *
 * Side Effects:
 *	An error gets printed.
 */
static void
VarREError(int err, regex_t *pat, const char *str)
{
	char   *errbuf;
	int     errlen;

	errlen = regerror(err, pat, 0, 0);
	errbuf = emalloc(errlen);
	regerror(err, pat, errbuf, errlen);
	Error("%s: %s", str, errbuf);
	free(errbuf);
}


/**
 * Perform a regex substitution on the given word, placing the
 * result in the passed buffer.  A space is added if requested.
 *
 * Results:
 *	TRUE if a space is needed before more characters are added.
 */
static Boolean
VarRESubstitute(const char *word, Boolean addSpace, Buffer *buf, void *patternp)
{
	VarPattern	*pat;
	int		xrv;
	const char	*wp;
	char		*rp;
	int		added;
	int		flags = 0;

#define	MAYBE_ADD_SPACE()			\
	if (addSpace && !added)			\
		Buf_AddByte(buf, (Byte)' ');	\
	added = 1

	added = 0;
	wp = word;
	pat = patternp;

	if ((pat->flags & (VAR_SUB_ONE | VAR_SUB_MATCHED)) ==
	    (VAR_SUB_ONE | VAR_SUB_MATCHED)) {
		xrv = REG_NOMATCH;
	} else {
  tryagain:
		xrv = regexec(&pat->re, wp, pat->nsub, pat->matches, flags);
	}

	switch (xrv) {
	  case 0:
		pat->flags |= VAR_SUB_MATCHED;
		if (pat->matches[0].rm_so > 0) {
			MAYBE_ADD_SPACE();
			Buf_AddBytes(buf, pat->matches[0].rm_so,
			    (const Byte *)wp);
		}

		for (rp = Buf_Data(pat->rhs); *rp; rp++) {
			if ((*rp == '\\') && ((rp[1] == '&') || (rp[1] == '\\'))) {
				MAYBE_ADD_SPACE();
				Buf_AddByte(buf, (Byte)rp[1]);
				rp++;

			} else if ((*rp == '&') ||
			    ((*rp == '\\') && isdigit((unsigned char)rp[1]))) {
				int	n;
				const char *subbuf;
				int	sublen;
				char	errstr[3];

				if (*rp == '&') {
					n = 0;
					errstr[0] = '&';
					errstr[1] = '\0';
				} else {
					n = rp[1] - '0';
					errstr[0] = '\\';
					errstr[1] = rp[1];
					errstr[2] = '\0';
					rp++;
				}

				if (n > pat->nsub) {
					Error("No subexpression %s",
					    &errstr[0]);
					subbuf = "";
					sublen = 0;

				} else if ((pat->matches[n].rm_so == -1) &&
				    (pat->matches[n].rm_eo == -1)) {
					Error("No match for subexpression %s",
					    &errstr[0]);
					subbuf = "";
					sublen = 0;

				} else {
					subbuf = wp + pat->matches[n].rm_so;
					sublen = pat->matches[n].rm_eo -
					    pat->matches[n].rm_so;
				}

				if (sublen > 0) {
					MAYBE_ADD_SPACE();
					Buf_AddBytes(buf, sublen,
					    (const Byte *)subbuf);
				}
			} else {
				MAYBE_ADD_SPACE();
				Buf_AddByte(buf, (Byte)*rp);
			}
		}
		wp += pat->matches[0].rm_eo;
		if (pat->flags & VAR_SUB_GLOBAL) {
			flags |= REG_NOTBOL;
			if (pat->matches[0].rm_so == 0 &&
			    pat->matches[0].rm_eo == 0) {
				MAYBE_ADD_SPACE();
				Buf_AddByte(buf, (Byte)*wp);
				wp++;
			}
			if (*wp)
				goto tryagain;
		}
		if (*wp) {
			MAYBE_ADD_SPACE();
			Buf_Append(buf, wp);
		}
		break;

	  default:
		VarREError(xrv, &pat->re, "Unexpected regex error");
		/* fall through */

	  case REG_NOMATCH:
		if (*wp) {
			MAYBE_ADD_SPACE();
			Buf_Append(buf, wp);
		}
		break;
	}
	return (addSpace || added);
}

/**
 * Find a variable in a variable list.
 */
static Var *
VarLookup(Lst *vlist, const char *name)
{
	LstNode	*ln;

	LST_FOREACH(ln, vlist)
		if (strcmp(((const Var *)Lst_Datum(ln))->name, name) == 0)
			return (Lst_Datum(ln));
	return (NULL);
}

/**
 * Expand a variable name's embedded variables in the given context.
 *
 * Results:
 *	The contents of name, possibly expanded.
 */
static char *
VarPossiblyExpand(const char *name, GNode *ctxt)
{
	Buffer	*buf;

	if (strchr(name, '$') != NULL) {
		buf = Var_Subst(name, ctxt, 0);
		return (Buf_Peel(buf));
	} else {
		return estrdup(name);
	}
}

/**
 * If the variable name begins with a '.', it could very well be
 * one of the local ones.  We check the name against all the local
 * variables and substitute the short version in for 'name' if it
 * matches one of them.
 */
static const char *
VarLocal(const char name[])
{
	if (name[0] == '.') {
		switch (name[1]) {
		case 'A':
			if (!strcmp(name, ".ALLSRC"))
				return (ALLSRC);
			if (!strcmp(name, ".ARCHIVE"))
				return (ARCHIVE);
			break;
		case 'I':
			if (!strcmp(name, ".IMPSRC"))
				return (IMPSRC);
			break;
		case 'M':
			if (!strcmp(name, ".MEMBER"))
				return (MEMBER);
			break;
		case 'O':
			if (!strcmp(name, ".OODATE"))
				return (OODATE);
			break;
		case 'P':
			if (!strcmp(name, ".PREFIX"))
				return (PREFIX);
			break;
		case 'T':
			if (!strcmp(name, ".TARGET"))
				return (TARGET);
			break;
		default:
			break;
		}
	}
	return (name);
}

/**
 * Find the given variable in the given context and the enviornment.
 *
 * Results:
 *	A pointer to the structure describing the desired variable or
 *	NULL if the variable does not exist.
 */
static Var *
VarFindEnv(const char name[], GNode *ctxt)
{
	Var	*var;

	name = VarLocal(name);

	if ((var = VarLookup(&ctxt->context, name)) != NULL)
		return (var);

	if ((var = VarLookup(&VAR_ENV->context, name)) != NULL)
		return (var);

	return (NULL);
}

/**
 * Look for the variable in the given context.
 */
static Var *
VarFindOnly(const char name[], GNode *ctxt)
{
	Var	*var;

	name = VarLocal(name);

	if ((var = VarLookup(&ctxt->context, name)) != NULL)
		return (var);

	return (NULL);
}

/**
 * Look for the variable in all contexts.
 */
static Var *
VarFindAny(const char name[], GNode *ctxt)
{
	Boolean	localCheckEnvFirst;
	LstNode	*ln;
	Var	*var;

	name = VarLocal(name);

	/*
	 * Note whether this is one of the specific variables we were told
	 * through the -E flag to use environment-variable-override for.
	 */
	localCheckEnvFirst = FALSE;
	LST_FOREACH(ln, &envFirstVars) {
		if (strcmp(Lst_Datum(ln), name) == 0) {
			localCheckEnvFirst = TRUE;
			break;
		}
	}

	/*
	 * First look for the variable in the given context. If it's not there,
	 * look for it in VAR_CMD, VAR_GLOBAL and the environment,
	 * in that order, depending on the FIND_* flags in 'flags'
	 */
	if ((var = VarLookup(&ctxt->context, name)) != NULL)
		return (var);

	/* not there - try command line context */
	if (ctxt != VAR_CMD) {
		if ((var = VarLookup(&VAR_CMD->context, name)) != NULL)
			return (var);
	}

	/* not there - try global context, but only if not -e/-E */
	if (ctxt != VAR_GLOBAL && (!checkEnvFirst && !localCheckEnvFirst)) {
		if ((var = VarLookup(&VAR_GLOBAL->context, name)) != NULL)
			return (var);
	}

	if ((var = VarLookup(&VAR_ENV->context, name)) != NULL)
		return (var);

	/* deferred check for the environment (in case of -e/-E) */
	if ((ctxt != VAR_GLOBAL) && (checkEnvFirst || localCheckEnvFirst)) {
		if ((var = VarLookup(&VAR_GLOBAL->context, name)) != NULL)
			return (var);
	}

	return (NULL);
}

/**
 * Add a new variable of name name and value val to the given context.
 *
 * Side Effects:
 *	The new variable is placed at the front of the given context
 *	The name and val arguments are duplicated so they may
 *	safely be freed.
 */
static void
VarAdd(const char *name, const char *val, GNode *ctxt)
{

	Lst_AtFront(&ctxt->context, VarCreate(name, val, 0));
	DEBUGF(VAR, ("%s:%s = %s\n", ctxt->name, name, val));
}

/**
 * Remove a variable from a context.
 *
 * Side Effects:
 *	The Var structure is removed and freed.
 */
void
Var_Delete(const char *name, GNode *ctxt)
{
	LstNode *ln;

	DEBUGF(VAR, ("%s:delete %s\n", ctxt->name, name));
	LST_FOREACH(ln, &ctxt->context) {
		if (strcmp(((const Var *)Lst_Datum(ln))->name, name) == 0) {
			VarDestroy(Lst_Datum(ln), TRUE);
			Lst_Remove(&ctxt->context, ln);
			break;
		}
	}
}

/**
 * Set the variable name to the value val in the given context.
 *
 * Side Effects:
 *	If the variable doesn't yet exist, a new record is created for it.
 *	Else the old value is freed and the new one stuck in its place
 *
 * Notes:
 *	The variable is searched for only in its context before being
 *	created in that context. I.e. if the context is VAR_GLOBAL,
 *	only VAR_GLOBAL->context is searched. Likewise if it is VAR_CMD, only
 *	VAR_CMD->context is searched. This is done to avoid the literally
 *	thousands of unnecessary strcmp's that used to be done to
 *	set, say, $(@) or $(<).
 */
void
Var_Set(const char *name, const char *val, GNode *ctxt)
{
	Var    *v;
	char   *n;

	/*
	 * We only look for a variable in the given context since anything
	 * set here will override anything in a lower context, so there's not
	 * much point in searching them all just to save a bit of memory...
	 */
	n = VarPossiblyExpand(name, ctxt);
	v = VarFindOnly(n, ctxt);
	if (v == NULL) {
		VarAdd(n, val, ctxt);
		if (ctxt == VAR_CMD) {
			/*
			 * Any variables given on the command line
			 * are automatically exported to the
			 * environment (as per POSIX standard)
			 */
			setenv(n, val, 1);
		}
	} else {
		Buf_Clear(v->val);
		Buf_Append(v->val, val);

		if (ctxt == VAR_CMD || (v->flags & VAR_TO_ENV)) {
			/*
			 * Any variables given on the command line
			 * are automatically exported to the
			 * environment (as per POSIX standard)
			 */
			setenv(n, val, 1);
		}

	}

	DEBUGF(VAR, ("%s:%s = %s\n", ctxt->name, n, val));
	free(n);
}

/**
 * Set the VAR_TO_ENV flag on a variable
 */
void
Var_SetEnv(const char *name, GNode *ctxt)
{
	Var    *v;

	v = VarFindOnly(name, VAR_CMD);
	if (v != NULL) {
		/*
		 * Do not allow .EXPORT: to be set on variables
		 * from the comand line or MAKEFLAGS.
		 */
		Error(
		    "Warning: Did not set .EXPORTVAR: on %s because it "
		    "is from the comand line or MAKEFLAGS", name);
		return;
	}

	v = VarFindAny(name, ctxt);
	if (v == NULL) {
		Lst_AtFront(&VAR_ENV->context,
		    VarCreate(name, NULL, VAR_TO_ENV));
		setenv(name, "", 1);
		Error("Warning: .EXPORTVAR: set on undefined variable %s", name);
	} else {
		if ((v->flags & VAR_TO_ENV) == 0) {
			v->flags |= VAR_TO_ENV;
			setenv(v->name, Buf_Data(v->val), 1);
		}
	}
}

/**
 * The variable of the given name has the given value appended to it in
 * the given context.
 *
 * Side Effects:
 *	If the variable doesn't exist, it is created. Else the strings
 *	are concatenated (with a space in between).
 *
 * Notes:
 *	Only if the variable is being sought in the global context is the
 *	environment searched.
 *	XXX: Knows its calling circumstances in that if called with ctxt
 *	an actual target, it will only search that context since only
 *	a local variable could be being appended to. This is actually
 *	a big win and must be tolerated.
 */
void
Var_Append(const char *name, const char *val, GNode *ctxt)
{
	Var	*v;
	char	*n;

	n = VarPossiblyExpand(name, ctxt);
	if (ctxt == VAR_GLOBAL) {
		v = VarFindEnv(n, ctxt);
	} else {
		v = VarFindOnly(n, ctxt);
	}
	if (v == NULL) {
		VarAdd(n, val, ctxt);
	} else {
		Buf_AddByte(v->val, (Byte)' ');
		Buf_Append(v->val, val);
		DEBUGF(VAR, ("%s:%s = %s\n", ctxt->name, n, Buf_Data(v->val)));
	}
	free(n);
}

/**
 * See if the given variable exists.
 *
 * Results:
 *	TRUE if it does, FALSE if it doesn't
 */
Boolean
Var_Exists(const char *name, GNode *ctxt)
{
	Var	*v;
	char	*n;

	n = VarPossiblyExpand(name, ctxt);
	v = VarFindAny(n, ctxt);
	if (v == NULL) {
		free(n);
		return (FALSE);
	} else {
		free(n);
		return (TRUE);
	}
}

/**
 * Return the value of the named variable in the given context
 *
 * Results:
 *	The value if the variable exists, NULL if it doesn't
 */
char *
Var_Value(const char *name, GNode *ctxt, char **frp)
{
	Var	*v;
	char	*n;
	char	*p;

	n = VarPossiblyExpand(name, ctxt);
	v = VarFindAny(n, ctxt);
	if (v == NULL) {
		p = NULL;
		*frp = NULL;
	} else {
		p = Buf_Data(v->val);
		*frp = NULL;
	}
	free(n);
	return (p);
}

/**
 * Modify each of the words of the passed string using the given
 * function. Used to implement all modifiers.
 *
 * Results:
 *	A string of all the words modified appropriately.
 *
 * Side Effects:
 *	Uses brk_string() so it invalidates any previous call to
 *	brk_string().
 */
static char *
VarModify(const char *str, VarModifyProc *modProc, void *datum)
{
	char	**av;		/* word list [first word does not count] */
	int	ac;
	Buffer	*buf;		/* Buffer for the new string */
	Boolean	addSpace;	/* TRUE if need to add a space to the buffer
				 * before adding the trimmed word */
	int	i;

	av = brk_string(str, &ac, FALSE);

	buf = Buf_Init(0);

	addSpace = FALSE;
	for (i = 1; i < ac; i++)
		addSpace = (*modProc)(av[i], addSpace, buf, datum);

	return (Buf_Peel(buf));
}

/**
 * Sort the words in the string.
 *
 * Input:
 *	str		String whose words should be sorted
 *	cmp		A comparison function to control the ordering
 *
 * Results:
 *	A string containing the words sorted
 *
 * Side Effects:
 *	Uses brk_string() so it invalidates any previous call to
 *	brk_string().
 */
static char *
VarSortWords(const char *str, int (*cmp)(const void *, const void *))
{
	char	**av;
	int	ac;
	Buffer	*buf;
	int	i;

	av = brk_string(str, &ac, FALSE);
	qsort(av + 1, ac - 1, sizeof(char *), cmp);

	buf = Buf_Init(0);
	for (i = 1; i < ac; i++) {
		Buf_Append(buf, av[i]);
		Buf_AddByte(buf, (Byte)((i < ac - 1) ? ' ' : '\0'));
	}

	return (Buf_Peel(buf));
}

static int
SortIncreasing(const void *l, const void *r)
{

	return (strcmp(*(const char* const*)l, *(const char* const*)r));
}

/**
 * Pass through the tstr looking for 1) escaped delimiters,
 * '$'s and backslashes (place the escaped character in
 * uninterpreted) and 2) unescaped $'s that aren't before
 * the delimiter (expand the variable substitution).
 * Return the expanded string or NULL if the delimiter was missing
 * If pattern is specified, handle escaped ampersands, and replace
 * unescaped ampersands with the lhs of the pattern.
 *
 * Results:
 *	A string of all the words modified appropriately.
 *	If length is specified, return the string length of the buffer
 *	If flags is specified and the last character of the pattern is a
 *	$ set the VAR_MATCH_END bit of flags.
 */
static Buffer *
VarGetPattern(VarParser *vp, int delim, int *flags, VarPattern *patt)
{
	Buffer		*buf;

	buf = Buf_Init(0);

	/*
	 * Skim through until the matching delimiter is found; pick up
	 * variable substitutions on the way. Also allow backslashes to quote
	 * the delimiter, $, and \, but don't touch other backslashes.
	 */
	while (*vp->ptr != '\0') {
		if (*vp->ptr == delim) {
			return (buf);

		} else if ((vp->ptr[0] == '\\') &&
		    ((vp->ptr[1] == delim) ||
		     (vp->ptr[1] == '\\') ||
		     (vp->ptr[1] == '$') ||
		     (vp->ptr[1] == '&' && patt != NULL))) {
			vp->ptr++;		/* consume backslash */
			Buf_AddByte(buf, (Byte)vp->ptr[0]);
			vp->ptr++;

		} else if (vp->ptr[0] == '$') {
			if (vp->ptr[1] == delim) {
				if (flags == NULL) {
					Buf_AddByte(buf, (Byte)vp->ptr[0]);
					vp->ptr++;
				} else {
					/*
					 * Unescaped $ at end of patt =>
					 * anchor patt at end.
					 */
					*flags |= VAR_MATCH_END;
					vp->ptr++;
				}
			} else {
				VarParser	subvp = {
					vp->ptr,
					vp->ptr,
					vp->ctxt,
					vp->err,
					vp->execute
				};
				char   *rval;
				Boolean rfree;

				/*
				 * If unescaped dollar sign not
				 * before the delimiter, assume it's
				 * a variable substitution and
				 * recurse.
				 */
				rval = VarParse(&subvp, &rfree);
				Buf_Append(buf, rval);
				if (rfree)
					free(rval);
				vp->ptr = subvp.ptr;
			}
		} else if (vp->ptr[0] == '&' && patt != NULL) {
			Buf_AppendBuf(buf, patt->lhs);
			vp->ptr++;
		} else {
			Buf_AddByte(buf, (Byte)vp->ptr[0]);
			vp->ptr++;
		}
	}

	Buf_Destroy(buf, TRUE);
	return (NULL);
}

/**
 * Make sure this variable is fully expanded.
 */
static char *
VarExpand(Var *v, VarParser *vp)
{
	char	*value;
	char	*result;

	if (v->flags & VAR_IN_USE) {
		Fatal("Variable %s is recursive.", v->name);
		/* NOTREACHED */
	}

	v->flags |= VAR_IN_USE;

	/*
	 * Before doing any modification, we have to make sure the
	 * value has been fully expanded. If it looks like recursion
	 * might be necessary (there's a dollar sign somewhere in the
	 * variable's value) we just call Var_Subst to do any other
	 * substitutions that are necessary. Note that the value
	 * returned by Var_Subst will have been
	 * dynamically-allocated, so it will need freeing when we
	 * return.
	 */
	value = Buf_Data(v->val);
	if (strchr(value, '$') == NULL) {
		result = strdup(value);
	} else {
		Buffer	*buf;

		buf = Var_Subst(value, vp->ctxt, vp->err);
		result = Buf_Peel(buf);
	}

	v->flags &= ~VAR_IN_USE;

	return (result);
}

/**
 * Select only those words in value that match the modifier.
 */
static char *
modifier_M(VarParser *vp, const char value[], char endc)
{
	char	*patt;
	char	*ptr;
	char	*newValue;
	char	modifier;

	modifier = vp->ptr[0];
	vp->ptr++;	/* consume 'M' or 'N' */

	/*
	 * Compress the \:'s out of the pattern, so allocate enough
	 * room to hold the uncompressed pattern and compress the
	 * pattern into that space.
	 */
	patt = estrdup(vp->ptr);
	ptr = patt;
	while (vp->ptr[0] != '\0') {
		if (vp->ptr[0] == endc || vp->ptr[0] == ':') {
			break;
		}
		if (vp->ptr[0] == '\\' &&
		    (vp->ptr[1] == endc || vp->ptr[1] == ':')) {
			vp->ptr++;	/* consume backslash */
		}
		*ptr = vp->ptr[0];
		ptr++;
		vp->ptr++;
	}
	*ptr = '\0';

	if (modifier == 'M') {
		newValue = VarModify(value, VarMatch, patt);
	} else {
		newValue = VarModify(value, VarNoMatch, patt);
	}
	free(patt);

	return (newValue);
}

/**
 * Substitute the replacement string for the pattern.  The substitution
 * is applied to each word in value.
 */
static char *
modifier_S(VarParser *vp, const char value[], Var *v)
{
	VarPattern	patt;
	char		delim;
	char		*newValue;

	patt.flags = 0;

	vp->ptr++;		/* consume 'S' */

	delim = *vp->ptr;	/* used to find end of pattern */
	vp->ptr++;		/* consume 1st delim */

	/*
	 * If pattern begins with '^', it is anchored to the start of the
	 * word -- skip over it and flag pattern.
	 */
	if (*vp->ptr == '^') {
		patt.flags |= VAR_MATCH_START;
		vp->ptr++;
	}

	patt.lhs = VarGetPattern(vp, delim, &patt.flags, NULL);
	if (patt.lhs == NULL) {
		/*
		 * LHS didn't end with the delim, complain and exit.
		 */
		Fatal("Unclosed substitution for %s (%c missing)",
		    v->name, delim);
	}

	vp->ptr++;	/* consume 2nd delim */

	patt.rhs = VarGetPattern(vp, delim, NULL, &patt);
	if (patt.rhs == NULL) {
		/*
		 * RHS didn't end with the delim, complain and exit.
		 */
		Fatal("Unclosed substitution for %s (%c missing)",
		    v->name, delim);
	}

	vp->ptr++;	/* consume last delim */

	/*
	 * Check for global substitution. If 'g' after the final delimiter,
	 * substitution is global and is marked that way.
	 */
	if (vp->ptr[0] == 'g') {
		patt.flags |= VAR_SUB_GLOBAL;
		vp->ptr++;
	}

	/*
	 * Global substitution of the empty string causes an infinite number
	 * of matches, unless anchored by '^' (start of string) or '$' (end
	 * of string). Catch the infinite substitution here. Note that flags
	 * can only contain the 3 bits we're interested in so we don't have
	 * to mask unrelated bits. We can test for equality.
	 */
	if (Buf_Size(patt.lhs) == 0 && patt.flags == VAR_SUB_GLOBAL)
		Fatal("Global substitution of the empty string");

	newValue = VarModify(value, VarSubstitute, &patt);

	/*
	 * Free the two strings.
	 */
	free(patt.lhs);
	free(patt.rhs);

	return (newValue);
}

static char *
modifier_C(VarParser *vp, char value[], Var *v)
{
	VarPattern	patt;
	char		delim;
	int		error;
	char		*newValue;

	patt.flags = 0;

	vp->ptr++;		/* consume 'C' */

	delim = *vp->ptr;	/* delimiter between sections */

	vp->ptr++;		/* consume 1st delim */

	patt.lhs = VarGetPattern(vp, delim, NULL, NULL);
	if (patt.lhs == NULL) {
		Fatal("Unclosed substitution for %s (%c missing)",
		     v->name, delim);
	}

	vp->ptr++;		/* consume 2st delim */

	patt.rhs = VarGetPattern(vp, delim, NULL, NULL);
	if (patt.rhs == NULL) {
		Fatal("Unclosed substitution for %s (%c missing)",
		     v->name, delim);
	}

	vp->ptr++;		/* consume last delim */

	switch (*vp->ptr) {
	case 'g':
		patt.flags |= VAR_SUB_GLOBAL;
		vp->ptr++;		/* consume 'g' */
		break;
	case '1':
		patt.flags |= VAR_SUB_ONE;
		vp->ptr++;		/* consume '1' */
		break;
	default:
		break;
	}

	error = regcomp(&patt.re, Buf_Data(patt.lhs), REG_EXTENDED);
	if (error) {
		VarREError(error, &patt.re, "RE substitution error");
		free(patt.rhs);
		free(patt.lhs);
		return (var_Error);
	}

	patt.nsub = patt.re.re_nsub + 1;
	if (patt.nsub < 1)
		patt.nsub = 1;
	if (patt.nsub > 10)
		patt.nsub = 10;
	patt.matches = emalloc(patt.nsub * sizeof(regmatch_t));

	newValue = VarModify(value, VarRESubstitute, &patt);

	regfree(&patt.re);
	free(patt.matches);
	free(patt.rhs);
	free(patt.lhs);

	return (newValue);
}

static char *
sysVvarsub(VarParser *vp, char startc, Var *v, const char value[])
{
#ifdef SYSVVARSUB
	/*
	 * This can either be a bogus modifier or a System-V substitution
	 * command.
	 */
	char		endc;
	VarPattern	patt;
	Boolean		eqFound;
	int		cnt;
	char		*newStr;
	const char	*cp;

	endc = (startc == OPEN_PAREN) ? CLOSE_PAREN : CLOSE_BRACE;

	patt.flags = 0;

	/*
	 * First we make a pass through the string trying to verify it is a
	 * SYSV-make-style translation: it must be: <string1>=<string2>)
	 */
	eqFound = FALSE;
	cp = vp->ptr;
	cnt = 1;
	while (*cp != '\0' && cnt) {
		if (*cp == '=') {
			eqFound = TRUE;
			/* continue looking for endc */
		} else if (*cp == endc)
			cnt--;
		else if (*cp == startc)
			cnt++;
		if (cnt)
			cp++;
	}

	if (*cp == endc && eqFound) {
		/*
		 * Now we break this sucker into the lhs and rhs.
		 */
		patt.lhs = VarGetPattern(vp, '=', &patt.flags, NULL);
		if (patt.lhs == NULL) {
			Fatal("Unclosed substitution for %s (%c missing)",
			      v->name, '=');
		}
		vp->ptr++;	/* consume '=' */

		patt.rhs = VarGetPattern(vp, endc, NULL, &patt);
		if (patt.rhs == NULL) {
			Fatal("Unclosed substitution for %s (%c missing)",
			      v->name, endc);
		}

		/*
		 * SYSV modifications happen through the whole string. Note
		 * the pattern is anchored at the end.
		 */
		newStr = VarModify(value, VarSYSVMatch, &patt);

		free(patt.lhs);
		free(patt.rhs);
	} else
#endif
	{
		Error("Unknown modifier '%c'\n", *vp->ptr);
		vp->ptr++;
		while (*vp->ptr != '\0') {
			if (*vp->ptr == endc && *vp->ptr == ':') {
				break;
			}
			vp->ptr++;
		}
		newStr = var_Error;
	}

	return (newStr);
}

/**
 * Quote shell meta-characters in the string
 *
 * Results:
 *	The quoted string
 */
static char *
Var_Quote(const char *str)
{
	Buffer *buf;
	/* This should cover most shells :-( */
	static char meta[] = "\n \t'`\";&<>()|*?{}[]\\$!#^~";

	buf = Buf_Init(MAKE_BSIZE);
	for (; *str; str++) {
		if (strchr(meta, *str) != NULL)
			Buf_AddByte(buf, (Byte)'\\');
		Buf_AddByte(buf, (Byte)*str);
	}

	return (Buf_Peel(buf));
}


/*
 * Now we need to apply any modifiers the user wants applied.
 * These are:
 *	:M<pattern>
 *		words which match the given <pattern>.
 *		<pattern> is of the standard file
 *		wildcarding form.
 *	:S<d><pat1><d><pat2><d>[g]
 *		Substitute <pat2> for <pat1> in the value
 *	:C<d><pat1><d><pat2><d>[g]
 *		Substitute <pat2> for regex <pat1> in the value
 *	:H	Substitute the head of each word
 *	:T	Substitute the tail of each word
 *	:E	Substitute the extension (minus '.') of
 *		each word
 *	:R	Substitute the root of each word
 *		(pathname minus the suffix).
 *	:lhs=rhs
 *		Like :S, but the rhs goes to the end of
 *		the invocation.
 *	:U	Converts variable to upper-case.
 *	:L	Converts variable to lower-case.
 *
 * XXXHB update this comment or remove it and point to the man page.
 */
static char *
ParseModifier(VarParser *vp, char startc, Var *v, Boolean *freeResult)
{
	char	*value;
	char	endc;

	value = VarExpand(v, vp);
	*freeResult = TRUE;

	endc = (startc == OPEN_PAREN) ? CLOSE_PAREN : CLOSE_BRACE;

	vp->ptr++;	/* consume first colon */

	while (*vp->ptr != '\0') {
		char	*newStr;	/* New value to return */

		if (*vp->ptr == endc) {
			return (value);
		}

		DEBUGF(VAR, ("Applying :%c to \"%s\"\n", *vp->ptr, value));
		switch (*vp->ptr) {
		case 'N':
		case 'M':
			newStr = modifier_M(vp, value, endc);
			break;
		case 'S':
			newStr = modifier_S(vp, value, v);
			break;
		case 'C':
			newStr = modifier_C(vp, value, v);
			break;
		default:
			if (vp->ptr[1] != endc && vp->ptr[1] != ':') {
#ifdef SUNSHCMD
				if ((vp->ptr[0] == 's') &&
				    (vp->ptr[1] == 'h') &&
				    (vp->ptr[2] == endc || vp->ptr[2] == ':')) {
					const char	*error;

					if (vp->execute) {
						newStr = Buf_Peel(
						    Cmd_Exec(value, &error));
					} else {
						newStr = estrdup("");
					}

					if (error)
						Error(error, value);
					vp->ptr += 2;
				} else
#endif
				{
					newStr = sysVvarsub(vp, startc, v, value);
				}
				break;
			}

			switch (vp->ptr[0]) {
			case 'L':
				{
				const char	*cp;
				Buffer		*buf;
				buf = Buf_Init(MAKE_BSIZE);
				for (cp = value; *cp; cp++)
					Buf_AddByte(buf, (Byte)tolower(*cp));

				newStr = Buf_Peel(buf);

				vp->ptr++;
				break;
				}
			case 'O':
				newStr = VarSortWords(value, SortIncreasing);
				vp->ptr++;
				break;
			case 'Q':
				newStr = Var_Quote(value);
				vp->ptr++;
				break;
			case 'T':
				newStr = VarModify(value, VarTail, NULL);
				vp->ptr++;
				break;
			case 'U':
				{
				const char	*cp;
				Buffer		*buf;
				buf = Buf_Init(MAKE_BSIZE);
				for (cp = value; *cp; cp++)
					Buf_AddByte(buf, (Byte)toupper(*cp));

				newStr = Buf_Peel(buf);

				vp->ptr++;
				break;
				}
			case 'H':
				newStr = VarModify(value, VarHead, NULL);
				vp->ptr++;
				break;
			case 'E':
				newStr = VarModify(value, VarSuffix, NULL);
				vp->ptr++;
				break;
			case 'R':
				newStr = VarModify(value, VarRoot, NULL);
				vp->ptr++;
				break;
			default:
				newStr = sysVvarsub(vp, startc, v, value);
				break;
			}
			break;
		}

		DEBUGF(VAR, ("Result is \"%s\"\n", newStr));
		if (*freeResult) {
			free(value);
		}

		value = newStr;
		*freeResult = (value == var_Error) ? FALSE : TRUE;

		if (vp->ptr[0] == ':') {
			vp->ptr++;	/* consume colon */
		}
	}

	return (value);
}

static char *
ParseRestModifier(VarParser *vp, char startc, Buffer *buf, Boolean *freeResult)
{
	const char	*vname;
	size_t		vlen;
	Var		*v;
	char		*value;

	vname = Buf_GetAll(buf, &vlen);

	v = VarFindAny(vname, vp->ctxt);
	if (v != NULL) {
		value = ParseModifier(vp, startc, v, freeResult);
		return (value);
	}

	if ((vp->ctxt == VAR_CMD) || (vp->ctxt == VAR_GLOBAL)) {
		size_t  consumed;
		/*
		 * Still need to get to the end of the variable
		 * specification, so kludge up a Var structure for the
		 * modifications
		 */
		v = VarCreate(vname, NULL, VAR_JUNK);
		value = ParseModifier(vp, startc, v, freeResult);
		if (*freeResult) {
			free(value);
		}
		VarDestroy(v, TRUE);

		consumed = vp->ptr - vp->input + 1;
		/*
		 * If substituting a local variable in a non-local context,
		 * assume it's for dynamic source stuff. We have to handle
		 * this specially and return the longhand for the variable
		 * with the dollar sign escaped so it makes it back to the
		 * caller. Only four of the local variables are treated
		 * specially as they are the only four that will be set when
		 * dynamic sources are expanded.
		 */
		if (vlen == 1 ||
		    (vlen == 2 && (vname[1] == 'F' || vname[1] == 'D'))) {
			if (strchr("!%*@", vname[0]) != NULL) {
				value = emalloc(consumed + 1);
				strncpy(value, vp->input, consumed);
				value[consumed] = '\0';

				*freeResult = TRUE;
				return (value);
			}
		}
		if (vlen > 2 &&
		    vname[0] == '.' &&
		    isupper((unsigned char)vname[1])) {
			if ((strncmp(vname, ".TARGET", vlen - 1) == 0) ||
			    (strncmp(vname, ".ARCHIVE", vlen - 1) == 0) ||
			    (strncmp(vname, ".PREFIX", vlen - 1) == 0) ||
			    (strncmp(vname, ".MEMBER", vlen - 1) == 0)) {
				value = emalloc(consumed + 1);
				strncpy(value, vp->input, consumed);
				value[consumed] = '\0';

				*freeResult = TRUE;
				return (value);
			}
		}

		*freeResult = FALSE;
		return (vp->err ? var_Error : varNoError);
	} else {
		/*
		 * Check for D and F forms of local variables since we're in
		 * a local context and the name is the right length.
		 */
		if (vlen == 2 &&
		    (vname[1] == 'F' || vname[1] == 'D') &&
		    (strchr("!%*<>@", vname[0]) != NULL)) {
			char	name[2];

			name[0] = vname[0];
			name[1] = '\0';

			v = VarFindOnly(name, vp->ctxt);
			if (v != NULL) {
				value = ParseModifier(vp, startc, v, freeResult);
				return (value);
			}
		}

		/*
		 * Still need to get to the end of the variable
		 * specification, so kludge up a Var structure for the
		 * modifications
		 */
		v = VarCreate(vname, NULL, VAR_JUNK);
		value = ParseModifier(vp, startc, v, freeResult);
		if (*freeResult) {
			free(value);
		}
		VarDestroy(v, TRUE);

		*freeResult = FALSE;
		return (vp->err ? var_Error : varNoError);
	}
}

static char *
ParseRestEnd(VarParser *vp, Buffer *buf, Boolean *freeResult)
{
	const char	*vname;
	size_t		vlen;
	Var		*v;
	char		*value;

	vname = Buf_GetAll(buf, &vlen);

	v = VarFindAny(vname, vp->ctxt);
	if (v != NULL) {
		value = VarExpand(v, vp);
		*freeResult = TRUE;
		return (value);
	}

	if ((vp->ctxt == VAR_CMD) || (vp->ctxt == VAR_GLOBAL)) {
		size_t	consumed = vp->ptr - vp->input + 1;

		/*
		 * If substituting a local variable in a non-local context,
		 * assume it's for dynamic source stuff. We have to handle
		 * this specially and return the longhand for the variable
		 * with the dollar sign escaped so it makes it back to the
		 * caller. Only four of the local variables are treated
		 * specially as they are the only four that will be set when
		 * dynamic sources are expanded.
		 */
		if (vlen == 1 ||
		    (vlen == 2 && (vname[1] == 'F' || vname[1] == 'D'))) {
			if (strchr("!%*@", vname[0]) != NULL) {
				value = emalloc(consumed + 1);
				strncpy(value, vp->input, consumed);
				value[consumed] = '\0';

				*freeResult = TRUE;
				return (value);
			}
		}
		if (vlen > 2 &&
		    vname[0] == '.' &&
		    isupper((unsigned char)vname[1])) {
			if ((strncmp(vname, ".TARGET", vlen - 1) == 0) ||
			    (strncmp(vname, ".ARCHIVE", vlen - 1) == 0) ||
			    (strncmp(vname, ".PREFIX", vlen - 1) == 0) ||
			    (strncmp(vname, ".MEMBER", vlen - 1) == 0)) {
				value = emalloc(consumed + 1);
				strncpy(value, vp->input, consumed);
				value[consumed] = '\0';

				*freeResult = TRUE;
				return (value);
			}
		}
	} else {
		/*
		 * Check for D and F forms of local variables since we're in
		 * a local context and the name is the right length.
		 */
		if (vlen == 2 &&
		    (vname[1] == 'F' || vname[1] == 'D') &&
		    (strchr("!%*<>@", vname[0]) != NULL)) {
			char	name[2];

			name[0] = vname[0];
			name[1] = '\0';

			v = VarFindOnly(name, vp->ctxt);
			if (v != NULL) {
				char	*val;
				/*
				 * No need for nested expansion or anything,
				 * as we're the only one who sets these
				 * things and we sure don't put nested
				 * invocations in them...
				 */
				val = Buf_Data(v->val);

				if (vname[1] == 'D') {
					val = VarModify(val, VarHead, NULL);
				} else {
					val = VarModify(val, VarTail, NULL);
				}

				*freeResult = TRUE;
				return (val);
			}
		}
	}

	*freeResult = FALSE;
	return (vp->err ? var_Error : varNoError);
}

/**
 * Parse a multi letter variable name, and return it's value.
 */
static char *
VarParseLong(VarParser *vp, Boolean *freeResult)
{
	Buffer		*buf;
	char		startc;
	char		endc;
	char		*value;

	buf = Buf_Init(MAKE_BSIZE);

	startc = vp->ptr[0];
	vp->ptr++;		/* consume opening paren or brace */

	endc = (startc == OPEN_PAREN) ? CLOSE_PAREN : CLOSE_BRACE;

	/*
	 * Process characters until we reach an end character or a colon,
	 * replacing embedded variables as we go.
	 */
	while (*vp->ptr != '\0') {
		if (*vp->ptr == endc) {
			value = ParseRestEnd(vp, buf, freeResult);
			vp->ptr++;	/* consume closing paren or brace */
			Buf_Destroy(buf, TRUE);
			return (value);

		} else if (*vp->ptr == ':') {
			value = ParseRestModifier(vp, startc, buf, freeResult);
			vp->ptr++;	/* consume closing paren or brace */
			Buf_Destroy(buf, TRUE);
			return (value);

		} else if (*vp->ptr == '$') {
			VarParser	subvp = {
				vp->ptr,
				vp->ptr,
				vp->ctxt,
				vp->err,
				vp->execute
			};
			char	*rval;
			Boolean	rfree;

			rval = VarParse(&subvp, &rfree);
			if (rval == var_Error) {
				Fatal("Error expanding embedded variable.");
			}
			Buf_Append(buf, rval);
			if (rfree)
				free(rval);
			vp->ptr = subvp.ptr;
		} else {
			Buf_AddByte(buf, (Byte)*vp->ptr);
			vp->ptr++;
		}
	}

	/* If we did not find the end character, return var_Error */
	Buf_Destroy(buf, TRUE);
	*freeResult = FALSE;
	return (var_Error);
}

/**
 * Parse a single letter variable name, and return it's value.
 */
static char *
VarParseShort(VarParser *vp, Boolean *freeResult)
{
	char	vname[2];
	Var	*v;
	char	*value;

	vname[0] = vp->ptr[0];
	vname[1] = '\0';

	vp->ptr++;	/* consume single letter */

	v = VarFindAny(vname, vp->ctxt);
	if (v != NULL) {
		value = VarExpand(v, vp);
		*freeResult = TRUE;
		return (value);
	}

	/*
	 * If substituting a local variable in a non-local context, assume
	 * it's for dynamic source stuff. We have to handle this specially
	 * and return the longhand for the variable with the dollar sign
	 * escaped so it makes it back to the caller. Only four of the local
	 * variables are treated specially as they are the only four that
	 * will be set when dynamic sources are expanded.
	 */
	if ((vp->ctxt == VAR_CMD) || (vp->ctxt == VAR_GLOBAL)) {

		/* XXX: It looks like $% and $! are reversed here */
		switch (vname[0]) {
		case '@':
			*freeResult = TRUE;
			return (estrdup("$(.TARGET)"));
		case '%':
			*freeResult = TRUE;
			return (estrdup("$(.ARCHIVE)"));
		case '*':
			*freeResult = TRUE;
			return (estrdup("$(.PREFIX)"));
		case '!':
			*freeResult = TRUE;
			return (estrdup("$(.MEMBER)"));
		default:
			*freeResult = FALSE;
			return (vp->err ? var_Error : varNoError);
		}
	}

	/* Variable name was not found. */
	*freeResult = FALSE;
	return (vp->err ? var_Error : varNoError);
}

static char *
VarParse(VarParser *vp, Boolean *freeResult)
{

	vp->ptr++;	/* consume '$' or last letter of conditional */

	if (vp->ptr[0] == '\0') {
		/* Error, there is only a dollar sign in the input string. */
		*freeResult = FALSE;
		return (vp->err ? var_Error : varNoError);

	} else if (vp->ptr[0] == OPEN_PAREN || vp->ptr[0] == OPEN_BRACE) {
		/* multi letter variable name */
		return (VarParseLong(vp, freeResult));

	} else {
		/* single letter variable name */
		return (VarParseShort(vp, freeResult));
	}
}

/**
 * Given the start of a variable invocation, extract the variable
 * name and find its value, then modify it according to the
 * specification.
 *
 * Results:
 *	The value of the variable or var_Error if the specification
 *	is invalid.  The number of characters in the specification
 *	is placed in the variable pointed to by consumed.  (for
 *	invalid specifications, this is just 2 to skip the '$' and
 *	the following letter, or 1 if '$' was the last character
 *	in the string).  A Boolean in *freeResult telling whether the
 *	returned string should be freed by the caller.
 */
char *
Var_Parse(const char input[], GNode *ctxt, Boolean err,
	size_t *consumed, Boolean *freeResult)
{
	VarParser	vp = {
		input,
		input,
		ctxt,
		err,
		TRUE
	};
	char		*value;

	value = VarParse(&vp, freeResult);
	*consumed += vp.ptr - vp.input;
	return (value);
}

/*
 * Given the start of a variable invocation, determine the length
 * of the specification.
 *
 * Results:
 *	The number of characters in the specification.  For invalid
 *	specifications, this is just 2 to skip the '$' and the
 *	following letter, or 1 if '$' was the last character in the
 *	string.
 */
size_t
Var_Match(const char input[], GNode *ctxt)
{
	VarParser	vp = {
		input,
		input,
		ctxt,
		FALSE,
		FALSE
	};
	char		*value;
	Boolean		freeResult;

	value = VarParse(&vp, &freeResult);
	if (freeResult) {
		free(value);
	}
	return (vp.ptr - vp.input);
}

static int
match_var(const char str[], const char var[])
{
	const char	*start = str;
	size_t		len;

	str++;			/* consume '$' */

	if (str[0] == OPEN_PAREN || str[0] == OPEN_BRACE) {
		str++;		/* consume opening paren or brace */

		while (str[0] != '\0') {
			if (str[0] == '$') {
				/*
				 * A variable inside the variable. We cannot
				 * expand the external variable yet.
				 */
				return (str - start);
			} else if (str[0] == ':' ||
				   str[0] == CLOSE_PAREN ||
				   str[0] == CLOSE_BRACE) {
				len = str - (start + 2);

				if (var[len] == '\0' && strncmp(var, start + 2, len) == 0) {
					return (0);	/* match */
				} else {
					/*
					 * Not the variable we want to
					 * expand.
					 */
					return (str - start);
				}
			} else {
				++str;
			}
		}
		return (str - start);
	} else {
		/* Single letter variable name */
		if (var[1] == '\0' && var[0] == str[0]) {
			return (0);	/* match */
		} else {
			str++;	/* consume variable name */
			return (str - start);
		}
	}
}

/**
 * Substitute for all variables in the given string in the given
 * context If err is TRUE, Parse_Error will be called when an
 * undefined variable is encountered.
 *
 * Results:
 *	The resulting string.
 *
 * Side Effects:
 *	None. The old string must be freed by the caller
 */
Buffer *
Var_Subst(const char *str, GNode *ctxt, Boolean err)
{
	Boolean	errorReported;
	Buffer *buf;		/* Buffer for forming things */

	/*
	 * Set TRUE if an error has already been reported to prevent a
	 * plethora of messages when recursing. XXXHB this comment sounds
	 * wrong.
	 */
	errorReported = FALSE;

	buf = Buf_Init(0);
	while (str[0] != '\0') {
		if ((str[0] == '$') && (str[1] == '$')) {
			/*
			 * A dollar sign may be escaped with another dollar
			 * sign.  In such a case, we skip over the escape
			 * character and store the dollar sign into the
			 * buffer directly.
			 */
			str++;
			Buf_AddByte(buf, (Byte)str[0]);
			str++;

		} else if (str[0] == '$') {
			/* Variable invocation. */
			VarParser subvp = {
				str,
				str,
				ctxt,
				err,
				TRUE
			};
			char	*rval;
			Boolean	rfree;

			rval = VarParse(&subvp, &rfree);

			/*
			 * When we come down here, val should either point to
			 * the value of this variable, suitably modified, or
			 * be NULL. Length should be the total length of the
			 * potential variable invocation (from $ to end
			 * character...)
			 */
			if (rval == var_Error || rval == varNoError) {
				/*
				 * If performing old-time variable
				 * substitution, skip over the variable and
				 * continue with the substitution. Otherwise,
				 * store the dollar sign and advance str so
				 * we continue with the string...
				 */
				if (oldVars) {
					str = subvp.ptr;
				} else if (err) {
					/*
					 * If variable is undefined, complain
					 * and skip the variable. The
					 * complaint will stop us from doing
					 * anything when the file is parsed.
					 */
					if (!errorReported) {
						Parse_Error(PARSE_FATAL,
							    "Undefined variable \"%.*s\"", subvp.ptr - subvp.input, str);
					}
					errorReported = TRUE;
					str = subvp.ptr;
				} else {
					Buf_AddByte(buf, (Byte)str[0]);
					str++;
				}
			} else {
				/*
				 * Copy all the characters from the variable
				 * value straight into the new string.
				 */
				Buf_Append(buf, rval);
				if (rfree) {
					free(rval);
				}
				str = subvp.ptr;
			}
		} else {
			Buf_AddByte(buf, (Byte)str[0]);
			str++;
		}
	}

	return (buf);
}

/**
 * Substitute for all variables except if it is the same as 'var',
 * in the given string in the given context.  If err is TRUE,
 * Parse_Error will be called when an undefined variable is
 * encountered.
 *
 * Results:
 *	The resulting string.
 *
 * Side Effects:
 *	None. The old string must be freed by the caller
 */
Buffer *
Var_SubstOnly(const char *var, const char *str, Boolean err)
{
	GNode *ctxt = VAR_GLOBAL;
	Boolean	errorReported;
	Buffer	*buf;		/* Buffer for forming things */

	/*
	 * Set TRUE if an error has already been reported to prevent a
	 * plethora of messages when recursing. XXXHB this comment sounds
	 * wrong.
	 */
	errorReported = FALSE;

	buf = Buf_Init(0);
	while (str[0] != '\0') {
		if (str[0] == '$') {
			int	skip;

			skip = match_var(str, var);
			if (skip > 0) {
				Buf_AddBytes(buf, skip, str);
				str += skip;
			} else {
				/* Variable invocation. */
				VarParser	subvp = {
					str,
					str,
					ctxt,
					err,
					TRUE
				};
				char	*rval;
				Boolean	rfree;

				rval = VarParse(&subvp, &rfree);

				/*
				 * When we get down here, rval should either
				 * point to the value of this variable, or be
				 * NULL.
				 */
				if (rval == var_Error || rval == varNoError) {
					/*
					 * If performing old-time variable
					 * substitution, skip over the
					 * variable and continue with the
					 * substitution. Otherwise, store the
					 * dollar sign and advance str so we
					 * continue with the string...
					 */
					if (oldVars) {
						str = subvp.ptr;
					} else if (err) {
						/*
						 * If variable is undefined,
						 * complain and skip the
						 * variable. The complaint
						 * will stop us from doing
						 * anything when the file is
						 * parsed.
						 */
						if (!errorReported) {
							Parse_Error(PARSE_FATAL,
								    "Undefined variable \"%.*s\"", subvp.ptr - subvp.input, str);
						}
						errorReported = TRUE;
						str = subvp.ptr;
					} else {
						Buf_AddByte(buf, (Byte)str[0]);
						str++;
					}
				} else {
					/*
					 * Copy all the characters from the
					 * variable value straight into the
					 * new string.
					 */
					Buf_Append(buf, rval);
					if (rfree) {
						free(rval);
					}
					str = subvp.ptr;
				}
			}
		} else {
			Buf_AddByte(buf, (Byte)str[0]);
			str++;
		}
	}

	return (buf);
}

/**
 * Initialize the module
 *
 * Side Effects:
 *	The VAR_CMD and VAR_GLOBAL contexts are created
 */
void
Var_Init(char **env)
{
	char	**ptr;

	VAR_CMD = Targ_NewGN("Command");
	VAR_ENV = Targ_NewGN("Environment");
	VAR_GLOBAL = Targ_NewGN("Global");

	/*
	 * Copy user environment variables into ENV context.
	 */
	for (ptr = env; *ptr != NULL; ++ptr) {
		char		*tmp = estrdup(*ptr);
		const char	*name = tmp;
		char		*sep = strchr(name, '=');
		const char	*value = sep + 1;

		if (sep != NULL) {
			*sep = '\0';
			VarAdd(name, value, VAR_ENV);
		}
		free(tmp);
	}
}

/**
 * Print all variables in global and command line contexts.
 */
void
Var_Dump(void)
{
	const LstNode	*ln;
	const Var	*v;

	printf("#*** Global Variables:\n");
	LST_FOREACH(ln, &VAR_GLOBAL->context) {
		v = Lst_Datum(ln);
		printf("%-16s = %s\n", v->name, Buf_Data(v->val));
	}

	printf("#*** Command-line Variables:\n");
	LST_FOREACH(ln, &VAR_CMD->context) {
		v = Lst_Datum(ln);
		printf("%-16s = %s\n", v->name, Buf_Data(v->val));
	}
}

