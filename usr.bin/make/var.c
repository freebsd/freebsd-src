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

static void VarDelete(void *);
static char *VarGetPattern(GNode *, int, char **, int, int *, size_t *,
			   VarPattern *);
static char *VarModify(char *,
		       Boolean (*)(const char *, Boolean, Buffer *, void *),
		       void *);
static int VarPrintVar(void *, void *);

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
 *
 * Side Effects:
 *	The caller must free the new contents or old contents of name.
 *-----------------------------------------------------------------------
 */
static char *
VarPossiblyExpand(const char *name, GNode *ctxt)
{
	char *tmp;
	char *result;

	/*
	 * XXX make a temporary copy of the name because Var_Subst insists
	 * on writing into the string.
	 */
	tmp = estrdup(name);
	if (strchr(name, '$') != NULL) {
		result = Var_Subst(NULL, tmp, ctxt, 0);
		free(tmp);
		return (result);
	} else
		return (tmp);
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
    Boolean		localCheckEnvFirst;
    LstNode         	*var;
    Var		  	*v;

	/*
	 * If the variable name begins with a '.', it could very well be one of
	 * the local ones.  We check the name against all the local variables
	 * and substitute the short version in for 'name' if it matches one of
	 * them.
	 */
	if (*name == '.' && isupper((unsigned char)name[1]))
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
		}

    /*
     * Note whether this is one of the specific variables we were told through
     * the -E flag to use environment-variable-override for.
     */
    if (Lst_Find(&envFirstVars, name, (CompareProc *)strcmp) != NULL) {
	localCheckEnvFirst = TRUE;
    } else {
	localCheckEnvFirst = FALSE;
    }

    /*
     * First look for the variable in the given context. If it's not there,
     * look for it in VAR_CMD, VAR_GLOBAL and the environment, in that order,
     * depending on the FIND_* flags in 'flags'
     */
    var = Lst_Find(&ctxt->context, name, VarCmp);

    if ((var == NULL) && (flags & FIND_CMD) && (ctxt != VAR_CMD)) {
	var = Lst_Find(&VAR_CMD->context, name, VarCmp);
    }
    if ((var == NULL) && (flags & FIND_GLOBAL) && (ctxt != VAR_GLOBAL) &&
	!checkEnvFirst && !localCheckEnvFirst)
    {
	var = Lst_Find(&VAR_GLOBAL->context, name, VarCmp);
    }
    if ((var == NULL) && (flags & FIND_ENV)) {
	char *env;

	if ((env = getenv(name)) != NULL) {
	    v = emalloc(sizeof(Var));
	    v->name = estrdup(name);
	    v->val = Buf_Init(0);
	    v->flags = VAR_FROM_ENV;

	    Buf_Append(v->val, env);

	    return (v);
	} else if ((checkEnvFirst || localCheckEnvFirst) &&
		   (flags & FIND_GLOBAL) && (ctxt != VAR_GLOBAL))
	{
	    var = Lst_Find(&VAR_GLOBAL->context, name, VarCmp);
	    if (var == NULL) {
		return (NULL);
	    } else {
		return (Lst_Datum(var));
	    }
	} else {
	    return (NULL);
	}
    } else if (var == NULL) {
	return (NULL);
    } else {
	return (Lst_Datum(var));
    }
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
    Var		  *v;

    v = emalloc(sizeof(Var));
    v->name = estrdup(name);
    v->val = Buf_Init(0);
    v->flags = 0;

    if (val != NULL) {
	Buf_Append(v->val, val);
    }

    Lst_AtFront(&ctxt->context, v);
    DEBUGF(VAR, ("%s:%s = %s\n", ctxt->name, name, val));
}

/*-
 *-----------------------------------------------------------------------
 * VarDelete  --
 *	Delete a variable and all the space associated with it.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static void
VarDelete(void *vp)
{
    Var *v = vp;

    free(v->name);
    Buf_Destroy(v->val, TRUE);
    free(v);
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
	VarDelete(Lst_Datum(ln));
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
	free(v->name);
	Buf_Destroy(v->val, TRUE);
	free(v);
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
	    Buf_Destroy(v->val, FALSE);
	    free(v->name);
	    free(v);
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
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarModify(char *str, Boolean (*modProc)(const char *, Boolean, Buffer *, void *),
    void *datum)
{
    Buffer  	  *buf;	    	    /* Buffer for the new string */
    Boolean 	  addSpace; 	    /* TRUE if need to add a space to the
				     * buffer before adding the trimmed
				     * word */
    char **av;			    /* word list [first word does not count] */
    int ac, i;

    buf = Buf_Init(0);
    addSpace = FALSE;

    av = brk_string(str, &ac, FALSE);

    for (i = 1; i < ac; i++)
	addSpace = (*modProc)(av[i], addSpace, buf, datum);

    Buf_AddByte(buf, '\0');
    str = (char *)Buf_GetAll(buf, (size_t *)NULL);
    Buf_Destroy(buf, FALSE);
    return (str);
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
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarSortWords(char *str, int (*cmp)(const void *, const void *))
{
	Buffer *buf;
	char **av;
	int ac, i;

	buf = Buf_Init(0);
	av = brk_string(str, &ac, FALSE);
	qsort(av + 1, ac - 1, sizeof(char *), cmp);
	for (i = 1; i < ac; i++) {
		Buf_Append(buf, av[i]);
		Buf_AddByte(buf, (Byte)((i < ac - 1) ? ' ' : '\0'));
	}
	str = (char *)Buf_GetAll(buf, (size_t *)NULL);
	Buf_Destroy(buf, FALSE);
	return (str);
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
VarGetPattern(GNode *ctxt, int err, char **tstr, int delim, int *flags,
    size_t *length, VarPattern *pattern)
{
    char *cp;
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
		    cp2 = Var_Parse(cp, ctxt, err, &len, &freeIt);
		    Buf_Append(buf, cp2);
		    if (freeIt)
			free(cp2);
		    cp += len - 1;
		} else {
		    char *cp2 = &cp[1];

		    if (*cp2 == '(' || *cp2 == '{') {
			/*
			 * Find the end of this variable reference
			 * and suck it in without further ado.
			 * It will be interperated later.
			 */
			int have = *cp2;
			int want = (*cp2 == '(') ? ')' : '}';
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
	*tstr = ++cp;
	cp = (char *)Buf_GetAll(buf, length);
	*length -= 1;	/* Don't count the NULL */
	Buf_Destroy(buf, FALSE);
	return (cp);
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

/*-
 *-----------------------------------------------------------------------
 * Var_Parse --
 *	Given the start of a variable invocation, extract the variable
 *	name and find its value, then modify it according to the
 *	specification.
 *
 * Results:
 *	The (possibly-modified) value of the variable or var_Error if the
 *	specification is invalid. The length of the specification is
 *	placed in *lengthPtr (for invalid specifications, this is just
 *	2 to skip the '$' and the following letter, or 1 if '$' was the
 *	last character in the string).
 *	A Boolean in *freePtr telling whether the returned string should
 *	be freed by the caller.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
char *
Var_Parse(char *str, GNode *ctxt, Boolean err, size_t *lengthPtr,
    Boolean *freePtr)
{
    char	    *tstr;    	/* Pointer into str */
    Var	    	    *v;	    	/* Variable in invocation */
    char	    *cp;    	/* Secondary pointer into str (place marker
				 * for tstr) */
    Boolean 	    haveModifier;/* TRUE if have modifiers for the variable */
    char	    endc;    	/* Ending character when variable in parens
				 * or braces */
    char	    startc=0;	/* Starting character when variable in parens
				 * or braces */
    int             cnt;	/* Used to count brace pairs when variable in
				 * in parens or braces */
    char    	    *start;
    char	     delim;
    Boolean 	    dynamic;	/* TRUE if the variable is local and we're
				 * expanding it in a non-local context. This
				 * is done to support dynamic sources. The
				 * result is just the invocation, unaltered */
    int		vlen;		/* length of variable name, after embedded variable
				 * expansion */

    *freePtr = FALSE;
    dynamic = FALSE;
    start = str;

    if (str[1] != '(' && str[1] != '{') {
	/*
	 * If it's not bounded by braces of some sort, life is much simpler.
	 * We just need to check for the first character and return the
	 * value if it exists.
	 */
	char	  name[2];

	name[0] = str[1];
	name[1] = '\0';

	v = VarFind(name, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	if (v == (Var *)NULL) {
	    if (str[1] != '\0')
		*lengthPtr = 2;
	    else
		*lengthPtr = 1;

	    if ((ctxt == VAR_CMD) || (ctxt == VAR_GLOBAL)) {
		/*
		 * If substituting a local variable in a non-local context,
		 * assume it's for dynamic source stuff. We have to handle
		 * this specially and return the longhand for the variable
		 * with the dollar sign escaped so it makes it back to the
		 * caller. Only four of the local variables are treated
		 * specially as they are the only four that will be set
		 * when dynamic sources are expanded.
		 */
		/* XXX: It looks like $% and $! are reversed here */
		switch (str[1]) {
		    case '@':
			return ("$(.TARGET)");
		    case '%':
			return ("$(.ARCHIVE)");
		    case '*':
			return ("$(.PREFIX)");
		    case '!':
			return ("$(.MEMBER)");
		    default:
			break;
		}
	    }
	    /*
	     * Error
	     */
	    return (err ? var_Error : varNoError);
	} else {
	    haveModifier = FALSE;
	    tstr = &str[1];
	    endc = str[1];
	}
    } else {
	/* build up expanded variable name in this buffer */
	Buffer	*buf = Buf_Init(MAKE_BSIZE);

	startc = str[1];
	endc = startc == '(' ? ')' : '}';

	/*
	 * Skip to the end character or a colon, whichever comes first,
	 * replacing embedded variables as we go.
	 */
	for (tstr = str + 2; *tstr != '\0' && *tstr != endc && *tstr != ':'; tstr++)
		if (*tstr == '$') {
			size_t rlen;
			Boolean	rfree;
			char*	rval = Var_Parse(tstr, ctxt, err, &rlen, &rfree);

			if (rval == var_Error) {
				Fatal("Error expanding embedded variable.");
			} else if (rval != NULL) {
				Buf_Append(buf, rval);
				if (rfree)
					free(rval);
			}
			tstr += rlen - 1;
		} else
			Buf_AddByte(buf, (Byte)*tstr);

	if (*tstr == '\0') {
	    /*
	     * If we never did find the end character, return NULL
	     * right now, setting the length to be the distance to
	     * the end of the string, since that's what make does.
	     */
	    *lengthPtr = tstr - str;
	    return (var_Error);
	}

	haveModifier = (*tstr == ':');
	*tstr = '\0';

	Buf_AddByte(buf, (Byte)'\0');
	str = Buf_GetAll(buf, (size_t *)NULL);
	vlen = strlen(str);

	v = VarFind(str, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	if ((v == (Var *)NULL) && (ctxt != VAR_CMD) && (ctxt != VAR_GLOBAL) &&
	    (vlen == 2) && (str[1] == 'F' || str[1] == 'D'))
	{
	    /*
	     * Check for bogus D and F forms of local variables since we're
	     * in a local context and the name is the right length.
	     */
	    switch (str[0]) {
		case '@':
		case '%':
		case '*':
		case '!':
		case '>':
		case '<':
		{
		    char    vname[2];
		    char    *val;

		    /*
		     * Well, it's local -- go look for it.
		     */
		    vname[0] = str[0];
		    vname[1] = '\0';
		    v = VarFind(vname, ctxt, 0);

		    if (v != NULL && !haveModifier) {
			/*
			 * No need for nested expansion or anything, as we're
			 * the only one who sets these things and we sure don't
			 * put nested invocations in them...
			 */
			val = (char *)Buf_GetAll(v->val, (size_t *)NULL);

			if (str[1] == 'D') {
			    val = VarModify(val, VarHead, (void *)NULL);
			} else {
			    val = VarModify(val, VarTail, (void *)NULL);
			}
			/*
			 * Resulting string is dynamically allocated, so
			 * tell caller to free it.
			 */
			*freePtr = TRUE;
			*lengthPtr = tstr-start+1;
			*tstr = endc;
			Buf_Destroy(buf, TRUE);
			return (val);
		    }
		    break;
		default:
		    break;
		}
	    }
	}

	if (v == (Var *)NULL) {
	    if (((vlen == 1) ||
		 (((vlen == 2) && (str[1] == 'F' || str[1] == 'D')))) &&
		((ctxt == VAR_CMD) || (ctxt == VAR_GLOBAL)))
	    {
		/*
		 * If substituting a local variable in a non-local context,
		 * assume it's for dynamic source stuff. We have to handle
		 * this specially and return the longhand for the variable
		 * with the dollar sign escaped so it makes it back to the
		 * caller. Only four of the local variables are treated
		 * specially as they are the only four that will be set
		 * when dynamic sources are expanded.
		 */
		switch (str[0]) {
		    case '@':
		    case '%':
		    case '*':
		    case '!':
			dynamic = TRUE;
			break;
		    default:
			break;
		}
	    } else if ((vlen > 2) && (str[0] == '.') &&
		       isupper((unsigned char)str[1]) &&
		       ((ctxt == VAR_CMD) || (ctxt == VAR_GLOBAL)))
	    {
		int	len;

		len = vlen - 1;
		if ((strncmp(str, ".TARGET", len) == 0) ||
		    (strncmp(str, ".ARCHIVE", len) == 0) ||
		    (strncmp(str, ".PREFIX", len) == 0) ||
		    (strncmp(str, ".MEMBER", len) == 0))
		{
		    dynamic = TRUE;
		}
	    }

	    if (!haveModifier) {
		/*
		 * No modifiers -- have specification length so we can return
		 * now.
		 */
		*lengthPtr = tstr - start + 1;
		*tstr = endc;
		if (dynamic) {
		    str = emalloc(*lengthPtr + 1);
		    strncpy(str, start, *lengthPtr);
		    str[*lengthPtr] = '\0';
		    *freePtr = TRUE;
		    Buf_Destroy(buf, TRUE);
		    return (str);
		} else {
		    Buf_Destroy(buf, TRUE);
		    return (err ? var_Error : varNoError);
		}
	    } else {
		/*
		 * Still need to get to the end of the variable specification,
		 * so kludge up a Var structure for the modifications
		 */
		v = emalloc(sizeof(Var));
		v->name = estrdup(str);
		v->val = Buf_Init(1);
		v->flags = VAR_JUNK;
	    }
	}
	Buf_Destroy(buf, TRUE);
    }

    if (v->flags & VAR_IN_USE) {
	Fatal("Variable %s is recursive.", v->name);
	/*NOTREACHED*/
    } else {
	v->flags |= VAR_IN_USE;
    }
    /*
     * Before doing any modification, we have to make sure the value
     * has been fully expanded. If it looks like recursion might be
     * necessary (there's a dollar sign somewhere in the variable's value)
     * we just call Var_Subst to do any other substitutions that are
     * necessary. Note that the value returned by Var_Subst will have
     * been dynamically-allocated, so it will need freeing when we
     * return.
     */
    str = (char *)Buf_GetAll(v->val, (size_t *)NULL);
    if (strchr(str, '$') != (char *)NULL) {
	str = Var_Subst(NULL, str, ctxt, err);
	*freePtr = TRUE;
    }

    v->flags &= ~VAR_IN_USE;

    /*
     * Now we need to apply any modifiers the user wants applied.
     * These are:
     *  	  :M<pattern>	words which match the given <pattern>.
     *  	  	    	<pattern> is of the standard file
     *  	  	    	wildcarding form.
     *  	  :S<d><pat1><d><pat2><d>[g]
     *  	  	    	Substitute <pat2> for <pat1> in the value
     *		  :C<d><pat1><d><pat2><d>[g]
     *				Substitute <pat2> for regex <pat1> in the value
     *  	  :H	    	Substitute the head of each word
     *  	  :T	    	Substitute the tail of each word
     *  	  :E	    	Substitute the extension (minus '.') of
     *  	  	    	each word
     *  	  :R	    	Substitute the root of each word
     *  	  	    	(pathname minus the suffix).
     *	    	  :lhs=rhs  	Like :S, but the rhs goes to the end of
     *	    	    	    	the invocation.
     *		  :U		Converts variable to upper-case.
     *		  :L		Converts variable to lower-case.
     */
    if ((str != NULL) && haveModifier) {
	/*
	 * Skip initial colon while putting it back.
	 */
	*tstr++ = ':';
	while (*tstr != endc) {
	    char	*newStr;    /* New value to return */
	    char	termc;	    /* Character which terminated scan */

	    DEBUGF(VAR, ("Applying :%c to \"%s\"\n", *tstr, str));
	    switch (*tstr) {
		case 'N':
		case 'M':
		{
		    char    *pattern;
		    char    *cp2;
		    Boolean copy;

		    copy = FALSE;
		    for (cp = tstr + 1;
			 *cp != '\0' && *cp != ':' && *cp != endc;
			 cp++)
		    {
			if (*cp == '\\' && (cp[1] == ':' || cp[1] == endc)) {
			    copy = TRUE;
			    cp++;
			}
		    }
		    termc = *cp;
		    *cp = '\0';
		    if (copy) {
			/*
			 * Need to compress the \:'s out of the pattern, so
			 * allocate enough room to hold the uncompressed
			 * pattern (note that cp started at tstr+1, so
			 * cp - tstr takes the null byte into account) and
			 * compress the pattern into the space.
			 */
			pattern = emalloc(cp - tstr);
			for (cp2 = pattern, cp = tstr + 1;
			     *cp != '\0';
			     cp++, cp2++)
			{
			    if ((*cp == '\\') &&
				(cp[1] == ':' || cp[1] == endc)) {
				    cp++;
			    }
			    *cp2 = *cp;
			}
			*cp2 = '\0';
		    } else {
			pattern = &tstr[1];
		    }
		    if (*tstr == 'M' || *tstr == 'm') {
			newStr = VarModify(str, VarMatch, pattern);
		    } else {
			newStr = VarModify(str, VarNoMatch, pattern);
		    }
		    if (copy) {
			free(pattern);
		    }
		    break;
		}
		case 'S':
		{
		    VarPattern 	    pattern;
		    char	    del;
		    Buffer  	    *buf;    	/* Buffer for patterns */

		    pattern.flags = 0;
		    del = tstr[1];
		    tstr += 2;

		    /*
		     * If pattern begins with '^', it is anchored to the
		     * start of the word -- skip over it and flag pattern.
		     */
		    if (*tstr == '^') {
			pattern.flags |= VAR_MATCH_START;
			tstr += 1;
		    }

		    buf = Buf_Init(0);

		    /*
		     * Pass through the lhs looking for 1) escaped delimiters,
		     * '$'s and backslashes (place the escaped character in
		     * uninterpreted) and 2) unescaped $'s that aren't before
		     * the delimiter (expand the variable substitution).
		     * The result is left in the Buffer buf.
		     */
		    for (cp = tstr; *cp != '\0' && *cp != del; cp++) {
			if ((*cp == '\\') &&
			    ((cp[1] == del) ||
			     (cp[1] == '$') ||
			     (cp[1] == '\\')))
			{
			    Buf_AddByte(buf, (Byte)cp[1]);
			    cp++;
			} else if (*cp == '$') {
			    if (cp[1] != del) {
				/*
				 * If unescaped dollar sign not before the
				 * delimiter, assume it's a variable
				 * substitution and recurse.
				 */
				char	    *cp2;
				size_t len;
				Boolean	    freeIt;

				cp2 = Var_Parse(cp, ctxt, err, &len, &freeIt);
				Buf_Append(buf, cp2);
				if (freeIt) {
				    free(cp2);
				}
				cp += len - 1;
			    } else {
				/*
				 * Unescaped $ at end of pattern => anchor
				 * pattern at end.
				 */
				pattern.flags |= VAR_MATCH_END;
			    }
			} else {
			    Buf_AddByte(buf, (Byte)*cp);
			}
		    }

		    Buf_AddByte(buf, (Byte)'\0');

		    /*
		     * If lhs didn't end with the delimiter, complain and
		     * exit.
		     */
		    if (*cp != del) {
			Fatal("Unclosed substitution for %s (%c missing)",
			      v->name, del);
		    }

		    /*
		     * Fetch pattern and destroy buffer, but preserve the data
		     * in it, since that's our lhs. Note that Buf_GetAll
		     * will return the actual number of bytes, which includes
		     * the null byte, so we have to decrement the length by
		     * one.
		     */
		    pattern.lhs = (char *)Buf_GetAll(buf, &pattern.leftLen);
		    pattern.leftLen--;
		    Buf_Destroy(buf, FALSE);

		    /*
		     * Now comes the replacement string. Three things need to
		     * be done here: 1) need to compress escaped delimiters and
		     * ampersands and 2) need to replace unescaped ampersands
		     * with the l.h.s. (since this isn't regexp, we can do
		     * it right here) and 3) expand any variable substitutions.
		     */
		    buf = Buf_Init(0);

		    tstr = cp + 1;
		    for (cp = tstr; *cp != '\0' && *cp != del; cp++) {
			if ((*cp == '\\') &&
			    ((cp[1] == del) ||
			     (cp[1] == '&') ||
			     (cp[1] == '\\') ||
			     (cp[1] == '$')))
			{
			    Buf_AddByte(buf, (Byte)cp[1]);
			    cp++;
			} else if ((*cp == '$') && (cp[1] != del)) {
			    char    *cp2;
			    size_t len;
			    Boolean freeIt;

			    cp2 = Var_Parse(cp, ctxt, err, &len, &freeIt);
			    Buf_Append(buf, cp2);
			    cp += len - 1;
			    if (freeIt) {
				free(cp2);
			    }
			} else if (*cp == '&') {
			    Buf_AddBytes(buf, pattern.leftLen,
					 (Byte *)pattern.lhs);
			} else {
			    Buf_AddByte(buf, (Byte)*cp);
			}
		    }

		    Buf_AddByte(buf, (Byte)'\0');

		    /*
		     * If didn't end in delimiter character, complain
		     */
		    if (*cp != del) {
			Fatal("Unclosed substitution for %s (%c missing)",
			      v->name, del);
		    }

		    pattern.rhs = (char *)Buf_GetAll(buf, &pattern.rightLen);
		    pattern.rightLen--;
		    Buf_Destroy(buf, FALSE);

		    /*
		     * Check for global substitution. If 'g' after the final
		     * delimiter, substitution is global and is marked that
		     * way.
		     */
		    cp++;
		    if (*cp == 'g') {
			pattern.flags |= VAR_SUB_GLOBAL;
			cp++;
		    }

		    /*
		     * Global substitution of the empty string causes an
		     * infinite number of matches, unless anchored by '^'
		     * (start of string) or '$' (end of string). Catch the
		     * infinite substitution here.
		     * Note that flags can only contain the 3 bits we're
		     * interested in so we don't have to mask unrelated
		     * bits. We can test for equality.
		     */
		    if (!pattern.leftLen && pattern.flags == VAR_SUB_GLOBAL)
			Fatal("Global substitution of the empty string");

		    termc = *cp;
		    newStr = VarModify(str, VarSubstitute, &pattern);
		    /*
		     * Free the two strings.
		     */
		    free(pattern.lhs);
		    free(pattern.rhs);
		    break;
		}
		case 'C':
		{
		    VarREPattern    pattern;
		    char	   *re;
		    int		    error;

		    pattern.flags = 0;
		    delim = tstr[1];
		    tstr += 2;

		    cp = tstr;

		    if ((re = VarGetPattern(ctxt, err, &cp, delim, NULL,
			NULL, NULL)) == NULL) {
			/* was: goto cleanup */
			*lengthPtr = cp - start + 1;
			if (*freePtr)
			    free(str);
			if (delim != '\0')
			    Fatal("Unclosed substitution for %s (%c missing)",
				  v->name, delim);
			return (var_Error);
		    }

		    if ((pattern.replace = VarGetPattern(ctxt, err, &cp,
			delim, NULL, NULL, NULL)) == NULL){
			free(re);

			/* was: goto cleanup */
			*lengthPtr = cp - start + 1;
			if (*freePtr)
			    free(str);
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
			*lengthPtr = cp - start + 1;
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
		    newStr = VarModify(str, VarRESubstitute, &pattern);
		    regfree(&pattern.re);
		    free(pattern.replace);
		    free(pattern.matches);
		    break;
		}
		case 'L':
		    if (tstr[1] == endc || tstr[1] == ':') {
			Buffer *buf;
			buf = Buf_Init(MAKE_BSIZE);
			for (cp = str; *cp ; cp++)
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
			newStr = VarSortWords(str, SortIncreasing);
			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /* FALLTHROUGH */
		case 'Q':
		    if (tstr[1] == endc || tstr[1] == ':') {
			newStr = Var_Quote(str);
			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /*FALLTHRU*/
		case 'T':
		    if (tstr[1] == endc || tstr[1] == ':') {
			newStr = VarModify(str, VarTail, (void *)NULL);
			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /*FALLTHRU*/
		case 'U':
		    if (tstr[1] == endc || tstr[1] == ':') {
			Buffer *buf;
			buf = Buf_Init(MAKE_BSIZE);
			for (cp = str; *cp ; cp++)
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
			newStr = VarModify(str, VarHead, (void *)NULL);
			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /*FALLTHRU*/
		case 'E':
		    if (tstr[1] == endc || tstr[1] == ':') {
			newStr = VarModify(str, VarSuffix, (void *)NULL);
			cp = tstr + 1;
			termc = *cp;
			break;
		    }
		    /*FALLTHRU*/
		case 'R':
		    if (tstr[1] == endc || tstr[1] == ':') {
			newStr = VarModify(str, VarRoot, (void *)NULL);
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

			buf = Cmd_Exec(str, &error);
			newStr = Buf_GetAll(buf, NULL);
			Buf_Destroy(buf, FALSE);

			if (error)
			    Error(error, str);
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
		    VarPattern      pattern;
		    Boolean         eqFound;

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
				*lengthPtr = cp - start + 1;
				if (*freePtr)
				    free(str);
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
				*lengthPtr = cp - start + 1;
				if (*freePtr)
				    free(str);
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
			newStr = VarModify(str, VarSYSVMatch, &pattern);

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
		free(str);
	    }
	    str = newStr;
	    if (str != var_Error) {
		*freePtr = TRUE;
	    } else {
		*freePtr = FALSE;
	    }
	    if (termc == '\0') {
		Error("Unclosed variable specification for %s", v->name);
	    } else if (termc == ':') {
		*cp++ = termc;
	    } else {
		*cp = termc;
	    }
	    tstr = cp;
	}
	*lengthPtr = tstr - start + 1;
    } else {
	*lengthPtr = tstr - start + 1;
	*tstr = endc;
    }

    if (v->flags & VAR_FROM_ENV) {
	Boolean	  destroy = FALSE;

	if (str != (char *)Buf_GetAll(v->val, (size_t *)NULL)) {
	    destroy = TRUE;
	} else {
	    /*
	     * Returning the value unmodified, so tell the caller to free
	     * the thing.
	     */
	    *freePtr = TRUE;
	}
	free(v->name);
	Buf_Destroy(v->val, destroy);
	free(v);
    } else if (v->flags & VAR_JUNK) {
	/*
	 * Perform any free'ing needed and set *freePtr to FALSE so the caller
	 * doesn't try to free a static pointer.
	 */
	if (*freePtr) {
	    free(str);
	}
	*freePtr = FALSE;
	free(v->name);
	Buf_Destroy(v->val, TRUE);
	free(v);
	if (dynamic) {
	    str = emalloc(*lengthPtr + 1);
	    strncpy(str, start, *lengthPtr);
	    str[*lengthPtr] = '\0';
	    *freePtr = TRUE;
	} else {
	    str = err ? var_Error : varNoError;
	}
    }
    return (str);
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
char *
Var_Subst(const char *var, char *str, GNode *ctxt, Boolean undefErr)
{
    Buffer  	  *buf;	    	    /* Buffer for forming things */
    char    	  *val;		    /* Value to substitute for a variable */
    size_t	  length;   	    /* Length of the variable invocation */
    Boolean 	  doFree;   	    /* Set true if val should be freed */
    char	*result;
    static Boolean errorReported;   /* Set true if an error has already
				     * been reported to prevent a plethora
				     * of messages when recursing */

    buf = Buf_Init(MAKE_BSIZE);
    errorReported = FALSE;

    while (*str) {
	if (var == NULL && (*str == '$') && (str[1] == '$')) {
	    /*
	     * A dollar sign may be escaped either with another dollar sign.
	     * In such a case, we skip over the escape character and store the
	     * dollar sign into the buffer directly.
	     */
	    str++;
	    Buf_AddByte(buf, (Byte)*str);
	    str++;
	} else if (*str != '$') {
	    /*
	     * Skip as many characters as possible -- either to the end of
	     * the string or to the next dollar sign (variable invocation).
	     */
	    const char *cp;

	    for (cp = str++; *str != '$' && *str != '\0'; str++)
		continue;
	    Buf_AppendRange(buf, cp, str);
	} else {
	    if (var != NULL) {
		int expand;
		for (;;) {
		    if (str[1] != '(' && str[1] != '{') {
			if (str[1] != *var || var[1] != '\0') {
			    Buf_AddBytes(buf, 2, (const Byte *)str);
			    str += 2;
			    expand = FALSE;
			} else
			    expand = TRUE;
			break;
		    } else {
			char *p;

			/*
			 * Scan up to the end of the variable name.
			 */
			for (p = &str[2]; *p &&
			     *p != ':' && *p != ')' && *p != '}'; p++)
			    if (*p == '$')
				break;
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

			if (strncmp(var, str + 2, p - str - 2) != 0 ||
			    var[p - str - 2] != '\0') {
			    /*
			     * Not the variable we want to expand, scan
			     * until the next variable
			     */
			    for (;*p != '$' && *p != '\0'; p++)
				continue;
			    Buf_AppendRange(buf, str, p);
			    str = p;
			    expand = FALSE;
			}
			else
			    expand = TRUE;
			break;
		    }
		}
		if (!expand)
		    continue;
	    }

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
	}
    }

    Buf_AddByte(buf, '\0');
    result = (char *)Buf_GetAll(buf, (size_t *)NULL);
    Buf_Destroy(buf, FALSE);
    return (result);
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

/****************** PRINT DEBUGGING INFO *****************/
static int
VarPrintVar(void *vp, void *dummy __unused)
{
    Var    *v = (Var *) vp;

    printf("%-16s = %s\n", v->name, (char *)Buf_GetAll(v->val, (size_t *)NULL));
    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Dump --
 *	print all variables in a context
 *-----------------------------------------------------------------------
 */
void
Var_Dump(GNode *ctxt)
{

    Lst_ForEach(&ctxt->context, VarPrintVar, (void *)NULL);
}
