/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: variable.c,v 1.21 1997/10/12 16:21:21 jkh Exp $
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
    Variable *vp;

    /* Trim leading and trailing whitespace */
    var = string_skipwhite(string_prune(var));

    if (!var || !*var)
	return;

    /* Put it in the environment in any case */
    setenv(var, value, 1);

    /* Now search to see if it's already in the list */
    for (vp = VarHead; vp; vp = vp->next) {
	if (!strcmp(vp->name, var)) {
	    if (isDebug())
		msgDebug("variable %s was %s, now %s\n", vp->name, vp->value, value);
	    free(vp->value);
	    vp->value = strdup(value);
	    return;
	}
    }

    /* No?  Create a new one */
    vp = (Variable *)safe_malloc(sizeof(Variable));
    vp->name = strdup(var);
    vp->value = strdup(value);
    vp->next = VarHead;
    VarHead = vp;
    if (isDebug())
	msgDebug("Setting variable %s to %s\n", vp->name, vp->value);
}

void
variable_set(char *var)
{
    char tmp[1024], *cp;

    if (!var)
	msgFatal("NULL variable name & value passed.");
    else if (!*var)
	msgDebug("Warning:  Zero length name & value passed to variable_set()\n");
    SAFE_STRCPY(tmp, var);
    if ((cp = index(tmp, '=')) == NULL)
	msgFatal("Invalid variable format: %s", var);
    *(cp++) = '\0';
    make_variable(tmp, string_skipwhite(cp));
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

int
variable_cmp(char *var, char *value)
{
    char *val;

    if ((val = variable_get(var)))
	return strcmp(val, value);
    return -1;
}

void
variable_unset(char *var)
{
    Variable *vp;
    char name[512], *cp;

    if ((cp = index(var, '=')) != NULL)
	sstrncpy(name, var, cp - var);
    else
	SAFE_STRCPY(name, var);
    unsetenv(name);
    /* Now search to see if it's in our list, if we have one.. */
    if (!VarHead)
	return;
    else if (!VarHead->next && !strcmp(VarHead->name, name)) {
	safe_free(VarHead->name);
	safe_free(VarHead->value);
	free(VarHead);
	VarHead = NULL;
    }
    else {
	for (vp = VarHead; vp; vp = vp->next) {
	    if (!strcmp(vp->name, name)) {
		Variable *save = vp->next;

		safe_free(vp->name);
		safe_free(vp->value);
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

    cp = variable_get(var);
    if (cp && variable_get(VAR_NONINTERACTIVE))
	return cp;
    else if ((cp = msgGetInput(cp, prompt)) != NULL)
	variable_set2(var, cp);
    else
	cp = NULL;
    return cp;
}

/* Check if value passed in data (in the form "variable=value") is equal to value of 
   variable stored in env */
int
variable_check(char *data)
{
    char *cp, *cp2, *cp3, tmp[256];

    if (!data)
	return FALSE;
    SAFE_STRCPY(tmp, data);
    if ((cp = index(tmp, '=')) != NULL) {
        *(cp++) = '\0';
	if (*cp == '"') {	/* smash quotes if present */
	    ++cp;
	    if ((cp3 = index(cp, '"')) != NULL)
		*cp3 = '\0';
	}
	if ((cp3 = index(cp, ',')) != NULL)
	    *cp3 = '\0';
        cp2 = getenv(tmp);

        if (cp2)
            return !strcmp(cp, cp2);
        else
            return FALSE;
    }
    else
        return getenv(tmp) ? 1 : 0;
} 
