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

/*
 * Simple commandline interpreter, toplevel and misc.
 *
 * XXX may be obsoleted by BootFORTH or some other, better, interpreter.
 */

#include <stand.h>
#include <string.h>
#include "bootstrap.h"

#ifdef LOADER_VERIEXEC
#include <verify_file.h>
#endif

#define	MAXARGS	20			/* maximum number of arguments allowed */

const char * volatile	interp_identifier;

/*
 * Interactive mode
 */
void
interact(void)
{
	static char		input[256];		/* big enough? */

	TSENTER();

	/*
	 * Because interp_identifier is volatile, it cannot be optimized out by
	 * the compiler as it's considered an externally observable event.  This
	 * prevents the compiler from optimizing out our carefully placed
	 * $Interpreter:4th string that userboot may use to determine that
	 * we need to switch interpreters.
	 */
	interp_identifier = bootprog_interp;
	interp_init();

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
		interp_emit_prompt();
		ngets(input, sizeof(input));
#ifdef LOADER_VERIEXEC
		/* some settings should be restritcted */
		ve_status_set(-1, VE_UNVERIFIED_OK);
#endif
		interp_run(input);
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
	char		**argvbuf;

	/*
	 * Since argv is static, we need to save it here.
	 */
	argvbuf = (char**) calloc((u_int)argc, sizeof(char*));
	for (i = 0; i < argc; i++)
		argvbuf[i] = strdup(argv[i]);

	res=CMD_OK;
	for (i = 1; (i < argc) && (res == CMD_OK); i++)
		res = interp_include(argvbuf[i]);

	for (i = 0; i < argc; i++)
		free(argvbuf[i]);
	free(argvbuf);

	return(res);
}

/*
 * Emit the current prompt; use the same syntax as the parser
 * for embedding environment variables. Does not accept input.
 */
void
interp_emit_prompt(void)
{
	char		*pr, *p, *cp, *ev;

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

static struct bootblk_command *
interp_lookup_cmd(const char *cmd)
{
	struct bootblk_command	**cmdp;

	/* search the command set for the command */
	SET_FOREACH(cmdp, Xcommand_set) {
		if (((*cmdp)->c_name != NULL) && !strcmp(cmd, (*cmdp)->c_name))
			return (*cmdp);
	}
	return (NULL);
}

/*
 * Perform a builtin command
 */
int
interp_builtin_cmd(int argc, char *argv[])
{
	int			result;
	struct bootblk_command	*cmd;

	if (argc < 1)
		return (CMD_OK);

	/* set return defaults; a successful command will override these */
	command_errmsg = command_errbuf;
	strcpy(command_errbuf, "no error message");
	result = CMD_ERROR;

	cmd = interp_lookup_cmd(argv[0]);
	if (cmd != NULL && cmd->c_fn) {
		TSENTER2(argv[0]);
		result = cmd->c_fn(argc, argv);
		TSEXIT();
	} else {
		command_errmsg = "unknown command";
	}
	return (result);
}

/*
 * Return true if the builtin command exists
 */
bool
interp_has_builtin_cmd(const char *cmd)
{
	return (interp_lookup_cmd(cmd) != NULL);
}
