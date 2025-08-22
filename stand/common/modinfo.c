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

/*
 * Copy module-related data into the load area, where it can be
 * used as a directory for loaded modules.
 *
 * Module data is presented in a self-describing format.  Each datum
 * is preceded by a 32-bit identifier and a 32-bit size field.
 *
 * Currently, the following data are saved:
 *
 * MOD_NAME	(variable)		module name (string)
 * MOD_TYPE	(variable)		module type (string)
 * MOD_ARGS	(variable)		module parameters (string)
 * MOD_ADDR	sizeof(vm_offset_t)	module load address
 * MOD_SIZE	sizeof(size_t)		module size
 * MOD_METADATA	(variable)		type-specific metadata
 *
 * Clients are required to define a MOD_ALIGN(l) macro which rounds the passed
 * in length to the required alignment for the kernel being booted.
 */

#define COPY32(v, a, c) {			\
    uint32_t	x = (v);			\
    if (c)					\
        archsw.arch_copyin(&x, a, sizeof(x));	\
    a += sizeof(x);				\
}

#define MOD_STR(t, a, s, c) {			\
    COPY32(t, a, c);				\
    COPY32(strlen(s) + 1, a, c)			\
    if (c)					\
        archsw.arch_copyin(s, a, strlen(s) + 1);\
    a += MOD_ALIGN(strlen(s) + 1);		\
}

#define MOD_NAME(a, s, c)	MOD_STR(MODINFO_NAME, a, s, c)
#define MOD_TYPE(a, s, c)	MOD_STR(MODINFO_TYPE, a, s, c)
#define MOD_ARGS(a, s, c)	MOD_STR(MODINFO_ARGS, a, s, c)

#define MOD_VAR(t, a, s, c) {			\
    COPY32(t, a, c);				\
    COPY32(sizeof(s), a, c);			\
    if (c)					\
        archsw.arch_copyin(&s, a, sizeof(s));	\
    a += MOD_ALIGN(sizeof(s));			\
}

#define MOD_ADDR(a, s, c)	MOD_VAR(MODINFO_ADDR, a, s, c)
#define MOD_SIZE(a, s, c)	MOD_VAR(MODINFO_SIZE, a, s, c)

#define MOD_METADATA(a, mm, c) {		\
    COPY32(MODINFO_METADATA | mm->md_type, a, c);\
    COPY32(mm->md_size, a, c);			\
    if (c) {					\
        archsw.arch_copyin(mm->md_data, a, mm->md_size);\
	mm->md_addr = a;			\
    }						\
    a += MOD_ALIGN(mm->md_size);		\
}

#define MOD_END(a, c) {				\
    COPY32(MODINFO_END, a, c);			\
    COPY32(0, a, c);				\
}

#define MOD_ALIGN(l)	roundup(l, align)

const char md_modtype[] = MODTYPE;
const char md_kerntype[] = KERNTYPE;
const char md_modtype_obj[] = MODTYPE_OBJ;
const char md_kerntype_mb[] = KERNTYPE_MB;

vm_offset_t
md_copymodules(vm_offset_t addr, bool kern64)
{
	struct preloaded_file	*fp;
	struct file_metadata	*md;
	uint64_t		scratch64;
	uint32_t		scratch32;
	int			c;
	int			align;

	align = kern64 ? sizeof(uint64_t) : sizeof(uint32_t);
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

/*
 * Copy the environment into the load area starting at (addr).
 * Each variable is formatted as <name>=<value>, with a single nul
 * separating each variable, and a double nul terminating the environment.
 */
vm_offset_t
md_copyenv(vm_offset_t start)
{
	struct env_var *ep;
	vm_offset_t addr, last;
	size_t len;

	addr = last = start;

	/* Traverse the environment. */
	for (ep = environ; ep != NULL; ep = ep->ev_next) {
		if ((ep->ev_flags & EV_NOKENV) != 0)
			continue;
		len = strlen(ep->ev_name);
		if ((size_t)archsw.arch_copyin(ep->ev_name, addr, len) != len)
			break;
		addr += len;
		if (archsw.arch_copyin("=", addr, 1) != 1)
			break;
		addr++;
		if (ep->ev_value != NULL) {
			len = strlen(ep->ev_value);
			if ((size_t)archsw.arch_copyin(ep->ev_value, addr, len) != len)
				break;
			addr += len;
		}
		if (archsw.arch_copyin("", addr, 1) != 1)
			break;
		last = ++addr;
	}

	if (archsw.arch_copyin("", last++, 1) != 1)
		last = start;
	return(last);
}

/*
 * Take the ending address and round it up to the currently required
 * alignment. This typically is the page size, but is the larger of the compiled
 * kernel page size, the loader page size, and the typical page size on the
 * platform.
 *
 * XXX For the moment, it's just PAGE_SIZE to make the refactoring go faster,
 * but needs to hook-in the replacement of arch_loadaddr.
 *
 * Also, we may need other logical things when dealing with different types of
 * page sizes and/or masking or sizes. This works well for addr and sizes, but
 * not for masks.
 *
 * Also, this is different than the MOD_ALIGN macro above, which is used for
 * aligning elements in the metadata lists, not for whare modules can begin.
 */
vm_offset_t
md_align(vm_offset_t addr)
{
	return (roundup(addr, PAGE_SIZE));
}
