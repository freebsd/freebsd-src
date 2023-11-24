#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Konstantin Belousov <kib@FreeBSD.org>
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

# Test of open(2) with the O_RESOLVE_BENEATH flag.

# userret: returning with the following locks held:
# shared lockmgr ufs (ufs) r = 0 (0xfffff804ec0d2a48) locked @
# kern/vfs_subr.c:2590 seen in WiP code:
# https://people.freebsd.org/~pho/stress/log/kostik1126.txt

top=/tmp/beneath.d
mkdir -p $top
cat > $top/beneath.c <<EOF
/* $Id: beneath.c,v 1.1 2018/10/13 16:53:02 kostik Exp kostik $ */

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	struct stat st;
	char *name;
	int error, fd, i;

	for (i = 1; i < argc; i++) {
		name = argv[i];
		alarm(120);
		fd = open(name, O_RDONLY | O_RESOLVE_BENEATH);
		if (fd == -1) {
			fprintf(stderr, "open(\"%s\") failed, error %d %s\n",
			    name, errno, strerror(errno));
		} else {
			fprintf(stderr, "open(\"%s\") succeeded\n", name);
			close(fd);
		}
		error = fstatat(AT_FDCWD, name, &st, AT_RESOLVE_BENEATH);
		if (error == -1){
			fprintf(stderr, "stat(\"%s\") failed, error %d %s\n",
			    name, errno, strerror(errno));
		} else {
			fprintf(stderr, "stat(\"%s\") succeeded\n", name);
		}
	}
}
EOF
cc -o $top/beneath -Wall -Wextra $top/beneath.c || exit 1
rm $top/beneath.c

# Test with two directories as arguments:
cd $top
mkdir -p a/b
./beneath a/b
./beneath $top/a/b
touch $top/a/c
./beneath a/c
./beneath $top/a/c
./beneath a/d
./beneath $top/a/d

# CWD is still $top for this test
top2=/var/tmp/beneath.d
mkdir -p $top2
mkdir -p $top2/a/b
./beneath $top2/a/b > /dev/null 2>&1

touch $top2/a/c
./beneath $top2/a/c > /dev/null 2>&1

# Other CWDs
(cd /etc; find . | head -1000 | xargs $top/beneath) > /dev/null 2>&1
(cd /var; find . | head -1000 | xargs $top/beneath) > /dev/null 2>&1

rm -rf $top $top2
exit 0
