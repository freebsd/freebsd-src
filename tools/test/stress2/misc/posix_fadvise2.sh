#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Looping thread seen:
# https://people.freebsd.org/~pho/stress/log/kostik850.txt
# Fixed by r292326.

. ../default.cfg
[ -f /usr/libexec/sendmail/sendmail ] || exit 0

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > posix_fadvise2.c
mycc -o posix_fadvise2 -Wall -Wextra -O2 posix_fadvise2.c || exit 1
rm -f posix_fadvise2.c

/tmp/posix_fadvise2

rm -f /tmp/posix_fadvise2 /tmp/posix_fadvise2
exit
EOF
#include <sys/param.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char *file[2] = {
	"/usr/libexec/sendmail/sendmail",
	"/tmp/posix_fadvise2"};

int
main(void)
{
	int fd, i, r;

	for (i = 0; i < 2; i++) {
		fprintf(stderr, "Testing with %s.\n", file[i]);
		if ((fd = open(file[i], O_RDONLY)) == -1)
			err(1, "open(%s)", file[i]);

/*		Arguments from syscall4.sh test as seen in kostik850.txt */
		if ((r = posix_fadvise(fd, 0x1e9cda7a9ada8319,
		    0x1e9d1deee0401abd, POSIX_FADV_DONTNEED)) != 0)
			errc(1, r, "posix_fadvise(%s)", file[i]);

		close(fd);
	}

	return(0);
}
