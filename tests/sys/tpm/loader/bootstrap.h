/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#ifndef TPM_TEST_BOOTSTRAP_H
#define TPM_TEST_BOOTSTRAP_H

/* Only the loader metadata list is needed by efitcg.c. */
struct file_metadata {
	size_t md_size;
	uint16_t md_type;
	vm_offset_t md_addr;
	struct file_metadata *md_next;
	char md_data[1];
};

struct preloaded_file {
	struct file_metadata *f_metadata;
};

#endif
