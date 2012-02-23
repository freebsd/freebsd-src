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

#if EFSYS_OPT_NVRAM

#if EFSYS_OPT_FALCON

static efx_nvram_ops_t	__cs	__efx_nvram_falcon_ops = {
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

static efx_nvram_ops_t	__cs	__efx_nvram_siena_ops = {
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

	__checkReturn	int
efx_nvram_init(
	__in		efx_nic_t *enp)
{
	efx_nvram_ops_t *envop;
	int rc;

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

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}

	enp->en_envop = envop;
	enp->en_mod_flags |= EFX_MOD_NVRAM;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_DIAG

	__checkReturn		int
efx_nvram_test(
	__in			efx_nic_t *enp)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	if ((rc = envop->envo_test(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */

	__checkReturn		int
efx_nvram_size(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *sizep)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);

	if ((rc = envop->envo_size(enp, type, sizep)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
efx_nvram_get_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4])
{
	efx_nvram_ops_t *envop = enp->en_envop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);

	if ((rc = envop->envo_get_version(enp, type, subtypep, version)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
efx_nvram_rw_start(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out_opt		size_t *chunk_sizep)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	int rc;

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
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
efx_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);
	EFSYS_ASSERT3U(type, !=, EFX_NVRAM_INVALID);

	EFSYS_ASSERT3U(enp->en_nvram_locked, ==, type);

	if ((rc = envop->envo_read_chunk(enp, type, offset, data, size)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
efx_nvram_erase(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);
	EFSYS_ASSERT3U(type, !=, EFX_NVRAM_INVALID);

	EFSYS_ASSERT3U(enp->en_nvram_locked, ==, type);

	if ((rc = envop->envo_erase(enp, type)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
efx_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	efx_nvram_ops_t *envop = enp->en_envop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NVRAM);

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);
	EFSYS_ASSERT3U(type, !=, EFX_NVRAM_INVALID);

	EFSYS_ASSERT3U(enp->en_nvram_locked, ==, type);

	if ((rc = envop->envo_write_chunk(enp, type, offset, data, size)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

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

	__checkReturn		int
efx_nvram_set_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint16_t version[4])
{
	efx_nvram_ops_t *envop = enp->en_envop;
	int rc;

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
	EFSYS_PROBE1(fail1, int, rc);

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
