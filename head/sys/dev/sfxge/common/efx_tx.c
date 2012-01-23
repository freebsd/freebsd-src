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

#if EFSYS_OPT_QSTATS
#define	EFX_TX_QSTAT_INCR(_etp, _stat)					\
	do {								\
		(_etp)->et_stat[_stat]++;				\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)
#else
#define	EFX_TX_QSTAT_INCR(_etp, _stat)
#endif

	__checkReturn	int
efx_tx_init(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);

	if (!(enp->en_mod_flags & EFX_MOD_EV)) {
		rc = EINVAL;
		goto fail1;
	}

	if (enp->en_mod_flags & EFX_MOD_TX) {
		rc = EINVAL;
		goto fail2;
	}

	EFSYS_ASSERT3U(enp->en_tx_qcount, ==, 0);

	/*
	 * Disable the timer-based TX DMA backoff and allow TX DMA to be
	 * controlled by the RX FIFO fill level (although always allow a
	 * minimal trickle).
	 */
	EFX_BAR_READO(enp, FR_AZ_TX_RESERVED_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TX_RX_SPACER, 0xfe);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TX_RX_SPACER_EN, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TX_ONE_PKT_PER_Q, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TX_PUSH_EN, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TX_DIS_NON_IP_EV, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TX_PREF_THRESHOLD, 2);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TX_PREF_WD_TMR, 0x3fffff);

	/*
	 * Filter all packets less than 14 bytes to avoid parsing
	 * errors.
	 */
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_TX_FLUSH_MIN_LEN_EN, 1);
	EFX_BAR_WRITEO(enp, FR_AZ_TX_RESERVED_REG, &oword);

	/*
	 * Do not set TX_NO_EOP_DISC_EN, since it limits packets to 16
	 * descriptors (which is bad).
	 */
	EFX_BAR_READO(enp, FR_AZ_TX_CFG_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TX_NO_EOP_DISC_EN, 0);
	EFX_BAR_WRITEO(enp, FR_AZ_TX_CFG_REG, &oword);

	enp->en_mod_flags |= EFX_MOD_TX;
	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_FILTER
extern	__checkReturn	int
efx_tx_filter_insert(
	__in		efx_txq_t *etp,
	__inout		efx_filter_spec_t *spec)
{
	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_dmaq_id = (uint16_t)etp->et_index;
	return efx_filter_insert_filter(etp->et_enp, spec, B_FALSE);
}
#endif

#if EFSYS_OPT_FILTER
extern	__checkReturn	int
efx_tx_filter_remove(
	__in		efx_txq_t *etp,
	__inout		efx_filter_spec_t *spec)
{
	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_dmaq_id = (uint16_t)etp->et_index;
	return efx_filter_remove_filter(etp->et_enp, spec);
}
#endif

#define	EFX_TX_DESC(_etp, _addr, _size, _eop, _added)			\
	do {								\
		unsigned int id;					\
		size_t offset;						\
		efx_qword_t qword;					\
									\
		id = (_added)++ & (_etp)->et_mask;			\
		offset = id * sizeof (efx_qword_t);			\
									\
		EFSYS_PROBE5(tx_post, unsigned int, (_etp)->et_index,	\
		    unsigned int, id, efsys_dma_addr_t, (_addr),	\
		    size_t, (_size), boolean_t, (_eop));		\
									\
		EFX_POPULATE_QWORD_4(qword,				\
		    FSF_AZ_TX_KER_CONT, (_eop) ? 0 : 1,			\
		    FSF_AZ_TX_KER_BYTE_COUNT, (uint32_t)(_size),	\
		    FSF_AZ_TX_KER_BUF_ADDR_DW0,				\
		    (uint32_t)((_addr) & 0xffffffff),			\
		    FSF_AZ_TX_KER_BUF_ADDR_DW1,				\
		    (uint32_t)((_addr) >> 32));				\
		EFSYS_MEM_WRITEQ((_etp)->et_esmp, offset, &qword);	\
									\
		_NOTE(CONSTANTCONDITION)				\
	} while (B_FALSE)

	__checkReturn	int
efx_tx_qpost(
	__in		efx_txq_t *etp,
	__in_ecount(n)	efx_buffer_t *eb,
	__in		unsigned int n,
	__in		unsigned int completed,
	__inout		unsigned int *addedp)
{
	unsigned int added = *addedp;
	unsigned int i;
	int rc = ENOSPC;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	if (added - completed + n > EFX_TXQ_LIMIT(etp->et_mask + 1))
		goto fail1;

	for (i = 0; i < n; i++) {
		efx_buffer_t *ebp = &eb[i];
		efsys_dma_addr_t start = ebp->eb_addr;
		size_t size = ebp->eb_size;
		efsys_dma_addr_t end = start + size;

		/* Fragments must not span 4k boundaries. */
		EFSYS_ASSERT(P2ROUNDUP(start + 1, 4096) >= end);

		EFX_TX_DESC(etp, start, size, ebp->eb_eop, added);
	}

	EFX_TX_QSTAT_INCR(etp, TX_POST);

	*addedp = added;
	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

		void
efx_tx_qpush(
	__in	efx_txq_t *etp,
	__in	unsigned int added)
{
	efx_nic_t *enp = etp->et_enp;
	uint32_t wptr;
	efx_dword_t dword;
	efx_oword_t oword;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	/* Guarantee ordering of memory (descriptors) and PIO (doorbell) */
	EFSYS_PIO_WRITE_BARRIER();

	/* Push the populated descriptors out */
	wptr = added & etp->et_mask;

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_TX_DESC_WPTR, wptr);

	/* Only write the third DWORD */
	EFX_POPULATE_DWORD_1(dword,
	    EFX_DWORD_0, EFX_OWORD_FIELD(oword, EFX_DWORD_3));
	EFX_BAR_TBL_WRITED3(enp, FR_BZ_TX_DESC_UPD_REGP0,
			    etp->et_index, &dword, B_FALSE);
}

		void
efx_tx_qflush(
	__in	efx_txq_t *etp)
{
	efx_nic_t *enp = etp->et_enp;
	efx_oword_t oword;
	uint32_t label;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	label = etp->et_index;

	/* Flush the queue */
	EFX_POPULATE_OWORD_2(oword, FRF_AZ_TX_FLUSH_DESCQ_CMD, 1,
	    FRF_AZ_TX_FLUSH_DESCQ, label);
	EFX_BAR_WRITEO(enp, FR_AZ_TX_FLUSH_DESCQ_REG, &oword);
}

		void
efx_tx_qenable(
	__in	efx_txq_t *etp)
{
	efx_nic_t *enp = etp->et_enp;
	efx_oword_t oword;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	EFX_BAR_TBL_READO(enp, FR_AZ_TX_DESC_PTR_TBL,
			    etp->et_index, &oword);

	EFSYS_PROBE5(tx_descq_ptr, unsigned int, etp->et_index,
	    uint32_t, EFX_OWORD_FIELD(oword, EFX_DWORD_3),
	    uint32_t, EFX_OWORD_FIELD(oword, EFX_DWORD_2),
	    uint32_t, EFX_OWORD_FIELD(oword, EFX_DWORD_1),
	    uint32_t, EFX_OWORD_FIELD(oword, EFX_DWORD_0));

	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TX_DC_HW_RPTR, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TX_DESCQ_HW_RPTR, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TX_DESCQ_EN, 1);

	EFX_BAR_TBL_WRITEO(enp, FR_AZ_TX_DESC_PTR_TBL,
			    etp->et_index, &oword);
}

	__checkReturn	int
efx_tx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		uint16_t flags,
	__in		efx_evq_t *eep,
	__deref_out	efx_txq_t **etpp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_txq_t *etp;
	efx_oword_t oword;
	uint32_t size;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_TX);

	EFX_STATIC_ASSERT(EFX_EV_TX_NLABELS == (1 << FRF_AZ_TX_DESCQ_LABEL_WIDTH));
	EFSYS_ASSERT3U(label, <, EFX_EV_TX_NLABELS);
	EFSYS_ASSERT3U(enp->en_tx_qcount + 1, <, encp->enc_txq_limit);

	if (!ISP2(n) || !(n & EFX_TXQ_NDESCS_MASK)) {
		rc = EINVAL;
		goto fail1;
	}
	if (index >= encp->enc_txq_limit) {
		rc = EINVAL;
		goto fail2;
	}
	for (size = 0; (1 << size) <= (EFX_TXQ_MAXNDESCS / EFX_TXQ_MINNDESCS);
	    size++)
		if ((1 << size) == (int)(n / EFX_TXQ_MINNDESCS))
			break;
	if (id + (1 << size) >= encp->enc_buftbl_limit) {
		rc = EINVAL;
		goto fail3;
	}

	/* Allocate an TXQ object */
	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (efx_txq_t), etp);

	if (etp == NULL) {
		rc = ENOMEM;
		goto fail4;
	}

	etp->et_magic = EFX_TXQ_MAGIC;
	etp->et_enp = enp;
	etp->et_index = index;
	etp->et_mask = n - 1;
	etp->et_esmp = esmp;

	/* Set up the new descriptor queue */
	EFX_POPULATE_OWORD_6(oword,
	    FRF_AZ_TX_DESCQ_BUF_BASE_ID, id,
	    FRF_AZ_TX_DESCQ_EVQ_ID, eep->ee_index,
	    FRF_AZ_TX_DESCQ_OWNER_ID, 0,
	    FRF_AZ_TX_DESCQ_LABEL, label,
	    FRF_AZ_TX_DESCQ_SIZE, size,
	    FRF_AZ_TX_DESCQ_TYPE, 0);

	EFX_SET_OWORD_FIELD(oword, FRF_BZ_TX_NON_IP_DROP_DIS, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_TX_IP_CHKSM_DIS,
	    (flags & EFX_CKSUM_IPV4) ? 0 : 1);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_TX_TCP_CHKSM_DIS,
	    (flags & EFX_CKSUM_TCPUDP) ? 0 : 1);

	EFX_BAR_TBL_WRITEO(enp, FR_AZ_TX_DESC_PTR_TBL,
	    etp->et_index, &oword);

	enp->en_tx_qcount++;
	*etpp = etp;
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

#if EFSYS_OPT_NAMES
/* START MKCONFIG GENERATED EfxTransmitQueueStatNamesBlock 78ca9ab00287fffb */
static const char 	__cs * __cs __efx_tx_qstat_name[] = {
	"post",
	"unaligned_split",
};
/* END MKCONFIG GENERATED EfxTransmitQueueStatNamesBlock */

		const char __cs *
efx_tx_qstat_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id)
{
	_NOTE(ARGUNUSED(enp))
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(id, <, TX_NQSTATS);

	return (__efx_tx_qstat_name[id]);
}
#endif	/* EFSYS_OPT_NAMES */

#if EFSYS_OPT_QSTATS
					void
efx_tx_qstats_update(
	__in				efx_txq_t *etp,
	__inout_ecount(TX_NQSTATS)	efsys_stat_t *stat)
{
	unsigned int id;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	for (id = 0; id < TX_NQSTATS; id++) {
		efsys_stat_t *essp = &stat[id];

		EFSYS_STAT_INCR(essp, etp->et_stat[id]);
		etp->et_stat[id] = 0;
	}
}
#endif	/* EFSYS_OPT_QSTATS */

		void
efx_tx_qdestroy(
	__in	efx_txq_t *etp)
{
	efx_nic_t *enp = etp->et_enp;
	efx_oword_t oword;

	EFSYS_ASSERT3U(etp->et_magic, ==, EFX_TXQ_MAGIC);

	EFSYS_ASSERT(enp->en_tx_qcount != 0);
	--enp->en_tx_qcount;

	/* Purge descriptor queue */
	EFX_ZERO_OWORD(oword);

	EFX_BAR_TBL_WRITEO(enp, FR_AZ_TX_DESC_PTR_TBL,
			    etp->et_index, &oword);

	/* Free the TXQ object */
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_txq_t), etp);
}

		void
efx_tx_fini(
	__in	efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_TX);
	EFSYS_ASSERT3U(enp->en_tx_qcount, ==, 0);

	enp->en_mod_flags &= ~EFX_MOD_TX;
}
