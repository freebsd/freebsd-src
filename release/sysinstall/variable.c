/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $FreeBSD: src/release/sysinstall/variable.c,v 1.30.2.1 2000/09/16 09:08:45 jkh Exp $
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
make_variable(char *var, char *value, int dirty)
{
    Variable *vp;

    /* Trim leading and trailing whitespace */
    var = string_skipwhite(string_prune(var));

    if (!var || !*var)
	return;


    /* Now search to see if it's already in the list */
    for (vp = VarHead; vp; vp = vp->next) {
	if (!strcmp(vp->name, var)) {
	    if (vp->dirty && !dirty)
		return;
	    setenv(var, value, 1);
	    free(vp->value);
	    vp->value = strdup(value);
	    if (dirty != -1)
		vp->dirty = dirty;
	    return;
	}
    }

    setenv(var, value, 1);
    /* No?  Create a new one */
    vp = (Variable *)safe_malloc(sizeof(Variable));
    vp->name = strdup(var);
    vp->value = strdup(value);
    if (dirty == -1)
	dirty = 0;
    vp->dirty = dirty;
    vp->next = VarHead;
    VarHead = vp;
}

void
variable_set(char *var, int dirty)
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
    make_variable(tmp, string_skipwhite(cp), dirty);
}

void
variable_set2(char *var, char *value, int dirty)
{
    if (!var || !value)
	msgFatal("Null name or value passed to set_variable2(%s) = %s!",
		var ? var : "", value ? value : "");
    else if (!*var || !*value)
	msgDebug("Warning:  Zero length name or value passed to variable_set2(%s) = %s\n",
		var, value);
    make_variable(var, value, dirty);
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
variable_get_value(char *var, char *prompt, int dirty)
{
    char *cp;

    cp = variable_get(var);
    if (cp && variable_get(VAR_NONINTERACTIVE))
	return cp;
    else if ((cp = msgGetInput(cp, prompt)) != NULL)
	variable_set2(var, cp, dirty);
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
	else if ((cp3 = index(cp, ',')) != NULL)
	    *cp3 = '\0';
        cp2 = variable_get(tmp);
        if (cp2) {
	    if (!*cp)
		return TRUE;
	    else
        	return !strcmp(cp, cp2);
	}
        else
            return FALSE;
    }
    else
        return variable_get(tmp) ? TRUE : FALSE;
} 

int
dump_variables(dialogMenuItem *unused)
{
    FILE *fp;
    Variable *vp;

    if (isDebug())
	msgDebug("Writing sysinstall variables to file..");

    fp = fopen("/etc/sysinstall.vars", "w");
    if (!fp) {
	msgConfirm("Unable to write to /etc/sysinstall.vars: %s",
		   strerror(errno));
	return DITEM_FAILURE;
    }

    for (vp = VarHead; vp; vp = vp->next)
	fprintf(fp, "%s=\"%s\" (%d)\n", vp->name, vp->value, vp->dirty);

    fclose(fp);

    return DITEM_SUCCESS;
}
