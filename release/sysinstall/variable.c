/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: variable.c,v 1.9 1996/04/23 01:29:35 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"

/* Routines for dealing with variable lists */

static void
make_variable(char *var, char *value)
{
    Variable *newvar;

    /* Put it in the environment in any case */
    setenv(var, value, 1);

    /* Now search to see if it's already in the list */
    for (newvar = VarHead; newvar; newvar = newvar->next) {
	if (!strcmp(newvar->name, var)) {
	    strncpy(newvar->value, value, VAR_VALUE_MAX);
	    return;
	}
    }

    /* No?  Create a new one */
    newvar = (Variable *)safe_malloc(sizeof(Variable));
    strncpy(newvar->name, var, VAR_NAME_MAX);
    strncpy(newvar->value, value, VAR_VALUE_MAX);
    newvar->next = VarHead;
    VarHead = newvar;
    if (isDebug())
	msgDebug("Setting variable %s to %s\n", newvar->name, newvar->value);
}

void
variable_set(char *var)
{
    char tmp[VAR_NAME_MAX + VAR_VALUE_MAX], *cp;

    if (!var)
	msgFatal("NULL variable name & value passed.");
    else if (!*var)
	msgDebug("Warning:  Zero length name & value passed to variable_set()\n");
    strncpy(tmp, var, VAR_NAME_MAX + VAR_VALUE_MAX);
    if ((cp = index(tmp, '=')) == NULL)
	msgFatal("Invalid variable format: %s", var);
    *(cp++) = '\0';
    make_variable(tmp, cp);
}

void
variable_set2(char *var, char *value)
{
    if (!var || !value)
	msgFatal("Null name or value passed to set_variable2!");
    else if (!*var || !*value)
	msgDebug("Warning:  Zero length name or value passed to variable_set2()\n");
    make_variable(var, value);
}

char *
variable_get(char *var)
{
    return getenv(var);
}

void
variable_unset(char *var)
{
    Variable *vp;

    unsetenv(var);

    /* Now search to see if it's in our list, if we have one.. */
    if (!VarHead)
	return;
    else if (!VarHead->next && !strcmp(VarHead->name, var)) {
	free(VarHead);
	VarHead = NULL;
    }
    else {
	for (vp = VarHead; vp; vp = vp->next) {
	    if (!strcmp(vp->name, var)) {
		Variable *save = vp->next;

		*vp = *save;
		safe_free(save);
		break;
	    }
	}
    }
}

/* Prompt user for the name of a variable */
char *
variable_get_value(char *var, char *prompt)
{
    char *cp;
    
    if ((cp = msgGetInput(variable_get(var), prompt)) != NULL)
	variable_set2(var, cp);
    else
	cp = NULL;
    return cp;
}
