/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: command.c,v 1.14 1996/04/13 13:31:24 jkh Exp $
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

#define MAX_NUM_COMMANDS	10

typedef struct {
    char key[FILENAME_MAX];
    struct {
	enum { CMD_SHELL, CMD_FUNCTION } type;
	void *ptr, *data;
    } cmds[MAX_NUM_COMMANDS];
    int ncmds;
} Command;

#define MAX_CMDS	200
static Command *commandStack[MAX_CMDS];
int numCommands;

/* Nuke the command stack */
void
command_clear(void)
{
    int i, j;

    for (i = 0; i < numCommands; i++)
	for (j = 0; j < commandStack[i]->ncmds; j++)
	    if (commandStack[i]->cmds[j].type == CMD_SHELL)
		free(commandStack[i]->cmds[j].ptr);
    free(commandStack[i]);
    numCommands = 0;
}

static void
addit(char *key, int type, void *cmd, void *data)
{
    int i;

    /* First, look for the key already present and add a command to it if found */
    for (i = 0; i < numCommands; i++) {
	if (!strcmp(commandStack[i]->key, key)) {
	    if (commandStack[i]->ncmds == MAX_NUM_COMMANDS)
		msgFatal("More than %d commands stacked up behind %s??", MAX_NUM_COMMANDS, key);
	    commandStack[i]->cmds[commandStack[i]->ncmds].type = type;
	    commandStack[i]->cmds[commandStack[i]->ncmds].ptr = cmd;
	    commandStack[i]->cmds[commandStack[i]->ncmds].data = data;
	    ++(commandStack[i]->ncmds);
	    return;
	}
    }
    if (numCommands == MAX_CMDS)
	msgFatal("More than %d commands accumulated??", MAX_CMDS);

    /* If we fell to here, it's a new key */
    commandStack[numCommands] = safe_malloc(sizeof(Command));
    strcpy(commandStack[numCommands]->key, key);
    commandStack[numCommands]->ncmds = 1;
    commandStack[numCommands]->cmds[0].type = type;
    commandStack[numCommands]->cmds[0].ptr = cmd;
    commandStack[numCommands]->cmds[0].data = data;
    ++numCommands;
}

/* Add a shell command under a given key */
void
command_shell_add(char *key, char *fmt, ...)
{
    va_list args;
    char *cmd;

    cmd = (char *)safe_malloc(256);
    va_start(args, fmt);
    vsnprintf(cmd, 256, fmt, args);
    va_end(args);

    addit(key, CMD_SHELL, cmd, NULL);
}

/* Add a shell command under a given key */
void
command_func_add(char *key, commandFunc func, void *data)
{
    addit(key, CMD_FUNCTION, func, data);
}

static int
sort_compare(Command *p1, Command *p2)
{
    if (!p1 && !p2)
	return 0;
    else if (!p1 && p2)	/* NULL has a "greater" value for commands */
	return 1;
    else if (p1 && !p2)
	return -1;
    else
	return strcmp(p1->key, p2->key);
}

void
command_sort(void)
{
    int i, j;

    commandStack[numCommands] = NULL;
    /* Just do a crude bubble sort since the list is small */
    for (i = 0; i < numCommands; i++) {
	for (j = 0; j < numCommands; j++) {
	    if (sort_compare(commandStack[j], commandStack[j + 1]) > 0) {
		Command *tmp = commandStack[j];

		commandStack[j] = commandStack[j + 1];
		commandStack[j + 1] = tmp;
	    }
	}
    }
}

/* Run all accumulated commands in sorted order */
void
command_execute(void)
{
    int i, j, ret;
    commandFunc func;

    for (i = 0; i < numCommands; i++) {
	for (j = 0; j < commandStack[i]->ncmds; j++) {
	    /* If it's a shell command, run system on it */
	    if (commandStack[i]->cmds[j].type == CMD_SHELL) {
		msgNotify("Doing %s", commandStack[i]->cmds[j].ptr);
		ret = vsystem((char *)commandStack[i]->cmds[j].ptr);
		if (isDebug())
		    msgDebug("Command `%s' returns status %d\n", commandStack[i]->cmds[j].ptr, ret);
	    }
	    else {
		/* It's a function pointer - call it with the key and the data */
		func = (commandFunc)commandStack[i]->cmds[j].ptr;
		if (isDebug())
		    msgDebug("%x: Execute(%s, %s)", func, commandStack[i]->key, commandStack[i]->cmds[j].data);
		ret = (*func)(commandStack[i]->key, commandStack[i]->cmds[j].data);
		if (isDebug())
		    msgDebug("Function @ %x returns status %d\n", commandStack[i]->cmds[j].ptr, ret);
	    }
	}
    }
}
