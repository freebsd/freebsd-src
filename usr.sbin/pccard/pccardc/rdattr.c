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
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <pccard/cardinfo.h>

int
rdattr_main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     i, reg, length;
	char    name[64];
	u_char *buf;
	int     fd;
	off_t   offs;

	if (argc != 4)
		errx(1, "Usage: %s rdattr slot offs length", argv[0]);

	sprintf(name, CARD_DEVICE, atoi(argv[1]));
	fd = open(name, O_RDONLY);
	if (fd < 0)
		err(1, "%s", name);

	reg = MDF_ATTR;
	if (ioctl(fd, PIOCRWFLAG, &reg))
		err(1, "ioctl (PIOCRWFLAG)");

	if (sscanf(argv[2], "%x", &reg) != 1 ||
	    sscanf(argv[3], "%x", &length) != 1)
		errx(1, "arg error");

	offs = reg;
	if ((buf = malloc(length)) == 0)
		errx(1, "malloc failed");

	lseek(fd, offs, SEEK_SET);
	if (read(fd, buf, length) != length)
		err(1, "%s", name);

	for (i = 0; i < length; i++) {
		if (i % 16 == 0) {
			printf("%04x: ", (int) offs + i);
		}
		printf("%02x ", buf[i]);
		if (i % 16 == 15) {
			printf("\n");
		}
	}
	if (i % 16 != 0) {
		printf("\n");
	}
	return 0;
}
