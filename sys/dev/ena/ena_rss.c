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

#include <sys/cdefs.h>
#include "opt_rss.h"

#include "ena_rss.h"

/*
 * This function should generate unique key for the whole driver.
 * If the key was already genereated in the previous call (for example
 * for another adapter), then it should be returned instead.
 */
void
ena_rss_key_fill(void *key, size_t size)
{
	static bool key_generated;
	static uint8_t default_key[ENA_HASH_KEY_SIZE];

	KASSERT(size <= ENA_HASH_KEY_SIZE,
	    ("Requested more bytes than ENA RSS key can hold"));

	if (!key_generated) {
		arc4random_buf(default_key, ENA_HASH_KEY_SIZE);
		key_generated = true;
	}

	memcpy(key, default_key, size);
}

/*
 * ENA HW expects the key to be in reverse-byte order.
 */
static void
ena_rss_reorder_hash_key(u8 *reordered_key, const u8 *key, size_t key_size)
{
	int i;

	key = key + key_size - 1;

	for (i = 0; i < key_size; ++i)
		*reordered_key++ = *key--;
}

int
ena_rss_set_hash(struct ena_com_dev *ena_dev, const u8 *key)
{
	enum ena_admin_hash_functions ena_func = ENA_ADMIN_TOEPLITZ;
	u8 hw_key[ENA_HASH_KEY_SIZE];

	ena_rss_reorder_hash_key(hw_key, key, ENA_HASH_KEY_SIZE);

	return (ena_com_fill_hash_function(ena_dev, ena_func, hw_key,
	    ENA_HASH_KEY_SIZE, 0x0));
}

int
ena_rss_get_hash_key(struct ena_com_dev *ena_dev, u8 *key)
{
	u8 hw_key[ENA_HASH_KEY_SIZE];
	int rc;

	rc = ena_com_get_hash_key(ena_dev, hw_key);
	if (rc != 0)
		return rc;

	ena_rss_reorder_hash_key(key, hw_key, ENA_HASH_KEY_SIZE);

	return (0);
}

static int
ena_rss_init_default(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	device_t dev = adapter->pdev;
	int qid, rc, i;

	rc = ena_com_rss_init(ena_dev, ENA_RX_RSS_TABLE_LOG_SIZE);
	if (unlikely(rc != 0)) {
		ena_log(dev, ERR, "Cannot init indirect table\n");
		return (rc);
	}

	for (i = 0; i < ENA_RX_RSS_TABLE_SIZE; i++) {
#ifdef RSS
		qid = rss_get_indirection_to_bucket(i) % adapter->num_io_queues;
#else
		qid = i % adapter->num_io_queues;
#endif
		rc = ena_com_indirect_table_fill_entry(ena_dev, i,
		    ENA_IO_RXQ_IDX(qid));
		if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
			ena_log(dev, ERR, "Cannot fill indirect table\n");
			goto err_rss_destroy;
		}
	}


#ifdef RSS
	uint8_t rss_algo = rss_gethashalgo();
	if (rss_algo == RSS_HASH_TOEPLITZ) {
		uint8_t hash_key[RSS_KEYSIZE];

		rss_getkey(hash_key);
		rc = ena_rss_set_hash(ena_dev, hash_key);
	} else
#endif
		rc = ena_com_fill_hash_function(ena_dev, ENA_ADMIN_TOEPLITZ,
		    NULL, ENA_HASH_KEY_SIZE, 0x0);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
		ena_log(dev, ERR, "Cannot fill hash function\n");
		goto err_rss_destroy;
	}

	rc = ena_com_set_default_hash_ctrl(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
		ena_log(dev, ERR, "Cannot fill hash control\n");
		goto err_rss_destroy;
	}

	rc = ena_rss_indir_init(adapter);

	return (rc == EOPNOTSUPP ? 0 : rc);

err_rss_destroy:
	ena_com_rss_destroy(ena_dev);
	return (rc);
}

/* Configure the Rx forwarding */
int
ena_rss_configure(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	int rc;

	/* In case the RSS table was destroyed */
	if (!ena_dev->rss.tbl_log_size) {
		rc = ena_rss_init_default(adapter);
		if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
			ena_log(adapter->pdev, ERR,
			    "WARNING: RSS was not properly re-initialized,"
			    " it will affect bandwidth\n");
			ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_RSS_ACTIVE, adapter);
			return (rc);
		}
	}

	/* Set indirect table */
	rc = ena_com_indirect_table_set(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP)))
		return (rc);

	/* Configure hash function (if supported) */
	rc = ena_com_set_hash_function(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP)))
		return (rc);

	/* Configure hash inputs (if supported) */
	rc = ena_com_set_hash_ctrl(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP)))
		return (rc);

	return (0);
}

static void
ena_rss_init_default_deferred(void *arg)
{
	struct ena_adapter *adapter;
	devclass_t dc;
	int max;
	int rc;

	dc = devclass_find("ena");
	if (unlikely(dc == NULL)) {
		ena_log_raw(ERR, "SYSINIT: %s: No devclass ena\n", __func__);
		return;
	}

	max = devclass_get_maxunit(dc);
	while (max-- >= 0) {
		adapter = devclass_get_softc(dc, max);
		if (adapter != NULL) {
			rc = ena_rss_init_default(adapter);
			ENA_FLAG_SET_ATOMIC(ENA_FLAG_RSS_ACTIVE, adapter);
			if (unlikely(rc != 0)) {
				ena_log(adapter->pdev, WARN,
				    "WARNING: RSS was not properly initialized,"
				    " it will affect bandwidth\n");
				ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_RSS_ACTIVE,
				    adapter);
			}
		}
	}
}
SYSINIT(ena_rss_init, SI_SUB_KICK_SCHEDULER, SI_ORDER_SECOND,
    ena_rss_init_default_deferred, NULL);

int
ena_rss_indir_get(struct ena_adapter *adapter, uint32_t *table)
{
	int rc, i;

	rc = ena_com_indirect_table_get(adapter->ena_dev, table);
	if (rc != 0) {
		if (rc == EOPNOTSUPP)
			device_printf(adapter->pdev,
			    "Reading from indirection table not supported\n");
		else
			device_printf(adapter->pdev,
			    "Unable to get indirection table\n");
		return (rc);
	}

	for (i = 0; i < ENA_RX_RSS_TABLE_SIZE; ++i)
		table[i] = ENA_IO_RXQ_IDX_TO_COMBINED_IDX(table[i]);

	return (0);
}

int
ena_rss_indir_set(struct ena_adapter *adapter, uint32_t *table)
{
	int rc, i;

	for (i = 0; i < ENA_RX_RSS_TABLE_SIZE; ++i) {
		rc = ena_com_indirect_table_fill_entry(adapter->ena_dev, i,
		    ENA_IO_RXQ_IDX(table[i]));
		if (rc != 0) {
			device_printf(adapter->pdev,
			    "Cannot fill indirection table entry %d\n", i);
			return (rc);
		}
	}

	rc = ena_com_indirect_table_set(adapter->ena_dev);
	if (rc == EOPNOTSUPP)
		device_printf(adapter->pdev,
		    "Writing to indirection table not supported\n");
	else if (rc != 0)
		device_printf(adapter->pdev, "Cannot set indirection table\n");

	return (rc);
}

int
ena_rss_indir_init(struct ena_adapter *adapter)
{
	struct ena_indir *indir = adapter->rss_indir;
	int rc;

	if (indir == NULL)
		adapter->rss_indir = indir = malloc(sizeof(struct ena_indir),
		    M_DEVBUF, M_WAITOK | M_ZERO);

	rc = ena_rss_indir_get(adapter, indir->table);
	if (rc != 0) {
		free(adapter->rss_indir, M_DEVBUF);
		adapter->rss_indir = NULL;

		return (rc);
	}

	ena_rss_copy_indir_buf(indir->sysctl_buf, indir->table);

	return (0);
}
