/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2017 Mellanox Technologies, Ltd.
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
 */
#ifndef	_LINUXKPI_LINUX_TYPES_H_
#define	_LINUXKPI_LINUX_TYPES_H_

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <linux/compiler.h>
#include <asm/types.h>

#ifndef __bitwise__
#ifdef __CHECKER__
#define __bitwise__ __attribute__((bitwise))
#else
#define __bitwise__
#endif
#endif

typedef uint16_t __le16;
typedef uint16_t __be16;
typedef uint32_t __le32;
typedef uint32_t __be32;
typedef uint64_t __le64;
typedef uint64_t __be64;

typedef uint16_t __aligned_u16 __aligned(sizeof(uint16_t));
typedef uint32_t __aligned_u32 __aligned(sizeof(uint32_t));
typedef uint64_t __aligned_u64 __aligned(sizeof(uint64_t));

#ifdef _KERNEL
typedef unsigned short ushort;
typedef unsigned int    uint;
#endif
typedef unsigned long ulong;
typedef unsigned gfp_t;
typedef off_t loff_t;
typedef vm_paddr_t resource_size_t;
typedef uint16_t __bitwise__ __sum16;
typedef uint32_t __bitwise__ __wsum;
typedef unsigned long pgoff_t;
typedef unsigned __poll_t;

typedef uint64_t phys_addr_t;

typedef size_t __kernel_size_t;
typedef	unsigned long	kernel_ulong_t;

#define	DECLARE_BITMAP(n, bits)						\
	unsigned long n[howmany(bits, sizeof(long) * 8)]

typedef unsigned long irq_hw_number_t;

#ifndef LIST_HEAD_DEF
#define	LIST_HEAD_DEF
struct list_head {
	struct list_head *next;
	struct list_head *prev;
};
#endif

struct rcu_head {
	void *raw[2];
} __aligned(sizeof(void *));

typedef void (*rcu_callback_t)(struct rcu_head *head);
typedef void (*call_rcu_func_t)(struct rcu_head *head, rcu_callback_t func);
typedef int linux_task_fn_t(void *data);

#endif	/* _LINUXKPI_LINUX_TYPES_H_ */
