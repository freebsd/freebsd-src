/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025, Michal Meloun <mmel@freebsd.org>
 *
 */

#ifndef __ARM_IFUNC_H
#define	__ARM_IFUNC_H

#define	DEFINE_IFUNC(qual, ret_type, name, args)			\
    static ret_type (*name##_resolver(void))args __used;		\
    qual ret_type name args __attribute__((ifunc(#name "_resolver")));	\
    static ret_type (*name##_resolver(void))args

#ifdef __not_yet__
#define	DEFINE_UIFUNC(qual, ret_type, name, args)			\
    static ret_type (*name##_resolver(uint32_t, uint32_t, uint32_t,	\
	uint32_t))args __used;						\
    qual ret_type name args __attribute__((ifunc(#name "_resolver")));	\
    static ret_type (*name##_resolver(					\
	uint32_t elf_hwcap __unused,					\
	uint32_t elf_hwcap2 __unused,					\
	uint32_t arg3 __unused,						\
	uint32_t arg4 __unused))args
#endif

#endif
