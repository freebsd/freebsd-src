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
 * $Id$
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <pccard/card.h>
void
dumpslot(sl)
	int     sl;
{
	char    name[64];
	int     fd;
	struct pcic_reg r;

	sprintf(name, "/dev/card%d", sl);
	fd = open(name, 2);
	if (fd < 0) {
		perror(name);
		return;
	}
	printf("Registers for slot %d\n", sl);
	for (r.reg = 0; r.reg < 0x40; r.reg++) {
		if (ioctl(fd, PIOCGREG, &r)) {
			perror("ioctl");
			break;
		}
		if ((r.reg % 16) == 0)
			printf("%02x:", r.reg);
		printf(" %02x", r.value);
		if ((r.reg % 16) == 15)
			printf("\n");
	}
	close(fd);
}

int
rdreg_main(argc, argv)
	int     argc;
	char   *argv[];
{
	if (argc != 2) {
		dumpslot(0);
		dumpslot(1);
	} else
		dumpslot(atoi(argv[1]));
	return 0;
}
