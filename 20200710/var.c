/*	$NetBSD: var.c,v 1.255 2020/07/04 17:41:04 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. Neither the name of the University nor the names of its contributors
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

/*
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
 */

#ifndef MAKE_NATIVE
static char rcsid[] = "$NetBSD: var.c,v 1.255 2020/07/04 17:41:04 rillig Exp $";
#else
#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)var.c	8.3 (Berkeley) 3/19/94";
#else
__RCSID("$NetBSD: var.c,v 1.255 2020/07/04 17:41:04 rillig Exp $");
#endif
#endif /* not lint */
#endif

/*-
 * var.c --
 *	Variable-handling functions
 *
 * Interface:
 *	Var_Set		    Set the value of a variable in the given
 *			    context. The variable is created if it doesn't
 *			    yet exist. The value and variable name need not
 *			    be preserved.
 *
 *	Var_Append	    Append more characters to an existing variable
 *			    in the given context. The variable needn't
 *			    exist already -- it will be created if it doesn't.
 *			    A space is placed between the old value and the
 *			    new one.
 *
 *	Var_Exists	    See if a variable exists.
 *
 *	Var_Value 	    Return the value of a variable in a context or
 *			    NULL if the variable is undefined.
 *
 *	Var_Subst 	    Substitute either a single variable or all
 *			    variables in a string, using the given context as
 *			    the top-most one.
 *
 *	Var_Parse 	    Parse a variable expansion from a string and
 *			    return the result and the number of characters
 *			    consumed.
 *
 *	Var_Delete	    Delete a variable in a context.
 *
 *	Var_Init  	    Initialize this module.
 *
 * Debugging:
 *	Var_Dump  	    Print out all variables defined in the given
 *			    context.
 *
 * XXX: There's a lot of duplication in these functions.
 */

#include    <sys/stat.h>
#include    <sys/types.h>
#ifndef NO_REGEX
#include    <regex.h>
#endif
#include    <ctype.h>
#include    <stdlib.h>
#include    <limits.h>
#include    <time.h>

#include    "make.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include    "buf.h"
#include    "dir.h"
#include    "job.h"
#include    "metachar.h"

extern int makelevel;
/*
 * This lets us tell if we have replaced the original environ
 * (which we cannot free).
 */
char **savedEnv = NULL;

/*
 * This is a harmless return value for Var_Parse that can be used by Var_Subst
 * to determine if there was an error in parsing -- easier than returning
 * a flag, as things outside this module don't give a hoot.
 */
char var_Error[] = "";

/*
 * Similar to var_Error, but returned when the 'VARF_UNDEFERR' flag for
 * Var_Parse is not set. Why not just use a constant? Well, gcc likes
 * to condense identical string instances...
 */
static char varNoError[] = "";

/*
 * Traditionally we consume $$ during := like any other expansion.
 * Other make's do not.
 * This knob allows controlling the behavior.
 * FALSE for old behavior.
 * TRUE for new compatible.
 */
#define SAVE_DOLLARS ".MAKE.SAVE_DOLLARS"
static Boolean save_dollars = FALSE;

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
GNode          *VAR_INTERNAL;	/* variables from make itself */
GNode          *VAR_GLOBAL;	/* variables from the makefile */
GNode          *VAR_CMD;	/* variables defined on the command-line */

#define FIND_CMD	0x1	/* look in VAR_CMD when searching */
#define FIND_GLOBAL	0x2	/* look in VAR_GLOBAL as well */
#define FIND_ENV  	0x4	/* look in the environment also */

typedef enum {
    VAR_IN_USE		= 0x01,	/* Variable's value is currently being used.
				 * Used to avoid endless recursion */
    VAR_FROM_ENV	= 0x02,	/* Variable comes from the environment */
    VAR_JUNK		= 0x04,	/* Variable is a junk variable that
				 * should be destroyed when done with
				 * it. Used by Var_Parse for undefined,
				 * modified variables */
    VAR_KEEP		= 0x08,	/* Variable is VAR_JUNK, but we found
				 * a use for it in some modifier and
				 * the value is therefore valid */
    VAR_EXPORTED	= 0x10,	/* Variable is exported */
    VAR_REEXPORT	= 0x20,	/* Indicate if var needs re-export.
				 * This would be true if it contains $'s */
    VAR_FROM_CMD	= 0x40	/* Variable came from command line */
} Var_Flags;

typedef struct Var {
    char          *name;	/* the variable's name */
    Buffer	  val;		/* its value */
    Var_Flags	  flags;    	/* miscellaneous status flags */
}  Var;

/*
 * Exporting vars is expensive so skip it if we can
 */
#define VAR_EXPORTED_NONE	0
#define VAR_EXPORTED_YES	1
#define VAR_EXPORTED_ALL	2
static int var_exportedVars = VAR_EXPORTED_NONE;
/*
 * We pass this to Var_Export when doing the initial export
 * or after updating an exported var.
 */
#define VAR_EXPORT_PARENT	1
/*
 * We pass this to Var_Export1 to tell it to leave the value alone.
 */
#define VAR_EXPORT_LITERAL	2

typedef enum {
	VAR_SUB_GLOBAL	= 0x01,	/* Apply substitution globally */
	VAR_SUB_ONE	= 0x02,	/* Apply substitution to one word */
	VAR_SUB_MATCHED	= 0x04,	/* There was a match */
	VAR_MATCH_START	= 0x08,	/* Match at start of word */
	VAR_MATCH_END	= 0x10,	/* Match at end of word */
	VAR_NOSUBST	= 0x20	/* don't expand vars in VarGetPattern */
} VarPattern_Flags;

typedef enum {
	VAR_NO_EXPORT	= 0x01	/* do not export */
} VarSet_Flags;

typedef struct {
    /*
     * The following fields are set by Var_Parse() when it
     * encounters modifiers that need to keep state for use by
     * subsequent modifiers within the same variable expansion.
     */
    Byte	varSpace;	/* Word separator in expansions */
    Boolean	oneBigWord;	/* TRUE if we will treat the variable as a
				 * single big word, even if it contains
				 * embedded spaces (as opposed to the
				 * usual behaviour of treating it as
				 * several space-separated words). */
} Var_Parse_State;

/* struct passed as 'void *' to VarSubstitute() for ":S/lhs/rhs/",
 * to VarSYSVMatch() for ":lhs=rhs". */
typedef struct {
    const char   *lhs;		/* String to match */
    int		  leftLen;	/* Length of string */
    const char   *rhs;		/* Replacement string (w/ &'s removed) */
    int		  rightLen;	/* Length of replacement */
    VarPattern_Flags flags;
} VarPattern;

/* struct passed as 'void *' to VarLoopExpand() for ":@tvar@str@" */
typedef struct {
    GNode	*ctxt;		/* variable context */
    char	*tvar;		/* name of temp var */
    int		tvarLen;
    char	*str;		/* string to expand */
    int		strLen;
    Varf_Flags	flags;
} VarLoop;

#ifndef NO_REGEX
/* struct passed as 'void *' to VarRESubstitute() for ":C///" */
typedef struct {
    regex_t	   re;
    int		   nsub;
    regmatch_t 	  *matches;
    char 	  *replace;
    int		   flags;
} VarREPattern;
#endif

/* struct passed to VarSelectWords() for ":[start..end]" */
typedef struct {
    int		start;		/* first word to select */
    int		end;		/* last word to select */
} VarSelectWords_t;

#define BROPEN	'{'
#define BRCLOSE	'}'
#define PROPEN	'('
#define PRCLOSE	')'

/*-
 *-----------------------------------------------------------------------
 * VarFind --
 *	Find the given variable in the given context and any other contexts
 *	indicated.
 *
 * Input:
 *	name		name to find
 *	ctxt		context in which to find it
 *	flags		FIND_GLOBAL set means to look in the
 *			VAR_GLOBAL context as well. FIND_CMD set means
 *			to look in the VAR_CMD context also. FIND_ENV
 *			set means to look in the environment
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
    Hash_Entry         	*var;
    Var			*v;

    /*
     * If the variable name begins with a '.', it could very well be one of
     * the local ones.  We check the name against all the local variables
     * and substitute the short version in for 'name' if it matches one of
     * them.
     */
    if (*name == '.' && isupper((unsigned char) name[1])) {
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
    }

#ifdef notyet
    /* for compatibility with gmake */
    if (name[0] == '^' && name[1] == '\0')
	name = ALLSRC;
#endif

    /*
     * First look for the variable in the given context. If it's not there,
     * look for it in VAR_CMD, VAR_GLOBAL and the environment, in that order,
     * depending on the FIND_* flags in 'flags'
     */
    var = Hash_FindEntry(&ctxt->context, name);

    if (var == NULL && (flags & FIND_CMD) && ctxt != VAR_CMD) {
	var = Hash_FindEntry(&VAR_CMD->context, name);
    }
    if (!checkEnvFirst && var == NULL && (flags & FIND_GLOBAL) &&
	ctxt != VAR_GLOBAL)
    {
	var = Hash_FindEntry(&VAR_GLOBAL->context, name);
	if (var == NULL && ctxt != VAR_INTERNAL) {
	    /* VAR_INTERNAL is subordinate to VAR_GLOBAL */
	    var = Hash_FindEntry(&VAR_INTERNAL->context, name);
	}
    }
    if (var == NULL && (flags & FIND_ENV)) {
	char *env;

	if ((env = getenv(name)) != NULL) {
	    int		len;

	    v = bmake_malloc(sizeof(Var));
	    v->name = bmake_strdup(name);

	    len = strlen(env);

	    Buf_Init(&v->val, len + 1);
	    Buf_AddBytes(&v->val, len, env);

	    v->flags = VAR_FROM_ENV;
	    return v;
	} else if (checkEnvFirst && (flags & FIND_GLOBAL) &&
		   ctxt != VAR_GLOBAL)
	{
	    var = Hash_FindEntry(&VAR_GLOBAL->context, name);
	    if (var == NULL && ctxt != VAR_INTERNAL) {
		var = Hash_FindEntry(&VAR_INTERNAL->context, name);
	    }
	    if (var == NULL) {
		return NULL;
	    } else {
		return (Var *)Hash_GetValue(var);
	    }
	} else {
	    return NULL;
	}
    } else if (var == NULL) {
	return NULL;
    } else {
	return (Var *)Hash_GetValue(var);
    }
}

/*-
 *-----------------------------------------------------------------------
 * VarFreeEnv  --
 *	If the variable is an environment variable, free it
 *
 * Input:
 *	v		the variable
 *	destroy		true if the value buffer should be destroyed.
 *
 * Results:
 *	1 if it is an environment variable 0 ow.
 *
 * Side Effects:
 *	The variable is free'ed if it is an environent variable.
 *-----------------------------------------------------------------------
 */
static Boolean
VarFreeEnv(Var *v, Boolean destroy)
{
    if ((v->flags & VAR_FROM_ENV) == 0)
	return FALSE;
    free(v->name);
    Buf_Destroy(&v->val, destroy);
    free(v);
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * VarAdd  --
 *	Add a new variable of name name and value val to the given context
 *
 * Input:
 *	name		name of variable to add
 *	val		value to set it to
 *	ctxt		context in which to set it
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
    Var   	  *v;
    int		  len;
    Hash_Entry    *h;

    v = bmake_malloc(sizeof(Var));

    len = val ? strlen(val) : 0;
    Buf_Init(&v->val, len + 1);
    Buf_AddBytes(&v->val, len, val);

    v->flags = 0;

    h = Hash_CreateEntry(&ctxt->context, name, NULL);
    Hash_SetValue(h, v);
    v->name = h->name;
    if (DEBUG(VAR) && (ctxt->flags & INTERNAL) == 0) {
	fprintf(debug_file, "%s:%s = %s\n", ctxt->name, name, val);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Var_Delete --
 *	Remove a variable from a context.
 *
 * Side Effects:
 *	The Var structure is removed and freed.
 *
 *-----------------------------------------------------------------------
 */
void
Var_Delete(const char *name, GNode *ctxt)
{
    Hash_Entry 	  *ln;
    char *cp;

    if (strchr(name, '$')) {
	cp = Var_Subst(NULL, name, VAR_GLOBAL, VARF_WANTRES);
    } else {
	cp = (char *)name;
    }
    ln = Hash_FindEntry(&ctxt->context, cp);
    if (DEBUG(VAR)) {
	fprintf(debug_file, "%s:delete %s%s\n",
	    ctxt->name, cp, ln ? "" : " (not found)");
    }
    if (cp != name) {
	free(cp);
    }
    if (ln != NULL) {
	Var 	  *v;

	v = (Var *)Hash_GetValue(ln);
	if ((v->flags & VAR_EXPORTED)) {
	    unsetenv(v->name);
	}
	if (strcmp(MAKE_EXPORTED, v->name) == 0) {
	    var_exportedVars = VAR_EXPORTED_NONE;
	}
	if (v->name != ln->name)
	    free(v->name);
	Hash_DeleteEntry(&ctxt->context, ln);
	Buf_Destroy(&v->val, TRUE);
	free(v);
    }
}


/*
 * Export a var.
 * We ignore make internal variables (those which start with '.')
 * Also we jump through some hoops to avoid calling setenv
 * more than necessary since it can leak.
 * We only manipulate flags of vars if 'parent' is set.
 */
static int
Var_Export1(const char *name, int flags)
{
    char tmp[BUFSIZ];
    Var *v;
    char *val = NULL;
    int n;
    int parent = (flags & VAR_EXPORT_PARENT);

    if (*name == '.')
	return 0;		/* skip internals */
    if (!name[1]) {
	/*
	 * A single char.
	 * If it is one of the vars that should only appear in
	 * local context, skip it, else we can get Var_Subst
	 * into a loop.
	 */
	switch (name[0]) {
	case '@':
	case '%':
	case '*':
	case '!':
	    return 0;
	}
    }
    v = VarFind(name, VAR_GLOBAL, 0);
    if (v == NULL) {
	return 0;
    }
    if (!parent &&
	(v->flags & (VAR_EXPORTED|VAR_REEXPORT)) == VAR_EXPORTED) {
	return 0;			/* nothing to do */
    }
    val = Buf_GetAll(&v->val, NULL);
    if ((flags & VAR_EXPORT_LITERAL) == 0 && strchr(val, '$')) {
	if (parent) {
	    /*
	     * Flag this as something we need to re-export.
	     * No point actually exporting it now though,
	     * the child can do it at the last minute.
	     */
	    v->flags |= (VAR_EXPORTED|VAR_REEXPORT);
	    return 1;
	}
	if (v->flags & VAR_IN_USE) {
	    /*
	     * We recursed while exporting in a child.
	     * This isn't going to end well, just skip it.
	     */
	    return 0;
	}
	n = snprintf(tmp, sizeof(tmp), "${%s}", name);
	if (n < (int)sizeof(tmp)) {
	    val = Var_Subst(NULL, tmp, VAR_GLOBAL, VARF_WANTRES);
	    setenv(name, val, 1);
	    free(val);
	}
    } else {
	if (parent) {
	    v->flags &= ~VAR_REEXPORT;	/* once will do */
	}
	if (parent || !(v->flags & VAR_EXPORTED)) {
	    setenv(name, val, 1);
	}
    }
    /*
     * This is so Var_Set knows to call Var_Export again...
     */
    if (parent) {
	v->flags |= VAR_EXPORTED;
    }
    return 1;
}

static void
Var_ExportVars_callback(void *entry, void *unused MAKE_ATTR_UNUSED)
{
    Var *var = entry;
    Var_Export1(var->name, 0);
}

/*
 * This gets called from our children.
 */
void
Var_ExportVars(void)
{
    char tmp[BUFSIZ];
    char *val;
    int n;

    /*
     * Several make's support this sort of mechanism for tracking
     * recursion - but each uses a different name.
     * We allow the makefiles to update MAKELEVEL and ensure
     * children see a correctly incremented value.
     */
    snprintf(tmp, sizeof(tmp), "%d", makelevel + 1);
    setenv(MAKE_LEVEL_ENV, tmp, 1);

    if (VAR_EXPORTED_NONE == var_exportedVars)
	return;

    if (VAR_EXPORTED_ALL == var_exportedVars) {
	/* Ouch! This is crazy... */
	Hash_ForEach(&VAR_GLOBAL->context, Var_ExportVars_callback, NULL);
	return;
    }
    /*
     * We have a number of exported vars,
     */
    n = snprintf(tmp, sizeof(tmp), "${" MAKE_EXPORTED ":O:u}");
    if (n < (int)sizeof(tmp)) {
	char **av;
	char *as;
	int ac;
	int i;

	val = Var_Subst(NULL, tmp, VAR_GLOBAL, VARF_WANTRES);
	if (*val) {
	    av = brk_string(val, &ac, FALSE, &as);
	    for (i = 0; i < ac; i++) {
		Var_Export1(av[i], 0);
	    }
	    free(as);
	    free(av);
	}
	free(val);
    }
}

/*
 * This is called when .export is seen or
 * .MAKE.EXPORTED is modified.
 * It is also called when any exported var is modified.
 */
void
Var_Export(char *str, int isExport)
{
    char *name;
    char *val;
    char **av;
    char *as;
    int flags;
    int ac;
    int i;

    if (isExport && (!str || !str[0])) {
	var_exportedVars = VAR_EXPORTED_ALL; /* use with caution! */
	return;
    }

    flags = 0;
    if (strncmp(str, "-env", 4) == 0) {
	str += 4;
    } else if (strncmp(str, "-literal", 8) == 0) {
	str += 8;
	flags |= VAR_EXPORT_LITERAL;
    } else {
	flags |= VAR_EXPORT_PARENT;
    }
    val = Var_Subst(NULL, str, VAR_GLOBAL, VARF_WANTRES);
    if (*val) {
	av = brk_string(val, &ac, FALSE, &as);
	for (i = 0; i < ac; i++) {
	    name = av[i];
	    if (!name[1]) {
		/*
		 * A single char.
		 * If it is one of the vars that should only appear in
		 * local context, skip it, else we can get Var_Subst
		 * into a loop.
		 */
		switch (name[0]) {
		case '@':
		case '%':
		case '*':
		case '!':
		    continue;
		}
	    }
	    if (Var_Export1(name, flags)) {
		if (VAR_EXPORTED_ALL != var_exportedVars)
		    var_exportedVars = VAR_EXPORTED_YES;
		if (isExport && (flags & VAR_EXPORT_PARENT)) {
		    Var_Append(MAKE_EXPORTED, name, VAR_GLOBAL);
		}
	    }
	}
	free(as);
	free(av);
    }
    free(val);
}


/*
 * This is called when .unexport[-env] is seen.
 */
extern char **environ;

void
Var_UnExport(char *str)
{
    char tmp[BUFSIZ];
    char *vlist;
    char *cp;
    Boolean unexport_env;
    int n;

    if (!str || !str[0]) {
	return;			/* assert? */
    }

    vlist = NULL;

    str += 8;
    unexport_env = (strncmp(str, "-env", 4) == 0);
    if (unexport_env) {
	char **newenv;

	cp = getenv(MAKE_LEVEL_ENV);	/* we should preserve this */
	if (environ == savedEnv) {
	    /* we have been here before! */
	    newenv = bmake_realloc(environ, 2 * sizeof(char *));
	} else {
	    if (savedEnv) {
		free(savedEnv);
		savedEnv = NULL;
	    }
	    newenv = bmake_malloc(2 * sizeof(char *));
	}
	if (!newenv)
	    return;
	/* Note: we cannot safely free() the original environ. */
	environ = savedEnv = newenv;
	newenv[0] = NULL;
	newenv[1] = NULL;
	if (cp && *cp)
	    setenv(MAKE_LEVEL_ENV, cp, 1);
    } else {
	for (; *str != '\n' && isspace((unsigned char) *str); str++)
	    continue;
	if (str[0] && str[0] != '\n') {
	    vlist = str;
	}
    }

    if (!vlist) {
	/* Using .MAKE.EXPORTED */
	n = snprintf(tmp, sizeof(tmp), "${" MAKE_EXPORTED ":O:u}");
	if (n < (int)sizeof(tmp)) {
	    vlist = Var_Subst(NULL, tmp, VAR_GLOBAL, VARF_WANTRES);
	}
    }
    if (vlist) {
	Var *v;
	char **av;
	char *as;
	int ac;
	int i;

	av = brk_string(vlist, &ac, FALSE, &as);
	for (i = 0; i < ac; i++) {
	    v = VarFind(av[i], VAR_GLOBAL, 0);
	    if (!v)
		continue;
	    if (!unexport_env &&
		(v->flags & (VAR_EXPORTED|VAR_REEXPORT)) == VAR_EXPORTED) {
		unsetenv(v->name);
	    }
	    v->flags &= ~(VAR_EXPORTED|VAR_REEXPORT);
	    /*
	     * If we are unexporting a list,
	     * remove each one from .MAKE.EXPORTED.
	     * If we are removing them all,
	     * just delete .MAKE.EXPORTED below.
	     */
	    if (vlist == str) {
		n = snprintf(tmp, sizeof(tmp),
			     "${" MAKE_EXPORTED ":N%s}", v->name);
		if (n < (int)sizeof(tmp)) {
		    cp = Var_Subst(NULL, tmp, VAR_GLOBAL, VARF_WANTRES);
		    Var_Set(MAKE_EXPORTED, cp, VAR_GLOBAL);
		    free(cp);
		}
	    }
	}
	free(as);
	free(av);
	if (vlist != str) {
	    Var_Delete(MAKE_EXPORTED, VAR_GLOBAL);
	    free(vlist);
	}
    }
}

static void
Var_Set_with_flags(const char *name, const char *val, GNode *ctxt,
		   VarSet_Flags flags)
{
    Var *v;
    char *expanded_name = NULL;

    /*
     * We only look for a variable in the given context since anything set
     * here will override anything in a lower context, so there's not much
     * point in searching them all just to save a bit of memory...
     */
    if (strchr(name, '$') != NULL) {
	expanded_name = Var_Subst(NULL, name, ctxt, VARF_WANTRES);
	if (expanded_name[0] == 0) {
	    if (DEBUG(VAR)) {
		fprintf(debug_file, "Var_Set(\"%s\", \"%s\", ...) "
			"name expands to empty string - ignored\n",
			name, val);
	    }
	    free(expanded_name);
	    return;
	}
	name = expanded_name;
    }
    if (ctxt == VAR_GLOBAL) {
	v = VarFind(name, VAR_CMD, 0);
	if (v != NULL) {
	    if ((v->flags & VAR_FROM_CMD)) {
		if (DEBUG(VAR)) {
		    fprintf(debug_file, "%s:%s = %s ignored!\n", ctxt->name, name, val);
		}
		goto out;
	    }
	    VarFreeEnv(v, TRUE);
	}
    }
    v = VarFind(name, ctxt, 0);
    if (v == NULL) {
	if (ctxt == VAR_CMD && (flags & VAR_NO_EXPORT) == 0) {
	    /*
	     * This var would normally prevent the same name being added
	     * to VAR_GLOBAL, so delete it from there if needed.
	     * Otherwise -V name may show the wrong value.
	     */
	    Var_Delete(name, VAR_GLOBAL);
	}
	VarAdd(name, val, ctxt);
    } else {
	Buf_Empty(&v->val);
	if (val)
	    Buf_AddBytes(&v->val, strlen(val), val);

	if (DEBUG(VAR)) {
	    fprintf(debug_file, "%s:%s = %s\n", ctxt->name, name, val);
	}
	if ((v->flags & VAR_EXPORTED)) {
	    Var_Export1(name, VAR_EXPORT_PARENT);
	}
    }
    /*
     * Any variables given on the command line are automatically exported
     * to the environment (as per POSIX standard)
     */
    if (ctxt == VAR_CMD && (flags & VAR_NO_EXPORT) == 0) {
	if (v == NULL) {
	    /* we just added it */
	    v = VarFind(name, ctxt, 0);
	}
	if (v != NULL)
	    v->flags |= VAR_FROM_CMD;
	/*
	 * If requested, don't export these in the environment
	 * individually.  We still put them in MAKEOVERRIDES so
	 * that the command-line settings continue to override
	 * Makefile settings.
	 */
	if (varNoExportEnv != TRUE)
	    setenv(name, val ? val : "", 1);

	Var_Append(MAKEOVERRIDES, name, VAR_GLOBAL);
    }
    if (*name == '.') {
	if (strcmp(name, SAVE_DOLLARS) == 0)
	    save_dollars = s2Boolean(val, save_dollars);
    }

out:
    free(expanded_name);
    if (v != NULL)
	VarFreeEnv(v, TRUE);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Set --
 *	Set the variable name to the value val in the given context.
 *
 * Input:
 *	name		name of variable to set
 *	val		value to give to the variable
 *	ctxt		context in which to set it
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
 *	If the context is VAR_GLOBAL though, we check if the variable
 *	was set in VAR_CMD from the command line and skip it if so.
 *-----------------------------------------------------------------------
 */
void
Var_Set(const char *name, const char *val, GNode *ctxt)
{
    Var_Set_with_flags(name, val, ctxt, 0);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Append --
 *	The variable of the given name has the given value appended to it in
 *	the given context.
 *
 * Input:
 *	name		name of variable to modify
 *	val		String to append to it
 *	ctxt		Context in which this should occur
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
    Var *v;
    Hash_Entry *h;
    char *expanded_name = NULL;

    if (strchr(name, '$') != NULL) {
	expanded_name = Var_Subst(NULL, name, ctxt, VARF_WANTRES);
	if (expanded_name[0] == 0) {
	    if (DEBUG(VAR)) {
		fprintf(debug_file, "Var_Append(\"%s\", \"%s\", ...) "
			"name expands to empty string - ignored\n",
			name, val);
	    }
	    free(expanded_name);
	    return;
	}
	name = expanded_name;
    }

    v = VarFind(name, ctxt, ctxt == VAR_GLOBAL ? (FIND_CMD|FIND_ENV) : 0);

    if (v == NULL) {
	Var_Set(name, val, ctxt);
    } else if (ctxt == VAR_CMD || !(v->flags & VAR_FROM_CMD)) {
	Buf_AddByte(&v->val, ' ');
	Buf_AddBytes(&v->val, strlen(val), val);

	if (DEBUG(VAR)) {
	    fprintf(debug_file, "%s:%s = %s\n", ctxt->name, name,
		    Buf_GetAll(&v->val, NULL));
	}

	if (v->flags & VAR_FROM_ENV) {
	    /*
	     * If the original variable came from the environment, we
	     * have to install it in the global context (we could place
	     * it in the environment, but then we should provide a way to
	     * export other variables...)
	     */
	    v->flags &= ~VAR_FROM_ENV;
	    h = Hash_CreateEntry(&ctxt->context, name, NULL);
	    Hash_SetValue(h, v);
	}
    }
    free(expanded_name);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Exists --
 *	See if the given variable exists.
 *
 * Input:
 *	name		Variable to find
 *	ctxt		Context in which to start search
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
    Var		  *v;
    char          *cp;

    if ((cp = strchr(name, '$')) != NULL) {
	cp = Var_Subst(NULL, name, ctxt, VARF_WANTRES);
    }
    v = VarFind(cp ? cp : name, ctxt, FIND_CMD|FIND_GLOBAL|FIND_ENV);
    free(cp);
    if (v == NULL) {
	return FALSE;
    }

    (void)VarFreeEnv(v, TRUE);
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * Var_Value --
 *	Return the value of the named variable in the given context
 *
 * Input:
 *	name		name to find
 *	ctxt		context in which to search for it
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
    Var *v;

    v = VarFind(name, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
    *frp = NULL;
    if (v == NULL)
	return NULL;

    char *p = (Buf_GetAll(&v->val, NULL));
    if (VarFreeEnv(v, FALSE))
	*frp = p;
    return p;
}


/* This callback for VarModify gets a single word from an expression and
 * typically adds a modification of this word to the buffer. It may also do
 * nothing or add several words.
 *
 * If addSpaces is TRUE, it must add a space before adding anything else to
 * the buffer.
 *
 * It returns the addSpace value for the next call of this callback. Typical
 * return values are the current addSpaces or TRUE. */
typedef Boolean (*VarModifyCallback)(GNode *ctxt, Var_Parse_State *vpstate,
    const char *word, Boolean addSpace, Buffer *buf, void *data);


/* Callback function for VarModify to implement the :H modifier.
 * Add the dirname of the given word to the buffer. */
static Boolean
VarHead(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
	const char *word, Boolean addSpace, Buffer *buf,
	void *dummy MAKE_ATTR_UNUSED)
{
    const char *slash = strrchr(word, '/');

    if (addSpace && vpstate->varSpace)
	Buf_AddByte(buf, vpstate->varSpace);
    if (slash != NULL)
	Buf_AddBytes(buf, slash - word, word);
    else
	Buf_AddByte(buf, '.');

    return TRUE;
}

/* Callback function for VarModify to implement the :T modifier.
 * Add the basename of the given word to the buffer. */
static Boolean
VarTail(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
	const char *word, Boolean addSpace, Buffer *buf,
	void *dummy MAKE_ATTR_UNUSED)
{
    const char *slash = strrchr(word, '/');
    const char *base = slash != NULL ? slash + 1 : word;

    if (addSpace && vpstate->varSpace)
	Buf_AddByte(buf, vpstate->varSpace);
    Buf_AddBytes(buf, strlen(base), base);
    return TRUE;
}

/* Callback function for VarModify to implement the :E modifier.
 * Add the filename suffix of the given word to the buffer, if it exists. */
static Boolean
VarSuffix(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
	  const char *word, Boolean addSpace, Buffer *buf,
	  void *dummy MAKE_ATTR_UNUSED)
{
    const char *dot = strrchr(word, '.');
    if (dot == NULL)
	return addSpace;

    if (addSpace && vpstate->varSpace)
	Buf_AddByte(buf, vpstate->varSpace);
    Buf_AddBytes(buf, strlen(dot + 1), dot + 1);
    return TRUE;
}

/* Callback function for VarModify to implement the :R modifier.
 * Add the filename basename of the given word to the buffer. */
static Boolean
VarRoot(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
	const char *word, Boolean addSpace, Buffer *buf,
	void *dummy MAKE_ATTR_UNUSED)
{
    char *dot = strrchr(word, '.');
    size_t len = dot != NULL ? (size_t)(dot - word) : strlen(word);

    if (addSpace && vpstate->varSpace)
	Buf_AddByte(buf, vpstate->varSpace);
    Buf_AddBytes(buf, len, word);
    return TRUE;
}

/* Callback function for VarModify to implement the :M modifier.
 * Place the word in the buffer if it matches the given pattern. */
static Boolean
VarMatch(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
	 const char *word, Boolean addSpace, Buffer *buf,
	 void *data)
{
    const char *pattern = data;
    if (DEBUG(VAR))
	fprintf(debug_file, "VarMatch [%s] [%s]\n", word, pattern);
    if (!Str_Match(word, pattern))
	return addSpace;
    if (addSpace && vpstate->varSpace)
	Buf_AddByte(buf, vpstate->varSpace);
    Buf_AddBytes(buf, strlen(word), word);
    return TRUE;
}

#ifdef SYSVVARSUB
/* Callback function for VarModify to implement the :%.from=%.to modifier. */
static Boolean
VarSYSVMatch(GNode *ctx, Var_Parse_State *vpstate,
	     const char *word, Boolean addSpace, Buffer *buf,
	     void *data)
{
    size_t len;
    char *ptr;
    Boolean hasPercent;
    VarPattern *pat = data;

    if (addSpace && vpstate->varSpace)
	Buf_AddByte(buf, vpstate->varSpace);

    if ((ptr = Str_SYSVMatch(word, pat->lhs, &len, &hasPercent)) != NULL) {
	char *varexp = Var_Subst(NULL, pat->rhs, ctx, VARF_WANTRES);
	Str_SYSVSubst(buf, varexp, ptr, len, hasPercent);
	free(varexp);
    } else {
	Buf_AddBytes(buf, strlen(word), word);
    }

    return TRUE;
}
#endif

/* Callback function for VarModify to implement the :N modifier.
 * Place the word in the buffer if it doesn't match the given pattern. */
static Boolean
VarNoMatch(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
	   const char *word, Boolean addSpace, Buffer *buf,
	   void *data)
{
    const char *pattern = data;
    if (Str_Match(word, pattern))
	return addSpace;
    if (addSpace && vpstate->varSpace)
	Buf_AddByte(buf, vpstate->varSpace);
    Buf_AddBytes(buf, strlen(word), word);
    return TRUE;
}

/* Callback function for VarModify to implement the :S,from,to, modifier.
 * Perform a string substitution on the given word. */
static Boolean
VarSubstitute(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
	      const char *word, Boolean addSpace, Buffer *buf,
	      void *data)
{
    int wordLen = strlen(word);
    const char *cp;		/* General pointer */
    VarPattern *pattern = data;

    if ((pattern->flags & (VAR_SUB_ONE|VAR_SUB_MATCHED)) !=
	(VAR_SUB_ONE|VAR_SUB_MATCHED)) {
	/*
	 * Still substituting -- break it down into simple anchored cases
	 * and if none of them fits, perform the general substitution case.
	 */
	if ((pattern->flags & VAR_MATCH_START) &&
	    (strncmp(word, pattern->lhs, pattern->leftLen) == 0)) {
	    /*
	     * Anchored at start and beginning of word matches pattern
	     */
	    if ((pattern->flags & VAR_MATCH_END) &&
	        (wordLen == pattern->leftLen)) {
		/*
		 * Also anchored at end and matches to the end (word
		 * is same length as pattern) add space and rhs only
		 * if rhs is non-null.
		 */
		if (pattern->rightLen != 0) {
		    if (addSpace && vpstate->varSpace) {
			Buf_AddByte(buf, vpstate->varSpace);
		    }
		    addSpace = TRUE;
		    Buf_AddBytes(buf, pattern->rightLen, pattern->rhs);
		}
		pattern->flags |= VAR_SUB_MATCHED;
	    } else if (pattern->flags & VAR_MATCH_END) {
		/*
		 * Doesn't match to end -- copy word wholesale
		 */
		goto nosub;
	    } else {
		/*
		 * Matches at start but need to copy in trailing characters
		 */
		if ((pattern->rightLen + wordLen - pattern->leftLen) != 0) {
		    if (addSpace && vpstate->varSpace) {
			Buf_AddByte(buf, vpstate->varSpace);
		    }
		    addSpace = TRUE;
		}
		Buf_AddBytes(buf, pattern->rightLen, pattern->rhs);
		Buf_AddBytes(buf, wordLen - pattern->leftLen,
			     (word + pattern->leftLen));
		pattern->flags |= VAR_SUB_MATCHED;
	    }
	} else if (pattern->flags & VAR_MATCH_START) {
	    /*
	     * Had to match at start of word and didn't -- copy whole word.
	     */
	    goto nosub;
	} else if (pattern->flags & VAR_MATCH_END) {
	    /*
	     * Anchored at end, Find only place match could occur (leftLen
	     * characters from the end of the word) and see if it does. Note
	     * that because the $ will be left at the end of the lhs, we have
	     * to use strncmp.
	     */
	    cp = word + (wordLen - pattern->leftLen);
	    if ((cp >= word) &&
		(strncmp(cp, pattern->lhs, pattern->leftLen) == 0)) {
		/*
		 * Match found. If we will place characters in the buffer,
		 * add a space before hand as indicated by addSpace, then
		 * stuff in the initial, unmatched part of the word followed
		 * by the right-hand-side.
		 */
		if (((cp - word) + pattern->rightLen) != 0) {
		    if (addSpace && vpstate->varSpace) {
			Buf_AddByte(buf, vpstate->varSpace);
		    }
		    addSpace = TRUE;
		}
		Buf_AddBytes(buf, cp - word, word);
		Buf_AddBytes(buf, pattern->rightLen, pattern->rhs);
		pattern->flags |= VAR_SUB_MATCHED;
	    } else {
		/*
		 * Had to match at end and didn't. Copy entire word.
		 */
		goto nosub;
	    }
	} else {
	    /*
	     * Pattern is unanchored: search for the pattern in the word using
	     * String_FindSubstring, copying unmatched portions and the
	     * right-hand-side for each match found, handling non-global
	     * substitutions correctly, etc. When the loop is done, any
	     * remaining part of the word (word and wordLen are adjusted
	     * accordingly through the loop) is copied straight into the
	     * buffer.
	     * addSpace is set FALSE as soon as a space is added to the
	     * buffer.
	     */
	    Boolean done;
	    int origSize;

	    done = FALSE;
	    origSize = Buf_Size(buf);
	    while (!done) {
		cp = Str_FindSubstring(word, pattern->lhs);
		if (cp != NULL) {
		    if (addSpace && (((cp - word) + pattern->rightLen) != 0)) {
			Buf_AddByte(buf, vpstate->varSpace);
			addSpace = FALSE;
		    }
		    Buf_AddBytes(buf, cp - word, word);
		    Buf_AddBytes(buf, pattern->rightLen, pattern->rhs);
		    wordLen -= (cp - word) + pattern->leftLen;
		    word = cp + pattern->leftLen;
		    if (wordLen == 0) {
			done = TRUE;
		    }
		    if ((pattern->flags & VAR_SUB_GLOBAL) == 0) {
			done = TRUE;
		    }
		    pattern->flags |= VAR_SUB_MATCHED;
		} else {
		    done = TRUE;
		}
	    }
	    if (wordLen != 0) {
		if (addSpace && vpstate->varSpace) {
		    Buf_AddByte(buf, vpstate->varSpace);
		}
		Buf_AddBytes(buf, wordLen, word);
	    }
	    /*
	     * If added characters to the buffer, need to add a space
	     * before we add any more. If we didn't add any, just return
	     * the previous value of addSpace.
	     */
	    return (Buf_Size(buf) != origSize) || addSpace;
	}
	return addSpace;
    }
nosub:
    if (addSpace && vpstate->varSpace) {
	Buf_AddByte(buf, vpstate->varSpace);
    }
    Buf_AddBytes(buf, wordLen, word);
    return TRUE;
}

#ifndef NO_REGEX
/*-
 *-----------------------------------------------------------------------
 * VarREError --
 *	Print the error caused by a regcomp or regexec call.
 *
 * Side Effects:
 *	An error gets printed.
 *
 *-----------------------------------------------------------------------
 */
static void
VarREError(int reerr, regex_t *pat, const char *str)
{
    char *errbuf;
    int errlen;

    errlen = regerror(reerr, pat, 0, 0);
    errbuf = bmake_malloc(errlen);
    regerror(reerr, pat, errbuf, errlen);
    Error("%s: %s", str, errbuf);
    free(errbuf);
}

/* Callback function for VarModify to implement the :C/from/to/ modifier.
 * Perform a regex substitution on the given word. */
static Boolean
VarRESubstitute(GNode *ctx MAKE_ATTR_UNUSED,
		Var_Parse_State *vpstate MAKE_ATTR_UNUSED,
		const char *word, Boolean addSpace, Buffer *buf,
		void *data)
{
    VarREPattern *pat = data;
    int xrv;
    const char *wp = word;
    char *rp;
    int added = 0;
    int flags = 0;

#define MAYBE_ADD_SPACE()		\
	if (addSpace && !added)		\
	    Buf_AddByte(buf, ' ');	\
	added = 1

    if ((pat->flags & (VAR_SUB_ONE|VAR_SUB_MATCHED)) ==
	(VAR_SUB_ONE|VAR_SUB_MATCHED))
	xrv = REG_NOMATCH;
    else {
    tryagain:
	xrv = regexec(&pat->re, wp, pat->nsub, pat->matches, flags);
    }

    switch (xrv) {
    case 0:
	pat->flags |= VAR_SUB_MATCHED;
	if (pat->matches[0].rm_so > 0) {
	    MAYBE_ADD_SPACE();
	    Buf_AddBytes(buf, pat->matches[0].rm_so, wp);
	}

	for (rp = pat->replace; *rp; rp++) {
	    if ((*rp == '\\') && ((rp[1] == '&') || (rp[1] == '\\'))) {
		MAYBE_ADD_SPACE();
		Buf_AddByte(buf, rp[1]);
		rp++;
	    } else if ((*rp == '&') ||
		((*rp == '\\') && isdigit((unsigned char)rp[1]))) {
		int n;
		const char *subbuf;
		int sublen;
		char errstr[3];

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
		    Error("No subexpression %s", &errstr[0]);
		    subbuf = "";
		    sublen = 0;
		} else if ((pat->matches[n].rm_so == -1) &&
			   (pat->matches[n].rm_eo == -1)) {
		    Error("No match for subexpression %s", &errstr[0]);
		    subbuf = "";
		    sublen = 0;
		} else {
		    subbuf = wp + pat->matches[n].rm_so;
		    sublen = pat->matches[n].rm_eo - pat->matches[n].rm_so;
		}

		if (sublen > 0) {
		    MAYBE_ADD_SPACE();
		    Buf_AddBytes(buf, sublen, subbuf);
		}
	    } else {
		MAYBE_ADD_SPACE();
		Buf_AddByte(buf, *rp);
	    }
	}
	wp += pat->matches[0].rm_eo;
	if (pat->flags & VAR_SUB_GLOBAL) {
	    flags |= REG_NOTBOL;
	    if (pat->matches[0].rm_so == 0 && pat->matches[0].rm_eo == 0) {
		MAYBE_ADD_SPACE();
		Buf_AddByte(buf, *wp);
		wp++;

	    }
	    if (*wp)
		goto tryagain;
	}
	if (*wp) {
	    MAYBE_ADD_SPACE();
	    Buf_AddBytes(buf, strlen(wp), wp);
	}
	break;
    default:
	VarREError(xrv, &pat->re, "Unexpected regex error");
	/* fall through */
    case REG_NOMATCH:
	if (*wp) {
	    MAYBE_ADD_SPACE();
	    Buf_AddBytes(buf, strlen(wp), wp);
	}
	break;
    }
    return addSpace || added;
}
#endif


/* Callback function for VarModify to implement the :@var@...@ modifier of
 * ODE make. We set the temp variable named in pattern.lhs to word and
 * expand pattern.rhs. */
static Boolean
VarLoopExpand(GNode *ctx MAKE_ATTR_UNUSED,
	      Var_Parse_State *vpstate MAKE_ATTR_UNUSED,
	      const char *word, Boolean addSpace, Buffer *buf,
	      void *data)
{
    VarLoop *loop = data;
    char *s;
    int slen;

    if (*word) {
	Var_Set_with_flags(loop->tvar, word, loop->ctxt, VAR_NO_EXPORT);
	s = Var_Subst(NULL, loop->str, loop->ctxt, loop->flags);
	if (DEBUG(VAR)) {
	    fprintf(debug_file,
		    "VarLoopExpand: in \"%s\", replace \"%s\" with \"%s\" "
		    "to \"%s\"\n",
		    word, loop->tvar, loop->str, s ? s : "(null)");
	}
	if (s != NULL && *s != '\0') {
	    if (addSpace && *s != '\n')
		Buf_AddByte(buf, ' ');
	    Buf_AddBytes(buf, (slen = strlen(s)), s);
	    addSpace = (slen > 0 && s[slen - 1] != '\n');
	}
	free(s);
    }
    return addSpace;
}


/*-
 *-----------------------------------------------------------------------
 * VarSelectWords --
 *	Implements the :[start..end] modifier.
 *	This is a special case of VarModify since we want to be able
 *	to scan the list backwards if start > end.
 *
 * Input:
 *	str		String whose words should be trimmed
 *	seldata		words to select
 *
 * Results:
 *	A string of all the words selected.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarSelectWords(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
	       const char *str, VarSelectWords_t *seldata)
{
    Buffer buf;			/* Buffer for the new string */
    Boolean addSpace;		/* TRUE if need to add a space to the
				 * buffer before adding the trimmed
				 * word */
    char **av;			/* word list */
    char *as;			/* word list memory */
    int ac, i;
    int start, end, step;

    Buf_Init(&buf, 0);
    addSpace = FALSE;

    if (vpstate->oneBigWord) {
	/* fake what brk_string() would do if there were only one word */
	ac = 1;
	av = bmake_malloc((ac + 1) * sizeof(char *));
	as = bmake_strdup(str);
	av[0] = as;
	av[1] = NULL;
    } else {
	av = brk_string(str, &ac, FALSE, &as);
    }

    /*
     * Now sanitize seldata.
     * If seldata->start or seldata->end are negative, convert them to
     * the positive equivalents (-1 gets converted to argc, -2 gets
     * converted to (argc-1), etc.).
     */
    if (seldata->start < 0)
	seldata->start = ac + seldata->start + 1;
    if (seldata->end < 0)
	seldata->end = ac + seldata->end + 1;

    /*
     * We avoid scanning more of the list than we need to.
     */
    if (seldata->start > seldata->end) {
	start = MIN(ac, seldata->start) - 1;
	end = MAX(0, seldata->end - 1);
	step = -1;
    } else {
	start = MAX(0, seldata->start - 1);
	end = MIN(ac, seldata->end);
	step = 1;
    }

    for (i = start;
	 (step < 0 && i >= end) || (step > 0 && i < end);
	 i += step) {
	if (av[i] && *av[i]) {
	    if (addSpace && vpstate->varSpace) {
		Buf_AddByte(&buf, vpstate->varSpace);
	    }
	    Buf_AddBytes(&buf, strlen(av[i]), av[i]);
	    addSpace = TRUE;
	}
    }

    free(as);
    free(av);

    return Buf_Destroy(&buf, FALSE);
}


/* Callback function for VarModify to implement the :tA modifier.
 * Replace each word with the result of realpath() if successful. */
static Boolean
VarRealpath(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
	    const char *word, Boolean addSpace, Buffer *buf,
	    void *patternp MAKE_ATTR_UNUSED)
{
    struct stat st;
    char rbuf[MAXPATHLEN];
    char *rp;

    if (addSpace && vpstate->varSpace)
	Buf_AddByte(buf, vpstate->varSpace);
    rp = cached_realpath(word, rbuf);
    if (rp && *rp == '/' && stat(rp, &st) == 0)
	word = rp;

    Buf_AddBytes(buf, strlen(word), word);
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * Modify each of the words of the passed string using the given function.
 *
 * Input:
 *	str		String whose words should be trimmed
 *	modProc		Function to use to modify them
 *	data		Custom data for the modProc
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
VarModify(GNode *ctx, Var_Parse_State *vpstate,
    const char *str, VarModifyCallback modProc, void *datum)
{
    Buffer buf;			/* Buffer for the new string */
    Boolean addSpace; 		/* TRUE if need to add a space to the
				 * buffer before adding the trimmed word */
    char **av;			/* word list */
    char *as;			/* word list memory */
    int ac, i;

    Buf_Init(&buf, 0);
    addSpace = FALSE;

    if (vpstate->oneBigWord) {
	/* fake what brk_string() would do if there were only one word */
	ac = 1;
	av = bmake_malloc((ac + 1) * sizeof(char *));
	as = bmake_strdup(str);
	av[0] = as;
	av[1] = NULL;
    } else {
	av = brk_string(str, &ac, FALSE, &as);
    }

    if (DEBUG(VAR)) {
	fprintf(debug_file, "VarModify: split \"%s\" into %d words\n",
		str, ac);
    }

    for (i = 0; i < ac; i++)
	addSpace = modProc(ctx, vpstate, av[i], addSpace, &buf, datum);

    free(as);
    free(av);

    return Buf_Destroy(&buf, FALSE);
}


static int
VarWordCompare(const void *a, const void *b)
{
    int r = strcmp(*(const char * const *)a, *(const char * const *)b);
    return r;
}

static int
VarWordCompareReverse(const void *a, const void *b)
{
    int r = strcmp(*(const char * const *)b, *(const char * const *)a);
    return r;
}

/*-
 *-----------------------------------------------------------------------
 * VarOrder --
 *	Order the words in the string.
 *
 * Input:
 *	str		String whose words should be sorted.
 *	otype		How to order: s - sort, x - random.
 *
 * Results:
 *	A string containing the words ordered.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarOrder(const char *str, const char otype)
{
    Buffer buf;			/* Buffer for the new string */
    char **av;			/* word list [first word does not count] */
    char *as;			/* word list memory */
    int ac, i;

    Buf_Init(&buf, 0);

    av = brk_string(str, &ac, FALSE, &as);

    if (ac > 0) {
	switch (otype) {
	case 'r':		/* reverse sort alphabetically */
	    qsort(av, ac, sizeof(char *), VarWordCompareReverse);
	    break;
	case 's':		/* sort alphabetically */
	    qsort(av, ac, sizeof(char *), VarWordCompare);
	    break;
	case 'x':		/* randomize */
	    {
		/*
		 * We will use [ac..2] range for mod factors. This will produce
		 * random numbers in [(ac-1)..0] interval, and minimal
		 * reasonable value for mod factor is 2 (the mod 1 will produce
		 * 0 with probability 1).
		 */
		for (i = ac - 1; i > 0; i--) {
		    int rndidx = random() % (i + 1);
		    char *t = av[i];
		    av[i] = av[rndidx];
		    av[rndidx] = t;
		}
	    }
	}
    }

    for (i = 0; i < ac; i++) {
	Buf_AddBytes(&buf, strlen(av[i]), av[i]);
	if (i != ac - 1)
	    Buf_AddByte(&buf, ' ');
    }

    free(as);
    free(av);

    return Buf_Destroy(&buf, FALSE);
}


/*-
 *-----------------------------------------------------------------------
 * VarUniq --
 *	Remove adjacent duplicate words.
 *
 * Input:
 *	str		String whose words should be sorted
 *
 * Results:
 *	A string containing the resulting words.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarUniq(const char *str)
{
    Buffer	  buf;		/* Buffer for new string */
    char 	**av;		/* List of words to affect */
    char 	 *as;		/* Word list memory */
    int 	  ac, i, j;

    Buf_Init(&buf, 0);
    av = brk_string(str, &ac, FALSE, &as);

    if (ac > 1) {
	for (j = 0, i = 1; i < ac; i++)
	    if (strcmp(av[i], av[j]) != 0 && (++j != i))
		av[j] = av[i];
	ac = j + 1;
    }

    for (i = 0; i < ac; i++) {
	Buf_AddBytes(&buf, strlen(av[i]), av[i]);
	if (i != ac - 1)
	    Buf_AddByte(&buf, ' ');
    }

    free(as);
    free(av);

    return Buf_Destroy(&buf, FALSE);
}

/*-
 *-----------------------------------------------------------------------
 * VarRange --
 *	Return an integer sequence
 *
 * Input:
 *	str		String whose words provide default range
 *	ac		range length, if 0 use str words
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarRange(const char *str, int ac)
{
    Buffer	  buf;		/* Buffer for new string */
    char	  tmp[32];	/* each element */
    char 	**av;		/* List of words to affect */
    char 	 *as;		/* Word list memory */
    int 	  i, n;

    Buf_Init(&buf, 0);
    if (ac > 0) {
	as = NULL;
	av = NULL;
    } else {
	av = brk_string(str, &ac, FALSE, &as);
    }
    for (i = 0; i < ac; i++) {
	n = snprintf(tmp, sizeof(tmp), "%d", 1 + i);
	if (n >= (int)sizeof(tmp))
	    break;
	Buf_AddBytes(&buf, n, tmp);
	if (i != ac - 1)
	    Buf_AddByte(&buf, ' ');
    }

    free(as);
    free(av);

    return Buf_Destroy(&buf, FALSE);
}


/*-
 *-----------------------------------------------------------------------
 * VarGetPattern --
 *	During the parsing of a part of a modifier such as :S or :@,
 *	pass through the tstr looking for 1) escaped delimiters,
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
VarGetPattern(GNode *ctxt, Var_Parse_State *vpstate MAKE_ATTR_UNUSED,
	      VarPattern_Flags flags, const char **tstr, int delim,
	      VarPattern_Flags *vflags, int *length, VarPattern *pattern)
{
    const char *cp;
    char *rstr;
    Buffer buf;
    int junk;
    int errnum = flags & VARF_UNDEFERR;

    Buf_Init(&buf, 0);
    if (length == NULL)
	length = &junk;

#define IS_A_MATCH(cp, delim) \
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
	    Buf_AddByte(&buf, cp[1]);
	    cp++;
	} else if (*cp == '$') {
	    if (cp[1] == delim) {
		if (vflags == NULL)
		    Buf_AddByte(&buf, *cp);
		else
		    /*
		     * Unescaped $ at end of pattern => anchor
		     * pattern at end.
		     */
		    *vflags |= VAR_MATCH_END;
	    } else {
		if (vflags == NULL || (*vflags & VAR_NOSUBST) == 0) {
		    char   *cp2;
		    int     len;
		    void   *freeIt;

		    /*
		     * If unescaped dollar sign not before the
		     * delimiter, assume it's a variable
		     * substitution and recurse.
		     */
		    cp2 = Var_Parse(cp, ctxt, errnum | (flags & VARF_WANTRES),
				    &len, &freeIt);
		    Buf_AddBytes(&buf, strlen(cp2), cp2);
		    free(freeIt);
		    cp += len - 1;
		} else {
		    const char *cp2 = &cp[1];

		    if (*cp2 == PROPEN || *cp2 == BROPEN) {
			/*
			 * Find the end of this variable reference
			 * and suck it in without further ado.
			 * It will be interpreted later.
			 */
			int have = *cp2;
			int want = (*cp2 == PROPEN) ? PRCLOSE : BRCLOSE;
			int depth = 1;

			for (++cp2; *cp2 != '\0' && depth > 0; ++cp2) {
			    if (cp2[-1] != '\\') {
				if (*cp2 == have)
				    ++depth;
				if (*cp2 == want)
				    --depth;
			    }
			}
			Buf_AddBytes(&buf, cp2 - cp, cp);
			cp = --cp2;
		    } else
			Buf_AddByte(&buf, *cp);
		}
	    }
	} else if (pattern && *cp == '&')
	    Buf_AddBytes(&buf, pattern->leftLen, pattern->lhs);
	else
	    Buf_AddByte(&buf, *cp);
    }

    if (*cp != delim) {
	*tstr = cp;
	*length = 0;
	return NULL;
    }

    *tstr = ++cp;
    *length = Buf_Size(&buf);
    rstr = Buf_Destroy(&buf, FALSE);
    if (DEBUG(VAR))
	fprintf(debug_file, "Modifier pattern: \"%s\"\n", rstr);
    return rstr;
}

/*-
 *-----------------------------------------------------------------------
 * VarQuote --
 *	Quote shell meta-characters and space characters in the string
 *	if quoteDollar is set, also quote and double any '$' characters.
 *
 * Results:
 *	The quoted string
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarQuote(char *str, Boolean quoteDollar)
{

    Buffer  	  buf;
    const char	*newline;
    size_t nlen;

    if ((newline = Shell_GetNewline()) == NULL)
	newline = "\\\n";
    nlen = strlen(newline);

    Buf_Init(&buf, 0);

    for (; *str != '\0'; str++) {
	if (*str == '\n') {
	    Buf_AddBytes(&buf, nlen, newline);
	    continue;
	}
	if (isspace((unsigned char)*str) || ismeta((unsigned char)*str))
	    Buf_AddByte(&buf, '\\');
	Buf_AddByte(&buf, *str);
	if (quoteDollar && *str == '$')
	    Buf_AddBytes(&buf, 2, "\\$");
    }

    str = Buf_Destroy(&buf, FALSE);
    if (DEBUG(VAR))
	fprintf(debug_file, "QuoteMeta: [%s]\n", str);
    return str;
}

/*-
 *-----------------------------------------------------------------------
 * VarHash --
 *      Hash the string using the MurmurHash3 algorithm.
 *      Output is computed using 32bit Little Endian arithmetic.
 *
 * Input:
 *	str		String to modify
 *
 * Results:
 *      Hash value of str, encoded as 8 hex digits.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarHash(const char *str)
{
    static const char    hexdigits[16] = "0123456789abcdef";
    Buffer         buf;
    size_t         len, len2;
    const unsigned char *ustr = (const unsigned char *)str;
    uint32_t       h, k, c1, c2;

    h  = 0x971e137bU;
    c1 = 0x95543787U;
    c2 = 0x2ad7eb25U;
    len2 = strlen(str);

    for (len = len2; len; ) {
	k = 0;
	switch (len) {
	default:
	    k = ((uint32_t)ustr[3] << 24) |
		((uint32_t)ustr[2] << 16) |
		((uint32_t)ustr[1] << 8) |
		(uint32_t)ustr[0];
	    len -= 4;
	    ustr += 4;
	    break;
	case 3:
	    k |= (uint32_t)ustr[2] << 16;
	    /* FALLTHROUGH */
	case 2:
	    k |= (uint32_t)ustr[1] << 8;
	    /* FALLTHROUGH */
	case 1:
	    k |= (uint32_t)ustr[0];
	    len = 0;
	}
	c1 = c1 * 5 + 0x7b7d159cU;
	c2 = c2 * 5 + 0x6bce6396U;
	k *= c1;
	k = (k << 11) ^ (k >> 21);
	k *= c2;
	h = (h << 13) ^ (h >> 19);
	h = h * 5 + 0x52dce729U;
	h ^= k;
    }
    h ^= len2;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    Buf_Init(&buf, 0);
    for (len = 0; len < 8; ++len) {
	Buf_AddByte(&buf, hexdigits[h & 15]);
	h >>= 4;
    }

    return Buf_Destroy(&buf, FALSE);
}

static char *
VarStrftime(const char *fmt, int zulu, time_t utc)
{
    char buf[BUFSIZ];

    if (!utc)
	time(&utc);
    if (!*fmt)
	fmt = "%c";
    strftime(buf, sizeof(buf), fmt, zulu ? gmtime(&utc) : localtime(&utc));

    buf[sizeof(buf) - 1] = '\0';
    return bmake_strdup(buf);
}

typedef struct {
    /* const parameters */
    int startc;
    int endc;
    Var *v;
    GNode *ctxt;
    int flags;
    int *lengthPtr;
    void **freePtr;

    /* read-write */
    char *nstr;
    const char *tstr;
    const char *start;
    const char *cp;		/* Secondary pointer into str (place marker
				 * for tstr) */
    char termc;			/* Character which terminated scan */
    int cnt;			/* Used to count brace pairs when variable in
				 * in parens or braces */
    char delim;
    int modifier;		/* that we are processing */
    Var_Parse_State parsestate;	/* Flags passed to helper functions */

    /* result */
    char *newStr;		/* New value to return */
} ApplyModifiersState;

/* we now have some modifiers with long names */
#define STRMOD_MATCH(s, want, n) \
    (strncmp(s, want, n) == 0 && (s[n] == st->endc || s[n] == ':'))
#define STRMOD_MATCHX(s, want, n) \
    (strncmp(s, want, n) == 0 && \
     (s[n] == st->endc || s[n] == ':' || s[n] == '='))
#define CHARMOD_MATCH(c) (c == st->endc || c == ':')

/* :@var@...${var}...@ */
static Boolean
ApplyModifier_At(ApplyModifiersState *st) {
    VarLoop loop;
    VarPattern_Flags vflags = VAR_NOSUBST;

    st->cp = ++(st->tstr);
    st->delim = '@';
    loop.tvar = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	&vflags, &loop.tvarLen, NULL);
    if (loop.tvar == NULL)
	return FALSE;

    loop.str = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	&vflags, &loop.strLen, NULL);
    if (loop.str == NULL)
	return FALSE;

    st->termc = *st->cp;
    st->delim = '\0';

    loop.flags = st->flags & (VARF_UNDEFERR | VARF_WANTRES);
    loop.ctxt = st->ctxt;
    st->newStr = VarModify(
	st->ctxt, &st->parsestate, st->nstr, VarLoopExpand, &loop);
    Var_Delete(loop.tvar, st->ctxt);
    free(loop.tvar);
    free(loop.str);
    return TRUE;
}

/* :Ddefined or :Uundefined */
static void
ApplyModifier_Defined(ApplyModifiersState *st)
{
    Buffer buf;			/* Buffer for patterns */
    int nflags;

    if (st->flags & VARF_WANTRES) {
	int wantres;
	if (*st->tstr == 'U')
	    wantres = ((st->v->flags & VAR_JUNK) != 0);
	else
	    wantres = ((st->v->flags & VAR_JUNK) == 0);
	nflags = st->flags & ~VARF_WANTRES;
	if (wantres)
	    nflags |= VARF_WANTRES;
    } else
	nflags = st->flags;

    /*
     * Pass through tstr looking for 1) escaped delimiters,
     * '$'s and backslashes (place the escaped character in
     * uninterpreted) and 2) unescaped $'s that aren't before
     * the delimiter (expand the variable substitution).
     * The result is left in the Buffer buf.
     */
    Buf_Init(&buf, 0);
    for (st->cp = st->tstr + 1;
	 *st->cp != st->endc && *st->cp != ':' && *st->cp != '\0';
	 st->cp++) {
	if (*st->cp == '\\' &&
	    (st->cp[1] == ':' || st->cp[1] == '$' || st->cp[1] == st->endc ||
	     st->cp[1] == '\\')) {
	    Buf_AddByte(&buf, st->cp[1]);
	    st->cp++;
	} else if (*st->cp == '$') {
	    /*
	     * If unescaped dollar sign, assume it's a
	     * variable substitution and recurse.
	     */
	    char    *cp2;
	    int	    len;
	    void    *freeIt;

	    cp2 = Var_Parse(st->cp, st->ctxt, nflags, &len, &freeIt);
	    Buf_AddBytes(&buf, strlen(cp2), cp2);
	    free(freeIt);
	    st->cp += len - 1;
	} else {
	    Buf_AddByte(&buf, *st->cp);
	}
    }

    st->termc = *st->cp;

    if ((st->v->flags & VAR_JUNK) != 0)
	st->v->flags |= VAR_KEEP;
    if (nflags & VARF_WANTRES) {
	st->newStr = Buf_Destroy(&buf, FALSE);
    } else {
	st->newStr = st->nstr;
	Buf_Destroy(&buf, TRUE);
    }
}

/* :gmtime */
static Boolean
ApplyModifier_Gmtime(ApplyModifiersState *st)
{
    time_t utc;
    char *ep;

    st->cp = st->tstr + 1;	/* make sure it is set */
    if (!STRMOD_MATCHX(st->tstr, "gmtime", 6))
	return FALSE;
    if (st->tstr[6] == '=') {
	utc = strtoul(&st->tstr[7], &ep, 10);
	st->cp = ep;
    } else {
	utc = 0;
	st->cp = st->tstr + 6;
    }
    st->newStr = VarStrftime(st->nstr, 1, utc);
    st->termc = *st->cp;
    return TRUE;
}

/* :localtime */
static Boolean
ApplyModifier_Localtime(ApplyModifiersState *st)
{
    time_t utc;
    char *ep;

    st->cp = st->tstr + 1;	/* make sure it is set */
    if (!STRMOD_MATCHX(st->tstr, "localtime", 9))
	return FALSE;

    if (st->tstr[9] == '=') {
	utc = strtoul(&st->tstr[10], &ep, 10);
	st->cp = ep;
    } else {
	utc = 0;
	st->cp = st->tstr + 9;
    }
    st->newStr = VarStrftime(st->nstr, 0, utc);
    st->termc = *st->cp;
    return TRUE;
}

/* :hash */
static Boolean
ApplyModifier_Hash(ApplyModifiersState *st)
{
    st->cp = st->tstr + 1;	/* make sure it is set */
    if (!STRMOD_MATCH(st->tstr, "hash", 4))
	return FALSE;
    st->newStr = VarHash(st->nstr);
    st->cp = st->tstr + 4;
    st->termc = *st->cp;
    return TRUE;
}

/* :P */
static void
ApplyModifier_Path(ApplyModifiersState *st)
{
    GNode *gn;

    if ((st->v->flags & VAR_JUNK) != 0)
	st->v->flags |= VAR_KEEP;
    gn = Targ_FindNode(st->v->name, TARG_NOCREATE);
    if (gn == NULL || gn->type & OP_NOPATH) {
	st->newStr = NULL;
    } else if (gn->path) {
	st->newStr = bmake_strdup(gn->path);
    } else {
	st->newStr = Dir_FindFile(st->v->name, Suff_FindPath(gn));
    }
    if (!st->newStr)
	st->newStr = bmake_strdup(st->v->name);
    st->cp = ++st->tstr;
    st->termc = *st->tstr;
}

/* :!cmd! */
static Boolean
ApplyModifier_Exclam(ApplyModifiersState *st)
{
    const char *emsg;
    VarPattern pattern;

    pattern.flags = 0;

    st->delim = '!';
    emsg = NULL;
    st->cp = ++st->tstr;
    pattern.rhs = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	NULL, &pattern.rightLen, NULL);
    if (pattern.rhs == NULL)
	return FALSE;
    if (st->flags & VARF_WANTRES)
	st->newStr = Cmd_Exec(pattern.rhs, &emsg);
    else
	st->newStr = varNoError;
    free(UNCONST(pattern.rhs));
    if (emsg)
	Error(emsg, st->nstr);
    st->termc = *st->cp;
    st->delim = '\0';
    if (st->v->flags & VAR_JUNK)
	st->v->flags |= VAR_KEEP;
    return TRUE;
}

/* :range */
static Boolean
ApplyModifier_Range(ApplyModifiersState *st)
{
    int n;
    char *ep;

    st->cp = st->tstr + 1;	/* make sure it is set */
    if (!STRMOD_MATCHX(st->tstr, "range", 5))
	return FALSE;

    if (st->tstr[5] == '=') {
	n = strtoul(&st->tstr[6], &ep, 10);
	st->cp = ep;
    } else {
	n = 0;
	st->cp = st->tstr + 5;
    }
    st->newStr = VarRange(st->nstr, n);
    st->termc = *st->cp;
    return TRUE;
}

/* :Mpattern or :Npattern */
static void
ApplyModifier_Match(ApplyModifiersState *st)
{
    char    *pattern;
    const char *endpat;		/* points just after end of pattern */
    char    *cp2;
    Boolean copy;		/* pattern should be, or has been, copied */
    Boolean needSubst;
    int nest;

    copy = FALSE;
    needSubst = FALSE;
    nest = 1;
    /*
     * In the loop below, ignore ':' unless we are at
     * (or back to) the original brace level.
     * XXX This will likely not work right if $() and ${}
     * are intermixed.
     */
    for (st->cp = st->tstr + 1;
	 *st->cp != '\0' && !(*st->cp == ':' && nest == 1);
	 st->cp++) {
	if (*st->cp == '\\' &&
	    (st->cp[1] == ':' || st->cp[1] == st->endc ||
	     st->cp[1] == st->startc)) {
	    if (!needSubst)
		copy = TRUE;
	    st->cp++;
	    continue;
	}
	if (*st->cp == '$')
	    needSubst = TRUE;
	if (*st->cp == '(' || *st->cp == '{')
	    ++nest;
	if (*st->cp == ')' || *st->cp == '}') {
	    --nest;
	    if (nest == 0)
		break;
	}
    }
    st->termc = *st->cp;
    endpat = st->cp;
    if (copy) {
	/*
	 * Need to compress the \:'s out of the pattern, so
	 * allocate enough room to hold the uncompressed
	 * pattern (note that st->cp started at st->tstr+1, so
	 * st->cp - st->tstr takes the null byte into account) and
	 * compress the pattern into the space.
	 */
	pattern = bmake_malloc(st->cp - st->tstr);
	for (cp2 = pattern, st->cp = st->tstr + 1;
	     st->cp < endpat;
	     st->cp++, cp2++) {
	    if ((*st->cp == '\\') && (st->cp+1 < endpat) &&
		(st->cp[1] == ':' || st->cp[1] == st->endc))
		st->cp++;
	    *cp2 = *st->cp;
	}
	*cp2 = '\0';
	endpat = cp2;
    } else {
	/*
	 * Either Var_Subst or VarModify will need a
	 * nul-terminated string soon, so construct one now.
	 */
	pattern = bmake_strndup(st->tstr+1, endpat - (st->tstr + 1));
    }
    if (needSubst) {
	/* pattern contains embedded '$', so use Var_Subst to expand it. */
	cp2 = pattern;
	pattern = Var_Subst(NULL, cp2, st->ctxt, st->flags);
	free(cp2);
    }
    if (DEBUG(VAR))
	fprintf(debug_file, "Pattern[%s] for [%s] is [%s]\n",
	    st->v->name, st->nstr, pattern);
    if (*st->tstr == 'M') {
	st->newStr = VarModify(st->ctxt, &st->parsestate, st->nstr, VarMatch,
			       pattern);
    } else {
	st->newStr = VarModify(st->ctxt, &st->parsestate, st->nstr, VarNoMatch,
			       pattern);
    }
    free(pattern);
}

/* :S,from,to, */
static Boolean
ApplyModifier_Subst(ApplyModifiersState *st)
{
    VarPattern 	    pattern;
    Var_Parse_State tmpparsestate;

    pattern.flags = 0;
    tmpparsestate = st->parsestate;
    st->delim = st->tstr[1];
    st->tstr += 2;

    /*
     * If pattern begins with '^', it is anchored to the
     * start of the word -- skip over it and flag pattern.
     */
    if (*st->tstr == '^') {
	pattern.flags |= VAR_MATCH_START;
	st->tstr += 1;
    }

    st->cp = st->tstr;
    pattern.lhs = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	&pattern.flags, &pattern.leftLen, NULL);
    if (pattern.lhs == NULL)
	return FALSE;

    pattern.rhs = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	NULL, &pattern.rightLen, &pattern);
    if (pattern.rhs == NULL)
	return FALSE;

    /*
     * Check for global substitution. If 'g' after the final
     * delimiter, substitution is global and is marked that
     * way.
     */
    for (;; st->cp++) {
	switch (*st->cp) {
	case 'g':
	    pattern.flags |= VAR_SUB_GLOBAL;
	    continue;
	case '1':
	    pattern.flags |= VAR_SUB_ONE;
	    continue;
	case 'W':
	    tmpparsestate.oneBigWord = TRUE;
	    continue;
	}
	break;
    }

    st->termc = *st->cp;
    st->newStr = VarModify(
	st->ctxt, &tmpparsestate, st->nstr, VarSubstitute, &pattern);

    /* Free the two strings. */
    free(UNCONST(pattern.lhs));
    free(UNCONST(pattern.rhs));
    st->delim = '\0';
    return TRUE;
}

#ifndef NO_REGEX
/* :C,from,to, */
static Boolean
ApplyModifier_Regex(ApplyModifiersState *st)
{
    VarREPattern    pattern;
    char           *re;
    int             error;
    Var_Parse_State tmpparsestate;

    pattern.flags = 0;
    tmpparsestate = st->parsestate;
    st->delim = st->tstr[1];
    st->tstr += 2;

    st->cp = st->tstr;

    re = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	NULL, NULL, NULL);
    if (re == NULL)
	return FALSE;

    pattern.replace = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	NULL, NULL, NULL);
    if (pattern.replace == NULL) {
	free(re);
	return FALSE;
    }

    for (;; st->cp++) {
	switch (*st->cp) {
	case 'g':
	    pattern.flags |= VAR_SUB_GLOBAL;
	    continue;
	case '1':
	    pattern.flags |= VAR_SUB_ONE;
	    continue;
	case 'W':
	    tmpparsestate.oneBigWord = TRUE;
	    continue;
	}
	break;
    }

    st->termc = *st->cp;

    error = regcomp(&pattern.re, re, REG_EXTENDED);
    free(re);
    if (error) {
	*st->lengthPtr = st->cp - st->start + 1;
	VarREError(error, &pattern.re, "RE substitution error");
	free(pattern.replace);
	return FALSE;
    }

    pattern.nsub = pattern.re.re_nsub + 1;
    if (pattern.nsub < 1)
	pattern.nsub = 1;
    if (pattern.nsub > 10)
	pattern.nsub = 10;
    pattern.matches = bmake_malloc(pattern.nsub * sizeof(regmatch_t));
    st->newStr = VarModify(
	st->ctxt, &tmpparsestate, st->nstr, VarRESubstitute, &pattern);
    regfree(&pattern.re);
    free(pattern.replace);
    free(pattern.matches);
    st->delim = '\0';
    return TRUE;
}
#endif

/* :tA, :tu, :tl, etc. */
static Boolean
ApplyModifier_To(ApplyModifiersState *st)
{
    st->cp = st->tstr + 1;	/* make sure it is set */
    if (st->tstr[1] != st->endc && st->tstr[1] != ':') {
	if (st->tstr[1] == 's') {
	    /* Use the char (if any) at st->tstr[2] as the word separator. */
	    VarPattern pattern;

	    if (st->tstr[2] != st->endc &&
		(st->tstr[3] == st->endc || st->tstr[3] == ':')) {
		/* ":ts<unrecognised><endc>" or
		 * ":ts<unrecognised>:" */
		st->parsestate.varSpace = st->tstr[2];
		st->cp = st->tstr + 3;
	    } else if (st->tstr[2] == st->endc || st->tstr[2] == ':') {
		/* ":ts<endc>" or ":ts:" */
		st->parsestate.varSpace = 0;	/* no separator */
		st->cp = st->tstr + 2;
	    } else if (st->tstr[2] == '\\') {
		const char *xp = &st->tstr[3];
		int base = 8;	/* assume octal */

		switch (st->tstr[3]) {
		case 'n':
		    st->parsestate.varSpace = '\n';
		    st->cp = st->tstr + 4;
		    break;
		case 't':
		    st->parsestate.varSpace = '\t';
		    st->cp = st->tstr + 4;
		    break;
		case 'x':
		    base = 16;
		    xp++;
		    goto get_numeric;
		case '0':
		    base = 0;
		    goto get_numeric;
		default:
		    if (isdigit((unsigned char)st->tstr[3])) {
			char *ep;
		    get_numeric:
			st->parsestate.varSpace = strtoul(xp, &ep, base);
			if (*ep != ':' && *ep != st->endc)
			    return FALSE;
			st->cp = ep;
		    } else {
			/* ":ts<backslash><unrecognised>". */
			return FALSE;
		    }
		    break;
		}
	    } else {
		/* Found ":ts<unrecognised><unrecognised>". */
		return FALSE;
	    }

	    st->termc = *st->cp;

	    /*
	     * We cannot be certain that VarModify will be used - even if there
	     * is a subsequent modifier, so do a no-op VarSubstitute now to for
	     * str to be re-expanded without the spaces.
	     */
	    pattern.flags = VAR_SUB_ONE;
	    pattern.lhs = pattern.rhs = "\032";
	    pattern.leftLen = pattern.rightLen = 1;

	    st->newStr = VarModify(
		st->ctxt, &st->parsestate, st->nstr, VarSubstitute, &pattern);
	} else if (st->tstr[2] == st->endc || st->tstr[2] == ':') {
	    /* Check for two-character options: ":tu", ":tl" */
	    if (st->tstr[1] == 'A') {	/* absolute path */
		st->newStr = VarModify(
			st->ctxt, &st->parsestate, st->nstr, VarRealpath, NULL);
		st->cp = st->tstr + 2;
		st->termc = *st->cp;
	    } else if (st->tstr[1] == 'u') {
		char *dp = bmake_strdup(st->nstr);
		for (st->newStr = dp; *dp; dp++)
		    *dp = toupper((unsigned char)*dp);
		st->cp = st->tstr + 2;
		st->termc = *st->cp;
	    } else if (st->tstr[1] == 'l') {
		char *dp = bmake_strdup(st->nstr);
		for (st->newStr = dp; *dp; dp++)
		    *dp = tolower((unsigned char)*dp);
		st->cp = st->tstr + 2;
		st->termc = *st->cp;
	    } else if (st->tstr[1] == 'W' || st->tstr[1] == 'w') {
		st->parsestate.oneBigWord = (st->tstr[1] == 'W');
		st->newStr = st->nstr;
		st->cp = st->tstr + 2;
		st->termc = *st->cp;
	    } else {
		/* Found ":t<unrecognised>:" or ":t<unrecognised><endc>". */
		return FALSE;
	    }
	} else {
	    /* Found ":t<unrecognised><unrecognised>". */
	    return FALSE;
	}
    } else {
	/* Found ":t<endc>" or ":t:". */
	return FALSE;
    }
    return TRUE;
}

/* :[#], :[1], etc. */
static int
ApplyModifier_Words(ApplyModifiersState *st)
{
    /*
     * Look for the closing ']', recursively
     * expanding any embedded variables.
     *
     * estr is a pointer to the expanded result,
     * which we must free().
     */
    char *estr;

    st->cp = st->tstr + 1;	/* point to char after '[' */
    st->delim = ']';		/* look for closing ']' */
    estr = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	NULL, NULL, NULL);
    if (estr == NULL)
	return 'c';		/* report missing ']' */
    /* now st->cp points just after the closing ']' */
    st->delim = '\0';
    if (st->cp[0] != ':' && st->cp[0] != st->endc) {
	/* Found junk after ']' */
	free(estr);
	return 'b';
    }
    if (estr[0] == '\0') {
	/* Found empty square brackets in ":[]". */
	free(estr);
	return 'b';
    } else if (estr[0] == '#' && estr[1] == '\0') {
	/* Found ":[#]" */

	/*
	 * We will need enough space for the decimal
	 * representation of an int.  We calculate the
	 * space needed for the octal representation,
	 * and add enough slop to cope with a '-' sign
	 * (which should never be needed) and a '\0'
	 * string terminator.
	 */
	int newStrSize = (sizeof(int) * CHAR_BIT + 2) / 3 + 2;

	st->newStr = bmake_malloc(newStrSize);
	if (st->parsestate.oneBigWord) {
	    strncpy(st->newStr, "1", newStrSize);
	} else {
	    /* XXX: brk_string() is a rather expensive
	     * way of counting words. */
	    char **av;
	    char *as;
	    int ac;

	    av = brk_string(st->nstr, &ac, FALSE, &as);
	    snprintf(st->newStr, newStrSize, "%d", ac);
	    free(as);
	    free(av);
	}
	st->termc = *st->cp;
	free(estr);
	return 0;
    } else if (estr[0] == '*' && estr[1] == '\0') {
	/* Found ":[*]" */
	st->parsestate.oneBigWord = TRUE;
	st->newStr = st->nstr;
	st->termc = *st->cp;
	free(estr);
	return 0;
    } else if (estr[0] == '@' && estr[1] == '\0') {
	/* Found ":[@]" */
	st->parsestate.oneBigWord = FALSE;
	st->newStr = st->nstr;
	st->termc = *st->cp;
	free(estr);
	return 0;
    } else {
	char *ep;
	/*
	 * We expect estr to contain a single
	 * integer for :[N], or two integers
	 * separated by ".." for :[start..end].
	 */
	VarSelectWords_t seldata = { 0, 0 };

	seldata.start = strtol(estr, &ep, 0);
	if (ep == estr) {
	    /* Found junk instead of a number */
	    free(estr);
	    return 'b';
	} else if (ep[0] == '\0') {
	    /* Found only one integer in :[N] */
	    seldata.end = seldata.start;
	} else if (ep[0] == '.' && ep[1] == '.' && ep[2] != '\0') {
	    /* Expecting another integer after ".." */
	    ep += 2;
	    seldata.end = strtol(ep, &ep, 0);
	    if (ep[0] != '\0') {
		/* Found junk after ".." */
		free(estr);
		return 'b';
	    }
	} else {
	    /* Found junk instead of ".." */
	    free(estr);
	    return 'b';
	}
	/*
	 * Now seldata is properly filled in,
	 * but we still have to check for 0 as
	 * a special case.
	 */
	if (seldata.start == 0 && seldata.end == 0) {
	    /* ":[0]" or perhaps ":[0..0]" */
	    st->parsestate.oneBigWord = TRUE;
	    st->newStr = st->nstr;
	    st->termc = *st->cp;
	    free(estr);
	    return 0;
	} else if (seldata.start == 0 || seldata.end == 0) {
	    /* ":[0..N]" or ":[N..0]" */
	    free(estr);
	    return 'b';
	}
	/* Normal case: select the words described by seldata. */
	st->newStr = VarSelectWords(
	    st->ctxt, &st->parsestate, st->nstr, &seldata);

	st->termc = *st->cp;
	free(estr);
	return 0;
    }
}

/* :O or :Ox */
static Boolean
ApplyModifier_Order(ApplyModifiersState *st)
{
    char otype;

    st->cp = st->tstr + 1;	/* skip to the rest in any case */
    if (st->tstr[1] == st->endc || st->tstr[1] == ':') {
	otype = 's';
	st->termc = *st->cp;
    } else if ((st->tstr[1] == 'r' || st->tstr[1] == 'x') &&
	       (st->tstr[2] == st->endc || st->tstr[2] == ':')) {
	otype = st->tstr[1];
	st->cp = st->tstr + 2;
	st->termc = *st->cp;
    } else {
	return FALSE;
    }
    st->newStr = VarOrder(st->nstr, otype);
    return TRUE;
}

/* :? then : else */
static Boolean
ApplyModifier_IfElse(ApplyModifiersState *st)
{
    VarPattern pattern;
    Boolean value;
    int cond_rc;
    VarPattern_Flags lhs_flags, rhs_flags;

    /* find ':', and then substitute accordingly */
    if (st->flags & VARF_WANTRES) {
	cond_rc = Cond_EvalExpression(NULL, st->v->name, &value, 0, FALSE);
	if (cond_rc == COND_INVALID) {
	    lhs_flags = rhs_flags = VAR_NOSUBST;
	} else if (value) {
	    lhs_flags = 0;
	    rhs_flags = VAR_NOSUBST;
	} else {
	    lhs_flags = VAR_NOSUBST;
	    rhs_flags = 0;
	}
    } else {
	/* we are just consuming and discarding */
	cond_rc = value = 0;
	lhs_flags = rhs_flags = VAR_NOSUBST;
    }
    pattern.flags = 0;

    st->cp = ++st->tstr;
    st->delim = ':';
    pattern.lhs = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	&lhs_flags, &pattern.leftLen, NULL);
    if (pattern.lhs == NULL)
	return FALSE;

    /* BROPEN or PROPEN */
    st->delim = st->endc;
    pattern.rhs = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	&rhs_flags, &pattern.rightLen, NULL);
    if (pattern.rhs == NULL)
	return FALSE;

    st->termc = *--st->cp;
    st->delim = '\0';
    if (cond_rc == COND_INVALID) {
	Error("Bad conditional expression `%s' in %s?%s:%s",
	    st->v->name, st->v->name, pattern.lhs, pattern.rhs);
	return FALSE;
    }

    if (value) {
	st->newStr = UNCONST(pattern.lhs);
	free(UNCONST(pattern.rhs));
    } else {
	st->newStr = UNCONST(pattern.rhs);
	free(UNCONST(pattern.lhs));
    }
    if (st->v->flags & VAR_JUNK)
	st->v->flags |= VAR_KEEP;
    return TRUE;
}

/* "::=", "::!=", "::+=", or "::?=" */
static int
ApplyModifier_Assign(ApplyModifiersState *st)
{
    if (st->tstr[1] == '=' ||
	(st->tstr[2] == '=' &&
	 (st->tstr[1] == '!' || st->tstr[1] == '+' || st->tstr[1] == '?'))) {
	GNode *v_ctxt;		/* context where v belongs */
	const char *emsg;
	char *sv_name;
	VarPattern pattern;
	int how;
	VarPattern_Flags vflags;

	if (st->v->name[0] == 0)
	    return 'b';

	v_ctxt = st->ctxt;
	sv_name = NULL;
	++st->tstr;
	if (st->v->flags & VAR_JUNK) {
	    /*
	     * We need to bmake_strdup() it incase
	     * VarGetPattern() recurses.
	     */
	    sv_name = st->v->name;
	    st->v->name = bmake_strdup(st->v->name);
	} else if (st->ctxt != VAR_GLOBAL) {
	    Var *gv = VarFind(st->v->name, st->ctxt, 0);
	    if (gv == NULL)
		v_ctxt = VAR_GLOBAL;
	    else
		VarFreeEnv(gv, TRUE);
	}

	switch ((how = *st->tstr)) {
	case '+':
	case '?':
	case '!':
	    st->cp = &st->tstr[2];
	    break;
	default:
	    st->cp = ++st->tstr;
	    break;
	}
	st->delim = st->startc == PROPEN ? PRCLOSE : BRCLOSE;
	pattern.flags = 0;

	vflags = (st->flags & VARF_WANTRES) ? 0 : VAR_NOSUBST;
	pattern.rhs = VarGetPattern(
	    st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	    &vflags, &pattern.rightLen, NULL);
	if (st->v->flags & VAR_JUNK) {
	    /* restore original name */
	    free(st->v->name);
	    st->v->name = sv_name;
	}
	if (pattern.rhs == NULL)
	    return 'c';

	st->termc = *--st->cp;
	st->delim = '\0';

	if (st->flags & VARF_WANTRES) {
	    switch (how) {
	    case '+':
		Var_Append(st->v->name, pattern.rhs, v_ctxt);
		break;
	    case '!':
		st->newStr = Cmd_Exec(pattern.rhs, &emsg);
		if (emsg)
		    Error(emsg, st->nstr);
		else
		    Var_Set(st->v->name, st->newStr, v_ctxt);
		free(st->newStr);
		break;
	    case '?':
		if ((st->v->flags & VAR_JUNK) == 0)
		    break;
		/* FALLTHROUGH */
	    default:
		Var_Set(st->v->name, pattern.rhs, v_ctxt);
		break;
	    }
	}
	free(UNCONST(pattern.rhs));
	st->newStr = varNoError;
	return 0;
    }
    return 'd';			/* "::<unrecognised>" */
}

/* remember current value */
static Boolean
ApplyModifier_Remember(ApplyModifiersState *st)
{
    st->cp = st->tstr + 1;	/* make sure it is set */
    if (!STRMOD_MATCHX(st->tstr, "_", 1))
	return FALSE;

    if (st->tstr[1] == '=') {
	char *np;
	int n;

	st->cp++;
	n = strcspn(st->cp, ":)}");
	np = bmake_strndup(st->cp, n + 1);
	np[n] = '\0';
	st->cp = st->tstr + 2 + n;
	Var_Set(np, st->nstr, st->ctxt);
	free(np);
    } else {
	Var_Set("_", st->nstr, st->ctxt);
    }
    st->newStr = st->nstr;
    st->termc = *st->cp;
    return TRUE;
}

#ifdef SYSVVARSUB
/* :from=to */
static int
ApplyModifier_SysV(ApplyModifiersState *st)
{
    /*
     * This can either be a bogus modifier or a System-V
     * substitution command.
     */
    VarPattern      pattern;
    Boolean         eqFound = FALSE;

    pattern.flags = 0;

    /*
     * First we make a pass through the string trying
     * to verify it is a SYSV-make-style translation:
     * it must be: <string1>=<string2>)
     */
    st->cp = st->tstr;
    st->cnt = 1;
    while (*st->cp != '\0' && st->cnt) {
	if (*st->cp == '=') {
	    eqFound = TRUE;
	    /* continue looking for st->endc */
	} else if (*st->cp == st->endc)
	    st->cnt--;
	else if (*st->cp == st->startc)
	    st->cnt++;
	if (st->cnt)
	    st->cp++;
    }
    if (*st->cp != st->endc || !eqFound)
	return 0;

    st->delim = '=';
    st->cp = st->tstr;
    pattern.lhs = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	&pattern.flags, &pattern.leftLen, NULL);
    if (pattern.lhs == NULL)
	return 'c';

    st->delim = st->endc;
    pattern.rhs = VarGetPattern(
	st->ctxt, &st->parsestate, st->flags, &st->cp, st->delim,
	NULL, &pattern.rightLen, &pattern);
    if (pattern.rhs == NULL)
	return 'c';

    /*
     * SYSV modifications happen through the whole
     * string. Note the pattern is anchored at the end.
     */
    st->termc = *--st->cp;
    st->delim = '\0';
    if (pattern.leftLen == 0 && *st->nstr == '\0') {
	st->newStr = st->nstr;	/* special case */
    } else {
	st->newStr = VarModify(
	    st->ctxt, &st->parsestate, st->nstr, VarSYSVMatch, &pattern);
    }
    free(UNCONST(pattern.lhs));
    free(UNCONST(pattern.rhs));
    return '=';
}
#endif

/*
 * Now we need to apply any modifiers the user wants applied.
 * These are:
 *  	  :M<pattern>	words which match the given <pattern>.
 *  			<pattern> is of the standard file
 *  			wildcarding form.
 *  	  :N<pattern>	words which do not match the given <pattern>.
 *  	  :S<d><pat1><d><pat2><d>[1gW]
 *  			Substitute <pat2> for <pat1> in the value
 *  	  :C<d><pat1><d><pat2><d>[1gW]
 *  			Substitute <pat2> for regex <pat1> in the value
 *  	  :H		Substitute the head of each word
 *  	  :T		Substitute the tail of each word
 *  	  :E		Substitute the extension (minus '.') of
 *  			each word
 *  	  :R		Substitute the root of each word
 *  			(pathname minus the suffix).
 *	  :O		("Order") Alphabeticaly sort words in variable.
 *	  :Ox		("intermiX") Randomize words in variable.
 *	  :u		("uniq") Remove adjacent duplicate words.
 *	  :tu		Converts the variable contents to uppercase.
 *	  :tl		Converts the variable contents to lowercase.
 *	  :ts[c]	Sets varSpace - the char used to
 *			separate words to 'c'. If 'c' is
 *			omitted then no separation is used.
 *	  :tW		Treat the variable contents as a single
 *			word, even if it contains spaces.
 *			(Mnemonic: one big 'W'ord.)
 *	  :tw		Treat the variable contents as multiple
 *			space-separated words.
 *			(Mnemonic: many small 'w'ords.)
 *	  :[index]	Select a single word from the value.
 *	  :[start..end]	Select multiple words from the value.
 *	  :[*] or :[0]	Select the entire value, as a single
 *			word.  Equivalent to :tW.
 *	  :[@]		Select the entire value, as multiple
 *			words.	Undoes the effect of :[*].
 *			Equivalent to :tw.
 *	  :[#]		Returns the number of words in the value.
 *
 *	  :?<true-value>:<false-value>
 *			If the variable evaluates to true, return
 *			true value, else return the second value.
 *    	  :lhs=rhs  	Like :S, but the rhs goes to the end of
 *    			the invocation.
 *	  :sh		Treat the current value as a command
 *			to be run, new value is its output.
 * The following added so we can handle ODE makefiles.
 *	  :@<tmpvar>@<newval>@
 *			Assign a temporary local variable <tmpvar>
 *			to the current value of each word in turn
 *			and replace each word with the result of
 *			evaluating <newval>
 *	  :D<newval>	Use <newval> as value if variable defined
 *	  :U<newval>	Use <newval> as value if variable undefined
 *	  :L		Use the name of the variable as the value.
 *	  :P		Use the path of the node that has the same
 *			name as the variable as the value.  This
 *			basically includes an implied :L so that
 *			the common method of refering to the path
 *			of your dependent 'x' in a rule is to use
 *			the form '${x:P}'.
 *	  :!<cmd>!	Run cmd much the same as :sh run's the
 *			current value of the variable.
 * The ::= modifiers, actually assign a value to the variable.
 * Their main purpose is in supporting modifiers of .for loop
 * iterators and other obscure uses.  They always expand to
 * nothing.  In a target rule that would otherwise expand to an
 * empty line they can be preceded with @: to keep make happy.
 * Eg.
 *
 * foo:	.USE
 * .for i in ${.TARGET} ${.TARGET:R}.gz
 * 	@: ${t::=$i}
 *	@echo blah ${t:T}
 * .endfor
 *
 *	  ::=<str>	Assigns <str> as the new value of variable.
 *	  ::?=<str>	Assigns <str> as value of variable if
 *			it was not already set.
 *	  ::+=<str>	Appends <str> to variable.
 *	  ::!=<cmd>	Assigns output of <cmd> as the new value of
 *			variable.
 */
static char *
ApplyModifiers(char *nstr, const char *tstr,
	       int const startc, int const endc,
	       Var * const v, GNode * const ctxt, int const flags,
	       int * const lengthPtr, void ** const freePtr)
{
    ApplyModifiersState st = {
	startc, endc, v, ctxt, flags, lengthPtr, freePtr,
	nstr, tstr, tstr, tstr,
	'\0', 0, '\0', 0, {' ', FALSE}, NULL
    };

    while (*st.tstr && *st.tstr != st.endc) {

	if (*st.tstr == '$') {
	    /*
	     * We may have some complex modifiers in a variable.
	     */
	    void *freeIt;
	    char *rval;
	    int rlen;
	    int c;

	    rval = Var_Parse(st.tstr, st.ctxt, st.flags, &rlen, &freeIt);

	    /*
	     * If we have not parsed up to st.endc or ':',
	     * we are not interested.
	     */
	    if (rval != NULL && *rval &&
		(c = st.tstr[rlen]) != '\0' &&
		c != ':' &&
		c != st.endc) {
		free(freeIt);
		goto apply_mods;
	    }

	    if (DEBUG(VAR)) {
		fprintf(debug_file, "Got '%s' from '%.*s'%.*s\n",
		       rval, rlen, st.tstr, rlen, st.tstr + rlen);
	    }

	    st.tstr += rlen;

	    if (rval != NULL && *rval) {
		int used;

		st.nstr = ApplyModifiers(st.nstr, rval, 0, 0, st.v,
				      st.ctxt, st.flags, &used, st.freePtr);
		if (st.nstr == var_Error
		    || (st.nstr == varNoError && (st.flags & VARF_UNDEFERR) == 0)
		    || strlen(rval) != (size_t) used) {
		    free(freeIt);
		    goto out;	/* error already reported */
		}
	    }
	    free(freeIt);
	    if (*st.tstr == ':')
		st.tstr++;
	    else if (!*st.tstr && st.endc) {
		Error("Unclosed variable specification after complex "
		    "modifier (expecting '%c') for %s", st.endc, st.v->name);
		goto out;
	    }
	    continue;
	}
    apply_mods:
	if (DEBUG(VAR)) {
	    fprintf(debug_file, "Applying[%s] :%c to \"%s\"\n", st.v->name,
		*st.tstr, st.nstr);
	}
	st.newStr = var_Error;
	switch ((st.modifier = *st.tstr)) {
	case ':':
	    {
		int res = ApplyModifier_Assign(&st);
		if (res == 'b')
		    goto bad_modifier;
		if (res == 'c')
		    goto cleanup;
		if (res == 'd')
		    goto default_case;
		break;
	    }
	case '@':
	    ApplyModifier_At(&st);
	    break;
	case '_':
	    if (!ApplyModifier_Remember(&st))
		goto default_case;
	    break;
	case 'D':
	case 'U':
	    ApplyModifier_Defined(&st);
	    break;
	case 'L':
	    {
		if ((st.v->flags & VAR_JUNK) != 0)
		    st.v->flags |= VAR_KEEP;
		st.newStr = bmake_strdup(st.v->name);
		st.cp = ++st.tstr;
		st.termc = *st.tstr;
		break;
	    }
	case 'P':
	    ApplyModifier_Path(&st);
	    break;
	case '!':
	    if (!ApplyModifier_Exclam(&st))
		goto cleanup;
	    break;
	case '[':
	    {
		int res = ApplyModifier_Words(&st);
		if (res == 'b')
		    goto bad_modifier;
		if (res == 'c')
		    goto cleanup;
		break;
	    }
	case 'g':
	    if (!ApplyModifier_Gmtime(&st))
		goto default_case;
	    break;
	case 'h':
	    if (!ApplyModifier_Hash(&st))
		goto default_case;
	    break;
	case 'l':
	    if (!ApplyModifier_Localtime(&st))
		goto default_case;
	    break;
	case 't':
	    if (!ApplyModifier_To(&st))
		goto bad_modifier;
	    break;
	case 'N':
	case 'M':
	    ApplyModifier_Match(&st);
	    break;
	case 'S':
	    if (!ApplyModifier_Subst(&st))
		goto cleanup;
	    break;
	case '?':
	    if (!ApplyModifier_IfElse(&st))
		goto cleanup;
	    break;
#ifndef NO_REGEX
	case 'C':
	    if (!ApplyModifier_Regex(&st))
		goto cleanup;
	    break;
#endif
	case 'q':
	case 'Q':
	    if (st.tstr[1] == st.endc || st.tstr[1] == ':') {
		st.newStr = VarQuote(st.nstr, st.modifier == 'q');
		st.cp = st.tstr + 1;
		st.termc = *st.cp;
		break;
	    }
	    goto default_case;
	case 'T':
	    if (st.tstr[1] == st.endc || st.tstr[1] == ':') {
		st.newStr = VarModify(st.ctxt, &st.parsestate, st.nstr, VarTail,
				   NULL);
		st.cp = st.tstr + 1;
		st.termc = *st.cp;
		break;
	    }
	    goto default_case;
	case 'H':
	    if (st.tstr[1] == st.endc || st.tstr[1] == ':') {
		st.newStr = VarModify(st.ctxt, &st.parsestate, st.nstr, VarHead,
				   NULL);
		st.cp = st.tstr + 1;
		st.termc = *st.cp;
		break;
	    }
	    goto default_case;
	case 'E':
	    if (st.tstr[1] == st.endc || st.tstr[1] == ':') {
		st.newStr = VarModify(st.ctxt, &st.parsestate, st.nstr, VarSuffix,
				   NULL);
		st.cp = st.tstr + 1;
		st.termc = *st.cp;
		break;
	    }
	    goto default_case;
	case 'R':
	    if (st.tstr[1] == st.endc || st.tstr[1] == ':') {
		st.newStr = VarModify(st.ctxt, &st.parsestate, st.nstr, VarRoot,
				   NULL);
		st.cp = st.tstr + 1;
		st.termc = *st.cp;
		break;
	    }
	    goto default_case;
	case 'r':
	    if (!ApplyModifier_Range(&st))
		goto default_case;
	    break;
	case 'O':
	    if (!ApplyModifier_Order(&st))
		goto bad_modifier;
	    break;
	case 'u':
	    if (st.tstr[1] == st.endc || st.tstr[1] == ':') {
		st.newStr = VarUniq(st.nstr);
		st.cp = st.tstr + 1;
		st.termc = *st.cp;
		break;
	    }
	    goto default_case;
#ifdef SUNSHCMD
	case 's':
	    if (st.tstr[1] == 'h' && (st.tstr[2] == st.endc || st.tstr[2] == ':')) {
		const char *emsg;
		if (st.flags & VARF_WANTRES) {
		    st.newStr = Cmd_Exec(st.nstr, &emsg);
		    if (emsg)
			Error(emsg, st.nstr);
		} else
		    st.newStr = varNoError;
		st.cp = st.tstr + 2;
		st.termc = *st.cp;
		break;
	    }
	    goto default_case;
#endif
	default:
	default_case:
	    {
#ifdef SYSVVARSUB
		int res = ApplyModifier_SysV(&st);
		if (res == 'c')
		    goto cleanup;
		if (res != '=')
#endif
		{
		    Error("Unknown modifier '%c'", *st.tstr);
		    for (st.cp = st.tstr+1;
			 *st.cp != ':' && *st.cp != st.endc && *st.cp != '\0';
			 st.cp++)
			continue;
		    st.termc = *st.cp;
		    st.newStr = var_Error;
		}
	    }
	}
	if (DEBUG(VAR)) {
	    fprintf(debug_file, "Result[%s] of :%c is \"%s\"\n",
		st.v->name, st.modifier, st.newStr);
	}

	if (st.newStr != st.nstr) {
	    if (*st.freePtr) {
		free(st.nstr);
		*st.freePtr = NULL;
	    }
	    st.nstr = st.newStr;
	    if (st.nstr != var_Error && st.nstr != varNoError) {
		*st.freePtr = st.nstr;
	    }
	}
	if (st.termc == '\0' && st.endc != '\0') {
	    Error("Unclosed variable specification (expecting '%c') "
		"for \"%s\" (value \"%s\") modifier %c",
		st.endc, st.v->name, st.nstr, st.modifier);
	} else if (st.termc == ':') {
	    st.cp++;
	}
	st.tstr = st.cp;
    }
out:
    *st.lengthPtr = st.tstr - st.start;
    return st.nstr;

bad_modifier:
    /* "{(" */
    Error("Bad modifier `:%.*s' for %s", (int)strcspn(st.tstr, ":)}"), st.tstr,
	  st.v->name);

cleanup:
    *st.lengthPtr = st.cp - st.start;
    if (st.delim != '\0')
	Error("Unclosed substitution for %s (%c missing)",
	      st.v->name, st.delim);
    free(*st.freePtr);
    *st.freePtr = NULL;
    return var_Error;
}

/*-
 *-----------------------------------------------------------------------
 * Var_Parse --
 *	Given the start of a variable invocation, extract the variable
 *	name and find its value, then modify it according to the
 *	specification.
 *
 * Input:
 *	str		The string to parse
 *	ctxt		The context for the variable
 *	flags		VARF_UNDEFERR	if undefineds are an error
 *			VARF_WANTRES	if we actually want the result
 *			VARF_ASSIGN	if we are in a := assignment
 *	lengthPtr	OUT: The length of the specification
 *	freePtr		OUT: Non-NULL if caller should free *freePtr
 *
 * Results:
 *	The (possibly-modified) value of the variable or var_Error if the
 *	specification is invalid. The length of the specification is
 *	placed in *lengthPtr (for invalid specifications, this is just
 *	2...?).
 *	If *freePtr is non-NULL then it's a pointer that the caller
 *	should pass to free() to free memory used by the result.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
/* coverity[+alloc : arg-*4] */
char *
Var_Parse(const char *str, GNode *ctxt, Varf_Flags flags,
	  int *lengthPtr, void **freePtr)
{
    const char	*tstr;		/* Pointer into str */
    Var		*v;		/* Variable in invocation */
    Boolean 	 haveModifier;	/* TRUE if have modifiers for the variable */
    char	 endc;		/* Ending character when variable in parens
				 * or braces */
    char	 startc;	/* Starting character when variable in parens
				 * or braces */
    int		 vlen;		/* Length of variable name */
    const char 	*start;		/* Points to original start of str */
    char	*nstr;		/* New string, used during expansion */
    Boolean	 dynamic;	/* TRUE if the variable is local and we're
				 * expanding it in a non-local context. This
				 * is done to support dynamic sources. The
				 * result is just the invocation, unaltered */
    const char	*extramodifiers; /* extra modifiers to apply first */
    char	 name[2];

    *freePtr = NULL;
    extramodifiers = NULL;
    dynamic = FALSE;
    start = str;

    startc = str[1];
    if (startc != PROPEN && startc != BROPEN) {
	/*
	 * If it's not bounded by braces of some sort, life is much simpler.
	 * We just need to check for the first character and return the
	 * value if it exists.
	 */

	/* Error out some really stupid names */
	if (startc == '\0' || strchr(")}:$", startc)) {
	    *lengthPtr = 1;
	    return var_Error;
	}
	name[0] = startc;
	name[1] = '\0';

	v = VarFind(name, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	if (v == NULL) {
	    *lengthPtr = 2;

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
		switch (str[1]) {
		case '@':
		    return UNCONST("$(.TARGET)");
		case '%':
		    return UNCONST("$(.MEMBER)");
		case '*':
		    return UNCONST("$(.PREFIX)");
		case '!':
		    return UNCONST("$(.ARCHIVE)");
		}
	    }
	    return (flags & VARF_UNDEFERR) ? var_Error : varNoError;
	} else {
	    haveModifier = FALSE;
	    tstr = &str[1];
	    endc = str[1];
	}
    } else {
	Buffer buf;		/* Holds the variable name */
	int depth = 1;

	endc = startc == PROPEN ? PRCLOSE : BRCLOSE;
	Buf_Init(&buf, 0);

	/*
	 * Skip to the end character or a colon, whichever comes first.
	 */
	for (tstr = str + 2; *tstr != '\0'; tstr++) {
	    /* Track depth so we can spot parse errors. */
	    if (*tstr == startc)
		depth++;
	    if (*tstr == endc) {
		if (--depth == 0)
		    break;
	    }
	    if (depth == 1 && *tstr == ':')
		break;
	    /* A variable inside a variable, expand. */
	    if (*tstr == '$') {
		int rlen;
		void *freeIt;
		char *rval = Var_Parse(tstr, ctxt, flags, &rlen, &freeIt);
		if (rval != NULL)
		    Buf_AddBytes(&buf, strlen(rval), rval);
		free(freeIt);
		tstr += rlen - 1;
	    } else
		Buf_AddByte(&buf, *tstr);
	}
	if (*tstr == ':') {
	    haveModifier = TRUE;
	} else if (*tstr == endc) {
	    haveModifier = FALSE;
	} else {
	    /*
	     * If we never did find the end character, return NULL
	     * right now, setting the length to be the distance to
	     * the end of the string, since that's what make does.
	     */
	    *lengthPtr = tstr - str;
	    Buf_Destroy(&buf, TRUE);
	    return var_Error;
	}
	str = Buf_GetAll(&buf, &vlen);

	/*
	 * At this point, str points into newly allocated memory from
	 * buf, containing only the name of the variable.
	 *
	 * start and tstr point into the const string that was pointed
	 * to by the original value of the str parameter.  start points
	 * to the '$' at the beginning of the string, while tstr points
	 * to the char just after the end of the variable name -- this
	 * will be '\0', ':', PRCLOSE, or BRCLOSE.
	 */

	v = VarFind(str, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	/*
	 * Check also for bogus D and F forms of local variables since we're
	 * in a local context and the name is the right length.
	 */
	if ((v == NULL) && (ctxt != VAR_CMD) && (ctxt != VAR_GLOBAL) &&
		(vlen == 2) && (str[1] == 'F' || str[1] == 'D') &&
		strchr("@%?*!<>", str[0]) != NULL) {
	    /*
	     * Well, it's local -- go look for it.
	     */
	    name[0] = *str;
	    name[1] = '\0';
	    v = VarFind(name, ctxt, 0);

	    if (v != NULL) {
		if (str[1] == 'D') {
		    extramodifiers = "H:";
		} else { /* F */
		    extramodifiers = "T:";
		}
	    }
	}

	if (v == NULL) {
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
		switch (*str) {
		case '@':
		case '%':
		case '*':
		case '!':
		    dynamic = TRUE;
		    break;
		}
	    } else if (vlen > 2 && *str == '.' &&
		       isupper((unsigned char) str[1]) &&
		       (ctxt == VAR_CMD || ctxt == VAR_GLOBAL))
	    {
		int len = vlen - 1;
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
		if (dynamic) {
		    char *pstr = bmake_strndup(start, *lengthPtr);
		    *freePtr = pstr;
		    Buf_Destroy(&buf, TRUE);
		    return pstr;
		} else {
		    Buf_Destroy(&buf, TRUE);
		    return (flags & VARF_UNDEFERR) ? var_Error : varNoError;
		}
	    } else {
		/*
		 * Still need to get to the end of the variable specification,
		 * so kludge up a Var structure for the modifications
		 */
		v = bmake_malloc(sizeof(Var));
		v->name = UNCONST(str);
		Buf_Init(&v->val, 1);
		v->flags = VAR_JUNK;
		Buf_Destroy(&buf, FALSE);
	    }
	} else
	    Buf_Destroy(&buf, TRUE);
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
    nstr = Buf_GetAll(&v->val, NULL);
    if (strchr(nstr, '$') != NULL && (flags & VARF_WANTRES) != 0) {
	nstr = Var_Subst(NULL, nstr, ctxt, flags);
	*freePtr = nstr;
    }

    v->flags &= ~VAR_IN_USE;

    if (nstr != NULL && (haveModifier || extramodifiers != NULL)) {
	void *extraFree;
	int used;

	extraFree = NULL;
	if (extramodifiers != NULL) {
	    nstr = ApplyModifiers(nstr, extramodifiers, '(', ')',
				  v, ctxt, flags, &used, &extraFree);
	}

	if (haveModifier) {
	    /* Skip initial colon. */
	    tstr++;

	    nstr = ApplyModifiers(nstr, tstr, startc, endc,
				  v, ctxt, flags, &used, freePtr);
	    tstr += used;
	    free(extraFree);
	} else {
	    *freePtr = extraFree;
	}
    }
    *lengthPtr = tstr - start + (*tstr ? 1 : 0);

    if (v->flags & VAR_FROM_ENV) {
	Boolean destroy = FALSE;

	if (nstr != Buf_GetAll(&v->val, NULL)) {
	    destroy = TRUE;
	} else {
	    /*
	     * Returning the value unmodified, so tell the caller to free
	     * the thing.
	     */
	    *freePtr = nstr;
	}
	VarFreeEnv(v, destroy);
    } else if (v->flags & VAR_JUNK) {
	/*
	 * Perform any free'ing needed and set *freePtr to NULL so the caller
	 * doesn't try to free a static pointer.
	 * If VAR_KEEP is also set then we want to keep str as is.
	 */
	if (!(v->flags & VAR_KEEP)) {
	    if (*freePtr) {
		free(nstr);
		*freePtr = NULL;
	    }
	    if (dynamic) {
		nstr = bmake_strndup(start, *lengthPtr);
		*freePtr = nstr;
	    } else {
		nstr = (flags & VARF_UNDEFERR) ? var_Error : varNoError;
	    }
	}
	if (nstr != Buf_GetAll(&v->val, NULL))
	    Buf_Destroy(&v->val, TRUE);
	free(v->name);
	free(v);
    }
    return nstr;
}

/*-
 *-----------------------------------------------------------------------
 * Var_Subst  --
 *	Substitute for all variables in the given string in the given context.
 *	If flags & VARF_UNDEFERR, Parse_Error will be called when an undefined
 *	variable is encountered.
 *
 * Input:
 *	var		Named variable || NULL for all
 *	str		the string which to substitute
 *	ctxt		the context wherein to find variables
 *	flags		VARF_UNDEFERR	if undefineds are an error
 *			VARF_WANTRES	if we actually want the result
 *			VARF_ASSIGN	if we are in a := assignment
 *
 * Results:
 *	The resulting string.
 *
 * Side Effects:
 *	None.
 *-----------------------------------------------------------------------
 */
char *
Var_Subst(const char *var, const char *str, GNode *ctxt, Varf_Flags flags)
{
    Buffer	buf;		/* Buffer for forming things */
    char	*val;		/* Value to substitute for a variable */
    int		length;		/* Length of the variable invocation */
    Boolean	trailingBslash;	/* variable ends in \ */
    void	*freeIt = NULL;	/* Set if it should be freed */
    static Boolean errorReported; /* Set true if an error has already
				 * been reported to prevent a plethora
				 * of messages when recursing */

    Buf_Init(&buf, 0);
    errorReported = FALSE;
    trailingBslash = FALSE;

    while (*str) {
	if (*str == '\n' && trailingBslash)
	    Buf_AddByte(&buf, ' ');
	if (var == NULL && (*str == '$') && (str[1] == '$')) {
	    /*
	     * A dollar sign may be escaped either with another dollar sign.
	     * In such a case, we skip over the escape character and store the
	     * dollar sign into the buffer directly.
	     */
	    if (save_dollars && (flags & VARF_ASSIGN))
		Buf_AddByte(&buf, *str);
	    str++;
	    Buf_AddByte(&buf, *str);
	    str++;
	} else if (*str != '$') {
	    /*
	     * Skip as many characters as possible -- either to the end of
	     * the string or to the next dollar sign (variable invocation).
	     */
	    const char *cp;

	    for (cp = str++; *str != '$' && *str != '\0'; str++)
		continue;
	    Buf_AddBytes(&buf, str - cp, cp);
	} else {
	    if (var != NULL) {
		int expand;
		for (;;) {
		    if (str[1] == '\0') {
			/* A trailing $ is kind of a special case */
			Buf_AddByte(&buf, str[0]);
			str++;
			expand = FALSE;
		    } else if (str[1] != PROPEN && str[1] != BROPEN) {
			if (str[1] != *var || strlen(var) > 1) {
			    Buf_AddBytes(&buf, 2, str);
			    str += 2;
			    expand = FALSE;
			} else
			    expand = TRUE;
			break;
		    } else {
			const char *p;

			/* Scan up to the end of the variable name. */
			for (p = &str[2]; *p &&
			     *p != ':' && *p != PRCLOSE && *p != BRCLOSE; p++)
			    if (*p == '$')
				break;
			/*
			 * A variable inside the variable. We cannot expand
			 * the external variable yet, so we try again with
			 * the nested one
			 */
			if (*p == '$') {
			    Buf_AddBytes(&buf, p - str, str);
			    str = p;
			    continue;
			}

			if (strncmp(var, str + 2, p - str - 2) != 0 ||
			    var[p - str - 2] != '\0') {
			    /*
			     * Not the variable we want to expand, scan
			     * until the next variable
			     */
			    for (; *p != '$' && *p != '\0'; p++)
				continue;
			    Buf_AddBytes(&buf, p - str, str);
			    str = p;
			    expand = FALSE;
			} else
			    expand = TRUE;
			break;
		    }
		}
		if (!expand)
		    continue;
	    }

	    val = Var_Parse(str, ctxt, flags, &length, &freeIt);

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
		} else if ((flags & VARF_UNDEFERR) || val == var_Error) {
		    /*
		     * If variable is undefined, complain and skip the
		     * variable. The complaint will stop us from doing anything
		     * when the file is parsed.
		     */
		    if (!errorReported) {
			Parse_Error(PARSE_FATAL, "Undefined variable \"%.*s\"",
				    length, str);
		    }
		    str += length;
		    errorReported = TRUE;
		} else {
		    Buf_AddByte(&buf, *str);
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
		length = strlen(val);
		Buf_AddBytes(&buf, length, val);
		trailingBslash = length > 0 && val[length - 1] == '\\';
	    }
	    free(freeIt);
	    freeIt = NULL;
	}
    }

    return Buf_DestroyCompact(&buf);
}

/* Initialize the module. */
void
Var_Init(void)
{
    VAR_INTERNAL = Targ_NewGN("Internal");
    VAR_GLOBAL = Targ_NewGN("Global");
    VAR_CMD = Targ_NewGN("Command");
}


void
Var_End(void)
{
}


/****************** PRINT DEBUGGING INFO *****************/
static void
VarPrintVar(void *vp, void *data MAKE_ATTR_UNUSED)
{
    Var *v = (Var *)vp;
    fprintf(debug_file, "%-16s = %s\n", v->name, Buf_GetAll(&v->val, NULL));
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
    Hash_ForEach(&ctxt->context, VarPrintVar, NULL);
}
