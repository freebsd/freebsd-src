/*
 * Copyright (c) 1983 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

#include "externs.h"

/*ARGSUSED*/
main(argc, argv)
	int argc;
	register char **argv;
{
	register char *p;
	int i;

	(void) srand(getpid());
	issetuid = getuid() != geteuid();
	if (p = rindex(*argv, '/'))
		p++;
	else
		p = *argv;
	if (strcmp(p, "driver") == 0 || strcmp(p, "saildriver") == 0)
		mode = MODE_DRIVER;
	else if (strcmp(p, "sail.log") == 0)
		mode = MODE_LOGGER;
	else
		mode = MODE_PLAYER;
	while ((p = *++argv) && *p == '-')
		switch (p[1]) {
		case 'd':
			mode = MODE_DRIVER;
			break;
		case 's':
			mode = MODE_LOGGER;
			break;
		case 'D':
			debug++;
			break;
		case 'x':
			randomize;
			break;
		case 'l':
			longfmt++;
			break;
		case 'b':
			nobells++;
			break;
		default:
			fprintf(stderr, "SAIL: Unknown flag %s.\n", p);
			exit(1);
		}
	if (*argv)
		game = atoi(*argv);
	else
		game = -1;
	if (i = setjmp(restart))
		mode = i;
	switch (mode) {
	case MODE_PLAYER:
		return pl_main();
	case MODE_DRIVER:
		return dr_main();
	case MODE_LOGGER:
		return lo_main();
	default:
		fprintf(stderr, "SAIL: Unknown mode %d.\n", mode);
		abort();
	}
	/*NOTREACHED*/
}
