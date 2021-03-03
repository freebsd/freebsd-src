/*-
 * Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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
 *
 */

/* Test PTYs */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <err.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "stress.h"

#define TXT "Hello, world!"

int
setup(int nb __unused)
{
        return (0);
}

void
cleanup(void)
{
}

int
test(void)
{
	struct termios tios;
        int i, master, slave;
	int s[32], m[32];
	char buf[512], slname[1025];

	for (i = 0; i < 32; i++) {
		if (openpty(&m[i], &s[i], slname, NULL, NULL) == -1)
			err(1, "openpty");
	}
	for (i = 0; i < 32; i++) {
		close(m[i]);
		close(s[i]);
	}

	for (i = 0; i < 1024; i++) {
		if (openpty(&m[0], &s[0], slname, NULL, NULL) == -1)
			err(1, "openpty");
		close(m[0]);
		close(s[0]);
	}

	for (i = 0; i < 10 && done_testing == 0; i++) {
		if (openpty(&master, &slave, slname, NULL, NULL) == -1)
			err(1, "openpty");
		if ((i & 1) == 0) {
			if (close(master) == -1)
				err(1, "close(master)");
			if (close(slave) == -1)
				err(1, "close(%s)", slname);
		} else {
			if (close(slave) == -1)
				err(1, "close(%s)", slname);
			if (close(master) == -1)
				err(1, "close(master)");
		}
	}

        if (openpty(&master, &slave, slname, NULL, NULL) == -1)
                err(1, "openpty");
	if (tcgetattr(slave, &tios) < 0)
		err(1, "tcgetattr(%s)", slname);
	cfmakeraw(&tios);
	if (tcsetattr(slave, TCSAFLUSH, &tios) < 0)
		err(1, "tcsetattr(%s)", slname);

	for (i = 0; i < 64 && done_testing == 0; i++) {
		if (write(master, TXT, sizeof(TXT)) == -1)
			err(1, "write");
		if (read(slave, buf, sizeof(TXT)) == -1)
			err(1, "read(%s)", slname);
	}
        close(master);
        close(slave);
        return (0);
}
