/*	$NetBSD: var.c,v 1.484 2020/09/02 06:25:48 rillig Exp $	*/

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
static char rcsid[] = "$NetBSD: var.c,v 1.484 2020/09/02 06:25:48 rillig Exp $";
#else
#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)var.c	8.3 (Berkeley) 3/19/94";
#else
__RCSID("$NetBSD: var.c,v 1.484 2020/09/02 06:25:48 rillig Exp $");
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
 *			    yet exist.
 *
 *	Var_Append	    Append more characters to an existing variable
 *			    in the given context. The variable needn't
 *			    exist already -- it will be created if it doesn't.
 *			    A space is placed between the old value and the
 *			    new one.
 *
 *	Var_Exists	    See if a variable exists.
 *
 *	Var_Value 	    Return the unexpanded value of a variable in a
 *			    context or NULL if the variable is undefined.
 *
 *	Var_Subst 	    Substitute either a single variable or all
 *			    variables in a string, using the given context.
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
#include    <time.h>

#include    "make.h"

#ifdef HAVE_INTTYPES_H
#include    <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include    <stdint.h>
#endif

#include    "enum.h"
#include    "dir.h"
#include    "job.h"
#include    "metachar.h"

#define VAR_DEBUG_IF(cond, fmt, ...)	\
    if (!(DEBUG(VAR) && (cond)))	\
	(void) 0;			\
    else				\
	fprintf(debug_file, fmt, __VA_ARGS__)

#define VAR_DEBUG(fmt, ...) VAR_DEBUG_IF(TRUE, fmt, __VA_ARGS__)

ENUM_FLAGS_RTTI_3(VarEvalFlags,
		  VARE_UNDEFERR, VARE_WANTRES, VARE_ASSIGN);

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
 * Similar to var_Error, but returned when the 'VARE_UNDEFERR' flag for
 * Var_Parse is not set.
 *
 * Why not just use a constant? Well, GCC likes to condense identical string
 * instances...
 */
static char varNoError[] = "";

/*
 * Traditionally we consume $$ during := like any other expansion.
 * Other make's do not.
 * This knob allows controlling the behavior.
 * FALSE to consume $$ during := assignment.
 * TRUE to preserve $$ during := assignment.
 */
#define SAVE_DOLLARS ".MAKE.SAVE_DOLLARS"
static Boolean save_dollars = FALSE;

/*
 * Internally, variables are contained in four different contexts.
 *	1) the environment. They cannot be changed. If an environment
 *	    variable is appended to, the result is placed in the global
 *	    context.
 *	2) the global context. Variables set in the Makefile are located in
 *	    the global context.
 *	3) the command-line context. All variables set on the command line
 *	   are placed in this context. They are UNALTERABLE once placed here.
 *	4) the local context. Each target has associated with it a context
 *	   list. On this list are located the structures describing such
 *	   local variables as $(@) and $(*)
 * The four contexts are searched in the reverse order from which they are
 * listed (but see checkEnvFirst).
 */
GNode          *VAR_INTERNAL;	/* variables from make itself */
GNode          *VAR_GLOBAL;	/* variables from the makefile */
GNode          *VAR_CMD;	/* variables defined on the command-line */

typedef enum {
    FIND_CMD		= 0x01,	/* look in VAR_CMD when searching */
    FIND_GLOBAL		= 0x02,	/* look in VAR_GLOBAL as well */
    FIND_ENV		= 0x04	/* look in the environment also */
} VarFindFlags;

typedef enum {
    /* The variable's value is currently being used by Var_Parse or Var_Subst.
     * This marker is used to avoid endless recursion. */
    VAR_IN_USE = 0x01,
    /* The variable comes from the environment.
     * These variables are not registered in any GNode, therefore they must
     * be freed as soon as they are not used anymore. */
    VAR_FROM_ENV = 0x02,
    /* The variable is a junk variable that should be destroyed when done with
     * it.  Used by Var_Parse for undefined, modified variables. */
    VAR_JUNK = 0x04,
    /* Variable is VAR_JUNK, but we found a use for it in some modifier and
     * the value is therefore valid. */
    VAR_KEEP = 0x08,
    /* The variable is exported to the environment, to be used by child
     * processes. */
    VAR_EXPORTED = 0x10,
    /* At the point where this variable was exported, it contained an
     * unresolved reference to another variable.  Before any child process is
     * started, it needs to be exported again, in the hope that the referenced
     * variable can then be resolved. */
    VAR_REEXPORT = 0x20,
    /* The variable came from command line. */
    VAR_FROM_CMD = 0x40,
    VAR_READONLY = 0x80
} VarFlags;

ENUM_FLAGS_RTTI_8(VarFlags,
		  VAR_IN_USE, VAR_FROM_ENV, VAR_JUNK, VAR_KEEP,
		  VAR_EXPORTED, VAR_REEXPORT, VAR_FROM_CMD, VAR_READONLY);

typedef struct Var {
    char          *name;	/* the variable's name; it is allocated for
				 * environment variables and aliased to the
				 * Hash_Entry name for all other variables,
				 * and thus must not be modified */
    Buffer	  val;		/* its value */
    VarFlags	  flags;    	/* miscellaneous status flags */
} Var;

/*
 * Exporting vars is expensive so skip it if we can
 */
typedef enum {
    VAR_EXPORTED_NONE,
    VAR_EXPORTED_YES,
    VAR_EXPORTED_ALL
} VarExportedMode;

static VarExportedMode var_exportedVars = VAR_EXPORTED_NONE;

typedef enum {
    /*
     * We pass this to Var_Export when doing the initial export
     * or after updating an exported var.
     */
    VAR_EXPORT_PARENT	= 0x01,
    /*
     * We pass this to Var_Export1 to tell it to leave the value alone.
     */
    VAR_EXPORT_LITERAL	= 0x02
} VarExportFlags;

/* Flags for pattern matching in the :S and :C modifiers */
typedef enum {
    VARP_SUB_GLOBAL	= 0x01,	/* Apply substitution globally */
    VARP_SUB_ONE	= 0x02,	/* Apply substitution to one word */
    VARP_ANCHOR_START	= 0x04,	/* Match at start of word */
    VARP_ANCHOR_END	= 0x08	/* Match at end of word */
} VarPatternFlags;

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
 *	flags		FIND_GLOBAL	look in VAR_GLOBAL as well
 *			FIND_CMD	look in VAR_CMD as well
 *			FIND_ENV	look in the environment as well
 *
 * Results:
 *	A pointer to the structure describing the desired variable or
 *	NULL if the variable does not exist.
 *-----------------------------------------------------------------------
 */
static Var *
VarFind(const char *name, GNode *ctxt, VarFindFlags flags)
{
    Hash_Entry *var;

    /*
     * If the variable name begins with a '.', it could very well be one of
     * the local ones.  We check the name against all the local variables
     * and substitute the short version in for 'name' if it matches one of
     * them.
     */
    if (*name == '.' && isupper((unsigned char)name[1])) {
	switch (name[1]) {
	case 'A':
	    if (strcmp(name, ".ALLSRC") == 0)
		name = ALLSRC;
	    if (strcmp(name, ".ARCHIVE") == 0)
		name = ARCHIVE;
	    break;
	case 'I':
	    if (strcmp(name, ".IMPSRC") == 0)
		name = IMPSRC;
	    break;
	case 'M':
	    if (strcmp(name, ".MEMBER") == 0)
		name = MEMBER;
	    break;
	case 'O':
	    if (strcmp(name, ".OODATE") == 0)
		name = OODATE;
	    break;
	case 'P':
	    if (strcmp(name, ".PREFIX") == 0)
		name = PREFIX;
	    break;
	case 'S':
	    if (strcmp(name, ".SHELL") == 0 ) {
		if (!shellPath)
		    Shell_Init();
	    }
	    break;
	case 'T':
	    if (strcmp(name, ".TARGET") == 0)
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

    if (var == NULL && (flags & FIND_CMD) && ctxt != VAR_CMD)
	var = Hash_FindEntry(&VAR_CMD->context, name);

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
	    Var *v = bmake_malloc(sizeof(Var));
	    size_t len;
	    v->name = bmake_strdup(name);

	    len = strlen(env);
	    Buf_Init(&v->val, len + 1);
	    Buf_AddBytes(&v->val, env, len);

	    v->flags = VAR_FROM_ENV;
	    return v;
	}

	if (checkEnvFirst && (flags & FIND_GLOBAL) && ctxt != VAR_GLOBAL) {
	    var = Hash_FindEntry(&VAR_GLOBAL->context, name);
	    if (var == NULL && ctxt != VAR_INTERNAL)
		var = Hash_FindEntry(&VAR_INTERNAL->context, name);
	    if (var == NULL)
		return NULL;
	    else
		return (Var *)Hash_GetValue(var);
	}

	return NULL;
    }

    if (var == NULL)
	return NULL;
    else
	return (Var *)Hash_GetValue(var);
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
 *	TRUE if it is an environment variable, FALSE otherwise.
 *-----------------------------------------------------------------------
 */
static Boolean
VarFreeEnv(Var *v, Boolean destroy)
{
    if (!(v->flags & VAR_FROM_ENV))
	return FALSE;
    free(v->name);
    Buf_Destroy(&v->val, destroy);
    free(v);
    return TRUE;
}

/* Add a new variable of the given name and value to the given context.
 * The name and val arguments are duplicated so they may safely be freed. */
static void
VarAdd(const char *name, const char *val, GNode *ctxt, VarSet_Flags flags)
{
    Var *v = bmake_malloc(sizeof(Var));
    size_t len = strlen(val);
    Hash_Entry *he;

    Buf_Init(&v->val, len + 1);
    Buf_AddBytes(&v->val, val, len);

    v->flags = 0;
    if (flags & VAR_SET_READONLY)
	v->flags |= VAR_READONLY;

    he = Hash_CreateEntry(&ctxt->context, name, NULL);
    Hash_SetValue(he, v);
    v->name = he->name;
    VAR_DEBUG_IF(!(ctxt->flags & INTERNAL),
		 "%s:%s = %s\n", ctxt->name, name, val);
}

/* Remove a variable from a context, freeing the Var structure as well. */
void
Var_Delete(const char *name, GNode *ctxt)
{
    char *name_freeIt = NULL;
    Hash_Entry *he;

    if (strchr(name, '$') != NULL)
	name = name_freeIt = Var_Subst(name, VAR_GLOBAL, VARE_WANTRES);
    he = Hash_FindEntry(&ctxt->context, name);
    VAR_DEBUG("%s:delete %s%s\n",
	      ctxt->name, name, he != NULL ? "" : " (not found)");
    free(name_freeIt);

    if (he != NULL) {
	Var *v = (Var *)Hash_GetValue(he);
	if (v->flags & VAR_EXPORTED)
	    unsetenv(v->name);
	if (strcmp(v->name, MAKE_EXPORTED) == 0)
	    var_exportedVars = VAR_EXPORTED_NONE;
	if (v->name != he->name)
	    free(v->name);
	Hash_DeleteEntry(&ctxt->context, he);
	Buf_Destroy(&v->val, TRUE);
	free(v);
    }
}


/*
 * Export a single variable.
 * We ignore make internal variables (those which start with '.').
 * Also we jump through some hoops to avoid calling setenv
 * more than necessary since it can leak.
 * We only manipulate flags of vars if 'parent' is set.
 */
static Boolean
Var_Export1(const char *name, VarExportFlags flags)
{
    VarExportFlags parent = flags & VAR_EXPORT_PARENT;
    Var *v;
    char *val;

    if (name[0] == '.')
	return FALSE;		/* skip internals */
    if (name[1] == '\0') {
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
	    return FALSE;
	}
    }

    v = VarFind(name, VAR_GLOBAL, 0);
    if (v == NULL)
	return FALSE;

    if (!parent && (v->flags & VAR_EXPORTED) && !(v->flags & VAR_REEXPORT))
	return FALSE;		/* nothing to do */

    val = Buf_GetAll(&v->val, NULL);
    if (!(flags & VAR_EXPORT_LITERAL) && strchr(val, '$') != NULL) {
	char *expr;

	if (parent) {
	    /*
	     * Flag this as something we need to re-export.
	     * No point actually exporting it now though,
	     * the child can do it at the last minute.
	     */
	    v->flags |= VAR_EXPORTED | VAR_REEXPORT;
	    return TRUE;
	}
	if (v->flags & VAR_IN_USE) {
	    /*
	     * We recursed while exporting in a child.
	     * This isn't going to end well, just skip it.
	     */
	    return FALSE;
	}

	expr = str_concat3("${", name, "}");
	val = Var_Subst(expr, VAR_GLOBAL, VARE_WANTRES);
	setenv(name, val, 1);
	free(val);
	free(expr);
    } else {
	if (parent)
	    v->flags &= ~(unsigned)VAR_REEXPORT;	/* once will do */
	if (parent || !(v->flags & VAR_EXPORTED))
	    setenv(name, val, 1);
    }
    /*
     * This is so Var_Set knows to call Var_Export again...
     */
    if (parent) {
	v->flags |= VAR_EXPORTED;
    }
    return TRUE;
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
    char *val;

    /*
     * Several make's support this sort of mechanism for tracking
     * recursion - but each uses a different name.
     * We allow the makefiles to update MAKELEVEL and ensure
     * children see a correctly incremented value.
     */
    char tmp[BUFSIZ];
    snprintf(tmp, sizeof(tmp), "%d", makelevel + 1);
    setenv(MAKE_LEVEL_ENV, tmp, 1);

    if (var_exportedVars == VAR_EXPORTED_NONE)
	return;

    if (var_exportedVars == VAR_EXPORTED_ALL) {
	/* Ouch! This is crazy... */
	Hash_ForEach(&VAR_GLOBAL->context, Var_ExportVars_callback, NULL);
	return;
    }

    val = Var_Subst("${" MAKE_EXPORTED ":O:u}", VAR_GLOBAL, VARE_WANTRES);
    if (*val) {
        Words words = Str_Words(val, FALSE);
	size_t i;

	for (i = 0; i < words.len; i++)
	    Var_Export1(words.words[i], 0);
	Words_Free(words);
    }
    free(val);
}

/*
 * This is called when .export is seen or .MAKE.EXPORTED is modified.
 *
 * It is also called when any exported variable is modified.
 * XXX: Is it really?
 *
 * str has the format "[-env|-literal] varname...".
 */
void
Var_Export(const char *str, Boolean isExport)
{
    VarExportFlags flags;
    char *val;

    if (isExport && str[0] == '\0') {
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

    val = Var_Subst(str, VAR_GLOBAL, VARE_WANTRES);
    if (val[0] != '\0') {
        Words words = Str_Words(val, FALSE);

	size_t i;
	for (i = 0; i < words.len; i++) {
	    const char *name = words.words[i];
	    if (Var_Export1(name, flags)) {
		if (var_exportedVars != VAR_EXPORTED_ALL)
		    var_exportedVars = VAR_EXPORTED_YES;
		if (isExport && (flags & VAR_EXPORT_PARENT)) {
		    Var_Append(MAKE_EXPORTED, name, VAR_GLOBAL);
		}
	    }
	}
	Words_Free(words);
    }
    free(val);
}


extern char **environ;

/*
 * This is called when .unexport[-env] is seen.
 *
 * str must have the form "unexport[-env] varname...".
 */
void
Var_UnExport(const char *str)
{
    const char *varnames;
    char *varnames_freeIt;
    Boolean unexport_env;

    varnames = NULL;
    varnames_freeIt = NULL;

    str += strlen("unexport");
    unexport_env = strncmp(str, "-env", 4) == 0;
    if (unexport_env) {
	const char *cp;
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

	/* Note: we cannot safely free() the original environ. */
	environ = savedEnv = newenv;
	newenv[0] = NULL;
	newenv[1] = NULL;
	if (cp && *cp)
	    setenv(MAKE_LEVEL_ENV, cp, 1);
    } else {
	for (; isspace((unsigned char)*str); str++)
	    continue;
	if (str[0] != '\0')
	    varnames = str;
    }

    if (varnames == NULL) {
	/* Using .MAKE.EXPORTED */
	varnames = varnames_freeIt = Var_Subst("${" MAKE_EXPORTED ":O:u}",
					       VAR_GLOBAL, VARE_WANTRES);
    }

    {
	Var *v;
	size_t i;

	Words words = Str_Words(varnames, FALSE);
	for (i = 0; i < words.len; i++) {
	    const char *varname = words.words[i];
	    v = VarFind(varname, VAR_GLOBAL, 0);
	    if (v == NULL) {
		VAR_DEBUG("Not unexporting \"%s\" (not found)\n", varname);
		continue;
	    }

	    VAR_DEBUG("Unexporting \"%s\"\n", varname);
	    if (!unexport_env && (v->flags & VAR_EXPORTED) &&
		!(v->flags & VAR_REEXPORT))
		unsetenv(v->name);
	    v->flags &= ~(unsigned)(VAR_EXPORTED | VAR_REEXPORT);

	    /*
	     * If we are unexporting a list,
	     * remove each one from .MAKE.EXPORTED.
	     * If we are removing them all,
	     * just delete .MAKE.EXPORTED below.
	     */
	    if (varnames == str) {
		char *expr = str_concat3("${" MAKE_EXPORTED ":N", v->name, "}");
		char *cp = Var_Subst(expr, VAR_GLOBAL, VARE_WANTRES);
		Var_Set(MAKE_EXPORTED, cp, VAR_GLOBAL);
		free(cp);
		free(expr);
	    }
	}
	Words_Free(words);
	if (varnames != str) {
	    Var_Delete(MAKE_EXPORTED, VAR_GLOBAL);
	    free(varnames_freeIt);
	}
    }
}

/* See Var_Set for documentation. */
void
Var_Set_with_flags(const char *name, const char *val, GNode *ctxt,
		   VarSet_Flags flags)
{
    const char *unexpanded_name = name;
    char *name_freeIt = NULL;
    Var *v;

    assert(val != NULL);

    /*
     * We only look for a variable in the given context since anything set
     * here will override anything in a lower context, so there's not much
     * point in searching them all just to save a bit of memory...
     */
    if (strchr(name, '$') != NULL)
	name = name_freeIt = Var_Subst(name, ctxt, VARE_WANTRES);

    if (name[0] == '\0') {
	VAR_DEBUG("Var_Set(\"%s\", \"%s\", ...) "
		  "name expands to empty string - ignored\n",
		  unexpanded_name, val);
	free(name_freeIt);
	return;
    }

    if (ctxt == VAR_GLOBAL) {
	v = VarFind(name, VAR_CMD, 0);
	if (v != NULL) {
	    if (v->flags & VAR_FROM_CMD) {
		VAR_DEBUG("%s:%s = %s ignored!\n", ctxt->name, name, val);
		goto out;
	    }
	    VarFreeEnv(v, TRUE);
	}
    }

    v = VarFind(name, ctxt, 0);
    if (v == NULL) {
	if (ctxt == VAR_CMD && !(flags & VAR_NO_EXPORT)) {
	    /*
	     * This var would normally prevent the same name being added
	     * to VAR_GLOBAL, so delete it from there if needed.
	     * Otherwise -V name may show the wrong value.
	     */
	    Var_Delete(name, VAR_GLOBAL);
	}
	VarAdd(name, val, ctxt, flags);
    } else {
	if ((v->flags & VAR_READONLY) && !(flags & VAR_SET_READONLY)) {
	    VAR_DEBUG("%s:%s = %s ignored (read-only)\n",
	      ctxt->name, name, val);
	    goto out;
	}	    
	Buf_Empty(&v->val);
	if (val)
	    Buf_AddStr(&v->val, val);

	VAR_DEBUG("%s:%s = %s\n", ctxt->name, name, val);
	if (v->flags & VAR_EXPORTED) {
	    Var_Export1(name, VAR_EXPORT_PARENT);
	}
    }
    /*
     * Any variables given on the command line are automatically exported
     * to the environment (as per POSIX standard)
     * Other than internals.
     */
    if (ctxt == VAR_CMD && !(flags & VAR_NO_EXPORT) && name[0] != '.') {
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
	if (!varNoExportEnv)
	    setenv(name, val ? val : "", 1);

	Var_Append(MAKEOVERRIDES, name, VAR_GLOBAL);
    }
    if (name[0] == '.' && strcmp(name, SAVE_DOLLARS) == 0)
	save_dollars = s2Boolean(val, save_dollars);

out:
    free(name_freeIt);
    if (v != NULL)
	VarFreeEnv(v, TRUE);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Set --
 *	Set the variable name to the value val in the given context.
 *
 *	If the variable doesn't yet exist, it is created.
 *	Otherwise the new value overwrites and replaces the old value.
 *
 * Input:
 *	name		name of variable to set
 *	val		value to give to the variable
 *	ctxt		context in which to set it
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
 *	If the variable doesn't exist, it is created. Otherwise the strings
 *	are concatenated, with a space in between.
 *
 * Input:
 *	name		name of variable to modify
 *	val		string to append to it
 *	ctxt		context in which this should occur
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
    char *name_freeIt = NULL;
    Var *v;

    assert(val != NULL);

    if (strchr(name, '$') != NULL) {
	const char *unexpanded_name = name;
	name = name_freeIt = Var_Subst(name, ctxt, VARE_WANTRES);
	if (name[0] == '\0') {
	    VAR_DEBUG("Var_Append(\"%s\", \"%s\", ...) "
		      "name expands to empty string - ignored\n",
		      unexpanded_name, val);
	    free(name_freeIt);
	    return;
	}
    }

    v = VarFind(name, ctxt, ctxt == VAR_GLOBAL ? (FIND_CMD | FIND_ENV) : 0);

    if (v == NULL) {
	Var_Set(name, val, ctxt);
    } else if (ctxt == VAR_CMD || !(v->flags & VAR_FROM_CMD)) {
	Buf_AddByte(&v->val, ' ');
	Buf_AddStr(&v->val, val);

	VAR_DEBUG("%s:%s = %s\n", ctxt->name, name,
		  Buf_GetAll(&v->val, NULL));

	if (v->flags & VAR_FROM_ENV) {
	    Hash_Entry *h;

	    /*
	     * If the original variable came from the environment, we
	     * have to install it in the global context (we could place
	     * it in the environment, but then we should provide a way to
	     * export other variables...)
	     */
	    v->flags &= ~(unsigned)VAR_FROM_ENV;
	    h = Hash_CreateEntry(&ctxt->context, name, NULL);
	    Hash_SetValue(h, v);
	}
    }
    free(name_freeIt);
}

/* See if the given variable exists, in the given context or in other
 * fallback contexts.
 *
 * Input:
 *	name		Variable to find
 *	ctxt		Context in which to start search
 */
Boolean
Var_Exists(const char *name, GNode *ctxt)
{
    char *name_freeIt = NULL;
    Var *v;

    if (strchr(name, '$') != NULL)
	name = name_freeIt = Var_Subst(name, ctxt, VARE_WANTRES);

    v = VarFind(name, ctxt, FIND_CMD | FIND_GLOBAL | FIND_ENV);
    free(name_freeIt);
    if (v == NULL)
	return FALSE;

    (void)VarFreeEnv(v, TRUE);
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * Var_Value --
 *	Return the unexpanded value of the given variable in the given
 *	context, or the usual contexts.
 *
 * Input:
 *	name		name to find
 *	ctxt		context in which to search for it
 *
 * Results:
 *	The value if the variable exists, NULL if it doesn't.
 *	If the returned value is not NULL, the caller must free *freeIt
 *	as soon as the returned value is no longer needed.
 *-----------------------------------------------------------------------
 */
const char *
Var_Value(const char *name, GNode *ctxt, char **freeIt)
{
    Var *v = VarFind(name, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
    char *p;

    *freeIt = NULL;
    if (v == NULL)
	return NULL;

    p = Buf_GetAll(&v->val, NULL);
    if (VarFreeEnv(v, FALSE))
	*freeIt = p;
    return p;
}


/* SepBuf is a string being built from "words", interleaved with separators. */
typedef struct {
    Buffer buf;
    Boolean needSep;
    char sep;			/* usually ' ', but see the :ts modifier */
} SepBuf;

static void
SepBuf_Init(SepBuf *buf, char sep)
{
    Buf_Init(&buf->buf, 32 /* bytes */);
    buf->needSep = FALSE;
    buf->sep = sep;
}

static void
SepBuf_Sep(SepBuf *buf)
{
    buf->needSep = TRUE;
}

static void
SepBuf_AddBytes(SepBuf *buf, const char *mem, size_t mem_size)
{
    if (mem_size == 0)
	return;
    if (buf->needSep && buf->sep != '\0') {
	Buf_AddByte(&buf->buf, buf->sep);
	buf->needSep = FALSE;
    }
    Buf_AddBytes(&buf->buf, mem, mem_size);
}

static void
SepBuf_AddBytesBetween(SepBuf *buf, const char *start, const char *end)
{
    SepBuf_AddBytes(buf, start, (size_t)(end - start));
}

static void
SepBuf_AddStr(SepBuf *buf, const char *str)
{
    SepBuf_AddBytes(buf, str, strlen(str));
}

static char *
SepBuf_Destroy(SepBuf *buf, Boolean free_buf)
{
    return Buf_Destroy(&buf->buf, free_buf);
}


/* This callback for ModifyWords gets a single word from an expression and
 * typically adds a modification of this word to the buffer. It may also do
 * nothing or add several words. */
typedef void (*ModifyWordsCallback)(const char *word, SepBuf *buf, void *data);


/* Callback for ModifyWords to implement the :H modifier.
 * Add the dirname of the given word to the buffer. */
static void
ModifyWord_Head(const char *word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
    const char *slash = strrchr(word, '/');
    if (slash != NULL)
	SepBuf_AddBytesBetween(buf, word, slash);
    else
	SepBuf_AddStr(buf, ".");
}

/* Callback for ModifyWords to implement the :T modifier.
 * Add the basename of the given word to the buffer. */
static void
ModifyWord_Tail(const char *word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
    const char *slash = strrchr(word, '/');
    const char *base = slash != NULL ? slash + 1 : word;
    SepBuf_AddStr(buf, base);
}

/* Callback for ModifyWords to implement the :E modifier.
 * Add the filename suffix of the given word to the buffer, if it exists. */
static void
ModifyWord_Suffix(const char *word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
    const char *dot = strrchr(word, '.');
    if (dot != NULL)
	SepBuf_AddStr(buf, dot + 1);
}

/* Callback for ModifyWords to implement the :R modifier.
 * Add the basename of the given word to the buffer. */
static void
ModifyWord_Root(const char *word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
    const char *dot = strrchr(word, '.');
    size_t len = dot != NULL ? (size_t)(dot - word) : strlen(word);
    SepBuf_AddBytes(buf, word, len);
}

/* Callback for ModifyWords to implement the :M modifier.
 * Place the word in the buffer if it matches the given pattern. */
static void
ModifyWord_Match(const char *word, SepBuf *buf, void *data)
{
    const char *pattern = data;
    VAR_DEBUG("VarMatch [%s] [%s]\n", word, pattern);
    if (Str_Match(word, pattern))
	SepBuf_AddStr(buf, word);
}

/* Callback for ModifyWords to implement the :N modifier.
 * Place the word in the buffer if it doesn't match the given pattern. */
static void
ModifyWord_NoMatch(const char *word, SepBuf *buf, void *data)
{
    const char *pattern = data;
    if (!Str_Match(word, pattern))
	SepBuf_AddStr(buf, word);
}

#ifdef SYSVVARSUB
/*-
 *-----------------------------------------------------------------------
 * Str_SYSVMatch --
 *	Check word against pattern for a match (% is wild),
 *
 * Input:
 *	word		Word to examine
 *	pattern		Pattern to examine against
 *
 * Results:
 *	Returns the start of the match, or NULL.
 *	*match_len returns the length of the match, if any.
 *	*hasPercent returns whether the pattern contains a percent.
 *-----------------------------------------------------------------------
 */
static const char *
Str_SYSVMatch(const char *word, const char *pattern, size_t *match_len,
	      Boolean *hasPercent)
{
    const char *p = pattern;
    const char *w = word;
    const char *percent;
    size_t w_len;
    size_t p_len;
    const char *w_tail;

    *hasPercent = FALSE;
    if (*p == '\0') {		/* ${VAR:=suffix} */
	*match_len = strlen(w);	/* Null pattern is the whole string */
	return w;
    }

    percent = strchr(p, '%');
    if (percent != NULL) {	/* ${VAR:...%...=...} */
	*hasPercent = TRUE;
	if (*w == '\0')
	    return NULL;	/* empty word does not match pattern */

	/* check that the prefix matches */
	for (; p != percent && *w != '\0' && *w == *p; w++, p++)
	    continue;
	if (p != percent)
	    return NULL;	/* No match */

	p++;			/* Skip the percent */
	if (*p == '\0') {
	    /* No more pattern, return the rest of the string */
	    *match_len = strlen(w);
	    return w;
	}
    }

    /* Test whether the tail matches */
    w_len = strlen(w);
    p_len = strlen(p);
    if (w_len < p_len)
	return NULL;

    w_tail = w + w_len - p_len;
    if (memcmp(p, w_tail, p_len) != 0)
	return NULL;

    *match_len = (size_t)(w_tail - w);
    return w;
}

typedef struct {
    GNode *ctx;
    const char *lhs;
    const char *rhs;
} ModifyWord_SYSVSubstArgs;

/* Callback for ModifyWords to implement the :%.from=%.to modifier. */
static void
ModifyWord_SYSVSubst(const char *word, SepBuf *buf, void *data)
{
    const ModifyWord_SYSVSubstArgs *args = data;
    char *rhs_expanded;
    const char *rhs;
    const char *percent;

    size_t match_len;
    Boolean lhsPercent;
    const char *match = Str_SYSVMatch(word, args->lhs, &match_len, &lhsPercent);
    if (match == NULL) {
	SepBuf_AddStr(buf, word);
	return;
    }

    /* Append rhs to the buffer, substituting the first '%' with the
     * match, but only if the lhs had a '%' as well. */

    rhs_expanded = Var_Subst(args->rhs, args->ctx, VARE_WANTRES);

    rhs = rhs_expanded;
    percent = strchr(rhs, '%');

    if (percent != NULL && lhsPercent) {
	/* Copy the prefix of the replacement pattern */
	SepBuf_AddBytesBetween(buf, rhs, percent);
	rhs = percent + 1;
    }
    if (percent != NULL || !lhsPercent)
	SepBuf_AddBytes(buf, match, match_len);

    /* Append the suffix of the replacement pattern */
    SepBuf_AddStr(buf, rhs);

    free(rhs_expanded);
}
#endif


typedef struct {
    const char	*lhs;
    size_t	lhsLen;
    const char	*rhs;
    size_t	rhsLen;
    VarPatternFlags pflags;
    Boolean	matched;
} ModifyWord_SubstArgs;

/* Callback for ModifyWords to implement the :S,from,to, modifier.
 * Perform a string substitution on the given word. */
static void
ModifyWord_Subst(const char *word, SepBuf *buf, void *data)
{
    size_t wordLen = strlen(word);
    ModifyWord_SubstArgs *args = data;
    const char *match;

    if ((args->pflags & VARP_SUB_ONE) && args->matched)
	goto nosub;

    if (args->pflags & VARP_ANCHOR_START) {
	if (wordLen < args->lhsLen ||
	    memcmp(word, args->lhs, args->lhsLen) != 0)
	    goto nosub;

	if (args->pflags & VARP_ANCHOR_END) {
	    if (wordLen != args->lhsLen)
		goto nosub;

	    /* :S,^whole$,replacement, */
	    SepBuf_AddBytes(buf, args->rhs, args->rhsLen);
	    args->matched = TRUE;
	} else {
	    /* :S,^prefix,replacement, */
	    SepBuf_AddBytes(buf, args->rhs, args->rhsLen);
	    SepBuf_AddBytes(buf, word + args->lhsLen, wordLen - args->lhsLen);
	    args->matched = TRUE;
	}
	return;
    }

    if (args->pflags & VARP_ANCHOR_END) {
	const char *start;

	if (wordLen < args->lhsLen)
	    goto nosub;

	start = word + (wordLen - args->lhsLen);
	if (memcmp(start, args->lhs, args->lhsLen) != 0)
	    goto nosub;

	/* :S,suffix$,replacement, */
	SepBuf_AddBytesBetween(buf, word, start);
	SepBuf_AddBytes(buf, args->rhs, args->rhsLen);
	args->matched = TRUE;
	return;
    }

    /* unanchored case, may match more than once */
    while ((match = Str_FindSubstring(word, args->lhs)) != NULL) {
	SepBuf_AddBytesBetween(buf, word, match);
	SepBuf_AddBytes(buf, args->rhs, args->rhsLen);
	args->matched = TRUE;
	wordLen -= (size_t)(match - word) + args->lhsLen;
	word += (size_t)(match - word) + args->lhsLen;
	if (wordLen == 0 || !(args->pflags & VARP_SUB_GLOBAL))
	    break;
    }
nosub:
    SepBuf_AddBytes(buf, word, wordLen);
}

#ifndef NO_REGEX
/* Print the error caused by a regcomp or regexec call. */
static void
VarREError(int reerr, regex_t *pat, const char *str)
{
    size_t errlen = regerror(reerr, pat, 0, 0);
    char *errbuf = bmake_malloc(errlen);
    regerror(reerr, pat, errbuf, errlen);
    Error("%s: %s", str, errbuf);
    free(errbuf);
}

typedef struct {
    regex_t	   re;
    size_t	   nsub;
    char 	  *replace;
    VarPatternFlags pflags;
    Boolean	   matched;
} ModifyWord_SubstRegexArgs;

/* Callback for ModifyWords to implement the :C/from/to/ modifier.
 * Perform a regex substitution on the given word. */
static void
ModifyWord_SubstRegex(const char *word, SepBuf *buf, void *data)
{
    ModifyWord_SubstRegexArgs *args = data;
    int xrv;
    const char *wp = word;
    char *rp;
    int flags = 0;
    regmatch_t m[10];

    if ((args->pflags & VARP_SUB_ONE) && args->matched)
	goto nosub;

tryagain:
    xrv = regexec(&args->re, wp, args->nsub, m, flags);

    switch (xrv) {
    case 0:
	args->matched = TRUE;
	SepBuf_AddBytes(buf, wp, (size_t)m[0].rm_so);

	for (rp = args->replace; *rp; rp++) {
	    if (*rp == '\\' && (rp[1] == '&' || rp[1] == '\\')) {
		SepBuf_AddBytes(buf, rp + 1, 1);
		rp++;
		continue;
	    }

	    if (*rp == '&') {
		SepBuf_AddBytesBetween(buf, wp + m[0].rm_so, wp + m[0].rm_eo);
		continue;
	    }

	    if (*rp != '\\' || !isdigit((unsigned char)rp[1])) {
		SepBuf_AddBytes(buf, rp, 1);
		continue;
	    }

	    {			/* \0 to \9 backreference */
		size_t n = (size_t)(rp[1] - '0');
		rp++;

		if (n >= args->nsub) {
		    Error("No subexpression \\%zu", n);
		} else if (m[n].rm_so == -1 && m[n].rm_eo == -1) {
		    Error("No match for subexpression \\%zu", n);
		} else {
		    SepBuf_AddBytesBetween(buf, wp + m[n].rm_so,
					   wp + m[n].rm_eo);
		}
	    }
	}

	wp += m[0].rm_eo;
	if (args->pflags & VARP_SUB_GLOBAL) {
	    flags |= REG_NOTBOL;
	    if (m[0].rm_so == 0 && m[0].rm_eo == 0) {
		SepBuf_AddBytes(buf, wp, 1);
		wp++;
	    }
	    if (*wp)
		goto tryagain;
	}
	if (*wp) {
	    SepBuf_AddStr(buf, wp);
	}
	break;
    default:
	VarREError(xrv, &args->re, "Unexpected regex error");
	/* fall through */
    case REG_NOMATCH:
    nosub:
	SepBuf_AddStr(buf, wp);
	break;
    }
}
#endif


typedef struct {
    GNode	*ctx;
    char	*tvar;		/* name of temporary variable */
    char	*str;		/* string to expand */
    VarEvalFlags eflags;
} ModifyWord_LoopArgs;

/* Callback for ModifyWords to implement the :@var@...@ modifier of ODE make. */
static void
ModifyWord_Loop(const char *word, SepBuf *buf, void *data)
{
    const ModifyWord_LoopArgs *args;
    char *s;

    if (word[0] == '\0')
	return;

    args = data;
    Var_Set_with_flags(args->tvar, word, args->ctx, VAR_NO_EXPORT);
    s = Var_Subst(args->str, args->ctx, args->eflags);

    VAR_DEBUG("ModifyWord_Loop: in \"%s\", replace \"%s\" with \"%s\" "
	      "to \"%s\"\n",
	      word, args->tvar, args->str, s);

    if (s[0] == '\n' || (buf->buf.count > 0 &&
			 buf->buf.buffer[buf->buf.count - 1] == '\n'))
	buf->needSep = FALSE;
    SepBuf_AddStr(buf, s);
    free(s);
}


/*-
 * Implements the :[first..last] modifier.
 * This is a special case of ModifyWords since we want to be able
 * to scan the list backwards if first > last.
 */
static char *
VarSelectWords(char sep, Boolean oneBigWord, const char *str, int first,
	       int last)
{
    Words words;
    int start, end, step;
    int i;

    SepBuf buf;
    SepBuf_Init(&buf, sep);

    if (oneBigWord) {
	/* fake what Str_Words() would do if there were only one word */
	words.len = 1;
	words.words = bmake_malloc((words.len + 1) * sizeof(char *));
	words.freeIt = bmake_strdup(str);
	words.words[0] = words.freeIt;
	words.words[1] = NULL;
    } else {
	words = Str_Words(str, FALSE);
    }

    /*
     * Now sanitize the given range.
     * If first or last are negative, convert them to the positive equivalents
     * (-1 gets converted to ac, -2 gets converted to (ac - 1), etc.).
     */
    if (first < 0)
	first += (int)words.len + 1;
    if (last < 0)
	last += (int)words.len + 1;

    /*
     * We avoid scanning more of the list than we need to.
     */
    if (first > last) {
	start = MIN((int)words.len, first) - 1;
	end = MAX(0, last - 1);
	step = -1;
    } else {
	start = MAX(0, first - 1);
	end = MIN((int)words.len, last);
	step = 1;
    }

    for (i = start; (step < 0) == (i >= end); i += step) {
	SepBuf_AddStr(&buf, words.words[i]);
	SepBuf_Sep(&buf);
    }

    Words_Free(words);

    return SepBuf_Destroy(&buf, FALSE);
}


/* Callback for ModifyWords to implement the :tA modifier.
 * Replace each word with the result of realpath() if successful. */
static void
ModifyWord_Realpath(const char *word, SepBuf *buf, void *data MAKE_ATTR_UNUSED)
{
    struct stat st;
    char rbuf[MAXPATHLEN];

    const char *rp = cached_realpath(word, rbuf);
    if (rp != NULL && *rp == '/' && stat(rp, &st) == 0)
	word = rp;

    SepBuf_AddStr(buf, word);
}

/*-
 *-----------------------------------------------------------------------
 * Modify each of the words of the passed string using the given function.
 *
 * Input:
 *	str		String whose words should be modified
 *	modifyWord	Function that modifies a single word
 *	modifyWord_args Custom arguments for modifyWord
 *
 * Results:
 *	A string of all the words modified appropriately.
 *-----------------------------------------------------------------------
 */
static char *
ModifyWords(GNode *ctx, char sep, Boolean oneBigWord, const char *str,
	    ModifyWordsCallback modifyWord, void *modifyWord_args)
{
    SepBuf result;
    Words words;
    size_t i;

    if (oneBigWord) {
	SepBuf_Init(&result, sep);
	modifyWord(str, &result, modifyWord_args);
	return SepBuf_Destroy(&result, FALSE);
    }

    SepBuf_Init(&result, sep);

    words = Str_Words(str, FALSE);

    VAR_DEBUG("ModifyWords: split \"%s\" into %zu words\n", str, words.len);

    for (i = 0; i < words.len; i++) {
	modifyWord(words.words[i], &result, modifyWord_args);
	if (result.buf.count > 0)
	    SepBuf_Sep(&result);
    }

    Words_Free(words);

    return SepBuf_Destroy(&result, FALSE);
}


static char *
Words_JoinFree(Words words)
{
    Buffer buf;
    size_t i;

    Buf_Init(&buf, 0);

    for (i = 0; i < words.len; i++) {
	if (i != 0)
	    Buf_AddByte(&buf, ' ');	/* XXX: st->sep, for consistency */
	Buf_AddStr(&buf, words.words[i]);
    }

    Words_Free(words);

    return Buf_Destroy(&buf, FALSE);
}

/* Remove adjacent duplicate words. */
static char *
VarUniq(const char *str)
{
    Words words = Str_Words(str, FALSE);

    if (words.len > 1) {
	size_t i, j;
	for (j = 0, i = 1; i < words.len; i++)
	    if (strcmp(words.words[i], words.words[j]) != 0 && (++j != i))
		words.words[j] = words.words[i];
	words.len = j + 1;
    }

    return Words_JoinFree(words);
}


/*-
 * Parse a part of a modifier such as the "from" and "to" in :S/from/to/
 * or the "var" or "replacement" in :@var@replacement+${var}@, up to and
 * including the next unescaped delimiter.  The delimiter, as well as the
 * backslash or the dollar, can be escaped with a backslash.
 *
 * Return the parsed (and possibly expanded) string, or NULL if no delimiter
 * was found.  On successful return, the parsing position pp points right
 * after the delimiter.  The delimiter is not included in the returned
 * value though.
 */
static char *
ParseModifierPart(
    const char **pp,		/* The parsing position, updated upon return */
    int delim,			/* Parsing stops at this delimiter */
    VarEvalFlags eflags,	/* Flags for evaluating nested variables;
				 * if VARE_WANTRES is not set, the text is
				 * only parsed */
    GNode *ctxt,		/* For looking up nested variables */
    size_t *out_length,		/* Optionally stores the length of the returned
				 * string, just to save another strlen call. */
    VarPatternFlags *out_pflags,/* For the first part of the :S modifier,
				 * sets the VARP_ANCHOR_END flag if the last
				 * character of the pattern is a $. */
    ModifyWord_SubstArgs *subst	/* For the second part of the :S modifier,
				 * allow ampersands to be escaped and replace
				 * unescaped ampersands with subst->lhs. */
) {
    Buffer buf;
    const char *p;
    char *rstr;

    Buf_Init(&buf, 0);

    /*
     * Skim through until the matching delimiter is found;
     * pick up variable substitutions on the way. Also allow
     * backslashes to quote the delimiter, $, and \, but don't
     * touch other backslashes.
     */
    p = *pp;
    while (*p != '\0' && *p != delim) {
	const char *varstart;

	Boolean is_escaped = p[0] == '\\' && (
	    p[1] == delim || p[1] == '\\' || p[1] == '$' ||
	    (p[1] == '&' && subst != NULL));
	if (is_escaped) {
	    Buf_AddByte(&buf, p[1]);
	    p += 2;
	    continue;
	}

	if (*p != '$') {	/* Unescaped, simple text */
	    if (subst != NULL && *p == '&')
		Buf_AddBytes(&buf, subst->lhs, subst->lhsLen);
	    else
		Buf_AddByte(&buf, *p);
	    p++;
	    continue;
	}

	if (p[1] == delim) {	/* Unescaped $ at end of pattern */
	    if (out_pflags != NULL)
		*out_pflags |= VARP_ANCHOR_END;
	    else
		Buf_AddByte(&buf, *p);
	    p++;
	    continue;
	}

	if (eflags & VARE_WANTRES) {	/* Nested variable, evaluated */
	    const char *cp2;
	    int len;
	    void *freeIt;
	    VarEvalFlags nested_eflags = eflags & ~(unsigned)VARE_ASSIGN;

	    cp2 = Var_Parse(p, ctxt, nested_eflags, &len, &freeIt);
	    Buf_AddStr(&buf, cp2);
	    free(freeIt);
	    p += len;
	    continue;
	}

	/* XXX: This whole block is very similar to Var_Parse without
	 * VARE_WANTRES.  There may be subtle edge cases though that are
	 * not yet covered in the unit tests and that are parsed differently,
	 * depending on whether they are evaluated or not.
	 *
	 * This subtle difference is not documented in the manual page,
	 * neither is the difference between parsing :D and :M documented.
	 * No code should ever depend on these details, but who knows. */

	varstart = p;		/* Nested variable, only parsed */
	if (p[1] == PROPEN || p[1] == BROPEN) {
	    /*
	     * Find the end of this variable reference
	     * and suck it in without further ado.
	     * It will be interpreted later.
	     */
	    int have = p[1];
	    int want = have == PROPEN ? PRCLOSE : BRCLOSE;
	    int depth = 1;

	    for (p += 2; *p != '\0' && depth > 0; p++) {
		if (p[-1] != '\\') {
		    if (*p == have)
			depth++;
		    if (*p == want)
			depth--;
		}
	    }
	    Buf_AddBytesBetween(&buf, varstart, p);
	} else {
	    Buf_AddByte(&buf, *varstart);
	    p++;
	}
    }

    if (*p != delim) {
	*pp = p;
	return NULL;
    }

    *pp = ++p;
    if (out_length != NULL)
	*out_length = Buf_Size(&buf);

    rstr = Buf_Destroy(&buf, FALSE);
    VAR_DEBUG("Modifier part: \"%s\"\n", rstr);
    return rstr;
}

/* Quote shell meta-characters and space characters in the string.
 * If quoteDollar is set, also quote and double any '$' characters. */
static char *
VarQuote(const char *str, Boolean quoteDollar)
{
    char *res;
    Buffer buf;
    Buf_Init(&buf, 0);

    for (; *str != '\0'; str++) {
	if (*str == '\n') {
	    const char *newline = Shell_GetNewline();
	    if (newline == NULL)
		newline = "\\\n";
	    Buf_AddStr(&buf, newline);
	    continue;
	}
	if (isspace((unsigned char)*str) || ismeta((unsigned char)*str))
	    Buf_AddByte(&buf, '\\');
	Buf_AddByte(&buf, *str);
	if (quoteDollar && *str == '$')
	    Buf_AddStr(&buf, "\\$");
    }

    res = Buf_Destroy(&buf, FALSE);
    VAR_DEBUG("QuoteMeta: [%s]\n", res);
    return res;
}

/* Compute the 32-bit hash of the given string, using the MurmurHash3
 * algorithm. Output is encoded as 8 hex digits, in Little Endian order. */
static char *
VarHash(const char *str)
{
    static const char    hexdigits[16] = "0123456789abcdef";
    const unsigned char *ustr = (const unsigned char *)str;

    uint32_t h  = 0x971e137bU;
    uint32_t c1 = 0x95543787U;
    uint32_t c2 = 0x2ad7eb25U;
    size_t len2 = strlen(str);

    char *buf;
    size_t i;

    size_t len;
    for (len = len2; len; ) {
	uint32_t k = 0;
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
    h ^= (uint32_t)len2;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    buf = bmake_malloc(9);
    for (i = 0; i < 8; i++) {
	buf[i] = hexdigits[h & 0x0f];
	h >>= 4;
    }
    buf[8] = '\0';
    return buf;
}

static char *
VarStrftime(const char *fmt, Boolean zulu, time_t tim)
{
    char buf[BUFSIZ];

    if (!tim)
	time(&tim);
    if (!*fmt)
	fmt = "%c";
    strftime(buf, sizeof(buf), fmt, zulu ? gmtime(&tim) : localtime(&tim));

    buf[sizeof(buf) - 1] = '\0';
    return bmake_strdup(buf);
}

/* The ApplyModifier functions all work in the same way.  They get the
 * current parsing position (pp) and parse the modifier from there.  The
 * modifier typically lasts until the next ':', or a closing '}' or ')'
 * (taken from st->endc), or the end of the string (parse error).
 *
 * The high-level behavior of these functions is:
 *
 * 1. parse the modifier
 * 2. evaluate the modifier
 * 3. housekeeping
 *
 * Parsing the modifier
 *
 * If parsing succeeds, the parsing position *pp is updated to point to the
 * first character following the modifier, which typically is either ':' or
 * st->endc.
 *
 * If parsing fails because of a missing delimiter (as in the :S, :C or :@
 * modifiers), set st->missing_delim and return AMR_CLEANUP.
 *
 * If parsing fails because the modifier is unknown, return AMR_UNKNOWN to
 * try the SysV modifier ${VAR:from=to} as fallback.  This should only be
 * done as long as there have been no side effects from evaluating nested
 * variables, to avoid evaluating them more than once.  In this case, the
 * parsing position must not be updated.  (XXX: Why not? The original parsing
 * position is well-known in ApplyModifiers.)
 *
 * If parsing fails and the SysV modifier ${VAR:from=to} should not be used
 * as a fallback, either issue an error message using Error or Parse_Error
 * and then return AMR_CLEANUP, or return AMR_BAD for the default error
 * message.  Both of these return values will stop processing the variable
 * expression.  (XXX: As of 2020-08-23, evaluation of the whole string
 * continues nevertheless after skipping a few bytes, which essentially is
 * undefined behavior.  Not in the sense of C, but still it's impossible to
 * predict what happens in the parser.)
 *
 * Evaluating the modifier
 *
 * After parsing, the modifier is evaluated.  The side effects from evaluating
 * nested variable expressions in the modifier text often already happen
 * during parsing though.
 *
 * Evaluating the modifier usually takes the current value of the variable
 * expression from st->val, or the variable name from st->v->name and stores
 * the result in st->newVal.
 *
 * If evaluating fails (as of 2020-08-23), an error message is printed using
 * Error.  This function has no side-effects, it really just prints the error
 * message.  Processing the expression continues as if everything were ok.
 * XXX: This should be fixed by adding proper error handling to Var_Subst,
 * Var_Parse, ApplyModifiers and ModifyWords.
 *
 * Housekeeping
 *
 * Some modifiers such as :D and :U turn undefined variables into useful
 * variables (VAR_JUNK, VAR_KEEP).
 *
 * Some modifiers need to free some memory.
 */

typedef struct {
    const char startc;		/* '\0' or '{' or '(' */
    const char endc;		/* '\0' or '}' or ')' */
    Var * const v;
    GNode * const ctxt;
    const VarEvalFlags eflags;

    char *val;			/* The old value of the expression,
				 * before applying the modifier, never NULL */
    char *newVal;		/* The new value of the expression,
				 * after applying the modifier, never NULL */
    char missing_delim;		/* For error reporting */

    char sep;			/* Word separator in expansions
				 * (see the :ts modifier) */
    Boolean oneBigWord;		/* TRUE if some modifiers that otherwise split
				 * the variable value into words, like :S and
				 * :C, treat the variable value as a single big
				 * word, possibly containing spaces. */
} ApplyModifiersState;

typedef enum {
    AMR_OK,			/* Continue parsing */
    AMR_UNKNOWN,		/* Not a match, try other modifiers as well */
    AMR_BAD,			/* Error out with "Bad modifier" message */
    AMR_CLEANUP			/* Error out, with "Unfinished modifier"
				 * if st->missing_delim is set. */
} ApplyModifierResult;

/* Test whether mod starts with modname, followed by a delimiter. */
static Boolean
ModMatch(const char *mod, const char *modname, char endc)
{
    size_t n = strlen(modname);
    return strncmp(mod, modname, n) == 0 &&
	   (mod[n] == endc || mod[n] == ':');
}

/* Test whether mod starts with modname, followed by a delimiter or '='. */
static inline Boolean
ModMatchEq(const char *mod, const char *modname, char endc)
{
    size_t n = strlen(modname);
    return strncmp(mod, modname, n) == 0 &&
	   (mod[n] == endc || mod[n] == ':' || mod[n] == '=');
}

/* :@var@...${var}...@ */
static ApplyModifierResult
ApplyModifier_Loop(const char **pp, ApplyModifiersState *st)
{
    ModifyWord_LoopArgs args;
    char delim;
    char prev_sep;
    VarEvalFlags eflags = st->eflags & ~(unsigned)VARE_WANTRES;

    args.ctx = st->ctxt;

    (*pp)++;			/* Skip the first '@' */
    delim = '@';
    args.tvar = ParseModifierPart(pp, delim, eflags,
				  st->ctxt, NULL, NULL, NULL);
    if (args.tvar == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }
    if (DEBUG(LINT) && strchr(args.tvar, '$') != NULL) {
	Parse_Error(PARSE_FATAL,
		    "In the :@ modifier of \"%s\", the variable name \"%s\" "
		    "must not contain a dollar.",
		    st->v->name, args.tvar);
	return AMR_CLEANUP;
    }

    args.str = ParseModifierPart(pp, delim, eflags,
				 st->ctxt, NULL, NULL, NULL);
    if (args.str == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }

    args.eflags = st->eflags & (VARE_UNDEFERR | VARE_WANTRES);
    prev_sep = st->sep;
    st->sep = ' ';		/* XXX: should be st->sep for consistency */
    st->newVal = ModifyWords(st->ctxt, st->sep, st->oneBigWord, st->val,
			     ModifyWord_Loop, &args);
    st->sep = prev_sep;
    Var_Delete(args.tvar, st->ctxt);
    free(args.tvar);
    free(args.str);
    return AMR_OK;
}

/* :Ddefined or :Uundefined */
static ApplyModifierResult
ApplyModifier_Defined(const char **pp, ApplyModifiersState *st)
{
    Buffer buf;
    const char *p;

    VarEvalFlags eflags = st->eflags & ~(unsigned)VARE_WANTRES;
    if (st->eflags & VARE_WANTRES) {
	if ((**pp == 'D') == !(st->v->flags & VAR_JUNK))
	    eflags |= VARE_WANTRES;
    }

    Buf_Init(&buf, 0);
    p = *pp + 1;
    while (*p != st->endc && *p != ':' && *p != '\0') {

        /* Escaped delimiter or other special character */
	if (*p == '\\') {
	    char c = p[1];
	    if (c == st->endc || c == ':' || c == '$' || c == '\\') {
		Buf_AddByte(&buf, c);
		p += 2;
		continue;
	    }
	}

	/* Nested variable expression */
	if (*p == '$') {
	    const char *cp2;
	    int len;
	    void *freeIt;

	    cp2 = Var_Parse(p, st->ctxt, eflags, &len, &freeIt);
	    Buf_AddStr(&buf, cp2);
	    free(freeIt);
	    p += len;
	    continue;
	}

	/* Ordinary text */
	Buf_AddByte(&buf, *p);
	p++;
    }
    *pp = p;

    if (st->v->flags & VAR_JUNK)
	st->v->flags |= VAR_KEEP;
    if (eflags & VARE_WANTRES) {
	st->newVal = Buf_Destroy(&buf, FALSE);
    } else {
	st->newVal = st->val;
	Buf_Destroy(&buf, TRUE);
    }
    return AMR_OK;
}

/* :gmtime */
static ApplyModifierResult
ApplyModifier_Gmtime(const char **pp, ApplyModifiersState *st)
{
    time_t utc;

    const char *mod = *pp;
    if (!ModMatchEq(mod, "gmtime", st->endc))
	return AMR_UNKNOWN;

    if (mod[6] == '=') {
	char *ep;
	utc = (time_t)strtoul(mod + 7, &ep, 10);
	*pp = ep;
    } else {
	utc = 0;
	*pp = mod + 6;
    }
    st->newVal = VarStrftime(st->val, TRUE, utc);
    return AMR_OK;
}

/* :localtime */
static Boolean
ApplyModifier_Localtime(const char **pp, ApplyModifiersState *st)
{
    time_t utc;

    const char *mod = *pp;
    if (!ModMatchEq(mod, "localtime", st->endc))
	return AMR_UNKNOWN;

    if (mod[9] == '=') {
	char *ep;
	utc = (time_t)strtoul(mod + 10, &ep, 10);
	*pp = ep;
    } else {
	utc = 0;
	*pp = mod + 9;
    }
    st->newVal = VarStrftime(st->val, FALSE, utc);
    return AMR_OK;
}

/* :hash */
static ApplyModifierResult
ApplyModifier_Hash(const char **pp, ApplyModifiersState *st)
{
    if (!ModMatch(*pp, "hash", st->endc))
	return AMR_UNKNOWN;

    st->newVal = VarHash(st->val);
    *pp += 4;
    return AMR_OK;
}

/* :P */
static ApplyModifierResult
ApplyModifier_Path(const char **pp, ApplyModifiersState *st)
{
    GNode *gn;
    char *path;

    if (st->v->flags & VAR_JUNK)
	st->v->flags |= VAR_KEEP;

    gn = Targ_FindNode(st->v->name, TARG_NOCREATE);
    if (gn == NULL || gn->type & OP_NOPATH) {
	path = NULL;
    } else if (gn->path) {
	path = bmake_strdup(gn->path);
    } else {
	Lst searchPath = Suff_FindPath(gn);
	path = Dir_FindFile(st->v->name, searchPath);
    }
    if (path == NULL)
	path = bmake_strdup(st->v->name);
    st->newVal = path;

    (*pp)++;
    return AMR_OK;
}

/* :!cmd! */
static ApplyModifierResult
ApplyModifier_Exclam(const char **pp, ApplyModifiersState *st)
{
    char delim;
    char *cmd;
    const char *errfmt;

    (*pp)++;
    delim = '!';
    cmd = ParseModifierPart(pp, delim, st->eflags, st->ctxt,
			    NULL, NULL, NULL);
    if (cmd == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }

    errfmt = NULL;
    if (st->eflags & VARE_WANTRES)
	st->newVal = Cmd_Exec(cmd, &errfmt);
    else
	st->newVal = varNoError;
    free(cmd);

    if (errfmt != NULL)
	Error(errfmt, st->val);	/* XXX: why still return AMR_OK? */

    if (st->v->flags & VAR_JUNK)
	st->v->flags |= VAR_KEEP;
    return AMR_OK;
}

/* The :range modifier generates an integer sequence as long as the words.
 * The :range=7 modifier generates an integer sequence from 1 to 7. */
static ApplyModifierResult
ApplyModifier_Range(const char **pp, ApplyModifiersState *st)
{
    size_t n;
    Buffer buf;
    size_t i;

    const char *mod = *pp;
    if (!ModMatchEq(mod, "range", st->endc))
	return AMR_UNKNOWN;

    if (mod[5] == '=') {
	char *ep;
	n = (size_t)strtoul(mod + 6, &ep, 10);
	*pp = ep;
    } else {
	n = 0;
	*pp = mod + 5;
    }

    if (n == 0) {
        Words words = Str_Words(st->val, FALSE);
        n = words.len;
        Words_Free(words);
    }

    Buf_Init(&buf, 0);

    for (i = 0; i < n; i++) {
	if (i != 0)
	    Buf_AddByte(&buf, ' ');	/* XXX: st->sep, for consistency */
	Buf_AddInt(&buf, 1 + (int)i);
    }

    st->newVal = Buf_Destroy(&buf, FALSE);
    return AMR_OK;
}

/* :Mpattern or :Npattern */
static ApplyModifierResult
ApplyModifier_Match(const char **pp, ApplyModifiersState *st)
{
    const char *mod = *pp;
    Boolean copy = FALSE;	/* pattern should be, or has been, copied */
    Boolean needSubst = FALSE;
    const char *endpat;
    char *pattern;
    ModifyWordsCallback callback;

    /*
     * In the loop below, ignore ':' unless we are at (or back to) the
     * original brace level.
     * XXX This will likely not work right if $() and ${} are intermixed.
     */
    int nest = 0;
    const char *p;
    for (p = mod + 1; *p != '\0' && !(*p == ':' && nest == 0); p++) {
	if (*p == '\\' &&
	    (p[1] == ':' || p[1] == st->endc || p[1] == st->startc)) {
	    if (!needSubst)
		copy = TRUE;
	    p++;
	    continue;
	}
	if (*p == '$')
	    needSubst = TRUE;
	if (*p == '(' || *p == '{')
	    nest++;
	if (*p == ')' || *p == '}') {
	    nest--;
	    if (nest < 0)
		break;
	}
    }
    *pp = p;
    endpat = p;

    if (copy) {
	char *dst;
	const char *src;

	/* Compress the \:'s out of the pattern. */
	pattern = bmake_malloc((size_t)(endpat - (mod + 1)) + 1);
	dst = pattern;
	src = mod + 1;
	for (; src < endpat; src++, dst++) {
	    if (src[0] == '\\' && src + 1 < endpat &&
		/* XXX: st->startc is missing here; see above */
		(src[1] == ':' || src[1] == st->endc))
		src++;
	    *dst = *src;
	}
	*dst = '\0';
	endpat = dst;
    } else {
	pattern = bmake_strsedup(mod + 1, endpat);
    }

    if (needSubst) {
	/* pattern contains embedded '$', so use Var_Subst to expand it. */
	char *old_pattern = pattern;
	pattern = Var_Subst(pattern, st->ctxt, st->eflags);
	free(old_pattern);
    }

    VAR_DEBUG("Pattern[%s] for [%s] is [%s]\n", st->v->name, st->val, pattern);

    callback = mod[0] == 'M' ? ModifyWord_Match : ModifyWord_NoMatch;
    st->newVal = ModifyWords(st->ctxt, st->sep, st->oneBigWord, st->val,
			     callback, pattern);
    free(pattern);
    return AMR_OK;
}

/* :S,from,to, */
static ApplyModifierResult
ApplyModifier_Subst(const char **pp, ApplyModifiersState *st)
{
    ModifyWord_SubstArgs args;
    char *lhs, *rhs;
    Boolean oneBigWord;

    char delim = (*pp)[1];
    if (delim == '\0') {
	Error("Missing delimiter for :S modifier");
	(*pp)++;
	return AMR_CLEANUP;
    }

    *pp += 2;

    args.pflags = 0;
    args.matched = FALSE;

    /*
     * If pattern begins with '^', it is anchored to the
     * start of the word -- skip over it and flag pattern.
     */
    if (**pp == '^') {
	args.pflags |= VARP_ANCHOR_START;
	(*pp)++;
    }

    lhs = ParseModifierPart(pp, delim, st->eflags, st->ctxt,
			    &args.lhsLen, &args.pflags, NULL);
    if (lhs == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }
    args.lhs = lhs;

    rhs = ParseModifierPart(pp, delim, st->eflags, st->ctxt,
			    &args.rhsLen, NULL, &args);
    if (rhs == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }
    args.rhs = rhs;

    oneBigWord = st->oneBigWord;
    for (;; (*pp)++) {
	switch (**pp) {
	case 'g':
	    args.pflags |= VARP_SUB_GLOBAL;
	    continue;
	case '1':
	    args.pflags |= VARP_SUB_ONE;
	    continue;
	case 'W':
	    oneBigWord = TRUE;
	    continue;
	}
	break;
    }

    st->newVal = ModifyWords(st->ctxt, st->sep, oneBigWord, st->val,
			     ModifyWord_Subst, &args);

    free(lhs);
    free(rhs);
    return AMR_OK;
}

#ifndef NO_REGEX

/* :C,from,to, */
static ApplyModifierResult
ApplyModifier_Regex(const char **pp, ApplyModifiersState *st)
{
    char *re;
    ModifyWord_SubstRegexArgs args;
    Boolean oneBigWord;
    int error;

    char delim = (*pp)[1];
    if (delim == '\0') {
	Error("Missing delimiter for :C modifier");
	(*pp)++;
	return AMR_CLEANUP;
    }

    *pp += 2;

    re = ParseModifierPart(pp, delim, st->eflags, st->ctxt, NULL, NULL, NULL);
    if (re == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }

    args.replace = ParseModifierPart(pp, delim, st->eflags, st->ctxt,
				     NULL, NULL, NULL);
    if (args.replace == NULL) {
	free(re);
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }

    args.pflags = 0;
    args.matched = FALSE;
    oneBigWord = st->oneBigWord;
    for (;; (*pp)++) {
	switch (**pp) {
	case 'g':
	    args.pflags |= VARP_SUB_GLOBAL;
	    continue;
	case '1':
	    args.pflags |= VARP_SUB_ONE;
	    continue;
	case 'W':
	    oneBigWord = TRUE;
	    continue;
	}
	break;
    }

    error = regcomp(&args.re, re, REG_EXTENDED);
    free(re);
    if (error) {
	VarREError(error, &args.re, "Regex compilation error");
	free(args.replace);
	return AMR_CLEANUP;
    }

    args.nsub = args.re.re_nsub + 1;
    if (args.nsub > 10)
	args.nsub = 10;
    st->newVal = ModifyWords(st->ctxt, st->sep, oneBigWord, st->val,
			     ModifyWord_SubstRegex, &args);
    regfree(&args.re);
    free(args.replace);
    return AMR_OK;
}
#endif

static void
ModifyWord_Copy(const char *word, SepBuf *buf, void *data MAKE_ATTR_UNUSED)
{
    SepBuf_AddStr(buf, word);
}

/* :ts<separator> */
static ApplyModifierResult
ApplyModifier_ToSep(const char **pp, ApplyModifiersState *st)
{
    /* XXX: pp points to the 's', for historic reasons only.
     * Changing this will influence the error messages. */
    const char *sep = *pp + 1;

    /* ":ts<any><endc>" or ":ts<any>:" */
    if (sep[0] != st->endc && (sep[1] == st->endc || sep[1] == ':')) {
	st->sep = sep[0];
	*pp = sep + 1;
	goto ok;
    }

    /* ":ts<endc>" or ":ts:" */
    if (sep[0] == st->endc || sep[0] == ':') {
	st->sep = '\0';		/* no separator */
	*pp = sep;
	goto ok;
    }

    /* ":ts<unrecognised><unrecognised>". */
    if (sep[0] != '\\')
	return AMR_BAD;

    /* ":ts\n" */
    if (sep[1] == 'n') {
	st->sep = '\n';
	*pp = sep + 2;
	goto ok;
    }

    /* ":ts\t" */
    if (sep[1] == 't') {
	st->sep = '\t';
	*pp = sep + 2;
	goto ok;
    }

    /* ":ts\x40" or ":ts\100" */
    {
	const char *numStart = sep + 1;
	int base = 8;		/* assume octal */
	char *end;

	if (sep[1] == 'x') {
	    base = 16;
	    numStart++;
	} else if (!isdigit((unsigned char)sep[1]))
	    return AMR_BAD;	/* ":ts<backslash><unrecognised>". */

	st->sep = (char)strtoul(numStart, &end, base);
	if (*end != ':' && *end != st->endc)
	    return AMR_BAD;
	*pp = end;
    }

ok:
    st->newVal = ModifyWords(st->ctxt, st->sep, st->oneBigWord, st->val,
			     ModifyWord_Copy, NULL);
    return AMR_OK;
}

/* :tA, :tu, :tl, :ts<separator>, etc. */
static ApplyModifierResult
ApplyModifier_To(const char **pp, ApplyModifiersState *st)
{
    const char *mod = *pp;
    assert(mod[0] == 't');

    *pp = mod + 1;		/* make sure it is set */
    if (mod[1] == st->endc || mod[1] == ':' || mod[1] == '\0')
	return AMR_BAD;		/* Found ":t<endc>" or ":t:". */

    if (mod[1] == 's')
	return ApplyModifier_ToSep(pp, st);

    if (mod[2] != st->endc && mod[2] != ':')
	return AMR_BAD;		/* Found ":t<unrecognised><unrecognised>". */

    /* Check for two-character options: ":tu", ":tl" */
    if (mod[1] == 'A') {	/* absolute path */
	st->newVal = ModifyWords(st->ctxt, st->sep, st->oneBigWord, st->val,
				 ModifyWord_Realpath, NULL);
	*pp = mod + 2;
	return AMR_OK;
    }

    if (mod[1] == 'u') {
	size_t i;
	size_t len = strlen(st->val);
	st->newVal = bmake_malloc(len + 1);
	for (i = 0; i < len + 1; i++)
	    st->newVal[i] = (char)toupper((unsigned char)st->val[i]);
	*pp = mod + 2;
	return AMR_OK;
    }

    if (mod[1] == 'l') {
	size_t i;
	size_t len = strlen(st->val);
	st->newVal = bmake_malloc(len + 1);
	for (i = 0; i < len + 1; i++)
	    st->newVal[i] = (char)tolower((unsigned char)st->val[i]);
	*pp = mod + 2;
	return AMR_OK;
    }

    if (mod[1] == 'W' || mod[1] == 'w') {
	st->oneBigWord = mod[1] == 'W';
	st->newVal = st->val;
	*pp = mod + 2;
	return AMR_OK;
    }

    /* Found ":t<unrecognised>:" or ":t<unrecognised><endc>". */
    return AMR_BAD;
}

/* :[#], :[1], etc. */
static ApplyModifierResult
ApplyModifier_Words(const char **pp, ApplyModifiersState *st)
{
    char delim;
    char *estr;
    char *ep;
    int first, last;

    (*pp)++;			/* skip the '[' */
    delim = ']';		/* look for closing ']' */
    estr = ParseModifierPart(pp, delim, st->eflags, st->ctxt,
			     NULL, NULL, NULL);
    if (estr == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }

    /* now *pp points just after the closing ']' */
    if (**pp != ':' && **pp != st->endc)
	goto bad_modifier;	/* Found junk after ']' */

    if (estr[0] == '\0')
	goto bad_modifier;	/* empty square brackets in ":[]". */

    if (estr[0] == '#' && estr[1] == '\0') { /* Found ":[#]" */
	if (st->oneBigWord) {
	    st->newVal = bmake_strdup("1");
	} else {
	    Buffer buf;

	    Words words = Str_Words(st->val, FALSE);
	    size_t ac = words.len;
	    Words_Free(words);
	    	
	    Buf_Init(&buf, 4);	/* 3 digits + '\0' is usually enough */
	    Buf_AddInt(&buf, (int)ac);
	    st->newVal = Buf_Destroy(&buf, FALSE);
	}
	goto ok;
    }

    if (estr[0] == '*' && estr[1] == '\0') {
	/* Found ":[*]" */
	st->oneBigWord = TRUE;
	st->newVal = st->val;
	goto ok;
    }

    if (estr[0] == '@' && estr[1] == '\0') {
	/* Found ":[@]" */
	st->oneBigWord = FALSE;
	st->newVal = st->val;
	goto ok;
    }

    /*
     * We expect estr to contain a single integer for :[N], or two integers
     * separated by ".." for :[start..end].
     */
    first = (int)strtol(estr, &ep, 0);
    if (ep == estr)		/* Found junk instead of a number */
	goto bad_modifier;

    if (ep[0] == '\0') {	/* Found only one integer in :[N] */
	last = first;
    } else if (ep[0] == '.' && ep[1] == '.' && ep[2] != '\0') {
	/* Expecting another integer after ".." */
	ep += 2;
	last = (int)strtol(ep, &ep, 0);
	if (ep[0] != '\0')	/* Found junk after ".." */
	    goto bad_modifier;
    } else
	goto bad_modifier;	/* Found junk instead of ".." */

    /*
     * Now seldata is properly filled in, but we still have to check for 0 as
     * a special case.
     */
    if (first == 0 && last == 0) {
	/* ":[0]" or perhaps ":[0..0]" */
	st->oneBigWord = TRUE;
	st->newVal = st->val;
	goto ok;
    }

    /* ":[0..N]" or ":[N..0]" */
    if (first == 0 || last == 0)
	goto bad_modifier;

    /* Normal case: select the words described by seldata. */
    st->newVal = VarSelectWords(st->sep, st->oneBigWord, st->val, first, last);

ok:
    free(estr);
    return AMR_OK;

bad_modifier:
    free(estr);
    return AMR_BAD;
}

static int
str_cmp_asc(const void *a, const void *b)
{
    return strcmp(*(const char * const *)a, *(const char * const *)b);
}

static int
str_cmp_desc(const void *a, const void *b)
{
    return strcmp(*(const char * const *)b, *(const char * const *)a);
}

/* :O (order ascending) or :Or (order descending) or :Ox (shuffle) */
static ApplyModifierResult
ApplyModifier_Order(const char **pp, ApplyModifiersState *st)
{
    const char *mod = (*pp)++;	/* skip past the 'O' in any case */

    Words words = Str_Words(st->val, FALSE);

    if (mod[1] == st->endc || mod[1] == ':') {
	/* :O sorts ascending */
	qsort(words.words, words.len, sizeof(char *), str_cmp_asc);

    } else if ((mod[1] == 'r' || mod[1] == 'x') &&
	       (mod[2] == st->endc || mod[2] == ':')) {
	(*pp)++;

	if (mod[1] == 'r') {
	    /* :Or sorts descending */
	    qsort(words.words, words.len, sizeof(char *), str_cmp_desc);

	} else {
	    /* :Ox shuffles
	     *
	     * We will use [ac..2] range for mod factors. This will produce
	     * random numbers in [(ac-1)..0] interval, and minimal
	     * reasonable value for mod factor is 2 (the mod 1 will produce
	     * 0 with probability 1).
	     */
	    size_t i;
	    for (i = words.len - 1; i > 0; i--) {
		size_t rndidx = (size_t)random() % (i + 1);
		char *t = words.words[i];
		words.words[i] = words.words[rndidx];
		words.words[rndidx] = t;
	    }
	}
    } else {
	Words_Free(words);
	return AMR_BAD;
    }

    st->newVal = Words_JoinFree(words);
    return AMR_OK;
}

/* :? then : else */
static ApplyModifierResult
ApplyModifier_IfElse(const char **pp, ApplyModifiersState *st)
{
    char delim;
    char *then_expr, *else_expr;

    Boolean value = FALSE;
    VarEvalFlags then_eflags = st->eflags & ~(unsigned)VARE_WANTRES;
    VarEvalFlags else_eflags = st->eflags & ~(unsigned)VARE_WANTRES;

    int cond_rc = COND_PARSE;	/* anything other than COND_INVALID */
    if (st->eflags & VARE_WANTRES) {
	cond_rc = Cond_EvalExpression(NULL, st->v->name, &value, 0, FALSE);
	if (cond_rc != COND_INVALID && value)
	    then_eflags |= VARE_WANTRES;
	if (cond_rc != COND_INVALID && !value)
	    else_eflags |= VARE_WANTRES;
    }

    (*pp)++;			/* skip past the '?' */
    delim = ':';
    then_expr = ParseModifierPart(pp, delim, then_eflags, st->ctxt,
				  NULL, NULL, NULL);
    if (then_expr == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }

    delim = st->endc;		/* BRCLOSE or PRCLOSE */
    else_expr = ParseModifierPart(pp, delim, else_eflags, st->ctxt,
				  NULL, NULL, NULL);
    if (else_expr == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }

    (*pp)--;
    if (cond_rc == COND_INVALID) {
	Error("Bad conditional expression `%s' in %s?%s:%s",
	      st->v->name, st->v->name, then_expr, else_expr);
	return AMR_CLEANUP;
    }

    if (value) {
	st->newVal = then_expr;
	free(else_expr);
    } else {
	st->newVal = else_expr;
	free(then_expr);
    }
    if (st->v->flags & VAR_JUNK)
	st->v->flags |= VAR_KEEP;
    return AMR_OK;
}

/*
 * The ::= modifiers actually assign a value to the variable.
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
static ApplyModifierResult
ApplyModifier_Assign(const char **pp, ApplyModifiersState *st)
{
    GNode *v_ctxt;
    char *sv_name;
    char delim;
    char *val;

    const char *mod = *pp;
    const char *op = mod + 1;
    if (!(op[0] == '=' ||
	  (op[1] == '=' &&
	   (op[0] == '!' || op[0] == '+' || op[0] == '?'))))
	return AMR_UNKNOWN;	/* "::<unrecognised>" */


    if (st->v->name[0] == 0) {
	*pp = mod + 1;
	return AMR_BAD;
    }

    v_ctxt = st->ctxt;		/* context where v belongs */
    sv_name = NULL;
    if (st->v->flags & VAR_JUNK) {
	/*
	 * We need to bmake_strdup() it in case ParseModifierPart() recurses.
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

    switch (op[0]) {
    case '+':
    case '?':
    case '!':
	*pp = mod + 3;
	break;
    default:
	*pp = mod + 2;
	break;
    }

    delim = st->startc == PROPEN ? PRCLOSE : BRCLOSE;
    val = ParseModifierPart(pp, delim, st->eflags, st->ctxt, NULL, NULL, NULL);
    if (st->v->flags & VAR_JUNK) {
	/* restore original name */
	free(st->v->name);
	st->v->name = sv_name;
    }
    if (val == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }

    (*pp)--;

    if (st->eflags & VARE_WANTRES) {
	switch (op[0]) {
	case '+':
	    Var_Append(st->v->name, val, v_ctxt);
	    break;
	case '!': {
	    const char *errfmt;
	    char *cmd_output = Cmd_Exec(val, &errfmt);
	    if (errfmt)
		Error(errfmt, val);
	    else
		Var_Set(st->v->name, cmd_output, v_ctxt);
	    free(cmd_output);
	    break;
	}
	case '?':
	    if (!(st->v->flags & VAR_JUNK))
		break;
	    /* FALLTHROUGH */
	default:
	    Var_Set(st->v->name, val, v_ctxt);
	    break;
	}
    }
    free(val);
    st->newVal = varNoError;	/* XXX: varNoError is kind of an error,
				 * the intention here is to just return
				 * an empty string. */
    return AMR_OK;
}

/* remember current value */
static ApplyModifierResult
ApplyModifier_Remember(const char **pp, ApplyModifiersState *st)
{
    const char *mod = *pp;
    if (!ModMatchEq(mod, "_", st->endc))
	return AMR_UNKNOWN;

    if (mod[1] == '=') {
	size_t n = strcspn(mod + 2, ":)}");
	char *name = bmake_strldup(mod + 2, n);
	Var_Set(name, st->val, st->ctxt);
	free(name);
	*pp = mod + 2 + n;
    } else {
	Var_Set("_", st->val, st->ctxt);
	*pp = mod + 1;
    }
    st->newVal = st->val;
    return AMR_OK;
}

/* Apply the given function to each word of the variable value. */
static ApplyModifierResult
ApplyModifier_WordFunc(const char **pp, ApplyModifiersState *st,
		       ModifyWordsCallback modifyWord)
{
    char delim = (*pp)[1];
    if (delim != st->endc && delim != ':')
	return AMR_UNKNOWN;

    st->newVal = ModifyWords(st->ctxt, st->sep, st->oneBigWord,
			    st->val, modifyWord, NULL);
    (*pp)++;
    return AMR_OK;
}

#ifdef SYSVVARSUB
/* :from=to */
static ApplyModifierResult
ApplyModifier_SysV(const char **pp, ApplyModifiersState *st)
{
    char delim;
    char *lhs, *rhs;

    const char *mod = *pp;
    Boolean eqFound = FALSE;

    /*
     * First we make a pass through the string trying
     * to verify it is a SYSV-make-style translation:
     * it must be: <string1>=<string2>)
     */
    int nest = 1;
    const char *next = mod;
    while (*next != '\0' && nest > 0) {
	if (*next == '=') {
	    eqFound = TRUE;
	    /* continue looking for st->endc */
	} else if (*next == st->endc)
	    nest--;
	else if (*next == st->startc)
	    nest++;
	if (nest > 0)
	    next++;
    }
    if (*next != st->endc || !eqFound)
	return AMR_UNKNOWN;

    delim = '=';
    *pp = mod;
    lhs = ParseModifierPart(pp, delim, st->eflags, st->ctxt, NULL, NULL, NULL);
    if (lhs == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }

    delim = st->endc;
    rhs = ParseModifierPart(pp, delim, st->eflags, st->ctxt, NULL, NULL, NULL);
    if (rhs == NULL) {
	st->missing_delim = delim;
	return AMR_CLEANUP;
    }

    /*
     * SYSV modifications happen through the whole
     * string. Note the pattern is anchored at the end.
     */
    (*pp)--;
    if (lhs[0] == '\0' && *st->val == '\0') {
	st->newVal = st->val;	/* special case */
    } else {
	ModifyWord_SYSVSubstArgs args = {st->ctxt, lhs, rhs};
	st->newVal = ModifyWords(st->ctxt, st->sep, st->oneBigWord, st->val,
				 ModifyWord_SYSVSubst, &args);
    }
    free(lhs);
    free(rhs);
    return AMR_OK;
}
#endif

/* Apply any modifiers (such as :Mpattern or :@var@loop@ or :Q or ::=value). */
static char *
ApplyModifiers(
    const char **pp,		/* the parsing position, updated upon return */
    char *val,			/* the current value of the variable */
    char const startc,		/* '(' or '{', or '\0' for indirect modifiers */
    char const endc,		/* ')' or '}', or '\0' for indirect modifiers */
    Var * const v,		/* the variable may have its flags changed */
    GNode * const ctxt,		/* for looking up and modifying variables */
    VarEvalFlags const eflags,
    void ** const freePtr	/* free this after using the return value */
) {
    ApplyModifiersState st = {
	startc, endc, v, ctxt, eflags, val,
	var_Error,		/* .newVal */
	'\0',			/* .missing_delim */
	' ',			/* .sep */
	FALSE			/* .oneBigWord */
    };
    const char *p;
    const char *mod;
    ApplyModifierResult res;

    assert(startc == '(' || startc == '{' || startc == '\0');
    assert(endc == ')' || endc == '}' || endc == '\0');
    assert(val != NULL);

    p = *pp;
    while (*p != '\0' && *p != endc) {

	if (*p == '$') {
	    /*
	     * We may have some complex modifiers in a variable.
	     */
	    int rlen;
	    void *freeIt;
	    const char *rval = Var_Parse(p, st.ctxt, st.eflags, &rlen, &freeIt);

	    /*
	     * If we have not parsed up to st.endc or ':',
	     * we are not interested.
	     */
	    int c;
	    if (rval[0] != '\0' &&
		(c = p[rlen]) != '\0' && c != ':' && c != st.endc) {
		free(freeIt);
		goto apply_mods;
	    }

	    VAR_DEBUG("Indirect modifier \"%s\" from \"%.*s\"\n",
		      rval, rlen, p);

	    p += rlen;

	    if (rval[0] != '\0') {
		const char *rval_pp = rval;
		st.val = ApplyModifiers(&rval_pp, st.val, '\0', '\0', v,
					ctxt, eflags, freePtr);
		if (st.val == var_Error
		    || (st.val == varNoError && !(st.eflags & VARE_UNDEFERR))
		    || *rval_pp != '\0') {
		    free(freeIt);
		    goto out;	/* error already reported */
		}
	    }
	    free(freeIt);
	    if (*p == ':')
		p++;
	    else if (*p == '\0' && endc != '\0') {
		Error("Unclosed variable specification after complex "
		      "modifier (expecting '%c') for %s", st.endc, st.v->name);
		goto out;
	    }
	    continue;
	}
    apply_mods:
	st.newVal = var_Error;	/* default value, in case of errors */
	res = AMR_BAD;		/* just a safe fallback */
	mod = p;

	if (DEBUG(VAR)) {
	    char eflags_str[VarEvalFlags_ToStringSize];
	    char vflags_str[VarFlags_ToStringSize];
	    Boolean is_single_char = mod[0] != '\0' &&
		(mod[1] == endc || mod[1] == ':');

	    /* At this point, only the first character of the modifier can
	     * be used since the end of the modifier is not yet known. */
	    VAR_DEBUG("Applying ${%s:%c%s} to \"%s\" "
		      "(eflags = %s, vflags = %s)\n",
		      st.v->name, mod[0], is_single_char ? "" : "...", st.val,
		      Enum_FlagsToString(eflags_str, sizeof eflags_str,
					 st.eflags, VarEvalFlags_ToStringSpecs),
		      Enum_FlagsToString(vflags_str, sizeof vflags_str,
					 st.v->flags, VarFlags_ToStringSpecs));
	}

	switch (*mod) {
	case ':':
	    res = ApplyModifier_Assign(&p, &st);
	    break;
	case '@':
	    res = ApplyModifier_Loop(&p, &st);
	    break;
	case '_':
	    res = ApplyModifier_Remember(&p, &st);
	    break;
	case 'D':
	case 'U':
	    res = ApplyModifier_Defined(&p, &st);
	    break;
	case 'L':
	    if (st.v->flags & VAR_JUNK)
		st.v->flags |= VAR_KEEP;
	    st.newVal = bmake_strdup(st.v->name);
	    p++;
	    res = AMR_OK;
	    break;
	case 'P':
	    res = ApplyModifier_Path(&p, &st);
	    break;
	case '!':
	    res = ApplyModifier_Exclam(&p, &st);
	    break;
	case '[':
	    res = ApplyModifier_Words(&p, &st);
	    break;
	case 'g':
	    res = ApplyModifier_Gmtime(&p, &st);
	    break;
	case 'h':
	    res = ApplyModifier_Hash(&p, &st);
	    break;
	case 'l':
	    res = ApplyModifier_Localtime(&p, &st);
	    break;
	case 't':
	    res = ApplyModifier_To(&p, &st);
	    break;
	case 'N':
	case 'M':
	    res = ApplyModifier_Match(&p, &st);
	    break;
	case 'S':
	    res = ApplyModifier_Subst(&p, &st);
	    break;
	case '?':
	    res = ApplyModifier_IfElse(&p, &st);
	    break;
#ifndef NO_REGEX
	case 'C':
	    res = ApplyModifier_Regex(&p, &st);
	    break;
#endif
	case 'q':
	case 'Q':
	    if (p[1] == st.endc || p[1] == ':') {
		st.newVal = VarQuote(st.val, *mod == 'q');
		p++;
		res = AMR_OK;
	    } else
		res = AMR_UNKNOWN;
	    break;
	case 'T':
	    res = ApplyModifier_WordFunc(&p, &st, ModifyWord_Tail);
	    break;
	case 'H':
	    res = ApplyModifier_WordFunc(&p, &st, ModifyWord_Head);
	    break;
	case 'E':
	    res = ApplyModifier_WordFunc(&p, &st, ModifyWord_Suffix);
	    break;
	case 'R':
	    res = ApplyModifier_WordFunc(&p, &st, ModifyWord_Root);
	    break;
	case 'r':
	    res = ApplyModifier_Range(&p, &st);
	    break;
	case 'O':
	    res = ApplyModifier_Order(&p, &st);
	    break;
	case 'u':
	    if (p[1] == st.endc || p[1] == ':') {
		st.newVal = VarUniq(st.val);
		p++;
		res = AMR_OK;
	    } else
		res = AMR_UNKNOWN;
	    break;
#ifdef SUNSHCMD
	case 's':
	    if (p[1] == 'h' && (p[2] == st.endc || p[2] == ':')) {
		if (st.eflags & VARE_WANTRES) {
		    const char *errfmt;
		    st.newVal = Cmd_Exec(st.val, &errfmt);
		    if (errfmt)
			Error(errfmt, st.val);
		} else
		    st.newVal = varNoError;
		p += 2;
		res = AMR_OK;
	    } else
		res = AMR_UNKNOWN;
	    break;
#endif
	default:
	    res = AMR_UNKNOWN;
	}

#ifdef SYSVVARSUB
	if (res == AMR_UNKNOWN) {
	    assert(p == mod);
	    res = ApplyModifier_SysV(&p, &st);
	}
#endif

	if (res == AMR_UNKNOWN) {
	    Error("Unknown modifier '%c'", *mod);
	    for (p++; *p != ':' && *p != st.endc && *p != '\0'; p++)
		continue;
	    st.newVal = var_Error;
	}
	if (res == AMR_CLEANUP)
	    goto cleanup;
	if (res == AMR_BAD)
	    goto bad_modifier;

	if (DEBUG(VAR)) {
	    char eflags_str[VarEvalFlags_ToStringSize];
	    char vflags_str[VarFlags_ToStringSize];
	    const char *quot = st.newVal == var_Error ? "" : "\"";
	    const char *newVal = st.newVal == var_Error ? "error" : st.newVal;

	    VAR_DEBUG("Result of ${%s:%.*s} is %s%s%s "
		      "(eflags = %s, vflags = %s)\n",
		      st.v->name, (int)(p - mod), mod, quot, newVal, quot,
		      Enum_FlagsToString(eflags_str, sizeof eflags_str,
					 st.eflags, VarEvalFlags_ToStringSpecs),
		      Enum_FlagsToString(vflags_str, sizeof vflags_str,
					 st.v->flags, VarFlags_ToStringSpecs));
	}

	if (st.newVal != st.val) {
	    if (*freePtr) {
		free(st.val);
		*freePtr = NULL;
	    }
	    st.val = st.newVal;
	    if (st.val != var_Error && st.val != varNoError) {
		*freePtr = st.val;
	    }
	}
	if (*p == '\0' && st.endc != '\0') {
	    Error("Unclosed variable specification (expecting '%c') "
		  "for \"%s\" (value \"%s\") modifier %c",
		  st.endc, st.v->name, st.val, *mod);
	} else if (*p == ':') {
	    p++;
	}
	mod = p;
    }
out:
    *pp = p;
    assert(st.val != NULL);	/* Use var_Error or varNoError instead. */
    return st.val;

bad_modifier:
    Error("Bad modifier `:%.*s' for %s",
	  (int)strcspn(mod, ":)}"), mod, st.v->name);

cleanup:
    *pp = p;
    if (st.missing_delim != '\0')
	Error("Unfinished modifier for %s ('%c' missing)",
	      st.v->name, st.missing_delim);
    free(*freePtr);
    *freePtr = NULL;
    return var_Error;
}

static Boolean
VarIsDynamic(GNode *ctxt, const char *varname, size_t namelen)
{
    if ((namelen == 1 ||
	 (namelen == 2 && (varname[1] == 'F' || varname[1] == 'D'))) &&
	(ctxt == VAR_CMD || ctxt == VAR_GLOBAL))
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
	switch (varname[0]) {
	case '@':
	case '%':
	case '*':
	case '!':
	    return TRUE;
	}
	return FALSE;
    }

    if ((namelen == 7 || namelen == 8) && varname[0] == '.' &&
	isupper((unsigned char)varname[1]) &&
	(ctxt == VAR_CMD || ctxt == VAR_GLOBAL))
    {
	return strcmp(varname, ".TARGET") == 0 ||
	       strcmp(varname, ".ARCHIVE") == 0 ||
	       strcmp(varname, ".PREFIX") == 0 ||
	       strcmp(varname, ".MEMBER") == 0;
    }

    return FALSE;
}

/*-
 *-----------------------------------------------------------------------
 * Var_Parse --
 *	Given the start of a variable invocation (such as $v, $(VAR),
 *	${VAR:Mpattern}), extract the variable name, possibly some
 *	modifiers and find its value by applying the modifiers to the
 *	original value.
 *
 * Input:
 *	str		The string to parse
 *	ctxt		The context for the variable
 *	flags		VARE_UNDEFERR	if undefineds are an error
 *			VARE_WANTRES	if we actually want the result
 *			VARE_ASSIGN	if we are in a := assignment
 *	lengthPtr	OUT: The length of the specification
 *	freePtr		OUT: Non-NULL if caller should free *freePtr
 *
 * Results:
 *	Returns the value of the variable expression, never NULL.
 *	var_Error if there was a parse error and VARE_UNDEFERR was set.
 *	varNoError if there was a parse error and VARE_UNDEFERR was not set.
 *
 *	Parsing should continue at str + *lengthPtr.
 *
 *	After using the returned value, *freePtr must be freed, preferably
 *	using bmake_free since it is NULL in most cases.
 *
 * Side Effects:
 *	Any effects from the modifiers, such as :!cmd! or ::=value.
 *-----------------------------------------------------------------------
 */
/* coverity[+alloc : arg-*4] */
const char *
Var_Parse(const char * const str, GNode *ctxt, VarEvalFlags eflags,
	  int *lengthPtr, void **freePtr)
{
    const char	*tstr;		/* Pointer into str */
    Boolean 	 haveModifier;	/* TRUE if have modifiers for the variable */
    char	 startc;	/* Starting character if variable in parens
				 * or braces */
    char	 endc;		/* Ending character if variable in parens
				 * or braces */
    Boolean	 dynamic;	/* TRUE if the variable is local and we're
				 * expanding it in a non-local context. This
				 * is done to support dynamic sources. The
				 * result is just the invocation, unaltered */
    const char *extramodifiers;
    Var *v;
    char *nstr;
    char eflags_str[VarEvalFlags_ToStringSize];

    VAR_DEBUG("%s: %s with %s\n", __func__, str,
	      Enum_FlagsToString(eflags_str, sizeof eflags_str, eflags,
				 VarEvalFlags_ToStringSpecs));

    *freePtr = NULL;
    extramodifiers = NULL;	/* extra modifiers to apply first */
    dynamic = FALSE;

#ifdef USE_DOUBLE_BOOLEAN
    /* Appease GCC 5.5.0, which thinks that the variable might not be
     * initialized. */
    endc = '\0';
#endif

    startc = str[1];
    if (startc != PROPEN && startc != BROPEN) {
	char name[2];

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

	    if (ctxt == VAR_CMD || ctxt == VAR_GLOBAL) {
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
		    return "$(.TARGET)";
		case '%':
		    return "$(.MEMBER)";
		case '*':
		    return "$(.PREFIX)";
		case '!':
		    return "$(.ARCHIVE)";
		}
	    }
	    return (eflags & VARE_UNDEFERR) ? var_Error : varNoError;
	} else {
	    haveModifier = FALSE;
	    tstr = str + 1;
	}
    } else {
	Buffer namebuf;		/* Holds the variable name */
	int depth;
	size_t namelen;
	char *varname;

	endc = startc == PROPEN ? PRCLOSE : BRCLOSE;

	Buf_Init(&namebuf, 0);

	/*
	 * Skip to the end character or a colon, whichever comes first.
	 */
	depth = 1;
	for (tstr = str + 2; *tstr != '\0'; tstr++) {
	    /* Track depth so we can spot parse errors. */
	    if (*tstr == startc)
		depth++;
	    if (*tstr == endc) {
		if (--depth == 0)
		    break;
	    }
	    if (*tstr == ':' && depth == 1)
		break;
	    /* A variable inside a variable, expand. */
	    if (*tstr == '$') {
		int rlen;
		void *freeIt;
		const char *rval = Var_Parse(tstr, ctxt, eflags, &rlen,
					     &freeIt);
		Buf_AddStr(&namebuf, rval);
		free(freeIt);
		tstr += rlen - 1;
	    } else
		Buf_AddByte(&namebuf, *tstr);
	}
	if (*tstr == ':') {
	    haveModifier = TRUE;
	} else if (*tstr == endc) {
	    haveModifier = FALSE;
	} else {
	    Parse_Error(PARSE_FATAL, "Unclosed variable \"%s\"",
			Buf_GetAll(&namebuf, NULL));
	    /*
	     * If we never did find the end character, return NULL
	     * right now, setting the length to be the distance to
	     * the end of the string, since that's what make does.
	     */
	    *lengthPtr = (int)(size_t)(tstr - str);
	    Buf_Destroy(&namebuf, TRUE);
	    return var_Error;
	}

	varname = Buf_GetAll(&namebuf, &namelen);

	/*
	 * At this point, varname points into newly allocated memory from
	 * namebuf, containing only the name of the variable.
	 *
	 * start and tstr point into the const string that was pointed
	 * to by the original value of the str parameter.  start points
	 * to the '$' at the beginning of the string, while tstr points
	 * to the char just after the end of the variable name -- this
	 * will be '\0', ':', PRCLOSE, or BRCLOSE.
	 */

	v = VarFind(varname, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	/*
	 * Check also for bogus D and F forms of local variables since we're
	 * in a local context and the name is the right length.
	 */
	if (v == NULL && ctxt != VAR_CMD && ctxt != VAR_GLOBAL &&
	    namelen == 2 && (varname[1] == 'F' || varname[1] == 'D') &&
	    strchr("@%?*!<>", varname[0]) != NULL)
	{
	    /*
	     * Well, it's local -- go look for it.
	     */
	    char name[] = { varname[0], '\0' };
	    v = VarFind(name, ctxt, 0);

	    if (v != NULL) {
		if (varname[1] == 'D') {
		    extramodifiers = "H:";
		} else { /* F */
		    extramodifiers = "T:";
		}
	    }
	}

	if (v == NULL) {
	    dynamic = VarIsDynamic(ctxt, varname, namelen);

	    if (!haveModifier) {
		/*
		 * No modifiers -- have specification length so we can return
		 * now.
		 */
		*lengthPtr = (int)(size_t)(tstr - str) + 1;
		if (dynamic) {
		    char *pstr = bmake_strldup(str, (size_t)*lengthPtr);
		    *freePtr = pstr;
		    Buf_Destroy(&namebuf, TRUE);
		    return pstr;
		} else {
		    Buf_Destroy(&namebuf, TRUE);
		    return (eflags & VARE_UNDEFERR) ? var_Error : varNoError;
		}
	    } else {
		/*
		 * Still need to get to the end of the variable specification,
		 * so kludge up a Var structure for the modifications
		 */
		v = bmake_malloc(sizeof(Var));
		v->name = varname;
		Buf_Init(&v->val, 1);
		v->flags = VAR_JUNK;
		Buf_Destroy(&namebuf, FALSE);
	    }
	} else
	    Buf_Destroy(&namebuf, TRUE);
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
    if (strchr(nstr, '$') != NULL && (eflags & VARE_WANTRES) != 0) {
	nstr = Var_Subst(nstr, ctxt, eflags);
	*freePtr = nstr;
    }

    v->flags &= ~(unsigned)VAR_IN_USE;

    if (haveModifier || extramodifiers != NULL) {
	void *extraFree;

	extraFree = NULL;
	if (extramodifiers != NULL) {
	    const char *em = extramodifiers;
	    nstr = ApplyModifiers(&em, nstr, '(', ')',
				  v, ctxt, eflags, &extraFree);
	}

	if (haveModifier) {
	    /* Skip initial colon. */
	    tstr++;

	    nstr = ApplyModifiers(&tstr, nstr, startc, endc,
				  v, ctxt, eflags, freePtr);
	    free(extraFree);
	} else {
	    *freePtr = extraFree;
	}
    }

    /* Skip past endc if possible. */
    *lengthPtr = (int)(size_t)(tstr + (*tstr ? 1 : 0) - str);

    if (v->flags & VAR_FROM_ENV) {
	Boolean destroy = nstr != Buf_GetAll(&v->val, NULL);
	if (!destroy) {
	    /*
	     * Returning the value unmodified, so tell the caller to free
	     * the thing.
	     */
	    *freePtr = nstr;
	}
	(void)VarFreeEnv(v, destroy);
    } else if (v->flags & VAR_JUNK) {
	/*
	 * Perform any freeing needed and set *freePtr to NULL so the caller
	 * doesn't try to free a static pointer.
	 * If VAR_KEEP is also set then we want to keep str(?) as is.
	 */
	if (!(v->flags & VAR_KEEP)) {
	    if (*freePtr != NULL) {
		free(*freePtr);
		*freePtr = NULL;
	    }
	    if (dynamic) {
		nstr = bmake_strldup(str, (size_t)*lengthPtr);
		*freePtr = nstr;
	    } else {
		nstr = (eflags & VARE_UNDEFERR) ? var_Error : varNoError;
	    }
	}
	if (nstr != Buf_GetAll(&v->val, NULL))
	    Buf_Destroy(&v->val, TRUE);
	free(v->name);
	free(v);
    }
    return nstr;
}

/* Substitute for all variables in the given string in the given context.
 *
 * If eflags & VARE_UNDEFERR, Parse_Error will be called when an undefined
 * variable is encountered.
 *
 * If eflags & VARE_WANTRES, any effects from the modifiers, such as ::=,
 * :sh or !cmd! take place.
 *
 * Input:
 *	str		the string which to substitute
 *	ctxt		the context wherein to find variables
 *	eflags		VARE_UNDEFERR	if undefineds are an error
 *			VARE_WANTRES	if we actually want the result
 *			VARE_ASSIGN	if we are in a := assignment
 *
 * Results:
 *	The resulting string.
 */
char *
Var_Subst(const char *str, GNode *ctxt, VarEvalFlags eflags)
{
    Buffer buf;			/* Buffer for forming things */
    Boolean trailingBslash;

    /* Set true if an error has already been reported,
     * to prevent a plethora of messages when recursing */
    static Boolean errorReported;

    Buf_Init(&buf, 0);
    errorReported = FALSE;
    trailingBslash = FALSE;	/* variable ends in \ */

    while (*str) {
	if (*str == '\n' && trailingBslash)
	    Buf_AddByte(&buf, ' ');

	if (*str == '$' && str[1] == '$') {
	    /*
	     * A dollar sign may be escaped with another dollar sign.
	     * In such a case, we skip over the escape character and store the
	     * dollar sign into the buffer directly.
	     */
	    if (save_dollars && (eflags & VARE_ASSIGN))
		Buf_AddByte(&buf, '$');
	    Buf_AddByte(&buf, '$');
	    str += 2;
	} else if (*str != '$') {
	    /*
	     * Skip as many characters as possible -- either to the end of
	     * the string or to the next dollar sign (variable invocation).
	     */
	    const char *cp;

	    for (cp = str++; *str != '$' && *str != '\0'; str++)
		continue;
	    Buf_AddBytesBetween(&buf, cp, str);
	} else {
	    int length;
	    void *freeIt;
	    const char *val = Var_Parse(str, ctxt, eflags, &length, &freeIt);

	    if (val == var_Error || val == varNoError) {
		/*
		 * If performing old-time variable substitution, skip over
		 * the variable and continue with the substitution. Otherwise,
		 * store the dollar sign and advance str so we continue with
		 * the string...
		 */
		if (oldVars) {
		    str += length;
		} else if ((eflags & VARE_UNDEFERR) || val == var_Error) {
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
		size_t val_len;

		str += length;

		val_len = strlen(val);
		Buf_AddBytes(&buf, val, val_len);
		trailingBslash = val_len > 0 && val[val_len - 1] == '\\';
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
    Var_Stats();
}

void
Var_Stats(void)
{
    Hash_DebugStats(&VAR_GLOBAL->context, "VAR_GLOBAL");
}


/****************** PRINT DEBUGGING INFO *****************/
static void
VarPrintVar(void *vp, void *data MAKE_ATTR_UNUSED)
{
    Var *v = (Var *)vp;
    fprintf(debug_file, "%-16s = %s\n", v->name, Buf_GetAll(&v->val, NULL));
}

/* Print all variables in a context, unordered. */
void
Var_Dump(GNode *ctxt)
{
    Hash_ForEach(&ctxt->context, VarPrintVar, NULL);
}
