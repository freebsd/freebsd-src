/*
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
 *	$Id: interp_forth.c,v 1.7 1998/11/07 06:18:00 jkh Exp $
 */

#include <stand.h>
#include "bootstrap.h"
#include "ficl.h"

/* #define BFORTH_DEBUG */

#ifdef BFORTH_DEBUG
# define DEBUG(fmt, args...)	printf("%s: " fmt "\n" , __FUNCTION__ , ## args)
#else
# define DEBUG(fmt, args...)
#endif

/*
 * BootForth   Interface to Ficl Forth interpreter.
 */

FICL_VM	*bf_vm;

/*
 * Shim for taking commands from BF and passing them out to 'standard'
 * argv/argc command functions.
 */
static void
bf_command(FICL_VM *vm)
{
    char			*name, *line, *tail, *cp;
    int				len;
    struct bootblk_command	**cmdp;
    bootblk_cmd_t		*cmd;
    int				argc, result;
    char			**argv;

    /* Get the name of the current word */
    name = vm->runningWord->name;
    
    /* Find our command structure */
    cmd == NULL;
    SET_FOREACH(cmdp, Xcommand_set) {
	if (((*cmdp)->c_name != NULL) && !strcmp(name, (*cmdp)->c_name))
	    cmd = (*cmdp)->c_fn;
    }
    if (cmd == NULL)
	panic("callout for unknown command '%s'", name);
    
    /* Get remainder of invocation */
    tail = vmGetInBuf(vm);
    for (cp = tail, len = 0; *cp != 0 && *cp != '\n'; cp++, len++)
	;
    
    line = malloc(strlen(name) + len + 2);
    strcpy(line, name);
    if (len > 0) {
	strcat(line, " ");
	strncat(line, tail, len);
	vmUpdateTib(vm, tail + len);
    }
    DEBUG("cmd '%s'", line);
    
    command_errmsg = command_errbuf;
    command_errbuf[0] = 0;
    if (!parse(&argc, &argv, line)) {
	result = (cmd)(argc, argv);
	free(argv);
	if(result != 0) {
		vmTextOut(vm,argv[0],0);
		vmTextOut(vm,": ",0);
		vmTextOut(vm,command_errmsg,1);
	}
    } else {
	vmTextOut(vm, "parse error\n", 1);
	result=1;
    }
    free(line);
    stackPushINT32(vm->pStack,!result);
}

/*
 * Initialise the Forth interpreter, create all our commands as words.
 */
void
bf_init(void)
{
    struct bootblk_command	**cmdp;
    int fd;
   
    ficlInitSystem(4000);	/* Default dictionary ~4000 cells */
    bf_vm = ficlNewVM();

    /* make all commands appear as Forth words */
    SET_FOREACH(cmdp, Xcommand_set)
	ficlBuild((*cmdp)->c_name, bf_command, FW_DEFAULT);

    /* try to load and run init file if present */
    if ((fd = open("/boot/boot.4th", O_RDONLY)) != -1) {
	(void)ficlExecFD(bf_vm, fd);
	close(fd);
    }
}

/*
 * Feed a line of user input to the Forth interpreter
 */
void
bf_run(char *line)
{
    int		result;
    
    result = ficlExec(bf_vm, line);
    DEBUG("ficlExec '%s' = %d", line, result);
    
    if (result == VM_USEREXIT)
	panic("interpreter exit");
}
