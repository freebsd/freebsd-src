/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 *	$Id: ldd.c,v 1.2 1993/11/09 04:19:27 paul Exp $
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <a.out.h>

static char	*progname;

void
usage()
{
	fprintf(stderr, "Usage: %s <filename> ...\n", progname);
}

int
main(argc, argv)
int	argc;
char	*argv[];
{
	int		rval = 0;
	int		c;
	extern int	optind;

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;

	while ((c = getopt(argc, argv, "")) != EOF) {
		switch (c) {
		default:
			usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc <= 0) {
		usage();
		exit(1);
	}

	/* ld.so magic */
	setenv("LD_TRACE_LOADED_OBJECTS", "", 1);

	while (argc--) {
		int	fd;
		struct exec hdr;
		int	status;

		if ((fd = open(*argv, O_RDONLY, 0)) < 0) {
			perror(*argv);
			rval |= 1;
			argv++;
			continue;
		}
		if (read(fd, &hdr, sizeof hdr) != sizeof hdr ||
					!(N_GETFLAG(hdr) & EX_DYNAMIC)) {
			fprintf(stderr, "%s: not a dynamic executable\n",
						*argv);
			(void)close(fd);
			rval |= 1;
			argv++;
			continue;
		}
		(void)close(fd);

		printf("%s:\n", *argv);
		fflush(stdout);

		switch (fork()) {
		case -1:
			perror("fork");
			exit(1);
			break;
		default:
			if (wait(&status) <= 0)
				perror("wait");

			if (WIFSIGNALED(status)) {
				fprintf(stderr, "%s: signal %d\n",
						*argv, WTERMSIG(status));
				rval |= 1;
			} else if (WIFEXITED(status) && WEXITSTATUS(status)) {
				fprintf(stderr, "%s: exit status %d\n",
						*argv, WEXITSTATUS(status));
				rval |= 1;
			}
			break;
		case 0:
			rval != execl(*argv, *argv, NULL) != 0;
			perror(*argv);
			_exit(1);
		}
		argv++;
	}

	return rval;
}
