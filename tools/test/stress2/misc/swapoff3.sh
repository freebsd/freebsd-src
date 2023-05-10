#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Mark Johnston
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

# "panic: Swapdev not found" seen:
# https://people.freebsd.org/~pho/stress/log/swapoff3.txt
# Fixed by r358024-6

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
dir=`dirname $diskimage`
[ `df -k $dir | tail -1 | awk '{print $4}'` -lt \
    $((20 * 1024 * 1024)) ] &&
    { echo "Need 20GB on $dir"; exit 0; }

cat > /tmp/swapoff3.c <<EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
	char *addr, *naddr, *res;
	size_t i, len, swap, vsz;
	u_int free;

	vsz = sizeof(free);
	if (sysctlbyname("vm.stats.vm.v_free_count", &free, &vsz, NULL, 0) != 0)
		err(1, "sysctl(vm.stats.vm.v_free_count)");
	vsz = sizeof(swap);
	if (sysctlbyname("vm.swap_total", &swap, &vsz, NULL, 0) != 0)
		err(1, "sysctl(vm.swap_total)");

	len = (size_t)free * PAGE_SIZE + swap / 4;
	addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
	    -1, 0);
	if (addr == MAP_FAILED)
		err(1, "mmap");

	res = malloc(howmany(len, PAGE_SIZE));
	if (res == NULL)
		err(1, "malloc");

	for (i = 0; i < len; i += PAGE_SIZE)
		addr[i] = 1;

	for (;;) {
		if (mincore(addr, len, res) != 0)
			err(1, "mincore");
		for (i = 0; i < howmany(len, PAGE_SIZE); i++)
			if ((res[i] & MINCORE_INCORE) == 0) {
				naddr = addr + i * PAGE_SIZE;
				if (munmap(naddr, PAGE_SIZE) != 0)
					err(1, "munmap");
				if (mmap(naddr, PAGE_SIZE,
				    PROT_READ | PROT_WRITE,
				    MAP_ANON | MAP_PRIVATE | MAP_FIXED,
				    -1, 0) == MAP_FAILED)
					err(1, "mmap");
				assert(*naddr == 0);
				*naddr = 1;
			}
	}

	return (0);
}
EOF
mycc -o /tmp/swapoff3 -Wall -Wextra -O2 -g /tmp/swapoff3.c || exit

set -e
md1=$mdstart
md2=$((md1 + 1))
[ `sysctl -n vm.swap_total` -gt 0 ] && { swapoff -a > /dev/null; off=1; }
truncate -s 10G $dir/swap1
mdconfig -a -t vnode -f $dir/swap1 -u $md1
swapon /dev/md$md1
truncate -s 10G $dir/swap2
mdconfig -a -t vnode -f $dir/swap2 -u $md2
swapon /dev/md$md2
set +e

timeout 4m /tmp/swapoff3 &
for i in `jot 60`; do
	n=`swapinfo | tail -1 | awk '{print $3}'`
	[ $n -gt 0 ] && break
	sleep 1
done
echo "Total in use: $n"

n=0
start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
	for dev in /dev/md$md1 /dev/md$md2; do
		for i in `jot 3`; do
			swapoff $dev && break
		done
		swapon $dev
		n=$((n + 1))
	done
	sleep .1
done
kill $!
echo "$n swapoffs"
[ $n -lt 1000 ] && { echo "Only performed $n swapoffs"; s=1; } || s=0
wait
for dev in /dev/md$md1 /dev/md$md2; do
	swapoff $dev
done
mdconfig -d -u $md1
mdconfig -d -u $md2
rm -f $dir/swap1 $dir/swap2 /tmp/swapoff3 /tmp/swapoff3.c
[ $off ] && swapon -a > /dev/null
exit 0
