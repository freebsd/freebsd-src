#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# Test scenario by kib@
# Demonstrate process looping in kernel mode.
# Fixed in r251684.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/rwlock_ronly.c
mycc -o rwlock_ronly -Wall -Wextra rwlock_ronly.c || exit 1
rm -f rwlock_ronly.c
cd $odir

/tmp/rwlock_ronly || echo OK

rm -f /tmp/rwlock_ronly
exit

EOF
/* $Id: rwlock_ronly.c,v 1.2 2013/06/10 04:44:08 kostik Exp kostik $ */

#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/umtx.h>
#include <err.h>
#include <unistd.h>

int
main(void)
{
	char *p;
	struct urwlock *rw;
	int error;

	p = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
	    MAP_SHARED | MAP_ANON, -1, 0);
	if (p == (char *)MAP_FAILED)
		err(1, "mmap");

	rw = (struct urwlock *)p;
	rw->rw_state = URWLOCK_READ_WAITERS;

	error = mprotect(p, getpagesize(), PROT_READ);
	if (error == -1)
		err(1, "mprotect");

	error = _umtx_op(p, UMTX_OP_RW_RDLOCK, 0, NULL, NULL);
	if (error != 0)
		err(1, "rdlock");

	return (0);
}
