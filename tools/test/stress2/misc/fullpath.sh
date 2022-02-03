#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Test scenario by marcus@freebsd.org

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -z "`type perl 2>/dev/null`" ] && exit 0

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > fullpath.c
mycc -o fullpath -Wall fullpath.c
rm -f fullpath.c
cd /proc

for i in `jot 5`; do
	/tmp/fullpath &
done

for i in `jot 30`; do
	for j in `jot 25`; do
		pid=`perl -e 'print splice(@ARGV,rand(@ARGV),1), " ";' $(echo [0-9]*)`
#		echo $pid
		procstat -f $pid > /dev/null 2>&1
		procstat -f $pid > /dev/null 2>&1
		procstat -f $pid > /dev/null 2>&1
		procstat -f $pid > /dev/null 2>&1
		procstat -f $pid > /dev/null 2>&1
	done
done

for i in `jot 5`; do
	wait
done

rm -f /tmp/fullpath
exit
EOF

#include <sys/wait.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

char buf[4096];

void
handler(int i) {
	exit(0);
}

int
test(void) {
	pid_t r;
	int status;

	for (;;) {
		r = fork();
		if (r == 0) {
			bzero(buf, sizeof(buf));
			exit(0); /*child dies */
		}
		if (r < 0) {
			perror("fork");
			exit(2);
		}
		wait(&status);
	}
	return 0;
}

int main(int argc, char **argv)
{

	signal(SIGALRM, handler);
	alarm(60);

	return test();
}
