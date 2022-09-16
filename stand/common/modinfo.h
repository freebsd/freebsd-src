/*-
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef COMMON_MODINFO_H
#define COMMON_MODINFO_H

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
    if (c)					\
        archsw.arch_copyin(mm->md_data, a, mm->md_size);\
    a += MOD_ALIGN(mm->md_size);		\
}

#define MOD_END(a, c) {				\
    COPY32(MODINFO_END, a, c);			\
    COPY32(0, a, c);				\
}

vm_offset_t md_copymodules(vm_offset_t addr, bool kern64);

#endif /* COMMON_MODINFO_H */
