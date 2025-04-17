/*-
 * Copyright (c) 2024, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sys/efi.h>
#include <machine/metadata.h>

/* Note, we mix and match FreeBSD types and EFI standard defined types */

typedef void (*efi_map_entry_cb)(struct efi_md *, void *argp);

struct preloaded_file;

bool efi_read_from_pa(uint64_t pa, uint32_t map_size, uint32_t desc_size, uint32_t vers);
void efi_read_from_sysfs(void);
void efi_set_systbl(uint64_t tbl);
void foreach_efi_map_entry(struct efi_map_header *efihdr, efi_map_entry_cb cb, void *argp);
void print_efi_map(struct efi_map_header *efihdr);
void efi_bi_loadsmap(struct preloaded_file *kfp);

extern uint32_t efi_map_size;
extern vm_paddr_t efi_map_phys_src;	/* From DTB */
extern vm_paddr_t efi_map_phys_dst;	/* From our memory map metadata module */
