/*-
 * Copyright (c) 1998, Peter Wemm <peter@netplex.com.au>
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
 * $FreeBSD$
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef FREEBSD_ELF
int objformat_aout = 0;
#else
int objformat_aout = 1;
#endif

void
getobjfmt(void)
{
	char *env;
	int i;

	/* first hint is /etc/objectformat */
	FILE *fp = fopen("/etc/objectformat", "r");
	if (fp) {
		char buf[1024];
		buf[1023] = '\0';
		while (fgets(buf, sizeof(buf) - 1, fp) != NULL) {
			i = strlen(buf);
			if (buf[i - 1] == '\n')
				buf[i - 1] = '\0';
			if (strcmp(buf, "OBJFORMAT=aout") == 0)
				objformat_aout = 1;
			else if (strcmp(buf, "OBJFORMAT=elf") == 0)
				objformat_aout = 0;
			else
				fprintf(stderr, "Unrecognized line in /etc/objectformat: %s\n", buf);
		}
		fclose(fp);
	}
	/* but the user $OBJFORMAT overrides system default */
	env = getenv("OBJFORMAT");
	if (env) {
		if (strcmp(env, "aout") == 0)
			objformat_aout = 1;
		else if (strcmp(env, "elf") == 0)
			objformat_aout = 0;
		else
			fprintf(stderr, "Unrecognized value of $OBJFORMAT: %s\n", env);
	}
}

void
scanargv(int *argc, char **argv, int strip)
{
	int i, j;

	for (i = 1; i < *argc; i++) {
		if (strcmp (argv[i], "-aout") == 0) {
			objformat_aout = 1;
			continue;
		} else if (strcmp (argv[i], "-elf") == 0) {
			objformat_aout = 0;
			continue;
		}
	}

	/* if just looking, return now */
	if (!strip)
		return;

	/* otherwise, remove all traces of switches from argv */
	for (i = 1; i < *argc; i++) {
		if (strcmp (argv[i], "-aout") == 0 ||
		    strcmp (argv[i], "-elf") == 0) {
			/* copy NULL at end of argv as well */
			for (j = i + 1; j <= *argc; j++) {
				argv[j - 1] = argv[j];
			}
			(*argc)--;
		}
	}
}


#ifdef MAIN
int
main(int argc, char **argv)
{
	char *path, *chunk;
	char *postfix;
	char *cmd, *newcmd = NULL;
	char *objformat_path;
	int i;

	cmd = strrchr(argv[0], '/');
	if (cmd)
		cmd++;
	else
		cmd = argv[0];

	getobjfmt();
	scanargv(&argc, argv, 1);

	if (strcmp(cmd, "objformat") == 0) {
		if (objformat_aout)
			printf("aout\n");
		else
			printf("elf\n");
		exit(0);
	}

	/* 'make world' glue */
	objformat_path = getenv("OBJFORMAT_PATH");
	if (objformat_path == NULL)
		objformat_path = "/usr/libexec";
	path = strdup(objformat_path);

	if (objformat_aout) {
		putenv("OBJFORMAT=aout");
		postfix = "aout";
	} else {
		putenv("OBJFORMAT=elf");
		postfix = "elf";
	}

	while ((chunk = strsep(&path, ":")) != NULL) {
		if (newcmd != NULL) {
			free(newcmd);
			newcmd = NULL;
		}
		asprintf(&newcmd, "%s/%s/%s", chunk, postfix, cmd);
		if (newcmd == NULL)
			err(1, "cannot allocate memory for new command");

		if (getenv("OBJFORMAT_DEBUG") != NULL) {
			fprintf(stderr, "objformat: %s -> %s\n", cmd, newcmd);
#if 0
			for (i = 1; i < argc; i++) 
				fprintf(stderr, "argv[%d]: %s\n", i, argv[i]);
#endif
		}

		argv[0] = newcmd;
		execv(newcmd, argv);
	}
	err(1, "could not exec %s/%s in %s", postfix, cmd, objformat_path);
}

#endif
