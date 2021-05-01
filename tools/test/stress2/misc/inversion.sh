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

# Provokes a deadlock by lower priority process holding a lock and
# never beeing run

. ../default.cfg

dir=$RUNDIR
N=5	# Number of CPUs + 1
M=25	# Number of lower priority jobs

odir=`pwd`
mkdir -p $dir || exit 1
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/inversion.c
mycc -o inversion -Wall inversion.c
rm -f inversion.c

mp=`df $dir | tail -1 | awk '{print $NF}'`
mp=`mount | grep "on $mp "`
if echo $mp | grep -wq nfs; then
	pgrep -q lockd || { echo "lockd not running"; exit 0; }
fi

for i in `jot $N`; do
	./inversion 600 &
done

while pgrep inversion > /dev/null; do
	(
		for i in `jot $M`; do
			nice -n 20 lockf -s -t 0 .lock pwd > /dev/null &
		done
		for i in `jot $M`; do
			wait
		done
	)
done

for i in `jot $N`; do
	wait
done
rm -f inversion
exit

EOF
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void
handler(int i)
{
	exit(0);
}

int
main(int argc, char **argv)
{

	int t;
	if (argc == 2)
		t = atoi(argv[1]);
	else
		t = 60;
	signal(SIGALRM, handler);
	alarm(t);
	for (;;)
		;
	return (0);
}
