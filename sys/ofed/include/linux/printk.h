/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _FBSD_PRINTK_H_
#define	_FBSD_PRINTK_H_

/* GID printing macros */
#define	GID_PRINT_FMT			"%.4x:%.4x:%.4x:%.4x:%.4x:%.4x:%.4x:%.4x"
#define	GID_PRINT_ARGS(gid_raw)		htons(((u16 *)gid_raw)[0]), htons(((u16 *)gid_raw)[1]),\
					htons(((u16 *)gid_raw)[2]), htons(((u16 *)gid_raw)[3]),\
					htons(((u16 *)gid_raw)[4]), htons(((u16 *)gid_raw)[5]),\
					htons(((u16 *)gid_raw)[6]), htons(((u16 *)gid_raw)[7])

#endif					/* _FBSD_PRINTK_H */
