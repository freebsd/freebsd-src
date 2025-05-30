/*-
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef COMMON_MODINFO_H
#define COMMON_MODINFO_H

extern const char md_modtype[];
extern const char md_kerntype[];
extern const char md_modtype_obj[];
extern const char md_kerntype_mb[];

int md_load(char *args, vm_offset_t *modulep, vm_offset_t *dtb);
int md_load64(char *args, vm_offset_t *modulep, vm_offset_t *dtb);

vm_offset_t md_copymodules(vm_offset_t addr, bool kern64);
vm_offset_t md_copyenv(vm_offset_t addr);
vm_offset_t md_align(vm_offset_t addr);

#endif /* COMMON_MODINFO_H */
