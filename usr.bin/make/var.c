/*-
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

/*-
 * var.c --
 *	Variable-handling functions
 *
 * Interface:
 *	Var_Set	  	    Set the value of a variable in the given
 *	    	  	    context. The variable is created if it doesn't
 *	    	  	    yet exist. The value and variable name need not
 *	    	  	    be preserved.
 *
 *	Var_Append	    Append more characters to an existing variable
 *	    	  	    in the given context. The variable needn't
 *	    	  	    exist already -- it will be created if it doesn't.
 *	    	  	    A space is placed between the old value and the
 *	    	  	    new one.
 *
 *	Var_Exists	    See if a variable exists.
 *
 *	Var_Value 	    Return the value of a variable in a context or
 *	    	  	    NULL if the variable is undefined.
 *
 *	Var_Subst 	    Substitute named variable, or all variables if
 *			    NULL in a string using
 *	    	  	    the given context as the top-most one. If the
 *	    	  	    third argument is non-zero, Parse_Error is
 *	    	  	    called if any variables are undefined.
 *
 *	Var_Parse 	    Parse a variable expansion from a string and
 *	    	  	    return the result and the number of characters
 *	    	  	    consumed.
 *
 *	Var_Delete	    Delete a variable in a context.
 *
 *	Var_Init  	    Initialize this module.
 *
 * Debugging:
 *	Var_Dump  	    Print out all variables defined in the given
 *	    	  	    context.
 *
 * XXX: There's a lot of duplication in these functions.
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "config.h"
#include "globals.h"
#include "GNode.h"
#include "make.h"
#include "nonints.h"
#include "parse.h"
#include "str.h"
#include "targ.h"
#include "util.h"
#include "var.h"

/*
 * This is a harmless return value for Var_Parse that can be used by Var_Subst
 * to determine if there was an error in parsing -- easier than returning
 * a flag, as things outside this module don't give a hoot.
 */
char 	var_Error[] = "";

/*
 * Similar to var_Error, but returned when the 'err' flag for Var_Parse is
 * set false. Why not just use a constant? Well, gcc likes to condense
 * identical string instances...
 */
static char	varNoError[] = "";

/*
 * Internally, variables are contained in four different contexts.
 *	1) the environment. They may not be changed. If an environment
 *	    variable is appended-to, the result is placed in the global
 *	    context.
 *	2) the global context. Variables set in the Makefile are located in
 *	    the global context. It is the penultimate context searched when
 *	    substituting.
 *	3) the command-line context. All variables set on the command line
 *	   are placed in this context. They are UNALTERABLE once placed here.
 *	4) the local context. Each target has associated with it a context
 *	   list. On this list are located the structures describing such
 *	   local variables as $(@) and $(*)
 * The four contexts are searched in the reverse order from which they are
 * listed.
 */
GNode          *VAR_GLOBAL;   /* variables from the makefile */
GNode          *VAR_CMD;      /* variables defined on the command-line */

#define	FIND_CMD	0x1   /* look in VAR_CMD when searching */
#define	FIND_GLOBAL	0x2   /* look in VAR_GLOBAL as well */
#define	FIND_ENV  	0x4   /* look in the environment also */

#define	OPEN_PAREN		'('
#define	CLOSE_PAREN		')'
#define	OPEN_BRACE		'{'
#define	CLOSE_BRACE		'}'

/*
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

/*
 * Destroy a Var object.
 *
 * Params:
 * 	v	Object to destroy.
 * 	f	True if internal buffer in Buffer object is to be removed.
 */
static void
VarDestroy(Var *v, Boolean f)
{

	Buf_Destroy(v->val, f);
	free(v->name);
	free(v);
}

/*-
 *-----------------------------------------------------------------------
 * VarCmp  --
 *	See if the given variable matches the named one. Called from
 *	Lst_Find when searching for a variable of a given name.
 *
 * Results:
 *	0 if they match. non-zero otherwise.
 *
 * Side Effects:
 *	none
 *-----------------------------------------------------------------------
 */
static int
VarCmp(const void *v, const void *name)
{

    return (strcmp(name, ((const Var *)v)->name));
}

/*-
 *-----------------------------------------------------------------------
 * VarPossiblyExpand --
 *	Expand a variable name's embedded variables in the given context.
 *
 * Results:
 *	The contents of name, possibly expanded.
 *-----------------------------------------------------------------------
 */
static char *
VarPossiblyExpand(const char *name, GNode *ctxt)
{
	Buffer	*buf;
	char	*str;
	char	*tmp;

	/*
	 * XXX make a temporary copy of the name because Var_Subst insists
	 * on writing into the string.
	 */
	tmp = estrdup(name);
	if (strchr(name, '$') != NULL) {
		buf = Var_Subst(NULL, tmp, ctxt, 0);
		str = Buf_GetAll(buf, NULL);
		Buf_Destroy(buf, FALSE);

		free(tmp);
		return (str);
	} else {
		return (tmp);
	}
}

/*-
 *-----------------------------------------------------------------------
 * VarFind --
 *	Find the given variable in the given context and any other contexts
 *	indicated.
 *
 *	Flags:
 *		FIND_GLOBAL	set means look in the VAR_GLOBAL context too
 *		FIND_CMD	set means to look in the VAR_CMD context too
 *		FIND_ENV	set means to look in the environment
 *
 * Results:
 *	A pointer to the structure describing the desired variable or
 *	NULL if the variable does not exist.
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static Var *
VarFind(const char *name, GNode *ctxt, int flags)
{
	Boolean	localCheckEnvFirst;
	LstNode	*var;
	char	*env;

	/*
	 * If the variable name begins with a '.', it could very well be one of
	 * the local ones.  We check the name against all the local variables
	 * and substitute the short version in for 'name' if it matches one of
	 * them.
	 */
	if (name[0] == '.') {
		switch (name[1]) {
		case 'A':
			if (!strcmp(name, ".ALLSRC"))
				name = ALLSRC;
			if (!strcmp(name, ".ARCHIVE"))
				name = ARCHIVE;
			break;
		case 'I':
			if (!strcmp(name, ".IMPSRC"))
				name = IMPSRC;
			break;
		case 'M':
			if (!strcmp(name, ".MEMBER"))
				name = MEMBER;
			break;
		case 'O':
			if (!strcmp(name, ".OODATE"))
				name = OODATE;
			break;
		case 'P':
			if (!strcmp(name, ".PREFIX"))
				name = PREFIX;
			break;
		case 'T':
			if (!strcmp(name, ".TARGET"))
				name = TARGET;
			break;
		default:
			break;
		}
	}

	/*
	 * Note whether this is one of the specific variables we were told
	 * through the -E flag to use environment-variable-override for.
	 */
	if (Lst_Find(&envFirstVars, name, (CompareProc *)strcmp) != NULL) {
		localCheckEnvFirst = TRUE;
	} else {
		localCheckEnvFirst = FALSE;
	}

	/*
	 * First look for the variable in the given context. If it's not there,
	 * look for it in VAR_CMD, VAR_GLOBAL and the environment,
	 * in that order, depending on the FIND_* flags in 'flags'
	 */
	var = Lst_Find(&ctxt->context, name, VarCmp);
	if (var != NULL) {
		/* got it */
		return (Lst_Datum(var));
	}

	/* not there - try command line context */
	if ((flags & FIND_CMD) && (ctxt != VAR_CMD)) {
		var = Lst_Find(&VAR_CMD->context, name, VarCmp);
		if (var != NULL)
			return (Lst_Datum(var));
	}

	/* not there - try global context, but only if not -e/-E */
	if ((flags & FIND_GLOBAL) && (ctxt != VAR_GLOBAL) &&
	    !checkEnvFirst && !localCheckEnvFirst) {
		var = Lst_Find(&VAR_GLOBAL->context, name, VarCmp);
		if (var != NULL)
			return (Lst_Datum(var));
	}

	if (!(flags & FIND_ENV))
		/* we were not told to look into the environment */
		return (NULL);

	/* look in the environment */
	if ((env = getenv(name)) != NULL) {
		/* craft this variable from the environment value */
		return (VarCreate(name, env, VAR_FROM_ENV));
	}

	/* deferred check for the environment (in case of -e/-E) */
	if ((checkEnvFirst || localCheckEnvFirst) &&
	    (flags & FIND_GLOBAL) && (ctxt != VAR_GLOBAL)) {
		var = Lst_Find(&VAR_GLOBAL->context, name, VarCmp);
		if (var != NULL)
			return (Lst_Datum(var));
	}
	return (NULL);
}

/*-
 *-----------------------------------------------------------------------
 * VarAdd  --
 *	Add a new variable of name name and value val to the given context.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The new variable is placed at the front of the given context
 *	The name and val arguments are duplicated so they may
 *	safely be freed.
 *-----------------------------------------------------------------------
 */
static void
VarAdd(const char *name, const char *val, GNode *ctxt)
{

    Lst_AtFront(&ctxt->context, VarCreate(name, val, 0));
    DEBUGF(VAR, ("%s:%s = %s\n", ctxt->name, name, val));
}

/*-
 *-----------------------------------------------------------------------
 * Var_Delete --
 *	Remove a variable from a context.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The Var structure is removed and freed.
 *
 *-----------------------------------------------------------------------
 */
void
Var_Delete(const char *name, GNode *ctxt)
{
    LstNode *ln;

    DEBUGF(VAR, ("%s:delete %s\n", ctxt->name, name));
    ln = Lst_Find(&ctxt->context, name, VarCmp);
    if (ln != NULL) {
	VarDestroy(Lst_Datum(ln), TRUE);
	Lst_Remove(&ctxt->context, ln);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Var_Set --
 *	Set the variable name to the value val in the given context.
 *
 * Results:
 *	None.
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
 *-----------------------------------------------------------------------
 */
void
Var_Set(const char *name, const char *val, GNode *ctxt)
{
    Var		*v;
    char	*n;

    /*
     * We only look for a variable in the given context since anything set
     * here will override anything in a lower context, so there's not much
     * point in searching them all just to save a bit of memory...
     */
    n = VarPossiblyExpand(name, ctxt);
    v = VarFind(n, ctxt, 0);
    if (v == NULL) {
	VarAdd(n, val, ctxt);
    } else {
	Buf_Clear(v->val);
	Buf_Append(v->val, val);

	DEBUGF(VAR, ("%s:%s = %s\n", ctxt->name, n, val));
    }
    /*
     * Any variables given on the command line are automatically exported
     * to the environment (as per POSIX standard)
     */
    if (ctxt == VAR_CMD) {
	setenv(n, val, 1);
    }
    free(n);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Append --
 *	The variable of the given name has the given value appended to it in
 *	the given context.
 *
 * Results:
 *	None
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
 *-----------------------------------------------------------------------
 */
void
Var_Append(const char *name, const char *val, GNode *ctxt)
{
    Var		*v;
    char	*n;

    n = VarPossiblyExpand(name, ctxt);
    v = VarFind(n, ctxt, (ctxt == VAR_GLOBAL) ? FIND_ENV : 0);

    if (v == NULL) {
	VarAdd(n, val, ctxt);
    } else {
	Buf_AddByte(v->val, (Byte)' ');
	Buf_Append(v->val, val);

	DEBUGF(VAR, ("%s:%s = %s\n", ctxt->name, n,
	       (char *)Buf_GetAll(v->val, (size_t *)NULL)));

	if (v->flags & VAR_FROM_ENV) {
	    /*
	     * If the original variable came from the environment, we
	     * have to install it in the global context (we could place
	     * it in the environment, but then we should provide a way to
	     * export other variables...)
	     */
	    v->flags &= ~VAR_FROM_ENV;
	    Lst_AtFront(&ctxt->context, v);
	}
    }
    free(n);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Exists --
 *	See if the given variable exists.
 *
 * Results:
 *	TRUE if it does, FALSE if it doesn't
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
Boolean
Var_Exists(const char *name, GNode *ctxt)
{
    Var		*v;
    char	*n;

    n = VarPossiblyExpand(name, ctxt);
    v = VarFind(n, ctxt, FIND_CMD|FIND_GLOBAL|FIND_ENV);
    free(n);

    if (v == NULL) {
	return (FALSE);
    } else if (v->flags & VAR_FROM_ENV) {
	VarDestroy(v, TRUE);
    }
    return (TRUE);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Value --
 *	Return the value of the named variable in the given context
 *
 * Results:
 *	The value if the variable exists, NULL if it doesn't
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
char *
Var_Value(const char *name, GNode *ctxt, char **frp)
{
    Var		*v;
    char	*n;

    n = VarPossiblyExpand(name, ctxt);
    v = VarFind(n, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
    free(n);
    *frp = NULL;
    if (v != NULL) {
	char *p = (char *)Buf_GetAll(v->val, (size_t *)NULL);

	if (v->flags & VAR_FROM_ENV) {
	    VarDestroy(v, FALSE);
	    *frp = p;
	}
	return (p);
    } else {
	return (NULL);
    }
}

/*-
 *-----------------------------------------------------------------------
 * VarModify --
 *	Modify each of the words of the passed string using the given
 *	function. Used to implement all modifiers.
 *
 * Results:
 *	A string of all the words modified appropriately.
 *
 * Side Effects:
 *	Uses brk_string() so it invalidates any previous call to
 *	brk_string().
 *
 *-----------------------------------------------------------------------
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
	char	*result;

	av = brk_string(str, &ac, FALSE);

	buf = Buf_Init(0);

	addSpace = FALSE;
	for (i = 1; i < ac; i++)
		addSpace = (*modProc)(av[i], addSpace, buf, datum);

	result = (char *)Buf_GetAll(buf, (size_t *)NULL);
	Buf_Destroy(buf, FALSE);
	return (result);
}

/*-
 *-----------------------------------------------------------------------
 * VarSortWords --
 *	Sort the words in the string.
 *
 * Input:
 *	str		String whose words should be sorted
 *	cmp		A comparison function to control the ordering
 *
 * Results:
 *	A string containing the words sorted
 *
 * Side Effects:
 *      Uses brk_string() so it invalidates any previous call to
 *	brk_string().
 *
 *-----------------------------------------------------------------------
 */
static char *
VarSortWords(const char *str, int (*cmp)(const void *, const void *))
{
	char	**av;
	int	ac;
	Buffer	*buf;
	int	i;
	char	*result;

	av = brk_string(str, &ac, FALSE);
	qsort(av + 1, ac - 1, sizeof(char *), cmp);

	buf = Buf_Init(0);
	for (i = 1; i < ac; i++) {
		Buf_Append(buf, av[i]);
		Buf_AddByte(buf, (Byte)((i < ac - 1) ? ' ' : '\0'));
	}

	result = (char *)Buf_GetAll(buf, (size_t *)NULL);
	Buf_Destroy(buf, FALSE);
	return (result);
}

static int
SortIncreasing(const void *l, const void *r)
{

	return (strcmp(*(const char* const*)l, *(const char* const*)r));
}

/*-
 *-----------------------------------------------------------------------
 * VarGetPattern --
 *	Pass through the tstr looking for 1) escaped delimiters,
 *	'$'s and backslashes (place the escaped character in
 *	uninterpreted) and 2) unescaped $'s that aren't before
 *	the delimiter (expand the variable substitution unless flags
 *	has VAR_NOSUBST set).
 *	Return the expanded string or NULL if the delimiter was missing
 *	If pattern is specified, handle escaped ampersands, and replace
 *	unescaped ampersands with the lhs of the pattern.
 *
 * Results:
 *	A string of all the words modified appropriately.
 *	If length is specified, return the string length of the buffer
 *	If flags is specified and the last character of the pattern is a
 *	$ set the VAR_MATCH_END bit of flags.
 *
 * Side Effects:
 *	None.
 *-----------------------------------------------------------------------
 */
static char *
VarGetPattern(GNode *ctxt, int err, const char **tstr, int delim, int *flags,
    size_t *length, VarPattern *pattern)
{
    const char *cp;
    Buffer *buf = Buf_Init(0);
    size_t junk;

    if (length == NULL)
	length = &junk;

#define	IS_A_MATCH(cp, delim) \
    ((cp[0] == '\\') && ((cp[1] == delim) ||  \
     (cp[1] == '\\') || (cp[1] == '$') || (pattern && (cp[1] == '&'))))

    /*
     * Skim through until the matching delimiter is found;
     * pick up variable substitutions on the way. Also allow
     * backslashes to quote the delimiter, $, and \, but don't
     * touch other backslashes.
     */
    for (cp = *tstr; *cp && (*cp != delim); cp++) {
	if (IS_A_MATCH(cp, delim)) {
	    Buf_AddByte(buf, (Byte)cp[1]);
	    cp++;
	} else if (*cp == '$') {
	    if (cp[1] == delim) {
		if (flags == NULL)
		    Buf_AddByte(buf, (Byte)*cp);
		else
		    /*
		     * Unescaped $ at end of pattern => anchor
		     * pattern at end.
		     */
		    *flags |= VAR_MATCH_END;
	    } else {
		if (flags == NULL || (*flags & VAR_NOSUBST) == 0) {
		    char   *cp2;
		    size_t len;
		    Boolean freeIt;

		    /*
		     * If unescaped dollar sign not before the
		     * delimiter, assume it's a variable
		     * substitution and recurse.
		     */
		    len = 0;
		    cp2 = Var_Parse(cp, ctxt, err, &len, &freeIt);
		    Buf_Append(buf, cp2);
		    if (freeIt)
			free(cp2);
		    cp += len - 1;
		} else {
		    const char *cp2 = &cp[1];

		    if (*cp2 == OPEN_PAREN || *cp2 == OPEN_BRACE) {
			/*
			 * Find the end of this variable reference
			 * and suck it in without further ado.
			 * It will be interperated later.
			 */
			int have = *cp2;
			int want = (*cp2 == OPEN_PAREN) ? CLOSE_PAREN : CLOSE_BRACE;
			int depth = 1;

			for (++cp2; *cp2 != '\0' && depth > 0; ++cp2) {
			    if (cp2[-1] != '\\') {
				if (*cp2 == have)
				    ++depth;
				if (*cp2 == want)
				    --depth;
			    }
			}
			Buf_AppendRange(buf, cp, cp2);
			cp = --cp2;
		    } else
			Buf_AddByte(buf, (Byte)*cp);
		}
	    }
	}
	else if (pattern && *cp == '&')
	    Buf_AddBytes(buf, pattern->leftLen, (Byte *)pattern->lhs);
	else
	    Buf_AddByte(buf, (Byte)*cp);
    }

    Buf_AddByte(buf, (Byte)'\0');

    if (*cp != delim) {
	*tstr = cp;
	*length = 0;
	return (NULL);
    } else {
	char *result;
	*tstr = ++cp;
	result = (char *)Buf_GetAll(buf, length);
	*length -= 1;	/* Don't count the NULL */
	Buf_Destroy(buf, FALSE);
	return (result);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Var_Quote --
 *	Quote shell meta-characters in the string
 *
 * Results:
 *	The quoted string
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
char *
Var_Quote(const char *str)
{
    Buffer  	 *buf;
    /* This should cover most shells :-( */
    static char meta[] = "\n \t'`\";&<>()|*?{}[]\\$!#^~";
    char 	  *ret;

    buf = Buf_Init(MAKE_BSIZE);
    for (; *str; str++) {
	if (strchr(meta, *str) != NULL)
	    Buf_AddByte(buf, (Byte)'\\');
	Buf_AddByte(buf, (Byte)*str);
    }
    Buf_AddByte(buf, (Byte)'\0');
    ret = Buf_GetAll(buf, NULL);
    Buf_Destroy(buf, FALSE);
    return (ret);
}

/*-
 *-----------------------------------------------------------------------
 * VarREError --
 *	Print the error caused by a regcomp or regexec call.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	An error gets printed.
 *
 *-----------------------------------------------------------------------
 */
void
VarREError(int err, regex_t *pat, const char *str)
{
    char *errbuf;
    int errlen;

    errlen = regerror(err, pat, 0, 0);
    errbuf = emalloc(errlen);
    regerror(err, pat, errbuf, errlen);
    Error("%s: %s", str, errbuf);
    free(errbuf);
}

/*
 * Make sure this variable is fully expanded.
 */
static char *
VarExpand(Var *v, GNode *ctxt, Boolean err)
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
	value = (char *)Buf_GetAll(v->val, (size_t *)NULL);
	if (strchr(value, '$') == NULL) {
		result = strdup(value);
	} else {
		Buffer	*buf;

		buf = Var_Subst(NULL, value, ctxt, err);
		result = Buf_GetAll(buf, NULL);
		Buf_Destroy(buf, FALSE);
	}

	v->flags &= ~VAR_IN_USE;

	return (result);
}

/**
 * Select only those words that match the modifier.
 */
static char *
modifier_M(const char mod[], const char value[], char endc, size_t *consumed)
{
	const char	*cur;
	char		*patt;
	char		*ptr;
	char		*newValue;

	/*
	 * Compress the \:'s out of the pattern, so allocate enough
	 * room to hold the uncompressed pattern and compress the
	 * pattern into that space.
	 */
	patt = estrdup(mod);
	ptr = patt;
	for (cur = mod + 1; cur != '\0'; cur++) {
		if ((cur[0] == endc) || (cur[0] == ':')) {
			break;
		}
		if ((cur[0] == '\\') &&
		    ((cur[1] == endc) || (cur[1] == ':'))) {
			cur++;	/* skip over backslash */
		}
		*ptr = *cur;
		ptr++;
	}
	*ptr = '\0';

	if (*mod == 'M' || *mod == 'm') {
		newValue = VarModify(value, VarMatch, patt);
	} else {
		newValue = VarModify(value, VarNoMatch, patt);
	}
	free(patt);

	*consumed += (cur - mod);

	if (*cur == ':') {
		*consumed += 1;	/* include colon as part of modifier */
	}

	return (newValue);
}

static char *
modifier_S(const char mod[], const char value[], Var *v, GNode *ctxt, Boolean err, size_t *consumed)
{
	VarPattern	pattern;
	Buffer		*buf;		/* Buffer for patterns */
	char		delim;
	const char	*cur;
	char		*newValue;

	pattern.flags = 0;
	buf = Buf_Init(0);

	delim = mod[1];	/* used to find end of pattern */

	/*
	 * If pattern begins with '^', it is anchored to the start of the
	 * word -- skip over it and flag pattern.
	 */
	if (mod[2] == '^') {
		pattern.flags |= VAR_MATCH_START;
		cur = mod + 3;
	} else {
		cur = mod + 2;
	}

	/*
	 * Pass through the lhs looking for 1) escaped delimiters, '$'s and
	 * backslashes (place the escaped character in uninterpreted) and 2)
	 * unescaped $'s that aren't before the delimiter (expand the
	 * variable substitution). The result is left in the Buffer buf.
	 */
	while (cur[0] != delim) {
		if (cur[0] == '\0') {
			/*
			 * LHS didn't end with the delim, complain and exit.
			 */
			Fatal("Unclosed substitution for %s (%c missing)",
			      v->name, delim);

		} else if ((cur[0] == '\\') &&
			   ((cur[1] == delim) ||
			    (cur[1] == '$') ||
			    (cur[1] == '\\'))) {
			cur++;	/* skip backslash */
			Buf_AddByte(buf, (Byte) cur[0]);
			cur++;

		} else if (cur[0] == '$') {
			if (cur[1] == delim) {
				/*
				 * Unescaped $ at end of pattern => anchor
				 * pattern at end.
				 */
				pattern.flags |= VAR_MATCH_END;
				cur++;
			} else {
				/*
				 * If unescaped dollar sign not before the
				 * delimiter, assume it's a variable
				 * substitution and recurse.
				 */
				char   *cp2;
				size_t  len;
				Boolean freeIt;

				len = 0;
				cp2 = Var_Parse(cur, ctxt, err, &len, &freeIt);
				cur += len;
				Buf_Append(buf, cp2);
				if (freeIt) {
					free(cp2);
				}
			}
		} else {
			Buf_AddByte(buf, (Byte)cur[0]);
			cur++;
		}
	}
	cur++;	/* skip over delim */

	/*
	 * Fetch pattern and destroy buffer, but preserve the data in it,
	 * since that's our lhs.
	 */
	pattern.lhs = (char *)Buf_GetAll(buf, &pattern.leftLen);
	Buf_Destroy(buf, FALSE);

	/*
	 * Now comes the replacement string. Three things need to be done
	 * here: 1) need to compress escaped delimiters and ampersands and 2)
	 * need to replace unescaped ampersands with the l.h.s. (since this
	 * isn't regexp, we can do it right here) and 3) expand any variable
	 * substitutions.
	 */
	buf = Buf_Init(0);

	while (cur[0] != delim) {
		if (cur[0] == '\0') {
			/*
			 * Didn't end with delim character, complain
			 */
			Fatal("Unclosed substitution for %s (%c missing)",
			      v->name, delim);

		} else if ((cur[0] == '\\') &&
		    ((cur[1] == delim) ||
		     (cur[1] == '&') ||
		     (cur[1] == '\\') ||
		     (cur[1] == '$'))) {
			cur++;	/* skip backslash */
			Buf_AddByte(buf, (Byte) cur[0]);
			cur++;

		} else if (cur[0] == '$') {
			 if (cur[1] == delim) {
				Buf_AddByte(buf, (Byte) cur[0]);
				cur++;
			} else {
				char   *cp2;
				size_t  len;
				Boolean freeIt;

				len = 0;
				cp2 = Var_Parse(cur, ctxt, err, &len, &freeIt);
				cur += len;
				Buf_Append(buf, cp2);
				if (freeIt) {
					free(cp2);
				}
			}
		} else if (cur[0] == '&') {
			Buf_AddBytes(buf, pattern.leftLen, (Byte *)pattern.lhs);
			cur++;
		} else {
			Buf_AddByte(buf, (Byte) cur[0]);
			cur++;
		}
	}
	cur++;	/* skip over delim */

	pattern.rhs = (char *)Buf_GetAll(buf, &pattern.rightLen);
	Buf_Destroy(buf, FALSE);

	/*
	 * Check for global substitution. If 'g' after the final delimiter,
	 * substitution is global and is marked that way.
	 */
	if (cur[0] == 'g') {
		pattern.flags |= VAR_SUB_GLOBAL;
		cur++;
	}

	/*
	 * Global substitution of the empty string causes an infinite number
	 * of matches, unless anchored by '^' (start of string) or '$' (end
	 * of string). Catch the infinite substitution here. Note that flags
	 * can only contain the 3 bits we're interested in so we don't have
	 * to mask unrelated bits. We can test for equality.
	 */
	if (!pattern.leftLen && pattern.flags == VAR_SUB_GLOBAL)
		Fatal("Global substitution of the empty string");

	newValue = VarModify(value, VarSubstitute, &pattern);

	/*
	 * Free the two strings.
	 */
	free(pattern.lhs);
	free(pattern.rhs);

	*consumed += (cur - mod);

	if (cur[0] == ':') {
		*consumed += 1;	/* include colin as part of modifier */
	}

	return (newValue);
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
ParseModifier(const char input[], const char tstr[],
	char startc, char endc, Boolean dynamic, Var *v,
	GNode *ctxt, Boolean err, size_t *lengthPtr, Boolean *freePtr)
{
	char		*value;
	const char	*cp;
	size_t		used;

	value = VarExpand(v, ctxt, err);
	*freePtr = TRUE;

	tstr++;
	while (*tstr != endc) {
	    char	*newStr;    /* New value to return */
	    char	termc;	    /* Character which terminated scan */
	    Boolean	readonly = FALSE;
	    size_t	consumed = 0;

	    DEBUGF(VAR, ("Applying :%c to \"%s\"\n", *tstr, value));
	    switch (*tstr) {
		case 'N':
		case 'M':
			readonly = TRUE; /* tstr isn't modified here */

			newStr = modifier_M(tstr, value, endc, &consumed);
			tstr += consumed;
			break;
		case 'S':

			readonly = TRUE; /* tstr isn't modified here */

			newStr = modifier_S(tstr, value, v, ctxt, err, &consumed);
			tstr += consumed;
			break;
		case 'C':
		{
		    int		delim;
		    VarREPattern    pattern;
		    char	   *re;
		    int		    error;

		    pattern.flags = 0;
		    delim = tstr[1];
		    tstr += 2;

		    cp = tstr;

		    if ((re = VarGetPattern(ctxt, err, &cp, delim, NULL,
			NULL, NULL)) == NULL) {
			*lengthPtr = cp - input + 1;
			if (*freePtr)
			    free(value);
			if (delim != '\0')
			    Fatal("Unclosed substitution for %s (%c missing)",
				  v->name, delim);
			return (var_Error);
		    }

		    if ((pattern.replace = VarGetPattern(ctxt, err, &cp,
			delim, NULL, NULL, NULL)) == NULL){
			free(re);

			/* was: goto cleanup */
			*lengthPtr = cp - input + 1;
			if (*freePtr)
			    free(value);
			if (delim != '\0')
			    Fatal("Unclosed substitution for %s (%c missing)",
				  v->name, delim);
			return (var_Error);
		    }

		    for (;; cp++) {
			switch (*cp) {
			case 'g':
			    pattern.flags |= VAR_SUB_GLOBAL;
			    continue;
			case '1':
			    pattern.flags |= VAR_SUB_ONE;
			    continue;
			default:
			    break;
			}
			break;
		    }

		    termc = *cp;

		    error = regcomp(&pattern.re, re, REG_EXTENDED);
		    free(re);
		    if (error)	{
			*lengthPtr = cp - input + 1;
			VarREError(error, &pattern.re, "RE substitution error");
			free(pattern.replace);
			return (var_Error);
		    }

		    pattern.nsub = pattern.re.re_nsub + 1;
		    if (pattern.nsub < 1)
			pattern.nsub = 1;
		    if (pattern.nsub > 10)
			pattern.nsub = 10;
		    pattern.matches = emalloc(pattern.nsub *
					      sizeof(regmatch_t));
		    newStr = VarModify(value, VarRESubstitute, &pattern);
		    regfree(&pattern.re);
		    free(pattern.replace);
		    free(pattern.matches);
		    break;
		}
		case 'L':
		    if (tstr[1] == endc || tstr[1] == ':') {
			Buffer *buf;
			buf = Buf_Init(MAKE_BSIZE);
			for (cp = value; *cp ; cp++)
			    Buf_AddByte(buf, (Byte)tolower(*cp));

			Buf_AddByte(buf, (Byte)'\0');
			newStr = (char *)Buf_GetAll(buf, (size_t *)NULL);
			Buf_Destroy(buf, FALSE);

			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /* FALLTHROUGH */
		case 'O':
		    if (tstr[1] == endc || tstr[1] == ':') {
			newStr = VarSortWords(value, SortIncreasing);
			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /* FALLTHROUGH */
		case 'Q':
		    if (tstr[1] == endc || tstr[1] == ':') {
			newStr = Var_Quote(value);
			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /*FALLTHRU*/
		case 'T':
		    if (tstr[1] == endc || tstr[1] == ':') {
			newStr = VarModify(value, VarTail, (void *)NULL);
			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /*FALLTHRU*/
		case 'U':
		    if (tstr[1] == endc || tstr[1] == ':') {
			Buffer *buf;
			buf = Buf_Init(MAKE_BSIZE);
			for (cp = value; *cp ; cp++)
			    Buf_AddByte(buf, (Byte)toupper(*cp));

			Buf_AddByte(buf, (Byte)'\0');
			newStr = (char *)Buf_GetAll(buf, (size_t *)NULL);
			Buf_Destroy(buf, FALSE);

			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /* FALLTHROUGH */
		case 'H':
		    if (tstr[1] == endc || tstr[1] == ':') {
			newStr = VarModify(value, VarHead, (void *)NULL);
			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /*FALLTHRU*/
		case 'E':
		    if (tstr[1] == endc || tstr[1] == ':') {
			newStr = VarModify(value, VarSuffix, (void *)NULL);
			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /*FALLTHRU*/
		case 'R':
		    if (tstr[1] == endc || tstr[1] == ':') {
			newStr = VarModify(value, VarRoot, (void *)NULL);
			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /*FALLTHRU*/
#ifdef SUNSHCMD
		case 's':
		    if (tstr[1] == 'h' && (tstr[2] == endc || tstr[2] == ':')) {
			const char *error;
			Buffer *buf;

			buf = Cmd_Exec(value, &error);
			newStr = Buf_GetAll(buf, NULL);
			Buf_Destroy(buf, FALSE);

			if (error)
			    Error(error, value);
			cp = tstr + 2;
			termc = *cp;
			break;
		    }
		    /*FALLTHRU*/
#endif
		default:
		{
#ifdef SYSVVARSUB
		    /*
		     * This can either be a bogus modifier or a System-V
		     * substitution command.
		     */
		    VarPattern	pattern;
		    Boolean	eqFound;
		    int		cnt;

		    pattern.flags = 0;
		    eqFound = FALSE;
		    /*
		     * First we make a pass through the string trying
		     * to verify it is a SYSV-make-style translation:
		     * it must be: <string1>=<string2>)
		     */
		    cp = tstr;
		    cnt = 1;
		    while (*cp != '\0' && cnt) {
			if (*cp == '=') {
			    eqFound = TRUE;
			    /* continue looking for endc */
			}
			else if (*cp == endc)
			    cnt--;
			else if (*cp == startc)
			    cnt++;
			if (cnt)
			    cp++;
		    }
		    if (*cp == endc && eqFound) {
			int delim;

			/*
			 * Now we break this sucker into the lhs and
			 * rhs. We must null terminate them of course.
			 */
			cp = tstr;

			delim = '=';
			if ((pattern.lhs = VarGetPattern(ctxt,
			    err, &cp, delim, &pattern.flags, &pattern.leftLen,
			    NULL)) == NULL) {
				/* was: goto cleanup */
				*lengthPtr = cp - input + 1;
				if (*freePtr)
				    free(value);
				if (delim != '\0')
				    Fatal("Unclosed substitution for %s (%c missing)",
					  v->name, delim);
				return (var_Error);
			}

			delim = endc;
			if ((pattern.rhs = VarGetPattern(ctxt,
			    err, &cp, delim, NULL, &pattern.rightLen,
			    &pattern)) == NULL) {
				/* was: goto cleanup */
				*lengthPtr = cp - input + 1;
				if (*freePtr)
				    free(value);
				if (delim != '\0')
				    Fatal("Unclosed substitution for %s (%c missing)",
					  v->name, delim);
				return (var_Error);
			}

			/*
			 * SYSV modifications happen through the whole
			 * string. Note the pattern is anchored at the end.
			 */
			termc = *--cp;
			delim = '\0';
			newStr = VarModify(value, VarSYSVMatch, &pattern);

			free(pattern.lhs);
			free(pattern.rhs);

			termc = endc;
		    } else
#endif
		    {
			Error("Unknown modifier '%c'\n", *tstr);
			for (cp = tstr+1;
			     *cp != ':' && *cp != endc && *cp != '\0';
			     cp++)
				 continue;
			termc = *cp;
			newStr = var_Error;
		    }
		}
	    }

	    DEBUGF(VAR, ("Result is \"%s\"\n", newStr));
	    if (*freePtr) {
		    free(value);
	    }
	    value = newStr;
	    if (value != var_Error) {
		    *freePtr = TRUE;
	    } else {
		    *freePtr = FALSE;
	    }

	    if (readonly == FALSE) {
		    if (termc == '\0') {
			    Error("Unclosed variable specification for %s",
				  v->name);
		    } else if (termc == ':') {
			    cp++;
		    } else {
		    }
		    tstr = cp;
	    }
	}

	used = tstr - input + 1;
	*lengthPtr = used;

	if (v->flags & VAR_FROM_ENV) {
		if (value == (char *)Buf_GetAll(v->val, (size_t *)NULL)) {
			VarDestroy(v, FALSE);
			*freePtr = TRUE;
			return (value);
		} else {
			VarDestroy(v, TRUE);
			return (value);
		}
	} else if (v->flags & VAR_JUNK) {
		/*
		 * Perform any free'ing needed and set *freePtr to
		 * FALSE so the caller doesn't try to free a static
		 * pointer.
		 */
		if (*freePtr) {
			free(value);
		}
		if (dynamic) {
			char	*result;

			VarDestroy(v, TRUE);
			result = emalloc(used + 1);
			strncpy(result, input, used);
			result[used] = '\0';

			*freePtr = TRUE;
			return (result);
		} else {
			VarDestroy(v, TRUE);

			*freePtr = FALSE;
			return (err ? var_Error : varNoError);
		}
	} else {
		return (value);
	}
}

static char *
ParseRestModifier(const char input[], const char ptr[], char startc, char endc, Buffer *buf, GNode *ctxt, Boolean err, size_t *lengthPtr, Boolean *freePtr)
{
	const char	*vname;
	size_t		vlen;
	Var		*v;
	Boolean	dynamic;	/* TRUE if the variable is local and we're
				 * expanding it in a non-local context. This
				 * is done to support dynamic sources. The
				 * result is just the invocation, unaltered */

	vname = Buf_GetAll(buf, &vlen);

	dynamic = FALSE;

	v = VarFind(vname, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	if (v != NULL) {
		return (ParseModifier(input, ptr,
				startc, endc, dynamic, v,
				ctxt, err, lengthPtr, freePtr));
	}

	if ((ctxt == VAR_CMD) || (ctxt == VAR_GLOBAL)) {
		if ((vlen == 1) ||
		    ((vlen == 2) && (vname[1] == 'F' || vname[1] == 'D'))) {
			/*
			 * If substituting a local variable in a non-local
			 * context, assume it's for dynamic source stuff. We
			 * have to handle this specially and return the
			 * longhand for the variable with the dollar sign
			 * escaped so it makes it back to the caller. Only
			 * four of the local variables are treated specially
			 * as they are the only four that will be set when
			 * dynamic sources are expanded.
			 */
			if (strchr("!%*@", vname[0]) != NULL) {
				dynamic = TRUE;
			}
		}
		if ((vlen > 2) &&
		    (vname[0] == '.') &&
		    isupper((unsigned char)vname[1])) {
			if ((strncmp(vname, ".TARGET", vlen - 1) == 0) ||
			    (strncmp(vname, ".ARCHIVE", vlen - 1) == 0) ||
			    (strncmp(vname, ".PREFIX", vlen - 1) == 0) ||
			    (strncmp(vname, ".MEMBER", vlen - 1) == 0)) {
				dynamic = TRUE;
			}
		}
		/*
		 * Still need to get to the end of the variable
		 * specification, so kludge up a Var structure for
		 * the modifications
		 */
		v = VarCreate(vname, NULL, VAR_JUNK);
		return (ParseModifier(input, ptr,
				      startc, endc, dynamic, v,
				    ctxt, err, lengthPtr, freePtr));
	} else {
		/*
		 * Check for D and F forms of local variables since we're in
		 * a local context and the name is the right length.
		 */
		if ((vlen == 2) &&
		    (vname[1] == 'F' || vname[1] == 'D') &&
		    (strchr("!%*<>@", vname[0]) != NULL)) {
			char	name[2];

			/*
			 * Well, it's local -- go look for it.
			 */
			name[0] = vname[0];
			name[1] = '\0';

			v = VarFind(name, ctxt, 0);
			if (v != NULL) {
				return (ParseModifier(input, ptr,
						startc, endc, dynamic, v,
						ctxt, err, lengthPtr, freePtr));
			}
		}

		/*
		 * Still need to get to the end of the variable
		 * specification, so kludge up a Var structure for
		 * the modifications
		 */
		v = VarCreate(vname, NULL, VAR_JUNK);
		return (ParseModifier(input, ptr,
				      startc, endc, dynamic, v,
				    ctxt, err, lengthPtr, freePtr));
	}
}

static char *
ParseRestEnd(const char input[], Buffer *buf,
	GNode *ctxt, Boolean err, size_t *consumed, Boolean *freePtr)
{
	const char	*vname;
	size_t		vlen;
	Var		*v;
	char		*value;

	vname = Buf_GetAll(buf, &vlen);

	v = VarFind(vname, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	if (v != NULL) {
		value = VarExpand(v, ctxt, err);

		if (v->flags & VAR_FROM_ENV) {
			VarDestroy(v, TRUE);
		}

		*freePtr = TRUE;
		return (value);
	}

	if ((ctxt == VAR_CMD) || (ctxt == VAR_GLOBAL)) {
		/*
		 * If substituting a local variable in a non-local
		 * context, assume it's for dynamic source stuff. We
		 * have to handle this specially and return the
		 * longhand for the variable with the dollar sign
		 * escaped so it makes it back to the caller. Only
		 * four of the local variables are treated specially
		 * as they are the only four that will be set when
		 * dynamic sources are expanded.
		 */
		if (((vlen == 1)) ||
		    ((vlen == 2) && (vname[1] == 'F' || vname[1] == 'D'))) {
			if (strchr("!%*@", vname[0]) != NULL) {
				value = emalloc(*consumed + 1);
				strncpy(value, input, *consumed);
				value[*consumed] = '\0';

				*freePtr = TRUE;
				return (value);
			}
		}
		if ((vlen > 2) &&
		    (vname[0] == '.') &&
		    isupper((unsigned char)vname[1])) {
			if ((strncmp(vname, ".TARGET", vlen - 1) == 0) ||
			    (strncmp(vname, ".ARCHIVE", vlen - 1) == 0) ||
			    (strncmp(vname, ".PREFIX", vlen - 1) == 0) ||
			    (strncmp(vname, ".MEMBER", vlen - 1) == 0)) {
				value = emalloc(*consumed + 1);
				strncpy(value, input, *consumed);
				value[*consumed] = '\0';

				*freePtr = TRUE;
				return (value);
			}
		}

		*freePtr = FALSE;
		return (err ? var_Error : varNoError);
	} else {
		/*
		 * Check for D and F forms of local variables since we're in
		 * a local context and the name is the right length.
		 */
		if ((vlen == 2) &&
		    (vname[1] == 'F' || vname[1] == 'D') &&
		    (strchr("!%*<>@", vname[0]) != NULL)) {
			char	name[2];

			name[0] = vname[0];
			name[1] = '\0';

			v = VarFind(name, ctxt, 0);
			if (v != NULL) {
				char	*val;
				/*
				 * No need for nested expansion or
				 * anything, as we're the only one
				 * who sets these things and we sure
				 * don't put nested invocations in
				 * them...
				 */
				val = (char *)Buf_GetAll(v->val, NULL);

				if (vname[1] == 'D') {
					val = VarModify(val, VarHead, NULL);
				} else {
					val = VarModify(val, VarTail, NULL);
				}

				*freePtr = TRUE;
				return (val);
			}
		}

		*freePtr = FALSE;
		return (err ? var_Error : varNoError);
	}
}

/**
 * Parse a multi letter variable name, and return it's value.
 */
static char *
VarParseLong(const char input[], GNode *ctxt, Boolean err,
	size_t *consumed, Boolean *freePtr)
{
	Buffer		*buf;
	char		startc;
	char		endc;
	const char	*ptr;
	char		*result;

	buf = Buf_Init(MAKE_BSIZE);

	/*
	 * Process characters until we reach an end character or a
	 * colon, replacing embedded variables as we go.
	 */
	startc = input[0];
	endc = (startc == OPEN_PAREN) ? CLOSE_PAREN : CLOSE_BRACE;

	*consumed += 1;	/* consume opening paren or brace */
	input++;

	ptr = input;
	while (*ptr != endc) {
		if (*ptr == '\0') {
			/*
			 * If we did not find the end character,
			 * return var_Error right now, setting the
			 * length to be the distance to the end of
			 * the string, since that's what make does.
			 */
			Buf_Destroy(buf, TRUE);
			*freePtr = FALSE;
			return (var_Error);

		} else if (*ptr == ':') {
			result = ParseRestModifier(input - 2, ptr,
				     startc, endc, buf,
				     ctxt, err, consumed, freePtr);
			Buf_Destroy(buf, TRUE);
			return (result);

		} else if (*ptr == '$') {
			size_t	rlen;
			Boolean	rfree;
			char	*rval;

			rlen = 0;
			rval = Var_Parse(ptr, ctxt, err, &rlen, &rfree);
			if (rval == var_Error) {
				Fatal("Error expanding embedded variable.");
			}
			Buf_Append(buf, rval);
			if (rfree)
				free(rval);
			*consumed += rlen;
			ptr += rlen;

		} else {
			Buf_AddByte(buf, (Byte)*ptr);
			*consumed += 1;
			ptr++;
		}
	}

	*consumed += 1;	/* consume closing paren or brace */

	result = ParseRestEnd(input - 2, buf, ctxt, err, consumed, freePtr);

	Buf_Destroy(buf, TRUE);
	return (result);
}

/**
 * Parse a single letter variable name, and return it's value.
 */
static char *
VarParseShort(const char input[], GNode *ctxt, Boolean err,
	size_t *consumed, Boolean *freeResult)
{
	char	vname[2];
	Var	*v;
	char	*value;

	vname[0] = input[0];
	vname[1] = '\0';

	*consumed += 1;	/* consume single letter */

	v = VarFind(vname, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	if (v != NULL) {
		value = VarExpand(v, ctxt, err);

		if (v->flags & VAR_FROM_ENV) {
			VarDestroy(v, TRUE);
		}

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
	if ((ctxt == VAR_CMD) || (ctxt == VAR_GLOBAL)) {

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
			return (err ? var_Error : varNoError);
		}
	}

	/*
	 * Variable name was not found.
	 */
	*freeResult = FALSE;
	return (err ? var_Error : varNoError);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Parse --
 *	Given the start of a variable invocation, extract the variable
 *	name and find its value, then modify it according to the
 *	specification.
 *
 * Results:
 *	The value of the variable or var_Error if the specification
 *	is invalid.  The number of characters in the specification
 *	is placed in the variable pointed to by consumed.  (for
 *	invalid specifications, this is just 2 to skip the '$' and
 *	the following letter, or 1 if '$' was the last character
 *	in the string).  A Boolean in *freeResult telling whether the
 *	returned string should be freed by the caller.
 *
 * Side Effects:
 *	None.
 *
 * Assumption:
 *	It is assumed that Var_Parse() is called with input[0] == '$'.
 *
 *-----------------------------------------------------------------------
 */
char *
Var_Parse(const char input[], GNode *ctxt, Boolean err,
	size_t *consumed, Boolean *freeResult)
{
	/* assert(input[0] == '$'); */

	*consumed += 1;	/* consume '$' */
	input += 1;

	if (input[0] == '\0') {
		/* Error, there is only a dollar sign in the input string. */
		*freeResult = FALSE;
		return (err ? var_Error : varNoError);

	} else if (input[0] == OPEN_PAREN || input[0] == OPEN_BRACE) {
		/* multi letter variable name */
		return (VarParseLong(input, ctxt, err, consumed, freeResult));

	} else {
		/* single letter variable name */
		return (VarParseShort(input, ctxt, err, consumed, freeResult));
	}
}

/*-
 *-----------------------------------------------------------------------
 * Var_Subst  --
 *	Substitute for all variables in the given string in the given context
 *	If undefErr is TRUE, Parse_Error will be called when an undefined
 *	variable is encountered.
 *
 * Results:
 *	The resulting string.
 *
 * Side Effects:
 *	None. The old string must be freed by the caller
 *-----------------------------------------------------------------------
 */
Buffer *
Var_Subst(const char *var, const char *str, GNode *ctxt, Boolean undefErr)
{
    Boolean	errorReported;
    Buffer	*buf;		/* Buffer for forming things */

    /*
     * Set TRUE if an error has already been reported to prevent a
     * plethora of messages when recursing.
     * XXXHB this comment sounds wrong.
     */
    errorReported = FALSE;

    buf = Buf_Init(0);
    while (*str) {
	if (var == NULL && (str[0] == '$') && (str[1] == '$')) {
	    /*
	     * A dollar sign may be escaped either with another dollar sign.
	     * In such a case, we skip over the escape character and store the
	     * dollar sign into the buffer directly.
	     */
	    Buf_AddByte(buf, (Byte)str[0]);
	    str += 2;

	} else if (str[0] == '$') {
	    char	*val;	/* Value to substitute for a variable */
	    size_t	length;	/* Length of the variable invocation */
	    Boolean	doFree;	/* Set true if val should be freed */

	    /*
	     * Variable invocation.
	     */
	    if (var != NULL) {
		int expand;
		for (;;) {
		    if (str[1] == OPEN_PAREN || str[1] == OPEN_BRACE) {
			size_t		ln;
			const char	*p = str + 2;

			/*
			 * Scan up to the end of the variable name.
			 */
			while (*p != '\0' &&
			       *p != ':' &&
			       *p != CLOSE_PAREN &&
			       *p != CLOSE_BRACE &&
			       *p != '$') {
			    ++p;
			}

			/*
			 * A variable inside the variable. We cannot expand
			 * the external variable yet, so we try again with
			 * the nested one
			 */
			if (*p == '$') {
			    Buf_AppendRange(buf, str, p);
			    str = p;
			    continue;
			}

			ln = p - (str + 2);
			if (var[ln] == '\0' && strncmp(var, str + 2, ln) == 0) {
			    expand = TRUE;
			} else {
			    /*
			     * Not the variable we want to expand, scan
			     * until the next variable
			     */
			    while (*p != '$' && *p != '\0')
				p++;

			    Buf_AppendRange(buf, str, p);
			    str = p;
			    expand = FALSE;
			}
		    } else {
			/*
			 * Single letter variable name
			 */
			if (var[1] == '\0' && var[0] == str[1]) {
			    expand = TRUE;
			} else {
			    Buf_AddBytes(buf, 2, (const Byte *)str);
			    str += 2;
			    expand = FALSE;
			}
		    }
		    break;
		}
		if (!expand)
		    continue;
	    }

	    length = 0;
	    val = Var_Parse(str, ctxt, undefErr, &length, &doFree);

	    /*
	     * When we come down here, val should either point to the
	     * value of this variable, suitably modified, or be NULL.
	     * Length should be the total length of the potential
	     * variable invocation (from $ to end character...)
	     */
	    if (val == var_Error || val == varNoError) {
		/*
		 * If performing old-time variable substitution, skip over
		 * the variable and continue with the substitution. Otherwise,
		 * store the dollar sign and advance str so we continue with
		 * the string...
		 */
		if (oldVars) {
		    str += length;
		} else if (undefErr) {
		    /*
		     * If variable is undefined, complain and skip the
		     * variable. The complaint will stop us from doing anything
		     * when the file is parsed.
		     */
		    if (!errorReported) {
			Parse_Error(PARSE_FATAL,
				     "Undefined variable \"%.*s\"",length,str);
		    }
		    str += length;
		    errorReported = TRUE;
		} else {
		    Buf_AddByte(buf, (Byte)*str);
		    str += 1;
		}
	    } else {
		/*
		 * We've now got a variable structure to store in. But first,
		 * advance the string pointer.
		 */
		str += length;

		/*
		 * Copy all the characters from the variable value straight
		 * into the new string.
		 */
		Buf_Append(buf, val);
		if (doFree) {
		    free(val);
		}
	    }
	} else {
	    /*
	     * Skip as many characters as possible -- either to the end of
	     * the string or to the next dollar sign (variable invocation).
	     */
	    const char *cp = str;

	    do {
		str++;
	    } while (str[0] != '$' && str[0] != '\0');

	    Buf_AppendRange(buf, cp, str);
	}
    }

    return (buf);
}

/*-
 *-----------------------------------------------------------------------
 * Var_GetTail --
 *	Return the tail from each of a list of words. Used to set the
 *	System V local variables.
 *
 * Results:
 *	The resulting string.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
char *
Var_GetTail(char *file)
{

    return (VarModify(file, VarTail, (void *)NULL));
}

/*-
 *-----------------------------------------------------------------------
 * Var_GetHead --
 *	Find the leading components of a (list of) filename(s).
 *	XXX: VarHead does not replace foo by ., as (sun) System V make
 *	does.
 *
 * Results:
 *	The leading components.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
char *
Var_GetHead(char *file)
{

    return (VarModify(file, VarHead, (void *)NULL));
}

/*-
 *-----------------------------------------------------------------------
 * Var_Init --
 *	Initialize the module
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The VAR_CMD and VAR_GLOBAL contexts are created
 *-----------------------------------------------------------------------
 */
void
Var_Init(void)
{

    VAR_GLOBAL = Targ_NewGN("Global");
    VAR_CMD = Targ_NewGN("Command");
}

/*-
 *-----------------------------------------------------------------------
 * Var_Dump --
 *	print all variables in a context
 *-----------------------------------------------------------------------
 */
void
Var_Dump(const GNode *ctxt)
{
	const LstNode	*ln;
	const Var	*v;

	LST_FOREACH(ln, &ctxt->context) {
		v = Lst_Datum(ln);
		printf("%-16s = %s\n", v->name,
		    (const char *)Buf_GetAll(v->val, NULL));
	}
}
