#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2018 Konstantin Belousov <kib@FreeBSD.org>
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

# Demonstrate superpage mapping.

cat > /tmp/shm_super.c <<EOF
/* $Id: shm_super.c,v 1.1 2018/10/13 23:49:37 kostik Exp kostik $ */

#include <sys/param.h>
#include <sys/mman.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define M(x)    ((x) * 1024 * 1024)
#define SZ      M(4)

int
main(void)
{
        char buf[128];
        void *ptr;
        off_t cnt;
        int error, shmfd;

        shmfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0600);
        if (shmfd == -1)
                err(1, "shm_open");
        error = ftruncate(shmfd, SZ);
        if (error == -1)
                err(1, "truncate");
        memset(buf, 0, sizeof(buf));
        for (cnt = 0; cnt < SZ; cnt += sizeof(buf)) {
                error = write(shmfd, buf, sizeof(buf));
                if (error == -1)
                        err(1, "write");
                else if (error != sizeof(buf))
                        errx(1, "short write %d", (int)error);
        }
        ptr = mmap(NULL, SZ, PROT_READ | PROT_WRITE, MAP_SHARED |
            MAP_ALIGNED_SUPER, shmfd, 0);
        if (ptr == MAP_FAILED)
                err(1, "mmap");
        for (cnt = 0; cnt < SZ; cnt += PAGE_SIZE)
                *((char *)ptr + cnt) = 0;
        printf("ptr %p\n", ptr);
        snprintf(buf, sizeof(buf), "procstat -v %d", getpid());
        system(buf);
}
EOF
cc -o /tmp/shm_super -Wall -Wextra -O2 -g /tmp/shm_super.c || exit 1
rm /tmp/shm_super.c

/tmp/shm_super > /tmp/shm_super.log 2>&1
grep -wq S /tmp/shm_super.log && s=0 || { cat /tmp/shm_super.log; s=1; }
[ $s -eq 1 ] && echo "No superpage mappings found."

rm -f /tmp/shm_super /tmp/shm_super.log
exit $s
