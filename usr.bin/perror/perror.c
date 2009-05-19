/*-
 * Copyright (c) 2009 Advanced Computing Technologies LLC
 * Written by: George V. Neville-Neil <gnn@FreeBSD.org>
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
#include <sys/errno.h>

#define MAX_ERR 256

static void 
usage()
{

	fprintf(stderr, "usage: perror number\n");
	fprintf(stderr, "number must be between 1 and %d\n", ELAST);
	exit(1);
}

int 
main(int argc, char **argv)
{

	char errstr[MAX_ERR];
	char *cp;
	int errnum;

	if (argc != 2)
		usage();

	errnum = strtol(argv[1], &cp, 0);

	if (((errnum == 0) && (errno == EINVAL)) || (*cp != '\0')) {
		fprintf(stderr, "Argument %s not a number.\n", argv[1]);
		usage();
	}

	if ((errnum <=0) || (errnum > ELAST)) {
		fprintf(stderr, "Number %d out of range.\n", errnum);
		usage();
	}
		
	if (strerror_r(errnum, errstr, sizeof(errstr)) < 0) {
		fprintf(stderr, "Could not find error number %d.\n", errnum);
		usage();
	}

	printf("Error %d is \"%s\"\n", errnum, errstr);

	exit(0);
}
