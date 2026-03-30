/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2024 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef ENA_RSS_H
#define ENA_RSS_H

#include "opt_rss.h"

#include <sys/types.h>
#include "ena.h"

#include <net/rss_config.h>

/* Number of decimal digits needed to represent a value in range [0, n-1] */
#define ENA_DECIMAL_DIGITS(n) (snprintf(NULL, 0, "%d", n))
/*
 * Per-record size in sysctl_buf: " <index>:<queue>\0"
 * +1 for space/separator, +1 for ':', +1 for null terminator
 */
#define ENA_RSS_RECORD_SIZE(table_size, num_queues) \
	(1 + ENA_DECIMAL_DIGITS(table_size) + 1 + ENA_DECIMAL_DIGITS(num_queues) + 1)

struct ena_indir {
	uint32_t *table;
	/* This is the buffer wired to `rss.indir_table` sysctl. */
	char *sysctl_buf;
	u32 table_size;
	u32 record_size;
};

int ena_rss_set_hash(struct ena_com_dev *ena_dev, const u8 *key);
int ena_rss_get_hash_key(struct ena_com_dev *ena_dev, u8 *key);
int ena_rss_configure(struct ena_adapter *);
int ena_rss_indir_get(struct ena_adapter *adapter, uint32_t *table);
int ena_rss_indir_set(struct ena_adapter *adapter, uint32_t *table);
int ena_rss_indir_init(struct ena_adapter *adapter);

static inline void
ena_rss_copy_indir_buf(char *buf, uint32_t *table, u32 table_size,
    u32 record_size)
{
	int i;

	for (i = 0; i < table_size; ++i) {
		buf += snprintf(buf, record_size, "%s%d:%d",
		    i == 0 ? "" : " ", i, table[i]);
	}
}

static inline u32
ena_get_rss_indirection_table_size(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;

	if (!ena_com_indirection_table_config_supported(ena_dev)) {
		device_printf(adapter->pdev,
		    "RSS indirection table configuration not supported\n");
		return (0);
	}

	return (1UL << ena_dev->rss.tbl_log_size);
}
#endif /* !(ENA_RSS_H) */
