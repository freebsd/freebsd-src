/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Juniper Networks, Inc.
 * Copyright (c) 2022 Klara, Inc.
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

#ifndef	_FS_TARFS_TARFS_DBG_H_
#define	_FS_TARFS_TARFS_DBG_H_

#ifndef _KERNEL
#error Should only be included by kernel
#endif

#ifdef	TARFS_DEBUG
extern int tarfs_debug;

#define	TARFS_DEBUG_ALLOC	0x01
#define	TARFS_DEBUG_CHECKSUM	0x02
#define	TARFS_DEBUG_FS		0x04
#define	TARFS_DEBUG_LOOKUP	0x08
#define	TARFS_DEBUG_VNODE	0x10
#define	TARFS_DEBUG_IO		0x20
#define	TARFS_DEBUG_ZIO		0x40
#define	TARFS_DEBUG_ZIDX	0x80
#define	TARFS_DEBUG_MAP		0x100
#define	TARFS_DEBUG_BOUNCE	0x200

#define	TARFS_DPF(category, fmt, ...)					\
	do {								\
		if ((tarfs_debug & TARFS_DEBUG_##category) != 0)	\
			printf(fmt, ## __VA_ARGS__);			\
	} while (0)
#define	TARFS_DPF_IFF(category, cond, fmt, ...)				\
	do {								\
		if ((cond)						\
		    && (tarfs_debug & TARFS_DEBUG_##category) != 0)	\
			printf(fmt, ## __VA_ARGS__);			\
	} while (0)
#else
#define	TARFS_DPF(category, fmt, ...)
#define	TARFS_DPF_IFF(category, cond, fmt, ...)
#endif

#endif	/* _FS_TARFS_TARFS_DBG_H_ */
