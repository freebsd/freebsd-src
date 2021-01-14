/*	$NetBSD: var.c,v 1.781 2021/01/10 23:59:53 rillig Exp $	*/

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

/*
 * Handling of variables and the expressions formed from them.
 *
 * Variables are set using lines of the form VAR=value.  Both the variable
 * name and the value can contain references to other variables, by using
 * expressions like ${VAR}, ${VAR:Modifiers}, ${${VARNAME}} or ${VAR:${MODS}}.
 *
 * Interface:
 *	Var_Init	Initialize this module.
 *
 *	Var_End		Clean up the module.
 *
 *	Var_Set		Set the value of the variable, creating it if
 *			necessary.
 *
 *	Var_Append	Append more characters to the variable, creating it if
 *			necessary. A space is placed between the old value and
 *			the new one.
 *
 *	Var_Exists	See if a variable exists.
 *
 *	Var_Value	Return the unexpanded value of a variable, or NULL if
 *			the variable is undefined.
 *
 *	Var_Subst	Substitute all variable expressions in a string.
 *
 *	Var_Parse	Parse a variable expression such as ${VAR:Mpattern}.
 *
 *	Var_Delete	Delete a variable.
 *
 *	Var_ReexportVars
 *			Export some or even all variables to the environment
 *			of this process and its child processes.
 *
 *	Var_Export	Export the variable to the environment of this process
 *			and its child processes.
 *
 *	Var_UnExport	Don't export the variable anymore.
 *
 * Debugging:
 *	Var_Stats	Print out hashing statistics if in -dh mode.
 *
 *	Var_Dump	Print out all variables defined in the given context.
 *
 * XXX: There's a lot of duplication in these functions.
 */

#include <sys/stat.h>
#include <sys/types.h>
#ifndef NO_REGEX
#include <regex.h>
#endif

#include "make.h"

#include <errno.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <time.h>

#include "dir.h"
#include "job.h"
#include "metachar.h"

/*	"@(#)var.c	8.3 (Berkeley) 3/19/94" */
MAKE_RCSID("$NetBSD: var.c,v 1.781 2021/01/10 23:59:53 rillig Exp $");

typedef enum VarFlags {
	VAR_NONE	= 0,

	/*
	 * The variable's value is currently being used by Var_Parse or
	 * Var_Subst.  This marker is used to avoid endless recursion.
	 */
	VAR_IN_USE = 0x01,

	/*
	 * The variable comes from the environment.
	 * These variables are not registered in any GNode, therefore they
	 * must be freed as soon as they are not used anymore.
	 */
	VAR_FROM_ENV = 0x02,

	/*
	 * The variable is exported to the environment, to be used by child
	 * processes.
	 */
	VAR_EXPORTED = 0x10,

	/*
	 * At the point where this variable was exported, it contained an
	 * unresolved reference to another variable.  Before any child
	 * process is started, it needs to be exported again, in the hope
	 * that the referenced variable can then be resolved.
	 */
	VAR_REEXPORT = 0x20,

	/* The variable came from the command line. */
	VAR_FROM_CMD = 0x40,

	/*
	 * The variable value cannot be changed anymore, and the variable
	 * cannot be deleted.  Any attempts to do so are silently ignored,
	 * they are logged with -dv though.
	 */
	VAR_READONLY = 0x80
} VarFlags;

/*
 * Variables are defined using one of the VAR=value assignments.  Their
 * value can be queried by expressions such as $V, ${VAR}, or with modifiers
 * such as ${VAR:S,from,to,g:Q}.
 *
 * There are 3 kinds of variables: context variables, environment variables,
 * undefined variables.
 *
 * Context variables are stored in a GNode.context.  The only way to undefine
 * a context variable is using the .undef directive.  In particular, it must
 * not be possible to undefine a variable during the evaluation of an
 * expression, or Var.name might point nowhere.
 *
 * Environment variables are temporary.  They are returned by VarFind, and
 * after using them, they must be freed using VarFreeEnv.
 *
 * Undefined variables occur during evaluation of variable expressions such
 * as ${UNDEF:Ufallback} in Var_Parse and ApplyModifiers.
 */
typedef struct Var {
	/*
	 * The name of the variable, once set, doesn't change anymore.
	 * For context variables, it aliases the corresponding HashEntry name.
	 * For environment and undefined variables, it is allocated.
	 */
	FStr name;

	/* The unexpanded value of the variable. */
	Buffer val;
	/* Miscellaneous status flags. */
	VarFlags flags;
} Var;

/*
 * Exporting vars is expensive so skip it if we can
 */
typedef enum VarExportedMode {
	VAR_EXPORTED_NONE,
	VAR_EXPORTED_SOME,
	VAR_EXPORTED_ALL
} VarExportedMode;

typedef enum UnexportWhat {
	UNEXPORT_NAMED,
	UNEXPORT_ALL,
	UNEXPORT_ENV
} UnexportWhat;

/* Flags for pattern matching in the :S and :C modifiers */
typedef enum VarPatternFlags {
	VARP_NONE		= 0,
	/* Replace as often as possible ('g') */
	VARP_SUB_GLOBAL		= 1 << 0,
	/* Replace only once ('1') */
	VARP_SUB_ONE		= 1 << 1,
	/* Match at start of word ('^') */
	VARP_ANCHOR_START	= 1 << 2,
	/* Match at end of word ('$') */
	VARP_ANCHOR_END		= 1 << 3
} VarPatternFlags;

/* SepBuf is a string being built from words, interleaved with separators. */
typedef struct SepBuf {
	Buffer buf;
	Boolean needSep;
	/* Usually ' ', but see the ':ts' modifier. */
	char sep;
} SepBuf;


ENUM_FLAGS_RTTI_4(VarEvalFlags,
		  VARE_UNDEFERR, VARE_WANTRES, VARE_KEEP_DOLLAR,
		  VARE_KEEP_UNDEF);

/*
 * This lets us tell if we have replaced the original environ
 * (which we cannot free).
 */
char **savedEnv = NULL;

/*
 * Special return value for Var_Parse, indicating a parse error.  It may be
 * caused by an undefined variable, a syntax error in a modifier or
 * something entirely different.
 */
char var_Error[] = "";

/*
 * Special return value for Var_Parse, indicating an undefined variable in
 * a case where VARE_UNDEFERR is not set.  This undefined variable is
 * typically a dynamic variable such as ${.TARGET}, whose expansion needs to
 * be deferred until it is defined in an actual target.
 */
static char varUndefined[] = "";

/*
 * Traditionally this make consumed $$ during := like any other expansion.
 * Other make's do not, and this make follows straight since 2016-01-09.
 *
 * This knob allows controlling the behavior.
 * FALSE to consume $$ during := assignment.
 * TRUE to preserve $$ during := assignment.
 */
#define MAKE_SAVE_DOLLARS ".MAKE.SAVE_DOLLARS"
static Boolean save_dollars = FALSE;

/*
 * Internally, variables are contained in four different contexts.
 *	1) the environment. They cannot be changed. If an environment
 *	   variable is appended to, the result is placed in the global
 *	   context.
 *	2) the global context. Variables set in the makefiles are located
 *	   here.
 *	3) the command-line context. All variables set on the command line
 *	   are placed in this context.
 *	4) the local context. Each target has associated with it a context
 *	   list. On this list are located the structures describing such
 *	   local variables as $(@) and $(*)
 * The four contexts are searched in the reverse order from which they are
 * listed (but see opts.checkEnvFirst).
 */
GNode          *VAR_INTERNAL;	/* variables from make itself */
GNode          *VAR_GLOBAL;	/* variables from the makefile */
GNode          *VAR_CMDLINE;	/* variables defined on the command-line */

ENUM_FLAGS_RTTI_6(VarFlags,
		  VAR_IN_USE, VAR_FROM_ENV,
		  VAR_EXPORTED, VAR_REEXPORT, VAR_FROM_CMD, VAR_READONLY);

static VarExportedMode var_exportedVars = VAR_EXPORTED_NONE;


static Var *
VarNew(FStr name, const char *value, VarFlags flags)
{
	size_t value_len = strlen(value);
	Var *var = bmake_malloc(sizeof *var);
	var->name = name;
	Buf_InitSize(&var->val, value_len + 1);
	Buf_AddBytes(&var->val, value, value_len);
	var->flags = flags;
	return var;
}

static const char *
CanonicalVarname(const char *name)
{
	if (*name == '.' && ch_isupper(name[1])) {
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
			if (strcmp(name, ".SHELL") == 0) {
				if (shellPath == NULL)
					Shell_Init();
			}
			break;
		case 'T':
			if (strcmp(name, ".TARGET") == 0)
				name = TARGET;
			break;
		}
	}

	/* GNU make has an additional alias $^ == ${.ALLSRC}. */

	return name;
}

static Var *
GNode_FindVar(GNode *ctxt, const char *varname, unsigned int hash)
{
	return HashTable_FindValueHash(&ctxt->vars, varname, hash);
}

/*
 * Find the variable in the context, and maybe in other contexts as well.
 *
 * Input:
 *	name		name to find, is not expanded any further
 *	ctxt		context in which to look first
 *	elsewhere	TRUE to look in other contexts as well
 *
 * Results:
 *	The found variable, or NULL if the variable does not exist.
 *	If the variable is an environment variable, it must be freed using
 *	VarFreeEnv after use.
 */
static Var *
VarFind(const char *name, GNode *ctxt, Boolean elsewhere)
{
	Var *var;
	unsigned int nameHash;

	/*
	 * If the variable name begins with a '.', it could very well be
	 * one of the local ones.  We check the name against all the local
	 * variables and substitute the short version in for 'name' if it
	 * matches one of them.
	 */
	name = CanonicalVarname(name);
	nameHash = Hash_Hash(name);

	/* First look for the variable in the given context. */
	var = GNode_FindVar(ctxt, name, nameHash);
	if (!elsewhere)
		return var;

	/*
	 * The variable was not found in the given context.
	 * Now look for it in the other contexts as well.
	 */
	if (var == NULL && ctxt != VAR_CMDLINE)
		var = GNode_FindVar(VAR_CMDLINE, name, nameHash);

	if (!opts.checkEnvFirst && var == NULL && ctxt != VAR_GLOBAL) {
		var = GNode_FindVar(VAR_GLOBAL, name, nameHash);
		if (var == NULL && ctxt != VAR_INTERNAL) {
			/* VAR_INTERNAL is subordinate to VAR_GLOBAL */
			var = GNode_FindVar(VAR_INTERNAL, name, nameHash);
		}
	}

	if (var == NULL) {
		char *env;

		if ((env = getenv(name)) != NULL) {
			char *varname = bmake_strdup(name);
			return VarNew(FStr_InitOwn(varname), env, VAR_FROM_ENV);
		}

		if (opts.checkEnvFirst && ctxt != VAR_GLOBAL) {
			var = GNode_FindVar(VAR_GLOBAL, name, nameHash);
			if (var == NULL && ctxt != VAR_INTERNAL)
				var = GNode_FindVar(VAR_INTERNAL, name,
				    nameHash);
			return var;
		}

		return NULL;
	}

	return var;
}

/*
 * If the variable is an environment variable, free it.
 *
 * Input:
 *	v		the variable
 *	freeValue	true if the variable value should be freed as well
 *
 * Results:
 *	TRUE if it is an environment variable, FALSE otherwise.
 */
static Boolean
VarFreeEnv(Var *v, Boolean freeValue)
{
	if (!(v->flags & VAR_FROM_ENV))
		return FALSE;

	FStr_Done(&v->name);
	Buf_Destroy(&v->val, freeValue);
	free(v);
	return TRUE;
}

/*
 * Add a new variable of the given name and value to the given context.
 * The name and val arguments are duplicated so they may safely be freed.
 */
static void
VarAdd(const char *name, const char *val, GNode *ctxt, VarSetFlags flags)
{
	HashEntry *he = HashTable_CreateEntry(&ctxt->vars, name, NULL);
	Var *v = VarNew(FStr_InitRefer(/* aliased to */ he->key), val,
	    flags & VAR_SET_READONLY ? VAR_READONLY : VAR_NONE);
	HashEntry_Set(he, v);
	if (!(ctxt->flags & INTERNAL))
		DEBUG3(VAR, "%s:%s = %s\n", ctxt->name, name, val);
}

/*
 * Remove a variable from a context, freeing all related memory as well.
 * The variable name is kept as-is, it is not expanded.
 */
void
Var_DeleteVar(const char *varname, GNode *ctxt)
{
	HashEntry *he = HashTable_FindEntry(&ctxt->vars, varname);
	Var *v;

	if (he == NULL) {
		DEBUG2(VAR, "%s:delete %s (not found)\n", ctxt->name, varname);
		return;
	}

	DEBUG2(VAR, "%s:delete %s\n", ctxt->name, varname);
	v = HashEntry_Get(he);
	if (v->flags & VAR_EXPORTED)
		unsetenv(v->name.str);
	if (strcmp(v->name.str, MAKE_EXPORTED) == 0)
		var_exportedVars = VAR_EXPORTED_NONE;
	assert(v->name.freeIt == NULL);
	HashTable_DeleteEntry(&ctxt->vars, he);
	Buf_Destroy(&v->val, TRUE);
	free(v);
}

/*
 * Remove a variable from a context, freeing all related memory as well.
 * The variable name is expanded once.
 */
void
Var_Delete(const char *name, GNode *ctxt)
{
	FStr varname = FStr_InitRefer(name);

	if (strchr(varname.str, '$') != NULL) {
		char *expanded;
		(void)Var_Subst(varname.str, VAR_GLOBAL, VARE_WANTRES,
		    &expanded);
		/* TODO: handle errors */
		varname = FStr_InitOwn(expanded);
	}

	Var_DeleteVar(varname.str, ctxt);
	FStr_Done(&varname);
}

/*
 * Undefine one or more variables from the global scope.
 * The argument is expanded exactly once and then split into words.
 */
void
Var_Undef(const char *arg)
{
	VarParseResult vpr;
	char *expanded;
	Words varnames;
	size_t i;

	if (arg[0] == '\0') {
		Parse_Error(PARSE_FATAL,
		    "The .undef directive requires an argument");
		return;
	}

	vpr = Var_Subst(arg, VAR_GLOBAL, VARE_WANTRES, &expanded);
	if (vpr != VPR_OK) {
		Parse_Error(PARSE_FATAL,
		    "Error in variable names to be undefined");
		return;
	}

	varnames = Str_Words(expanded, FALSE);
	if (varnames.len == 1 && varnames.words[0][0] == '\0')
		varnames.len = 0;

	for (i = 0; i < varnames.len; i++) {
		const char *varname = varnames.words[i];
		Var_DeleteVar(varname, VAR_GLOBAL);
	}

	Words_Free(varnames);
	free(expanded);
}

static Boolean
MayExport(const char *name)
{
	if (name[0] == '.')
		return FALSE;	/* skip internals */
	if (name[0] == '-')
		return FALSE;	/* skip misnamed variables */
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
	return TRUE;
}

static Boolean
ExportVarEnv(Var *v)
{
	const char *name = v->name.str;
	char *val = v->val.data;
	char *expr;

	if ((v->flags & VAR_EXPORTED) && !(v->flags & VAR_REEXPORT))
		return FALSE;	/* nothing to do */

	if (strchr(val, '$') == NULL) {
		if (!(v->flags & VAR_EXPORTED))
			setenv(name, val, 1);
		return TRUE;
	}

	if (v->flags & VAR_IN_USE) {
		/*
		 * We recursed while exporting in a child.
		 * This isn't going to end well, just skip it.
		 */
		return FALSE;
	}

	/* XXX: name is injected without escaping it */
	expr = str_concat3("${", name, "}");
	(void)Var_Subst(expr, VAR_GLOBAL, VARE_WANTRES, &val);
	/* TODO: handle errors */
	setenv(name, val, 1);
	free(val);
	free(expr);
	return TRUE;
}

static Boolean
ExportVarPlain(Var *v)
{
	if (strchr(v->val.data, '$') == NULL) {
		setenv(v->name.str, v->val.data, 1);
		v->flags |= VAR_EXPORTED;
		v->flags &= ~(unsigned)VAR_REEXPORT;
		return TRUE;
	}

	/*
	 * Flag the variable as something we need to re-export.
	 * No point actually exporting it now though,
	 * the child process can do it at the last minute.
	 */
	v->flags |= VAR_EXPORTED | VAR_REEXPORT;
	return TRUE;
}

static Boolean
ExportVarLiteral(Var *v)
{
	if ((v->flags & VAR_EXPORTED) && !(v->flags & VAR_REEXPORT))
		return FALSE;

	if (!(v->flags & VAR_EXPORTED))
		setenv(v->name.str, v->val.data, 1);

	return TRUE;
}

/*
 * Export a single variable.
 *
 * We ignore make internal variables (those which start with '.').
 * Also we jump through some hoops to avoid calling setenv
 * more than necessary since it can leak.
 * We only manipulate flags of vars if 'parent' is set.
 */
static Boolean
ExportVar(const char *name, VarExportMode mode)
{
	Var *v;

	if (!MayExport(name))
		return FALSE;

	v = VarFind(name, VAR_GLOBAL, FALSE);
	if (v == NULL)
		return FALSE;

	if (mode == VEM_ENV)
		return ExportVarEnv(v);
	else if (mode == VEM_PLAIN)
		return ExportVarPlain(v);
	else
		return ExportVarLiteral(v);
}

/*
 * Actually export the variables that have been marked as needing to be
 * re-exported.
 */
void
Var_ReexportVars(void)
{
	char *xvarnames;

	/*
	 * Several make implementations support this sort of mechanism for
	 * tracking recursion - but each uses a different name.
	 * We allow the makefiles to update MAKELEVEL and ensure
	 * children see a correctly incremented value.
	 */
	char tmp[BUFSIZ];
	snprintf(tmp, sizeof tmp, "%d", makelevel + 1);
	setenv(MAKE_LEVEL_ENV, tmp, 1);

	if (var_exportedVars == VAR_EXPORTED_NONE)
		return;

	if (var_exportedVars == VAR_EXPORTED_ALL) {
		HashIter hi;

		/* Ouch! Exporting all variables at once is crazy... */
		HashIter_Init(&hi, &VAR_GLOBAL->vars);
		while (HashIter_Next(&hi) != NULL) {
			Var *var = hi.entry->value;
			ExportVar(var->name.str, VEM_ENV);
		}
		return;
	}

	(void)Var_Subst("${" MAKE_EXPORTED ":O:u}", VAR_GLOBAL, VARE_WANTRES,
	    &xvarnames);
	/* TODO: handle errors */
	if (xvarnames[0] != '\0') {
		Words varnames = Str_Words(xvarnames, FALSE);
		size_t i;

		for (i = 0; i < varnames.len; i++)
			ExportVar(varnames.words[i], VEM_ENV);
		Words_Free(varnames);
	}
	free(xvarnames);
}

static void
ExportVars(const char *varnames, Boolean isExport, VarExportMode mode)
{
	Words words = Str_Words(varnames, FALSE);
	size_t i;

	if (words.len == 1 && words.words[0][0] == '\0')
		words.len = 0;

	for (i = 0; i < words.len; i++) {
		const char *varname = words.words[i];
		if (!ExportVar(varname, mode))
			continue;

		if (var_exportedVars == VAR_EXPORTED_NONE)
			var_exportedVars = VAR_EXPORTED_SOME;

		if (isExport && mode == VEM_PLAIN)
			Var_Append(MAKE_EXPORTED, varname, VAR_GLOBAL);
	}
	Words_Free(words);
}

static void
ExportVarsExpand(const char *uvarnames, Boolean isExport, VarExportMode mode)
{
	char *xvarnames;

	(void)Var_Subst(uvarnames, VAR_GLOBAL, VARE_WANTRES, &xvarnames);
	/* TODO: handle errors */
	ExportVars(xvarnames, isExport, mode);
	free(xvarnames);
}

/* Export the named variables, or all variables. */
void
Var_Export(VarExportMode mode, const char *varnames)
{
	if (mode == VEM_PLAIN && varnames[0] == '\0') {
		var_exportedVars = VAR_EXPORTED_ALL; /* use with caution! */
		return;
	}

	ExportVarsExpand(varnames, TRUE, mode);
}

void
Var_ExportVars(const char *varnames)
{
	ExportVarsExpand(varnames, FALSE, VEM_PLAIN);
}


extern char **environ;

static void
ClearEnv(void)
{
	const char *cp;
	char **newenv;

	cp = getenv(MAKE_LEVEL_ENV);	/* we should preserve this */
	if (environ == savedEnv) {
		/* we have been here before! */
		newenv = bmake_realloc(environ, 2 * sizeof(char *));
	} else {
		if (savedEnv != NULL) {
			free(savedEnv);
			savedEnv = NULL;
		}
		newenv = bmake_malloc(2 * sizeof(char *));
	}

	/* Note: we cannot safely free() the original environ. */
	environ = savedEnv = newenv;
	newenv[0] = NULL;
	newenv[1] = NULL;
	if (cp != NULL && *cp != '\0')
		setenv(MAKE_LEVEL_ENV, cp, 1);
}

static void
GetVarnamesToUnexport(Boolean isEnv, const char *arg,
		      FStr *out_varnames, UnexportWhat *out_what)
{
	UnexportWhat what;
	FStr varnames = FStr_InitRefer("");

	if (isEnv) {
		if (arg[0] != '\0') {
			Parse_Error(PARSE_FATAL,
			    "The directive .unexport-env does not take "
			    "arguments");
		}
		what = UNEXPORT_ENV;

	} else {
		what = arg[0] != '\0' ? UNEXPORT_NAMED : UNEXPORT_ALL;
		if (what == UNEXPORT_NAMED)
			varnames = FStr_InitRefer(arg);
	}

	if (what != UNEXPORT_NAMED) {
		char *expanded;
		/* Using .MAKE.EXPORTED */
		(void)Var_Subst("${" MAKE_EXPORTED ":O:u}", VAR_GLOBAL,
		    VARE_WANTRES, &expanded);
		/* TODO: handle errors */
		varnames = FStr_InitOwn(expanded);
	}

	*out_varnames = varnames;
	*out_what = what;
}

static void
UnexportVar(const char *varname, UnexportWhat what)
{
	Var *v = VarFind(varname, VAR_GLOBAL, FALSE);
	if (v == NULL) {
		DEBUG1(VAR, "Not unexporting \"%s\" (not found)\n", varname);
		return;
	}

	DEBUG1(VAR, "Unexporting \"%s\"\n", varname);
	if (what != UNEXPORT_ENV &&
	    (v->flags & VAR_EXPORTED) && !(v->flags & VAR_REEXPORT))
		unsetenv(v->name.str);
	v->flags &= ~(unsigned)(VAR_EXPORTED | VAR_REEXPORT);

	if (what == UNEXPORT_NAMED) {
		/* Remove the variable names from .MAKE.EXPORTED. */
		/* XXX: v->name is injected without escaping it */
		char *expr = str_concat3("${" MAKE_EXPORTED ":N",
		    v->name.str, "}");
		char *cp;
		(void)Var_Subst(expr, VAR_GLOBAL, VARE_WANTRES, &cp);
		/* TODO: handle errors */
		Var_Set(MAKE_EXPORTED, cp, VAR_GLOBAL);
		free(cp);
		free(expr);
	}
}

static void
UnexportVars(FStr *varnames, UnexportWhat what)
{
	size_t i;
	Words words;

	if (what == UNEXPORT_ENV)
		ClearEnv();

	words = Str_Words(varnames->str, FALSE);
	for (i = 0; i < words.len; i++) {
		const char *varname = words.words[i];
		UnexportVar(varname, what);
	}
	Words_Free(words);

	if (what != UNEXPORT_NAMED)
		Var_Delete(MAKE_EXPORTED, VAR_GLOBAL);
}

/*
 * This is called when .unexport[-env] is seen.
 *
 * str must have the form "unexport[-env] varname...".
 */
void
Var_UnExport(Boolean isEnv, const char *arg)
{
	UnexportWhat what;
	FStr varnames;

	GetVarnamesToUnexport(isEnv, arg, &varnames, &what);
	UnexportVars(&varnames, what);
	FStr_Done(&varnames);
}

/* Set the variable to the value; the name is not expanded. */
static void
SetVar(const char *name, const char *val, GNode *ctxt, VarSetFlags flags)
{
	Var *v;

	if (ctxt == VAR_GLOBAL) {
		v = VarFind(name, VAR_CMDLINE, FALSE);
		if (v != NULL) {
			if (v->flags & VAR_FROM_CMD) {
				DEBUG3(VAR, "%s:%s = %s ignored!\n",
				    ctxt->name, name, val);
				return;
			}
			VarFreeEnv(v, TRUE);
		}
	}

	/*
	 * We only look for a variable in the given context since anything set
	 * here will override anything in a lower context, so there's not much
	 * point in searching them all just to save a bit of memory...
	 */
	v = VarFind(name, ctxt, FALSE);
	if (v == NULL) {
		if (ctxt == VAR_CMDLINE && !(flags & VAR_SET_NO_EXPORT)) {
			/*
			 * This var would normally prevent the same name being
			 * added to VAR_GLOBAL, so delete it from there if
			 * needed. Otherwise -V name may show the wrong value.
			 */
			/* XXX: name is expanded for the second time */
			Var_Delete(name, VAR_GLOBAL);
		}
		VarAdd(name, val, ctxt, flags);
	} else {
		if ((v->flags & VAR_READONLY) && !(flags & VAR_SET_READONLY)) {
			DEBUG3(VAR, "%s:%s = %s ignored (read-only)\n",
			    ctxt->name, name, val);
			return;
		}
		Buf_Empty(&v->val);
		Buf_AddStr(&v->val, val);

		DEBUG3(VAR, "%s:%s = %s\n", ctxt->name, name, val);
		if (v->flags & VAR_EXPORTED)
			ExportVar(name, VEM_PLAIN);
	}
	/*
	 * Any variables given on the command line are automatically exported
	 * to the environment (as per POSIX standard)
	 * Other than internals.
	 */
	if (ctxt == VAR_CMDLINE && !(flags & VAR_SET_NO_EXPORT) &&
	    name[0] != '.') {
		if (v == NULL)
			v = VarFind(name, ctxt, FALSE); /* we just added it */
		v->flags |= VAR_FROM_CMD;

		/*
		 * If requested, don't export these in the environment
		 * individually.  We still put them in MAKEOVERRIDES so
		 * that the command-line settings continue to override
		 * Makefile settings.
		 */
		if (!opts.varNoExportEnv)
			setenv(name, val, 1);

		Var_Append(MAKEOVERRIDES, name, VAR_GLOBAL);
	}
	if (name[0] == '.' && strcmp(name, MAKE_SAVE_DOLLARS) == 0)
		save_dollars = ParseBoolean(val, save_dollars);

	if (v != NULL)
		VarFreeEnv(v, TRUE);
}

/* See Var_Set for documentation. */
void
Var_SetWithFlags(const char *name, const char *val, GNode *ctxt,
		 VarSetFlags flags)
{
	const char *unexpanded_name = name;
	FStr varname = FStr_InitRefer(name);

	assert(val != NULL);

	if (strchr(varname.str, '$') != NULL) {
		char *expanded;
		(void)Var_Subst(varname.str, ctxt, VARE_WANTRES, &expanded);
		/* TODO: handle errors */
		varname = FStr_InitOwn(expanded);
	}

	if (varname.str[0] == '\0') {
		DEBUG2(VAR, "Var_Set(\"%s\", \"%s\", ...) "
			    "name expands to empty string - ignored\n",
		    unexpanded_name, val);
	} else
		SetVar(varname.str, val, ctxt, flags);

	FStr_Done(&varname);
}

/*
 * Set the variable name to the value val in the given context.
 *
 * If the variable doesn't yet exist, it is created.
 * Otherwise the new value overwrites and replaces the old value.
 *
 * Input:
 *	name		name of the variable to set, is expanded once
 *	val		value to give to the variable
 *	ctxt		context in which to set it
 */
void
Var_Set(const char *name, const char *val, GNode *ctxt)
{
	Var_SetWithFlags(name, val, ctxt, VAR_SET_NONE);
}

/*
 * The variable of the given name has the given value appended to it in the
 * given context.
 *
 * If the variable doesn't exist, it is created. Otherwise the strings are
 * concatenated, with a space in between.
 *
 * Input:
 *	name		name of the variable to modify, is expanded once
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
 */
void
Var_Append(const char *name, const char *val, GNode *ctxt)
{
	char *name_freeIt = NULL;
	Var *v;

	assert(val != NULL);

	if (strchr(name, '$') != NULL) {
		const char *unexpanded_name = name;
		(void)Var_Subst(name, ctxt, VARE_WANTRES, &name_freeIt);
		/* TODO: handle errors */
		name = name_freeIt;
		if (name[0] == '\0') {
			DEBUG2(VAR, "Var_Append(\"%s\", \"%s\", ...) "
				    "name expands to empty string - ignored\n",
			    unexpanded_name, val);
			free(name_freeIt);
			return;
		}
	}

	v = VarFind(name, ctxt, ctxt == VAR_GLOBAL);

	if (v == NULL) {
		/* XXX: name is expanded for the second time */
		Var_Set(name, val, ctxt);
	} else if (v->flags & VAR_READONLY) {
		DEBUG1(VAR, "Ignoring append to %s since it is read-only\n",
		    name);
	} else if (ctxt == VAR_CMDLINE || !(v->flags & VAR_FROM_CMD)) {
		Buf_AddByte(&v->val, ' ');
		Buf_AddStr(&v->val, val);

		DEBUG3(VAR, "%s:%s = %s\n",
		    ctxt->name, name, Buf_GetAll(&v->val, NULL));

		if (v->flags & VAR_FROM_ENV) {
			/*
			 * If the original variable came from the environment,
			 * we have to install it in the global context (we
			 * could place it in the environment, but then we
			 * should provide a way to export other variables...)
			 */
			v->flags &= ~(unsigned)VAR_FROM_ENV;
			/*
			 * This is the only place where a variable is
			 * created whose v->name is not the same as
			 * ctxt->context->key.
			 */
			HashTable_Set(&ctxt->vars, name, v);
		}
	}
	free(name_freeIt);
}

/*
 * See if the given variable exists, in the given context or in other
 * fallback contexts.
 *
 * Input:
 *	name		Variable to find, is expanded once
 *	ctxt		Context in which to start search
 */
Boolean
Var_Exists(const char *name, GNode *ctxt)
{
	FStr varname = FStr_InitRefer(name);
	Var *v;

	if (strchr(varname.str, '$') != NULL) {
		char *expanded;
		(void)Var_Subst(varname.str, ctxt, VARE_WANTRES, &expanded);
		/* TODO: handle errors */
		varname = FStr_InitOwn(expanded);
	}

	v = VarFind(varname.str, ctxt, TRUE);
	FStr_Done(&varname);
	if (v == NULL)
		return FALSE;

	(void)VarFreeEnv(v, TRUE);
	return TRUE;
}

/*
 * Return the unexpanded value of the given variable in the given context,
 * or the usual contexts.
 *
 * Input:
 *	name		name to find, is not expanded any further
 *	ctxt		context in which to search for it
 *
 * Results:
 *	The value if the variable exists, NULL if it doesn't.
 *	If the returned value is not NULL, the caller must free
 *	out_freeIt when the returned value is no longer needed.
 */
FStr
Var_Value(const char *name, GNode *ctxt)
{
	Var *v = VarFind(name, ctxt, TRUE);
	char *value;

	if (v == NULL)
		return FStr_InitRefer(NULL);

	value = Buf_GetAll(&v->val, NULL);
	return VarFreeEnv(v, FALSE)
	    ? FStr_InitOwn(value)
	    : FStr_InitRefer(value);
}

/*
 * Return the unexpanded variable value from this node, without trying to look
 * up the variable in any other context.
 */
const char *
Var_ValueDirect(const char *name, GNode *ctxt)
{
	Var *v = VarFind(name, ctxt, FALSE);
	return v != NULL ? Buf_GetAll(&v->val, NULL) : NULL;
}


static void
SepBuf_Init(SepBuf *buf, char sep)
{
	Buf_InitSize(&buf->buf, 32);
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


/*
 * This callback for ModifyWords gets a single word from a variable expression
 * and typically adds a modification of this word to the buffer. It may also
 * do nothing or add several words.
 *
 * For example, in ${:Ua b c:M*2}, the callback is called 3 times, once for
 * each word of "a b c".
 */
typedef void (*ModifyWordsCallback)(const char *word, SepBuf *buf, void *data);


/*
 * Callback for ModifyWords to implement the :H modifier.
 * Add the dirname of the given word to the buffer.
 */
/*ARGSUSED*/
static void
ModifyWord_Head(const char *word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
	const char *slash = strrchr(word, '/');
	if (slash != NULL)
		SepBuf_AddBytesBetween(buf, word, slash);
	else
		SepBuf_AddStr(buf, ".");
}

/*
 * Callback for ModifyWords to implement the :T modifier.
 * Add the basename of the given word to the buffer.
 */
/*ARGSUSED*/
static void
ModifyWord_Tail(const char *word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
	SepBuf_AddStr(buf, str_basename(word));
}

/*
 * Callback for ModifyWords to implement the :E modifier.
 * Add the filename suffix of the given word to the buffer, if it exists.
 */
/*ARGSUSED*/
static void
ModifyWord_Suffix(const char *word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
	const char *lastDot = strrchr(word, '.');
	if (lastDot != NULL)
		SepBuf_AddStr(buf, lastDot + 1);
}

/*
 * Callback for ModifyWords to implement the :R modifier.
 * Add the basename of the given word to the buffer.
 */
/*ARGSUSED*/
static void
ModifyWord_Root(const char *word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
	const char *lastDot = strrchr(word, '.');
	size_t len = lastDot != NULL ? (size_t)(lastDot - word) : strlen(word);
	SepBuf_AddBytes(buf, word, len);
}

/*
 * Callback for ModifyWords to implement the :M modifier.
 * Place the word in the buffer if it matches the given pattern.
 */
static void
ModifyWord_Match(const char *word, SepBuf *buf, void *data)
{
	const char *pattern = data;
	DEBUG2(VAR, "VarMatch [%s] [%s]\n", word, pattern);
	if (Str_Match(word, pattern))
		SepBuf_AddStr(buf, word);
}

/*
 * Callback for ModifyWords to implement the :N modifier.
 * Place the word in the buffer if it doesn't match the given pattern.
 */
static void
ModifyWord_NoMatch(const char *word, SepBuf *buf, void *data)
{
	const char *pattern = data;
	if (!Str_Match(word, pattern))
		SepBuf_AddStr(buf, word);
}

#ifdef SYSVVARSUB

/*
 * Check word against pattern for a match (% is a wildcard).
 *
 * Input:
 *	word		Word to examine
 *	pattern		Pattern to examine against
 *
 * Results:
 *	Returns the start of the match, or NULL.
 *	out_match_len returns the length of the match, if any.
 *	out_hasPercent returns whether the pattern contains a percent.
 */
static const char *
SysVMatch(const char *word, const char *pattern,
	  size_t *out_match_len, Boolean *out_hasPercent)
{
	const char *p = pattern;
	const char *w = word;
	const char *percent;
	size_t w_len;
	size_t p_len;
	const char *w_tail;

	*out_hasPercent = FALSE;
	percent = strchr(p, '%');
	if (percent != NULL) {	/* ${VAR:...%...=...} */
		*out_hasPercent = TRUE;
		if (w[0] == '\0')
			return NULL;	/* empty word does not match pattern */

		/* check that the prefix matches */
		for (; p != percent && *w != '\0' && *w == *p; w++, p++)
			continue;
		if (p != percent)
			return NULL;	/* No match */

		p++;		/* Skip the percent */
		if (*p == '\0') {
			/* No more pattern, return the rest of the string */
			*out_match_len = strlen(w);
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

	*out_match_len = (size_t)(w_tail - w);
	return w;
}

struct ModifyWord_SYSVSubstArgs {
	GNode *ctx;
	const char *lhs;
	const char *rhs;
};

/* Callback for ModifyWords to implement the :%.from=%.to modifier. */
static void
ModifyWord_SYSVSubst(const char *word, SepBuf *buf, void *data)
{
	const struct ModifyWord_SYSVSubstArgs *args = data;
	char *rhs_expanded;
	const char *rhs;
	const char *percent;

	size_t match_len;
	Boolean lhsPercent;
	const char *match = SysVMatch(word, args->lhs, &match_len, &lhsPercent);
	if (match == NULL) {
		SepBuf_AddStr(buf, word);
		return;
	}

	/*
	 * Append rhs to the buffer, substituting the first '%' with the
	 * match, but only if the lhs had a '%' as well.
	 */

	(void)Var_Subst(args->rhs, args->ctx, VARE_WANTRES, &rhs_expanded);
	/* TODO: handle errors */

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


struct ModifyWord_SubstArgs {
	const char *lhs;
	size_t lhsLen;
	const char *rhs;
	size_t rhsLen;
	VarPatternFlags pflags;
	Boolean matched;
};

/*
 * Callback for ModifyWords to implement the :S,from,to, modifier.
 * Perform a string substitution on the given word.
 */
static void
ModifyWord_Subst(const char *word, SepBuf *buf, void *data)
{
	size_t wordLen = strlen(word);
	struct ModifyWord_SubstArgs *args = data;
	const char *match;

	if ((args->pflags & VARP_SUB_ONE) && args->matched)
		goto nosub;

	if (args->pflags & VARP_ANCHOR_START) {
		if (wordLen < args->lhsLen ||
		    memcmp(word, args->lhs, args->lhsLen) != 0)
			goto nosub;

		if ((args->pflags & VARP_ANCHOR_END) && wordLen != args->lhsLen)
			goto nosub;

		/* :S,^prefix,replacement, or :S,^whole$,replacement, */
		SepBuf_AddBytes(buf, args->rhs, args->rhsLen);
		SepBuf_AddBytes(buf, word + args->lhsLen,
		    wordLen - args->lhsLen);
		args->matched = TRUE;
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

	if (args->lhs[0] == '\0')
		goto nosub;

	/* unanchored case, may match more than once */
	while ((match = strstr(word, args->lhs)) != NULL) {
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
VarREError(int reerr, const regex_t *pat, const char *str)
{
	size_t errlen = regerror(reerr, pat, NULL, 0);
	char *errbuf = bmake_malloc(errlen);
	regerror(reerr, pat, errbuf, errlen);
	Error("%s: %s", str, errbuf);
	free(errbuf);
}

struct ModifyWord_SubstRegexArgs {
	regex_t re;
	size_t nsub;
	char *replace;
	VarPatternFlags pflags;
	Boolean matched;
};

/*
 * Callback for ModifyWords to implement the :C/from/to/ modifier.
 * Perform a regex substitution on the given word.
 */
static void
ModifyWord_SubstRegex(const char *word, SepBuf *buf, void *data)
{
	struct ModifyWord_SubstRegexArgs *args = data;
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

		for (rp = args->replace; *rp != '\0'; rp++) {
			if (*rp == '\\' && (rp[1] == '&' || rp[1] == '\\')) {
				SepBuf_AddBytes(buf, rp + 1, 1);
				rp++;
				continue;
			}

			if (*rp == '&') {
				SepBuf_AddBytesBetween(buf,
				    wp + m[0].rm_so, wp + m[0].rm_eo);
				continue;
			}

			if (*rp != '\\' || !ch_isdigit(rp[1])) {
				SepBuf_AddBytes(buf, rp, 1);
				continue;
			}

			{	/* \0 to \9 backreference */
				size_t n = (size_t)(rp[1] - '0');
				rp++;

				if (n >= args->nsub) {
					Error("No subexpression \\%u",
					    (unsigned)n);
				} else if (m[n].rm_so == -1) {
					Error(
					    "No match for subexpression \\%u",
					    (unsigned)n);
				} else {
					SepBuf_AddBytesBetween(buf,
					    wp + m[n].rm_so, wp + m[n].rm_eo);
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
			if (*wp != '\0')
				goto tryagain;
		}
		if (*wp != '\0')
			SepBuf_AddStr(buf, wp);
		break;
	default:
		VarREError(xrv, &args->re, "Unexpected regex error");
		/* FALLTHROUGH */
	case REG_NOMATCH:
	nosub:
		SepBuf_AddStr(buf, wp);
		break;
	}
}
#endif


struct ModifyWord_LoopArgs {
	GNode *ctx;
	char *tvar;		/* name of temporary variable */
	char *str;		/* string to expand */
	VarEvalFlags eflags;
};

/* Callback for ModifyWords to implement the :@var@...@ modifier of ODE make. */
static void
ModifyWord_Loop(const char *word, SepBuf *buf, void *data)
{
	const struct ModifyWord_LoopArgs *args;
	char *s;

	if (word[0] == '\0')
		return;

	args = data;
	Var_SetWithFlags(args->tvar, word, args->ctx, VAR_SET_NO_EXPORT);
	(void)Var_Subst(args->str, args->ctx, args->eflags, &s);
	/* TODO: handle errors */

	DEBUG4(VAR, "ModifyWord_Loop: "
		    "in \"%s\", replace \"%s\" with \"%s\" to \"%s\"\n",
	    word, args->tvar, args->str, s);

	if (s[0] == '\n' || Buf_EndsWith(&buf->buf, '\n'))
		buf->needSep = FALSE;
	SepBuf_AddStr(buf, s);
	free(s);
}


/*
 * The :[first..last] modifier selects words from the expression.
 * It can also reverse the words.
 */
static char *
VarSelectWords(char sep, Boolean oneBigWord, const char *str, int first,
	       int last)
{
	Words words;
	int len, start, end, step;
	int i;

	SepBuf buf;
	SepBuf_Init(&buf, sep);

	if (oneBigWord) {
		/* fake what Str_Words() would do if there were only one word */
		words.len = 1;
		words.words = bmake_malloc(
		    (words.len + 1) * sizeof(words.words[0]));
		words.freeIt = bmake_strdup(str);
		words.words[0] = words.freeIt;
		words.words[1] = NULL;
	} else {
		words = Str_Words(str, FALSE);
	}

	/*
	 * Now sanitize the given range.  If first or last are negative,
	 * convert them to the positive equivalents (-1 gets converted to len,
	 * -2 gets converted to (len - 1), etc.).
	 */
	len = (int)words.len;
	if (first < 0)
		first += len + 1;
	if (last < 0)
		last += len + 1;

	/* We avoid scanning more of the list than we need to. */
	if (first > last) {
		start = (first > len ? len : first) - 1;
		end = last < 1 ? 0 : last - 1;
		step = -1;
	} else {
		start = first < 1 ? 0 : first - 1;
		end = last > len ? len : last;
		step = 1;
	}

	for (i = start; (step < 0) == (i >= end); i += step) {
		SepBuf_AddStr(&buf, words.words[i]);
		SepBuf_Sep(&buf);
	}

	Words_Free(words);

	return SepBuf_Destroy(&buf, FALSE);
}


/*
 * Callback for ModifyWords to implement the :tA modifier.
 * Replace each word with the result of realpath() if successful.
 */
/*ARGSUSED*/
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

/*
 * Modify each of the words of the passed string using the given function.
 *
 * Input:
 *	str		String whose words should be modified
 *	modifyWord	Function that modifies a single word
 *	modifyWord_args Custom arguments for modifyWord
 *
 * Results:
 *	A string of all the words modified appropriately.
 */
static char *
ModifyWords(const char *str,
	    ModifyWordsCallback modifyWord, void *modifyWord_args,
	    Boolean oneBigWord, char sep)
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

	DEBUG2(VAR, "ModifyWords: split \"%s\" into %u words\n",
	    str, (unsigned)words.len);

	for (i = 0; i < words.len; i++) {
		modifyWord(words.words[i], &result, modifyWord_args);
		if (Buf_Len(&result.buf) > 0)
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

	Buf_Init(&buf);

	for (i = 0; i < words.len; i++) {
		if (i != 0) {
			/* XXX: Use st->sep instead of ' ', for consistency. */
			Buf_AddByte(&buf, ' ');
		}
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
			if (strcmp(words.words[i], words.words[j]) != 0 &&
			    (++j != i))
				words.words[j] = words.words[i];
		words.len = j + 1;
	}

	return Words_JoinFree(words);
}


/*
 * Quote shell meta-characters and space characters in the string.
 * If quoteDollar is set, also quote and double any '$' characters.
 */
static char *
VarQuote(const char *str, Boolean quoteDollar)
{
	Buffer buf;
	Buf_Init(&buf);

	for (; *str != '\0'; str++) {
		if (*str == '\n') {
			const char *newline = Shell_GetNewline();
			if (newline == NULL)
				newline = "\\\n";
			Buf_AddStr(&buf, newline);
			continue;
		}
		if (ch_isspace(*str) || is_shell_metachar((unsigned char)*str))
			Buf_AddByte(&buf, '\\');
		Buf_AddByte(&buf, *str);
		if (quoteDollar && *str == '$')
			Buf_AddStr(&buf, "\\$");
	}

	return Buf_Destroy(&buf, FALSE);
}

/*
 * Compute the 32-bit hash of the given string, using the MurmurHash3
 * algorithm. Output is encoded as 8 hex digits, in Little Endian order.
 */
static char *
VarHash(const char *str)
{
	static const char hexdigits[16] = "0123456789abcdef";
	const unsigned char *ustr = (const unsigned char *)str;

	uint32_t h = 0x971e137bU;
	uint32_t c1 = 0x95543787U;
	uint32_t c2 = 0x2ad7eb25U;
	size_t len2 = strlen(str);

	char *buf;
	size_t i;

	size_t len;
	for (len = len2; len != 0;) {
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

	if (tim == 0)
		time(&tim);
	if (*fmt == '\0')
		fmt = "%c";
	strftime(buf, sizeof buf, fmt, zulu ? gmtime(&tim) : localtime(&tim));

	buf[sizeof buf - 1] = '\0';
	return bmake_strdup(buf);
}

/*
 * The ApplyModifier functions take an expression that is being evaluated.
 * Their task is to apply a single modifier to the expression.
 * To do this, they parse the modifier and its parameters from pp and apply
 * the parsed modifier to the current value of the expression, generating a
 * new value from it.
 *
 * The modifier typically lasts until the next ':', or a closing '}' or ')'
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
 * st->endc.  The modifier doesn't have to check for this delimiter character,
 * this is done by ApplyModifiers.
 *
 * XXX: As of 2020-11-15, some modifiers such as :S, :C, :P, :L do not
 * need to be followed by a ':' or endc; this was an unintended mistake.
 *
 * If parsing fails because of a missing delimiter (as in the :S, :C or :@
 * modifiers), return AMR_CLEANUP.
 *
 * If parsing fails because the modifier is unknown, return AMR_UNKNOWN to
 * try the SysV modifier ${VAR:from=to} as fallback.  This should only be
 * done as long as there have been no side effects from evaluating nested
 * variables, to avoid evaluating them more than once.  In this case, the
 * parsing position may or may not be updated.  (XXX: Why not? The original
 * parsing position is well-known in ApplyModifiers.)
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
 * expression from st->val, or the variable name from st->var->name and stores
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
 * Some modifiers such as :D and :U turn undefined expressions into defined
 * expressions (see VEF_UNDEF, VEF_DEF).
 *
 * Some modifiers need to free some memory.
 */

typedef enum VarExprFlags {
	VEF_NONE	= 0,
	/* The variable expression is based on an undefined variable. */
	VEF_UNDEF = 0x01,
	/*
	 * The variable expression started as an undefined expression, but one
	 * of the modifiers (such as :D or :U) has turned the expression from
	 * undefined to defined.
	 */
	VEF_DEF = 0x02
} VarExprFlags;

ENUM_FLAGS_RTTI_2(VarExprFlags,
		  VEF_UNDEF, VEF_DEF);


typedef struct ApplyModifiersState {
	/* '\0' or '{' or '(' */
	const char startc;
	/* '\0' or '}' or ')' */
	const char endc;
	Var *const var;
	GNode *const ctxt;
	const VarEvalFlags eflags;
	/*
	 * The new value of the expression, after applying the modifier,
	 * never NULL.
	 */
	FStr newVal;
	/* Word separator in expansions (see the :ts modifier). */
	char sep;
	/*
	 * TRUE if some modifiers that otherwise split the variable value
	 * into words, like :S and :C, treat the variable value as a single
	 * big word, possibly containing spaces.
	 */
	Boolean oneBigWord;
	VarExprFlags exprFlags;
} ApplyModifiersState;

static void
ApplyModifiersState_Define(ApplyModifiersState *st)
{
	if (st->exprFlags & VEF_UNDEF)
		st->exprFlags |= VEF_DEF;
}

typedef enum ApplyModifierResult {
	/* Continue parsing */
	AMR_OK,
	/* Not a match, try other modifiers as well */
	AMR_UNKNOWN,
	/* Error out with "Bad modifier" message */
	AMR_BAD,
	/* Error out without error message */
	AMR_CLEANUP
} ApplyModifierResult;

/*
 * Allow backslashes to escape the delimiter, $, and \, but don't touch other
 * backslashes.
 */
static Boolean
IsEscapedModifierPart(const char *p, char delim,
		      struct ModifyWord_SubstArgs *subst)
{
	if (p[0] != '\\')
		return FALSE;
	if (p[1] == delim || p[1] == '\\' || p[1] == '$')
		return TRUE;
	return p[1] == '&' && subst != NULL;
}

/* See ParseModifierPart */
static VarParseResult
ParseModifierPartSubst(
    const char **pp,
    char delim,
    VarEvalFlags eflags,
    ApplyModifiersState *st,
    char **out_part,
    /* Optionally stores the length of the returned string, just to save
     * another strlen call. */
    size_t *out_length,
    /* For the first part of the :S modifier, sets the VARP_ANCHOR_END flag
     * if the last character of the pattern is a $. */
    VarPatternFlags *out_pflags,
    /* For the second part of the :S modifier, allow ampersands to be
     * escaped and replace unescaped ampersands with subst->lhs. */
    struct ModifyWord_SubstArgs *subst
)
{
	Buffer buf;
	const char *p;

	Buf_Init(&buf);

	/*
	 * Skim through until the matching delimiter is found; pick up
	 * variable expressions on the way.
	 */
	p = *pp;
	while (*p != '\0' && *p != delim) {
		const char *varstart;

		if (IsEscapedModifierPart(p, delim, subst)) {
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

		if (eflags & VARE_WANTRES) { /* Nested variable, evaluated */
			const char *nested_p = p;
			FStr nested_val;
			VarEvalFlags nested_eflags =
			    eflags & ~(unsigned)VARE_KEEP_DOLLAR;

			(void)Var_Parse(&nested_p, st->ctxt, nested_eflags,
			    &nested_val);
			/* TODO: handle errors */
			Buf_AddStr(&buf, nested_val.str);
			FStr_Done(&nested_val);
			p += nested_p - p;
			continue;
		}

		/*
		 * XXX: This whole block is very similar to Var_Parse without
		 * VARE_WANTRES.  There may be subtle edge cases though that
		 * are not yet covered in the unit tests and that are parsed
		 * differently, depending on whether they are evaluated or
		 * not.
		 *
		 * This subtle difference is not documented in the manual
		 * page, neither is the difference between parsing :D and
		 * :M documented. No code should ever depend on these
		 * details, but who knows.
		 */

		varstart = p;	/* Nested variable, only parsed */
		if (p[1] == '(' || p[1] == '{') {
			/*
			 * Find the end of this variable reference
			 * and suck it in without further ado.
			 * It will be interpreted later.
			 */
			char startc = p[1];
			int endc = startc == '(' ? ')' : '}';
			int depth = 1;

			for (p += 2; *p != '\0' && depth > 0; p++) {
				if (p[-1] != '\\') {
					if (*p == startc)
						depth++;
					if (*p == endc)
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
		Error("Unfinished modifier for %s ('%c' missing)",
		    st->var->name.str, delim);
		*out_part = NULL;
		return VPR_ERR;
	}

	*pp = ++p;
	if (out_length != NULL)
		*out_length = Buf_Len(&buf);

	*out_part = Buf_Destroy(&buf, FALSE);
	DEBUG1(VAR, "Modifier part: \"%s\"\n", *out_part);
	return VPR_OK;
}

/*
 * Parse a part of a modifier such as the "from" and "to" in :S/from/to/ or
 * the "var" or "replacement ${var}" in :@var@replacement ${var}@, up to and
 * including the next unescaped delimiter.  The delimiter, as well as the
 * backslash or the dollar, can be escaped with a backslash.
 *
 * Return the parsed (and possibly expanded) string, or NULL if no delimiter
 * was found.  On successful return, the parsing position pp points right
 * after the delimiter.  The delimiter is not included in the returned
 * value though.
 */
static VarParseResult
ParseModifierPart(
    /* The parsing position, updated upon return */
    const char **pp,
    /* Parsing stops at this delimiter */
    char delim,
    /* Flags for evaluating nested variables; if VARE_WANTRES is not set,
     * the text is only parsed. */
    VarEvalFlags eflags,
    ApplyModifiersState *st,
    char **out_part
)
{
	return ParseModifierPartSubst(pp, delim, eflags, st, out_part,
	    NULL, NULL, NULL);
}

/* Test whether mod starts with modname, followed by a delimiter. */
MAKE_INLINE Boolean
ModMatch(const char *mod, const char *modname, char endc)
{
	size_t n = strlen(modname);
	return strncmp(mod, modname, n) == 0 &&
	       (mod[n] == endc || mod[n] == ':');
}

/* Test whether mod starts with modname, followed by a delimiter or '='. */
MAKE_INLINE Boolean
ModMatchEq(const char *mod, const char *modname, char endc)
{
	size_t n = strlen(modname);
	return strncmp(mod, modname, n) == 0 &&
	       (mod[n] == endc || mod[n] == ':' || mod[n] == '=');
}

static Boolean
TryParseIntBase0(const char **pp, int *out_num)
{
	char *end;
	long n;

	errno = 0;
	n = strtol(*pp, &end, 0);
	if ((n == LONG_MIN || n == LONG_MAX) && errno == ERANGE)
		return FALSE;
	if (n < INT_MIN || n > INT_MAX)
		return FALSE;

	*pp = end;
	*out_num = (int)n;
	return TRUE;
}

static Boolean
TryParseSize(const char **pp, size_t *out_num)
{
	char *end;
	unsigned long n;

	if (!ch_isdigit(**pp))
		return FALSE;

	errno = 0;
	n = strtoul(*pp, &end, 10);
	if (n == ULONG_MAX && errno == ERANGE)
		return FALSE;
	if (n > SIZE_MAX)
		return FALSE;

	*pp = end;
	*out_num = (size_t)n;
	return TRUE;
}

static Boolean
TryParseChar(const char **pp, int base, char *out_ch)
{
	char *end;
	unsigned long n;

	if (!ch_isalnum(**pp))
		return FALSE;

	errno = 0;
	n = strtoul(*pp, &end, base);
	if (n == ULONG_MAX && errno == ERANGE)
		return FALSE;
	if (n > UCHAR_MAX)
		return FALSE;

	*pp = end;
	*out_ch = (char)n;
	return TRUE;
}

/* :@var@...${var}...@ */
static ApplyModifierResult
ApplyModifier_Loop(const char **pp, const char *val, ApplyModifiersState *st)
{
	struct ModifyWord_LoopArgs args;
	char prev_sep;
	VarParseResult res;

	args.ctx = st->ctxt;

	(*pp)++;		/* Skip the first '@' */
	res = ParseModifierPart(pp, '@', VARE_NONE, st, &args.tvar);
	if (res != VPR_OK)
		return AMR_CLEANUP;
	if (opts.strict && strchr(args.tvar, '$') != NULL) {
		Parse_Error(PARSE_FATAL,
		    "In the :@ modifier of \"%s\", the variable name \"%s\" "
		    "must not contain a dollar.",
		    st->var->name.str, args.tvar);
		return AMR_CLEANUP;
	}

	res = ParseModifierPart(pp, '@', VARE_NONE, st, &args.str);
	if (res != VPR_OK)
		return AMR_CLEANUP;

	args.eflags = st->eflags & ~(unsigned)VARE_KEEP_DOLLAR;
	prev_sep = st->sep;
	st->sep = ' ';		/* XXX: should be st->sep for consistency */
	st->newVal = FStr_InitOwn(
	    ModifyWords(val, ModifyWord_Loop, &args, st->oneBigWord, st->sep));
	st->sep = prev_sep;
	/* XXX: Consider restoring the previous variable instead of deleting. */
	Var_Delete(args.tvar, st->ctxt);
	free(args.tvar);
	free(args.str);
	return AMR_OK;
}

/* :Ddefined or :Uundefined */
static ApplyModifierResult
ApplyModifier_Defined(const char **pp, const char *val, ApplyModifiersState *st)
{
	Buffer buf;
	const char *p;

	VarEvalFlags eflags = VARE_NONE;
	if (st->eflags & VARE_WANTRES)
		if ((**pp == 'D') == !(st->exprFlags & VEF_UNDEF))
			eflags = st->eflags;

	Buf_Init(&buf);
	p = *pp + 1;
	while (*p != st->endc && *p != ':' && *p != '\0') {

		/* XXX: This code is similar to the one in Var_Parse.
		 * See if the code can be merged.
		 * See also ApplyModifier_Match. */

		/* Escaped delimiter or other special character */
		if (*p == '\\') {
			char c = p[1];
			if (c == st->endc || c == ':' || c == '$' ||
			    c == '\\') {
				Buf_AddByte(&buf, c);
				p += 2;
				continue;
			}
		}

		/* Nested variable expression */
		if (*p == '$') {
			FStr nested_val;

			(void)Var_Parse(&p, st->ctxt, eflags, &nested_val);
			/* TODO: handle errors */
			Buf_AddStr(&buf, nested_val.str);
			FStr_Done(&nested_val);
			continue;
		}

		/* Ordinary text */
		Buf_AddByte(&buf, *p);
		p++;
	}
	*pp = p;

	ApplyModifiersState_Define(st);

	if (eflags & VARE_WANTRES) {
		st->newVal = FStr_InitOwn(Buf_Destroy(&buf, FALSE));
	} else {
		st->newVal = FStr_InitRefer(val);
		Buf_Destroy(&buf, TRUE);
	}
	return AMR_OK;
}

/* :L */
static ApplyModifierResult
ApplyModifier_Literal(const char **pp, ApplyModifiersState *st)
{
	ApplyModifiersState_Define(st);
	st->newVal = FStr_InitOwn(bmake_strdup(st->var->name.str));
	(*pp)++;
	return AMR_OK;
}

static Boolean
TryParseTime(const char **pp, time_t *out_time)
{
	char *end;
	unsigned long n;

	if (!ch_isdigit(**pp))
		return FALSE;

	errno = 0;
	n = strtoul(*pp, &end, 10);
	if (n == ULONG_MAX && errno == ERANGE)
		return FALSE;

	*pp = end;
	*out_time = (time_t)n;	/* ignore possible truncation for now */
	return TRUE;
}

/* :gmtime */
static ApplyModifierResult
ApplyModifier_Gmtime(const char **pp, const char *val, ApplyModifiersState *st)
{
	time_t utc;

	const char *mod = *pp;
	if (!ModMatchEq(mod, "gmtime", st->endc))
		return AMR_UNKNOWN;

	if (mod[6] == '=') {
		const char *arg = mod + 7;
		if (!TryParseTime(&arg, &utc)) {
			Parse_Error(PARSE_FATAL,
			    "Invalid time value: %s", mod + 7);
			return AMR_CLEANUP;
		}
		*pp = arg;
	} else {
		utc = 0;
		*pp = mod + 6;
	}
	st->newVal = FStr_InitOwn(VarStrftime(val, TRUE, utc));
	return AMR_OK;
}

/* :localtime */
static ApplyModifierResult
ApplyModifier_Localtime(const char **pp, const char *val,
			ApplyModifiersState *st)
{
	time_t utc;

	const char *mod = *pp;
	if (!ModMatchEq(mod, "localtime", st->endc))
		return AMR_UNKNOWN;

	if (mod[9] == '=') {
		const char *arg = mod + 10;
		if (!TryParseTime(&arg, &utc)) {
			Parse_Error(PARSE_FATAL,
			    "Invalid time value: %s", mod + 10);
			return AMR_CLEANUP;
		}
		*pp = arg;
	} else {
		utc = 0;
		*pp = mod + 9;
	}
	st->newVal = FStr_InitOwn(VarStrftime(val, FALSE, utc));
	return AMR_OK;
}

/* :hash */
static ApplyModifierResult
ApplyModifier_Hash(const char **pp, const char *val, ApplyModifiersState *st)
{
	if (!ModMatch(*pp, "hash", st->endc))
		return AMR_UNKNOWN;

	st->newVal = FStr_InitOwn(VarHash(val));
	*pp += 4;
	return AMR_OK;
}

/* :P */
static ApplyModifierResult
ApplyModifier_Path(const char **pp, ApplyModifiersState *st)
{
	GNode *gn;
	char *path;

	ApplyModifiersState_Define(st);

	gn = Targ_FindNode(st->var->name.str);
	if (gn == NULL || gn->type & OP_NOPATH) {
		path = NULL;
	} else if (gn->path != NULL) {
		path = bmake_strdup(gn->path);
	} else {
		SearchPath *searchPath = Suff_FindPath(gn);
		path = Dir_FindFile(st->var->name.str, searchPath);
	}
	if (path == NULL)
		path = bmake_strdup(st->var->name.str);
	st->newVal = FStr_InitOwn(path);

	(*pp)++;
	return AMR_OK;
}

/* :!cmd! */
static ApplyModifierResult
ApplyModifier_ShellCommand(const char **pp, ApplyModifiersState *st)
{
	char *cmd;
	const char *errfmt;
	VarParseResult res;

	(*pp)++;
	res = ParseModifierPart(pp, '!', st->eflags, st, &cmd);
	if (res != VPR_OK)
		return AMR_CLEANUP;

	errfmt = NULL;
	if (st->eflags & VARE_WANTRES)
		st->newVal = FStr_InitOwn(Cmd_Exec(cmd, &errfmt));
	else
		st->newVal = FStr_InitRefer("");
	if (errfmt != NULL)
		Error(errfmt, cmd);	/* XXX: why still return AMR_OK? */
	free(cmd);

	ApplyModifiersState_Define(st);
	return AMR_OK;
}

/*
 * The :range modifier generates an integer sequence as long as the words.
 * The :range=7 modifier generates an integer sequence from 1 to 7.
 */
static ApplyModifierResult
ApplyModifier_Range(const char **pp, const char *val, ApplyModifiersState *st)
{
	size_t n;
	Buffer buf;
	size_t i;

	const char *mod = *pp;
	if (!ModMatchEq(mod, "range", st->endc))
		return AMR_UNKNOWN;

	if (mod[5] == '=') {
		const char *p = mod + 6;
		if (!TryParseSize(&p, &n)) {
			Parse_Error(PARSE_FATAL,
			    "Invalid number: %s", mod + 6);
			return AMR_CLEANUP;
		}
		*pp = p;
	} else {
		n = 0;
		*pp = mod + 5;
	}

	if (n == 0) {
		Words words = Str_Words(val, FALSE);
		n = words.len;
		Words_Free(words);
	}

	Buf_Init(&buf);

	for (i = 0; i < n; i++) {
		if (i != 0) {
			/* XXX: Use st->sep instead of ' ', for consistency. */
			Buf_AddByte(&buf, ' ');
		}
		Buf_AddInt(&buf, 1 + (int)i);
	}

	st->newVal = FStr_InitOwn(Buf_Destroy(&buf, FALSE));
	return AMR_OK;
}

/* :Mpattern or :Npattern */
static ApplyModifierResult
ApplyModifier_Match(const char **pp, const char *val, ApplyModifiersState *st)
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
	 * XXX: This will likely not work right if $() and ${} are intermixed.
	 */
	/* XXX: This code is similar to the one in Var_Parse.
	 * See if the code can be merged.
	 * See also ApplyModifier_Defined. */
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
	} else {
		pattern = bmake_strsedup(mod + 1, endpat);
	}

	if (needSubst) {
		char *old_pattern = pattern;
		(void)Var_Subst(pattern, st->ctxt, st->eflags, &pattern);
		/* TODO: handle errors */
		free(old_pattern);
	}

	DEBUG3(VAR, "Pattern[%s] for [%s] is [%s]\n",
	    st->var->name.str, val, pattern);

	callback = mod[0] == 'M' ? ModifyWord_Match : ModifyWord_NoMatch;
	st->newVal = FStr_InitOwn(ModifyWords(val, callback, pattern,
	    st->oneBigWord, st->sep));
	free(pattern);
	return AMR_OK;
}

/* :S,from,to, */
static ApplyModifierResult
ApplyModifier_Subst(const char **pp, const char *val, ApplyModifiersState *st)
{
	struct ModifyWord_SubstArgs args;
	char *lhs, *rhs;
	Boolean oneBigWord;
	VarParseResult res;

	char delim = (*pp)[1];
	if (delim == '\0') {
		Error("Missing delimiter for :S modifier");
		(*pp)++;
		return AMR_CLEANUP;
	}

	*pp += 2;

	args.pflags = VARP_NONE;
	args.matched = FALSE;

	/*
	 * If pattern begins with '^', it is anchored to the
	 * start of the word -- skip over it and flag pattern.
	 */
	if (**pp == '^') {
		args.pflags |= VARP_ANCHOR_START;
		(*pp)++;
	}

	res = ParseModifierPartSubst(pp, delim, st->eflags, st, &lhs,
	    &args.lhsLen, &args.pflags, NULL);
	if (res != VPR_OK)
		return AMR_CLEANUP;
	args.lhs = lhs;

	res = ParseModifierPartSubst(pp, delim, st->eflags, st, &rhs,
	    &args.rhsLen, NULL, &args);
	if (res != VPR_OK)
		return AMR_CLEANUP;
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

	st->newVal = FStr_InitOwn(ModifyWords(val, ModifyWord_Subst, &args,
	    oneBigWord, st->sep));

	free(lhs);
	free(rhs);
	return AMR_OK;
}

#ifndef NO_REGEX

/* :C,from,to, */
static ApplyModifierResult
ApplyModifier_Regex(const char **pp, const char *val, ApplyModifiersState *st)
{
	char *re;
	struct ModifyWord_SubstRegexArgs args;
	Boolean oneBigWord;
	int error;
	VarParseResult res;

	char delim = (*pp)[1];
	if (delim == '\0') {
		Error("Missing delimiter for :C modifier");
		(*pp)++;
		return AMR_CLEANUP;
	}

	*pp += 2;

	res = ParseModifierPart(pp, delim, st->eflags, st, &re);
	if (res != VPR_OK)
		return AMR_CLEANUP;

	res = ParseModifierPart(pp, delim, st->eflags, st, &args.replace);
	if (args.replace == NULL) {
		free(re);
		return AMR_CLEANUP;
	}

	args.pflags = VARP_NONE;
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
	if (error != 0) {
		VarREError(error, &args.re, "Regex compilation error");
		free(args.replace);
		return AMR_CLEANUP;
	}

	args.nsub = args.re.re_nsub + 1;
	if (args.nsub > 10)
		args.nsub = 10;
	st->newVal = FStr_InitOwn(
	    ModifyWords(val, ModifyWord_SubstRegex, &args,
		oneBigWord, st->sep));
	regfree(&args.re);
	free(args.replace);
	return AMR_OK;
}

#endif

/* :Q, :q */
static ApplyModifierResult
ApplyModifier_Quote(const char **pp, const char *val, ApplyModifiersState *st)
{
	if ((*pp)[1] == st->endc || (*pp)[1] == ':') {
		st->newVal = FStr_InitOwn(VarQuote(val, **pp == 'q'));
		(*pp)++;
		return AMR_OK;
	} else
		return AMR_UNKNOWN;
}

/*ARGSUSED*/
static void
ModifyWord_Copy(const char *word, SepBuf *buf, void *data MAKE_ATTR_UNUSED)
{
	SepBuf_AddStr(buf, word);
}

/* :ts<separator> */
static ApplyModifierResult
ApplyModifier_ToSep(const char **pp, const char *val, ApplyModifiersState *st)
{
	const char *sep = *pp + 2;

	/* ":ts<any><endc>" or ":ts<any>:" */
	if (sep[0] != st->endc && (sep[1] == st->endc || sep[1] == ':')) {
		st->sep = sep[0];
		*pp = sep + 1;
		goto ok;
	}

	/* ":ts<endc>" or ":ts:" */
	if (sep[0] == st->endc || sep[0] == ':') {
		st->sep = '\0';	/* no separator */
		*pp = sep;
		goto ok;
	}

	/* ":ts<unrecognised><unrecognised>". */
	if (sep[0] != '\\') {
		(*pp)++;	/* just for backwards compatibility */
		return AMR_BAD;
	}

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
		const char *p = sep + 1;
		int base = 8;	/* assume octal */

		if (sep[1] == 'x') {
			base = 16;
			p++;
		} else if (!ch_isdigit(sep[1])) {
			(*pp)++;	/* just for backwards compatibility */
			return AMR_BAD;	/* ":ts<backslash><unrecognised>". */
		}

		if (!TryParseChar(&p, base, &st->sep)) {
			Parse_Error(PARSE_FATAL,
			    "Invalid character number: %s", p);
			return AMR_CLEANUP;
		}
		if (*p != ':' && *p != st->endc) {
			(*pp)++;	/* just for backwards compatibility */
			return AMR_BAD;
		}

		*pp = p;
	}

ok:
	st->newVal = FStr_InitOwn(
	    ModifyWords(val, ModifyWord_Copy, NULL, st->oneBigWord, st->sep));
	return AMR_OK;
}

static char *
str_toupper(const char *str)
{
	char *res;
	size_t i, len;

	len = strlen(str);
	res = bmake_malloc(len + 1);
	for (i = 0; i < len + 1; i++)
		res[i] = ch_toupper(str[i]);

	return res;
}

static char *
str_tolower(const char *str)
{
	char *res;
	size_t i, len;

	len = strlen(str);
	res = bmake_malloc(len + 1);
	for (i = 0; i < len + 1; i++)
		res[i] = ch_tolower(str[i]);

	return res;
}

/* :tA, :tu, :tl, :ts<separator>, etc. */
static ApplyModifierResult
ApplyModifier_To(const char **pp, const char *val, ApplyModifiersState *st)
{
	const char *mod = *pp;
	assert(mod[0] == 't');

	if (mod[1] == st->endc || mod[1] == ':' || mod[1] == '\0') {
		*pp = mod + 1;
		return AMR_BAD;	/* Found ":t<endc>" or ":t:". */
	}

	if (mod[1] == 's')
		return ApplyModifier_ToSep(pp, val, st);

	if (mod[2] != st->endc && mod[2] != ':') {
		*pp = mod + 1;
		return AMR_BAD;	/* Found ":t<unrecognised><unrecognised>". */
	}

	/* Check for two-character options: ":tu", ":tl" */
	if (mod[1] == 'A') {	/* absolute path */
		st->newVal = FStr_InitOwn(
		    ModifyWords(val, ModifyWord_Realpath, NULL,
		        st->oneBigWord, st->sep));
		*pp = mod + 2;
		return AMR_OK;
	}

	if (mod[1] == 'u') {	/* :tu */
		st->newVal = FStr_InitOwn(str_toupper(val));
		*pp = mod + 2;
		return AMR_OK;
	}

	if (mod[1] == 'l') {	/* :tl */
		st->newVal = FStr_InitOwn(str_tolower(val));
		*pp = mod + 2;
		return AMR_OK;
	}

	if (mod[1] == 'W' || mod[1] == 'w') { /* :tW, :tw */
		st->oneBigWord = mod[1] == 'W';
		st->newVal = FStr_InitRefer(val);
		*pp = mod + 2;
		return AMR_OK;
	}

	/* Found ":t<unrecognised>:" or ":t<unrecognised><endc>". */
	*pp = mod + 1;
	return AMR_BAD;
}

/* :[#], :[1], :[-1..1], etc. */
static ApplyModifierResult
ApplyModifier_Words(const char **pp, const char *val, ApplyModifiersState *st)
{
	char *estr;
	int first, last;
	VarParseResult res;
	const char *p;

	(*pp)++;		/* skip the '[' */
	res = ParseModifierPart(pp, ']', st->eflags, st, &estr);
	if (res != VPR_OK)
		return AMR_CLEANUP;

	/* now *pp points just after the closing ']' */
	if (**pp != ':' && **pp != st->endc)
		goto bad_modifier;	/* Found junk after ']' */

	if (estr[0] == '\0')
		goto bad_modifier;	/* empty square brackets in ":[]". */

	if (estr[0] == '#' && estr[1] == '\0') { /* Found ":[#]" */
		if (st->oneBigWord) {
			st->newVal = FStr_InitRefer("1");
		} else {
			Buffer buf;

			Words words = Str_Words(val, FALSE);
			size_t ac = words.len;
			Words_Free(words);

			/* 3 digits + '\0' is usually enough */
			Buf_InitSize(&buf, 4);
			Buf_AddInt(&buf, (int)ac);
			st->newVal = FStr_InitOwn(Buf_Destroy(&buf, FALSE));
		}
		goto ok;
	}

	if (estr[0] == '*' && estr[1] == '\0') {
		/* Found ":[*]" */
		st->oneBigWord = TRUE;
		st->newVal = FStr_InitRefer(val);
		goto ok;
	}

	if (estr[0] == '@' && estr[1] == '\0') {
		/* Found ":[@]" */
		st->oneBigWord = FALSE;
		st->newVal = FStr_InitRefer(val);
		goto ok;
	}

	/*
	 * We expect estr to contain a single integer for :[N], or two
	 * integers separated by ".." for :[start..end].
	 */
	p = estr;
	if (!TryParseIntBase0(&p, &first))
		goto bad_modifier;	/* Found junk instead of a number */

	if (p[0] == '\0') {		/* Found only one integer in :[N] */
		last = first;
	} else if (p[0] == '.' && p[1] == '.' && p[2] != '\0') {
		/* Expecting another integer after ".." */
		p += 2;
		if (!TryParseIntBase0(&p, &last) || *p != '\0')
			goto bad_modifier; /* Found junk after ".." */
	} else
		goto bad_modifier;	/* Found junk instead of ".." */

	/*
	 * Now first and last are properly filled in, but we still have to
	 * check for 0 as a special case.
	 */
	if (first == 0 && last == 0) {
		/* ":[0]" or perhaps ":[0..0]" */
		st->oneBigWord = TRUE;
		st->newVal = FStr_InitRefer(val);
		goto ok;
	}

	/* ":[0..N]" or ":[N..0]" */
	if (first == 0 || last == 0)
		goto bad_modifier;

	/* Normal case: select the words described by first and last. */
	st->newVal = FStr_InitOwn(
	    VarSelectWords(st->sep, st->oneBigWord, val, first, last));

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
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int
str_cmp_desc(const void *a, const void *b)
{
	return strcmp(*(const char *const *)b, *(const char *const *)a);
}

static void
ShuffleStrings(char **strs, size_t n)
{
	size_t i;

	for (i = n - 1; i > 0; i--) {
		size_t rndidx = (size_t)random() % (i + 1);
		char *t = strs[i];
		strs[i] = strs[rndidx];
		strs[rndidx] = t;
	}
}

/* :O (order ascending) or :Or (order descending) or :Ox (shuffle) */
static ApplyModifierResult
ApplyModifier_Order(const char **pp, const char *val, ApplyModifiersState *st)
{
	const char *mod = (*pp)++;	/* skip past the 'O' in any case */

	Words words = Str_Words(val, FALSE);

	if (mod[1] == st->endc || mod[1] == ':') {
		/* :O sorts ascending */
		qsort(words.words, words.len, sizeof words.words[0],
		    str_cmp_asc);

	} else if ((mod[1] == 'r' || mod[1] == 'x') &&
		   (mod[2] == st->endc || mod[2] == ':')) {
		(*pp)++;

		if (mod[1] == 'r') {	/* :Or sorts descending */
			qsort(words.words, words.len, sizeof words.words[0],
			    str_cmp_desc);
		} else
			ShuffleStrings(words.words, words.len);
	} else {
		Words_Free(words);
		return AMR_BAD;
	}

	st->newVal = FStr_InitOwn(Words_JoinFree(words));
	return AMR_OK;
}

/* :? then : else */
static ApplyModifierResult
ApplyModifier_IfElse(const char **pp, ApplyModifiersState *st)
{
	char *then_expr, *else_expr;
	VarParseResult res;

	Boolean value = FALSE;
	VarEvalFlags then_eflags = VARE_NONE;
	VarEvalFlags else_eflags = VARE_NONE;

	int cond_rc = COND_PARSE;	/* anything other than COND_INVALID */
	if (st->eflags & VARE_WANTRES) {
		cond_rc = Cond_EvalCondition(st->var->name.str, &value);
		if (cond_rc != COND_INVALID && value)
			then_eflags = st->eflags;
		if (cond_rc != COND_INVALID && !value)
			else_eflags = st->eflags;
	}

	(*pp)++;			/* skip past the '?' */
	res = ParseModifierPart(pp, ':', then_eflags, st, &then_expr);
	if (res != VPR_OK)
		return AMR_CLEANUP;

	res = ParseModifierPart(pp, st->endc, else_eflags, st, &else_expr);
	if (res != VPR_OK)
		return AMR_CLEANUP;

	(*pp)--;
	if (cond_rc == COND_INVALID) {
		Error("Bad conditional expression `%s' in %s?%s:%s",
		    st->var->name.str, st->var->name.str, then_expr, else_expr);
		return AMR_CLEANUP;
	}

	if (value) {
		st->newVal = FStr_InitOwn(then_expr);
		free(else_expr);
	} else {
		st->newVal = FStr_InitOwn(else_expr);
		free(then_expr);
	}
	ApplyModifiersState_Define(st);
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
 *	@: ${t::=$i}
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
	GNode *ctxt;
	char delim;
	char *val;
	VarParseResult res;

	const char *mod = *pp;
	const char *op = mod + 1;

	if (op[0] == '=')
		goto ok;
	if ((op[0] == '!' || op[0] == '+' || op[0] == '?') && op[1] == '=')
		goto ok;
	return AMR_UNKNOWN;	/* "::<unrecognised>" */
ok:

	if (st->var->name.str[0] == '\0') {
		*pp = mod + 1;
		return AMR_BAD;
	}

	ctxt = st->ctxt;	/* context where v belongs */
	if (!(st->exprFlags & VEF_UNDEF) && st->ctxt != VAR_GLOBAL) {
		Var *gv = VarFind(st->var->name.str, st->ctxt, FALSE);
		if (gv == NULL)
			ctxt = VAR_GLOBAL;
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

	delim = st->startc == '(' ? ')' : '}';
	res = ParseModifierPart(pp, delim, st->eflags, st, &val);
	if (res != VPR_OK)
		return AMR_CLEANUP;

	(*pp)--;

	if (st->eflags & VARE_WANTRES) {
		switch (op[0]) {
		case '+':
			Var_Append(st->var->name.str, val, ctxt);
			break;
		case '!': {
			const char *errfmt;
			char *cmd_output = Cmd_Exec(val, &errfmt);
			if (errfmt != NULL)
				Error(errfmt, val);
			else
				Var_Set(st->var->name.str, cmd_output, ctxt);
			free(cmd_output);
			break;
		}
		case '?':
			if (!(st->exprFlags & VEF_UNDEF))
				break;
			/* FALLTHROUGH */
		default:
			Var_Set(st->var->name.str, val, ctxt);
			break;
		}
	}
	free(val);
	st->newVal = FStr_InitRefer("");
	return AMR_OK;
}

/*
 * :_=...
 * remember current value
 */
static ApplyModifierResult
ApplyModifier_Remember(const char **pp, const char *val,
		       ApplyModifiersState *st)
{
	const char *mod = *pp;
	if (!ModMatchEq(mod, "_", st->endc))
		return AMR_UNKNOWN;

	if (mod[1] == '=') {
		size_t n = strcspn(mod + 2, ":)}");
		char *name = bmake_strldup(mod + 2, n);
		Var_Set(name, val, st->ctxt);
		free(name);
		*pp = mod + 2 + n;
	} else {
		Var_Set("_", val, st->ctxt);
		*pp = mod + 1;
	}
	st->newVal = FStr_InitRefer(val);
	return AMR_OK;
}

/*
 * Apply the given function to each word of the variable value,
 * for a single-letter modifier such as :H, :T.
 */
static ApplyModifierResult
ApplyModifier_WordFunc(const char **pp, const char *val,
		       ApplyModifiersState *st, ModifyWordsCallback modifyWord)
{
	char delim = (*pp)[1];
	if (delim != st->endc && delim != ':')
		return AMR_UNKNOWN;

	st->newVal = FStr_InitOwn(ModifyWords(val, modifyWord, NULL,
	    st->oneBigWord, st->sep));
	(*pp)++;
	return AMR_OK;
}

static ApplyModifierResult
ApplyModifier_Unique(const char **pp, const char *val, ApplyModifiersState *st)
{
	if ((*pp)[1] == st->endc || (*pp)[1] == ':') {
		st->newVal = FStr_InitOwn(VarUniq(val));
		(*pp)++;
		return AMR_OK;
	} else
		return AMR_UNKNOWN;
}

#ifdef SYSVVARSUB
/* :from=to */
static ApplyModifierResult
ApplyModifier_SysV(const char **pp, const char *val, ApplyModifiersState *st)
{
	char *lhs, *rhs;
	VarParseResult res;

	const char *mod = *pp;
	Boolean eqFound = FALSE;

	/*
	 * First we make a pass through the string trying to verify it is a
	 * SysV-make-style translation. It must be: <lhs>=<rhs>
	 */
	int depth = 1;
	const char *p = mod;
	while (*p != '\0' && depth > 0) {
		if (*p == '=') {	/* XXX: should also test depth == 1 */
			eqFound = TRUE;
			/* continue looking for st->endc */
		} else if (*p == st->endc)
			depth--;
		else if (*p == st->startc)
			depth++;
		if (depth > 0)
			p++;
	}
	if (*p != st->endc || !eqFound)
		return AMR_UNKNOWN;

	res = ParseModifierPart(pp, '=', st->eflags, st, &lhs);
	if (res != VPR_OK)
		return AMR_CLEANUP;

	/* The SysV modifier lasts until the end of the variable expression. */
	res = ParseModifierPart(pp, st->endc, st->eflags, st, &rhs);
	if (res != VPR_OK)
		return AMR_CLEANUP;

	(*pp)--;
	if (lhs[0] == '\0' && val[0] == '\0') {
		st->newVal = FStr_InitRefer(val); /* special case */
	} else {
		struct ModifyWord_SYSVSubstArgs args = { st->ctxt, lhs, rhs };
		st->newVal = FStr_InitOwn(
		    ModifyWords(val, ModifyWord_SYSVSubst, &args,
			st->oneBigWord, st->sep));
	}
	free(lhs);
	free(rhs);
	return AMR_OK;
}
#endif

#ifdef SUNSHCMD
/* :sh */
static ApplyModifierResult
ApplyModifier_SunShell(const char **pp, const char *val,
		       ApplyModifiersState *st)
{
	const char *p = *pp;
	if (p[1] == 'h' && (p[2] == st->endc || p[2] == ':')) {
		if (st->eflags & VARE_WANTRES) {
			const char *errfmt;
			st->newVal = FStr_InitOwn(Cmd_Exec(val, &errfmt));
			if (errfmt != NULL)
				Error(errfmt, val);
		} else
			st->newVal = FStr_InitRefer("");
		*pp = p + 2;
		return AMR_OK;
	} else
		return AMR_UNKNOWN;
}
#endif

static void
LogBeforeApply(const ApplyModifiersState *st, const char *mod, char endc,
	       const char *val)
{
	char eflags_str[VarEvalFlags_ToStringSize];
	char vflags_str[VarFlags_ToStringSize];
	char exprflags_str[VarExprFlags_ToStringSize];
	Boolean is_single_char = mod[0] != '\0' &&
				 (mod[1] == endc || mod[1] == ':');

	/* At this point, only the first character of the modifier can
	 * be used since the end of the modifier is not yet known. */
	debug_printf("Applying ${%s:%c%s} to \"%s\" (%s, %s, %s)\n",
	    st->var->name.str, mod[0], is_single_char ? "" : "...", val,
	    Enum_FlagsToString(eflags_str, sizeof eflags_str,
		st->eflags, VarEvalFlags_ToStringSpecs),
	    Enum_FlagsToString(vflags_str, sizeof vflags_str,
		st->var->flags, VarFlags_ToStringSpecs),
	    Enum_FlagsToString(exprflags_str, sizeof exprflags_str,
		st->exprFlags,
		VarExprFlags_ToStringSpecs));
}

static void
LogAfterApply(ApplyModifiersState *st, const char *p, const char *mod)
{
	char eflags_str[VarEvalFlags_ToStringSize];
	char vflags_str[VarFlags_ToStringSize];
	char exprflags_str[VarExprFlags_ToStringSize];
	const char *quot = st->newVal.str == var_Error ? "" : "\"";
	const char *newVal =
	    st->newVal.str == var_Error ? "error" : st->newVal.str;

	debug_printf("Result of ${%s:%.*s} is %s%s%s (%s, %s, %s)\n",
	    st->var->name.str, (int)(p - mod), mod, quot, newVal, quot,
	    Enum_FlagsToString(eflags_str, sizeof eflags_str,
		st->eflags, VarEvalFlags_ToStringSpecs),
	    Enum_FlagsToString(vflags_str, sizeof vflags_str,
		st->var->flags, VarFlags_ToStringSpecs),
	    Enum_FlagsToString(exprflags_str, sizeof exprflags_str,
		st->exprFlags,
		VarExprFlags_ToStringSpecs));
}

static ApplyModifierResult
ApplyModifier(const char **pp, const char *val, ApplyModifiersState *st)
{
	switch (**pp) {
	case ':':
		return ApplyModifier_Assign(pp, st);
	case '@':
		return ApplyModifier_Loop(pp, val, st);
	case '_':
		return ApplyModifier_Remember(pp, val, st);
	case 'D':
	case 'U':
		return ApplyModifier_Defined(pp, val, st);
	case 'L':
		return ApplyModifier_Literal(pp, st);
	case 'P':
		return ApplyModifier_Path(pp, st);
	case '!':
		return ApplyModifier_ShellCommand(pp, st);
	case '[':
		return ApplyModifier_Words(pp, val, st);
	case 'g':
		return ApplyModifier_Gmtime(pp, val, st);
	case 'h':
		return ApplyModifier_Hash(pp, val, st);
	case 'l':
		return ApplyModifier_Localtime(pp, val, st);
	case 't':
		return ApplyModifier_To(pp, val, st);
	case 'N':
	case 'M':
		return ApplyModifier_Match(pp, val, st);
	case 'S':
		return ApplyModifier_Subst(pp, val, st);
	case '?':
		return ApplyModifier_IfElse(pp, st);
#ifndef NO_REGEX
	case 'C':
		return ApplyModifier_Regex(pp, val, st);
#endif
	case 'q':
	case 'Q':
		return ApplyModifier_Quote(pp, val, st);
	case 'T':
		return ApplyModifier_WordFunc(pp, val, st, ModifyWord_Tail);
	case 'H':
		return ApplyModifier_WordFunc(pp, val, st, ModifyWord_Head);
	case 'E':
		return ApplyModifier_WordFunc(pp, val, st, ModifyWord_Suffix);
	case 'R':
		return ApplyModifier_WordFunc(pp, val, st, ModifyWord_Root);
	case 'r':
		return ApplyModifier_Range(pp, val, st);
	case 'O':
		return ApplyModifier_Order(pp, val, st);
	case 'u':
		return ApplyModifier_Unique(pp, val, st);
#ifdef SUNSHCMD
	case 's':
		return ApplyModifier_SunShell(pp, val, st);
#endif
	default:
		return AMR_UNKNOWN;
	}
}

static FStr ApplyModifiers(const char **, FStr, char, char, Var *,
			    VarExprFlags *, GNode *, VarEvalFlags);

typedef enum ApplyModifiersIndirectResult {
	/* The indirect modifiers have been applied successfully. */
	AMIR_CONTINUE,
	/* Fall back to the SysV modifier. */
	AMIR_APPLY_MODS,
	/* Error out. */
	AMIR_OUT
} ApplyModifiersIndirectResult;

/*
 * While expanding a variable expression, expand and apply indirect modifiers,
 * such as in ${VAR:${M_indirect}}.
 *
 * All indirect modifiers of a group must come from a single variable
 * expression.  ${VAR:${M1}} is valid but ${VAR:${M1}${M2}} is not.
 *
 * Multiple groups of indirect modifiers can be chained by separating them
 * with colons.  ${VAR:${M1}:${M2}} contains 2 indirect modifiers.
 *
 * If the variable expression is not followed by st->endc or ':', fall
 * back to trying the SysV modifier, such as in ${VAR:${FROM}=${TO}}.
 *
 * The expression ${VAR:${M1}${M2}} is not treated as an indirect
 * modifier, and it is neither a SysV modifier but a parse error.
 */
static ApplyModifiersIndirectResult
ApplyModifiersIndirect(ApplyModifiersState *st, const char **pp,
		       FStr *inout_value)
{
	const char *p = *pp;
	FStr mods;

	(void)Var_Parse(&p, st->ctxt, st->eflags, &mods);
	/* TODO: handle errors */

	if (mods.str[0] != '\0' && *p != '\0' && *p != ':' && *p != st->endc) {
		FStr_Done(&mods);
		return AMIR_APPLY_MODS;
	}

	DEBUG3(VAR, "Indirect modifier \"%s\" from \"%.*s\"\n",
	    mods.str, (int)(p - *pp), *pp);

	if (mods.str[0] != '\0') {
		const char *modsp = mods.str;
		FStr newVal = ApplyModifiers(&modsp, *inout_value, '\0', '\0',
		    st->var, &st->exprFlags, st->ctxt, st->eflags);
		*inout_value = newVal;
		if (newVal.str == var_Error || *modsp != '\0') {
			FStr_Done(&mods);
			*pp = p;
			return AMIR_OUT;	/* error already reported */
		}
	}
	FStr_Done(&mods);

	if (*p == ':')
		p++;
	else if (*p == '\0' && st->endc != '\0') {
		Error("Unclosed variable specification after complex "
		      "modifier (expecting '%c') for %s",
		    st->endc, st->var->name.str);
		*pp = p;
		return AMIR_OUT;
	}

	*pp = p;
	return AMIR_CONTINUE;
}

static ApplyModifierResult
ApplySingleModifier(ApplyModifiersState *st, const char *mod, char endc,
		    const char **pp, FStr *inout_value)
{
	ApplyModifierResult res;
	const char *p = *pp;
	const char *const val = inout_value->str;

	if (DEBUG(VAR))
		LogBeforeApply(st, mod, endc, val);

	res = ApplyModifier(&p, val, st);

#ifdef SYSVVARSUB
	if (res == AMR_UNKNOWN) {
		assert(p == mod);
		res = ApplyModifier_SysV(&p, val, st);
	}
#endif

	if (res == AMR_UNKNOWN) {
		Parse_Error(PARSE_FATAL, "Unknown modifier '%c'", *mod);
		/*
		 * Guess the end of the current modifier.
		 * XXX: Skipping the rest of the modifier hides
		 * errors and leads to wrong results.
		 * Parsing should rather stop here.
		 */
		for (p++; *p != ':' && *p != st->endc && *p != '\0'; p++)
			continue;
		st->newVal = FStr_InitRefer(var_Error);
	}
	if (res == AMR_CLEANUP || res == AMR_BAD) {
		*pp = p;
		return res;
	}

	if (DEBUG(VAR))
		LogAfterApply(st, p, mod);

	if (st->newVal.str != val) {
		FStr_Done(inout_value);
		*inout_value = st->newVal;
	}
	if (*p == '\0' && st->endc != '\0') {
		Error(
		    "Unclosed variable specification (expecting '%c') "
		    "for \"%s\" (value \"%s\") modifier %c",
		    st->endc, st->var->name.str, inout_value->str, *mod);
	} else if (*p == ':') {
		p++;
	} else if (opts.strict && *p != '\0' && *p != endc) {
		Parse_Error(PARSE_FATAL,
		    "Missing delimiter ':' after modifier \"%.*s\"",
		    (int)(p - mod), mod);
		/*
		 * TODO: propagate parse error to the enclosing
		 * expression
		 */
	}
	*pp = p;
	return AMR_OK;
}

/* Apply any modifiers (such as :Mpattern or :@var@loop@ or :Q or ::=value). */
static FStr
ApplyModifiers(
    const char **pp,		/* the parsing position, updated upon return */
    FStr value,			/* the current value of the expression */
    char startc,		/* '(' or '{', or '\0' for indirect modifiers */
    char endc,			/* ')' or '}', or '\0' for indirect modifiers */
    Var *v,
    VarExprFlags *exprFlags,
    GNode *ctxt,		/* for looking up and modifying variables */
    VarEvalFlags eflags
)
{
	ApplyModifiersState st = {
	    startc, endc, v, ctxt, eflags,
	    FStr_InitRefer(var_Error), /* .newVal */
	    ' ',		/* .sep */
	    FALSE,		/* .oneBigWord */
	    *exprFlags		/* .exprFlags */
	};
	const char *p;
	const char *mod;

	assert(startc == '(' || startc == '{' || startc == '\0');
	assert(endc == ')' || endc == '}' || endc == '\0');
	assert(value.str != NULL);

	p = *pp;

	if (*p == '\0' && endc != '\0') {
		Error(
		    "Unclosed variable expression (expecting '%c') for \"%s\"",
		    st.endc, st.var->name.str);
		goto cleanup;
	}

	while (*p != '\0' && *p != endc) {
		ApplyModifierResult res;

		if (*p == '$') {
			ApplyModifiersIndirectResult amir;
			amir = ApplyModifiersIndirect(&st, &p, &value);
			if (amir == AMIR_CONTINUE)
				continue;
			if (amir == AMIR_OUT)
				break;
		}

		/* default value, in case of errors */
		st.newVal = FStr_InitRefer(var_Error);
		mod = p;

		res = ApplySingleModifier(&st, mod, endc, &p, &value);
		if (res == AMR_CLEANUP)
			goto cleanup;
		if (res == AMR_BAD)
			goto bad_modifier;
	}

	*pp = p;
	assert(value.str != NULL); /* Use var_Error or varUndefined instead. */
	*exprFlags = st.exprFlags;
	return value;

bad_modifier:
	/* XXX: The modifier end is only guessed. */
	Error("Bad modifier `:%.*s' for %s",
	    (int)strcspn(mod, ":)}"), mod, st.var->name.str);

cleanup:
	*pp = p;
	FStr_Done(&value);
	*exprFlags = st.exprFlags;
	return FStr_InitRefer(var_Error);
}

/*
 * Only four of the local variables are treated specially as they are the
 * only four that will be set when dynamic sources are expanded.
 */
static Boolean
VarnameIsDynamic(const char *name, size_t len)
{
	if (len == 1 || (len == 2 && (name[1] == 'F' || name[1] == 'D'))) {
		switch (name[0]) {
		case '@':
		case '%':
		case '*':
		case '!':
			return TRUE;
		}
		return FALSE;
	}

	if ((len == 7 || len == 8) && name[0] == '.' && ch_isupper(name[1])) {
		return strcmp(name, ".TARGET") == 0 ||
		       strcmp(name, ".ARCHIVE") == 0 ||
		       strcmp(name, ".PREFIX") == 0 ||
		       strcmp(name, ".MEMBER") == 0;
	}

	return FALSE;
}

static const char *
UndefinedShortVarValue(char varname, const GNode *ctxt)
{
	if (ctxt == VAR_CMDLINE || ctxt == VAR_GLOBAL) {
		/*
		 * If substituting a local variable in a non-local context,
		 * assume it's for dynamic source stuff. We have to handle
		 * this specially and return the longhand for the variable
		 * with the dollar sign escaped so it makes it back to the
		 * caller. Only four of the local variables are treated
		 * specially as they are the only four that will be set
		 * when dynamic sources are expanded.
		 */
		switch (varname) {
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
	return NULL;
}

/*
 * Parse a variable name, until the end character or a colon, whichever
 * comes first.
 */
static char *
ParseVarname(const char **pp, char startc, char endc,
	     GNode *ctxt, VarEvalFlags eflags,
	     size_t *out_varname_len)
{
	Buffer buf;
	const char *p = *pp;
	int depth = 1;

	Buf_Init(&buf);

	while (*p != '\0') {
		/* Track depth so we can spot parse errors. */
		if (*p == startc)
			depth++;
		if (*p == endc) {
			if (--depth == 0)
				break;
		}
		if (*p == ':' && depth == 1)
			break;

		/* A variable inside a variable, expand. */
		if (*p == '$') {
			FStr nested_val;
			(void)Var_Parse(&p, ctxt, eflags, &nested_val);
			/* TODO: handle errors */
			Buf_AddStr(&buf, nested_val.str);
			FStr_Done(&nested_val);
		} else {
			Buf_AddByte(&buf, *p);
			p++;
		}
	}
	*pp = p;
	*out_varname_len = Buf_Len(&buf);
	return Buf_Destroy(&buf, FALSE);
}

static VarParseResult
ValidShortVarname(char varname, const char *start)
{
	switch (varname) {
	case '\0':
	case ')':
	case '}':
	case ':':
	case '$':
		break;		/* and continue below */
	default:
		return VPR_OK;
	}

	if (!opts.strict)
		return VPR_ERR;	/* XXX: Missing error message */

	if (varname == '$')
		Parse_Error(PARSE_FATAL,
		    "To escape a dollar, use \\$, not $$, at \"%s\"", start);
	else if (varname == '\0')
		Parse_Error(PARSE_FATAL, "Dollar followed by nothing");
	else
		Parse_Error(PARSE_FATAL,
		    "Invalid variable name '%c', at \"%s\"", varname, start);

	return VPR_ERR;
}

/*
 * Parse a single-character variable name such as $V or $@.
 * Return whether to continue parsing.
 */
static Boolean
ParseVarnameShort(char startc, const char **pp, GNode *ctxt,
		  VarEvalFlags eflags,
		  VarParseResult *out_FALSE_res, const char **out_FALSE_val,
		  Var **out_TRUE_var)
{
	char name[2];
	Var *v;
	VarParseResult vpr;

	/*
	 * If it's not bounded by braces of some sort, life is much simpler.
	 * We just need to check for the first character and return the
	 * value if it exists.
	 */

	vpr = ValidShortVarname(startc, *pp);
	if (vpr != VPR_OK) {
		(*pp)++;
		*out_FALSE_val = var_Error;
		*out_FALSE_res = vpr;
		return FALSE;
	}

	name[0] = startc;
	name[1] = '\0';
	v = VarFind(name, ctxt, TRUE);
	if (v == NULL) {
		const char *val;
		*pp += 2;

		val = UndefinedShortVarValue(startc, ctxt);
		if (val == NULL)
			val = eflags & VARE_UNDEFERR ? var_Error : varUndefined;

		if (opts.strict && val == var_Error) {
			Parse_Error(PARSE_FATAL,
			    "Variable \"%s\" is undefined", name);
			*out_FALSE_res = VPR_ERR;
			*out_FALSE_val = val;
			return FALSE;
		}

		/*
		 * XXX: This looks completely wrong.
		 *
		 * If undefined expressions are not allowed, this should
		 * rather be VPR_ERR instead of VPR_UNDEF, together with an
		 * error message.
		 *
		 * If undefined expressions are allowed, this should rather
		 * be VPR_UNDEF instead of VPR_OK.
		 */
		*out_FALSE_res = eflags & VARE_UNDEFERR ? VPR_UNDEF : VPR_OK;
		*out_FALSE_val = val;
		return FALSE;
	}

	*out_TRUE_var = v;
	return TRUE;
}

/* Find variables like @F or <D. */
static Var *
FindLocalLegacyVar(const char *varname, size_t namelen, GNode *ctxt,
		   const char **out_extraModifiers)
{
	/* Only resolve these variables if ctxt is a "real" target. */
	if (ctxt == VAR_CMDLINE || ctxt == VAR_GLOBAL)
		return NULL;

	if (namelen != 2)
		return NULL;
	if (varname[1] != 'F' && varname[1] != 'D')
		return NULL;
	if (strchr("@%?*!<>", varname[0]) == NULL)
		return NULL;

	{
		char name[] = { varname[0], '\0' };
		Var *v = VarFind(name, ctxt, FALSE);

		if (v != NULL) {
			if (varname[1] == 'D') {
				*out_extraModifiers = "H:";
			} else { /* F */
				*out_extraModifiers = "T:";
			}
		}
		return v;
	}
}

static VarParseResult
EvalUndefined(Boolean dynamic, const char *start, const char *p, char *varname,
	      VarEvalFlags eflags,
	      FStr *out_val)
{
	if (dynamic) {
		*out_val = FStr_InitOwn(bmake_strsedup(start, p));
		free(varname);
		return VPR_OK;
	}

	if ((eflags & VARE_UNDEFERR) && opts.strict) {
		Parse_Error(PARSE_FATAL,
		    "Variable \"%s\" is undefined", varname);
		free(varname);
		*out_val = FStr_InitRefer(var_Error);
		return VPR_ERR;
	}

	if (eflags & VARE_UNDEFERR) {
		free(varname);
		*out_val = FStr_InitRefer(var_Error);
		return VPR_UNDEF;	/* XXX: Should be VPR_ERR instead. */
	}

	free(varname);
	*out_val = FStr_InitRefer(varUndefined);
	return VPR_OK;
}

/*
 * Parse a long variable name enclosed in braces or parentheses such as $(VAR)
 * or ${VAR}, up to the closing brace or parenthesis, or in the case of
 * ${VAR:Modifiers}, up to the ':' that starts the modifiers.
 * Return whether to continue parsing.
 */
static Boolean
ParseVarnameLong(
	const char *p,
	char startc,
	GNode *ctxt,
	VarEvalFlags eflags,

	const char **out_FALSE_pp,
	VarParseResult *out_FALSE_res,
	FStr *out_FALSE_val,

	char *out_TRUE_endc,
	const char **out_TRUE_p,
	Var **out_TRUE_v,
	Boolean *out_TRUE_haveModifier,
	const char **out_TRUE_extraModifiers,
	Boolean *out_TRUE_dynamic,
	VarExprFlags *out_TRUE_exprFlags
)
{
	size_t namelen;
	char *varname;
	Var *v;
	Boolean haveModifier;
	Boolean dynamic = FALSE;

	const char *const start = p;
	char endc = startc == '(' ? ')' : '}';

	p += 2;			/* skip "${" or "$(" or "y(" */
	varname = ParseVarname(&p, startc, endc, ctxt, eflags, &namelen);

	if (*p == ':') {
		haveModifier = TRUE;
	} else if (*p == endc) {
		haveModifier = FALSE;
	} else {
		Parse_Error(PARSE_FATAL, "Unclosed variable \"%s\"", varname);
		free(varname);
		*out_FALSE_pp = p;
		*out_FALSE_val = FStr_InitRefer(var_Error);
		*out_FALSE_res = VPR_ERR;
		return FALSE;
	}

	v = VarFind(varname, ctxt, TRUE);

	/* At this point, p points just after the variable name,
	 * either at ':' or at endc. */

	if (v == NULL) {
		v = FindLocalLegacyVar(varname, namelen, ctxt,
		    out_TRUE_extraModifiers);
	}

	if (v == NULL) {
		/*
		 * Defer expansion of dynamic variables if they appear in
		 * non-local context since they are not defined there.
		 */
		dynamic = VarnameIsDynamic(varname, namelen) &&
			  (ctxt == VAR_CMDLINE || ctxt == VAR_GLOBAL);

		if (!haveModifier) {
			p++;	/* skip endc */
			*out_FALSE_pp = p;
			*out_FALSE_res = EvalUndefined(dynamic, start, p,
			    varname, eflags, out_FALSE_val);
			return FALSE;
		}

		/*
		 * The variable expression is based on an undefined variable.
		 * Nevertheless it needs a Var, for modifiers that access the
		 * variable name, such as :L or :?.
		 *
		 * Most modifiers leave this expression in the "undefined"
		 * state (VEF_UNDEF), only a few modifiers like :D, :U, :L,
		 * :P turn this undefined expression into a defined
		 * expression (VEF_DEF).
		 *
		 * At the end, after applying all modifiers, if the expression
		 * is still undefined, Var_Parse will return an empty string
		 * instead of the actually computed value.
		 */
		v = VarNew(FStr_InitOwn(varname), "", VAR_NONE);
		*out_TRUE_exprFlags = VEF_UNDEF;
	} else
		free(varname);

	*out_TRUE_endc = endc;
	*out_TRUE_p = p;
	*out_TRUE_v = v;
	*out_TRUE_haveModifier = haveModifier;
	*out_TRUE_dynamic = dynamic;
	return TRUE;
}

/* Free the environment variable now since we own it. */
static void
FreeEnvVar(void **out_val_freeIt, Var *v, const char *value)
{
	char *varValue = Buf_Destroy(&v->val, FALSE);
	if (value == varValue)
		*out_val_freeIt = varValue;
	else
		free(varValue);

	FStr_Done(&v->name);
	free(v);
}

/*
 * Given the start of a variable expression (such as $v, $(VAR),
 * ${VAR:Mpattern}), extract the variable name and value, and the modifiers,
 * if any.  While doing that, apply the modifiers to the value of the
 * expression, forming its final value.  A few of the modifiers such as :!cmd!
 * or ::= have side effects.
 *
 * Input:
 *	*pp		The string to parse.
 *			When parsing a condition in ParseEmptyArg, it may also
 *			point to the "y" of "empty(VARNAME:Modifiers)", which
 *			is syntactically the same.
 *	ctxt		The context for finding variables
 *	eflags		Control the exact details of parsing
 *
 * Output:
 *	*pp		The position where to continue parsing.
 *			TODO: After a parse error, the value of *pp is
 *			unspecified.  It may not have been updated at all,
 *			point to some random character in the string, to the
 *			location of the parse error, or at the end of the
 *			string.
 *	*out_val	The value of the variable expression, never NULL.
 *	*out_val	var_Error if there was a parse error.
 *	*out_val	var_Error if the base variable of the expression was
 *			undefined, eflags contains VARE_UNDEFERR, and none of
 *			the modifiers turned the undefined expression into a
 *			defined expression.
 *			XXX: It is not guaranteed that an error message has
 *			been printed.
 *	*out_val	varUndefined if the base variable of the expression
 *			was undefined, eflags did not contain VARE_UNDEFERR,
 *			and none of the modifiers turned the undefined
 *			expression into a defined expression.
 *			XXX: It is not guaranteed that an error message has
 *			been printed.
 *	*out_val_freeIt	Must be freed by the caller after using *out_val.
 */
/* coverity[+alloc : arg-*4] */
VarParseResult
Var_Parse(const char **pp, GNode *ctxt, VarEvalFlags eflags, FStr *out_val)
{
	const char *p = *pp;
	const char *const start = p;
	/* TRUE if have modifiers for the variable. */
	Boolean haveModifier;
	/* Starting character if variable in parens or braces. */
	char startc;
	/* Ending character if variable in parens or braces. */
	char endc;
	/*
	 * TRUE if the variable is local and we're expanding it in a
	 * non-local context. This is done to support dynamic sources.
	 * The result is just the expression, unaltered.
	 */
	Boolean dynamic;
	const char *extramodifiers;
	Var *v;
	FStr value;
	char eflags_str[VarEvalFlags_ToStringSize];
	VarExprFlags exprFlags = VEF_NONE;

	DEBUG2(VAR, "Var_Parse: %s with %s\n", start,
	    Enum_FlagsToString(eflags_str, sizeof eflags_str, eflags,
		VarEvalFlags_ToStringSpecs));

	*out_val = FStr_InitRefer(NULL);
	extramodifiers = NULL;	/* extra modifiers to apply first */
	dynamic = FALSE;

	/*
	 * Appease GCC, which thinks that the variable might not be
	 * initialized.
	 */
	endc = '\0';

	startc = p[1];
	if (startc != '(' && startc != '{') {
		VarParseResult res;
		if (!ParseVarnameShort(startc, pp, ctxt, eflags, &res,
		    &out_val->str, &v))
			return res;
		haveModifier = FALSE;
		p++;
	} else {
		VarParseResult res;
		if (!ParseVarnameLong(p, startc, ctxt, eflags,
		    pp, &res, out_val,
		    &endc, &p, &v, &haveModifier, &extramodifiers,
		    &dynamic, &exprFlags))
			return res;
	}

	if (v->flags & VAR_IN_USE)
		Fatal("Variable %s is recursive.", v->name.str);

	/*
	 * XXX: This assignment creates an alias to the current value of the
	 * variable.  This means that as long as the value of the expression
	 * stays the same, the value of the variable must not change.
	 * Using the '::=' modifier, it could be possible to do exactly this.
	 * At the bottom of this function, the resulting value is compared to
	 * the then-current value of the variable.  This might also invoke
	 * undefined behavior.
	 */
	value = FStr_InitRefer(Buf_GetAll(&v->val, NULL));

	/*
	 * Before applying any modifiers, expand any nested expressions from
	 * the variable value.
	 */
	if (strchr(value.str, '$') != NULL && (eflags & VARE_WANTRES)) {
		char *expanded;
		VarEvalFlags nested_eflags = eflags;
		if (opts.strict)
			nested_eflags &= ~(unsigned)VARE_UNDEFERR;
		v->flags |= VAR_IN_USE;
		(void)Var_Subst(value.str, ctxt, nested_eflags, &expanded);
		v->flags &= ~(unsigned)VAR_IN_USE;
		/* TODO: handle errors */
		value = FStr_InitOwn(expanded);
	}

	if (haveModifier || extramodifiers != NULL) {
		if (extramodifiers != NULL) {
			const char *em = extramodifiers;
			value = ApplyModifiers(&em, value, '\0', '\0',
			    v, &exprFlags, ctxt, eflags);
		}

		if (haveModifier) {
			p++;	/* Skip initial colon. */

			value = ApplyModifiers(&p, value, startc, endc,
			    v, &exprFlags, ctxt, eflags);
		}
	}

	if (*p != '\0')		/* Skip past endc if possible. */
		p++;

	*pp = p;

	if (v->flags & VAR_FROM_ENV) {
		FreeEnvVar(&value.freeIt, v, value.str);

	} else if (exprFlags & VEF_UNDEF) {
		if (!(exprFlags & VEF_DEF)) {
			FStr_Done(&value);
			if (dynamic) {
				value = FStr_InitOwn(bmake_strsedup(start, p));
			} else {
				/*
				 * The expression is still undefined,
				 * therefore discard the actual value and
				 * return an error marker instead.
				 */
				value = FStr_InitRefer(eflags & VARE_UNDEFERR
				    ? var_Error : varUndefined);
			}
		}
		if (value.str != Buf_GetAll(&v->val, NULL))
			Buf_Destroy(&v->val, TRUE);
		FStr_Done(&v->name);
		free(v);
	}
	*out_val = (FStr){ value.str, value.freeIt };
	return VPR_OK;		/* XXX: Is not correct in all cases */
}

static void
VarSubstDollarDollar(const char **pp, Buffer *res, VarEvalFlags eflags)
{
	/*
	 * A dollar sign may be escaped with another dollar
	 * sign.
	 */
	if (save_dollars && (eflags & VARE_KEEP_DOLLAR))
		Buf_AddByte(res, '$');
	Buf_AddByte(res, '$');
	*pp += 2;
}

static void
VarSubstExpr(const char **pp, Buffer *buf, GNode *ctxt,
	     VarEvalFlags eflags, Boolean *inout_errorReported)
{
	const char *p = *pp;
	const char *nested_p = p;
	FStr val;

	(void)Var_Parse(&nested_p, ctxt, eflags, &val);
	/* TODO: handle errors */

	if (val.str == var_Error || val.str == varUndefined) {
		if (!(eflags & VARE_KEEP_UNDEF)) {
			p = nested_p;
		} else if ((eflags & VARE_UNDEFERR) || val.str == var_Error) {

			/*
			 * XXX: This condition is wrong.  If val == var_Error,
			 * this doesn't necessarily mean there was an undefined
			 * variable.  It could equally well be a parse error;
			 * see unit-tests/varmod-order.exp.
			 */

			/*
			 * If variable is undefined, complain and skip the
			 * variable. The complaint will stop us from doing
			 * anything when the file is parsed.
			 */
			if (!*inout_errorReported) {
				Parse_Error(PARSE_FATAL,
				    "Undefined variable \"%.*s\"",
				    (int)(size_t)(nested_p - p), p);
			}
			p = nested_p;
			*inout_errorReported = TRUE;
		} else {
			/* Copy the initial '$' of the undefined expression,
			 * thereby deferring expansion of the expression, but
			 * expand nested expressions if already possible.
			 * See unit-tests/varparse-undef-partial.mk. */
			Buf_AddByte(buf, *p);
			p++;
		}
	} else {
		p = nested_p;
		Buf_AddStr(buf, val.str);
	}

	FStr_Done(&val);

	*pp = p;
}

/*
 * Skip as many characters as possible -- either to the end of the string
 * or to the next dollar sign (variable expression).
 */
static void
VarSubstPlain(const char **pp, Buffer *res)
{
	const char *p = *pp;
	const char *start = p;

	for (p++; *p != '$' && *p != '\0'; p++)
		continue;
	Buf_AddBytesBetween(res, start, p);
	*pp = p;
}

/*
 * Expand all variable expressions like $V, ${VAR}, $(VAR:Modifiers) in the
 * given string.
 *
 * Input:
 *	str		The string in which the variable expressions are
 *			expanded.
 *	ctxt		The context in which to start searching for
 *			variables.  The other contexts are searched as well.
 *	eflags		Special effects during expansion.
 */
VarParseResult
Var_Subst(const char *str, GNode *ctxt, VarEvalFlags eflags, char **out_res)
{
	const char *p = str;
	Buffer res;

	/* Set true if an error has already been reported,
	 * to prevent a plethora of messages when recursing */
	/* XXX: Why is the 'static' necessary here? */
	static Boolean errorReported;

	Buf_Init(&res);
	errorReported = FALSE;

	while (*p != '\0') {
		if (p[0] == '$' && p[1] == '$')
			VarSubstDollarDollar(&p, &res, eflags);
		else if (p[0] == '$')
			VarSubstExpr(&p, &res, ctxt, eflags, &errorReported);
		else
			VarSubstPlain(&p, &res);
	}

	*out_res = Buf_DestroyCompact(&res);
	return VPR_OK;
}

/* Initialize the variables module. */
void
Var_Init(void)
{
	VAR_INTERNAL = GNode_New("Internal");
	VAR_GLOBAL = GNode_New("Global");
	VAR_CMDLINE = GNode_New("Command");
}

/* Clean up the variables module. */
void
Var_End(void)
{
	Var_Stats();
}

void
Var_Stats(void)
{
	HashTable_DebugStats(&VAR_GLOBAL->vars, "VAR_GLOBAL");
}

/* Print all variables in a context, sorted by name. */
void
Var_Dump(GNode *ctxt)
{
	Vector /* of const char * */ vec;
	HashIter hi;
	size_t i;
	const char **varnames;

	Vector_Init(&vec, sizeof(const char *));

	HashIter_Init(&hi, &ctxt->vars);
	while (HashIter_Next(&hi) != NULL)
		*(const char **)Vector_Push(&vec) = hi.entry->key;
	varnames = vec.items;

	qsort(varnames, vec.len, sizeof varnames[0], str_cmp_asc);

	for (i = 0; i < vec.len; i++) {
		const char *varname = varnames[i];
		Var *var = HashTable_FindValue(&ctxt->vars, varname);
		debug_printf("%-16s = %s\n",
		    varname, Buf_GetAll(&var->val, NULL));
	}

	Vector_Done(&vec);
}
