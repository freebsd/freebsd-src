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
/*
 * Simple commandline interpreter.
 *
 * XXX may be obsoleted by BootFORTH
 */

#include <stand.h>
#include <string.h>
#include "bootstrap.h"

#define	MAXARGS	20			/* maximum number of arguments allowed */

static int	parse(char *buf, int *argcp, char **argvp[]);
static void	prompt(void);
/*
 * Parse the supplied text into argv/argc form.
 * XXX should perhaps learn about quotes, etc?
 * XXX can also do alias expansion, variable substitution, etc. here
 */
static int
parse(char *buf, int *argcp, char **argvp[])
{
    static int		argc;
    static char		*argv[MAXARGS], *cp;

    argc = 0;
    cp = buf;
    while ((*cp != 0) && (argc < MAXARGS)) {
	if (isspace(*cp)) {
	    *(cp++) = 0;
	} else {
	    argv[argc++] = cp++;
	    while ((*cp != 0) && !isspace(*cp))
		cp++;
	}
    }
    argv[argc] = NULL;
    /* command too complex */
    if (argc >= MAXARGS) {
	printf("too many arguments\n");
	return(1);
    }
    *argcp = argc;
    *argvp = argv;
    return(0);
}

/*
 * Perform the command
 */
static int
perform(int argc, char *argv[])
{
    int				i, result;
    struct bootblk_command	**cmdp;
    bootblk_cmd_t		*cmd;

    if (argc < 1)
	return(CMD_OK);

    /* set return defaults; a successful command will override these */
    command_errmsg = command_errbuf;
    strcpy(command_errbuf, "no error message");
    cmd = NULL;
    result = CMD_ERROR;

    cmdp = (struct bootblk_command **)Xcommand_set.ls_items;
    for (i = 0; i < Xcommand_set.ls_length; i++) {
	if ((cmdp[i]->c_name != NULL) && !strcmp(argv[0], cmdp[i]->c_name))
	    cmd = cmdp[i]->c_fn;
    }
    if (cmd != NULL) {
	result = (cmd)(argc, argv);
    } else {
	command_errmsg = "unknown command";
    }
    return(result);
}

/*
 * Interactive mode
 */
void
interact(void)
{
    char	input[256];			/* big enough? */
    int		argc;
    char	**argv;

    for (;;) {
	input[0] = '\0';
	prompt();
	ngets(input, sizeof(input));
	if (!parse(input, &argc, &argv) && 
	    (perform(argc, argv) != 0))
		printf("%s: %s\n", argv[0], command_errmsg);
    }
}

/*
 * Read command from a file
 */
COMMAND_SET(source, "source", "read commands from a file", command_source);

static int
command_source(int argc, char *argv[])
{
    int		i;

    for (i = 1; i < argc; i++)
	source(argv[i]);
    return(CMD_OK);
}

void
source(char *filename)
{
    char	input[256];			/* big enough? */
    int		argc;
    char	**argv, *cp;
    int		fd;

    if (((fd = open(filename, O_RDONLY)) == -1)) {
	printf("can't open '%s': %s\n", filename, strerror(errno));
    } else {
	while (fgetstr(input, sizeof(input), fd) > 0) {

	    /* Discard comments */
	    if (input[0] == '#')
		continue;
	    cp = input;
	    /* Echo? */
	    if (input[0] == '@') {
		cp++;
	    } else {
		prompt();
		printf("%s\n", input);
	    }

	    if (!parse(cp, &argc, &argv) && 
		(argc > 0) &&
		(perform(argc, argv) != 0))
		    printf("%s: %s\n", argv[0], command_errmsg);
	}
	close(fd);
    }
}

/*
 * Emit the current prompt; support primitive embedding of
 * environment variables.
 * We're a little rude here, modifying the return from getenv().
 */
static void
prompt(void) 
{
    char	*p, *cp, *ev, c;
    
    if ((p = getenv("prompt")) == NULL)
	p = ">";

    while (*p != 0) {
	if (*p == '$') {
	    for (cp = p + 1; (*cp != 0) && isalpha(*cp); cp++)
		;
	    c = *cp;
	    *cp = 0;
	    ev = getenv(p + 1);
	    *cp = c;
	    
	    if (ev != NULL) {
		printf(ev);
		p = cp;
		continue;
	    }
	}
	putchar(*p++);
    }
    putchar(' ');
}
