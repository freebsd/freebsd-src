/*	$NetBSD: var.c,v 1.1171 2025/06/29 11:02:17 rillig Exp $	*/

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
 *	Var_Set
 *	Var_SetExpand	Set the value of the variable, creating it if
 *			necessary.
 *
 *	Var_Append
 *	Var_AppendExpand
 *			Append more characters to the variable, creating it if
 *			necessary. A space is placed between the old value and
 *			the new one.
 *
 *	Var_Exists
 *	Var_ExistsExpand
 *			See if a variable exists.
 *
 *	Var_Value	Return the unexpanded value of a variable, or NULL if
 *			the variable is undefined.
 *
 *	Var_Subst	Substitute all expressions in a string.
 *
 *	Var_Parse	Parse an expression such as ${VAR:Mpattern}.
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
 *	Var_Dump	Print out all variables defined in the given scope.
 */

#include <sys/stat.h>
#include <sys/types.h>

#include "make.h"

#include <errno.h>
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <time.h>

#include "dir.h"
#include "job.h"
#include "metachar.h"

#ifndef SIZE_MAX
#define SIZE_MAX 0xffffffffUL
#endif

/*	"@(#)var.c	8.3 (Berkeley) 3/19/94" */
MAKE_RCSID("$NetBSD: var.c,v 1.1171 2025/06/29 11:02:17 rillig Exp $");

/*
 * Variables are defined using one of the VAR=value assignments.  Their
 * value can be queried by expressions such as $V, ${VAR}, or with modifiers
 * such as ${VAR:S,from,to,g:Q}.
 *
 * There are 3 kinds of variables: scope variables, environment variables,
 * undefined variables.
 *
 * Scope variables are stored in GNode.vars.  The only way to undefine
 * a scope variable is using the .undef directive.  In particular, it must
 * not be possible to undefine a variable during the evaluation of an
 * expression, or Var.name might point nowhere.  (There is another,
 * unintended way to undefine a scope variable, see varmod-loop-delete.mk.)
 *
 * Environment variables are short-lived.  They are returned by VarFind, and
 * after using them, they must be freed using VarFreeShortLived.
 *
 * Undefined variables occur during evaluation of expressions such
 * as ${UNDEF:Ufallback} in Var_Parse and ApplyModifiers.
 */
typedef struct Var {
	/*
	 * The name of the variable, once set, doesn't change anymore.
	 * For scope variables, it aliases the corresponding HashEntry name.
	 * For environment and undefined variables, it is allocated.
	 */
	FStr name;

	/* The unexpanded value of the variable. */
	Buffer val;

	/* The variable came from the command line. */
	bool fromCmd:1;

	/*
	 * The variable is short-lived.
	 * These variables are not registered in any GNode, therefore they
	 * must be freed after use.
	 */
	bool shortLived:1;

	/*
	 * The variable comes from the environment.
	 * Appending to its value depends on the scope, see var-op-append.mk.
	 */
	bool fromEnvironment:1;

	/*
	 * The variable value cannot be changed anymore, and the variable
	 * cannot be deleted.  Any attempts to do so are silently ignored,
	 * they are logged with -dv though.
	 * Use .[NO]READONLY: to adjust.
	 *
	 * See VAR_SET_READONLY.
	 */
	bool readOnly:1;

	/*
	 * The variable is read-only and immune to the .NOREADONLY special
	 * target.  Any attempt to modify it results in an error.
	 */
	bool readOnlyLoud:1;

	/*
	 * The variable is currently being accessed by Var_Parse or Var_Subst.
	 * This temporary marker is used to avoid endless recursion.
	 */
	bool inUse:1;

	/*
	 * The variable is exported to the environment, to be used by child
	 * processes.
	 */
	bool exported:1;

	/*
	 * At the point where this variable was exported, it contained an
	 * unresolved reference to another variable.  Before any child
	 * process is started, it needs to be actually exported, resolving
	 * the referenced variable just in time.
	 */
	bool reexport:1;
} Var;

/*
 * Exporting variables is expensive and may leak memory, so skip it if we
 * can.
 */
typedef enum VarExportedMode {
	VAR_EXPORTED_NONE,
	VAR_EXPORTED_SOME,
	VAR_EXPORTED_ALL
} VarExportedMode;

typedef enum UnexportWhat {
	/* Unexport the variables given by name. */
	UNEXPORT_NAMED,
	/*
	 * Unexport all globals previously exported, but keep the environment
	 * inherited from the parent.
	 */
	UNEXPORT_ALL,
	/*
	 * Unexport all globals previously exported and clear the environment
	 * inherited from the parent.
	 */
	UNEXPORT_ENV
} UnexportWhat;

/* Flags for pattern matching in the :S and :C modifiers */
typedef struct PatternFlags {
	bool subGlobal:1;	/* 'g': replace as often as possible */
	bool subOnce:1;		/* '1': replace only once */
	bool anchorStart:1;	/* '^': match only at start of word */
	bool anchorEnd:1;	/* '$': match only at end of word */
} PatternFlags;

/* SepBuf builds a string from words interleaved with separators. */
typedef struct SepBuf {
	Buffer buf;
	bool needSep;
	/* Usually ' ', but see the ':ts' modifier. */
	char sep;
} SepBuf;

typedef enum {
	VSK_MAKEFLAGS,
	VSK_TARGET,
	VSK_COMMAND,
	VSK_VARNAME,
	VSK_INDIRECT_MODIFIERS,
	VSK_COND,
	VSK_COND_THEN,
	VSK_COND_ELSE,
	VSK_EXPR,
	VSK_EXPR_PARSE
} EvalStackElementKind;

typedef struct {
	EvalStackElementKind kind;
	const char *str;
	const FStr *value;
} EvalStackElement;

typedef struct {
	EvalStackElement *elems;
	size_t len;
	size_t cap;
} EvalStack;

/* Whether we have replaced the original environ (which we cannot free). */
char **savedEnv = NULL;

/*
 * Special return value for Var_Parse, indicating a parse error.  It may be
 * caused by an undefined variable, a syntax error in a modifier or
 * something entirely different.
 */
char var_Error[] = "";

/*
 * Special return value for Var_Parse, indicating an undefined variable in
 * a case where VARE_EVAL_DEFINED is not set.  This undefined variable is
 * typically a dynamic variable such as ${.TARGET}, whose expansion needs to
 * be deferred until it is defined in an actual target.
 *
 * See VARE_EVAL_KEEP_UNDEFINED.
 */
static char varUndefined[] = "";

/*
 * Traditionally this make consumed $$ during := like any other expansion.
 * Other make's do not, and this make follows straight since 2016-01-09.
 *
 * This knob allows controlling the behavior:
 *	false to consume $$ during := assignment.
 *	true to preserve $$ during := assignment.
 */
#define MAKE_SAVE_DOLLARS ".MAKE.SAVE_DOLLARS"
static bool save_dollars = false;

/*
 * A scope collects variable names and their values.
 *
 * The main scope is SCOPE_GLOBAL, which contains the variables that are set
 * in the makefiles.  SCOPE_INTERNAL acts as a fallback for SCOPE_GLOBAL and
 * contains some internal make variables.  These internal variables can thus
 * be overridden, they can also be restored by undefining the overriding
 * variable.
 *
 * SCOPE_CMDLINE contains variables from the command line arguments.  These
 * override variables from SCOPE_GLOBAL.
 *
 * There is no scope for environment variables, these are generated on-the-fly
 * whenever they are referenced.
 *
 * Each target has its own scope, containing the 7 target-local variables
 * .TARGET, .ALLSRC, etc.  Variables set on dependency lines also go in
 * this scope.
 */

GNode *SCOPE_CMDLINE;
GNode *SCOPE_GLOBAL;
GNode *SCOPE_INTERNAL;

static VarExportedMode var_exportedVars = VAR_EXPORTED_NONE;

static const char VarEvalMode_Name[][32] = {
	"parse",
	"parse-balanced",
	"eval",
	"eval-defined-loud",
	"eval-defined",
	"eval-keep-undefined",
	"eval-keep-dollar-and-undefined",
};

static EvalStack evalStack;


static void
EvalStack_Push(EvalStackElementKind kind, const char *str, const FStr *value)
{
	if (evalStack.len >= evalStack.cap) {
		evalStack.cap = 16 + 2 * evalStack.cap;
		evalStack.elems = bmake_realloc(evalStack.elems,
		    evalStack.cap * sizeof(*evalStack.elems));
	}
	evalStack.elems[evalStack.len].kind = kind;
	evalStack.elems[evalStack.len].str = str;
	evalStack.elems[evalStack.len].value = value;
	evalStack.len++;
}

void
EvalStack_PushMakeflags(const char *makeflags)
{
	EvalStack_Push(VSK_MAKEFLAGS, makeflags, NULL);
}

void
EvalStack_Pop(void)
{
	assert(evalStack.len > 0);
	evalStack.len--;
}

bool
EvalStack_Details(Buffer *buf)
{
	size_t i;

	for (i = evalStack.len; i > 0; i--) {
		static const char descr[][42] = {
			"while evaluating MAKEFLAGS",
			"in target",
			"in command",
			"while evaluating variable",
			"while evaluating indirect modifiers",
			"while evaluating condition",
			"while evaluating then-branch of condition",
			"while evaluating else-branch of condition",
			"while evaluating",
			"while parsing",
		};
		EvalStackElement *elem = evalStack.elems + i - 1;
		EvalStackElementKind kind = elem->kind;
		const char* value = elem->value != NULL
		    && (kind == VSK_VARNAME || kind == VSK_EXPR)
		    ? elem->value->str : NULL;

		Buf_AddStr(buf, "\t");
		Buf_AddStr(buf, descr[kind]);
		Buf_AddStr(buf, " \"");
		Buf_AddStr(buf, elem->str);
		if (value != NULL) {
			Buf_AddStr(buf, "\" with value \"");
			Buf_AddStr(buf, value);
		}
		Buf_AddStr(buf, "\"\n");
	}
	return evalStack.len > 0;
}

static Var *
VarNew(FStr name, const char *value,
       bool shortLived, bool fromEnvironment, bool readOnly)
{
	size_t value_len = strlen(value);
	Var *var = bmake_malloc(sizeof *var);
	var->name = name;
	Buf_InitSize(&var->val, value_len + 1);
	Buf_AddBytes(&var->val, value, value_len);
	var->fromCmd = false;
	var->shortLived = shortLived;
	var->fromEnvironment = fromEnvironment;
	var->readOnly = readOnly;
	var->readOnlyLoud = false;
	var->inUse = false;
	var->exported = false;
	var->reexport = false;
	return var;
}

static Substring
CanonicalVarname(Substring name)
{
	if (Substring_Equals(name, "^"))
		return Substring_InitStr(ALLSRC);

	if (!(Substring_Length(name) > 0 && name.start[0] == '.'))
		return name;

	if (Substring_Equals(name, ".ALLSRC"))
		return Substring_InitStr(ALLSRC);
	if (Substring_Equals(name, ".ARCHIVE"))
		return Substring_InitStr(ARCHIVE);
	if (Substring_Equals(name, ".IMPSRC"))
		return Substring_InitStr(IMPSRC);
	if (Substring_Equals(name, ".MEMBER"))
		return Substring_InitStr(MEMBER);
	if (Substring_Equals(name, ".OODATE"))
		return Substring_InitStr(OODATE);
	if (Substring_Equals(name, ".PREFIX"))
		return Substring_InitStr(PREFIX);
	if (Substring_Equals(name, ".TARGET"))
		return Substring_InitStr(TARGET);

	if (Substring_Equals(name, ".SHELL") && shellPath == NULL)
		Shell_Init();

	return name;
}

static Var *
GNode_FindVar(GNode *scope, Substring varname, unsigned hash)
{
	return HashTable_FindValueBySubstringHash(&scope->vars, varname, hash);
}

/*
 * Find the variable in the scope, and maybe in other scopes as well.
 *
 * Input:
 *	name		name to find, is not expanded any further
 *	scope		scope in which to look first
 *	elsewhere	true to look in other scopes as well
 *
 * Results:
 *	The found variable, or NULL if the variable does not exist.
 *	If the variable is short-lived (such as environment variables), it
 *	must be freed using VarFreeShortLived after use.
 */
static Var *
VarFindSubstring(Substring name, GNode *scope, bool elsewhere)
{
	Var *var;
	unsigned nameHash;

	/* Replace '.TARGET' with '@', likewise for other local variables. */
	name = CanonicalVarname(name);
	nameHash = Hash_Substring(name);

	var = GNode_FindVar(scope, name, nameHash);
	if (!elsewhere)
		return var;

	if (var == NULL && scope != SCOPE_CMDLINE)
		var = GNode_FindVar(SCOPE_CMDLINE, name, nameHash);

	if (!opts.checkEnvFirst && var == NULL && scope != SCOPE_GLOBAL) {
		var = GNode_FindVar(SCOPE_GLOBAL, name, nameHash);
		if (var == NULL && scope != SCOPE_INTERNAL) {
			/* SCOPE_INTERNAL is subordinate to SCOPE_GLOBAL */
			var = GNode_FindVar(SCOPE_INTERNAL, name, nameHash);
		}
	}

	if (var == NULL) {
		FStr envName = Substring_Str(name);
		const char *envValue = getenv(envName.str);
		if (envValue != NULL)
			return VarNew(envName, envValue, true, true, false);
		FStr_Done(&envName);

		if (opts.checkEnvFirst && scope != SCOPE_GLOBAL) {
			var = GNode_FindVar(SCOPE_GLOBAL, name, nameHash);
			if (var == NULL && scope != SCOPE_INTERNAL)
				var = GNode_FindVar(SCOPE_INTERNAL, name,
				    nameHash);
			return var;
		}

		return NULL;
	}

	return var;
}

static Var *
VarFind(const char *name, GNode *scope, bool elsewhere)
{
	return VarFindSubstring(Substring_InitStr(name), scope, elsewhere);
}

/* If the variable is short-lived, free it, including its value. */
static void
VarFreeShortLived(Var *v)
{
	if (!v->shortLived)
		return;

	FStr_Done(&v->name);
	Buf_Done(&v->val);
	free(v);
}

static const char *
ValueDescription(const char *value)
{
	if (value[0] == '\0')
		return "# (empty)";
	if (ch_isspace(value[strlen(value) - 1]))
		return "# (ends with space)";
	return "";
}

/* Add a new variable of the given name and value to the given scope. */
static Var *
VarAdd(const char *name, const char *value, GNode *scope, VarSetFlags flags)
{
	HashEntry *he = HashTable_CreateEntry(&scope->vars, name, NULL);
	Var *v = VarNew(FStr_InitRefer(/* aliased to */ he->key), value,
	    false, false, (flags & VAR_SET_READONLY) != 0);
	HashEntry_Set(he, v);
	DEBUG4(VAR, "%s: %s = %s%s\n",
	    scope->name, name, value, ValueDescription(value));
	return v;
}

/*
 * Remove a variable from a scope, freeing all related memory as well.
 * The variable name is kept as-is, it is not expanded.
 */
void
Var_Delete(GNode *scope, const char *varname)
{
	HashEntry *he = HashTable_FindEntry(&scope->vars, varname);
	Var *v;

	if (he == NULL) {
		DEBUG2(VAR, "%s: ignoring delete '%s' as it is not found\n",
		    scope->name, varname);
		return;
	}

	v = he->value;
	if (v->readOnlyLoud) {
		Parse_Error(PARSE_FATAL,
		    "Cannot delete \"%s\" as it is read-only",
		    v->name.str);
		return;
	}
	if (v->readOnly) {
		DEBUG2(VAR, "%s: ignoring delete '%s' as it is read-only\n",
		    scope->name, varname);
		return;
	}
	if (v->inUse) {
		Parse_Error(PARSE_FATAL,
		    "Cannot delete variable \"%s\" while it is used",
		    v->name.str);
		return;
	}

	DEBUG2(VAR, "%s: delete %s\n", scope->name, varname);
	if (v->exported)
		unsetenv(v->name.str);
	if (strcmp(v->name.str, ".MAKE.EXPORTED") == 0)
		var_exportedVars = VAR_EXPORTED_NONE;

	assert(v->name.freeIt == NULL);
	HashTable_DeleteEntry(&scope->vars, he);
	Buf_Done(&v->val);
	free(v);
}

#ifdef CLEANUP
void
Var_DeleteAll(GNode *scope)
{
	HashIter hi;
	HashIter_Init(&hi, &scope->vars);
	while (HashIter_Next(&hi)) {
		Var *v = hi.entry->value;
		Buf_Done(&v->val);
		free(v);
	}
}
#endif

/*
 * Undefine one or more variables from the global scope.
 * The argument is expanded exactly once and then split into words.
 */
void
Var_Undef(const char *arg)
{
	char *expanded;
	Words varnames;
	size_t i;

	if (arg[0] == '\0') {
		Parse_Error(PARSE_FATAL,
		    "The .undef directive requires an argument");
		return;
	}

	expanded = Var_Subst(arg, SCOPE_GLOBAL, VARE_EVAL);
	if (expanded == var_Error) {
		/* TODO: Make this part of the code reachable. */
		Parse_Error(PARSE_FATAL,
		    "Error in variable names to be undefined");
		return;
	}

	varnames = Str_Words(expanded, false);
	if (varnames.len == 1 && varnames.words[0][0] == '\0')
		varnames.len = 0;

	for (i = 0; i < varnames.len; i++) {
		const char *varname = varnames.words[i];
		Global_Delete(varname);
	}

	Words_Free(varnames);
	free(expanded);
}

static bool
MayExport(const char *name)
{
	if (name[0] == '.')
		return false;	/* skip internals */
	if (name[0] == '-')
		return false;	/* skip misnamed variables */
	if (name[1] == '\0') {
		/*
		 * A single char.
		 * If it is one of the variables that should only appear in
		 * local scope, skip it, else we can get Var_Subst
		 * into a loop.
		 */
		switch (name[0]) {
		case '@':
		case '%':
		case '*':
		case '!':
			return false;
		}
	}
	return true;
}

static bool
ExportVarEnv(Var *v, GNode *scope)
{
	const char *name = v->name.str;
	char *val = v->val.data;
	char *expr;

	if (v->exported && !v->reexport)
		return false;	/* nothing to do */

	if (strchr(val, '$') == NULL) {
		if (!v->exported)
			setenv(name, val, 1);
		return true;
	}

	if (v->inUse)
		return false;	/* see EMPTY_SHELL in directive-export.mk */

	/* XXX: name is injected without escaping it */
	expr = str_concat3("${", name, "}");
	val = Var_Subst(expr, scope, VARE_EVAL);
	if (scope != SCOPE_GLOBAL) {
		/* we will need to re-export the global version */
		v = VarFind(name, SCOPE_GLOBAL, false);
		if (v != NULL)
			v->exported = false;
	}
	/* TODO: handle errors */
	setenv(name, val, 1);
	free(val);
	free(expr);
	return true;
}

static bool
ExportVarPlain(Var *v)
{
	if (strchr(v->val.data, '$') == NULL) {
		setenv(v->name.str, v->val.data, 1);
		v->exported = true;
		v->reexport = false;
		return true;
	}

	/*
	 * Flag the variable as something we need to re-export.
	 * No point actually exporting it now though,
	 * the child process can do it at the last minute.
	 * Avoid calling setenv more often than necessary since it can leak.
	 */
	v->exported = true;
	v->reexport = true;
	return true;
}

static bool
ExportVarLiteral(Var *v)
{
	if (v->exported && !v->reexport)
		return false;

	if (!v->exported)
		setenv(v->name.str, v->val.data, 1);

	return true;
}

/*
 * Mark a single variable to be exported later for subprocesses.
 *
 * Internal variables are not exported.
 */
static bool
ExportVar(const char *name, GNode *scope, VarExportMode mode)
{
	Var *v;

	if (!MayExport(name))
		return false;

	v = VarFind(name, scope, false);
	if (v == NULL && scope != SCOPE_GLOBAL)
		v = VarFind(name, SCOPE_GLOBAL, false);
	if (v == NULL)
		return false;

	if (mode == VEM_ENV)
		return ExportVarEnv(v, scope);
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
Var_ReexportVars(GNode *scope)
{
	char *xvarnames;

	/*
	 * Several make implementations support this sort of mechanism for
	 * tracking recursion - but each uses a different name.
	 * We allow the makefiles to update MAKELEVEL and ensure
	 * children see a correctly incremented value.
	 */
	char level_buf[21];
	snprintf(level_buf, sizeof level_buf, "%d", makelevel + 1);
	setenv(MAKE_LEVEL_ENV, level_buf, 1);

	if (var_exportedVars == VAR_EXPORTED_NONE)
		return;

	if (var_exportedVars == VAR_EXPORTED_ALL) {
		HashIter hi;

		/* Ouch! Exporting all variables at once is crazy. */
		HashIter_Init(&hi, &SCOPE_GLOBAL->vars);
		while (HashIter_Next(&hi)) {
			Var *var = hi.entry->value;
			ExportVar(var->name.str, scope, VEM_ENV);
		}
		return;
	}

	xvarnames = Var_Subst("${.MAKE.EXPORTED:O:u}", SCOPE_GLOBAL,
	    VARE_EVAL);
	/* TODO: handle errors */
	if (xvarnames[0] != '\0') {
		Words varnames = Str_Words(xvarnames, false);
		size_t i;

		for (i = 0; i < varnames.len; i++)
			ExportVar(varnames.words[i], scope, VEM_ENV);
		Words_Free(varnames);
	}
	free(xvarnames);
}

static void
ExportVars(const char *varnames, bool isExport, VarExportMode mode)
/* TODO: try to combine the parameters 'isExport' and 'mode'. */
{
	Words words = Str_Words(varnames, false);
	size_t i;

	if (words.len == 1 && words.words[0][0] == '\0')
		words.len = 0;

	for (i = 0; i < words.len; i++) {
		const char *varname = words.words[i];
		if (!ExportVar(varname, SCOPE_GLOBAL, mode))
			continue;

		if (var_exportedVars == VAR_EXPORTED_NONE)
			var_exportedVars = VAR_EXPORTED_SOME;

		if (isExport && mode == VEM_PLAIN)
			Global_Append(".MAKE.EXPORTED", varname);
	}
	Words_Free(words);
}

static void
ExportVarsExpand(const char *uvarnames, bool isExport, VarExportMode mode)
{
	char *xvarnames = Var_Subst(uvarnames, SCOPE_GLOBAL, VARE_EVAL);
	/* TODO: handle errors */
	ExportVars(xvarnames, isExport, mode);
	free(xvarnames);
}

/* Export the named variables, or all variables. */
void
Var_Export(VarExportMode mode, const char *varnames)
{
	if (mode == VEM_ALL) {
		var_exportedVars = VAR_EXPORTED_ALL; /* use with caution! */
		return;
	} else if (mode == VEM_PLAIN && varnames[0] == '\0') {
		Parse_Error(PARSE_WARNING, ".export requires an argument.");
		return;
	}

	ExportVarsExpand(varnames, true, mode);
}

void
Var_ExportVars(const char *varnames)
{
	ExportVarsExpand(varnames, false, VEM_PLAIN);
}


static void
ClearEnv(void)
{
	const char *level;
	char **newenv;

	level = getenv(MAKE_LEVEL_ENV);	/* we should preserve this */
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
	if (level != NULL && *level != '\0')
		setenv(MAKE_LEVEL_ENV, level, 1);
}

static void
GetVarnamesToUnexport(bool isEnv, const char *arg,
		      FStr *out_varnames, UnexportWhat *out_what)
{
	UnexportWhat what;
	FStr varnames = FStr_InitRefer("");

	if (isEnv) {
		if (arg[0] != '\0') {
			Parse_Error(PARSE_FATAL,
			    "The directive .unexport-env does not take "
			    "arguments");
			/* continue anyway */
		}
		what = UNEXPORT_ENV;

	} else {
		what = arg[0] != '\0' ? UNEXPORT_NAMED : UNEXPORT_ALL;
		if (what == UNEXPORT_NAMED)
			varnames = FStr_InitRefer(arg);
	}

	if (what != UNEXPORT_NAMED) {
		char *expanded = Var_Subst("${.MAKE.EXPORTED:O:u}",
		    SCOPE_GLOBAL, VARE_EVAL);
		/* TODO: handle errors */
		varnames = FStr_InitOwn(expanded);
	}

	*out_varnames = varnames;
	*out_what = what;
}

static void
UnexportVar(Substring varname, UnexportWhat what)
{
	Var *v = VarFindSubstring(varname, SCOPE_GLOBAL, false);
	if (v == NULL) {
		DEBUG2(VAR, "Not unexporting \"%.*s\" (not found)\n",
		    (int)Substring_Length(varname), varname.start);
		return;
	}

	DEBUG2(VAR, "Unexporting \"%.*s\"\n",
	    (int)Substring_Length(varname), varname.start);
	if (what != UNEXPORT_ENV && v->exported && !v->reexport)
		unsetenv(v->name.str);
	v->exported = false;
	v->reexport = false;

	if (what == UNEXPORT_NAMED) {
		/* Remove the variable names from .MAKE.EXPORTED. */
		/* XXX: v->name is injected without escaping it */
		char *expr = str_concat3(
		    "${.MAKE.EXPORTED:N", v->name.str, "}");
		char *filtered = Var_Subst(expr, SCOPE_GLOBAL, VARE_EVAL);
		/* TODO: handle errors */
		Global_Set(".MAKE.EXPORTED", filtered);
		free(filtered);
		free(expr);
	}
}

static void
UnexportVars(const char *varnames, UnexportWhat what)
{
	size_t i;
	SubstringWords words;

	if (what == UNEXPORT_ENV)
		ClearEnv();

	words = Substring_Words(varnames, false);
	for (i = 0; i < words.len; i++)
		UnexportVar(words.words[i], what);
	SubstringWords_Free(words);

	if (what != UNEXPORT_NAMED)
		Global_Delete(".MAKE.EXPORTED");
}

/* Handle the .unexport and .unexport-env directives. */
void
Var_UnExport(bool isEnv, const char *arg)
{
	UnexportWhat what;
	FStr varnames;

	GetVarnamesToUnexport(isEnv, arg, &varnames, &what);
	UnexportVars(varnames.str, what);
	FStr_Done(&varnames);
}

/* Set the variable to the value; the name is not expanded. */
void
Var_SetWithFlags(GNode *scope, const char *name, const char *val,
		 VarSetFlags flags)
{
	Var *v;

	assert(val != NULL);
	if (name[0] == '\0') {
		DEBUG3(VAR,
		    "%s: ignoring '%s = %s' as the variable name is empty\n",
		    scope->name, name, val);
		return;
	}

	if (scope == SCOPE_GLOBAL
	    && VarFind(name, SCOPE_CMDLINE, false) != NULL) {
		/*
		 * The global variable would not be visible anywhere.
		 * Therefore, there is no point in setting it at all.
		 */
		DEBUG3(VAR,
		    "%s: ignoring '%s = %s' "
		    "due to a command line variable of the same name\n",
		    scope->name, name, val);
		return;
	}

	/*
	 * Only look for a variable in the given scope since anything set
	 * here will override anything in a lower scope, so there's not much
	 * point in searching them all.
	 */
	v = VarFind(name, scope, false);
	if (v == NULL) {
		if (scope == SCOPE_CMDLINE && !(flags & VAR_SET_NO_EXPORT)) {
			/*
			 * This variable would normally prevent the same name
			 * being added to SCOPE_GLOBAL, so delete it from
			 * there if needed. Otherwise -V name may show the
			 * wrong value.
			 *
			 * See ExistsInCmdline.
			 */
			Var *gl = VarFind(name, SCOPE_GLOBAL, false);
			if (gl != NULL && strcmp(gl->val.data, val) == 0) {
				DEBUG3(VAR,
				    "%s: ignoring to override the global "
				    "'%s = %s' from a command line variable "
				    "as the value wouldn't change\n",
				    scope->name, name, val);
			} else if (gl != NULL && gl->readOnlyLoud)
				Parse_Error(PARSE_FATAL,
				    "Cannot override "
				    "read-only global variable \"%s\" "
				    "with a command line variable", name);
			else
				Var_Delete(SCOPE_GLOBAL, name);
		}
		if (strcmp(name, ".SUFFIXES") == 0) {
			/* special: treat as read-only */
			DEBUG3(VAR,
			    "%s: ignoring '%s = %s' as it is read-only\n",
			    scope->name, name, val);
			return;
		}
		v = VarAdd(name, val, scope, flags);
	} else {
		if (v->readOnlyLoud) {
			Parse_Error(PARSE_FATAL,
			    "Cannot overwrite \"%s\" as it is read-only",
			    name);
			return;
		}
		if (v->readOnly && !(flags & VAR_SET_READONLY)) {
			DEBUG3(VAR,
			    "%s: ignoring '%s = %s' as it is read-only\n",
			    scope->name, name, val);
			return;
		}
		Buf_Clear(&v->val);
		Buf_AddStr(&v->val, val);

		DEBUG4(VAR, "%s: %s = %s%s\n",
		    scope->name, name, val, ValueDescription(val));
		if (v->exported)
			ExportVar(name, scope, VEM_PLAIN);
	}

	if (scope == SCOPE_CMDLINE) {
		v->fromCmd = true;

		/*
		 * Any variables given on the command line are automatically
		 * exported to the environment (as per POSIX standard), except
		 * for internals.
		 */
		if (!(flags & VAR_SET_NO_EXPORT)) {

			/*
			 * If requested, don't export these in the
			 * environment individually.  We still put
			 * them in .MAKEOVERRIDES so that the
			 * command-line settings continue to override
			 * Makefile settings.
			 */
			if (!opts.varNoExportEnv && name[0] != '.')
				setenv(name, val, 1);

			if (!(flags & VAR_SET_INTERNAL))
				Global_Append(".MAKEOVERRIDES", name);
		}
	}

	if (name[0] == '.' && strcmp(name, MAKE_SAVE_DOLLARS) == 0)
		save_dollars = ParseBoolean(val, save_dollars);

	if (v != NULL)
		VarFreeShortLived(v);
}

void
Var_Set(GNode *scope, const char *name, const char *val)
{
	Var_SetWithFlags(scope, name, val, VAR_SET_NONE);
}

/*
 * In the scope, expand the variable name once, then create the variable or
 * replace its value.
 */
void
Var_SetExpand(GNode *scope, const char *name, const char *val)
{
	FStr varname = FStr_InitRefer(name);

	assert(val != NULL);

	Var_Expand(&varname, scope, VARE_EVAL);

	if (varname.str[0] == '\0') {
		DEBUG4(VAR,
		    "%s: ignoring '%s = %s' "
		    "as the variable name '%s' expands to empty\n",
		    scope->name, varname.str, val, name);
	} else
		Var_SetWithFlags(scope, varname.str, val, VAR_SET_NONE);

	FStr_Done(&varname);
}

void
Global_Set(const char *name, const char *value)
{
	Var_Set(SCOPE_GLOBAL, name, value);
}

void
Global_Delete(const char *name)
{
	Var_Delete(SCOPE_GLOBAL, name);
}

void
Global_Set_ReadOnly(const char *name, const char *value)
{
	Var_SetWithFlags(SCOPE_GLOBAL, name, value, VAR_SET_NONE);
	VarFind(name, SCOPE_GLOBAL, false)->readOnlyLoud = true;
}

/*
 * Append the value to the named variable.
 *
 * If the variable doesn't exist, it is created.  Otherwise a single space
 * and the given value are appended.
 */
void
Var_Append(GNode *scope, const char *name, const char *val)
{
	Var *v;

	v = VarFind(name, scope, scope == SCOPE_GLOBAL);

	if (v == NULL) {
		Var_SetWithFlags(scope, name, val, VAR_SET_NONE);
	} else if (v->readOnlyLoud) {
		Parse_Error(PARSE_FATAL,
		    "Cannot append to \"%s\" as it is read-only", name);
		return;
	} else if (v->readOnly) {
		DEBUG3(VAR, "%s: ignoring '%s += %s' as it is read-only\n",
		    scope->name, name, val);
	} else if (scope == SCOPE_CMDLINE || !v->fromCmd) {
		Buf_AddByte(&v->val, ' ');
		Buf_AddStr(&v->val, val);

		DEBUG3(VAR, "%s: %s = %s\n", scope->name, name, v->val.data);

		if (v->fromEnvironment) {
			/* See VarAdd. */
			HashEntry *he =
			    HashTable_CreateEntry(&scope->vars, name, NULL);
			HashEntry_Set(he, v);
			FStr_Done(&v->name);
			v->name = FStr_InitRefer(/* aliased to */ he->key);
			v->shortLived = false;
			v->fromEnvironment = false;
		}
	}
}

/*
 * In the scope, expand the variable name once.  If the variable exists in the
 * scope, add a space and the value, otherwise set the variable to the value.
 *
 * Appending to an environment variable only works in the global scope, that
 * is, for variable assignments in makefiles, but not inside conditions or the
 * commands of a target.
 */
void
Var_AppendExpand(GNode *scope, const char *name, const char *val)
{
	FStr xname = FStr_InitRefer(name);

	assert(val != NULL);

	Var_Expand(&xname, scope, VARE_EVAL);
	if (xname.str != name && xname.str[0] == '\0')
		DEBUG4(VAR,
		    "%s: ignoring '%s += %s' "
		    "as the variable name '%s' expands to empty\n",
		    scope->name, xname.str, val, name);
	else
		Var_Append(scope, xname.str, val);

	FStr_Done(&xname);
}

void
Global_Append(const char *name, const char *value)
{
	Var_Append(SCOPE_GLOBAL, name, value);
}

bool
Var_Exists(GNode *scope, const char *name)
{
	Var *v = VarFind(name, scope, true);
	if (v == NULL)
		return false;

	VarFreeShortLived(v);
	return true;
}

/*
 * See if the given variable exists, in the given scope or in other
 * fallback scopes.
 *
 * Input:
 *	scope		scope in which to start search
 *	name		name of the variable to find, is expanded once
 */
bool
Var_ExistsExpand(GNode *scope, const char *name)
{
	FStr varname = FStr_InitRefer(name);
	bool exists;

	Var_Expand(&varname, scope, VARE_EVAL);
	exists = Var_Exists(scope, varname.str);
	FStr_Done(&varname);
	return exists;
}

/*
 * Return the unexpanded value of the given variable in the given scope,
 * falling back to the command, global and environment scopes, in this order,
 * but see the -e option.
 *
 * Input:
 *	name		the name to find, is not expanded any further
 *
 * Results:
 *	The value if the variable exists, NULL if it doesn't.
 *	The value is valid until the next modification to any variable.
 */
FStr
Var_Value(GNode *scope, const char *name)
{
	Var *v = VarFind(name, scope, true);
	char *value;

	if (v == NULL)
		return FStr_InitRefer(NULL);

	if (!v->shortLived)
		return FStr_InitRefer(v->val.data);

	value = v->val.data;
	v->val.data = NULL;
	VarFreeShortLived(v);

	return FStr_InitOwn(value);
}

/* Set or clear the read-only attribute of the variable if it exists. */
void
Var_ReadOnly(const char *name, bool bf)
{
	Var *v;

	v = VarFind(name, SCOPE_GLOBAL, false);
	if (v == NULL) {
		DEBUG1(VAR, "Var_ReadOnly: %s not found\n", name);
		return;
	}
	v->readOnly = bf;
	DEBUG2(VAR, "Var_ReadOnly: %s %s\n", name, bf ? "true" : "false");
}

/*
 * Return the unexpanded variable value from this node, without trying to look
 * up the variable in any other scope.
 */
const char *
GNode_ValueDirect(GNode *gn, const char *name)
{
	Var *v = VarFind(name, gn, false);
	return v != NULL ? v->val.data : NULL;
}

static VarEvalMode
VarEvalMode_WithoutKeepDollar(VarEvalMode emode)
{
	return emode == VARE_EVAL_KEEP_DOLLAR_AND_UNDEFINED
	    ? VARE_EVAL_KEEP_UNDEFINED : emode;
}

static bool
VarEvalMode_ShouldEval(VarEvalMode emode)
{
	return emode != VARE_PARSE;
}

static bool
VarEvalMode_ShouldKeepUndef(VarEvalMode emode)
{
	return emode == VARE_EVAL_KEEP_UNDEFINED ||
	       emode == VARE_EVAL_KEEP_DOLLAR_AND_UNDEFINED;
}

static bool
VarEvalMode_ShouldKeepDollar(VarEvalMode emode)
{
	return emode == VARE_EVAL_KEEP_DOLLAR_AND_UNDEFINED;
}


static void
SepBuf_Init(SepBuf *buf, char sep)
{
	Buf_InitSize(&buf->buf, 32);
	buf->needSep = false;
	buf->sep = sep;
}

static void
SepBuf_Sep(SepBuf *buf)
{
	buf->needSep = true;
}

static void
SepBuf_AddBytes(SepBuf *buf, const char *mem, size_t mem_size)
{
	if (mem_size == 0)
		return;
	if (buf->needSep && buf->sep != '\0') {
		Buf_AddByte(&buf->buf, buf->sep);
		buf->needSep = false;
	}
	Buf_AddBytes(&buf->buf, mem, mem_size);
}

static void
SepBuf_AddRange(SepBuf *buf, const char *start, const char *end)
{
	SepBuf_AddBytes(buf, start, (size_t)(end - start));
}

static void
SepBuf_AddStr(SepBuf *buf, const char *str)
{
	SepBuf_AddBytes(buf, str, strlen(str));
}

static void
SepBuf_AddSubstring(SepBuf *buf, Substring sub)
{
	SepBuf_AddRange(buf, sub.start, sub.end);
}

static char *
SepBuf_DoneData(SepBuf *buf)
{
	return Buf_DoneData(&buf->buf);
}


/*
 * This callback for ModifyWords gets a single word from an expression
 * and typically adds a modification of this word to the buffer. It may also
 * do nothing or add several words.
 *
 * For example, when evaluating the modifier ':M*b' in ${:Ua b c:M*b}, the
 * callback is called 3 times, once for "a", "b" and "c".
 *
 * Some ModifyWord functions assume that they are always passed a
 * null-terminated substring, which is currently guaranteed but may change in
 * the future.
 */
typedef void (*ModifyWordProc)(Substring word, SepBuf *buf, void *data);


static void
ModifyWord_Head(Substring word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
	SepBuf_AddSubstring(buf, Substring_Dirname(word));
}

static void
ModifyWord_Tail(Substring word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
	SepBuf_AddSubstring(buf, Substring_Basename(word));
}

static void
ModifyWord_Suffix(Substring word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
	const char *lastDot = Substring_FindLast(word, '.');
	if (lastDot != NULL)
		SepBuf_AddRange(buf, lastDot + 1, word.end);
}

static void
ModifyWord_Root(Substring word, SepBuf *buf, void *dummy MAKE_ATTR_UNUSED)
{
	const char *lastDot, *end;

	lastDot = Substring_FindLast(word, '.');
	end = lastDot != NULL ? lastDot : word.end;
	SepBuf_AddRange(buf, word.start, end);
}

struct ModifyWord_SysVSubstArgs {
	GNode *scope;
	Substring lhsPrefix;
	bool lhsPercent;
	Substring lhsSuffix;
	const char *rhs;
};

static void
ModifyWord_SysVSubst(Substring word, SepBuf *buf, void *data)
{
	const struct ModifyWord_SysVSubstArgs *args = data;
	FStr rhs;
	const char *percent;

	if (Substring_IsEmpty(word))
		return;

	if (!Substring_HasPrefix(word, args->lhsPrefix) ||
	    !Substring_HasSuffix(word, args->lhsSuffix)) {
		SepBuf_AddSubstring(buf, word);
		return;
	}

	rhs = FStr_InitRefer(args->rhs);
	Var_Expand(&rhs, args->scope, VARE_EVAL);

	percent = args->lhsPercent ? strchr(rhs.str, '%') : NULL;

	if (percent != NULL)
		SepBuf_AddRange(buf, rhs.str, percent);
	if (percent != NULL || !args->lhsPercent)
		SepBuf_AddRange(buf,
		    word.start + Substring_Length(args->lhsPrefix),
		    word.end - Substring_Length(args->lhsSuffix));
	SepBuf_AddStr(buf, percent != NULL ? percent + 1 : rhs.str);

	FStr_Done(&rhs);
}

static const char *
Substring_Find(Substring haystack, Substring needle)
{
	size_t len, needleLen, i;

	len = Substring_Length(haystack);
	needleLen = Substring_Length(needle);
	for (i = 0; i + needleLen <= len; i++)
		if (memcmp(haystack.start + i, needle.start, needleLen) == 0)
			return haystack.start + i;
	return NULL;
}

struct ModifyWord_SubstArgs {
	Substring lhs;
	Substring rhs;
	PatternFlags pflags;
	bool matched;
};

static void
ModifyWord_Subst(Substring word, SepBuf *buf, void *data)
{
	struct ModifyWord_SubstArgs *args = data;
	size_t wordLen, lhsLen;
	const char *match;

	wordLen = Substring_Length(word);
	if (args->pflags.subOnce && args->matched)
		goto nosub;

	lhsLen = Substring_Length(args->lhs);
	if (args->pflags.anchorStart) {
		if (wordLen < lhsLen ||
		    memcmp(word.start, args->lhs.start, lhsLen) != 0)
			goto nosub;

		if (args->pflags.anchorEnd && wordLen != lhsLen)
			goto nosub;

		/* :S,^prefix,replacement, or :S,^whole$,replacement, */
		SepBuf_AddSubstring(buf, args->rhs);
		SepBuf_AddRange(buf, word.start + lhsLen, word.end);
		args->matched = true;
		return;
	}

	if (args->pflags.anchorEnd) {
		if (wordLen < lhsLen)
			goto nosub;
		if (memcmp(word.end - lhsLen, args->lhs.start, lhsLen) != 0)
			goto nosub;

		/* :S,suffix$,replacement, */
		SepBuf_AddRange(buf, word.start, word.end - lhsLen);
		SepBuf_AddSubstring(buf, args->rhs);
		args->matched = true;
		return;
	}

	if (Substring_IsEmpty(args->lhs))
		goto nosub;

	/* unanchored case, may match more than once */
	while ((match = Substring_Find(word, args->lhs)) != NULL) {
		SepBuf_AddRange(buf, word.start, match);
		SepBuf_AddSubstring(buf, args->rhs);
		args->matched = true;
		word.start = match + lhsLen;
		if (Substring_IsEmpty(word) || !args->pflags.subGlobal)
			break;
	}
nosub:
	SepBuf_AddSubstring(buf, word);
}

#ifdef HAVE_REGEX_H
/* Print the error caused by a regcomp or regexec call. */
static void
RegexError(int reerr, const regex_t *pat, const char *str)
{
	size_t errlen = regerror(reerr, pat, NULL, 0);
	char *errbuf = bmake_malloc(errlen);
	regerror(reerr, pat, errbuf, errlen);
	Parse_Error(PARSE_FATAL, "%s: %s", str, errbuf);
	free(errbuf);
}

/* In the modifier ':C', replace a backreference from \0 to \9. */
static void
RegexReplaceBackref(char ref, SepBuf *buf, const char *wp,
		    const regmatch_t *m, size_t nsub)
{
	unsigned n = (unsigned)ref - '0';

	if (n >= nsub)
		Parse_Error(PARSE_FATAL, "No subexpression \\%u", n);
	else if (m[n].rm_so == -1) {
		if (opts.strict)
			Error("No match for subexpression \\%u", n);
	} else {
		SepBuf_AddRange(buf,
		    wp + (size_t)m[n].rm_so,
		    wp + (size_t)m[n].rm_eo);
	}
}

/*
 * The regular expression matches the word; now add the replacement to the
 * buffer, taking back-references from 'wp'.
 */
static void
RegexReplace(Substring replace, SepBuf *buf, const char *wp,
	     const regmatch_t *m, size_t nsub)
{
	const char *rp;

	for (rp = replace.start; rp != replace.end; rp++) {
		if (*rp == '\\' && rp + 1 != replace.end &&
		    (rp[1] == '&' || rp[1] == '\\'))
			SepBuf_AddBytes(buf, ++rp, 1);
		else if (*rp == '\\' && rp + 1 != replace.end &&
			 ch_isdigit(rp[1]))
			RegexReplaceBackref(*++rp, buf, wp, m, nsub);
		else if (*rp == '&') {
			SepBuf_AddRange(buf,
			    wp + (size_t)m[0].rm_so,
			    wp + (size_t)m[0].rm_eo);
		} else
			SepBuf_AddBytes(buf, rp, 1);
	}
}

struct ModifyWord_SubstRegexArgs {
	regex_t re;
	size_t nsub;
	Substring replace;
	PatternFlags pflags;
	bool matched;
};

static void
ModifyWord_SubstRegex(Substring word, SepBuf *buf, void *data)
{
	struct ModifyWord_SubstRegexArgs *args = data;
	int xrv;
	const char *wp;
	int flags = 0;
	regmatch_t m[10];

	assert(word.end[0] == '\0');	/* assume null-terminated word */
	wp = word.start;
	if (args->pflags.subOnce && args->matched)
		goto no_match;

again:
	xrv = regexec(&args->re, wp, args->nsub, m, flags);
	if (xrv == 0)
		goto ok;
	if (xrv != REG_NOMATCH)
		RegexError(xrv, &args->re, "Unexpected regex error");
no_match:
	SepBuf_AddRange(buf, wp, word.end);
	return;

ok:
	args->matched = true;
	SepBuf_AddBytes(buf, wp, (size_t)m[0].rm_so);

	RegexReplace(args->replace, buf, wp, m, args->nsub);

	wp += (size_t)m[0].rm_eo;
	if (args->pflags.subGlobal) {
		flags |= REG_NOTBOL;
		if (m[0].rm_so == 0 && m[0].rm_eo == 0 && *wp != '\0') {
			SepBuf_AddBytes(buf, wp, 1);
			wp++;
		}
		if (*wp != '\0')
			goto again;
	}
	if (*wp != '\0')
		SepBuf_AddStr(buf, wp);
}
#endif

struct ModifyWord_LoopArgs {
	GNode *scope;
	const char *var;	/* name of the temporary variable */
	const char *body;	/* string to expand */
	VarEvalMode emode;
};

static void
ModifyWord_Loop(Substring word, SepBuf *buf, void *data)
{
	const struct ModifyWord_LoopArgs *args;
	char *s;

	if (Substring_IsEmpty(word))
		return;

	args = data;
	assert(word.end[0] == '\0');	/* assume null-terminated word */
	Var_SetWithFlags(args->scope, args->var, word.start,
	    VAR_SET_NO_EXPORT);
	s = Var_Subst(args->body, args->scope, args->emode);
	/* TODO: handle errors */

	DEBUG2(VAR, "ModifyWord_Loop: expand \"%s\" to \"%s\"\n",
	    args->body, s);

	if (s[0] == '\n' || Buf_EndsWith(&buf->buf, '\n'))
		buf->needSep = false;
	SepBuf_AddStr(buf, s);
	free(s);
}


/*
 * The :[first..last] modifier selects words from the expression.
 * It can also reverse the words.
 */
static char *
VarSelectWords(const char *str, int first, int last,
	       char sep, bool oneBigWord)
{
	SubstringWords words;
	int len, start, end, step;
	int i;

	SepBuf buf;
	SepBuf_Init(&buf, sep);

	if (oneBigWord) {
		/* fake what Substring_Words() would do */
		words.len = 1;
		words.words = bmake_malloc(sizeof(words.words[0]));
		words.freeIt = NULL;
		words.words[0] = Substring_InitStr(str); /* no need to copy */
	} else {
		words = Substring_Words(str, false);
	}

	/* Convert -1 to len, -2 to (len - 1), etc. */
	len = (int)words.len;
	if (first < 0)
		first += len + 1;
	if (last < 0)
		last += len + 1;

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
		SepBuf_AddSubstring(&buf, words.words[i]);
		SepBuf_Sep(&buf);
	}

	SubstringWords_Free(words);

	return SepBuf_DoneData(&buf);
}


static void
ModifyWord_Realpath(Substring word, SepBuf *buf, void *data MAKE_ATTR_UNUSED)
{
	struct stat st;
	char rbuf[MAXPATHLEN];
	const char *rp;

	assert(word.end[0] == '\0');	/* assume null-terminated word */
	rp = cached_realpath(word.start, rbuf);
	if (rp != NULL && *rp == '/' && stat(rp, &st) == 0)
		SepBuf_AddStr(buf, rp);
	else
		SepBuf_AddSubstring(buf, word);
}


static char *
SubstringWords_JoinFree(SubstringWords words)
{
	Buffer buf;
	size_t i;

	Buf_Init(&buf);

	for (i = 0; i < words.len; i++) {
		if (i != 0) {
			/*
			 * XXX: Use ch->sep instead of ' ', for consistency.
			 */
			Buf_AddByte(&buf, ' ');
		}
		Buf_AddRange(&buf, words.words[i].start, words.words[i].end);
	}

	SubstringWords_Free(words);

	return Buf_DoneData(&buf);
}


/*
 * Quote shell meta-characters and space characters in the string.
 * If quoteDollar is set, also quote and double any '$' characters.
 */
static void
QuoteShell(const char *str, bool quoteDollar, LazyBuf *buf)
{
	const char *p;

	LazyBuf_Init(buf, str);
	for (p = str; *p != '\0'; p++) {
		if (*p == '\n') {
			const char *newline = Shell_GetNewline();
			if (newline == NULL)
				newline = "\\\n";
			LazyBuf_AddStr(buf, newline);
			continue;
		}
		if (ch_isspace(*p) || ch_is_shell_meta(*p))
			LazyBuf_Add(buf, '\\');
		LazyBuf_Add(buf, *p);
		if (quoteDollar && *p == '$')
			LazyBuf_AddStr(buf, "\\$");
	}
}

/*
 * Compute the 32-bit hash of the given string, using the MurmurHash3
 * algorithm. Output is encoded as 8 hex digits, in Little Endian order.
 */
static char *
Hash(const char *str)
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
FormatTime(const char *fmt, time_t t, bool gmt)
{
	char buf[BUFSIZ];

	if (t == 0)
		time(&t);
	if (*fmt == '\0')
		fmt = "%c";
	if (gmt && strchr(fmt, 's') != NULL) {
		/* strftime "%s" only works with localtime, not with gmtime. */
		const char *prev_tz_env = getenv("TZ");
		char *prev_tz = prev_tz_env != NULL
		    ? bmake_strdup(prev_tz_env) : NULL;
		setenv("TZ", "UTC", 1);
		strftime(buf, sizeof buf, fmt, localtime(&t));
		if (prev_tz != NULL) {
			setenv("TZ", prev_tz, 1);
			free(prev_tz);
		} else
			unsetenv("TZ");
	} else
		strftime(buf, sizeof buf, fmt, (gmt ? gmtime : localtime)(&t));

	buf[sizeof buf - 1] = '\0';
	return bmake_strdup(buf);
}

/*
 * The ApplyModifier functions take an expression that is being evaluated.
 * Their task is to apply a single modifier to the expression.  This involves
 * parsing the modifier, evaluating it and finally updating the value of the
 * expression.
 *
 * Parsing the modifier
 *
 * If parsing succeeds, the parsing position *pp is updated to point to the
 * first character following the modifier, which typically is either ':' or
 * ch->endc.  The modifier doesn't have to check for this delimiter character,
 * this is done by ApplyModifiers.
 *
 * XXX: As of 2020-11-15, some modifiers such as :S, :C, :P, :L do not
 * need to be followed by a ':' or endc; this was an unintended mistake.
 *
 * If parsing fails because of a missing delimiter after a modifier part (as
 * in the :S, :C or :@ modifiers), return AMR_CLEANUP.
 *
 * If parsing fails because the modifier is unknown, return AMR_UNKNOWN to
 * try the SysV modifier ':from=to' as fallback.  This should only be
 * done as long as there have been no side effects from evaluating nested
 * variables, to avoid evaluating them more than once.  In this case, the
 * parsing position may or may not be updated.  (XXX: Why not? The original
 * parsing position is well-known in ApplyModifiers.)
 *
 * If parsing fails and the SysV modifier ${VAR:from=to} should not be used
 * as a fallback, issue an error message using Parse_Error (preferred over
 * Error) and then return AMR_CLEANUP, which stops processing the expression.
 * (XXX: As of 2020-08-23, evaluation of the string continues nevertheless
 * after skipping a few bytes, which results in garbage.)
 *
 * Evaluating the modifier
 *
 * After parsing, the modifier is evaluated.  The side effects from evaluating
 * nested expressions in the modifier text often already happen
 * during parsing though.  For most modifiers this doesn't matter since their
 * only noticeable effect is that they update the value of the expression.
 * Some modifiers such as ':sh' or '::=' have noticeable side effects though.
 *
 * Evaluating the modifier usually takes the current value of the
 * expression from ch->expr->value, or the variable name from ch->var->name,
 * and stores the result back in ch->expr->value via Expr_SetValueOwn or
 * Expr_SetValueRefer.
 *
 * Some modifiers such as :D and :U turn undefined expressions into defined
 * expressions using Expr_Define.
 */

typedef enum ExprDefined {
	/* The expression is based on a regular, defined variable. */
	DEF_REGULAR,
	/* The expression is based on an undefined variable. */
	DEF_UNDEF,
	/*
	 * The expression started as an undefined expression, but one
	 * of the modifiers (such as ':D' or ':U') has turned the expression
	 * from undefined to defined.
	 */
	DEF_DEFINED
} ExprDefined;

static const char ExprDefined_Name[][10] = {
	"regular",
	"undefined",
	"defined"
};

#if __STDC_VERSION__ >= 199901L
#define const_member		const
#else
#define const_member		/* no const possible */
#endif

/* An expression based on a variable, such as $@ or ${VAR:Mpattern:Q}. */
typedef struct Expr {
	const char *name;
	FStr value;
	VarEvalMode const_member emode;
	GNode *const_member scope;
	ExprDefined defined;
} Expr;

/*
 * The status of applying a chain of modifiers to an expression.
 *
 * The modifiers of an expression are broken into chains of modifiers,
 * starting a new nested chain whenever an indirect modifier starts.  There
 * are at most 2 nesting levels: the outer one for the direct modifiers, and
 * the inner one for the indirect modifiers.
 *
 * For example, the expression ${VAR:M*:${IND1}:${IND2}:O:u} has 3 chains of
 * modifiers:
 *
 *	Chain 1 starts with the single modifier ':M*'.
 *	  Chain 2 starts with all modifiers from ${IND1}.
 *	  Chain 2 ends at the ':' between ${IND1} and ${IND2}.
 *	  Chain 3 starts with all modifiers from ${IND2}.
 *	  Chain 3 ends at the ':' after ${IND2}.
 *	Chain 1 continues with the 2 modifiers ':O' and ':u'.
 *	Chain 1 ends at the final '}' of the expression.
 *
 * After such a chain ends, its properties no longer have any effect.
 *
 * See varmod-indirect.mk.
 */
typedef struct ModChain {
	Expr *expr;
	/* '\0' or '{' or '(' */
	char const_member startc;
	/* '\0' or '}' or ')' */
	char const_member endc;
	/* Separator when joining words (see the :ts modifier). */
	char sep;
	/*
	 * Whether some modifiers that otherwise split the variable value
	 * into words, like :S and :C, treat the variable value as a single
	 * big word, possibly containing spaces.
	 */
	bool oneBigWord;
} ModChain;

static void
Expr_Define(Expr *expr)
{
	if (expr->defined == DEF_UNDEF)
		expr->defined = DEF_DEFINED;
}

static const char *
Expr_Str(const Expr *expr)
{
	return expr->value.str;
}

static SubstringWords
Expr_Words(const Expr *expr)
{
	return Substring_Words(Expr_Str(expr), false);
}

static void
Expr_SetValue(Expr *expr, FStr value)
{
	FStr_Done(&expr->value);
	expr->value = value;
}

static void
Expr_SetValueOwn(Expr *expr, char *value)
{
	Expr_SetValue(expr, FStr_InitOwn(value));
}

static void
Expr_SetValueRefer(Expr *expr, const char *value)
{
	Expr_SetValue(expr, FStr_InitRefer(value));
}

static bool
Expr_ShouldEval(const Expr *expr)
{
	return VarEvalMode_ShouldEval(expr->emode);
}

static bool
ModChain_ShouldEval(const ModChain *ch)
{
	return Expr_ShouldEval(ch->expr);
}


typedef enum ApplyModifierResult {
	/* Continue parsing */
	AMR_OK,
	/* Not a match, try the ':from=to' modifier as well. */
	AMR_UNKNOWN,
	/* Error out without further error message. */
	AMR_CLEANUP
} ApplyModifierResult;

/*
 * Allow backslashes to escape the delimiters, $, and \, but don't touch other
 * backslashes.
 */
static bool
IsEscapedModifierPart(const char *p, char end1, char end2,
		      struct ModifyWord_SubstArgs *subst)
{
	if (p[0] != '\\' || p[1] == '\0')
		return false;
	if (p[1] == end1 || p[1] == end2 || p[1] == '\\' || p[1] == '$')
		return true;
	return p[1] == '&' && subst != NULL;
}

/*
 * In a part of a modifier, parse a subexpression and evaluate it.
 */
static void
ParseModifierPartExpr(const char **pp, LazyBuf *part, const ModChain *ch,
		      VarEvalMode emode)
{
	const char *p = *pp;
	FStr nested_val = Var_Parse(&p, ch->expr->scope,
	    VarEvalMode_WithoutKeepDollar(emode));
	/* TODO: handle errors */
	if (VarEvalMode_ShouldEval(emode))
		LazyBuf_AddStr(part, nested_val.str);
	else
		LazyBuf_AddSubstring(part, Substring_Init(*pp, p));
	FStr_Done(&nested_val);
	*pp = p;
}

/*
 * In a part of a modifier, parse some text that looks like a subexpression.
 * If the text starts with '$(', any '(' and ')' must be balanced.
 * If the text starts with '${', any '{' and '}' must be balanced.
 * If the text starts with '$', that '$' is copied verbatim, it is not parsed
 * as a short-name expression.
 */
static void
ParseModifierPartBalanced(const char **pp, LazyBuf *part)
{
	const char *p = *pp;

	if (p[1] == '(' || p[1] == '{') {
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
		LazyBuf_AddSubstring(part, Substring_Init(*pp, p));
		*pp = p;
	} else {
		LazyBuf_Add(part, *p);
		*pp = p + 1;
	}
}

/*
 * Parse a part of a modifier such as the "from" and "to" in :S/from/to/ or
 * the "var" or "replacement ${var}" in :@var@replacement ${var}@, up to and
 * including the next unescaped delimiter.  The delimiter, as well as the
 * backslash or the dollar, can be escaped with a backslash.
 *
 * Return true if parsing succeeded, together with the parsed (and possibly
 * expanded) part.  In that case, pp points right after the delimiter.  The
 * delimiter is not included in the part though.
 */
static bool
ParseModifierPart(
    /* The parsing position, updated upon return */
    const char **pp,
    char end1,
    char end2,
    /* Mode for evaluating nested expressions. */
    VarEvalMode emode,
    ModChain *ch,
    LazyBuf *part,
    /*
     * For the first part of the ':S' modifier, set anchorEnd if the last
     * character of the pattern is a $.
     */
    PatternFlags *out_pflags,
    /*
     * For the second part of the ':S' modifier, allow ampersands to be
     * escaped and replace unescaped ampersands with subst->lhs.
     */
    struct ModifyWord_SubstArgs *subst
)
{
	const char *p = *pp;

	LazyBuf_Init(part, p);
	while (*p != '\0' && *p != end1 && *p != end2) {
		if (IsEscapedModifierPart(p, end1, end2, subst)) {
			LazyBuf_Add(part, p[1]);
			p += 2;
		} else if (*p != '$') {	/* Unescaped, simple text */
			if (subst != NULL && *p == '&')
				LazyBuf_AddSubstring(part, subst->lhs);
			else
				LazyBuf_Add(part, *p);
			p++;
		} else if (p[1] == end2) {	/* Unescaped '$' at end */
			if (out_pflags != NULL)
				out_pflags->anchorEnd = true;
			else
				LazyBuf_Add(part, *p);
			p++;
		} else if (emode == VARE_PARSE_BALANCED)
			ParseModifierPartBalanced(&p, part);
		else
			ParseModifierPartExpr(&p, part, ch, emode);
	}

	if (*p != end1 && *p != end2) {
		Parse_Error(PARSE_FATAL,
		    "Unfinished modifier after \"%.*s\", expecting \"%c\"",
		    (int)(p - *pp), *pp, end2);
		LazyBuf_Done(part);
		*pp = p;
		return false;
	}
	*pp = p;
	if (end1 == end2)
		(*pp)++;

	{
		Substring sub = LazyBuf_Get(part);
		DEBUG2(VAR, "Modifier part: \"%.*s\"\n",
		    (int)Substring_Length(sub), sub.start);
	}

	return true;
}

MAKE_INLINE bool
IsDelimiter(char c, const ModChain *ch)
{
	return c == ':' || c == ch->endc || c == '\0';
}

/* Test whether mod starts with modname, followed by a delimiter. */
MAKE_INLINE bool
ModMatch(const char *mod, const char *modname, const ModChain *ch)
{
	size_t n = strlen(modname);
	return strncmp(mod, modname, n) == 0 && IsDelimiter(mod[n], ch);
}

/* Test whether mod starts with modname, followed by a delimiter or '='. */
MAKE_INLINE bool
ModMatchEq(const char *mod, const char *modname, const ModChain *ch)
{
	size_t n = strlen(modname);
	return strncmp(mod, modname, n) == 0 &&
	       (IsDelimiter(mod[n], ch) || mod[n] == '=');
}

static bool
TryParseIntBase0(const char **pp, int *out_num)
{
	char *end;
	long n;

	errno = 0;
	n = strtol(*pp, &end, 0);

	if (end == *pp)
		return false;
	if ((n == LONG_MIN || n == LONG_MAX) && errno == ERANGE)
		return false;
	if (n < INT_MIN || n > INT_MAX)
		return false;

	*pp = end;
	*out_num = (int)n;
	return true;
}

static bool
TryParseSize(const char **pp, size_t *out_num)
{
	char *end;
	unsigned long n;

	if (!ch_isdigit(**pp))
		return false;

	errno = 0;
	n = strtoul(*pp, &end, 10);
	if (n == ULONG_MAX && errno == ERANGE)
		return false;
	if (n > SIZE_MAX)
		return false;

	*pp = end;
	*out_num = (size_t)n;
	return true;
}

static bool
TryParseChar(const char **pp, int base, char *out_ch)
{
	char *end;
	unsigned long n;

	if (!ch_isalnum(**pp))
		return false;

	errno = 0;
	n = strtoul(*pp, &end, base);
	if (n == ULONG_MAX && errno == ERANGE)
		return false;
	if (n > UCHAR_MAX)
		return false;

	*pp = end;
	*out_ch = (char)n;
	return true;
}

/*
 * Modify each word of the expression using the given function and place the
 * result back in the expression.
 */
static void
ModifyWords(ModChain *ch,
	    ModifyWordProc modifyWord, void *modifyWord_args,
	    bool oneBigWord)
{
	Expr *expr = ch->expr;
	const char *val = Expr_Str(expr);
	SepBuf result;
	SubstringWords words;
	size_t i;
	Substring word;

	if (!ModChain_ShouldEval(ch))
		return;

	if (oneBigWord) {
		SepBuf_Init(&result, ch->sep);
		/* XXX: performance: Substring_InitStr calls strlen */
		word = Substring_InitStr(val);
		modifyWord(word, &result, modifyWord_args);
		goto done;
	}

	words = Substring_Words(val, false);

	DEBUG3(VAR, "ModifyWords: split \"%s\" into %u %s\n",
	    val, (unsigned)words.len, words.len != 1 ? "words" : "word");

	SepBuf_Init(&result, ch->sep);
	for (i = 0; i < words.len; i++) {
		modifyWord(words.words[i], &result, modifyWord_args);
		if (result.buf.len > 0)
			SepBuf_Sep(&result);
	}

	SubstringWords_Free(words);

done:
	Expr_SetValueOwn(expr, SepBuf_DoneData(&result));
}

/* :@var@...${var}...@ */
static ApplyModifierResult
ApplyModifier_Loop(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;
	struct ModifyWord_LoopArgs args;
	char prev_sep;
	LazyBuf tvarBuf, strBuf;
	FStr tvar, str;

	args.scope = expr->scope;

	(*pp)++;		/* Skip the first '@' */
	if (!ParseModifierPart(pp, '@', '@', VARE_PARSE,
	    ch, &tvarBuf, NULL, NULL))
		return AMR_CLEANUP;
	tvar = LazyBuf_DoneGet(&tvarBuf);
	args.var = tvar.str;
	if (strchr(args.var, '$') != NULL) {
		Parse_Error(PARSE_FATAL,
		    "In the :@ modifier, the variable name \"%s\" "
		    "must not contain a dollar",
		    args.var);
		goto cleanup_tvar;
	}

	if (!ParseModifierPart(pp, '@', '@', VARE_PARSE_BALANCED,
	    ch, &strBuf, NULL, NULL))
		goto cleanup_tvar;
	str = LazyBuf_DoneGet(&strBuf);
	args.body = str.str;

	if (!Expr_ShouldEval(expr))
		goto done;

	args.emode = VarEvalMode_WithoutKeepDollar(expr->emode);
	prev_sep = ch->sep;
	ch->sep = ' ';		/* XXX: should be ch->sep for consistency */
	ModifyWords(ch, ModifyWord_Loop, &args, ch->oneBigWord);
	ch->sep = prev_sep;
	/* XXX: Consider restoring the previous value instead of deleting. */
	Var_Delete(expr->scope, args.var);

done:
	FStr_Done(&tvar);
	FStr_Done(&str);
	return AMR_OK;

cleanup_tvar:
	FStr_Done(&tvar);
	return AMR_CLEANUP;
}

static void
ParseModifier_Defined(const char **pp, ModChain *ch, bool shouldEval,
		      LazyBuf *buf)
{
	const char *p;

	p = *pp + 1;
	LazyBuf_Init(buf, p);
	while (!IsDelimiter(*p, ch)) {

		/*
		 * XXX: This code is similar to the one in Var_Parse. See if
		 * the code can be merged. See also ParseModifier_Match and
		 * ParseModifierPart.
		 */

		/* See Buf_AddEscaped in for.c for the counterpart. */
		if (*p == '\\') {
			char c = p[1];
			if ((IsDelimiter(c, ch) && c != '\0') ||
			    c == '$' || c == '\\') {
				if (shouldEval)
					LazyBuf_Add(buf, c);
				p += 2;
				continue;
			}
		}

		if (*p == '$') {
			FStr val = Var_Parse(&p, ch->expr->scope,
			    shouldEval ? ch->expr->emode : VARE_PARSE);
			/* TODO: handle errors */
			if (shouldEval)
				LazyBuf_AddStr(buf, val.str);
			FStr_Done(&val);
			continue;
		}

		if (shouldEval)
			LazyBuf_Add(buf, *p);
		p++;
	}
	*pp = p;
}

/* :Ddefined or :Uundefined */
static ApplyModifierResult
ApplyModifier_Defined(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;
	LazyBuf buf;
	bool shouldEval =
	    Expr_ShouldEval(expr) &&
	    (**pp == 'D') == (expr->defined == DEF_REGULAR);

	ParseModifier_Defined(pp, ch, shouldEval, &buf);

	Expr_Define(expr);
	if (shouldEval)
		Expr_SetValue(expr, Substring_Str(LazyBuf_Get(&buf)));
	LazyBuf_Done(&buf);

	return AMR_OK;
}

/* :L */
static ApplyModifierResult
ApplyModifier_Literal(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;

	(*pp)++;

	if (Expr_ShouldEval(expr)) {
		Expr_Define(expr);
		Expr_SetValueOwn(expr, bmake_strdup(expr->name));
	}

	return AMR_OK;
}

static bool
TryParseTime(const char **pp, time_t *out_time)
{
	char *end;
	unsigned long n;

	if (!ch_isdigit(**pp))
		return false;

	errno = 0;
	n = strtoul(*pp, &end, 10);
	if (n == ULONG_MAX && errno == ERANGE)
		return false;

	*pp = end;
	*out_time = (time_t)n;	/* ignore possible truncation for now */
	return true;
}

/* :gmtime and :localtime */
static ApplyModifierResult
ApplyModifier_Time(const char **pp, ModChain *ch)
{
	Expr *expr;
	time_t t;
	const char *args;
	const char *mod = *pp;
	bool gmt = mod[0] == 'g';

	if (!ModMatchEq(mod, gmt ? "gmtime" : "localtime", ch))
		return AMR_UNKNOWN;
	args = mod + (gmt ? 6 : 9);

	if (args[0] == '=') {
		const char *p = args + 1;
		LazyBuf buf;
		FStr arg;
		if (!ParseModifierPart(&p, ':', ch->endc, ch->expr->emode,
		    ch, &buf, NULL, NULL))
			return AMR_CLEANUP;
		arg = LazyBuf_DoneGet(&buf);
		if (ModChain_ShouldEval(ch)) {
			const char *arg_p = arg.str;
			if (!TryParseTime(&arg_p, &t) || *arg_p != '\0') {
				Parse_Error(PARSE_FATAL,
				    "Invalid time value \"%s\"", arg.str);
				FStr_Done(&arg);
				return AMR_CLEANUP;
			}
		} else
			t = 0;
		FStr_Done(&arg);
		*pp = p;
	} else {
		t = 0;
		*pp = args;
	}

	expr = ch->expr;
	if (Expr_ShouldEval(expr))
		Expr_SetValueOwn(expr, FormatTime(Expr_Str(expr), t, gmt));

	return AMR_OK;
}

/* :hash */
static ApplyModifierResult
ApplyModifier_Hash(const char **pp, ModChain *ch)
{
	if (!ModMatch(*pp, "hash", ch))
		return AMR_UNKNOWN;
	*pp += 4;

	if (ModChain_ShouldEval(ch))
		Expr_SetValueOwn(ch->expr, Hash(Expr_Str(ch->expr)));

	return AMR_OK;
}

/* :P */
static ApplyModifierResult
ApplyModifier_Path(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;
	GNode *gn;
	char *path;

	(*pp)++;

	if (!Expr_ShouldEval(expr))
		return AMR_OK;

	Expr_Define(expr);

	gn = Targ_FindNode(expr->name);
	if (gn == NULL || gn->type & OP_NOPATH)
		path = NULL;
	else if (gn->path != NULL)
		path = bmake_strdup(gn->path);
	else {
		SearchPath *searchPath = Suff_FindPath(gn);
		path = Dir_FindFile(expr->name, searchPath);
	}
	if (path == NULL)
		path = bmake_strdup(expr->name);
	Expr_SetValueOwn(expr, path);

	return AMR_OK;
}

/* :!cmd! */
static ApplyModifierResult
ApplyModifier_ShellCommand(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;
	LazyBuf cmdBuf;
	FStr cmd;

	(*pp)++;
	if (!ParseModifierPart(pp, '!', '!', expr->emode,
	    ch, &cmdBuf, NULL, NULL))
		return AMR_CLEANUP;
	cmd = LazyBuf_DoneGet(&cmdBuf);

	if (Expr_ShouldEval(expr)) {
		char *output, *error;
		output = Cmd_Exec(cmd.str, &error);
		Expr_SetValueOwn(expr, output);
		if (error != NULL) {
			Parse_Error(PARSE_WARNING, "%s", error);
			free(error);
		}
	} else
		Expr_SetValueRefer(expr, "");

	FStr_Done(&cmd);
	Expr_Define(expr);

	return AMR_OK;
}

/*
 * The :range modifier generates an integer sequence as long as the words.
 * The :range=7 modifier generates an integer sequence from 1 to 7.
 */
static ApplyModifierResult
ApplyModifier_Range(const char **pp, ModChain *ch)
{
	size_t n;
	Buffer buf;
	size_t i;

	const char *mod = *pp;
	if (!ModMatchEq(mod, "range", ch))
		return AMR_UNKNOWN;

	if (mod[5] == '=') {
		const char *p = mod + 6;
		if (!TryParseSize(&p, &n)) {
			Parse_Error(PARSE_FATAL,
			    "Invalid number \"%s\" for modifier \":range\"",
			    mod + 6);
			return AMR_CLEANUP;
		}
		*pp = p;
	} else {
		n = 0;
		*pp = mod + 5;
	}

	if (!ModChain_ShouldEval(ch))
		return AMR_OK;

	if (n == 0) {
		SubstringWords words = Expr_Words(ch->expr);
		n = words.len;
		SubstringWords_Free(words);
	}

	Buf_Init(&buf);

	for (i = 0; i < n; i++) {
		if (i != 0) {
			/*
			 * XXX: Use ch->sep instead of ' ', for consistency.
			 */
			Buf_AddByte(&buf, ' ');
		}
		Buf_AddInt(&buf, 1 + (int)i);
	}

	Expr_SetValueOwn(ch->expr, Buf_DoneData(&buf));
	return AMR_OK;
}

/* Parse a ':M' or ':N' modifier. */
static char *
ParseModifier_Match(const char **pp, const ModChain *ch)
{
	const char *mod = *pp;
	Expr *expr = ch->expr;
	bool copy = false;	/* pattern should be, or has been, copied */
	bool needSubst = false;
	const char *endpat;
	char *pattern;

	/*
	 * In the loop below, ignore ':' unless we are at (or back to) the
	 * original brace level.
	 * XXX: This will likely not work right if $() and ${} are intermixed.
	 */
	/*
	 * XXX: This code is similar to the one in Var_Parse.
	 * See if the code can be merged.
	 * See also ApplyModifier_Defined.
	 */
	int depth = 0;
	const char *p;
	for (p = mod + 1; *p != '\0' && !(*p == ':' && depth == 0); p++) {
		if (*p == '\\' && p[1] != '\0' &&
		    (IsDelimiter(p[1], ch) || p[1] == ch->startc)) {
			if (!needSubst)
				copy = true;
			p++;
			continue;
		}
		if (*p == '$')
			needSubst = true;
		if (*p == '(' || *p == '{')
			depth++;
		if (*p == ')' || *p == '}') {
			depth--;
			if (depth < 0)
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
			    /* XXX: ch->startc is missing here; see above */
			    IsDelimiter(src[1], ch))
				src++;
			*dst = *src;
		}
		*dst = '\0';
	} else {
		pattern = bmake_strsedup(mod + 1, endpat);
	}

	if (needSubst) {
		char *old_pattern = pattern;
		/*
		 * XXX: Contrary to ParseModifierPart, a dollar in a ':M' or
		 * ':N' modifier must be escaped as '$$', not as '\$'.
		 */
		pattern = Var_Subst(pattern, expr->scope, expr->emode);
		/* TODO: handle errors */
		free(old_pattern);
	}

	DEBUG2(VAR, "Pattern for ':%c' is \"%s\"\n", mod[0], pattern);

	return pattern;
}

struct ModifyWord_MatchArgs {
	const char *pattern;
	bool neg;
	bool error_reported;
};

static void
ModifyWord_Match(Substring word, SepBuf *buf, void *data)
{
	struct ModifyWord_MatchArgs *args = data;
	StrMatchResult res;
	assert(word.end[0] == '\0');	/* assume null-terminated word */
	res = Str_Match(word.start, args->pattern);
	if (res.error != NULL && !args->error_reported) {
		args->error_reported = true;
		Parse_Error(PARSE_FATAL,
		    "%s in pattern \"%s\" of modifier \"%s\"",
		    res.error, args->pattern, args->neg ? ":N" : ":M");
	}
	if (res.matched != args->neg)
		SepBuf_AddSubstring(buf, word);
}

/* :Mpattern or :Npattern */
static ApplyModifierResult
ApplyModifier_Match(const char **pp, ModChain *ch)
{
	char mod = **pp;
	char *pattern;

	pattern = ParseModifier_Match(pp, ch);

	if (ModChain_ShouldEval(ch)) {
		struct ModifyWord_MatchArgs args;
		args.pattern = pattern;
		args.neg = mod == 'N';
		args.error_reported = false;
		ModifyWords(ch, ModifyWord_Match, &args, ch->oneBigWord);
	}

	free(pattern);
	return AMR_OK;
}

struct ModifyWord_MtimeArgs {
	bool error;
	bool use_fallback;
	ApplyModifierResult rc;
	time_t fallback;
};

static void
ModifyWord_Mtime(Substring word, SepBuf *buf, void *data)
{
	struct ModifyWord_MtimeArgs *args = data;
	struct stat st;
	char tbuf[21];

	if (Substring_IsEmpty(word))
		return;
	assert(word.end[0] == '\0');	/* assume null-terminated word */
	if (stat(word.start, &st) < 0) {
		if (args->error) {
			Parse_Error(PARSE_FATAL,
			    "Cannot determine mtime for \"%s\": %s",
			    word.start, strerror(errno));
			args->rc = AMR_CLEANUP;
			return;
		}
		if (args->use_fallback)
			st.st_mtime = args->fallback;
		else
			time(&st.st_mtime);
	}
	snprintf(tbuf, sizeof(tbuf), "%u", (unsigned)st.st_mtime);
	SepBuf_AddStr(buf, tbuf);
}

/* :mtime */
static ApplyModifierResult
ApplyModifier_Mtime(const char **pp, ModChain *ch)
{
	const char *p, *mod = *pp;
	struct ModifyWord_MtimeArgs args;

	if (!ModMatchEq(mod, "mtime", ch))
		return AMR_UNKNOWN;
	*pp += 5;
	p = *pp;
	args.error = false;
	args.use_fallback = p[0] == '=';
	args.rc = AMR_OK;
	if (args.use_fallback) {
		p++;
		if (TryParseTime(&p, &args.fallback)) {
		} else if (strncmp(p, "error", 5) == 0) {
			p += 5;
			args.error = true;
		} else
			goto invalid_argument;
		if (!IsDelimiter(*p, ch))
			goto invalid_argument;
		*pp = p;
	}
	ModifyWords(ch, ModifyWord_Mtime, &args, ch->oneBigWord);
	return args.rc;

invalid_argument:
	Parse_Error(PARSE_FATAL,
	    "Invalid argument \"%.*s\" for modifier \":mtime\"",
	    (int)strcspn(*pp + 1, ":{}()"), *pp + 1);
	return AMR_CLEANUP;
}

static void
ParsePatternFlags(const char **pp, PatternFlags *pflags, bool *oneBigWord)
{
	for (;; (*pp)++) {
		if (**pp == 'g')
			pflags->subGlobal = true;
		else if (**pp == '1')
			pflags->subOnce = true;
		else if (**pp == 'W')
			*oneBigWord = true;
		else
			break;
	}
}

MAKE_INLINE PatternFlags
PatternFlags_None(void)
{
	PatternFlags pflags = { false, false, false, false };
	return pflags;
}

/* :S,from,to, */
static ApplyModifierResult
ApplyModifier_Subst(const char **pp, ModChain *ch)
{
	struct ModifyWord_SubstArgs args;
	bool oneBigWord;
	LazyBuf lhsBuf, rhsBuf;

	char delim = (*pp)[1];
	if (delim == '\0') {
		Parse_Error(PARSE_FATAL,
		    "Missing delimiter for modifier \":S\"");
		(*pp)++;
		return AMR_CLEANUP;
	}

	*pp += 2;

	args.pflags = PatternFlags_None();
	args.matched = false;

	if (**pp == '^') {
		args.pflags.anchorStart = true;
		(*pp)++;
	}

	if (!ParseModifierPart(pp, delim, delim, ch->expr->emode,
	    ch, &lhsBuf, &args.pflags, NULL))
		return AMR_CLEANUP;
	args.lhs = LazyBuf_Get(&lhsBuf);

	if (!ParseModifierPart(pp, delim, delim, ch->expr->emode,
	    ch, &rhsBuf, NULL, &args)) {
		LazyBuf_Done(&lhsBuf);
		return AMR_CLEANUP;
	}
	args.rhs = LazyBuf_Get(&rhsBuf);

	oneBigWord = ch->oneBigWord;
	ParsePatternFlags(pp, &args.pflags, &oneBigWord);

	ModifyWords(ch, ModifyWord_Subst, &args, oneBigWord);

	LazyBuf_Done(&lhsBuf);
	LazyBuf_Done(&rhsBuf);
	return AMR_OK;
}

#ifdef HAVE_REGEX_H

/* :C,from,to, */
static ApplyModifierResult
ApplyModifier_Regex(const char **pp, ModChain *ch)
{
	struct ModifyWord_SubstRegexArgs args;
	bool oneBigWord;
	int error;
	LazyBuf reBuf, replaceBuf;
	FStr re;

	char delim = (*pp)[1];
	if (delim == '\0') {
		Parse_Error(PARSE_FATAL,
		    "Missing delimiter for modifier \":C\"");
		(*pp)++;
		return AMR_CLEANUP;
	}

	*pp += 2;

	if (!ParseModifierPart(pp, delim, delim, ch->expr->emode,
	    ch, &reBuf, NULL, NULL))
		return AMR_CLEANUP;
	re = LazyBuf_DoneGet(&reBuf);

	if (!ParseModifierPart(pp, delim, delim, ch->expr->emode,
	    ch, &replaceBuf, NULL, NULL)) {
		FStr_Done(&re);
		return AMR_CLEANUP;
	}
	args.replace = LazyBuf_Get(&replaceBuf);

	args.pflags = PatternFlags_None();
	args.matched = false;
	oneBigWord = ch->oneBigWord;
	ParsePatternFlags(pp, &args.pflags, &oneBigWord);

	if (!ModChain_ShouldEval(ch))
		goto done;

	error = regcomp(&args.re, re.str, REG_EXTENDED);
	if (error != 0) {
		RegexError(error, &args.re, "Regex compilation error");
		LazyBuf_Done(&replaceBuf);
		FStr_Done(&re);
		return AMR_CLEANUP;
	}

	args.nsub = args.re.re_nsub + 1;
	if (args.nsub > 10)
		args.nsub = 10;

	ModifyWords(ch, ModifyWord_SubstRegex, &args, oneBigWord);

	regfree(&args.re);
done:
	LazyBuf_Done(&replaceBuf);
	FStr_Done(&re);
	return AMR_OK;
}

#endif

/* :Q, :q */
static ApplyModifierResult
ApplyModifier_Quote(const char **pp, ModChain *ch)
{
	LazyBuf buf;
	bool quoteDollar;

	quoteDollar = **pp == 'q';
	if (!IsDelimiter((*pp)[1], ch))
		return AMR_UNKNOWN;
	(*pp)++;

	if (!ModChain_ShouldEval(ch))
		return AMR_OK;

	QuoteShell(Expr_Str(ch->expr), quoteDollar, &buf);
	if (buf.data != NULL)
		Expr_SetValue(ch->expr, LazyBuf_DoneGet(&buf));
	else
		LazyBuf_Done(&buf);

	return AMR_OK;
}

static void
ModifyWord_Copy(Substring word, SepBuf *buf, void *data MAKE_ATTR_UNUSED)
{
	SepBuf_AddSubstring(buf, word);
}

/* :ts<separator> */
static ApplyModifierResult
ApplyModifier_ToSep(const char **pp, ModChain *ch)
{
	const char *sep = *pp + 2;

	/*
	 * Even in parse-only mode, apply the side effects, since the side
	 * effects are neither observable nor is there a performance penalty.
	 * Checking for VARE_EVAL for every single piece of code in here
	 * would make the code in this function too hard to read.
	 */

	/* ":ts<any><endc>" or ":ts<any>:" */
	if (sep[0] != ch->endc && IsDelimiter(sep[1], ch)) {
		*pp = sep + 1;
		ch->sep = sep[0];
		goto ok;
	}

	/* ":ts<endc>" or ":ts:" */
	if (IsDelimiter(sep[0], ch)) {
		*pp = sep;
		ch->sep = '\0';	/* no separator */
		goto ok;
	}

	if (sep[0] != '\\')
		return AMR_UNKNOWN;

	/* ":ts\n" */
	if (sep[1] == 'n') {
		*pp = sep + 2;
		ch->sep = '\n';
		goto ok;
	}

	/* ":ts\t" */
	if (sep[1] == 't') {
		*pp = sep + 2;
		ch->sep = '\t';
		goto ok;
	}

	/* ":ts\x40" or ":ts\100" */
	{
		const char *p = sep + 1;
		int base = 8;	/* assume octal */

		if (sep[1] == 'x') {
			base = 16;
			p++;
		} else if (!ch_isdigit(sep[1]))
			return AMR_UNKNOWN;	/* ":ts\..." */

		if (!TryParseChar(&p, base, &ch->sep)) {
			Parse_Error(PARSE_FATAL,
			    "Invalid character number at \"%s\"", p);
			return AMR_CLEANUP;
		}
		if (!IsDelimiter(*p, ch))
			return AMR_UNKNOWN;

		*pp = p;
	}

ok:
	ModifyWords(ch, ModifyWord_Copy, NULL, ch->oneBigWord);
	return AMR_OK;
}

static char *
str_totitle(const char *str)
{
	size_t i, n = strlen(str) + 1;
	char *res = bmake_malloc(n);
	for (i = 0; i < n; i++) {
		if (i == 0 || ch_isspace(res[i - 1]))
			res[i] = ch_toupper(str[i]);
		else
			res[i] = ch_tolower(str[i]);
	}
	return res;
}


static char *
str_toupper(const char *str)
{
	size_t i, n = strlen(str) + 1;
	char *res = bmake_malloc(n);
	for (i = 0; i < n; i++)
		res[i] = ch_toupper(str[i]);
	return res;
}

static char *
str_tolower(const char *str)
{
	size_t i, n = strlen(str) + 1;
	char *res = bmake_malloc(n);
	for (i = 0; i < n; i++)
		res[i] = ch_tolower(str[i]);
	return res;
}

/* :tA, :tu, :tl, :ts<separator>, etc. */
static ApplyModifierResult
ApplyModifier_To(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;
	const char *mod = *pp;
	assert(mod[0] == 't');

	if (IsDelimiter(mod[1], ch))
		return AMR_UNKNOWN;		/* ":t<endc>" or ":t:" */

	if (mod[1] == 's')
		return ApplyModifier_ToSep(pp, ch);

	if (!IsDelimiter(mod[2], ch))
		return AMR_UNKNOWN;

	if (mod[1] == 'A') {				/* :tA */
		*pp = mod + 2;
		ModifyWords(ch, ModifyWord_Realpath, NULL, ch->oneBigWord);
		return AMR_OK;
	}

	if (mod[1] == 't') {				/* :tt */
		*pp = mod + 2;
		if (Expr_ShouldEval(expr))
			Expr_SetValueOwn(expr, str_totitle(Expr_Str(expr)));
		return AMR_OK;
	}

	if (mod[1] == 'u') {				/* :tu */
		*pp = mod + 2;
		if (Expr_ShouldEval(expr))
			Expr_SetValueOwn(expr, str_toupper(Expr_Str(expr)));
		return AMR_OK;
	}

	if (mod[1] == 'l') {				/* :tl */
		*pp = mod + 2;
		if (Expr_ShouldEval(expr))
			Expr_SetValueOwn(expr, str_tolower(Expr_Str(expr)));
		return AMR_OK;
	}

	if (mod[1] == 'W' || mod[1] == 'w') {		/* :tW, :tw */
		*pp = mod + 2;
		ch->oneBigWord = mod[1] == 'W';
		return AMR_OK;
	}

	return AMR_UNKNOWN;		/* ":t<any>:" or ":t<any><endc>" */
}

/* :[#], :[1], :[-1..1], etc. */
static ApplyModifierResult
ApplyModifier_Words(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;
	int first, last;
	const char *p;
	LazyBuf argBuf;
	FStr arg;

	(*pp)++;		/* skip the '[' */
	if (!ParseModifierPart(pp, ']', ']', expr->emode,
	    ch, &argBuf, NULL, NULL))
		return AMR_CLEANUP;
	arg = LazyBuf_DoneGet(&argBuf);
	p = arg.str;

	if (!IsDelimiter(**pp, ch)) {
		Parse_Error(PARSE_FATAL,
		    "Extra text after \"[%s]\"", arg.str);
		FStr_Done(&arg);
		return AMR_CLEANUP;
	}

	if (!ModChain_ShouldEval(ch))
		goto ok;

	if (p[0] == '\0')
		goto bad_modifier;		/* Found ":[]". */

	if (strcmp(p, "#") == 0) {		/* Found ":[#]" */
		if (ch->oneBigWord)
			Expr_SetValueRefer(expr, "1");
		else {
			Buffer buf;

			SubstringWords words = Expr_Words(expr);
			size_t ac = words.len;
			SubstringWords_Free(words);

			Buf_Init(&buf);
			Buf_AddInt(&buf, (int)ac);
			Expr_SetValueOwn(expr, Buf_DoneData(&buf));
		}
		goto ok;
	}

	if (strcmp(p, "*") == 0) {		/* ":[*]" */
		ch->oneBigWord = true;
		goto ok;
	}

	if (strcmp(p, "@") == 0) {		/* ":[@]" */
		ch->oneBigWord = false;
		goto ok;
	}

	/* Expect ":[N]" or ":[start..end]" */
	if (!TryParseIntBase0(&p, &first))
		goto bad_modifier;

	if (p[0] == '\0')			/* ":[N]" */
		last = first;
	else if (strncmp(p, "..", 2) == 0) {
		p += 2;
		if (!TryParseIntBase0(&p, &last) || *p != '\0')
			goto bad_modifier;
	} else
		goto bad_modifier;

	if (first == 0 && last == 0) {		/* ":[0]" or ":[0..0]" */
		ch->oneBigWord = true;
		goto ok;
	}

	if (first == 0 || last == 0)		/* ":[0..N]" or ":[N..0]" */
		goto bad_modifier;

	Expr_SetValueOwn(expr,
	    VarSelectWords(Expr_Str(expr), first, last,
		ch->sep, ch->oneBigWord));

ok:
	FStr_Done(&arg);
	return AMR_OK;

bad_modifier:
	Parse_Error(PARSE_FATAL, "Invalid modifier \":[%s]\"", arg.str);
	FStr_Done(&arg);
	return AMR_CLEANUP;
}

#if __STDC_VERSION__ >= 199901L || defined(HAVE_LONG_LONG_INT)
# define NUM_TYPE long long
# define PARSE_NUM_TYPE strtoll
#else
# define NUM_TYPE long
# define PARSE_NUM_TYPE strtol
#endif

static NUM_TYPE
num_val(Substring s)
{
	NUM_TYPE val;
	char *ep;

	val = PARSE_NUM_TYPE(s.start, &ep, 0);
	if (ep != s.start) {
		switch (*ep) {
		case 'K':
		case 'k':
			val <<= 10;
			break;
		case 'M':
		case 'm':
			val <<= 20;
			break;
		case 'G':
		case 'g':
			val <<= 30;
			break;
		}
	}
	return val;
}

static int
SubNumAsc(const void *sa, const void *sb)
{
	NUM_TYPE a, b;

	a = num_val(*((const Substring *)sa));
	b = num_val(*((const Substring *)sb));
	return a > b ? 1 : b > a ? -1 : 0;
}

static int
SubNumDesc(const void *sa, const void *sb)
{
	return SubNumAsc(sb, sa);
}

static int
Substring_Cmp(Substring a, Substring b)
{
	for (; a.start < a.end && b.start < b.end; a.start++, b.start++)
		if (a.start[0] != b.start[0])
			return (unsigned char)a.start[0]
			    - (unsigned char)b.start[0];
	return (int)((a.end - a.start) - (b.end - b.start));
}

static int
SubStrAsc(const void *sa, const void *sb)
{
	return Substring_Cmp(*(const Substring *)sa, *(const Substring *)sb);
}

static int
SubStrDesc(const void *sa, const void *sb)
{
	return SubStrAsc(sb, sa);
}

static void
ShuffleSubstrings(Substring *strs, size_t n)
{
	size_t i;

	for (i = n - 1; i > 0; i--) {
		size_t rndidx = (size_t)random() % (i + 1);
		Substring t = strs[i];
		strs[i] = strs[rndidx];
		strs[rndidx] = t;
	}
}

/*
 * :O		order ascending
 * :Or		order descending
 * :Ox		shuffle
 * :On		numeric ascending
 * :Onr, :Orn	numeric descending
 */
static ApplyModifierResult
ApplyModifier_Order(const char **pp, ModChain *ch)
{
	const char *mod = *pp;
	SubstringWords words;
	int (*cmp)(const void *, const void *);

	if (IsDelimiter(mod[1], ch)) {
		cmp = SubStrAsc;
		(*pp)++;
	} else if (IsDelimiter(mod[2], ch)) {
		if (mod[1] == 'n')
			cmp = SubNumAsc;
		else if (mod[1] == 'r')
			cmp = SubStrDesc;
		else if (mod[1] == 'x')
			cmp = NULL;
		else
			return AMR_UNKNOWN;
		*pp += 2;
	} else if (IsDelimiter(mod[3], ch)) {
		if ((mod[1] == 'n' && mod[2] == 'r') ||
		    (mod[1] == 'r' && mod[2] == 'n'))
			cmp = SubNumDesc;
		else
			return AMR_UNKNOWN;
		*pp += 3;
	} else
		return AMR_UNKNOWN;

	if (!ModChain_ShouldEval(ch))
		return AMR_OK;

	words = Expr_Words(ch->expr);
	if (cmp == NULL)
		ShuffleSubstrings(words.words, words.len);
	else {
		assert(words.words[0].end[0] == '\0');
		qsort(words.words, words.len, sizeof(words.words[0]), cmp);
	}
	Expr_SetValueOwn(ch->expr, SubstringWords_JoinFree(words));

	return AMR_OK;
}

/* :? then : else */
static ApplyModifierResult
ApplyModifier_IfElse(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;
	LazyBuf thenBuf;
	LazyBuf elseBuf;

	VarEvalMode then_emode = VARE_PARSE;
	VarEvalMode else_emode = VARE_PARSE;
	int parseErrorsBefore = parseErrors;

	CondResult cond_rc = CR_TRUE;	/* anything other than CR_ERROR */
	if (Expr_ShouldEval(expr)) {
		evalStack.elems[evalStack.len - 1].kind = VSK_COND;
		cond_rc = Cond_EvalCondition(expr->name);
		if (cond_rc == CR_TRUE)
			then_emode = expr->emode;
		else if (cond_rc == CR_FALSE)
			else_emode = expr->emode;
		else if (parseErrors == parseErrorsBefore)
			Parse_Error(PARSE_FATAL, "Bad condition");
	}

	evalStack.elems[evalStack.len - 1].kind = VSK_COND_THEN;
	(*pp)++;		/* skip past the '?' */
	if (!ParseModifierPart(pp, ':', ':', then_emode,
	    ch, &thenBuf, NULL, NULL))
		return AMR_CLEANUP;

	evalStack.elems[evalStack.len - 1].kind = VSK_COND_ELSE;
	if (!ParseModifierPart(pp, ch->endc, ch->endc, else_emode,
	    ch, &elseBuf, NULL, NULL)) {
		LazyBuf_Done(&thenBuf);
		return AMR_CLEANUP;
	}

	(*pp)--;		/* Go back to the ch->endc. */

	if (cond_rc == CR_ERROR) {
		LazyBuf_Done(&thenBuf);
		LazyBuf_Done(&elseBuf);
		return AMR_CLEANUP;
	}

	if (!Expr_ShouldEval(expr)) {
		LazyBuf_Done(&thenBuf);
		LazyBuf_Done(&elseBuf);
	} else if (cond_rc == CR_TRUE) {
		Expr_SetValue(expr, LazyBuf_DoneGet(&thenBuf));
		LazyBuf_Done(&elseBuf);
	} else {
		LazyBuf_Done(&thenBuf);
		Expr_SetValue(expr, LazyBuf_DoneGet(&elseBuf));
	}
	Expr_Define(expr);
	return AMR_OK;
}

/*
 * The ::= modifiers are special in that they do not read the variable value
 * but instead assign to that variable.  They always expand to an empty
 * string.
 *
 * Their main purpose is in supporting .for loops that generate shell commands
 * since an ordinary variable assignment at that point would terminate the
 * dependency group for these targets.  For example:
 *
 * list-targets: .USE
 * .for i in ${.TARGET} ${.TARGET:R}.gz
 *	@${t::=$i}
 *	@echo 'The target is ${t:T}.'
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
ApplyModifier_Assign(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;
	GNode *scope;
	FStr val;
	LazyBuf buf;

	const char *mod = *pp;
	const char *op = mod + 1;

	if (op[0] == '=')
		goto found_op;
	if ((op[0] == '+' || op[0] == '?' || op[0] == '!') && op[1] == '=')
		goto found_op;
	return AMR_UNKNOWN;	/* "::<unrecognized>" */

found_op:
	if (expr->name[0] == '\0') {
		const char *value = op[0] == '=' ? op + 1 : op + 2;
		*pp = mod + 1;
		/* Take a guess at where the modifier ends. */
		Parse_Error(PARSE_FATAL,
		    "Invalid attempt to assign \"%.*s\" to variable \"\" "
		    "via modifier \":%.*s\"",
		    (int)strcspn(value, ":)}"), value,
		    (int)(value - mod), mod);
		return AMR_CLEANUP;
	}

	*pp = mod + (op[0] != '=' ? 3 : 2);

	if (!ParseModifierPart(pp, ch->endc, ch->endc, expr->emode,
	    ch, &buf, NULL, NULL))
		return AMR_CLEANUP;
	val = LazyBuf_DoneGet(&buf);

	(*pp)--;		/* Go back to the ch->endc. */

	if (!Expr_ShouldEval(expr))
		goto done;

	scope = expr->scope;	/* scope where v belongs */
	if (expr->defined == DEF_REGULAR && expr->scope != SCOPE_GLOBAL
	    && VarFind(expr->name, expr->scope, false) == NULL)
		scope = SCOPE_GLOBAL;

	if (op[0] == '+')
		Var_Append(scope, expr->name, val.str);
	else if (op[0] == '!') {
		char *output, *error;
		output = Cmd_Exec(val.str, &error);
		if (error != NULL) {
			Parse_Error(PARSE_WARNING, "%s", error);
			free(error);
		} else
			Var_Set(scope, expr->name, output);
		free(output);
	} else if (op[0] == '?' && expr->defined == DEF_REGULAR) {
		/* Do nothing. */
	} else
		Var_Set(scope, expr->name, val.str);

	Expr_SetValueRefer(expr, "");

done:
	FStr_Done(&val);
	return AMR_OK;
}

/*
 * :_=...
 * remember current value
 */
static ApplyModifierResult
ApplyModifier_Remember(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;
	const char *mod = *pp;
	FStr name;

	if (!ModMatchEq(mod, "_", ch))
		return AMR_UNKNOWN;

	name = FStr_InitRefer("_");
	if (mod[1] == '=') {
		/*
		 * XXX: This ad-hoc call to strcspn deviates from the usual
		 * behavior defined in ParseModifierPart.  This creates an
		 * unnecessary and undocumented inconsistency in make.
		 */
		const char *arg = mod + 2;
		size_t argLen = strcspn(arg, ":)}");
		*pp = arg + argLen;
		name = FStr_InitOwn(bmake_strldup(arg, argLen));
	} else
		*pp = mod + 1;

	if (Expr_ShouldEval(expr))
		Var_Set(SCOPE_GLOBAL, name.str, Expr_Str(expr));
	FStr_Done(&name);

	return AMR_OK;
}

/*
 * Apply the given function to each word of the variable value,
 * for a single-letter modifier such as :H, :T.
 */
static ApplyModifierResult
ApplyModifier_WordFunc(const char **pp, ModChain *ch,
		       ModifyWordProc modifyWord)
{
	if (!IsDelimiter((*pp)[1], ch))
		return AMR_UNKNOWN;
	(*pp)++;

	ModifyWords(ch, modifyWord, NULL, ch->oneBigWord);

	return AMR_OK;
}

/* Remove adjacent duplicate words. */
static ApplyModifierResult
ApplyModifier_Unique(const char **pp, ModChain *ch)
{
	SubstringWords words;

	if (!IsDelimiter((*pp)[1], ch))
		return AMR_UNKNOWN;
	(*pp)++;

	if (!ModChain_ShouldEval(ch))
		return AMR_OK;

	words = Expr_Words(ch->expr);

	if (words.len > 1) {
		size_t di, si;
		for (di = 0, si = 1; si < words.len; si++)
			if (!Substring_Eq(words.words[di], words.words[si]))
				words.words[++di] = words.words[si];
		words.len = di + 1;
	}

	Expr_SetValueOwn(ch->expr, SubstringWords_JoinFree(words));

	return AMR_OK;
}

/* Test whether the modifier has the form '<lhs>=<rhs>'. */
static bool
IsSysVModifier(const char *p, char startc, char endc)
{
	bool eqFound = false;

	int depth = 1;
	while (*p != '\0') {
		if (*p == '=')	/* XXX: should also test depth == 1 */
			eqFound = true;
		else if (*p == endc) {
			if (--depth == 0)
				break;
		} else if (*p == startc)
			depth++;
		p++;
	}
	return eqFound;
}

/* :from=to */
static ApplyModifierResult
ApplyModifier_SysV(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;
	LazyBuf lhsBuf, rhsBuf;
	FStr rhs;
	struct ModifyWord_SysVSubstArgs args;
	Substring lhs;
	const char *lhsSuffix;

	const char *mod = *pp;

	if (!IsSysVModifier(mod, ch->startc, ch->endc))
		return AMR_UNKNOWN;

	if (!ParseModifierPart(pp, '=', '=', expr->emode,
	    ch, &lhsBuf, NULL, NULL))
		return AMR_CLEANUP;

	if (!ParseModifierPart(pp, ch->endc, ch->endc, expr->emode,
	    ch, &rhsBuf, NULL, NULL)) {
		LazyBuf_Done(&lhsBuf);
		return AMR_CLEANUP;
	}
	rhs = LazyBuf_DoneGet(&rhsBuf);

	(*pp)--;		/* Go back to the ch->endc. */

	/* Do not turn an empty expression into non-empty. */
	if (lhsBuf.len == 0 && Expr_Str(expr)[0] == '\0')
		goto done;

	lhs = LazyBuf_Get(&lhsBuf);
	lhsSuffix = Substring_SkipFirst(lhs, '%');

	args.scope = expr->scope;
	args.lhsPrefix = Substring_Init(lhs.start,
	    lhsSuffix != lhs.start ? lhsSuffix - 1 : lhs.start);
	args.lhsPercent = lhsSuffix != lhs.start;
	args.lhsSuffix = Substring_Init(lhsSuffix, lhs.end);
	args.rhs = rhs.str;

	ModifyWords(ch, ModifyWord_SysVSubst, &args, ch->oneBigWord);

done:
	LazyBuf_Done(&lhsBuf);
	FStr_Done(&rhs);
	return AMR_OK;
}

/* :sh */
static ApplyModifierResult
ApplyModifier_SunShell(const char **pp, ModChain *ch)
{
	Expr *expr = ch->expr;
	const char *p = *pp;
	if (!(p[1] == 'h' && IsDelimiter(p[2], ch)))
		return AMR_UNKNOWN;
	*pp = p + 2;

	if (Expr_ShouldEval(expr)) {
		char *output, *error;
		output = Cmd_Exec(Expr_Str(expr), &error);
		if (error != NULL) {
			Parse_Error(PARSE_WARNING, "%s", error);
			free(error);
		}
		Expr_SetValueOwn(expr, output);
	}

	return AMR_OK;
}

/*
 * In cases where the evaluation mode and the definedness are the "standard"
 * ones, don't log them, to keep the logs readable.
 */
static bool
ShouldLogInSimpleFormat(const Expr *expr)
{
	return (expr->emode == VARE_EVAL
		|| expr->emode == VARE_EVAL_DEFINED
		|| expr->emode == VARE_EVAL_DEFINED_LOUD)
	    && expr->defined == DEF_REGULAR;
}

static void
LogBeforeApply(const ModChain *ch, const char *mod)
{
	const Expr *expr = ch->expr;
	bool is_single_char = mod[0] != '\0' && IsDelimiter(mod[1], ch);

	/*
	 * At this point, only the first character of the modifier can
	 * be used since the end of the modifier is not yet known.
	 */

	if (!Expr_ShouldEval(expr)) {
		debug_printf("Parsing modifier ${%s:%c%s}\n",
		    expr->name, mod[0], is_single_char ? "" : "...");
		return;
	}

	if (ShouldLogInSimpleFormat(expr)) {
		debug_printf(
		    "Evaluating modifier ${%s:%c%s} on value \"%s\"\n",
		    expr->name, mod[0], is_single_char ? "" : "...",
		    Expr_Str(expr));
		return;
	}

	debug_printf(
	    "Evaluating modifier ${%s:%c%s} on value \"%s\" (%s, %s)\n",
	    expr->name, mod[0], is_single_char ? "" : "...", Expr_Str(expr),
	    VarEvalMode_Name[expr->emode], ExprDefined_Name[expr->defined]);
}

static void
LogAfterApply(const ModChain *ch, const char *p, const char *mod)
{
	const Expr *expr = ch->expr;
	const char *value = Expr_Str(expr);

	if (ShouldLogInSimpleFormat(expr)) {
		debug_printf("Result of ${%s:%.*s} is \"%s\"\n",
		    expr->name, (int)(p - mod), mod, value);
		return;
	}

	debug_printf("Result of ${%s:%.*s} is \"%s\" (%s, %s)\n",
	    expr->name, (int)(p - mod), mod, value,
	    VarEvalMode_Name[expr->emode],
	    ExprDefined_Name[expr->defined]);
}

static ApplyModifierResult
ApplyModifier(const char **pp, ModChain *ch)
{
	switch (**pp) {
	case '!':
		return ApplyModifier_ShellCommand(pp, ch);
	case ':':
		return ApplyModifier_Assign(pp, ch);
	case '?':
		return ApplyModifier_IfElse(pp, ch);
	case '@':
		return ApplyModifier_Loop(pp, ch);
	case '[':
		return ApplyModifier_Words(pp, ch);
	case '_':
		return ApplyModifier_Remember(pp, ch);
#ifdef HAVE_REGEX_H
	case 'C':
		return ApplyModifier_Regex(pp, ch);
#endif
	case 'D':
	case 'U':
		return ApplyModifier_Defined(pp, ch);
	case 'E':
		return ApplyModifier_WordFunc(pp, ch, ModifyWord_Suffix);
	case 'g':
	case 'l':
		return ApplyModifier_Time(pp, ch);
	case 'H':
		return ApplyModifier_WordFunc(pp, ch, ModifyWord_Head);
	case 'h':
		return ApplyModifier_Hash(pp, ch);
	case 'L':
		return ApplyModifier_Literal(pp, ch);
	case 'M':
	case 'N':
		return ApplyModifier_Match(pp, ch);
	case 'm':
		return ApplyModifier_Mtime(pp, ch);
	case 'O':
		return ApplyModifier_Order(pp, ch);
	case 'P':
		return ApplyModifier_Path(pp, ch);
	case 'Q':
	case 'q':
		return ApplyModifier_Quote(pp, ch);
	case 'R':
		return ApplyModifier_WordFunc(pp, ch, ModifyWord_Root);
	case 'r':
		return ApplyModifier_Range(pp, ch);
	case 'S':
		return ApplyModifier_Subst(pp, ch);
	case 's':
		return ApplyModifier_SunShell(pp, ch);
	case 'T':
		return ApplyModifier_WordFunc(pp, ch, ModifyWord_Tail);
	case 't':
		return ApplyModifier_To(pp, ch);
	case 'u':
		return ApplyModifier_Unique(pp, ch);
	default:
		return AMR_UNKNOWN;
	}
}

static void ApplyModifiers(Expr *, const char **, char, char);

typedef enum ApplyModifiersIndirectResult {
	/* The indirect modifiers have been applied successfully. */
	AMIR_CONTINUE,
	/* Fall back to the SysV modifier. */
	AMIR_SYSV,
	/* Error out. */
	AMIR_OUT
} ApplyModifiersIndirectResult;

/*
 * While expanding an expression, expand and apply indirect modifiers,
 * such as in ${VAR:${M_indirect}}.
 *
 * All indirect modifiers of a group must come from a single
 * expression.  ${VAR:${M1}} is valid but ${VAR:${M1}${M2}} is not.
 *
 * Multiple groups of indirect modifiers can be chained by separating them
 * with colons.  ${VAR:${M1}:${M2}} contains 2 indirect modifiers.
 *
 * If the expression is not followed by ch->endc or ':', fall
 * back to trying the SysV modifier, such as in ${VAR:${FROM}=${TO}}.
 */
static ApplyModifiersIndirectResult
ApplyModifiersIndirect(ModChain *ch, const char **pp)
{
	Expr *expr = ch->expr;
	const char *p = *pp;
	FStr mods = Var_Parse(&p, expr->scope, expr->emode);
	/* TODO: handle errors */

	if (mods.str[0] != '\0' && !IsDelimiter(*p, ch)) {
		FStr_Done(&mods);
		return AMIR_SYSV;
	}

	DEBUG3(VAR, "Indirect modifier \"%s\" from \"%.*s\"\n",
	    mods.str, (int)(p - *pp), *pp);

	if (ModChain_ShouldEval(ch) && mods.str[0] != '\0') {
		const char *modsp = mods.str;
		EvalStack_Push(VSK_INDIRECT_MODIFIERS, mods.str, NULL);
		ApplyModifiers(expr, &modsp, '\0', '\0');
		EvalStack_Pop();
		if (Expr_Str(expr) == var_Error || *modsp != '\0') {
			FStr_Done(&mods);
			*pp = p;
			return AMIR_OUT;	/* error already reported */
		}
	}
	FStr_Done(&mods);

	if (*p == ':')
		p++;
	else if (*p == '\0' && ch->endc != '\0') {
		Parse_Error(PARSE_FATAL,
		    "Unclosed expression after indirect modifier, "
		    "expecting \"%c\"",
		    ch->endc);
		*pp = p;
		return AMIR_OUT;
	}

	*pp = p;
	return AMIR_CONTINUE;
}

static ApplyModifierResult
ApplySingleModifier(const char **pp, ModChain *ch)
{
	ApplyModifierResult res;
	const char *mod = *pp;
	const char *p = *pp;

	if (DEBUG(VAR))
		LogBeforeApply(ch, mod);

	if (posix_state == PS_SET)
		res = ApplyModifier_SysV(&p, ch);
	else
		res = AMR_UNKNOWN;
	if (res == AMR_UNKNOWN)
		res = ApplyModifier(&p, ch);

	if (res == AMR_UNKNOWN && posix_state != PS_SET) {
		assert(p == mod);
		res = ApplyModifier_SysV(&p, ch);
	}

	if (res == AMR_UNKNOWN) {
		/*
		 * Guess the end of the current modifier.
		 * XXX: Skipping the rest of the modifier hides
		 * errors and leads to wrong results.
		 * Parsing should rather stop here.
		 */
		for (p++; !IsDelimiter(*p, ch); p++)
			continue;
		Parse_Error(PARSE_FATAL, "Unknown modifier \":%.*s\"",
		    (int)(p - mod), mod);
		Expr_SetValueRefer(ch->expr, var_Error);
		res = AMR_CLEANUP;
	}
	if (res != AMR_OK) {
		*pp = p;
		return res;
	}

	if (DEBUG(VAR))
		LogAfterApply(ch, p, mod);

	if (*p == '\0' && ch->endc != '\0') {
		Parse_Error(PARSE_FATAL,
		    "Unclosed expression, expecting \"%c\" for "
		    "modifier \"%.*s\"",
		    ch->endc, (int)(p - mod), mod);
	} else if (*p == ':') {
		p++;
	} else if (opts.strict && *p != '\0' && *p != ch->endc) {
		Parse_Error(PARSE_FATAL,
		    "Missing delimiter \":\" after modifier \"%.*s\"",
		    (int)(p - mod), mod);
		/*
		 * TODO: propagate parse error to the enclosing
		 * expression
		 */
	}
	*pp = p;
	return AMR_OK;
}

#if __STDC_VERSION__ >= 199901L
#define ModChain_Init(expr, startc, endc, sep, oneBigWord) \
	(ModChain) { expr, startc, endc, sep, oneBigWord }
#else
MAKE_INLINE ModChain
ModChain_Init(Expr *expr, char startc, char endc, char sep, bool oneBigWord)
{
	ModChain ch;
	ch.expr = expr;
	ch.startc = startc;
	ch.endc = endc;
	ch.sep = sep;
	ch.oneBigWord = oneBigWord;
	return ch;
}
#endif

/* Apply any modifiers (such as :Mpattern or :@var@loop@ or :Q or ::=value). */
static void
ApplyModifiers(
    Expr *expr,
    const char **pp,	/* the parsing position, updated upon return */
    char startc,	/* '(' or '{'; or '\0' for indirect modifiers */
    char endc		/* ')' or '}'; or '\0' for indirect modifiers */
)
{
	ModChain ch = ModChain_Init(expr, startc, endc, ' ', false);
	const char *p;

	assert(startc == '(' || startc == '{' || startc == '\0');
	assert(endc == ')' || endc == '}' || endc == '\0');
	assert(Expr_Str(expr) != NULL);

	p = *pp;

	if (*p == '\0' && endc != '\0') {
		Parse_Error(PARSE_FATAL,
		    "Unclosed expression, expecting \"%c\"", ch.endc);
		goto cleanup;
	}

	while (*p != '\0' && *p != endc) {
		ApplyModifierResult res;

		if (*p == '$') {
			/*
			 * TODO: Only evaluate the expression once, no matter
			 * whether it's an indirect modifier or the initial
			 * part of a SysV modifier.
			 */
			ApplyModifiersIndirectResult amir =
			    ApplyModifiersIndirect(&ch, &p);
			if (amir == AMIR_CONTINUE)
				continue;
			if (amir == AMIR_OUT)
				break;
		}

		res = ApplySingleModifier(&p, &ch);
		if (res == AMR_CLEANUP)
			goto cleanup;
	}

	*pp = p;
	assert(Expr_Str(expr) != NULL);	/* Use var_Error or varUndefined. */
	return;

cleanup:
	/*
	 * TODO: Use p + strlen(p) instead, to stop parsing immediately.
	 *
	 * In the unit tests, this generates a few shell commands with
	 * unbalanced quotes.  Instead of producing these incomplete strings,
	 * commands with evaluation errors should not be run at all.
	 *
	 * To make that happen, Var_Subst must report the actual errors
	 * instead of returning the resulting string unconditionally.
	 */
	*pp = p;
	Expr_SetValueRefer(expr, var_Error);
}

/*
 * Only 4 of the 7 built-in local variables are treated specially as they are
 * the only ones that will be set when dynamic sources are expanded.
 */
static bool
VarnameIsDynamic(Substring varname)
{
	const char *name;
	size_t len;

	name = varname.start;
	len = Substring_Length(varname);
	if (len == 1 || (len == 2 && (name[1] == 'F' || name[1] == 'D'))) {
		switch (name[0]) {
		case '@':
		case '%':
		case '*':
		case '!':
			return true;
		}
		return false;
	}

	if ((len == 7 || len == 8) && name[0] == '.' && ch_isupper(name[1])) {
		return Substring_Equals(varname, ".TARGET") ||
		       Substring_Equals(varname, ".ARCHIVE") ||
		       Substring_Equals(varname, ".PREFIX") ||
		       Substring_Equals(varname, ".MEMBER");
	}

	return false;
}

static const char *
UndefinedShortVarValue(char varname, const GNode *scope)
{
	if (scope == SCOPE_CMDLINE || scope == SCOPE_GLOBAL) {
		/*
		 * If substituting a local variable in a non-local scope,
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
static void
ParseVarname(const char **pp, char startc, char endc,
	     GNode *scope, VarEvalMode emode,
	     LazyBuf *buf)
{
	const char *p = *pp;
	int depth = 0;

	LazyBuf_Init(buf, p);

	while (*p != '\0') {
		if ((*p == endc || *p == ':') && depth == 0)
			break;
		if (*p == startc)
			depth++;
		if (*p == endc)
			depth--;

		if (*p == '$') {
			FStr nested_val = Var_Parse(&p, scope, emode);
			/* TODO: handle errors */
			LazyBuf_AddStr(buf, nested_val.str);
			FStr_Done(&nested_val);
		} else {
			LazyBuf_Add(buf, *p);
			p++;
		}
	}
	*pp = p;
}

static bool
IsShortVarnameValid(char varname, const char *start)
{
	if (varname != '$' && varname != ':' && varname != '}' &&
	    varname != ')' && varname != '\0')
		return true;

	if (!opts.strict)
		return false;	/* XXX: Missing error message */

	if (varname == '$' && save_dollars)
		Parse_Error(PARSE_FATAL,
		    "To escape a dollar, use \\$, not $$, at \"%s\"", start);
	else if (varname == '\0')
		Parse_Error(PARSE_FATAL, "Dollar followed by nothing");
	else if (save_dollars)
		Parse_Error(PARSE_FATAL,
		    "Invalid variable name \"%c\", at \"%s\"", varname, start);

	return false;
}

/*
 * Parse a single-character variable name such as in $V or $@.
 * Return whether to continue parsing.
 */
static bool
ParseVarnameShort(char varname, const char **pp, GNode *scope,
		  VarEvalMode emode,
		  const char **out_false_val,
		  Var **out_true_var)
{
	char name[2];
	Var *v;
	const char *val;

	if (!IsShortVarnameValid(varname, *pp)) {
		(*pp)++;	/* only skip the '$' */
		*out_false_val = var_Error;
		return false;
	}

	name[0] = varname;
	name[1] = '\0';
	v = VarFind(name, scope, true);
	if (v != NULL) {
		/* No need to advance *pp, the calling code handles this. */
		*out_true_var = v;
		return true;
	}

	*pp += 2;

	val = UndefinedShortVarValue(varname, scope);
	if (val == NULL)
		val = emode == VARE_EVAL_DEFINED
		    || emode == VARE_EVAL_DEFINED_LOUD
		    ? var_Error : varUndefined;

	if ((opts.strict || emode == VARE_EVAL_DEFINED_LOUD)
	    && val == var_Error) {
		Parse_Error(PARSE_FATAL,
		    "Variable \"%s\" is undefined", name);
	}

	*out_false_val = val;
	return false;
}

/* Find variables like @F or <D. */
static Var *
FindLocalLegacyVar(Substring varname, GNode *scope,
		   const char **out_extraModifiers)
{
	Var *v;

	/* Only resolve these variables if scope is a "real" target. */
	if (scope == SCOPE_CMDLINE || scope == SCOPE_GLOBAL)
		return NULL;

	if (Substring_Length(varname) != 2)
		return NULL;
	if (varname.start[1] != 'F' && varname.start[1] != 'D')
		return NULL;
	if (strchr("@%?*!<>^", varname.start[0]) == NULL)
		return NULL;

	v = VarFindSubstring(Substring_Init(varname.start, varname.start + 1),
	    scope, false);
	if (v == NULL)
		return NULL;

	*out_extraModifiers = varname.start[1] == 'D' ? "H:" : "T:";
	return v;
}

static FStr
EvalUndefined(bool dynamic, const char *start, const char *p,
	      Substring varname, VarEvalMode emode, int parseErrorsBefore)
{
	if (dynamic)
		return FStr_InitOwn(bmake_strsedup(start, p));

	if (emode == VARE_EVAL_DEFINED_LOUD
	    || (emode == VARE_EVAL_DEFINED && opts.strict)) {
		if (parseErrors == parseErrorsBefore) {
			Parse_Error(PARSE_FATAL,
			    "Variable \"%.*s\" is undefined",
			    (int) Substring_Length(varname), varname.start);
		}
		return FStr_InitRefer(var_Error);
	}

	return FStr_InitRefer(
	    emode == VARE_EVAL_DEFINED_LOUD || emode == VARE_EVAL_DEFINED
		? var_Error : varUndefined);
}

static void
CheckVarname(Substring name)
{
	const char *p;

	for (p = name.start; p < name.end; p++) {
		if (ch_isspace(*p))
			break;
	}
	if (p < name.end) {
		Parse_Error(PARSE_WARNING,
		    ch_isprint(*p)
		    ? "Invalid character \"%c\" in variable name \"%.*s\""
		    : "Invalid character \"\\x%02x\" in variable name \"%.*s\"",
		    (int)(*p),
		    (int)Substring_Length(name), name.start);
	}
}

/*
 * Parse a long variable name enclosed in braces or parentheses such as $(VAR)
 * or ${VAR}, up to the closing brace or parenthesis, or in the case of
 * ${VAR:Modifiers}, up to the ':' that starts the modifiers.
 * Return whether to continue parsing.
 */
static bool
ParseVarnameLong(
	const char **pp,
	char startc,
	GNode *scope,
	VarEvalMode emode,
	VarEvalMode nested_emode,
	int parseErrorsBefore,

	const char **out_false_pp,
	FStr *out_false_val,

	char *out_true_endc,
	Var **out_true_v,
	bool *out_true_haveModifier,
	const char **out_true_extraModifiers,
	bool *out_true_dynamic,
	ExprDefined *out_true_exprDefined
)
{
	LazyBuf varname;
	Substring name;
	Var *v;
	bool haveModifier;
	bool dynamic = false;

	const char *p = *pp;
	const char *start = p;
	char endc = startc == '(' ? ')' : '}';

	p += 2;			/* skip "${" or "$(" or "y(" */
	ParseVarname(&p, startc, endc, scope, nested_emode, &varname);
	name = LazyBuf_Get(&varname);

	if (*p == ':')
		haveModifier = true;
	else if (*p == endc)
		haveModifier = false;
	else {
		Parse_Error(PARSE_FATAL, "Unclosed variable \"%.*s\"",
		    (int)Substring_Length(name), name.start);
		LazyBuf_Done(&varname);
		*out_false_pp = p;
		*out_false_val = FStr_InitRefer(var_Error);
		return false;
	}

	v = VarFindSubstring(name, scope, true);

	/*
	 * At this point, p points just after the variable name, either at
	 * ':' or at endc.
	 */

	if (v == NULL && Substring_Equals(name, ".SUFFIXES")) {
		char *suffixes = Suff_NamesStr();
		v = VarNew(FStr_InitRefer(".SUFFIXES"), suffixes,
		    true, false, true);
		free(suffixes);
	} else if (v == NULL)
		v = FindLocalLegacyVar(name, scope, out_true_extraModifiers);

	if (v == NULL) {
		/*
		 * Defer expansion of dynamic variables if they appear in
		 * non-local scope since they are not defined there.
		 */
		dynamic = VarnameIsDynamic(name) &&
			  (scope == SCOPE_CMDLINE || scope == SCOPE_GLOBAL);

		if (!haveModifier) {
			CheckVarname(name);
			p++;	/* skip endc */
			*out_false_pp = p;
			*out_false_val = EvalUndefined(dynamic, start, p,
			    name, emode, parseErrorsBefore);
			LazyBuf_Done(&varname);
			return false;
		}

		/*
		 * The expression is based on an undefined variable.
		 * Nevertheless it needs a Var, for modifiers that access the
		 * variable name, such as :L or :?.
		 *
		 * Most modifiers leave this expression in the "undefined"
		 * state (DEF_UNDEF), only a few modifiers like :D, :U, :L,
		 * :P turn this undefined expression into a defined
		 * expression (DEF_DEFINED).
		 *
		 * In the end, after applying all modifiers, if the expression
		 * is still undefined, Var_Parse will return an empty string
		 * instead of the actually computed value.
		 */
		v = VarNew(LazyBuf_DoneGet(&varname), "",
		    true, false, false);
		*out_true_exprDefined = DEF_UNDEF;
	} else
		LazyBuf_Done(&varname);

	*pp = p;
	*out_true_endc = endc;
	*out_true_v = v;
	*out_true_haveModifier = haveModifier;
	*out_true_dynamic = dynamic;
	return true;
}

#if __STDC_VERSION__ >= 199901L
#define Expr_Init(name, value, emode, scope, defined) \
	(Expr) { name, value, emode, scope, defined }
#else
MAKE_INLINE Expr
Expr_Init(const char *name, FStr value,
	  VarEvalMode emode, GNode *scope, ExprDefined defined)
{
	Expr expr;

	expr.name = name;
	expr.value = value;
	expr.emode = emode;
	expr.scope = scope;
	expr.defined = defined;
	return expr;
}
#endif

/*
 * Expressions of the form ${:U...} with a trivial value are often generated
 * by .for loops and are boring, so evaluate them without debug logging.
 */
static bool
Var_Parse_U(const char **pp, VarEvalMode emode, FStr *out_value)
{
	const char *p;

	p = *pp;
	if (!(p[0] == '$' && p[1] == '{' && p[2] == ':' && p[3] == 'U'))
		return false;

	p += 4;
	while (*p != '$' && *p != '{' && *p != ':' && *p != '\\' &&
	       *p != '}' && *p != '\0')
		p++;
	if (*p != '}')
		return false;

	*out_value = emode == VARE_PARSE
	    ? FStr_InitRefer("")
	    : FStr_InitOwn(bmake_strsedup(*pp + 4, p));
	*pp = p + 1;
	return true;
}

/*
 * Given the start of an expression (such as $v, $(VAR), ${VAR:Mpattern}),
 * extract the variable name and the modifiers, if any.  While parsing, apply
 * the modifiers to the value of the expression.
 *
 * Input:
 *	*pp		The string to parse.
 *			When called from CondParser_FuncCallEmpty, it can
 *			also point to the "y" of "empty(VARNAME:Modifiers)".
 *	scope		The scope for finding variables.
 *	emode		Controls the exact details of parsing and evaluation.
 *
 * Output:
 *	*pp		The position where to continue parsing.
 *			TODO: After a parse error, the value of *pp is
 *			unspecified.  It may not have been updated at all,
 *			point to some random character in the string, to the
 *			location of the parse error, or at the end of the
 *			string.
 *	return		The value of the expression, never NULL.
 *	return		var_Error if there was a parse error.
 *	return		var_Error if the base variable of the expression was
 *			undefined, emode is VARE_EVAL_DEFINED, and none of
 *			the modifiers turned the undefined expression into a
 *			defined expression.
 *			XXX: It is not guaranteed that an error message has
 *			been printed.
 *	return		varUndefined if the base variable of the expression
 *			was undefined, emode was not VARE_EVAL_DEFINED,
 *			and none of the modifiers turned the undefined
 *			expression into a defined expression.
 */
FStr
Var_Parse(const char **pp, GNode *scope, VarEvalMode emode)
{
	const char *start, *p;
	bool haveModifier;	/* true for ${VAR:...}, false for ${VAR} */
	char startc;		/* the actual '{' or '(' or '\0' */
	char endc;		/* the expected '}' or ')' or '\0' */
	/*
	 * true if the expression is based on one of the 7 predefined
	 * variables that are local to a target, and the expression is
	 * expanded in a non-local scope.  The result is the text of the
	 * expression, unaltered.  This is needed to support dynamic sources.
	 */
	bool dynamic;
	const char *extramodifiers;
	Var *v;
	Expr expr = Expr_Init(NULL, FStr_InitRefer(NULL),
	    emode == VARE_EVAL_DEFINED || emode == VARE_EVAL_DEFINED_LOUD
		? VARE_EVAL : emode,
	    scope, DEF_REGULAR);
	FStr val;
	int parseErrorsBefore = parseErrors;

	if (Var_Parse_U(pp, emode, &val))
		return val;

	p = *pp;
	start = p;
	DEBUG2(VAR, "Var_Parse: %s (%s)\n", start, VarEvalMode_Name[emode]);

	val = FStr_InitRefer(NULL);
	extramodifiers = NULL;	/* extra modifiers to apply first */
	dynamic = false;

	endc = '\0';		/* Appease GCC. */

	startc = p[1];
	if (startc != '(' && startc != '{') {
		if (!ParseVarnameShort(startc, pp, scope, emode, &val.str, &v))
			return val;
		haveModifier = false;
		p++;
	} else {
		if (!ParseVarnameLong(&p, startc, scope, emode, expr.emode,
		    parseErrorsBefore,
		    pp, &val,
		    &endc, &v, &haveModifier, &extramodifiers,
		    &dynamic, &expr.defined))
			return val;
	}

	expr.name = v->name.str;
	if (v->inUse && VarEvalMode_ShouldEval(emode)) {
		Parse_Error(PARSE_FATAL, "Variable %s is recursive.",
		    v->name.str);
		FStr_Done(&val);
		if (*p != '\0')
			p++;
		*pp = p;
		return FStr_InitRefer(var_Error);
	}

	/*
	 * FIXME: This assignment creates an alias to the current value of the
	 * variable.  This means that as long as the value of the expression
	 * stays the same, the value of the variable must not change, and the
	 * variable must not be deleted.  Using the ':@' modifier, it is
	 * possible (since var.c 1.212 from 2017-02-01) to delete the variable
	 * while its value is still being used:
	 *
	 *	VAR=	value
	 *	_:=	${VAR:${:U:@VAR@@}:S,^,prefix,}
	 *
	 * The same effect might be achievable using the '::=' or the ':_'
	 * modifiers.
	 *
	 * At the bottom of this function, the resulting value is compared to
	 * the then-current value of the variable.  This might also invoke
	 * undefined behavior.
	 */
	expr.value = FStr_InitRefer(v->val.data);

	if (!VarEvalMode_ShouldEval(emode))
		EvalStack_Push(VSK_EXPR_PARSE, start, NULL);
	else if (expr.name[0] != '\0')
		EvalStack_Push(VSK_VARNAME, expr.name, &expr.value);
	else
		EvalStack_Push(VSK_EXPR, start, &expr.value);

	/*
	 * Before applying any modifiers, expand any nested expressions from
	 * the variable value.
	 */
	if (VarEvalMode_ShouldEval(emode) &&
	    strchr(Expr_Str(&expr), '$') != NULL) {
		char *expanded;
		v->inUse = true;
		expanded = Var_Subst(Expr_Str(&expr), scope, expr.emode);
		v->inUse = false;
		/* TODO: handle errors */
		Expr_SetValueOwn(&expr, expanded);
	}

	if (extramodifiers != NULL) {
		const char *em = extramodifiers;
		ApplyModifiers(&expr, &em, '\0', '\0');
	}

	if (haveModifier) {
		p++;		/* Skip initial colon. */
		ApplyModifiers(&expr, &p, startc, endc);
	}

	if (*p != '\0')		/* Skip past endc if possible. */
		p++;

	*pp = p;

	if (expr.defined == DEF_UNDEF) {
		Substring varname = Substring_InitStr(expr.name);
		FStr value = EvalUndefined(dynamic, start, p, varname, emode,
		    parseErrorsBefore);
		Expr_SetValue(&expr, value);
	}

	EvalStack_Pop();

	if (v->shortLived) {
		if (expr.value.str == v->val.data) {
			/* move ownership */
			expr.value.freeIt = v->val.data;
			v->val.data = NULL;
		}
		VarFreeShortLived(v);
	}

	return expr.value;
}

static void
VarSubstDollarDollar(const char **pp, Buffer *res, VarEvalMode emode)
{
	/* A dollar sign may be escaped with another dollar sign. */
	if (save_dollars && VarEvalMode_ShouldKeepDollar(emode))
		Buf_AddByte(res, '$');
	Buf_AddByte(res, '$');
	*pp += 2;
}

static void
VarSubstExpr(const char **pp, Buffer *buf, GNode *scope, VarEvalMode emode)
{
	const char *p = *pp;
	const char *nested_p = p;
	FStr val = Var_Parse(&nested_p, scope, emode);
	/* TODO: handle errors */

	if (val.str == var_Error || val.str == varUndefined) {
		if (!VarEvalMode_ShouldKeepUndef(emode)
		    || val.str == var_Error) {
			p = nested_p;
		} else {
			/*
			 * Copy the initial '$' of the undefined expression,
			 * thereby deferring expansion of the expression, but
			 * expand nested expressions if already possible. See
			 * unit-tests/varparse-undef-partial.mk.
			 */
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
 * Skip as many characters as possible -- either to the end of the string,
 * or to the next dollar sign, which may start an expression.
 */
static void
VarSubstPlain(const char **pp, Buffer *res)
{
	const char *p = *pp;
	const char *start = p;

	for (p++; *p != '$' && *p != '\0'; p++)
		continue;
	Buf_AddRange(res, start, p);
	*pp = p;
}

/*
 * Expand all expressions like $V, ${VAR}, $(VAR:Modifiers) in the
 * given string.
 *
 * Input:
 *	str		The string in which the expressions are expanded.
 *	scope		The scope in which to start searching for variables.
 *			The other scopes are searched as well.
 *	emode		The mode for parsing or evaluating subexpressions.
 */
char *
Var_Subst(const char *str, GNode *scope, VarEvalMode emode)
{
	const char *p = str;
	Buffer res;

	Buf_Init(&res);

	while (*p != '\0') {
		if (p[0] == '$' && p[1] == '$')
			VarSubstDollarDollar(&p, &res, emode);
		else if (p[0] == '$')
			VarSubstExpr(&p, &res, scope, emode);
		else
			VarSubstPlain(&p, &res);
	}

	return Buf_DoneData(&res);
}

char *
Var_SubstInTarget(const char *str, GNode *scope)
{
	char *res;
	EvalStack_Push(VSK_TARGET, scope->name, NULL);
	EvalStack_Push(VSK_COMMAND, str, NULL);
	res = Var_Subst(str, scope, VARE_EVAL);
	EvalStack_Pop();
	EvalStack_Pop();
	return res;
}

void
Var_ExportStackTrace(const char *target, const char *cmd)
{
	char *stackTrace;

	if (GetParentStackTrace() == NULL)
		return;

	if (target != NULL)
		EvalStack_Push(VSK_TARGET, target, NULL);
	if (cmd != NULL)
		EvalStack_Push(VSK_COMMAND, cmd, NULL);

	stackTrace = GetStackTrace(true);
	(void)setenv("MAKE_STACK_TRACE", stackTrace, 1);
	free(stackTrace);

	if (cmd != NULL)
		EvalStack_Pop();
	if (target != NULL)
		EvalStack_Pop();
}

void
Var_Expand(FStr *str, GNode *scope, VarEvalMode emode)
{
	char *expanded;

	if (strchr(str->str, '$') == NULL)
		return;
	expanded = Var_Subst(str->str, scope, emode);
	/* TODO: handle errors */
	FStr_Done(str);
	*str = FStr_InitOwn(expanded);
}

void
Var_Stats(void)
{
	HashTable_DebugStats(&SCOPE_GLOBAL->vars, "Global variables");
}

static int
StrAsc(const void *sa, const void *sb)
{
	return strcmp(
	    *((const char *const *)sa), *((const char *const *)sb));
}


/* Print all variables in a scope, sorted by name. */
void
Var_Dump(GNode *scope)
{
	Vector /* of const char * */ vec;
	HashIter hi;
	size_t i;
	const char **varnames;

	Vector_Init(&vec, sizeof(const char *));

	HashIter_Init(&hi, &scope->vars);
	while (HashIter_Next(&hi))
		*(const char **)Vector_Push(&vec) = hi.entry->key;
	varnames = vec.items;

	qsort(varnames, vec.len, sizeof varnames[0], StrAsc);

	for (i = 0; i < vec.len; i++) {
		const char *varname = varnames[i];
		const Var *var = HashTable_FindValue(&scope->vars, varname);
		debug_printf("%-16s = %s%s\n", varname,
		    var->val.data, ValueDescription(var->val.data));
	}

	Vector_Done(&vec);
}
