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
 */
#ifndef _LINUXKPI_ASM_PGTABLE_H_
#define	_LINUXKPI_ASM_PGTABLE_H_

#include <linux/page.h>

typedef unsigned long	pteval_t;
typedef unsigned long	pmdval_t;
typedef unsigned long	pudval_t;
typedef unsigned long	pgdval_t;
typedef unsigned long	pgprotval_t;
typedef struct page *pgtable_t;

#define	pgprot_decrypted(prot)	(prot)

#if defined(__i386__) || defined(__amd64__)
#define	_PAGE_BIT_PRESENT	0
#define	_PAGE_BIT_RW		1
#define	_PAGE_BIT_USER		2
#define	_PAGE_BIT_PWT		3
#define	_PAGE_BIT_PCD		4
#define	_PAGE_BIT_PAT		7

#define	_PAGE_PRESENT	(((pteval_t) 1) << _PAGE_BIT_PRESENT)
#define	_PAGE_RW	(((pteval_t) 1) << _PAGE_BIT_RW)
#define	_PAGE_PWT	(((pteval_t) 1) << _PAGE_BIT_PWT)
#define	_PAGE_PCD	(((pteval_t) 1) << _PAGE_BIT_PCD)
#define	_PAGE_PAT	(((pteval_t) 1) << _PAGE_BIT_PAT)

/*
 * On Linux, the value of `PMD_SHIFT` is hard-coded to 21. This corresponds to
 * the FreeBSD `PDRSHIFT` constant.
 */
#define	PMD_SHIFT	PDRSHIFT

#elif defined(__aarch64__)

/*
 * On Linux, the value of `PMD_SHIFT` is computed from `CONFIG_PGTABLE_LEVELS`.
 * The result corresponds to one of the FreeBSD `L*_SHIFT` constants. Here, we
 * take the value 21 computed from `CONFIG_PGTABLE_LEVELS = 4`, the default on
 * aarch64, which equals to `L2_SHIFT`.
 */
#define	PMD_SHIFT	L2_SHIFT

#elif defined(__powerpc__)

/*
 * On Linux, the value of `PMD_SHIFT` is the addition of `PAGE_SHIFT` and
 * `PTE_INDEX_SIZE` (hard-coded to 9). The result corresponds to the FreeBSD
 * `L3_PAGE_SIZE_SHIFT` constant.
 */
#define	PMD_SHIFT	L3_PAGE_SIZE_SHIFT

#elif defined(__riscv)

/*
 * On Linux, the value of `PMD_SHIFT` is hard-coded to 21. This corresponds to
 * the FreeBSD `L2_SHIFT` constant.
 */
#define	PMD_SHIFT	L2_SHIFT

#endif

#endif	/* _LINUXKPI_ASM_PGTABLE_H_ */
