#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# "panic: swap_reserved < decr" seen. Fixed in r195329

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > mlockall.c
mycc -o mlockall -Wall mlockall.c
rm -f mlockall.c

for i in `jot 10`; do
	/tmp/mlockall &
	sleep 1
	ps -x | grep /tmp/mlockall | grep -v grep | awk '{print $1}' | \
	    while read pid; do
		kill -2 $pid
		kill -9 $pid
	done
done

rm -f /tmp/mlockall
exit
EOF
#include <err.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void
child(void)
{
	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
		err(1, "mlockall(MCL_CURRENT | MCL_FUTURE)");
	fork();
	sleep(60);
}

int
main(int argc, char **argv)
{
	int status;

	if (fork() == 0)
		child();
	wait(&status);

	return (0);
}
