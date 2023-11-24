#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2017 Konstantin Belousov <kib@FreeBSD.org>
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

# Trigger a SIGSEGV/SIGBUS _not_ caused by an access to the unmapped page.

uname -a | egrep -q "i386|amd64" || exit 0
. ../default.cfg

cat > /tmp/segnp.c <<EOF
/* $Id: segnp.c,v 1.2 2017/08/12 10:23:28 kostik Exp kostik $ */

#include <stdio.h>

int
main(void)
{

	__asm __volatile ("movw %w0,%%ds\n" : : "q" (0x1117) : "memory");
	printf("Huh ?\n");
}
EOF
mycc -o /tmp/segnp -Wall -Wextra -O2 /tmp/segnp.c || exit 1
rm /tmp/segnp.c

echo "expect: Bus error (core dumped)"
(cd /tmp; /tmp/segnp)
s=$?
[ $s -ne 138 ] && { echo "Expected 138, got $s", exit 1; }

rm /tmp/segnp /tmp/segnp.core
exit 0
