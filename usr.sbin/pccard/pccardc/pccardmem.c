/*
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $Id: pccardmem.c,v 1.4 1996/04/18 04:24:54 nate Exp $
 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <pccard/card.h>

int
pccardmem_main(argc, argv)
	int     argc;
	char   *argv[];
{
	char    name[64];
	int     addr = 0;
	int     fd;

	if (argc > 2) {
		fprintf(stderr, "usage: %s [ memory-address ]\n", argv[0]);
		exit(1);
	}
	sprintf(name, CARD_DEVICE, 0);
	fd = open(name, 0);
	if (fd < 0) {
		perror(name);
		exit(1);
	}
	if (argc == 2) {
		if (sscanf(argv[1], "%x", &addr) != 1) {
			fprintf(stderr, "arg error\n");
			exit(1);
		}
	}
	if (ioctl(fd, PIOCRWMEM, &addr))
		perror("ioctl");
	else
		printf("PCCARD Memory address set to 0x%x\n", addr);
	exit(0);
}
