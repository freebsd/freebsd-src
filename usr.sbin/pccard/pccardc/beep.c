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

/*
 * Code cleanup, bug-fix and extension
 * by Tatsumi Hosokawa <hosokawa@mt.cs.keio.ac.jp>
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
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
beep_main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     fd, newstat, valid = 1;
	char    name[64], *p;

	if (argc != 2)
		valid = 0;
	if (valid) {
		for (p = argv[1]; *p; p++) {
			if (!isdigit(*p)) {
				valid = 0;
				break;
			}
		}
	}
	if (!valid)
		errx(1, "usage: %s beep newstat", argv[0]);

	sscanf(argv[1], "%d", &newstat);
	sprintf(name, CARD_DEVICE, 0);
	fd = open(name, O_RDWR);
	if (fd < 0)
		err(1, "%s", name);
	if (ioctl(fd, PIOCSBEEP, &newstat) < 0)
		err(1, "ioctl (PIOCSBEEP)");
	return 0;
}
