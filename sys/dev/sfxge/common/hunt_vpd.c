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


#if EFSYS_OPT_VPD

#if EFSYS_OPT_HUNTINGTON

#include "ef10_tlv_layout.h"

	__checkReturn		efx_rc_t
hunt_vpd_init(
	__in			efx_nic_t *enp)
{
	caddr_t svpd;
	size_t svpd_size;
	uint32_t pci_pf;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	pci_pf = enp->en_nic_cfg.enc_pf;
	/*
	 * The VPD interface exposes VPD resources from the combined static and
	 * dynamic VPD storage. As the static VPD configuration should *never*
	 * change, we can cache it.
	 */
	svpd = NULL;
	svpd_size = 0;
	rc = hunt_nvram_partn_read_tlv(enp,
	    NVRAM_PARTITION_TYPE_STATIC_CONFIG,
	    TLV_TAG_PF_STATIC_VPD(pci_pf),
	    &svpd, &svpd_size);
	if (rc != 0) {
		if (rc == EACCES) {
			/* Unpriviledged functions cannot access VPD */
			goto out;
		}
		goto fail1;
	}

	if (svpd != NULL && svpd_size > 0) {
		if ((rc = efx_vpd_hunk_verify(svpd, svpd_size, NULL)) != 0)
			goto fail2;
	}

	enp->en_u.hunt.enu_svpd = svpd;
	enp->en_u.hunt.enu_svpd_length = svpd_size;

out:
	return (0);

fail2:
	EFSYS_PROBE(fail2);

	EFSYS_KMEM_FREE(enp->en_esip, svpd_size, svpd);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
hunt_vpd_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep)
{
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	/*
	 * This function returns the total size the user should allocate
	 * for all VPD operations. We've already cached the static vpd,
	 * so we just need to return an upper bound on the dynamic vpd,
	 * which is the size of the DYNAMIC_CONFIG partition.
	 */
	if ((rc = efx_mcdi_nvram_info(enp, NVRAM_PARTITION_TYPE_DYNAMIC_CONFIG,
		    sizep, NULL, NULL)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
hunt_vpd_read(
	__in			efx_nic_t *enp,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	caddr_t dvpd;
	size_t dvpd_size;
	uint32_t pci_pf;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	pci_pf = enp->en_nic_cfg.enc_pf;

	if ((rc = hunt_nvram_partn_read_tlv(enp,
		    NVRAM_PARTITION_TYPE_DYNAMIC_CONFIG,
		    TLV_TAG_PF_DYNAMIC_VPD(pci_pf),
		    &dvpd, &dvpd_size)) != 0)
		goto fail1;

	if (dvpd_size > size) {
		rc = ENOSPC;
		goto fail2;
	}
	memcpy(data, dvpd, dvpd_size);

	/* Pad data with all-1s, consistent with update operations */
	memset(data + dvpd_size, 0xff, size - dvpd_size);

	EFSYS_KMEM_FREE(enp->en_esip, dvpd_size, dvpd);

	return (0);

fail2:
	EFSYS_PROBE(fail2);

	EFSYS_KMEM_FREE(enp->en_esip, dvpd_size, dvpd);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
hunt_vpd_verify(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	efx_vpd_tag_t stag;
	efx_vpd_tag_t dtag;
	efx_vpd_keyword_t skey;
	efx_vpd_keyword_t dkey;
	unsigned int scont;
	unsigned int dcont;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	/*
	 * Strictly you could take the view that dynamic vpd is optional.
	 * Instead, to conform more closely to the read/verify/reinit()
	 * paradigm, we require dynamic vpd. hunt_vpd_reinit() will
	 * reinitialize it as required.
	 */
	if ((rc = efx_vpd_hunk_verify(data, size, NULL)) != 0)
		goto fail1;

	/*
	 * Verify that there is no duplication between the static and
	 * dynamic cfg sectors.
	 */
	if (enp->en_u.hunt.enu_svpd_length == 0)
		goto done;

	dcont = 0;
	_NOTE(CONSTANTCONDITION)
	while (1) {
		if ((rc = efx_vpd_hunk_next(data, size, &dtag,
		    &dkey, NULL, NULL, &dcont)) != 0)
			goto fail2;
		if (dcont == 0)
			break;

		scont = 0;
		_NOTE(CONSTANTCONDITION)
		while (1) {
			if ((rc = efx_vpd_hunk_next(
			    enp->en_u.hunt.enu_svpd,
			    enp->en_u.hunt.enu_svpd_length, &stag, &skey,
			    NULL, NULL, &scont)) != 0)
				goto fail3;
			if (scont == 0)
				break;

			if (stag == dtag && skey == dkey) {
				rc = EEXIST;
				goto fail4;
			}
		}
	}

done:
	return (0);

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

	__checkReturn		efx_rc_t
hunt_vpd_reinit(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	boolean_t wantpid;
	efx_rc_t rc;

	/*
	 * Only create an ID string if the dynamic cfg doesn't have one
	 */
	if (enp->en_u.hunt.enu_svpd_length == 0)
		wantpid = B_TRUE;
	else {
		unsigned int offset;
		uint8_t length;

		rc = efx_vpd_hunk_get(enp->en_u.hunt.enu_svpd,
				    enp->en_u.hunt.enu_svpd_length,
				    EFX_VPD_ID, 0, &offset, &length);
		if (rc == 0)
			wantpid = B_FALSE;
		else if (rc == ENOENT)
			wantpid = B_TRUE;
		else
			goto fail1;
	}

	if ((rc = efx_vpd_hunk_reinit(data, size, wantpid)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
hunt_vpd_get(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__inout			efx_vpd_value_t *evvp)
{
	unsigned int offset;
	uint8_t length;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	/* Attempt to satisfy the request from svpd first */
	if (enp->en_u.hunt.enu_svpd_length > 0) {
		if ((rc = efx_vpd_hunk_get(enp->en_u.hunt.enu_svpd,
		    enp->en_u.hunt.enu_svpd_length, evvp->evv_tag,
		    evvp->evv_keyword, &offset, &length)) == 0) {
			evvp->evv_length = length;
			memcpy(evvp->evv_value,
			    enp->en_u.hunt.enu_svpd + offset, length);
			return (0);
		} else if (rc != ENOENT)
			goto fail1;
	}

	/* And then from the provided data buffer */
	if ((rc = efx_vpd_hunk_get(data, size, evvp->evv_tag,
	    evvp->evv_keyword, &offset, &length)) != 0)
		goto fail2;

	evvp->evv_length = length;
	memcpy(evvp->evv_value, data + offset, length);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
hunt_vpd_set(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			efx_vpd_value_t *evvp)
{
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	/* If the provided (tag,keyword) exists in svpd, then it is readonly */
	if (enp->en_u.hunt.enu_svpd_length > 0) {
		unsigned int offset;
		uint8_t length;

		if ((rc = efx_vpd_hunk_get(enp->en_u.hunt.enu_svpd,
		    enp->en_u.hunt.enu_svpd_length, evvp->evv_tag,
		    evvp->evv_keyword, &offset, &length)) == 0) {
			rc = EACCES;
			goto fail1;
		}
	}

	if ((rc = efx_vpd_hunk_set(data, size, evvp)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
hunt_vpd_next(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__out			efx_vpd_value_t *evvp,
	__inout			unsigned int *contp)
{
	_NOTE(ARGUNUSED(enp, data, size, evvp, contp))

	return (ENOTSUP);
}

	__checkReturn		efx_rc_t
hunt_vpd_write(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	size_t vpd_length;
	uint32_t pci_pf;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	pci_pf = enp->en_nic_cfg.enc_pf;

	/* Determine total length of new dynamic VPD */
	if ((rc = efx_vpd_hunk_length(data, size, &vpd_length)) != 0)
		goto fail1;

	/* Store new dynamic VPD in all segments in DYNAMIC_CONFIG partition */
	if ((rc = hunt_nvram_partn_write_segment_tlv(enp,
		    NVRAM_PARTITION_TYPE_DYNAMIC_CONFIG,
		    TLV_TAG_PF_DYNAMIC_VPD(pci_pf),
		    data, vpd_length, B_TRUE)) != 0) {
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

				void
hunt_vpd_fini(
	__in			efx_nic_t *enp)
{
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	if (enp->en_u.hunt.enu_svpd_length > 0) {
		EFSYS_KMEM_FREE(enp->en_esip, enp->en_u.hunt.enu_svpd_length,
				enp->en_u.hunt.enu_svpd);

		enp->en_u.hunt.enu_svpd = NULL;
		enp->en_u.hunt.enu_svpd_length = 0;
	}
}

#endif	/* EFSYS_OPT_HUNTINGTON */

#endif	/* EFSYS_OPT_VPD */
