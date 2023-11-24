#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Mark Johnston <markj@freebsd.org>
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

# Test scenario submitted by Mark Johnston <markj@freebsd.org>

# "Fatal trap 18: integer divide fault while in kernel mode" seen.
# Reported by syzkaller
# Fixed by r353010

cat > /tmp/fcntl3.c <<EOF
#include <err.h>
#include <fcntl.h>
#include <unistd.h>

int
main(void)
{

	if (fcntl(STDIN_FILENO, F_RDAHEAD) != 0)
		err(1, "fcntl");
	return (0);
}
EOF
cc -o /tmp/fcntl3 -Wall -Wextra -O2 /tmp/fcntl3.c || exit 1

echo "Expect: fcntl3: fcntl: Inappropriate ioctl for device"
/tmp/fcntl3

rm -f /tmp/fcntl3 /tmp/fcntl3.c
exit 0
