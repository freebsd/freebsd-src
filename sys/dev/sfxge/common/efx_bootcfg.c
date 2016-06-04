/*-
 * Copyright (c) 2009-2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_BOOTCFG

/*
 * Maximum size of BOOTCFG block across all nics as understood by SFCgPXE.
 * A multiple of 0x100 so trailing 0xff characters don't contrinbute to the
 * checksum.
 */
#define	BOOTCFG_MAX_SIZE 0x1000

#define	DHCP_END (uint8_t)0xff
#define	DHCP_PAD (uint8_t)0

static	__checkReturn		uint8_t
efx_bootcfg_csum(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	_NOTE(ARGUNUSED(enp))

	unsigned int pos;
	uint8_t checksum = 0;

	for (pos = 0; pos < size; pos++)
		checksum += data[pos];
	return (checksum);
}

static	__checkReturn		efx_rc_t
efx_bootcfg_verify(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__out_opt		size_t *usedp)
{
	size_t offset = 0;
	size_t used = 0;
	efx_rc_t rc;

	/* Start parsing tags immediatly after the checksum */
	for (offset = 1; offset < size; ) {
		uint8_t tag;
		uint8_t length;

		/* Consume tag */
		tag = data[offset];
		if (tag == DHCP_END) {
			offset++;
			used = offset;
			break;
		}
		if (tag == DHCP_PAD) {
			offset++;
			continue;
		}

		/* Consume length */
		if (offset + 1 >= size) {
			rc = ENOSPC;
			goto fail1;
		}
		length = data[offset + 1];

		/* Consume *length */
		if (offset + 1 + length >= size) {
			rc = ENOSPC;
			goto fail2;
		}

		offset += 2 + length;
		used = offset;
	}

	/* Checksum the entire sector, including bytes after any DHCP_END */
	if (efx_bootcfg_csum(enp, data, size) != 0) {
		rc = EINVAL;
		goto fail3;
	}

	if (usedp != NULL)
		*usedp = used;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

				efx_rc_t
efx_bootcfg_read(
	__in			efx_nic_t *enp,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	uint8_t *payload = NULL;
	size_t used_bytes;
	size_t sector_length;
	efx_rc_t rc;

	rc = efx_nvram_size(enp, EFX_NVRAM_BOOTROM_CFG, &sector_length);
	if (rc != 0)
		goto fail1;

	/*
	 * We need to read the entire BOOTCFG area to ensure we read all the
	 * tags, because legacy bootcfg sectors are not guaranteed to end with
	 * a DHCP_END character. If the user hasn't supplied a sufficiently
	 * large buffer then use our own buffer.
	 */
	if (sector_length > BOOTCFG_MAX_SIZE)
		sector_length = BOOTCFG_MAX_SIZE;
	if (sector_length > size) {
		EFSYS_KMEM_ALLOC(enp->en_esip, sector_length, payload);
		if (payload == NULL) {
			rc = ENOMEM;
			goto fail2;
		}
	} else
		payload = (uint8_t *)data;

	if ((rc = efx_nvram_rw_start(enp, EFX_NVRAM_BOOTROM_CFG, NULL)) != 0)
		goto fail3;

	rc = efx_nvram_read_chunk(enp, EFX_NVRAM_BOOTROM_CFG, 0,
				    (caddr_t)payload, sector_length);

	efx_nvram_rw_finish(enp, EFX_NVRAM_BOOTROM_CFG);

	if (rc != 0)
		goto fail4;

	/* Verify that the area is correctly formatted and checksummed */
	rc = efx_bootcfg_verify(enp, (caddr_t)payload, sector_length,
				    &used_bytes);
	if (rc != 0 || used_bytes == 0) {
		payload[0] = (uint8_t)~DHCP_END;
		payload[1] = DHCP_END;
		used_bytes = 2;
	}

	EFSYS_ASSERT(used_bytes >= 2);	/* checksum and DHCP_END */
	EFSYS_ASSERT(used_bytes <= sector_length);

	/*
	 * Legacy bootcfg sectors don't terminate with a DHCP_END character.
	 * Modify the returned payload so it does. BOOTCFG_MAX_SIZE is by
	 * definition large enough for any valid (per-port) bootcfg sector,
	 * so reinitialise the sector if there isn't room for the character.
	 */
	if (payload[used_bytes - 1] != DHCP_END) {
		if (used_bytes + 1 > sector_length) {
			payload[0] = 0;
			used_bytes = 1;
		}

		payload[used_bytes] = DHCP_END;
		++used_bytes;
	}

	/*
	 * Verify that the user supplied buffer is large enough for the
	 * entire used bootcfg area, then copy into the user supplied buffer.
	 */
	if (used_bytes > size) {
		rc = ENOSPC;
		goto fail5;
	}
	if (sector_length > size) {
		memcpy(data, payload, used_bytes);
		EFSYS_KMEM_FREE(enp->en_esip, sector_length, payload);
	}

	/* Zero out the unused portion of the user buffer */
	if (used_bytes < size)
		(void) memset(data + used_bytes, 0, size - used_bytes);

	/*
	 * The checksum includes trailing data after any DHCP_END character,
	 * which we've just modified (by truncation or appending DHCP_END).
	 */
	data[0] -= efx_bootcfg_csum(enp, data, size);

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);

	if (sector_length > size)
		EFSYS_KMEM_FREE(enp->en_esip, sector_length, payload);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

				efx_rc_t
efx_bootcfg_write(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	uint8_t *chunk;
	uint8_t checksum;
	size_t sector_length;
	size_t chunk_length;
	size_t used_bytes;
	size_t offset;
	size_t remaining;
	efx_rc_t rc;

	rc = efx_nvram_size(enp, EFX_NVRAM_BOOTROM_CFG, &sector_length);
	if (rc != 0)
		goto fail1;

	if (sector_length > BOOTCFG_MAX_SIZE)
		sector_length = BOOTCFG_MAX_SIZE;

	if ((rc = efx_bootcfg_verify(enp, data, size, &used_bytes)) != 0)
		goto fail2;

	/* The caller *must* terminate their block with a DHCP_END character */
	EFSYS_ASSERT(used_bytes >= 2);		/* checksum and DHCP_END */
	if ((uint8_t)data[used_bytes - 1] != DHCP_END) {
		rc = ENOENT;
		goto fail3;
	}

	/* Check that the hardware has support for this much data */
	if (used_bytes > MIN(sector_length, BOOTCFG_MAX_SIZE)) {
		rc = ENOSPC;
		goto fail4;
	}

	rc = efx_nvram_rw_start(enp, EFX_NVRAM_BOOTROM_CFG, &chunk_length);
	if (rc != 0)
		goto fail5;

	EFSYS_KMEM_ALLOC(enp->en_esip, chunk_length, chunk);
	if (chunk == NULL) {
		rc = ENOMEM;
		goto fail6;
	}

	if ((rc = efx_nvram_erase(enp, EFX_NVRAM_BOOTROM_CFG)) != 0)
		goto fail7;

	/*
	 * Write the entire sector_length bytes of data in chunks. Zero out
	 * all data following the DHCP_END, and adjust the checksum
	 */
	checksum = efx_bootcfg_csum(enp, data, used_bytes);
	for (offset = 0; offset < sector_length; offset += remaining) {
		remaining = MIN(chunk_length, sector_length - offset);

		/* Fill chunk */
		(void) memset(chunk, 0x0, chunk_length);
		if (offset < used_bytes)
			memcpy(chunk, data + offset,
			    MIN(remaining, used_bytes - offset));

		/* Adjust checksum */
		if (offset == 0)
			chunk[0] -= checksum;

		if ((rc = efx_nvram_write_chunk(enp, EFX_NVRAM_BOOTROM_CFG,
			    offset, (caddr_t)chunk, remaining)) != 0)
			goto fail8;
	}

	efx_nvram_rw_finish(enp, EFX_NVRAM_BOOTROM_CFG);

	EFSYS_KMEM_FREE(enp->en_esip, chunk_length, chunk);

	return (0);

fail8:
	EFSYS_PROBE(fail8);
fail7:
	EFSYS_PROBE(fail7);

	EFSYS_KMEM_FREE(enp->en_esip, chunk_length, chunk);
fail6:
	EFSYS_PROBE(fail6);

	efx_nvram_rw_finish(enp, EFX_NVRAM_BOOTROM_CFG);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_BOOTCFG */
