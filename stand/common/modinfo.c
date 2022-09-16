/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 *
 *	from: FreeBSD: src/sys/boot/sparc64/loader/metadata.c,v 1.6
 */
#include <stand.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/boot.h>
#include <sys/reboot.h>
#if defined(LOADER_FDT_SUPPORT)
#include <fdt_platform.h>
#endif

#ifdef __arm__
#include <machine/elf.h>
#endif
#include <machine/metadata.h>

#include "bootstrap.h"
#include "modinfo.h"

static int align;
#define MOD_ALIGN(l)	roundup(l, align)

vm_offset_t
md_copymodules(vm_offset_t addr, bool kern64)
{
	struct preloaded_file	*fp;
	struct file_metadata	*md;
	uint64_t		scratch64;
	uint32_t		scratch32;
	int			c;

	c = addr != 0;
	/* start with the first module on the list, should be the kernel */
	for (fp = file_findfile(NULL, NULL); fp != NULL; fp = fp->f_next) {

		MOD_NAME(addr, fp->f_name, c);	/* this field must come first */
		MOD_TYPE(addr, fp->f_type, c);
		if (fp->f_args)
			MOD_ARGS(addr, fp->f_args, c);
		if (kern64) {
			scratch64 = fp->f_addr;
			MOD_ADDR(addr, scratch64, c);
			scratch64 = fp->f_size;
			MOD_SIZE(addr, scratch64, c);
		} else {
			scratch32 = fp->f_addr;
#ifdef __arm__
			scratch32 -= __elfN(relocation_offset);
#endif
			MOD_ADDR(addr, scratch32, c);
			MOD_SIZE(addr, fp->f_size, c);
		}
		for (md = fp->f_metadata; md != NULL; md = md->md_next) {
			if (!(md->md_type & MODINFOMD_NOCOPY)) {
				MOD_METADATA(addr, md, c);
			}
		}
	}
	MOD_END(addr, c);
	return(addr);
}
