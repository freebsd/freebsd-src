/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: command.c,v 1.1 1995/05/08 06:08:27 jkh Exp $
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

#define MAX_NUM_COMMANDS	10

typedef struct {
    char key[FILENAME_MAX];
    char *cmds[MAX_NUM_COMMANDS];
    int ncmds;
} Command;

#define MAX_CMDS	200
static Command *commandStack[MAX_CMDS];
int numCommands;

void
command_clear(void)
{
    int i, j;

    for (i = 0; i < numCommands; i++)
	for (j = 0; j < commandStack[i]->ncmds; j++)
	    free(commandStack[i]->cmds[j]);
    free(commandStack[i]);
    numCommands = 0;
}

void
command_add(char *key, char *fmt, ...)
{
    va_list args;
    char *cmd;
    int i;

    cmd = (char *)safe_malloc(1024);
    va_start(args, fmt);
    vsnprintf(cmd, 1024, fmt, args);
    va_end(args);

    /* First, look for the key already present and add a command to it */
    for (i = 0; i < numCommands; i++) {
	if (!strcmp(commandStack[i]->key, key)) {
	    commandStack[i]->cmds[commandStack[i]->ncmds++] = cmd;
	    if (commandStack[i]->ncmds == MAX_NUM_COMMANDS)
		msgFatal("More than %d commands stacked up behind %s??",
			 MAX_NUM_COMMANDS, key);
	    return;
	}
    }
    /* If we fell to here, it's a new key */
    commandStack[numCommands] = safe_malloc(sizeof(Command));
    strcpy(commandStack[numCommands]->key, key);
    commandStack[numCommands]->ncmds = 1;
    commandStack[numCommands++]->cmds[0] = cmd;
    if (numCommands == MAX_CMDS)
	msgFatal("More than %d commands accumulated??", MAX_CMDS);
}

static int
sort_compare(const void *p1, const void *p2)
{
    return strcmp(((Command *)p1)->key, ((Command *)p2)->key);
}

void
command_sort(void)
{
    qsort(commandStack, numCommands, sizeof(Command *), sort_compare);
}

void
command_execute(void)
{
    int i, j, ret;

    for (i = 0; i < numCommands; i++) {
	for (j = 0; j < commandStack[i]->ncmds; j++) {
	    msgNotify("Executing command: %s", commandStack[i]->cmds[j]);
	    ret = system(commandStack[i]->cmds[j]);
	    msgDebug("Command: %s returns status %d\n",
		     commandStack[i]->cmds[j], ret);
	}
    }
}
