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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/common/interp.c,v 1.29 2003/08/25 23:30:41 obrien Exp $");

/*
 * Simple commandline interpreter, toplevel and misc.
 *
 * XXX may be obsoleted by BootFORTH or some other, better, interpreter.
 */

#include <stand.h>
#include <string.h>
#include "bootstrap.h"

#ifdef BOOT_FORTH
#include "ficl.h"
#define	RETURN(x)	stackPushINT(bf_vm->pStack,!x); return(x)

extern FICL_VM *bf_vm;
#else
#define	RETURN(x)	return(x)
#endif

#define	MAXARGS	20			/* maximum number of arguments allowed */

static void	prompt(void);

#ifndef BOOT_FORTH
static int	perform(int argc, char *argv[]);

/*
 * Perform the command
 */
int
perform(int argc, char *argv[])
{
    int				result;
    struct bootblk_command	**cmdp;
    bootblk_cmd_t		*cmd;

    if (argc < 1)
	return(CMD_OK);

    /* set return defaults; a successful command will override these */
    command_errmsg = command_errbuf;
    strcpy(command_errbuf, "no error message");
    cmd = NULL;
    result = CMD_ERROR;

    /* search the command set for the command */
    SET_FOREACH(cmdp, Xcommand_set) {
	if (((*cmdp)->c_name != NULL) && !strcmp(argv[0], (*cmdp)->c_name))
	    cmd = (*cmdp)->c_fn;
    }
    if (cmd != NULL) {
	result = (cmd)(argc, argv);
    } else {
	command_errmsg = "unknown command";
    }
    RETURN(result);
}
#endif	/* ! BOOT_FORTH */

/*
 * Interactive mode
 */
void
interact(void)
{
    char	input[256];			/* big enough? */
#ifndef BOOT_FORTH
    int		argc;
    char	**argv;
#endif

#ifdef BOOT_FORTH
    bf_init();
#endif

    /*
     * Read our default configuration
     */
    if(include("/boot/loader.rc")!=CMD_OK)
	include("/boot/boot.conf");
    printf("\n");
    /*
     * Before interacting, we might want to autoboot.
     */
    autoboot_maybe();
    
    /*
     * Not autobooting, go manual
     */
    printf("\nType '?' for a list of commands, 'help' for more detailed help.\n");
    if (getenv("prompt") == NULL)
	setenv("prompt", "${interpret}", 1);
    if (getenv("interpret") == NULL)
        setenv("interpret", "OK", 1);
    

    for (;;) {
	input[0] = '\0';
	prompt();
	ngets(input, sizeof(input));
#ifdef BOOT_FORTH
	bf_vm->sourceID.i = 0;
	bf_run(input);
#else
	if (!parse(&argc, &argv, input)) {
	    if (perform(argc, argv))
		printf("%s: %s\n", argv[0], command_errmsg);
	    free(argv);
	} else {
	    printf("parse error\n");
	}
#endif
    }
}

/*
 * Read commands from a file, then execute them.
 *
 * We store the commands in memory and close the source file so that the media
 * holding it can safely go away while we are executing.
 *
 * Commands may be prefixed with '@' (so they aren't displayed) or '-' (so
 * that the script won't stop if they fail).
 */
COMMAND_SET(include, "include", "read commands from a file", command_include);

static int
command_include(int argc, char *argv[])
{
    int		i;
    int		res;
    char	**argvbuf;

    /* 
     * Since argv is static, we need to save it here.
     */
    argvbuf = (char**) calloc((u_int)argc, sizeof(char*));
    for (i = 0; i < argc; i++)
	argvbuf[i] = strdup(argv[i]);

    res=CMD_OK;
    for (i = 1; (i < argc) && (res == CMD_OK); i++)
	res = include(argvbuf[i]);

    for (i = 0; i < argc; i++)
	free(argvbuf[i]);
    free(argvbuf);

    return(res);
}

struct includeline 
{
    char		*text;
    int			flags;
    int			line;
#define SL_QUIET	(1<<0)
#define SL_IGNOREERR	(1<<1)
    struct includeline	*next;
};

int
include(const char *filename)
{
    struct includeline	*script, *se, *sp;
    char		input[256];			/* big enough? */
#ifdef BOOT_FORTH
    int			res;
    char		*cp;
    int			prevsrcid, fd, line;
#else
    int			argc,res;
    char		**argv, *cp;
    int			fd, flags, line;
#endif

    if (((fd = open(filename, O_RDONLY)) == -1)) {
	sprintf(command_errbuf,"can't open '%s': %s\n", filename, strerror(errno));
	return(CMD_ERROR);
    }

    /*
     * Read the script into memory.
     */
    script = se = NULL;
    line = 0;
	
    while (fgetstr(input, sizeof(input), fd) >= 0) {
	line++;
#ifdef BOOT_FORTH
	cp = input;
#else
	flags = 0;
	/* Discard comments */
	if (strncmp(input+strspn(input, " "), "\\ ", 2) == 0)
	    continue;
	cp = input;
	/* Echo? */
	if (input[0] == '@') {
	    cp++;
	    flags |= SL_QUIET;
	}
	/* Error OK? */
	if (input[0] == '-') {
	    cp++;
	    flags |= SL_IGNOREERR;
	}
#endif
	/* Allocate script line structure and copy line, flags */
	sp = malloc(sizeof(struct includeline) + strlen(cp) + 1);
	sp->text = (char *)sp + sizeof(struct includeline);
	strcpy(sp->text, cp);
#ifndef BOOT_FORTH
	sp->flags = flags;
#endif
	sp->line = line;
	sp->next = NULL;
	    
	if (script == NULL) {
	    script = sp;
	} else {
	    se->next = sp;
	}
	se = sp;
    }
    close(fd);
    
    /*
     * Execute the script
     */
#ifndef BOOT_FORTH
    argv = NULL;
#else
    prevsrcid = bf_vm->sourceID.i;
    bf_vm->sourceID.i = fd;
#endif
    res = CMD_OK;
    for (sp = script; sp != NULL; sp = sp->next) {
	
#ifdef BOOT_FORTH
	res = bf_run(sp->text);
	if (res != VM_OUTOFTEXT) {
		sprintf(command_errbuf, "Error while including %s, in the line:\n%s", filename, sp->text);
		res = CMD_ERROR;
		break;
	} else
		res = CMD_OK;
#else
	/* print if not being quiet */
	if (!(sp->flags & SL_QUIET)) {
	    prompt();
	    printf("%s\n", sp->text);
	}

	/* Parse the command */
	if (!parse(&argc, &argv, sp->text)) {
	    if ((argc > 0) && (perform(argc, argv) != 0)) {
		/* normal command */
		printf("%s: %s\n", argv[0], command_errmsg);
		if (!(sp->flags & SL_IGNOREERR)) {
		    res=CMD_ERROR;
		    break;
		}
	    }
	    free(argv);
	    argv = NULL;
	} else {
	    printf("%s line %d: parse error\n", filename, sp->line);
	    res=CMD_ERROR;
	    break;
	}
#endif
    }
#ifndef BOOT_FORTH
    if (argv != NULL)
	free(argv);
#else
    bf_vm->sourceID.i = prevsrcid;
#endif
    while(script != NULL) {
	se = script;
	script = script->next;
	free(se);
    }
    return(res);
}

/*
 * Emit the current prompt; use the same syntax as the parser
 * for embedding environment variables.
 */
static void
prompt(void) 
{
    char	*pr, *p, *cp, *ev;
    
    if ((cp = getenv("prompt")) == NULL)
	cp = ">";
    pr = p = strdup(cp);

    while (*p != 0) {
	if ((*p == '$') && (*(p+1) == '{')) {
	    for (cp = p + 2; (*cp != 0) && (*cp != '}'); cp++)
		;
	    *cp = 0;
	    ev = getenv(p + 2);
	    
	    if (ev != NULL)
		printf("%s", ev);
	    p = cp + 1;
	    continue;
	}
	putchar(*p++);
    }
    putchar(' ');
    free(pr);
}
