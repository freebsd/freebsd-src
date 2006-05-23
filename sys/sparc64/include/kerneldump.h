/*-
 * Copyright (c) 2002 by Thomas Moestl <tmm@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sparc64/include/kerneldump.h,v 1.2 2003/04/08 06:35:08 jake Exp $
 */

#ifndef _MACHINE_KERNELDUMP_H_
#define	_MACHINE_KERNELDUMP_H_

struct sparc64_dump_reg {
	vm_paddr_t	dr_pa;
	vm_offset_t	dr_size;
	vm_offset_t	dr_offs;
};

/*
 * Kernel dump format for sparc64. This does not use ELF because it is of no
 * avail (only libkvm knows how to translate addresses properly anyway) and
 * would require some ugly hacks.
 */
struct sparc64_dump_hdr {
	vm_offset_t	dh_hdr_size;
	vm_paddr_t	dh_tsb_pa;
	vm_size_t	dh_tsb_size;
	vm_size_t	dh_tsb_mask;
	int		dh_nregions;
	struct sparc64_dump_reg	dh_regions[];
};

#endif /* _MACHINE_KERNELDUMP_H_ */
