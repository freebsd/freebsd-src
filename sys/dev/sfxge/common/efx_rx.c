/*-
 * Copyright 2007-2009 Solarflare Communications Inc.  All rights reserved.
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

	__checkReturn	int
efx_rx_init(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	unsigned int index;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);

	if (!(enp->en_mod_flags & EFX_MOD_EV)) {
		rc = EINVAL;
		goto fail1;
	}

	if (enp->en_mod_flags & EFX_MOD_RX) {
		rc = EINVAL;
		goto fail2;
	}

	EFX_BAR_READO(enp, FR_AZ_RX_CFG_REG, &oword);

	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_DESC_PUSH_EN, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_ALG, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_IP_HASH, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_TCP_SUP, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_INSRT_HDR, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_USR_BUF_SIZE, 0x3000 / 32);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_CFG_REG, &oword);

	/* Zero the RSS table */
	for (index = 0; index < FR_BZ_RX_INDIRECTION_TBL_ROWS;
	    index++) {
		EFX_ZERO_OWORD(oword);
		EFX_BAR_TBL_WRITEO(enp, FR_BZ_RX_INDIRECTION_TBL,
				    index, &oword);
	}

	enp->en_mod_flags |= EFX_MOD_RX;
	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_RX_HDR_SPLIT
	__checkReturn	int
efx_rx_hdr_split_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int hdr_buf_size,
	__in		unsigned int pld_buf_size)
{
	unsigned int nhdr32;
	unsigned int npld32;
	efx_oword_t oword;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);
	EFSYS_ASSERT3U(enp->en_family, >=, EFX_FAMILY_SIENA);

	nhdr32 = hdr_buf_size / 32;
	if ((nhdr32 == 0) ||
	    (nhdr32 >= (1 << FRF_CZ_RX_HDR_SPLIT_HDR_BUF_SIZE_WIDTH)) ||
	    ((hdr_buf_size % 32) != 0)) {
		rc = EINVAL;
		goto fail1;
	}

	npld32 = pld_buf_size / 32;
	if ((npld32 == 0) ||
	    (npld32 >= (1 << FRF_CZ_RX_HDR_SPLIT_PLD_BUF_SIZE_WIDTH)) ||
	    ((pld_buf_size % 32) != 0)) {
		rc = EINVAL;
		goto fail2;
	}

	if (enp->en_rx_qcount > 0) {
		rc = EBUSY;
		goto fail3;
	}

	EFX_BAR_READO(enp, FR_AZ_RX_CFG_REG, &oword);

	EFX_SET_OWORD_FIELD(oword, FRF_CZ_RX_HDR_SPLIT_EN, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_CZ_RX_HDR_SPLIT_HDR_BUF_SIZE, nhdr32);
	EFX_SET_OWORD_FIELD(oword, FRF_CZ_RX_HDR_SPLIT_PLD_BUF_SIZE, npld32);

	EFX_BAR_WRITEO(enp, FR_AZ_RX_CFG_REG, &oword);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}
#endif	/* EFSYS_OPT_RX_HDR_SPLIT */


#if EFSYS_OPT_RX_SCATTER
	__checkReturn	int
efx_rx_scatter_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int buf_size)
{
	unsigned int nbuf32;
	efx_oword_t oword;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);
	EFSYS_ASSERT3U(enp->en_family, >=, EFX_FAMILY_FALCON);

	nbuf32 = buf_size / 32;
	if ((nbuf32 == 0) ||
	    (nbuf32 >= (1 << FRF_BZ_RX_USR_BUF_SIZE_WIDTH)) ||
	    ((buf_size % 32) != 0)) {
		rc = EINVAL;
		goto fail1;
	}

	if (enp->en_rx_qcount > 0) {
		rc = EBUSY;
		goto fail2;
	}

	/* Set scatter buffer size */
	EFX_BAR_READO(enp, FR_AZ_RX_CFG_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_USR_BUF_SIZE, nbuf32);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_CFG_REG, &oword);

	/* Enable scatter for packets not matching a filter */
	EFX_BAR_READO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_SCATTER_ENBL_NO_MATCH_Q, 1);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}
#endif	/* EFSYS_OPT_RX_SCATTER */


#define	EFX_RX_LFSR_HASH(_enp, _insert)					\
	do {                                    			\
		efx_oword_t oword;					\
									\
		EFX_BAR_READO((_enp), FR_AZ_RX_CFG_REG, &oword);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_ALG, 0);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_IP_HASH, 0);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_TCP_SUP, 0);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_INSRT_HDR,	\
		    (_insert) ? 1 : 0);					\
		EFX_BAR_WRITEO((_enp), FR_AZ_RX_CFG_REG, &oword);	\
									\
		if ((_enp)->en_family == EFX_FAMILY_SIENA) {		\
			EFX_BAR_READO((_enp), FR_CZ_RX_RSS_IPV6_REG3,	\
			    &oword);					\
			EFX_SET_OWORD_FIELD(oword,			\
			    FRF_CZ_RX_RSS_IPV6_THASH_ENABLE, 0);	\
			EFX_BAR_WRITEO((_enp), FR_CZ_RX_RSS_IPV6_REG3,	\
			    &oword);					\
		}							\
									\
		_NOTE(CONSTANTCONDITION)				\
	} while (B_FALSE)

#define	EFX_RX_TOEPLITZ_IPV4_HASH(_enp, _insert, _ip, _tcp)		\
	do {                                    			\
		efx_oword_t oword;					\
									\
		EFX_BAR_READO((_enp), FR_AZ_RX_CFG_REG,	&oword);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_ALG, 1);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_IP_HASH,		\
		    (_ip) ? 1 : 0);					\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_TCP_SUP,		\
		    (_tcp) ? 0 : 1);					\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_INSRT_HDR,	\
		    (_insert) ? 1 : 0);					\
		EFX_BAR_WRITEO((_enp), FR_AZ_RX_CFG_REG, &oword);	\
									\
		_NOTE(CONSTANTCONDITION)				\
	} while (B_FALSE)

#define	EFX_RX_TOEPLITZ_IPV6_HASH(_enp, _ip, _tcp, _rc)			\
	do {                                    			\
		efx_oword_t oword;					\
									\
		if ((_enp)->en_family == EFX_FAMILY_FALCON) {		\
			(_rc) = ((_ip) || (_tcp)) ? ENOTSUP : 0;	\
			break;						\
		}							\
									\
		EFX_BAR_READO((_enp), FR_CZ_RX_RSS_IPV6_REG3, &oword);	\
		EFX_SET_OWORD_FIELD(oword,				\
		    FRF_CZ_RX_RSS_IPV6_THASH_ENABLE, 1);		\
		EFX_SET_OWORD_FIELD(oword,				\
		    FRF_CZ_RX_RSS_IPV6_IP_THASH_ENABLE, (_ip) ? 1 : 0);	\
		EFX_SET_OWORD_FIELD(oword,				\
		    FRF_CZ_RX_RSS_IPV6_TCP_SUPPRESS, (_tcp) ? 0 : 1);	\
		EFX_BAR_WRITEO((_enp), FR_CZ_RX_RSS_IPV6_REG3, &oword);	\
									\
		(_rc) = 0;						\
									\
		_NOTE(CONSTANTCONDITION)				\
	} while (B_FALSE)


#if EFSYS_OPT_RX_SCALE
	__checkReturn	int
efx_rx_scale_mode_set(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t alg,
	__in		efx_rx_hash_type_t type,
	__in		boolean_t insert)
{
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);
	EFSYS_ASSERT3U(enp->en_family, >=, EFX_FAMILY_FALCON);

	switch (alg) {
	case EFX_RX_HASHALG_LFSR:
		EFX_RX_LFSR_HASH(enp, insert);
		break;

	case EFX_RX_HASHALG_TOEPLITZ:
		EFX_RX_TOEPLITZ_IPV4_HASH(enp, insert,
		    type & (1 << EFX_RX_HASH_IPV4),
		    type & (1 << EFX_RX_HASH_TCPIPV4));

		EFX_RX_TOEPLITZ_IPV6_HASH(enp,
		    type & (1 << EFX_RX_HASH_IPV6),
		    type & (1 << EFX_RX_HASH_TCPIPV6),
		    rc);
		if (rc != 0)
			goto fail1;

		break;

	default:
		rc = EINVAL;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	EFX_RX_LFSR_HASH(enp, B_FALSE);

	return (rc);
}
#endif

#if EFSYS_OPT_RX_SCALE
	__checkReturn	int
efx_rx_scale_toeplitz_ipv4_key_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n)
{
	efx_oword_t oword;
	unsigned int byte;
	unsigned int offset;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	byte = 0;

	/* Write toeplitz hash key */
	EFX_ZERO_OWORD(oword);
	for (offset = (FRF_BZ_RX_RSS_TKEY_LBN + FRF_BZ_RX_RSS_TKEY_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset)
		oword.eo_u8[offset - 1] = key[byte++];

	EFX_BAR_WRITEO(enp, FR_BZ_RX_RSS_TKEY_REG, &oword);

	byte = 0;

	/* Verify toeplitz hash key */
	EFX_BAR_READO(enp, FR_BZ_RX_RSS_TKEY_REG, &oword);
	for (offset = (FRF_BZ_RX_RSS_TKEY_LBN + FRF_BZ_RX_RSS_TKEY_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset) {
		if (oword.eo_u8[offset - 1] != key[byte++]) {
			rc = EFAULT;
			goto fail1;
		}
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}
#endif

#if EFSYS_OPT_RX_SCALE
	__checkReturn	int
efx_rx_scale_toeplitz_ipv6_key_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n)
{
	efx_oword_t oword;
	unsigned int byte;
	int offset;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	byte = 0;

	/* Write toeplitz hash key 3 */
	EFX_BAR_READO(enp, FR_CZ_RX_RSS_IPV6_REG3, &oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_HI_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_HI_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset)
		oword.eo_u8[offset - 1] = key[byte++];

	EFX_BAR_WRITEO(enp, FR_CZ_RX_RSS_IPV6_REG3, &oword);

	/* Write toeplitz hash key 2 */
	EFX_ZERO_OWORD(oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_MID_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_MID_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset)
		oword.eo_u8[offset - 1] = key[byte++];

	EFX_BAR_WRITEO(enp, FR_CZ_RX_RSS_IPV6_REG2, &oword);

	/* Write toeplitz hash key 1 */
	EFX_ZERO_OWORD(oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_LO_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_LO_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset)
		oword.eo_u8[offset - 1] = key[byte++];

	EFX_BAR_WRITEO(enp, FR_CZ_RX_RSS_IPV6_REG1, &oword);

	byte = 0;

	/* Verify toeplitz hash key 3 */
	EFX_BAR_READO(enp, FR_CZ_RX_RSS_IPV6_REG3, &oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_HI_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_HI_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset) {
		if (oword.eo_u8[offset - 1] != key[byte++]) {
			rc = EFAULT;
			goto fail1;
		}
	}

	/* Verify toeplitz hash key 2 */
	EFX_BAR_READO(enp, FR_CZ_RX_RSS_IPV6_REG2, &oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_MID_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_MID_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset) {
		if (oword.eo_u8[offset - 1] != key[byte++]) {
			rc = EFAULT;
			goto fail2;
		}
	}

	/* Verify toeplitz hash key 1 */
	EFX_BAR_READO(enp, FR_CZ_RX_RSS_IPV6_REG1, &oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_LO_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_LO_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset) {
		if (oword.eo_u8[offset - 1] != key[byte++]) {
			rc = EFAULT;
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
#endif

#if EFSYS_OPT_RX_SCALE
	__checkReturn	int
efx_rx_scale_tbl_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	unsigned int *table,
	__in		size_t n)
{
	efx_oword_t oword;
	int index;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	EFX_STATIC_ASSERT(EFX_RSS_TBL_SIZE == FR_BZ_RX_INDIRECTION_TBL_ROWS);
	EFX_STATIC_ASSERT(EFX_MAXRSS == (1 << FRF_BZ_IT_QUEUE_WIDTH));

	if (n > FR_BZ_RX_INDIRECTION_TBL_ROWS) {
		rc = EINVAL;
		goto fail1;
	}

	for (index = 0; index < FR_BZ_RX_INDIRECTION_TBL_ROWS; index++) {
		uint32_t byte;

		/* Calculate the entry to place in the table */
		byte = (uint32_t)table[index % n];

		EFSYS_PROBE2(table, int, index, uint32_t, byte);

		EFX_POPULATE_OWORD_1(oword, FRF_BZ_IT_QUEUE, byte);

		/* Write the table */
		EFX_BAR_TBL_WRITEO(enp, FR_BZ_RX_INDIRECTION_TBL,
				    index, &oword);
	}

	for (index = FR_BZ_RX_INDIRECTION_TBL_ROWS - 1; index >= 0; --index) {
		uint32_t byte;

		/* Determine if we're starting a new batch */
		byte = (uint32_t)table[index % n];

		/* Read the table */
		EFX_BAR_TBL_READO(enp, FR_BZ_RX_INDIRECTION_TBL,
				    index, &oword);

		/* Verify the entry */
		if (EFX_OWORD_FIELD(oword, FRF_BZ_IT_QUEUE) != byte) {
			rc = EFAULT;
			goto fail2;
		}
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}
#endif

#if EFSYS_OPT_FILTER
extern	__checkReturn	int
efx_rx_filter_insert(
	__in		efx_rxq_t *erp,
	__inout		efx_filter_spec_t *spec)
{
	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_dmaq_id = (uint16_t)erp->er_index;
	return efx_filter_insert_filter(erp->er_enp, spec, B_FALSE);
}
#endif

#if EFSYS_OPT_FILTER
extern	__checkReturn	int
efx_rx_filter_remove(
	__in		efx_rxq_t *erp,
	__inout		efx_filter_spec_t *spec)
{
	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_dmaq_id = (uint16_t)erp->er_index;
	return efx_filter_remove_filter(erp->er_enp, spec);
}
#endif

extern			void
efx_rx_qpost(
	__in		efx_rxq_t *erp,
	__in_ecount(n)	efsys_dma_addr_t *addrp,
	__in		size_t size,
	__in		unsigned int n,
	__in		unsigned int completed,
	__in		unsigned int added)
{
	efx_qword_t qword;
	unsigned int i;
	unsigned int offset;
	unsigned int id;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	/* The client driver must not overfill the queue */
	EFSYS_ASSERT3U(added - completed + n, <=,
	    EFX_RXQ_LIMIT(erp->er_mask + 1));

	id = added & (erp->er_mask);
	for (i = 0; i < n; i++) {
		EFSYS_PROBE4(rx_post, unsigned int, erp->er_index,
		    unsigned int, id, efsys_dma_addr_t, addrp[i],
		    size_t, size);

		EFX_POPULATE_QWORD_3(qword,
		    FSF_AZ_RX_KER_BUF_SIZE, (uint32_t)(size),
		    FSF_AZ_RX_KER_BUF_ADDR_DW0,
		    (uint32_t)(addrp[i] & 0xffffffff),
		    FSF_AZ_RX_KER_BUF_ADDR_DW1,
		    (uint32_t)(addrp[i] >> 32));

		offset = id * sizeof (efx_qword_t);
		EFSYS_MEM_WRITEQ(erp->er_esmp, offset, &qword);

		id = (id + 1) & (erp->er_mask);
	}
}

		void
efx_rx_qpush(
	__in	efx_rxq_t *erp,
	__in	unsigned int added)
{
	efx_nic_t *enp = erp->er_enp;
	uint32_t wptr;
	efx_oword_t oword;
	efx_dword_t dword;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	/* Guarantee ordering of memory (descriptors) and PIO (doorbell) */
	EFSYS_PIO_WRITE_BARRIER();

	/* Push the populated descriptors out */
	wptr = added & erp->er_mask;

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_RX_DESC_WPTR, wptr);

	/* Only write the third DWORD */
	EFX_POPULATE_DWORD_1(dword,
	    EFX_DWORD_0, EFX_OWORD_FIELD(oword, EFX_DWORD_3));
	EFX_BAR_TBL_WRITED3(enp, FR_BZ_RX_DESC_UPD_REGP0,
			    erp->er_index, &dword, B_FALSE);
}

		void
efx_rx_qflush(
	__in	efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	efx_oword_t oword;
	uint32_t label;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	label = erp->er_index;

	/* Flush the queue */
	EFX_POPULATE_OWORD_2(oword, FRF_AZ_RX_FLUSH_DESCQ_CMD, 1,
	    FRF_AZ_RX_FLUSH_DESCQ, label);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_FLUSH_DESCQ_REG, &oword);
}

		void
efx_rx_qenable(
	__in	efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	efx_oword_t oword;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	EFX_BAR_TBL_READO(enp, FR_AZ_RX_DESC_PTR_TBL,
			    erp->er_index, &oword);

	EFX_SET_OWORD_FIELD(oword, FRF_AZ_RX_DC_HW_RPTR, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_RX_DESCQ_HW_RPTR, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_RX_DESCQ_EN, 1);

	EFX_BAR_TBL_WRITEO(enp, FR_AZ_RX_DESC_PTR_TBL,
			    erp->er_index, &oword);
}

	__checkReturn	int
efx_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep,
	__deref_out	efx_rxq_t **erpp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_rxq_t *erp;
	efx_oword_t oword;
	uint32_t size;
	boolean_t split;
	boolean_t jumbo;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	EFX_STATIC_ASSERT(EFX_EV_RX_NLABELS == (1 << FRF_AZ_RX_DESCQ_LABEL_WIDTH));
	EFSYS_ASSERT3U(label, <, EFX_EV_RX_NLABELS);
	EFSYS_ASSERT3U(enp->en_rx_qcount + 1, <, encp->enc_rxq_limit);

	if (!ISP2(n) || !(n & EFX_RXQ_NDESCS_MASK)) {
		rc = EINVAL;
		goto fail1;
	}
	if (index >= encp->enc_rxq_limit) {
		rc = EINVAL;
		goto fail2;
	}
	for (size = 0; (1 << size) <= (EFX_RXQ_MAXNDESCS / EFX_RXQ_MINNDESCS);
	    size++)
		if ((1 << size) == (int)(n / EFX_RXQ_MINNDESCS))
			break;
	if (id + (1 << size) >= encp->enc_buftbl_limit) {
		rc = EINVAL;
		goto fail3;
	}

	switch (type) {
	case EFX_RXQ_TYPE_DEFAULT:
		split = B_FALSE;
		jumbo = B_FALSE;
		break;

#if EFSYS_OPT_RX_HDR_SPLIT
	case EFX_RXQ_TYPE_SPLIT_HEADER:
		if ((enp->en_family < EFX_FAMILY_SIENA) || ((index & 1) != 0)) {
			rc = EINVAL;
			goto fail4;
		}
		split = B_TRUE;
		jumbo = B_TRUE;
		break;

	case EFX_RXQ_TYPE_SPLIT_PAYLOAD:
		if ((enp->en_family < EFX_FAMILY_SIENA) || ((index & 1) == 0)) {
			rc = EINVAL;
			goto fail4;
		}
		split = B_FALSE;
		jumbo = B_TRUE;
		break;
#endif	/* EFSYS_OPT_RX_HDR_SPLIT */

#if EFSYS_OPT_RX_SCATTER
	case EFX_RXQ_TYPE_SCATTER:
		if (enp->en_family < EFX_FAMILY_SIENA) {
			rc = EINVAL;
			goto fail4;
		}
		split = B_FALSE;
		jumbo = B_TRUE;
		break;
#endif	/* EFSYS_OPT_RX_SCATTER */

	default:
		rc = EINVAL;
		goto fail4;
	}

	/* Allocate an RXQ object */
	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (efx_rxq_t), erp);

	if (erp == NULL) {
		rc = ENOMEM;
		goto fail5;
	}

	erp->er_magic = EFX_RXQ_MAGIC;
	erp->er_enp = enp;
	erp->er_index = index;
	erp->er_mask = n - 1;
	erp->er_esmp = esmp;

	/* Set up the new descriptor queue */
	EFX_POPULATE_OWORD_10(oword,
	    FRF_CZ_RX_HDR_SPLIT, split,
	    FRF_AZ_RX_ISCSI_DDIG_EN, 0,
	    FRF_AZ_RX_ISCSI_HDIG_EN, 0,
	    FRF_AZ_RX_DESCQ_BUF_BASE_ID, id,
	    FRF_AZ_RX_DESCQ_EVQ_ID, eep->ee_index,
	    FRF_AZ_RX_DESCQ_OWNER_ID, 0,
	    FRF_AZ_RX_DESCQ_LABEL, label,
	    FRF_AZ_RX_DESCQ_SIZE, size,
	    FRF_AZ_RX_DESCQ_TYPE, 0,
	    FRF_AZ_RX_DESCQ_JUMBO, jumbo);

	EFX_BAR_TBL_WRITEO(enp, FR_AZ_RX_DESC_PTR_TBL,
			    erp->er_index, &oword);

	enp->en_rx_qcount++;
	*erpp = erp;
	return (0);

fail5:
	EFSYS_PROBE(fail5);
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

		void
efx_rx_qdestroy(
	__in	efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	efx_oword_t oword;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	EFSYS_ASSERT(enp->en_rx_qcount != 0);
	--enp->en_rx_qcount;

	/* Purge descriptor queue */
	EFX_ZERO_OWORD(oword);

	EFX_BAR_TBL_WRITEO(enp, FR_AZ_RX_DESC_PTR_TBL,
			    erp->er_index, &oword);

	/* Free the RXQ object */
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_rxq_t), erp);
}

		void
efx_rx_fini(
	__in	efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);
	EFSYS_ASSERT3U(enp->en_rx_qcount, ==, 0);

	enp->en_mod_flags &= ~EFX_MOD_RX;
}
