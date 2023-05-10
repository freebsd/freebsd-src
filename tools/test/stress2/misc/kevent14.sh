#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# "panic: Bad tailq NEXT(0xfffff815a589e238->tqh_last) != NULL" seen.
# https://people.freebsd.org/~pho/stress/log/kevent14.txt
# Fixed by r340734.

# Test scenario by: Mark Johnston <markj@freebsd.org>

cat > /tmp/kevent14.c <<EOF
#include <sys/event.h>

#include <err.h>
#include <time.h>
#include <stdlib.h>

int
main(void)
{
	struct kevent ev[2];
	time_t start;
	int kq, ret;

	kq = kqueue();
	if (kq < 0)
		err(1, "kqueue");

	EV_SET(&ev[0], 42, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, 0,
	    0, 0);

	start = time(NULL);
	while (time(NULL) - start < 120) {
		ret = kevent(kq, &ev[0], 1, &ev[1], 1, NULL);
		if (ret < 0)
			err(1, "kevent");
		if (ret == 0)
			errx(1, "no events");
		if (ret == 1 && (ev[1].flags & EV_ERROR) != 0)
			errc(1, ev[1].data, "kevent");
	}

	return (0);
}
EOF
cc -o /tmp/kevent14 -Wall -Wextra -g -O2 /tmp/kevent14.c
rm /tmp/kevent14.c

/tmp/kevent14

rm -f /tmp/kevent14
exit 0
