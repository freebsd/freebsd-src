/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2021 Amazon.com, Inc. or its affiliates.
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
 * $FreeBSD$
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

	KASSERT(size <= ENA_HASH_KEY_SIZE, ("Requested more bytes than ENA RSS key can hold"));

	if (!key_generated) {
		arc4random_buf(default_key, ENA_HASH_KEY_SIZE);
		key_generated = true;
	}

	memcpy(key, default_key, size);
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
		qid = i % adapter->num_io_queues;
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
		rc = ena_com_fill_hash_function(ena_dev, ENA_ADMIN_TOEPLITZ,
		    hash_key, RSS_KEYSIZE, 0xFFFFFFFF);
	} else
#endif
	rc = ena_com_fill_hash_function(ena_dev, ENA_ADMIN_CRC32, NULL,
	    ENA_HASH_KEY_SIZE, 0xFFFFFFFF);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
		ena_log(dev, ERR, "Cannot fill hash function\n");
		goto err_rss_destroy;
	}

	rc = ena_com_set_default_hash_ctrl(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
		ena_log(dev, ERR, "Cannot fill hash control\n");
		goto err_rss_destroy;
	}

	return (0);

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
				ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_RSS_ACTIVE, adapter);
			}
		}
	}
}
SYSINIT(ena_rss_init, SI_SUB_KICK_SCHEDULER, SI_ORDER_SECOND, ena_rss_init_default_deferred, NULL);
