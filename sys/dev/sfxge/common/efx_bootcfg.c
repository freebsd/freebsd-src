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
 * NOTE: This is larger than the Medford per-PF bootcfg sector.
 */
#define	BOOTCFG_MAX_SIZE 0x1000

#define	DHCP_END ((uint8_t)0xff)
#define	DHCP_PAD ((uint8_t)0)


/* Report size and offset of bootcfg sector in NVRAM partition. */
static	__checkReturn		efx_rc_t
efx_bootcfg_sector(
	__in			efx_nic_t *enp,
	__out			size_t *offsetp,
	__out			size_t *max_sizep)
{
	size_t max_size;
	size_t offset;
	int rc;

	switch (enp->en_family) {
#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		max_size = BOOTCFG_MAX_SIZE;
		offset = 0;
		break;
#endif /* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		max_size = BOOTCFG_MAX_SIZE;
		offset = 0;
		break;
#endif /* EFSYS_OPT_HUNTINGTON */

#if EFSYS_OPT_MEDFORD
	case EFX_FAMILY_MEDFORD: {
		efx_nic_cfg_t *encp = &(enp->en_nic_cfg);

		/* Shared partition (array indexed by PF) */
		max_size = 0x0800;
		offset = max_size * encp->enc_pf;
		break;
	}
#endif /* EFSYS_OPT_MEDFORD */

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}
	EFSYS_ASSERT3U(max_size, <=, BOOTCFG_MAX_SIZE);

	*offsetp = offset;
	*max_sizep = max_size;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}


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
	size_t partn_length;
	size_t sector_length;
	size_t sector_offset;
	efx_rc_t rc;

	rc = efx_nvram_size(enp, EFX_NVRAM_BOOTROM_CFG, &partn_length);
	if (rc != 0)
		goto fail1;

	/* The bootcfg sector may be stored in a (larger) shared partition */
	rc = efx_bootcfg_sector(enp, &sector_offset, &sector_length);
	if (rc != 0)
		goto fail2;

	if (sector_length > BOOTCFG_MAX_SIZE)
		sector_length = BOOTCFG_MAX_SIZE;

	if (sector_offset + sector_length > partn_length) {
		/* Partition is too small */
		rc = EFBIG;
		goto fail3;
	}

	/*
	 * We need to read the entire BOOTCFG sector to ensure we read all the
	 * tags, because legacy bootcfg sectors are not guaranteed to end with
	 * a DHCP_END character. If the user hasn't supplied a sufficiently
	 * large buffer then use our own buffer.
	 */
	if (sector_length > size) {
		EFSYS_KMEM_ALLOC(enp->en_esip, sector_length, payload);
		if (payload == NULL) {
			rc = ENOMEM;
			goto fail4;
		}
	} else
		payload = (uint8_t *)data;

	if ((rc = efx_nvram_rw_start(enp, EFX_NVRAM_BOOTROM_CFG, NULL)) != 0)
		goto fail5;

	rc = efx_nvram_read_chunk(enp, EFX_NVRAM_BOOTROM_CFG, sector_offset,
				    (caddr_t)payload, sector_length);

	efx_nvram_rw_finish(enp, EFX_NVRAM_BOOTROM_CFG);

	if (rc != 0)
		goto fail6;

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
		goto fail7;
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

fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
	if (sector_length > size)
		EFSYS_KMEM_FREE(enp->en_esip, sector_length, payload);
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

				efx_rc_t
efx_bootcfg_write(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	uint8_t *partn_data;
	uint8_t checksum;
	size_t partn_length;
	size_t sector_length;
	size_t sector_offset;
	size_t used_bytes;
	efx_rc_t rc;

	rc = efx_nvram_size(enp, EFX_NVRAM_BOOTROM_CFG, &partn_length);
	if (rc != 0)
		goto fail1;

	/* The bootcfg sector may be stored in a (larger) shared partition */
	rc = efx_bootcfg_sector(enp, &sector_offset, &sector_length);
	if (rc != 0)
		goto fail2;

	if (sector_length > BOOTCFG_MAX_SIZE)
		sector_length = BOOTCFG_MAX_SIZE;

	if (sector_offset + sector_length > partn_length) {
		/* Partition is too small */
		rc = EFBIG;
		goto fail3;
	}

	if ((rc = efx_bootcfg_verify(enp, data, size, &used_bytes)) != 0)
		goto fail4;

	/* The caller *must* terminate their block with a DHCP_END character */
	if ((used_bytes < 2) || ((uint8_t)data[used_bytes - 1] != DHCP_END)) {
		/* Block too short or DHCP_END missing */
		rc = ENOENT;
		goto fail5;
	}

	/* Check that the hardware has support for this much data */
	if (used_bytes > MIN(sector_length, BOOTCFG_MAX_SIZE)) {
		rc = ENOSPC;
		goto fail6;
	}

	/*
	 * If the BOOTCFG sector is stored in a shared partition, then we must
	 * read the whole partition and insert the updated bootcfg sector at the
	 * correct offset.
	 */
	EFSYS_KMEM_ALLOC(enp->en_esip, partn_length, partn_data);
	if (partn_data == NULL) {
		rc = ENOMEM;
		goto fail7;
	}

	rc = efx_nvram_rw_start(enp, EFX_NVRAM_BOOTROM_CFG, NULL);
	if (rc != 0)
		goto fail8;

	/* Read the entire partition */
	rc = efx_nvram_read_chunk(enp, EFX_NVRAM_BOOTROM_CFG, 0,
				    (caddr_t)partn_data, partn_length);
	if (rc != 0)
		goto fail9;

	/*
	 * Insert the BOOTCFG sector into the partition, Zero out all data after
	 * the DHCP_END tag, and adjust the checksum.
	 */
	(void) memset(partn_data + sector_offset, 0x0, sector_length);
	(void) memcpy(partn_data + sector_offset, data, used_bytes);

	checksum = efx_bootcfg_csum(enp, data, used_bytes);
	partn_data[sector_offset] -= checksum;

	if ((rc = efx_nvram_erase(enp, EFX_NVRAM_BOOTROM_CFG)) != 0)
		goto fail10;

	if ((rc = efx_nvram_write_chunk(enp, EFX_NVRAM_BOOTROM_CFG,
		    0, partn_data, partn_length)) != 0) {
		goto fail11;
	}

	efx_nvram_rw_finish(enp, EFX_NVRAM_BOOTROM_CFG);

	EFSYS_KMEM_FREE(enp->en_esip, partn_length, partn_data);

	return (0);

fail11:
	EFSYS_PROBE(fail11);
fail10:
	EFSYS_PROBE(fail10);
fail9:
	EFSYS_PROBE(fail9);

	efx_nvram_rw_finish(enp, EFX_NVRAM_BOOTROM_CFG);
fail8:
	EFSYS_PROBE(fail8);

	EFSYS_KMEM_FREE(enp->en_esip, partn_length, partn_data);
fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
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
