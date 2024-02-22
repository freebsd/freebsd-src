/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006, 2008 Stanislav Sedov <stas@FreeBSD.org>.
 * All rights reserved.
 * Copyright (c) 2012 Andriy Gapon <avg@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#include <sys/param.h>
#include <sys/systm.h>

#include <x86/ucode.h>

#ifndef _KERNEL
#include <err.h>

#include "cpucontrol.h"
#endif


#ifdef _KERNEL
#define WARNX(level, ...)					\
	if (bootverbose) {					\
		printf(__VA_ARGS__);				\
		printf("\n");					\
	}
#endif

/*
 * AMD family 10h and later.
 */
typedef struct amd_10h_fw_header {
	uint32_t	data_code;
	uint32_t	patch_id;
	uint16_t	mc_patch_data_id;
	uint8_t		mc_patch_data_len;
	uint8_t		init_flag;
	uint32_t	mc_patch_data_checksum;
	uint32_t	nb_dev_id;
	uint32_t	sb_dev_id;
	uint16_t	processor_rev_id;
	uint8_t		nb_rev_id;
	uint8_t		sb_rev_id;
	uint8_t		bios_api_rev;
	uint8_t		reserved1[3];
	uint32_t	match_reg[8];
} amd_10h_fw_header_t;

typedef struct equiv_cpu_entry {
	uint32_t	installed_cpu;
	uint32_t	fixed_errata_mask;
	uint32_t	fixed_errata_compare;
	uint16_t	equiv_cpu;
	uint16_t	res;
} equiv_cpu_entry_t;

typedef struct section_header {
	uint32_t	type;
	uint32_t	size;
} section_header_t;

typedef struct container_header {
	uint32_t	magic;
} container_header_t;

#define	AMD_10H_MAGIC			0x414d44
#define AMD_10H_EQUIV_TABLE_TYPE	0
#define AMD_10H_uCODE_TYPE		1

/*
 * NB: the format of microcode update files is not documented by AMD.
 * It has been reverse engineered from studying Coreboot, illumos and Linux
 * source code.
 */
const void *
ucode_amd_find(const char *path, uint32_t signature, uint32_t revision,
    const uint8_t *fw_data, size_t fw_size, size_t *selected_sizep)
{
	const amd_10h_fw_header_t *fw_header;
	const amd_10h_fw_header_t *selected_fw;
	const equiv_cpu_entry_t *equiv_cpu_table;
	const section_header_t *section_header;
	const container_header_t *container_header;
	size_t selected_size;
	uint16_t equiv_id;
	int i;

	WARNX(1, "found cpu family %#x model %#x "
	    "stepping %#x extfamily %#x extmodel %#x.",
	    ((signature >> 8) & 0x0f) + ((signature >> 20) & 0xff),
	    (signature >> 4) & 0x0f,
	    (signature >> 0) & 0x0f, (signature >> 20) & 0xff,
	    (signature >> 16) & 0x0f);
	WARNX(1, "microcode revision %#x", revision);

nextfile:
	WARNX(1, "checking %s for update.", path);
	WARNX(3, "processing next container file");
	if (fw_size <
	    (sizeof(*container_header) + sizeof(*section_header))) {
		WARNX(2, "file too short: %s", path);
		return (NULL);
	}

	container_header = (const container_header_t *)fw_data;
	if (container_header->magic != AMD_10H_MAGIC) {
		WARNX(2, "%s is not a valid amd firmware: bad magic", path);
		return (NULL);
	}
	fw_data += sizeof(*container_header);
	fw_size -= sizeof(*container_header);

	section_header = (const section_header_t *)fw_data;
	if (section_header->type != AMD_10H_EQUIV_TABLE_TYPE) {
		WARNX(2, "%s is not a valid amd firmware: "
		    "first section is not CPU equivalence table", path);
		return (NULL);
	}
	if (section_header->size == 0) {
		WARNX(2, "%s is not a valid amd firmware: "
		    "first section is empty", path);
		return (NULL);
	}
	fw_data += sizeof(*section_header);
	fw_size -= sizeof(*section_header);

	if (section_header->size > fw_size) {
		WARNX(2, "%s is not a valid amd firmware: "
		    "file is truncated", path);
		return (NULL);
	}
	if (section_header->size < sizeof(*equiv_cpu_table)) {
		WARNX(2, "%s is not a valid amd firmware: "
		    "first section is too short", path);
		return (NULL);
	}
	equiv_cpu_table = (const equiv_cpu_entry_t *)fw_data;
	fw_data += section_header->size;
	fw_size -= section_header->size;

	equiv_id = 0;
	for (i = 0; equiv_cpu_table[i].installed_cpu != 0; i++) {
		WARNX(3, "signature 0x%x i %d installed_cpu 0x%x equiv 0x%x",
		      signature, i, equiv_cpu_table[i].installed_cpu,
		      equiv_cpu_table[i].equiv_cpu);
		if (signature == equiv_cpu_table[i].installed_cpu) {
			equiv_id = equiv_cpu_table[i].equiv_cpu;
			WARNX(3, "equiv_id: %x, signature %8x,"
			    " equiv_cpu_table[%d] %8x", equiv_id, signature,
			    i, equiv_cpu_table[i].installed_cpu);
			break;
		}
	}
	if (equiv_id == 0) {
		WARNX(2, "CPU is not found in the equivalence table");
	}

	while (fw_size >= sizeof(*section_header)) {
		section_header = (const section_header_t *)fw_data;
		if (section_header->type == AMD_10H_MAGIC) {
			WARNX(2, "%s next section is actually a new container",
			    path);
			if (selected_fw != NULL)
				goto found;
			else
				goto nextfile;
		}
		fw_data += sizeof(*section_header);
		fw_size -= sizeof(*section_header);
		if (section_header->type != AMD_10H_uCODE_TYPE) {
			WARNX(2, "%s is not a valid amd firmware: "
			    "section has incorrect type", path);
			break;
		}
		if (section_header->size > fw_size) {
			WARNX(2, "%s is not a valid amd firmware: "
			    "file is truncated", path);
			break;
		}
		if (section_header->size < sizeof(*fw_header)) {
			WARNX(2, "%s is not a valid amd firmware: "
			    "section is too short", path);
			break;
		}
		fw_header = (const amd_10h_fw_header_t *)fw_data;
		fw_data += section_header->size;
		fw_size -= section_header->size;

		if (fw_header->processor_rev_id != equiv_id) {
			WARNX(1, "firmware processor_rev_id %x, equiv_id %x",
			    fw_header->processor_rev_id, equiv_id);
			continue; /* different cpu */
		}
		if (fw_header->patch_id <= revision) {
			WARNX(1, "patch_id %x, revision %x",
			    fw_header->patch_id, revision);
			continue; /* not newer revision */
		}
		if (fw_header->nb_dev_id != 0 || fw_header->sb_dev_id != 0) {
			WARNX(2, "Chipset-specific microcode is not supported");
		}

		WARNX(3, "selecting revision: %x", fw_header->patch_id);
		revision = fw_header->patch_id;
		selected_fw = fw_header;
		selected_size = section_header->size;
	}

	if (fw_size != 0) {
		WARNX(2, "%s is not a valid amd firmware: "
		    "file is truncated", path);
		selected_fw = NULL;
	}

found:
	*selected_sizep = selected_size;
	return (selected_fw);
}
