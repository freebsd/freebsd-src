#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# Reported by syzkaller.
# "panic: vm_page_free_prep: page 0x61e5968 has unexpected ref_count ." seen
# Fixed by r352748

# Test scenario by: Mark Johnston <markj@freebsd.org>

cat > /tmp/fexecve.c <<EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int
main(int argc __unused, char **argv)
{
	char template[PATH_MAX];
	void *addr;
	size_t sz;
	int fd;

	sz = 16 * 4096;

	(void)snprintf(template, sizeof(template), "fexecve.XXXXXX");
	fd = mkstemp(template);
	if (fd < 0)
		err(1, "mkstemp");
	if (fchmod(fd, 0700) < 0)
		err(1, "fchmod");
	if (ftruncate(fd, sz) < 0)
		err(1, "ftruncate");

	addr = mmap(NULL, sz, PROT_MAX(PROT_READ) | PROT_READ, MAP_SHARED,
	    fd, 0);
	if (addr == MAP_FAILED)
		err(1, "mmap");

	if (mlock(addr, sz) != 0)
		err(1, "mlock");

	if (ftruncate(fd, 0) != 0)
		err(1, "ftruncate");
	if (ftruncate(fd, sz) != 0)
		err(1, "ftruncate");

	(void)close(fd);

	fd = open(template, O_EXEC);
	if (fd < 0)
		err(1, "open");
	fexecve(fd, argv, NULL);
	err(1, "fexecve");

	return (0);
}
EOF
cc -o /tmp/fexecve -Wall -Wextra -O2 /tmp/fexecve.c || exit 1
echo "Expect: fexecve: fexecve: Input/output error"
(cd /tmp; /tmp/fexecve)

rm -f /tmp/fexecve /tmp/fexecve.c /tmp/fexecve.??????
