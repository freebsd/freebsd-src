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

# "panic: kern_jail: too many iovecs (28)" seen.

. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > jail4.c
mycc -o jail4 -Wall jail4.c
rm -f jail4.c
cd $odir
/tmp/jail4
rm -f /tmp/jail4
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
	path = 0x28190cb1 <Address 0x28190cb1 out of bounds>,
	hostname = 0x28167b90 <Address 0x28167b90 out of bounds>,
	jailname = 0x28198700 <Address 0x28198700 out of bounds>,
	ip4s = 0x0,
	ip6s = 0x0,
	ip4 = 0x0,
	ip6 = 0x0}
	 */
	j.version = 2;
	j.path = (char *)0x28190cb1;
	j.hostname = (char *)0x28167b90;
	j.jailname = (char *)0x28198700;
	j.ip4s = 0;
	j.ip6s = 0;
	j.ip4 = 0;
	j.ip6 = 0;

        if (jail(&j) == -1)
		err(1, "jail()");

	  return (0);
}
