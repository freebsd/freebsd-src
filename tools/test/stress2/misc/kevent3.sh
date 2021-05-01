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

. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > kevent3.c
mycc -o kevent3 -Wall kevent3.c -pthread || exit 1
rm -f kevent3.c
cd $RUNDIR

for i in `jot 64`; do
	for j in `jot 12`; do
		/tmp/kevent3 > /dev/null 2>&1 &
	done
	for j in `jot 12`; do
		wait
	done
done
rm -f /tmp/kevent3
exit 0
EOF
/*
 *  Obtained from:
 *  http://projects.info-pull.com/mokb/MOKB-24-11-2006.html
 */
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <stdio.h>
#include <unistd.h>

int main(void)
{
	struct kevent ke;
	int kq;

	kq = kqueue();
	EV_SET(&ke, getpid(), EVFILT_PROC, EV_ADD,
		NOTE_EXIT|NOTE_EXEC|NOTE_TRACK, 0, NULL);
	kevent(kq, &ke, 1, NULL, 0, NULL);
	if (fork() != 0)
		kevent(kq, NULL, 0, &ke, 1, NULL);

	return (0);
}
