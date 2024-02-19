/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 SRI International
 */

#define STUB_FUNC(f)	\
    void (f)(void);	\
    void (f)(void) { __builtin_trap(); }

STUB_FUNC(elf_aux_info);
