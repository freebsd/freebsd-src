/*-
 * Copyright (c) 1996 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *  $Id: brandelf.c,v 1.1.2.3 1997/08/23 15:57:35 joerg Exp $
 */

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/imgact_elf.h>

static void usage __P((void));

int
main(int argc, char **argv)
{

	const char *type = "FreeBSD";
	int retval = 0;
	int ch, change = 0, verbose = 0;

	while ((ch = getopt(argc, argv, "t:v")) !=  -1)
		switch (ch) {
		case 'v':
			verbose = 1;
			break;
		case 't':
			change = 1;
			type = optarg;
			break;
		default:
			usage();
	}
	argc -= optind;
	argv += optind;
	if (!argc)
		errx(1, "no file(s) specified");
	while (argc) {
		int fd;
		char buffer[EI_NINDENT];
		char string[(EI_NINDENT-EI_SPARE)+1];

		if ((fd = open(argv[0], change? O_RDWR: O_RDONLY, 0)) < 0) {
			warn("error opening file %s", argv[0]);
			retval = 1;
			goto fail;
			
		}
		if (read(fd, buffer, EI_NINDENT) < EI_NINDENT) {
			warnx("file '%s' too short", argv[0]);
			retval = 1;
			goto fail;
		}
		if (buffer[0] != ELFMAG0 || buffer[1] != ELFMAG1 ||
		    buffer[2] != ELFMAG2 || buffer[3] != ELFMAG3) {
			warnx("file '%s' is not ELF format", argv[0]);
			retval = 1;
			goto fail;
		}		
		if (!change) {
			bzero(string, sizeof(string));
			strncpy(string, &buffer[EI_SPARE], EI_NINDENT-EI_SPARE);
			if (strlen(string)) {
				fprintf(stdout, "File '%s' is of brand '%s'.\n",
					argv[0], string);
			}
			else
				fprintf(stdout, "File '%s' has no branding.\n",
					argv[0]);
		}
		else {
			strncpy(&buffer[EI_SPARE], type, EI_NINDENT-EI_SPARE);
			lseek(fd, 0, SEEK_SET);
			if (write(fd, buffer, EI_NINDENT) != EI_NINDENT) {
				warnx("error writing %s", argv[0]);
				retval = 1;
				goto fail;
			}
		}
fail:
		argc--;
		argv++;
	}

	return retval;
}

static void
usage()
{
	fprintf(stderr, "usage: brandelf [-t string] file ...\n");
	exit(1);
}
