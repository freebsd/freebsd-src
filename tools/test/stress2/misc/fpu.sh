#!/bin/sh

#
# Copyright (c) 2010 Peter Holm <pho@FreeBSD.org>
# All rights reserved.
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

# Regression test for FPU changes in r208833

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > fpu.c
mycc -o fpu -Wall -O2 fpu.c
rm -f fpu.c
[ -d $RUNDIR ] || mkdir -p $RUNDIR
cd $RUNDIR

r=`/tmp/fpu`
[ "$r" = "-0.000000017, 0.000000000, 0.000000000" ] || echo $r

cd $here
rm -f /tmp/fpu

exit 0
EOF
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

void
handler(int i)
{
}

void
test()
{
	float val = 0;
	double lval = 0;
	long double llval = 0;
	int i, j;

	for (i = 0; i < 100000; i++) {
		for (j = 0; j < 100000; j++) {
			val   = val   + 0.00001;
			lval  = lval  + 0.00001;
			llval = llval + 0.00001;
		}
		for (j = 0; j < 100000; j++) {
			val   = val   - 0.00001;
			lval  = lval  - 0.00001;
			llval = llval - 0.00001;
		}
	}
	printf("%.9f, %.9f, %.9Lf\n", val, lval, llval);
	exit(0);

}

int
main()
{
	pid_t pid;
	int i;

	signal(SIGHUP, handler);

	if ((pid = fork()) == 0)
		test();

	for (i = 0; i < 10000; i++)
		kill(pid, SIGHUP);

	wait(NULL);

	return (0);
}
