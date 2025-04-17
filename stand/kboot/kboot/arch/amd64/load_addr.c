/*-
 * Copyright (c) 2022 Netflix, Inc
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

#include <sys/param.h>
#include <machine/pc/bios.h>
#include <machine/metadata.h>

#include "stand.h"
#include "host_syscall.h"
#include "efi.h"
#include "kboot.h"
#include "bootstrap.h"

bool
enumerate_memory_arch(void)
{
	efi_read_from_sysfs();
	if (!populate_avail_from_iomem())
		return (false);
	print_avail();
	return (true);
}

/* XXX refactor with aarch64 */
uint64_t
kboot_get_phys_load_segment(void)
{
#define HOLE_SIZE	(64ul << 20)
#define KERN_ALIGN	(2ul << 20)
	static uint64_t	s = 0;

	if (s != 0)
		return (s);

	print_avail();
	s = first_avail(KERN_ALIGN, HOLE_SIZE, SYSTEM_RAM);
	printf("KBOOT GET PHYS Using %#llx\n", (long long)s);
	if (s != 0)
		return (s);
	s = 0x40000000 | 0x4200000;	/* should never get here */
	/* XXX PANIC? XXX */
	printf("Falling back to the crazy address %#lx which works in qemu\n", s);
	return (s);
}

void
bi_loadsmap(struct preloaded_file *kfp)
{
	efi_bi_loadsmap(kfp);
}
