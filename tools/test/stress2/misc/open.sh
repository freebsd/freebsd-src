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

# Regression test for:
# Bug 202892 open with O_CREAT | O_DIRECTORY when path references a symlink.
# Fixed by r287599.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/open.c
mycc -o open -Wall -Wextra -O0 -g open.c || exit 1
rm -f open.c

wdir=/tmp/open.$$
rm -rf $wdir
mkdir -p $wdir
cd $wdir
status=0
/tmp/open || { echo FAIL; status=$?; }
[ -f broken -o -f broken2 ]  && { ls -l; echo FAIL; status=1; }
cd $odir

rm -rf /tmp/open $wdir
exit $status

EOF
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
	int fd;

	/* Setup. */
	if (unlink("broken") <= 0 && errno != ENOENT)
		err(1, "unlink(broken)");
	if (unlink("target") <= 0 && errno != ENOENT)
		err(1, "unlink(target)");
	if (symlink("target", "broken") < 0)
		err(1, "symlink(target, broken)");

	/* Test. */
	fd = open("broken", O_CREAT | O_DIRECTORY, 0600);
	if (fd >= 0)
		errx(1, "open(broken, O_CREAT | O_DIRECTORY) - no error");

	fd = open("broken2", O_CREAT | O_DIRECTORY | O_EXCL, 0600);
	if (fd != -1)
		errx(1, "open() O_CREAT | O_DIRECTORY | O_EXCL");

	return (0);
}
