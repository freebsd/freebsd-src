/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2007 Semihalf, Rafal Jaworowski <raj@semihalf.com> 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <stdint.h>

#include "api_public.h"
#include "glue.h"

/*
 * MD primitives supporting placement of module data 
 */

void *
uboot_vm_translate(vm_offset_t o) {
	struct sys_info *si;
	static uintptr_t start = 0;
	static size_t size = 0;
	int i;

	if (size == 0) {
		if ((si = ub_get_sys_info()) == NULL)
			panic("could not retrieve system info");

		/* Find start/size of largest DRAM block. */
		for (i = 0; i < si->mr_no; i++) {
			if (si->mr[i].flags == MR_ATTR_DRAM
			    && si->mr[i].size > size) {
				start = si->mr[i].start;
				size = si->mr[i].size;
			}
		}

		if (size <= 0)
			panic("No suitable DRAM?\n");
		/*
		printf("Loading into memory region 0x%08X-0x%08X (%d MiB)\n",
		    start, start + size, size / 1024 / 1024);
		*/
	}
	if (o > size)
		panic("Address offset 0x%08jX bigger than size 0x%08X\n",
		      (intmax_t)o, size);
	return (void *)(start + o);
}

ssize_t
uboot_copyin(const void *src, vm_offset_t dest, const size_t len)
{
	bcopy(src, uboot_vm_translate(dest), len);
	return (len);
}

ssize_t
uboot_copyout(const vm_offset_t src, void *dest, const size_t len)
{
	bcopy(uboot_vm_translate(src), dest, len);
	return (len);
}

ssize_t
uboot_readin(const int fd, vm_offset_t dest, const size_t len)
{
	return (read(fd, uboot_vm_translate(dest), len));
}
