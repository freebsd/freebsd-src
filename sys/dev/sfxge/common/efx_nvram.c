/*-
 * Copyright (c) 2009-2015 Solarflare Communications Inc.
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

#include "efsys.h"
#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_impl.h"

#if EFSYS_OPT_NVRAM

#if EFSYS_OPT_FALCON

static efx_nvram_ops_t	__efx_nvram_falcon_ops = {
#if EFSYS_OPT_DIAG
	falcon_nvram_test,		/* envo_test */
#endif	/* EFSYS_OPT_DIAG */
	falcon_nvram_size,		/* envo_size */
	falcon_nvram_get_version,	/* envo_get_version */
	falcon_nvram_rw_start,		/* envo_rw_start */
	falcon_nvram_read_chunk,	/* envo_read_chunk */
	falcon_nvram_erase,		/* envo_erase */
	falcon_nvram_write_chunk,	/* envo_write_chunk */
	falcon_nvram_rw_finish,		/* envo_rw_finish */
	falcon_nvram_set_version,	/* envo_set_version */
};

#endif	/* EFSYS_OPT_FALCON */

#if EFSYS_OPT_SIENA

static efx_nvram_ops_t	__efx_nvram_siena_ops = {
#if EFSYS_OPT_DIAG
	siena_nvram_test,		/* envo_test */
#endif	/* EFSYS_OPT_DIAG */
	siena_nvram_size,		/* envo_size */
	siena_nvram_get_version,	/* envo_get_version */
	siena_nvram_rw_start,		/* envo_rw_start */
	siena_nvram_read_chunk,		/* envo_read_chunk */
	siena_nvram_erase,		/* envo_erase */
	siena_nvram_write_chunk,	/* envo_write_chunk */
	siena_nvram_rw_finish,		/* envo_rw_finish */
	siena_nvram_set_version,	/* envo_set_version */
};

#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON

static efx_nvram_ops_t	__efx_nvram_hunt_ops = {
#if EFSYS_OPT_DIAG
	hunt_nvram_test,		/* envo_test */
#endif	/* EFSYS_OPT_DIAG */
	hunt_nvram_size,		/* envo_size */
	hunt_nvram_get_version,		/* envo_get_version */
	hunt_nvram_rw_start,		/* envo_rw_start */
	hunt_nvram_read_chunk,		/* envo_read_chunk */
	hunt_nvram_erase,		/* envo_erase */
	hunt_nvram_write_chunk,		/* envo_write_chunk */
	hunt_nvram_rw_finish,		/* envo_rw_finish */
	hunt_nvram_set_version,		/* envo_set_version */
};

#endif	/* EFSYS_OPT_HUNTINGTON */

	__checkReturn	efx_rc_t
efx_nvram_init(
	__in		efx_nic_t *enp)
{
	efx_nvram_ops_t *envop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_NVRAM));

	switch (enp->en_family) {
#if EFSYS_OPT_FALCON
	case EFX_FAMILY_FALCON:
		envop = (efx_nvram_ops_t *)&__efx_nvram_falcon_ops;
		break;
#endif	/* EFSYS_OPT_FALCON */

#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		envop = (efx_nvram_ops_t *)&__efx_nvram_siena_ops;
		break;
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		envop = (efx_nvram_ops_t *)&__efx_nvram_hunt_ops;
		break;
#endif	/* EFSYS_OPT_HUNTINGTON */

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}

	enp->en_envop = envop;
	enp->en_mod_flags |= EFX_MOD_NVRAM;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#if EFSYS_OPT_DIAG

	__checkReturn		efx_rc_t
efx_nvram_test(
	__in			efx_nic_t *enp)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	if ((rc = envop->envo_test(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */

	__checkReturn		efx_rc_t
efx_nvram_size(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *sizep)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);

	if ((rc = envop->envo_size(enp, type, sizep)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_nvram_get_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4])
{
	efx_nvram_ops_t *envop = enp->en_envop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);

	if ((rc = envop->envo_get_version(enp, type, subtypep, version)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_nvram_rw_start(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out_opt		size_t *chunk_sizep)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);
	EFSYS_ASSERT3U(type, !=, EFX_NVRAM_INVALID);

	EFSYS_ASSERT3U(enp->en_nvram_locked, ==, EFX_NVRAM_INVALID);

	if ((rc = envop->envo_rw_start(enp, type, chunk_sizep)) != 0)
		goto fail1;

	enp->en_nvram_locked = type;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);
	EFSYS_ASSERT3U(type, !=, EFX_NVRAM_INVALID);

	EFSYS_ASSERT3U(enp->en_nvram_locked, ==, type);

	if ((rc = envop->envo_read_chunk(enp, type, offset, data, size)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_nvram_erase(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);
	EFSYS_ASSERT3U(type, !=, EFX_NVRAM_INVALID);

	EFSYS_ASSERT3U(enp->en_nvram_locked, ==, type);

	if ((rc = envop->envo_erase(enp, type)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);
	EFSYS_ASSERT3U(type, !=, EFX_NVRAM_INVALID);

	EFSYS_ASSERT3U(enp->en_nvram_locked, ==, type);

	if ((rc = envop->envo_write_chunk(enp, type, offset, data, size)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

				void
efx_nvram_rw_finish(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type)
{
	efx_nvram_ops_t *envop = enp->en_envop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);
	EFSYS_ASSERT3U(type, !=, EFX_NVRAM_INVALID);

	EFSYS_ASSERT3U(enp->en_nvram_locked, ==, type);

	envop->envo_rw_finish(enp, type);

	enp->en_nvram_locked = EFX_NVRAM_INVALID;
}

	__checkReturn		efx_rc_t
efx_nvram_set_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in_ecount(4)		uint16_t version[4])
{
	efx_nvram_ops_t *envop = enp->en_envop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);

	/*
	 * The Siena implementation of envo_set_version() will attempt to
	 * acquire the NVRAM_UPDATE lock for the DYNAMIC_CONFIG sector.
	 * Therefore, you can't have already acquired the NVRAM_UPDATE lock.
	 */
	EFSYS_ASSERT3U(enp->en_nvram_locked, ==, EFX_NVRAM_INVALID);

	if ((rc = envop->envo_set_version(enp, type, version)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

void
efx_nvram_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(enp->en_nvram_locked, ==, EFX_NVRAM_INVALID);

	enp->en_envop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_NVRAM;
}

#endif	/* EFSYS_OPT_NVRAM */

#if EFSYS_OPT_NVRAM || EFSYS_OPT_VPD

/*
 * Internal MCDI request handling
 */

	__checkReturn		efx_rc_t
efx_mcdi_nvram_partitions(
	__in			efx_nic_t *enp,
	__out_bcount(size)	caddr_t data,
	__in			size_t size,
	__out			unsigned int *npartnp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_NVRAM_PARTITIONS_IN_LEN,
			    MC_CMD_NVRAM_PARTITIONS_OUT_LENMAX)];
	unsigned int npartn;
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_NVRAM_PARTITIONS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_PARTITIONS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_PARTITIONS_OUT_LENMAX;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_NVRAM_PARTITIONS_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail2;
	}
	npartn = MCDI_OUT_DWORD(req, NVRAM_PARTITIONS_OUT_NUM_PARTITIONS);

	if (req.emr_out_length_used < MC_CMD_NVRAM_PARTITIONS_OUT_LEN(npartn)) {
		rc = ENOENT;
		goto fail3;
	}

	if (size < npartn * sizeof (uint32_t)) {
		rc = ENOSPC;
		goto fail3;
	}

	*npartnp = npartn;

	memcpy(data,
	    MCDI_OUT2(req, uint32_t, NVRAM_PARTITIONS_OUT_TYPE_ID),
	    (npartn * sizeof (uint32_t)));

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_nvram_metadata(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4],
	__out_bcount_opt(size)	char *descp,
	__in			size_t size)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_NVRAM_METADATA_IN_LEN,
			    MC_CMD_NVRAM_METADATA_OUT_LENMAX)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_NVRAM_METADATA;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_METADATA_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_METADATA_OUT_LENMAX;

	MCDI_IN_SET_DWORD(req, NVRAM_METADATA_IN_TYPE, partn);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_NVRAM_METADATA_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (MCDI_OUT_DWORD_FIELD(req, NVRAM_METADATA_OUT_FLAGS,
		NVRAM_METADATA_OUT_SUBTYPE_VALID)) {
		*subtypep = MCDI_OUT_DWORD(req, NVRAM_METADATA_OUT_SUBTYPE);
	} else {
		*subtypep = 0;
	}

	if (MCDI_OUT_DWORD_FIELD(req, NVRAM_METADATA_OUT_FLAGS,
		NVRAM_METADATA_OUT_VERSION_VALID)) {
		version[0] = MCDI_OUT_WORD(req, NVRAM_METADATA_OUT_VERSION_W);
		version[1] = MCDI_OUT_WORD(req, NVRAM_METADATA_OUT_VERSION_X);
		version[2] = MCDI_OUT_WORD(req, NVRAM_METADATA_OUT_VERSION_Y);
		version[3] = MCDI_OUT_WORD(req, NVRAM_METADATA_OUT_VERSION_Z);
	} else {
		version[0] = version[1] = version[2] = version[3] = 0;
	}

	if (MCDI_OUT_DWORD_FIELD(req, NVRAM_METADATA_OUT_FLAGS,
		NVRAM_METADATA_OUT_DESCRIPTION_VALID)) {
		/* Return optional descrition string */
		if ((descp != NULL) && (size > 0)) {
			size_t desclen;

			descp[0] = '\0';
			desclen = (req.emr_out_length_used
			    - MC_CMD_NVRAM_METADATA_OUT_LEN(0));

			EFSYS_ASSERT3U(desclen, <=,
			    MC_CMD_NVRAM_METADATA_OUT_DESCRIPTION_MAXNUM);

			if (size < desclen) {
				rc = ENOSPC;
				goto fail3;
			}

			memcpy(descp, MCDI_OUT2(req, char,
				NVRAM_METADATA_OUT_DESCRIPTION),
			    desclen);

			/* Ensure string is NUL terminated */
			descp[desclen] = '\0';
		}
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_nvram_info(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__out_opt		size_t *sizep,
	__out_opt		uint32_t *addressp,
	__out_opt		uint32_t *erase_sizep)
{
	uint8_t payload[MAX(MC_CMD_NVRAM_INFO_IN_LEN,
			    MC_CMD_NVRAM_INFO_OUT_LEN)];
	efx_mcdi_req_t req;
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_NVRAM_INFO;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_INFO_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_INFO_OUT_LEN;

	MCDI_IN_SET_DWORD(req, NVRAM_INFO_IN_TYPE, partn);

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_NVRAM_INFO_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (sizep)
		*sizep = MCDI_OUT_DWORD(req, NVRAM_INFO_OUT_SIZE);

	if (addressp)
		*addressp = MCDI_OUT_DWORD(req, NVRAM_INFO_OUT_PHYSADDR);

	if (erase_sizep)
		*erase_sizep = MCDI_OUT_DWORD(req, NVRAM_INFO_OUT_ERASESIZE);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_nvram_update_start(
	__in			efx_nic_t *enp,
	__in			uint32_t partn)
{
	uint8_t payload[MAX(MC_CMD_NVRAM_UPDATE_START_IN_LEN,
			    MC_CMD_NVRAM_UPDATE_START_OUT_LEN)];
	efx_mcdi_req_t req;
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_NVRAM_UPDATE_START;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_UPDATE_START_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_UPDATE_START_OUT_LEN;

	MCDI_IN_SET_DWORD(req, NVRAM_UPDATE_START_IN_TYPE, partn);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_nvram_read(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			uint32_t offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_NVRAM_READ_IN_LEN,
			    MC_CMD_NVRAM_READ_OUT_LENMAX)];
	efx_rc_t rc;

	if (size > MC_CMD_NVRAM_READ_OUT_LENMAX) {
		rc = EINVAL;
		goto fail1;
	}

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_NVRAM_READ;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_READ_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_READ_OUT_LENMAX;

	MCDI_IN_SET_DWORD(req, NVRAM_READ_IN_TYPE, partn);
	MCDI_IN_SET_DWORD(req, NVRAM_READ_IN_OFFSET, offset);
	MCDI_IN_SET_DWORD(req, NVRAM_READ_IN_LENGTH, size);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_NVRAM_READ_OUT_LEN(size)) {
		rc = EMSGSIZE;
		goto fail2;
	}

	memcpy(data,
	    MCDI_OUT2(req, uint8_t, NVRAM_READ_OUT_READ_BUFFER),
	    size);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_nvram_erase(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			uint32_t offset,
	__in			size_t size)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_NVRAM_ERASE_IN_LEN,
			    MC_CMD_NVRAM_ERASE_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_NVRAM_ERASE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_ERASE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_ERASE_OUT_LEN;

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
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_nvram_write(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			uint32_t offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_NVRAM_WRITE_IN_LENMAX,
			    MC_CMD_NVRAM_WRITE_OUT_LEN)];
	efx_rc_t rc;

	if (size > MC_CMD_NVRAM_WRITE_IN_LENMAX) {
		rc = EINVAL;
		goto fail1;
	}

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_NVRAM_WRITE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_WRITE_IN_LEN(size);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_WRITE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, NVRAM_WRITE_IN_TYPE, partn);
	MCDI_IN_SET_DWORD(req, NVRAM_WRITE_IN_OFFSET, offset);
	MCDI_IN_SET_DWORD(req, NVRAM_WRITE_IN_LENGTH, size);

	memcpy(MCDI_IN2(req, uint8_t, NVRAM_WRITE_IN_WRITE_BUFFER),
	    data, size);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
efx_mcdi_nvram_update_finish(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			boolean_t reboot)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_NVRAM_UPDATE_FINISH_IN_LEN,
			    MC_CMD_NVRAM_UPDATE_FINISH_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_NVRAM_UPDATE_FINISH;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_UPDATE_FINISH_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_UPDATE_FINISH_OUT_LEN;

	MCDI_IN_SET_DWORD(req, NVRAM_UPDATE_FINISH_IN_TYPE, partn);
	MCDI_IN_SET_DWORD(req, NVRAM_UPDATE_FINISH_IN_REBOOT, reboot);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#if EFSYS_OPT_DIAG

	__checkReturn		efx_rc_t
efx_mcdi_nvram_test(
	__in			efx_nic_t *enp,
	__in			uint32_t partn)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_NVRAM_TEST_IN_LEN,
			    MC_CMD_NVRAM_TEST_OUT_LEN)];
	int result;
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_NVRAM_TEST;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_TEST_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_TEST_OUT_LEN;

	MCDI_IN_SET_DWORD(req, NVRAM_TEST_IN_TYPE, partn);

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

		EFSYS_PROBE1(nvram_test_failure, int, partn);

		rc = (EINVAL);
		goto fail3;
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */


#endif /* EFSYS_OPT_NVRAM || EFSYS_OPT_VPD */
