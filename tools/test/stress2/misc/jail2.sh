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

# Regression test for problem found with the syscall.sh test

. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > jail2.c
mycc -o jail2 -Wall jail2.c
rm -f jail2.c
cd $odir
/tmp/jail2
rm -f /tmp/jail2
exit
EOF
#include <sys/param.h>
#include <sys/jail.h>
#include <err.h>

int
main()
{
	struct jail j;

	/*
	version = 0x2,
	path = 0x0,
	hostname = 0x0,
	jailname = 0x50000000 <Address 0x50000000 out of bounds>,
	ip4s = 0xf7000004,
	ip6s = 0x1,
	ip4 = 0x0,
	ip6 = 0x0
	 */
	j.version = 2;
	j.path = 0;
	j.hostname = 0;
	j.jailname = (char *)0x50000000;
	j.ip4s = 0xf7000004;
	j.ip6s = 1;
	j.ip4 = 0;
	j.ip6 = 0;

        if (jail(&j) == -1)
		err(1, "jail()");

	  return (0);
}
