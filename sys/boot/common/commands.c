/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id$
 */

#include <stand.h>
#include <string.h>
#include <sys/reboot.h>

#include "bootstrap.h"

char		*command_errmsg;
char		command_errbuf[256];	/* XXX should have procedural interface for setting, size limit? */
    
COMMAND_SET(help, "help", "detailed help", command_help);

static int
command_help(int argc, char *argv[])
{
    char	helppath[80];	/* XXX buffer size? */

    /* page the help text from our load path */
    sprintf(helppath, "%s/boot.help", getenv("loaddev"));
    printf("%s\n", helppath);
    if (pager_file(helppath) == -1)
	printf("Verbose help not available, use '?' to list commands\n");
    return(CMD_OK);
}

COMMAND_SET(commandlist, "?", "list commands", command_commandlist);

static int
command_commandlist(int argc, char *argv[])
{
    struct bootblk_command	**cmdp;
    int				i;
    
    printf("Available commands:\n");
    cmdp = (struct bootblk_command **)Xcommand_set.ls_items;
    for (i = 0; i < Xcommand_set.ls_length; i++)
	if (cmdp[i]->c_name != NULL)
	    printf("  %-15s  %s\n", cmdp[i]->c_name, cmdp[i]->c_desc);
    return(CMD_OK);
}

/*
 * XXX set/show should become set/echo if we have variable
 * substitution happening.
 */

COMMAND_SET(show, "show", "show variable(s)", command_show);

static int
command_show(int argc, char *argv[])
{
    struct env_var	*ev;
    char		*cp;

    if (argc < 2) {
	/* 
	 * With no arguments, print everything.
	 */
	pager_open();
	for (ev = environ; ev != NULL; ev = ev->ev_next) {
	    pager_output(ev->ev_name);
	    cp = getenv(ev->ev_name);
	    if (cp != NULL) {
		pager_output("=");
		pager_output(cp);
	    }
	    pager_output("\n");
	}
	pager_close();
    } else {
	if ((cp = getenv(argv[1])) != NULL) {
	    printf("%s\n", cp);
	} else {
	    sprintf(command_errbuf, "variable '%s' not found", argv[1]);
	    return(CMD_ERROR);
	}
    }
    return(CMD_OK);
}

COMMAND_SET(set, "set", "set a variable", command_set);

static int
command_set(int argc, char *argv[])
{
    int		err;
    
    if (argc != 2) {
	command_errmsg = "wrong number of arguments";
	return(CMD_ERROR);
    } else {
	if ((err = putenv(argv[1])) != 0) {
	    command_errmsg = strerror(err);
	    return(CMD_ERROR);
	}
    }
    return(CMD_OK);
}

COMMAND_SET(unset, "unset", "unset a variable", command_unset);

static int
command_unset(int argc, char *argv[]) 
{
    int		err;
    
    if (argc != 2) {
	command_errmsg = "wrong number of arguments";
	return(CMD_ERROR);
    } else {
	if ((err = unsetenv(argv[1])) != 0) {
	    command_errmsg = strerror(err);
	    return(CMD_ERROR);
	}
    }
    return(CMD_OK);
}

COMMAND_SET(panic, "panic", "test panic", command_panic);

static int
command_panic(int argc, char *argv[])
{
    char	*cp;
    
    cp = unargv(argc - 1, argv + 1);
    panic(cp);
}
