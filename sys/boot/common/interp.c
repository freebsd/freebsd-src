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
 *	$Id: interp.c,v 1.3 1998/09/03 02:10:07 msmith Exp $
 */
/*
 * Simple commandline interpreter, toplevel and misc.
 *
 * XXX may be obsoleted by BootFORTH
 */

#include <stand.h>
#include <string.h>
#include "bootstrap.h"

#define	MAXARGS	20			/* maximum number of arguments allowed */

static void	prompt(void);

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

    /*
     * Read our default configuration
     */
    source("/boot/boot.conf");
    printf("\n");
    /*
     * Before interacting, we might want to autoboot
     */
    if (getenv("no_autoboot") == NULL)
	autoboot(10, NULL);		/* try to boot automatically */

    /*
     * Not autobooting, go manual
     */
    printf("\nType '?' for a list of commands, 'help' for more detailed help.\n");
    setenv("prompt", "${currdev}>", 1);
    

    for (;;) {
	input[0] = '\0';
	prompt();
	ngets(input, sizeof(input));
	if (!parse(&argc, &argv, input)) {
	    if (perform(argc, argv))
		printf("%s: %s\n", argv[0], command_errmsg);
	    free(argv);
	} else {
	    printf("parse error\n");
	}
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

	    if (!parse(&argc, &argv, cp)) {
		if ((argc > 0) &&
		    (perform(argc, argv) != 0))
		    printf("%s: %s\n", argv[0], command_errmsg);
		free(argv);
	    }
	}
	close(fd);
    }
}

/*
 * Emit the current prompt; use the same syntax as the parser
 * for embedding environment variables.
 */
static void
prompt(void) 
{
    char	*p, *cp, *ev;
    
    if ((cp = getenv("prompt")) == NULL)
	cp = ">";
    p = strdup(cp);

    while (*p != 0) {
	if ((*p == '$') && (*(p+1) == '{')) {
	    for (cp = p + 2; (*cp != 0) && (*cp != '}'); cp++)
		;
	    *cp = 0;
	    ev = getenv(p + 2);
	    
	    if (ev != NULL)
		printf(ev);
	    p = cp + 1;
	    continue;
	}
	putchar(*p++);
    }
    putchar(' ');
}
