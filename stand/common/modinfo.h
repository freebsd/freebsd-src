/*-
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef COMMON_MODINFO_H
#define COMMON_MODINFO_H

vm_offset_t md_copymodules(vm_offset_t addr, bool kern64);

#endif /* COMMON_MODINFO_H */
