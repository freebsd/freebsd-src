/*-
 * Copyright (c) 2011 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * Copyright (c) 2014 Pedro Souza <pedrosouza@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <string.h>
#include "bootstrap.h"
#include "interp.h"

struct interp_simple_softc {
	int	dummy;
};

static void
interp_simple_init(void *ctx)
{

	(void)ctx; /* Silent the compiler */
}

static int
interp_simple_run(void *ctx, const char *input)
{
	struct interp_simple_softc *softc;
	int argc;
	char **argv;

	softc = ctx;
	(void)softc;	/* Currently unused */

	if (!parse(&argc, &argv, input)) {
		if (perform(argc, argv))
			printf("%s: %s\n", argv[0], command_errmsg);
		free(argv);
	} else {
		printf("parse error\n");
	}
	return 0;
}

static int
interp_simple_incl(void *ctx, const char *filename)
{
	struct includeline	*script, *se, *sp;
	char		input[256];			/* big enough? */
	int			argc,res;
	char		**argv, *cp;
	int			fd, flags, line;

	(void)ctx; /* Silent the compiler */

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
		/* Allocate script line structure and copy line, flags */
		if (*cp == '\0')
			continue;	/* ignore empty line, save memory */
		sp = malloc(sizeof(struct includeline) + strlen(cp) + 1);
		/* On malloc failure (it happens!), free as much as possible and exit */
		if (sp == NULL) {
			while (script != NULL) {
				se = script;
				script = script->next;
				free(se);
			}
			sprintf(command_errbuf, "file '%s' line %d: memory allocation "
			    "failure - aborting\n", filename, line);
			return (CMD_ERROR);
		}
		strcpy(sp->text, cp);
		sp->flags = flags;
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
	argv = NULL;
	res = CMD_OK;
	for (sp = script; sp != NULL; sp = sp->next) {

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
	}
	if (argv != NULL)
		free(argv);
	while(script != NULL) {
		se = script;
		script = script->next;
		free(se);
	}
	return(res);
}

struct interp	boot_interp_simple = {
	.init = interp_simple_init,
	.run = interp_simple_run,
	.incl = interp_simple_incl,
	.load_configs = default_load_config,
	.context = NULL
};
