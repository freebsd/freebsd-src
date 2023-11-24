#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Jeremy <peterj@FreeBSD.org>
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
# Unkillable process in "vm map (user)" seen.
# https://people.freebsd.org/~pho/stress/log/kostik1070.txt
# Fixed by: r327468

# OOM killing: https://people.freebsd.org/~pho/stress/log/chain.txt

if [ ! -f /usr/local/include/libmill.h -o \
    ! -x /usr/local/lib/libmill.so ]; then
	echo "ports/devel/libmill needed."
	exit 0
fi

. ../default.cfg

cat > /tmp/chain.c <<EOF
#include <stdio.h>
#include <stdlib.h>
#include <libmill.h>

coroutine void f(chan left, chan right) {
    chs(left, int, 1 + chr(right, int));
}

int
main(int argc __unused, char **argv)
{
	int i, n = argv[1] ? atoi(argv[1]) : 10000;
	chan leftmost = chmake(int, 0);
	chan left = NULL;
	chan right = leftmost;

	alarm(600);
	for (i = 0; i < n; i++) {
		left = right;
		right = chmake(int, 0);
		go(f(left, right));
	}
	chs(right, int, 0);
	i = chr(leftmost, int);
	printf("result = %d\n", i);
	return(0);
}
EOF

mycc -o /tmp/chain -I /usr/local/include -L /usr/local/lib -Wall -Wextra \
	 -O2 -g /tmp/chain.c -lmill || exit 1
limits -c 0 /tmp/chain 1000000
rm -f /tmp/chain /tmp/chain.c
exit 0
