/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: variable.c,v 1.6.2.1 1995/10/04 12:08:27 jkh Exp $
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
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

    /* First search to see if it's already there */
    for (newvar = VarHead; newvar; newvar = newvar->next) {
	if (!strcmp(newvar->name, var)) {
	    strncpy(newvar->value, value, VAR_VALUE_MAX);
	    setenv(var, value, 1);
	    return;
	}
    }
    setenv(var, value, 1);
    newvar = (Variable *)safe_malloc(sizeof(Variable));
    strncpy(newvar->name, var, VAR_NAME_MAX);
    strncpy(newvar->value, value, VAR_VALUE_MAX);
    newvar->next = VarHead;
    VarHead = newvar;
    setenv(newvar->name, newvar->value, 1);
    if (isDebug())
	msgDebug("Setting variable %s to %s\n", newvar->name, newvar->value);
}

void
variable_set(char *var)
{
    char tmp[VAR_NAME_MAX + VAR_VALUE_MAX], *cp;

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
    /* First search to see if it's already there */
    for (vp = VarHead; vp; vp = vp->next) {
	if (!strcmp(vp->name, var)) {
	    Variable *save = vp->next;

	    *vp = *save;
	    safe_free(save);
	    break;
	}
    }
}
