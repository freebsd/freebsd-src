#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Konstantin Belousov
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

# Pipe test scenario from https://reviews.freebsd.org/D23993
# https://gist.github.com/kostikbel/b2844258b7fba6e8ce3ccd8ef9422e5a

. ../default.cfg

cd /tmp
cat > pipe_enomem.c <<EOF
/* $Id: pipe_enomem.c,v 1.3 2020/03/07 21:02:04 kostik Exp kostik $ */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct pipepair {
	int pp[2];
};

static struct pipepair *p;

int
main(void)
{
	int error, pp[2];
	size_t i, k, nsz, sz;
	char x;

	sz = 1024;
	p = calloc(sz, sizeof(struct pipepair));
	if (p == NULL) {
		fprintf(stderr, "calloc: %s\n", strerror(errno));
		exit(1);
	}

	for (i = 0;; i++) {
		if (pipe(pp) == -1) {
			printf("created %zd pipes. syscall error %s\n",
			    i, strerror(errno));
			break;
		}
		if (i >= sz) {
			nsz = sz * 2;
			p = reallocf(p, nsz * sizeof(struct pipepair));
			if (p == NULL) {
				fprintf(stderr, "reallocf: %s\n",
				    strerror(errno));
				exit(1);
			}
			memset(p + sz, 0, (nsz - sz) * sizeof(struct pipepair));
			sz = nsz;
		}
		p[i].pp[0]= pp[0];
		p[i].pp[1]= pp[1];
	}

	x = 'a';
	for (k = 0; k < i; k++) {
		error = write(p[k].pp[1], &x, 1);
		if (error == -1)
			printf("pipe %zd fds %d %d error %s\n",
			    k, p[k].pp[0], p[k].pp[1], strerror(errno));
		else if (error == 0)
			printf("pipe %zd fds %d %d EOF\n",
			    k, p[k].pp[0], p[k].pp[1]);
	}
}
EOF

mycc -o pipe_enomem -Wall -Wextra -O2 pipe_enomem.c || exit 1
./pipe_enomem 2>&1 | head -5
rm -f pipe_enomem.c pipe_enomem
exit 0
