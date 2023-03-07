/*-
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef KBOOT_H
#define KBOOT_H

#define DEVT_HOSTDISK 1234

struct memory_segments
{
	uint64_t	start;
	uint64_t	end;
	uint64_t	type;	/* MD defined */
};

bool enumerate_memory_arch(void);
struct preloaded_file;
void bi_loadsmap(struct preloaded_file *kfp);

bool has_acpi(void);
vm_offset_t acpi_rsdp(void);

void do_init(void);

/* Per-platform fdt fixup */
void fdt_arch_fixups(void *fdtp);

uint64_t kboot_get_phys_load_segment(void);
uint8_t kboot_get_kernel_machine_bits(void);

/* main.c */
void kboot_kseg_get(int *nseg, void **ptr);

/* hostdisk.c */
extern const char *hostfs_root;
const char *hostdisk_gen_probe(void);
void hostdisk_zfs_probe(void);
bool hostdisk_zfs_find_default(void);

/* seg.c */
#define SYSTEM_RAM 1
void init_avail(void);
void need_avail(int n);
void add_avail(uint64_t start, uint64_t end, uint64_t type);
void remove_avail(uint64_t start, uint64_t end, uint64_t type);
uint64_t first_avail(uint64_t align, uint64_t min_size, uint64_t type);
void print_avail(void);
bool populate_avail_from_iomem(void);
uint64_t space_avail(uint64_t start);

/* util.c */
bool file2str(const char *fn, char *buffer, size_t buflen);
bool file2u64(const char *fn, uint64_t *val);

#endif /* KBOOT_H */
