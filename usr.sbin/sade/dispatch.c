/*
 * $FreeBSD$
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

#include "sade.h"
#include <ctype.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/fcntl.h>

#include "list.h"

static int dispatch_systemExecute(dialogMenuItem *unused);
static int dispatch_msgConfirm(dialogMenuItem *unused);

static struct _word {
    char *name;
    int (*handler)(dialogMenuItem *self);
} resWords[] = {
#ifdef WITH_SLICES
    { "diskPartitionEditor",	diskPartitionEditor	},
#endif
    { "diskPartitionWrite",	diskPartitionWrite	},
    { "diskLabelEditor",	diskLabelEditor		},
    { "diskLabelCommit",	diskLabelCommit		},
    { "msgConfirm",		dispatch_msgConfirm	},
    { "system",			dispatch_systemExecute	},
    { "dumpVariables",		dump_variables		},
    { NULL, NULL },
};

/*
 * Helper routines for buffering data.
 *
 * We read an entire configuration into memory before executing it
 * so that we are truely standalone and can do things like nuke the
 * file or disk we're working on.
 */

typedef struct command_buffer_ {
    qelement	queue;
    char *	string;
} command_buffer;

/*
 * Command processing
 */

static int
dispatch_systemExecute(dialogMenuItem *unused)
{
    char *cmd = variable_get(VAR_COMMAND);

    if (cmd)
	return systemExecute(cmd) ? DITEM_FAILURE : DITEM_SUCCESS;
    else
	msgDebug("_systemExecute: No command passed in `command' variable.\n");
    return DITEM_FAILURE;
}

static int
dispatch_msgConfirm(dialogMenuItem *unused)
{
    char *msg = variable_get(VAR_COMMAND);

    if (msg) {
	msgConfirm("%s", msg);
	return DITEM_SUCCESS;
    }

    msgDebug("_msgConfirm: No message passed in `command' variable.\n");
    return DITEM_FAILURE;
}

static int
call_possible_resword(char *name, dialogMenuItem *value, int *status)
{
    int i, rval;

    rval = 0;
    for (i = 0; resWords[i].name; i++) {
	if (!strcmp(name, resWords[i].name)) {
	    *status = resWords[i].handler(value);
	    rval = 1;
	    break;
	}
    }
    return rval;
}

/* For a given string, call it or spit out an undefined command diagnostic */
int
dispatchCommand(char *str)
{
    int i;
    char *cp;

    if (!str || !*str) {
	msgConfirm("Null or zero-length string passed to dispatchCommand");
	return DITEM_FAILURE;
    }
    /* If it's got a newline, trim it */
    if ((cp = index(str, '\n')) != NULL)
	*cp = '\0';

    /* If it's got a `=' sign in there, assume it's a variable setting */
    if (index(str, '=')) {
	if (isDebug())
	    msgDebug("dispatch: setting variable `%s'\n", str);
	variable_set(str, 0);
	i = DITEM_SUCCESS;
    }
    else {
	/* A command might be a pathname if it's encoded in argv[0], which
	   we also support */
	if ((cp = rindex(str, '/')) != NULL)
	    str = cp + 1;
	if (isDebug())
	    msgDebug("dispatch: calling resword `%s'\n", str);
	if (!call_possible_resword(str, NULL, &i)) {
	    msgNotify("Warning: No such command ``%s''", str);
	    i = DITEM_FAILURE;
	}
	/*
	 * Allow a user to prefix a command with "noError" to cause
	 * us to ignore any errors for that one command.
	 */
	if (i != DITEM_SUCCESS && variable_get(VAR_NO_ERROR))
	    i = DITEM_SUCCESS;
	variable_unset(VAR_NO_ERROR);
    }
    return i;
}

