/*-
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef KBOOT_H
#define KBOOT_H

#define DEVT_HOSTDISK 1234

bool has_acpi(void);
vm_offset_t acpi_rsdp(void);

void do_init(void);

/* Per-platform fdt fixup */
void fdt_arch_fixups(void *fdtp);

uint64_t kboot_get_phys_load_segment(void);
uint8_t kboot_get_kernel_machine_bits(void);

/* hostdisk.c */
extern const char *hostfs_root;
void hostdisk_zfs_probe(void);
bool hostdisk_zfs_find_default(void);

/* util.c */
bool file2str(const char *fn, char *buffer, size_t buflen);
bool file2u64(const char *fn, uint64_t *val);

#endif /* KBOOT_H */
