/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2026 Broadcom Inc. All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "bnxt.h"
#include "bnxt_coredump.h"
#include "hsi_struct_def.h"


/* Free crash dump memory */
void
bnxt_free_crash_dump_mem(struct bnxt_softc *softc)
{
	if (softc->fw_crash_mem) {
		bnxt_free_ctx_pg_tbls(softc, softc->fw_crash_mem);
		free(softc->fw_crash_mem, M_DEVBUF);
		softc->fw_crash_len = 0;
		softc->fw_crash_mem = NULL;
	}
}

/* Allocate crash dump memory */
int
bnxt_alloc_crash_dump_mem(struct bnxt_softc *softc)
{
	uint32_t mem_size = 0;
	int rc;

	if (!(softc->fw_dbg_cap & BNXT_FW_DBG_CAP_CRASHDUMP_HOST))
		return (0);

	rc = bnxt_hwrm_get_dump_len(softc, BNXT_DUMP_CRASH, &mem_size);
	if (rc)
		return (rc);

	mem_size = roundup(mem_size, 4);

	if (softc->fw_crash_mem && mem_size == softc->fw_crash_len)
		return (0);

	bnxt_free_crash_dump_mem(softc);

	softc->fw_crash_mem = malloc(sizeof(*softc->fw_crash_mem),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!softc->fw_crash_mem)
		return (-ENOMEM);

	rc = bnxt_alloc_ctx_pg_tbls(softc, softc->fw_crash_mem,
				    mem_size, 1, NULL);
	if (rc) {
		bnxt_free_crash_dump_mem(softc);
		return (rc);
	}

	softc->fw_crash_len = mem_size;
	return (0);
}

/* Copy crash data from ring memory */
static uint32_t
bnxt_copy_crash_data(struct bnxt_ring_mem_info *rmem, void *buf,
		     uint32_t dump_len)
{
	uint32_t data_copied = 0;
	uint32_t data_len;
	int i;

	for (i = 0; i < rmem->nr_pages; i++) {
		if (!rmem->pg_arr[i].idi_vaddr)
			continue;
		data_len = rmem->page_size;
		if (data_copied + data_len > dump_len)
			data_len = dump_len - data_copied;
		memcpy((uint8_t *)buf + data_copied,
		    rmem->pg_arr[i].idi_vaddr, data_len);
		data_copied += data_len;
		if (data_copied >= dump_len)
			break;
	}
	return (data_copied);
}

/* Copy crash dump from DDR memory */
static int
bnxt_copy_crash_dump(struct bnxt_softc *softc, void *buf, uint32_t dump_len)
{
	struct bnxt_ring_mem_info *rmem;
	uint32_t offset = 0;

	if (!softc->fw_crash_mem)
		return (-ENOENT);

	rmem = &softc->fw_crash_mem->ring_mem;

	if (rmem->depth > 1) {
		int i;

		for (i = 0; i < rmem->nr_pages; i++) {
			struct bnxt_ctx_pg_info *pg_tbl;

			pg_tbl = softc->fw_crash_mem->ctx_pg_tbl[i];
			if (!pg_tbl)
				continue;
			offset += bnxt_copy_crash_data(&pg_tbl->ring_mem,
						       (uint8_t *)buf + offset,
						       dump_len - offset);
			if (offset >= dump_len)
				break;
		}
	} else {
		bnxt_copy_crash_data(rmem, buf, dump_len);
	}

	return (0);
}

/* Check if crash dump is available */
static bool
bnxt_crash_dump_avail(struct bnxt_softc *softc)
{
	uint32_t sig = 0;

	/* First 4 bytes(signature) of crash dump is always non-zero */
	bnxt_copy_crash_dump(softc, &sig, sizeof(uint32_t));
	return (!!sig);
}

/* Get coredump */
int
bnxt_get_coredump(struct bnxt_softc *softc, uint16_t dump_type, void *buf,
		  uint32_t *dump_len)
{
	if (dump_type == BNXT_DUMP_CRASH &&
	    softc->fw_dbg_cap & BNXT_FW_DBG_CAP_CRASHDUMP_HOST) {
		return (bnxt_copy_crash_dump(softc, buf, *dump_len));
	} else {
		/* Other dump types not implemented yet */
		return (-EOPNOTSUPP);
	}
}

/* Get coredump length */
uint32_t
bnxt_get_coredump_length(struct bnxt_softc *softc, uint16_t dump_type)
{
	uint32_t len = 0;

	if (dump_type == BNXT_DUMP_CRASH &&
	    softc->fw_dbg_cap & BNXT_FW_DBG_CAP_CRASHDUMP_HOST &&
	    softc->fw_crash_mem) {
		if (!bnxt_crash_dump_avail(softc))
			return (0);

		return (softc->fw_crash_len);
	}

	if (bnxt_hwrm_get_dump_len(softc, dump_type, &len))
		return (0);

	return (len);
}
