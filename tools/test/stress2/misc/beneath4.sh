#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# Test of:
# AT_BENEATH              0x1000  /* Fail if not under dirfd */
# AT_RESOLVE_BENEATH      0x2000  /* As AT_BENEATH, but do not allow
#                                    resolve to walk out of dirfd even

dir=/tmp/beneath4.dir
rm -rf $dir
mkdir -p $dir
here=`pwd`
cd $dir

cat > beneath4.c <<EOF
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	struct stat st;
	int exp, fd, flag, r;
	char *cwd, *dir, *obj, *s;

	if (argc != 5) {
		fprintf(stderr,
		    "Usage: %s <dir> <test obj> <flag> <expected return>\n",
		    argv[0]);
		return (1);
	}

	cwd = getwd(NULL);
	dir = argv[1];
	obj = argv[2];
	sscanf(argv[3], "%x", &flag);
	exp = atoi(argv[4]);
#if 0
	if ((flag & AT_RESOLVE_BENEATH) == 0) {
		fprintf(stderr, "Flag must be %#x or %#x\n",
		    AT_BENEATH, AT_RESOLVE_BENEATH);
		return (1);
	}
#endif
	if ((fd = open(dir, O_DIRECTORY | O_RDONLY)) == -1)
		err(1, "open(%s)", dir);

	if (fstatat(fd, obj, &st, flag) == -1)
		r = errno;
	else
		r = 0;
	s = "FAIL";
	if (r == exp)
		s = "OK";
	warn("cwd=%s, top=%s. flag=%0.6x. fstatf(%s) = %2d (expect %2d). %4s",
	    cwd, dir, flag, obj, r, exp, s);

	return (r == exp ? 0 : errno);
}
EOF
cc -o beneath4 -Wall -Wextra -O2 -g beneath4.c || exit 1
rm beneath4.c

mkdir -p /tmp/beneath4.dir/a/a
touch    /tmp/beneath4.dir/a/f
ln       /tmp/beneath4.dir/a/f /tmp/beneath4.dir/a/c
ln -s    /tmp/beneath4.dir/a/a /tmp/beneath4.dir/a/d
ln -s    /tmp/beneath4.dir/a/b /tmp/beneath4.dir/a/e
mkfifo   /tmp/beneath4.dir/a/fifo

top=$dir/a

cd $here
s=0
#ls -lR $dir
#echo  AT_BENEATH
#$dir/beneath4 $top a                         0x1000  0 || s=1
#$dir/beneath4 $top b                         0x1000  2 || s=1
#$dir/beneath4 $top c                         0x1000  0 || s=1
#$dir/beneath4 $top d                         0x1000  0 || s=1
#$dir/beneath4 $top e                         0x1000  2 || s=1
#$dir/beneath4 $top fifo                      0x1000  0 || s=1
#$dir/beneath4 $top $top/../../beneath4.d/a/a 0x1000 93 || s=1
#$dir/beneath4 $top $top/..                   0x1000 93 || s=1
#$dir/beneath4 $top ../a                      0x1000  0 || s=1

printf "\nAT_RESOLVE_BENEATH\n"
$dir/beneath4 $top a                         0x2000  0 || s=1
$dir/beneath4 $top b                         0x2000  2 || s=1
$dir/beneath4 $top c                         0x2000  0 || s=1
$dir/beneath4 $top d                         0x2000 93 || s=1
$dir/beneath4 $top e                         0x2000 93 || s=1
$dir/beneath4 $top fifo                      0x2000  0 || s=1
$dir/beneath4 $top $top/../../beneath4.d/a/a 0x2000 93 || s=1
$dir/beneath4 $top $top/..                   0x2000 93 || s=1
$dir/beneath4 $top ../a                      0x2000 93 || s=1
printf "\nNo flag\n"
$dir/beneath4 $top ../a                      0x0000  0 || s=1
rm -rf $top
exit $s
