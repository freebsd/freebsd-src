/*-
 * Copyright (c) 2007 Robert N. M. Watson
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
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "ddb.h"

void
usage(void)
{

	fprintf(stderr, "usage:\n");
	fprintf(stderr, "ddb script scriptname\n");
	fprintf(stderr, "ddb script scriptname=script\n");
	fprintf(stderr, "ddb scripts\n");
	fprintf(stderr, "ddb unscript scriptname\n");
	exit(EX_USAGE);
}

int
main(int argc, char *argv[])
{

	if (argc < 2)
		usage();

	argc -= 1;
	argv += 1;
	if (strcmp(argv[0], "script") == 0)
		ddb_script(argc, argv);
	else if (strcmp(argv[0], "scripts") == 0)
		ddb_scripts(argc, argv);
	else if (strcmp(argv[0], "unscript") == 0)
		ddb_unscript(argc, argv);
	else
		usage();
	exit(EX_OK);
}
