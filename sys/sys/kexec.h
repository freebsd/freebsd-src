/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Juniper Networks, Inc.
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

#ifndef	_SYS_KEXEC_H_
#define	_SYS_KEXEC_H_

#include <sys/types.h>

struct kexec_segment {
	void *buf;
	size_t bufsz;
	vm_paddr_t mem;
	vm_size_t memsz;
};

/* Flags (aligned with Linux) */
#define	KEXEC_ON_CRASH		0x1

/* Aligned with Linux's limit */
#define	KEXEC_SEGMENT_MAX	16

#ifdef	_KERNEL
struct kexec_segment_stage {
	vm_page_t	first_page;
	void		*map_buf;
	vm_paddr_t	target;
	vm_size_t	size;
	vm_pindex_t	pindex;
};

struct kexec_image {
	struct kexec_segment_stage	 segments[KEXEC_SEGMENT_MAX];
	vm_paddr_t			 entry;
	struct vm_object		*map_obj;	/* Containing object */
	vm_offset_t			 map_addr;	/* Mapped in kernel space */
	vm_size_t			 map_size;
	vm_page_t			 first_md_page;
	void				*md_image;
};

#endif

#ifndef _KERNEL

__BEGIN_DECLS
int	kexec_load(uint64_t, unsigned long, struct kexec_segment *, unsigned long);
__END_DECLS

#else

void	kexec_reboot_md(struct kexec_image *);
int	kexec_load_md(struct kexec_image *);

#endif

#endif
