#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

# Calling exec*(2) in a thread of a i386 binary on amd64 caused a reboot.
# Test scenario by: Steven Chamberlain <stev..@pyro.eu.org>
# Fixed by r266464

[ `uname -p` = "amd64" ] || exit 0

. ../default.cfg

wd=/tmp/execi386.dir
mkdir -p $wd
here=`pwd`
cd $wd

cat > execi386.c <<EOF
/* https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=743141  */
#include <unistd.h>
#include <pthread.h>

void *
thread_main() {
        char *cmdline[] = { "./i386", NULL };

        execve(cmdline[0], cmdline, NULL);

	return (NULL);
}

int
main() {
        pthread_t thread;

        pthread_create(&thread, NULL, thread_main, NULL);
        pthread_join(thread, NULL);

        return (0);
}
EOF

mycc -o execi386 -Wall -Wextra -O2 -g execi386.c -lpthread || exit 1

cat > i386.c <<EOF
#include <stdio.h>

int
main(void)
{
	fprintf(stdout, "Hello, world\n");
	return (0);
}
EOF

mycc -m32 -o i386 -Wall -Wextra -O2 -g i386.c || exit 1

./execi386 > /dev/null || echo FAIL

cd $here
rm -rf $wd
exit 0
