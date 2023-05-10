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

# "panic: pmap_demote_pde: page table page for a wired mapping
# is missing" seen.

# Fixed by r345382

. ../default.cfg

cd /tmp
cat > mprotect.c <<EOF
#include <sys/mman.h>

#include <err.h>
#include <stdlib.h>

int
main(void)
{
	char *addr, c __unused;
	size_t i, len;

	len = 2 * 1024 * 1024;
	addr = mmap(NULL, 2 * 1024 * 1024, PROT_READ,
	    MAP_ANON | MAP_ALIGNED_SUPER, -1, 0);
	if (addr == MAP_FAILED)
		err(1, "mmap");
	if (mlock(addr, len) != 0) /* hopefully this gets a superpage */
		err(1, "mlock");
	if (mprotect(addr, len, PROT_NONE) != 0)
		err(1, "mprotect");
	if (mprotect(addr, len, PROT_READ) != 0)
		err(1, "mprotect");
	for (i = 0; i < len; i++) /* preemptive superpage mapping */
		c = *(volatile char *)(addr + i);
	if (mprotect(addr, 4096, PROT_NONE) != 0) /* trigger demotion */
		err(1, "mprotect");
	if (munlock(addr, len) != 0)
		err(1, "munlock");

	return (0);
}
EOF
mycc -o mprotect -Wall -Wextra -O0 mprotect.c || exit 1

./mprotect; s=$?

rm mprotect.c mprotect
exit $s
