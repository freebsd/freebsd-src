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

/* $FreeBSD$ */

/*
 * Code cleanup, bug-fix and extension
 * by Tatsumi Hosokawa <hosokawa@mt.cs.keio.ac.jp>
 */

#ifndef lint
static const char rcsid[] =
	"PAO: power.c,v 1.3 1999/02/11 05:00:54 kuriyama Exp";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <pccard/cardinfo.h>

int
power_main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     fd, i, newstat, valid = 1;
	char    name[64], *p;

	if (argc != 3)
		valid = 0;
	for (i = 1; i <= 2; i++) {
		if (valid) {
			for (p = argv[i]; *p; p++) {
				if (!isdigit(*p)) {
					valid = 0;
					break;
				}
			}
		}
	}
	if (!valid)
		errx(1, "Usage: %s power slot newstat", argv[0]);

	sscanf(argv[2], "%d", &newstat);
	sprintf(name, CARD_DEVICE, atoi(argv[1]));
	fd = open(name, O_RDWR);
	if (fd < 0)
		err(1, "%s", name);
	newstat = newstat ? 1 : 0;
	if (ioctl(fd, PIOCSVIR, &newstat) < 0)
		err(1, "ioctl (PIOCSVIR)");
	return 0;
}
