/*-
 * Copyright (c) 2007 Sean C. Farley <scf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


extern char **environ;


static void
dump_environ(void)
{
	char **environPtr;

	for (environPtr = environ; *environPtr != NULL; *environPtr++)
		printf("%s\n", *environPtr);

	return;
}


static void
usage(const char *program)
{
	fprintf(stderr, "Usage:  %s [-DGUcht] [-gu name] [-p name=value] "
	    "[(-S|-s name) value overwrite]\n\n"
	    "Options:\n"
	    "  -D\t\t\t\tDump environ\n"
	    "  -G name\t\t\tgetenv(NULL)\n"
	    "  -S value overwrite\t\tsetenv(NULL, value, overwrite)\n"
	    "  -U\t\t\t\tunsetenv(NULL)\n"
	    "  -c\t\t\t\tClear environ variable\n"
	    "  -g name\t\t\tgetenv(name)\n"
	    "  -h\t\t\t\tHelp\n"
	    "  -p name=value\t\t\tputenv(name=value)\n"
	    "  -s name value overwrite\tsetenv(name, value, overwrite)\n"
	    "  -t\t\t\t\tOutput is suitable for testing (no newlines)\n"
	    "  -u name\t\t\tunsetenv(name)\n",
	    basename(program));

	return;
}


int
main(int argc, char **argv)
{
	char *cleanEnv[] = { NULL };
	char arg;
	const char *eol = "\n";
	const char *value;

	if (argc == 1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	while ((arg = getopt(argc, argv, "DGS:Ucg:hp:s:tu:")) != -1) {
		switch (arg) {
			case 'D':
				errno = 0;
				dump_environ();
				break;

			case 'c':
				environ = cleanEnv;
				break;

			case 'G':
				value = getenv(NULL);
				printf("%s%s", value == NULL ? "" : value, eol);
				break;

			case 'g':
				value = getenv(optarg);
				printf("%s%s", value == NULL ? "" : value, eol);
				break;

			case 'p':
				errno = 0;
				printf("%d %d%s", putenv(optarg), errno, eol);
				break;

			case 'S':
				errno = 0;
				printf("%d %d%s", setenv(NULL, optarg,
				    atoi(argv[optind])), errno, eol);
				optind += 1;
				break;

			case 's':
				errno = 0;
				printf("%d %d%s", setenv(optarg, argv[optind],
				    atoi(argv[optind + 1])), errno, eol);
				optind += 2;
				break;

			case 't':
				eol = " ";
				break;

			case 'U':
				printf("%d %d%s", unsetenv(NULL), errno, eol);
				break;

			case 'u':
				printf("%d %d%s", unsetenv(optarg), errno, eol);
				break;

			case 'h':
			default:
				usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	// Output a closing newline in test mode.
	if (eol[0] == ' ')
		printf("\n");

	return (EXIT_SUCCESS);
}
