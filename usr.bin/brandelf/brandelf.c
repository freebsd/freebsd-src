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
 *  $Id: brandelf.c,v 1.5 1997/03/29 04:28:17 imp Exp $
 */

#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int usage(void);

int
main(int argc, char **argv)
{

	const char *type = "FreeBSD";
	int retval = 0;
	int ch, change = 0, verbose = 0;

	while ((ch = getopt(argc, argv, "t:v")) != -1)
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
	if (!argc) {
		fprintf(stderr, "No file(s) specified.\n");
		exit(1);
	}
	while (argc) {
		int fd;
		char buffer[EI_NIDENT];
		char string[(EI_NIDENT-EI_BRAND)+1];

		if ((fd = open(argv[0], O_RDWR, 0)) < 0) {
			fprintf(stderr, "No such file %s.\n", argv[0]);
			retval = 1;
			goto fail;
			
		}
		if (read(fd, buffer, EI_NIDENT) < EI_NIDENT) {
			fprintf(stderr, "File '%s' too short.\n", argv[0]);
			retval = 1;
			goto fail;
		}
		if (buffer[0] != ELFMAG0 || buffer[1] != ELFMAG1 ||
		    buffer[2] != ELFMAG2 || buffer[3] != ELFMAG3) {
			fprintf(stderr, "File '%s' is not ELF format.\n",
				argv[0]);
			retval = 1;
			goto fail;
		}		
		if (!change) {
			bzero(string, sizeof(string));
			strncpy(string, &buffer[EI_BRAND], EI_NIDENT-EI_BRAND);
			if (strlen(string)) {
				fprintf(stdout, "File '%s' is of brand '%s'.\n",
					argv[0], string);
			}
			else
				fprintf(stdout, "File '%s' has no branding.\n",
					argv[0]);
		}
		else {
			strncpy(&buffer[EI_BRAND], type, EI_NIDENT-EI_BRAND);
			lseek(fd, 0, SEEK_SET);
			if (write(fd, buffer, EI_NIDENT) != EI_NIDENT) {
				fprintf(stderr, "Error writing %s\n", argv[0]);
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

int
usage(void)
{
	fprintf(stderr, "Usage: brandelf [-t string] file ...\n");
	exit(1);
}
