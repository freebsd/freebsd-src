/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
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
#ifndef	_LINUXKPI_LINUX_ERR_H_
#define	_LINUXKPI_LINUX_ERR_H_

#include <sys/types.h>

#include <linux/compiler.h>

#define MAX_ERRNO	4095

#define IS_ERR_VALUE(x) unlikely((x) >= (uintptr_t)-MAX_ERRNO)

static inline void *
ERR_PTR(long error)
{
	return (void *)(intptr_t)error;
}

static inline long
PTR_ERR(const void *ptr)
{
	return (intptr_t)ptr;
}

static inline bool
IS_ERR(const void *ptr)
{
	return IS_ERR_VALUE((uintptr_t)ptr);
}

static inline bool
IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || IS_ERR_VALUE((uintptr_t)ptr);
}

static inline void *
ERR_CAST(const void *ptr)
{
	return __DECONST(void *, ptr);
}

static inline int
PTR_ERR_OR_ZERO(const void *ptr)
{
	if (IS_ERR(ptr))
		return PTR_ERR(ptr);
	else
		return 0;
}

#define PTR_RET(p) PTR_ERR_OR_ZERO(p)

#endif	/* _LINUXKPI_LINUX_ERR_H_ */
