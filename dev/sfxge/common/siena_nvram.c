/*-
 * Copyright 2009 Solarflare Communications Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efsys.h"
#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_impl.h"

#if EFSYS_OPT_SIENA

#if EFSYS_OPT_VPD || EFSYS_OPT_NVRAM

	__checkReturn		int
siena_nvram_partn_size(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__out			size_t *sizep)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_NVRAM_INFO_IN_LEN,
			    MC_CMD_NVRAM_INFO_OUT_LEN)];
	int rc;

	if ((1 << partn) & ~enp->en_u.siena.enu_partn_mask) {
		rc = ENOTSUP;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_NVRAM_INFO;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_INFO_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_INFO_OUT_LEN;

	MCDI_IN_SET_DWORD(req, NVRAM_INFO_IN_TYPE, partn);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_NVRAM_INFO_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	*sizep = MCDI_OUT_DWORD(req, NVRAM_INFO_OUT_SIZE);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
siena_nvram_partn_lock(
	__in			efx_nic_t *enp,
	__in			unsigned int partn)
{
	efx_mcdi_req_t req;
	uint8_t payload[MC_CMD_NVRAM_UPDATE_START_IN_LEN];
	int rc;

	req.emr_cmd = MC_CMD_NVRAM_UPDATE_START;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_UPDATE_START_IN_LEN;
	EFX_STATIC_ASSERT(MC_CMD_NVRAM_UPDATE_START_OUT_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, NVRAM_UPDATE_START_IN_TYPE, partn);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
siena_nvram_partn_read(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_NVRAM_READ_IN_LEN,
			    MC_CMD_NVRAM_READ_OUT_LEN(SIENA_NVRAM_CHUNK))];
	size_t chunk;
	int rc;

	while (size > 0) {
		chunk = MIN(size, SIENA_NVRAM_CHUNK);

		req.emr_cmd = MC_CMD_NVRAM_READ;
		req.emr_in_buf = payload;
		req.emr_in_length = MC_CMD_NVRAM_READ_IN_LEN;
		req.emr_out_buf = payload;
		req.emr_out_length =
			MC_CMD_NVRAM_READ_OUT_LEN(SIENA_NVRAM_CHUNK);

		MCDI_IN_SET_DWORD(req, NVRAM_READ_IN_TYPE, partn);
		MCDI_IN_SET_DWORD(req, NVRAM_READ_IN_OFFSET, offset);
		MCDI_IN_SET_DWORD(req, NVRAM_READ_IN_LENGTH, chunk);

		efx_mcdi_execute(enp, &req);

		if (req.emr_rc != 0) {
			rc = req.emr_rc;
			goto fail1;
		}

		if (req.emr_out_length_used <
		    MC_CMD_NVRAM_READ_OUT_LEN(chunk)) {
			rc = EMSGSIZE;
			goto fail2;
		}

		memcpy(data,
		    MCDI_OUT2(req, uint8_t, NVRAM_READ_OUT_READ_BUFFER),
		    chunk);

		size -= chunk;
		data += chunk;
		offset += chunk;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
siena_nvram_partn_erase(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__in			size_t size)
{
	efx_mcdi_req_t req;
	uint8_t payload[MC_CMD_NVRAM_ERASE_IN_LEN];
	int rc;

	req.emr_cmd = MC_CMD_NVRAM_ERASE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_ERASE_IN_LEN;
	EFX_STATIC_ASSERT(MC_CMD_NVRAM_ERASE_OUT_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, NVRAM_ERASE_IN_TYPE, partn);
	MCDI_IN_SET_DWORD(req, NVRAM_ERASE_IN_OFFSET, offset);
	MCDI_IN_SET_DWORD(req, NVRAM_ERASE_IN_LENGTH, size);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
siena_nvram_partn_write(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	efx_mcdi_req_t req;
	uint8_t payload[MC_CMD_NVRAM_WRITE_IN_LEN(SIENA_NVRAM_CHUNK)];
	size_t chunk;
	int rc;

	while (size > 0) {
		chunk = MIN(size, SIENA_NVRAM_CHUNK);

		req.emr_cmd = MC_CMD_NVRAM_WRITE;
		req.emr_in_buf = payload;
		req.emr_in_length = MC_CMD_NVRAM_WRITE_IN_LEN(chunk);
		EFX_STATIC_ASSERT(MC_CMD_NVRAM_WRITE_OUT_LEN == 0);
		req.emr_out_buf = NULL;
		req.emr_out_length = 0;

		MCDI_IN_SET_DWORD(req, NVRAM_WRITE_IN_TYPE, partn);
		MCDI_IN_SET_DWORD(req, NVRAM_WRITE_IN_OFFSET, offset);
		MCDI_IN_SET_DWORD(req, NVRAM_WRITE_IN_LENGTH, chunk);

		memcpy(MCDI_IN2(req, uint8_t, NVRAM_WRITE_IN_WRITE_BUFFER),
		    data, chunk);

		efx_mcdi_execute(enp, &req);

		if (req.emr_rc != 0) {
			rc = req.emr_rc;
			goto fail1;
		}

		size -= chunk;
		data += chunk;
		offset += chunk;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

				void
siena_nvram_partn_unlock(
	__in			efx_nic_t *enp,
	__in			unsigned int partn)
{
	efx_mcdi_req_t req;
	uint8_t payload[MC_CMD_NVRAM_UPDATE_FINISH_IN_LEN];
	uint32_t reboot;
	int rc;

	req.emr_cmd = MC_CMD_NVRAM_UPDATE_FINISH;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_UPDATE_FINISH_IN_LEN;
	EFX_STATIC_ASSERT(MC_CMD_NVRAM_UPDATE_FINISH_OUT_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	/*
	 * Reboot into the new image only for PHYs. The driver has to
	 * explicitly cope with an MC reboot after a firmware update.
	 */
	reboot = (partn == MC_CMD_NVRAM_TYPE_PHY_PORT0 ||
		    partn == MC_CMD_NVRAM_TYPE_PHY_PORT1 ||
		    partn == MC_CMD_NVRAM_TYPE_DISABLED_CALLISTO);

	MCDI_IN_SET_DWORD(req, NVRAM_UPDATE_FINISH_IN_TYPE, partn);
	MCDI_IN_SET_DWORD(req, NVRAM_UPDATE_FINISH_IN_REBOOT, reboot);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return;

fail1:
	EFSYS_PROBE1(fail1, int, rc);
}

#endif	/* EFSYS_OPT_VPD || EFSYS_OPT_NVRAM */

#if EFSYS_OPT_NVRAM

typedef struct siena_parttbl_entry_s {
	unsigned int		partn;
	unsigned int		port;
	efx_nvram_type_t	nvtype;
} siena_parttbl_entry_t;

static siena_parttbl_entry_t siena_parttbl[] = {
	{MC_CMD_NVRAM_TYPE_DISABLED_CALLISTO,	1, EFX_NVRAM_NULLPHY},
	{MC_CMD_NVRAM_TYPE_DISABLED_CALLISTO,	2, EFX_NVRAM_NULLPHY},
	{MC_CMD_NVRAM_TYPE_MC_FW,		1, EFX_NVRAM_MC_FIRMWARE},
	{MC_CMD_NVRAM_TYPE_MC_FW,		2, EFX_NVRAM_MC_FIRMWARE},
	{MC_CMD_NVRAM_TYPE_MC_FW_BACKUP,	1, EFX_NVRAM_MC_GOLDEN},
	{MC_CMD_NVRAM_TYPE_MC_FW_BACKUP,	2, EFX_NVRAM_MC_GOLDEN},
	{MC_CMD_NVRAM_TYPE_EXP_ROM,		1, EFX_NVRAM_BOOTROM},
	{MC_CMD_NVRAM_TYPE_EXP_ROM,		2, EFX_NVRAM_BOOTROM},
	{MC_CMD_NVRAM_TYPE_EXP_ROM_CFG_PORT0,	1, EFX_NVRAM_BOOTROM_CFG},
	{MC_CMD_NVRAM_TYPE_EXP_ROM_CFG_PORT1,	2, EFX_NVRAM_BOOTROM_CFG},
	{MC_CMD_NVRAM_TYPE_PHY_PORT0,		1, EFX_NVRAM_PHY},
	{MC_CMD_NVRAM_TYPE_PHY_PORT1,		2, EFX_NVRAM_PHY},
	{0, 0, 0},
};

static	__checkReturn		siena_parttbl_entry_t *
siena_parttbl_entry(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	siena_parttbl_entry_t *entry;

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);

	for (entry = siena_parttbl; entry->port > 0; ++entry) {
		if (entry->port == emip->emi_port && entry->nvtype == type)
			return (entry);
	}

	return (NULL);
}

#if EFSYS_OPT_DIAG

	__checkReturn		int
siena_nvram_test(
	__in			efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	siena_parttbl_entry_t *entry;
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_NVRAM_TEST_IN_LEN,
			    MC_CMD_NVRAM_TEST_OUT_LEN)];
	int result;
	int rc;

	req.emr_cmd = MC_CMD_NVRAM_TEST;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_TEST_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_TEST_OUT_LEN;

	/*
	 * Iterate over the list of supported partition types
	 * applicable to *this* port
	 */
	for (entry = siena_parttbl; entry->port > 0; ++entry) {
		if (entry->port != emip->emi_port ||
		    !(enp->en_u.siena.enu_partn_mask & (1 << entry->partn)))
			continue;

		MCDI_IN_SET_DWORD(req, NVRAM_TEST_IN_TYPE, entry->partn);

		efx_mcdi_execute(enp, &req);

		if (req.emr_rc != 0) {
			rc = req.emr_rc;
			goto fail1;
		}

		if (req.emr_out_length_used < MC_CMD_NVRAM_TEST_OUT_LEN) {
			rc = EMSGSIZE;
			goto fail2;
		}

		result = MCDI_OUT_DWORD(req, NVRAM_TEST_OUT_RESULT);
		if (result == MC_CMD_NVRAM_TEST_FAIL) {

			EFSYS_PROBE1(nvram_test_failure, int, entry->partn);

			rc = (EINVAL);
			goto fail3;
		}
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */

	__checkReturn		int
siena_nvram_size(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *sizep)
{
	siena_parttbl_entry_t *entry;
	int rc;

	if ((entry = siena_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = siena_nvram_partn_size(enp, entry->partn, sizep)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	*sizep = 0;

	return (rc);
}

#define	SIENA_DYNAMIC_CFG_SIZE(_nitems)					\
	(sizeof (siena_mc_dynamic_config_hdr_t) + ((_nitems) *		\
	sizeof (((siena_mc_dynamic_config_hdr_t *)NULL)->fw_version[0])))

	__checkReturn		int
siena_nvram_get_dynamic_cfg(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			boolean_t vpd,
	__out			siena_mc_dynamic_config_hdr_t **dcfgp,
	__out			size_t *sizep)
{
	siena_mc_dynamic_config_hdr_t *dcfg;
	size_t size;
	uint8_t cksum;
	unsigned int vpd_offset;
	unsigned int vpd_length;
	unsigned int hdr_length;
	unsigned int nversions;
	unsigned int pos;
	unsigned int region;
	int rc;

	EFSYS_ASSERT(partn == MC_CMD_NVRAM_TYPE_DYNAMIC_CFG_PORT0 ||
		    partn == MC_CMD_NVRAM_TYPE_DYNAMIC_CFG_PORT1);

	/*
	 * Allocate sufficient memory for the entire dynamiccfg area, even
	 * if we're not actually going to read in the VPD.
	 */
	if ((rc = siena_nvram_partn_size(enp, partn, &size)) != 0)
		goto fail1;

	EFSYS_KMEM_ALLOC(enp->en_esip, size, dcfg);
	if (dcfg == NULL) {
		rc = ENOMEM;
		goto fail2;
	}

	if ((rc = siena_nvram_partn_read(enp, partn, 0,
	    (caddr_t)dcfg, SIENA_NVRAM_CHUNK)) != 0)
		goto fail3;

	/* Verify the magic */
	if (EFX_DWORD_FIELD(dcfg->magic, EFX_DWORD_0)
	    != SIENA_MC_DYNAMIC_CONFIG_MAGIC)
		goto invalid1;

	/* All future versions of the structure must be backwards compatable */
	EFX_STATIC_ASSERT(SIENA_MC_DYNAMIC_CONFIG_VERSION == 0);

	hdr_length = EFX_WORD_FIELD(dcfg->length, EFX_WORD_0);
	nversions = EFX_DWORD_FIELD(dcfg->num_fw_version_items, EFX_DWORD_0);
	vpd_offset = EFX_DWORD_FIELD(dcfg->dynamic_vpd_offset, EFX_DWORD_0);
	vpd_length = EFX_DWORD_FIELD(dcfg->dynamic_vpd_length, EFX_DWORD_0);

	/* Verify the hdr doesn't overflow the partn size */
	if (hdr_length > size || vpd_offset > size || vpd_length > size ||
	    vpd_length + vpd_offset > size)
		goto invalid2;

	/* Verify the header has room for all it's versions */
	if (hdr_length < SIENA_DYNAMIC_CFG_SIZE(0) ||
	    hdr_length < SIENA_DYNAMIC_CFG_SIZE(nversions))
		goto invalid3;

	/*
	 * Read the remaining portion of the dcfg, either including
	 * the whole of VPD (there is no vpd length in this structure,
	 * so we have to parse each tag), or just the dcfg header itself
	 */
	region = vpd ? vpd_offset + vpd_length : hdr_length;
	if (region > SIENA_NVRAM_CHUNK) {
		if ((rc = siena_nvram_partn_read(enp, partn, SIENA_NVRAM_CHUNK,
		    (caddr_t)dcfg + SIENA_NVRAM_CHUNK,
		    region - SIENA_NVRAM_CHUNK)) != 0)
			goto fail4;
	}

	/* Verify checksum */
	cksum = 0;
	for (pos = 0; pos < hdr_length; pos++)
		cksum += ((uint8_t *)dcfg)[pos];
	if (cksum != 0)
		goto invalid4;

	goto done;

invalid4:
	EFSYS_PROBE(invalid4);
invalid3:
	EFSYS_PROBE(invalid3);
invalid2:
	EFSYS_PROBE(invalid2);
invalid1:
	EFSYS_PROBE(invalid1);

	/*
	 * Construct a new "null" dcfg, with an empty version vector,
	 * and an empty VPD chunk trailing. This has the neat side effect
	 * of testing the exception paths in the write path.
	 */
	EFX_POPULATE_DWORD_1(dcfg->magic,
			    EFX_DWORD_0, SIENA_MC_DYNAMIC_CONFIG_MAGIC);
	EFX_POPULATE_WORD_1(dcfg->length, EFX_WORD_0, sizeof (*dcfg));
	EFX_POPULATE_BYTE_1(dcfg->version, EFX_BYTE_0,
			    SIENA_MC_DYNAMIC_CONFIG_VERSION);
	EFX_POPULATE_DWORD_1(dcfg->dynamic_vpd_offset,
			    EFX_DWORD_0, sizeof (*dcfg));
	EFX_POPULATE_DWORD_1(dcfg->dynamic_vpd_length, EFX_DWORD_0, 0);
	EFX_POPULATE_DWORD_1(dcfg->num_fw_version_items, EFX_DWORD_0, 0);

done:
	*dcfgp = dcfg;
	*sizep = size;

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);

	EFSYS_KMEM_FREE(enp->en_esip, size, dcfg);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn		int
siena_nvram_get_subtype(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__out			uint32_t *subtypep)
{
	efx_mcdi_req_t req;
	uint8_t outbuf[MC_CMD_GET_BOARD_CFG_OUT_LEN];
	efx_word_t *fw_list;
	int rc;

	req.emr_cmd = MC_CMD_GET_BOARD_CFG;
	EFX_STATIC_ASSERT(MC_CMD_GET_BOARD_CFG_IN_LEN == 0);
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = outbuf;
	req.emr_out_length = sizeof (outbuf);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_BOARD_CFG_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	fw_list = MCDI_OUT2(req, efx_word_t,
			    GET_BOARD_CFG_OUT_FW_SUBTYPE_LIST);
	*subtypep = EFX_WORD_FIELD(fw_list[partn], EFX_WORD_0);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
siena_nvram_get_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4])
{
	siena_mc_dynamic_config_hdr_t *dcfg;
	siena_parttbl_entry_t *entry;
	unsigned int dcfg_partn;
	unsigned int partn;
	int rc;

	if ((entry = siena_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}
	partn = entry->partn;

	if ((1 << partn) & ~enp->en_u.siena.enu_partn_mask) {
		rc = ENOTSUP;
		goto fail2;
	}

	if ((rc = siena_nvram_get_subtype(enp, partn, subtypep)) != 0)
		goto fail3;

	/*
	 * Some partitions are accessible from both ports (for instance BOOTROM)
	 * Find the highest version reported by all dcfg structures on ports
	 * that have access to this partition.
	 */
	version[0] = version[1] = version[2] = version[3] = 0;
	for (entry = siena_parttbl; entry->port > 0; ++entry) {
		unsigned int nitems;
		uint16_t temp[4];
		size_t length;

		if (entry->partn != partn)
			continue;

		dcfg_partn = (entry->port == 1)
			? MC_CMD_NVRAM_TYPE_DYNAMIC_CFG_PORT0
			: MC_CMD_NVRAM_TYPE_DYNAMIC_CFG_PORT1;
		/*
		 * Ingore missing partitions on port 2, assuming they're due
		 * to to running on a single port part.
		 */
		if ((1 << dcfg_partn) &  ~enp->en_u.siena.enu_partn_mask) {
			if (entry->port == 2)
				continue;
		}

		if ((rc = siena_nvram_get_dynamic_cfg(enp, dcfg_partn,
		    B_FALSE, &dcfg, &length)) != 0)
			goto fail4;

		nitems = EFX_DWORD_FIELD(dcfg->num_fw_version_items,
			    EFX_DWORD_0);
		if (nitems < entry->partn)
			goto done;

		temp[0] = EFX_WORD_FIELD(dcfg->fw_version[partn].version_w,
			    EFX_WORD_0);
		temp[1] = EFX_WORD_FIELD(dcfg->fw_version[partn].version_x,
			    EFX_WORD_0);
		temp[2] = EFX_WORD_FIELD(dcfg->fw_version[partn].version_y,
			    EFX_WORD_0);
		temp[3] = EFX_WORD_FIELD(dcfg->fw_version[partn].version_z,
			    EFX_WORD_0);
		if (memcmp(version, temp, sizeof (temp)) < 0)
			memcpy(version, temp, sizeof (temp));

	done:
		EFSYS_KMEM_FREE(enp->en_esip, length, dcfg);
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
siena_nvram_rw_start(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *chunk_sizep)
{
	siena_parttbl_entry_t *entry;
	int rc;

	if ((entry = siena_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = siena_nvram_partn_lock(enp, entry->partn)) != 0)
		goto fail2;

	if (chunk_sizep != NULL)
		*chunk_sizep = SIENA_NVRAM_CHUNK;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
siena_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	siena_parttbl_entry_t *entry;
	int rc;

	if ((entry = siena_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = siena_nvram_partn_read(enp, entry->partn,
	    offset, data, size)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
siena_nvram_erase(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type)
{
	siena_parttbl_entry_t *entry;
	size_t size;
	int rc;

	if ((entry = siena_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = siena_nvram_partn_size(enp, entry->partn, &size)) != 0)
		goto fail2;

	if ((rc = siena_nvram_partn_erase(enp, entry->partn, 0, size)) != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
siena_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	siena_parttbl_entry_t *entry;
	int rc;

	if ((entry = siena_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = siena_nvram_partn_write(enp, entry->partn,
	    offset, data, size)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

				void
siena_nvram_rw_finish(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type)
{
	siena_parttbl_entry_t *entry;

	if ((entry = siena_parttbl_entry(enp, type)) != NULL)
		siena_nvram_partn_unlock(enp, entry->partn);
}

	__checkReturn		int
siena_nvram_set_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint16_t version[4])
{
	siena_mc_dynamic_config_hdr_t *dcfg = NULL;
	siena_parttbl_entry_t *entry;
	unsigned int dcfg_partn;
	size_t partn_size;
	unsigned int hdr_length;
	unsigned int vpd_length;
	unsigned int vpd_offset;
	unsigned int nitems;
	unsigned int required_hdr_length;
	unsigned int pos;
	uint8_t cksum;
	uint32_t subtype;
	size_t length;
	int rc;

	if ((entry = siena_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	dcfg_partn = (entry->port == 1)
		? MC_CMD_NVRAM_TYPE_DYNAMIC_CFG_PORT0
		: MC_CMD_NVRAM_TYPE_DYNAMIC_CFG_PORT1;

	if ((rc = siena_nvram_partn_size(enp, dcfg_partn, &partn_size)) != 0)
		goto fail2;

	if ((rc = siena_nvram_partn_lock(enp, dcfg_partn)) != 0)
		goto fail2;

	if ((rc = siena_nvram_get_dynamic_cfg(enp, dcfg_partn,
	    B_TRUE, &dcfg, &length)) != 0)
		goto fail3;

	hdr_length = EFX_WORD_FIELD(dcfg->length, EFX_WORD_0);
	nitems = EFX_DWORD_FIELD(dcfg->num_fw_version_items, EFX_DWORD_0);
	vpd_length = EFX_DWORD_FIELD(dcfg->dynamic_vpd_length, EFX_DWORD_0);
	vpd_offset = EFX_DWORD_FIELD(dcfg->dynamic_vpd_offset, EFX_DWORD_0);

	/*
	 * NOTE: This function will blatt any fields trailing the version
	 * vector, or the VPD chunk.
	 */
	required_hdr_length = SIENA_DYNAMIC_CFG_SIZE(entry->partn + 1);
	if (required_hdr_length + vpd_length > length) {
		rc = ENOSPC;
		goto fail4;
	}

	if (vpd_offset < required_hdr_length) {
		(void) memmove((caddr_t)dcfg + required_hdr_length,
			(caddr_t)dcfg + vpd_offset, vpd_length);
		vpd_offset = required_hdr_length;
		EFX_POPULATE_DWORD_1(dcfg->dynamic_vpd_offset,
				    EFX_DWORD_0, vpd_offset);
	}

	if (hdr_length < required_hdr_length) {
		(void) memset((caddr_t)dcfg + hdr_length, 0,
			required_hdr_length - hdr_length);
		hdr_length = required_hdr_length;
		EFX_POPULATE_WORD_1(dcfg->length,
				    EFX_WORD_0, hdr_length);
	}

	/* Get the subtype to insert into the fw_subtype array */
	if ((rc = siena_nvram_get_subtype(enp, entry->partn, &subtype)) != 0)
		goto fail5;

	/* Fill out the new version */
	EFX_POPULATE_DWORD_1(dcfg->fw_version[entry->partn].fw_subtype,
			    EFX_DWORD_0, subtype);
	EFX_POPULATE_WORD_1(dcfg->fw_version[entry->partn].version_w,
			    EFX_WORD_0, version[0]);
	EFX_POPULATE_WORD_1(dcfg->fw_version[entry->partn].version_x,
			    EFX_WORD_0, version[1]);
	EFX_POPULATE_WORD_1(dcfg->fw_version[entry->partn].version_y,
			    EFX_WORD_0, version[2]);
	EFX_POPULATE_WORD_1(dcfg->fw_version[entry->partn].version_z,
			    EFX_WORD_0, version[3]);

	/* Update the version count */
	if (nitems < entry->partn + 1) {
		nitems = entry->partn + 1;
		EFX_POPULATE_DWORD_1(dcfg->num_fw_version_items,
				    EFX_DWORD_0, nitems);
	}

	/* Update the checksum */
	cksum = 0;
	for (pos = 0; pos < hdr_length; pos++)
		cksum += ((uint8_t *)dcfg)[pos];
	dcfg->csum.eb_u8[0] -= cksum;

	/* Erase and write the new partition */
	if ((rc = siena_nvram_partn_erase(enp, dcfg_partn, 0, partn_size)) != 0)
		goto fail6;

	/* Write out the new structure to nvram */
	if ((rc = siena_nvram_partn_write(enp, dcfg_partn, 0,
	    (caddr_t)dcfg, vpd_offset + vpd_length)) != 0)
		goto fail7;

	EFSYS_KMEM_FREE(enp->en_esip, length, dcfg);

	siena_nvram_partn_unlock(enp, dcfg_partn);

	return (0);

fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);

	EFSYS_KMEM_FREE(enp->en_esip, length, dcfg);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_NVRAM */

#endif	/* EFSYS_OPT_SIENA */
