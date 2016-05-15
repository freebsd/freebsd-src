/*-
 * Copyright (c) 2015 Solarflare Communications Inc.
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
 *
 * $FreeBSD$
 */

#ifndef	_SYS_EF10_IMPL_H
#define	_SYS_EF10_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif

#if (EFSYS_OPT_HUNTINGTON && EFSYS_OPT_MEDFORD)
#define	EF10_MAX_PIOBUF_NBUFS	MAX(HUNT_PIOBUF_NBUFS, MEDFORD_PIOBUF_NBUFS)
#elif EFSYS_OPT_HUNTINGTON
#define	EF10_MAX_PIOBUF_NBUFS	HUNT_PIOBUF_NBUFS
#elif EFSYS_OPT_MEDFORD
#define	EF10_MAX_PIOBUF_NBUFS	MEDFORD_PIOBUF_NBUFS
#endif

/*
 * FIXME: This is just a power of 2 which fits in an MCDI v1 message, and could
 * possibly be increased, or the write size reported by newer firmware used
 * instead.
 */
#define	EF10_NVRAM_CHUNK 0x80

/* Alignment requirement for value written to RX WPTR:
 *  the WPTR must be aligned to an 8 descriptor boundary
 */
#define	EF10_RX_WPTR_ALIGN 8

/*
 * Max byte offset into the packet the TCP header must start for the hardware
 * to be able to parse the packet correctly.
 */
#define	EF10_TCP_HEADER_OFFSET_LIMIT	208

/* Invalid RSS context handle */
#define	EF10_RSS_CONTEXT_INVALID	(0xffffffff)


/* EV */

	__checkReturn	efx_rc_t
ef10_ev_init(
	__in		efx_nic_t *enp);

			void
ef10_ev_fini(
	__in		efx_nic_t *enp);

	__checkReturn	efx_rc_t
ef10_ev_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep);

			void
ef10_ev_qdestroy(
	__in		efx_evq_t *eep);

	__checkReturn	efx_rc_t
ef10_ev_qprime(
	__in		efx_evq_t *eep,
	__in		unsigned int count);

			void
ef10_ev_qpost(
	__in	efx_evq_t *eep,
	__in	uint16_t data);

	__checkReturn	efx_rc_t
ef10_ev_qmoderate(
	__in		efx_evq_t *eep,
	__in		unsigned int us);

#if EFSYS_OPT_QSTATS
			void
ef10_ev_qstats_update(
	__in				efx_evq_t *eep,
	__inout_ecount(EV_NQSTATS)	efsys_stat_t *stat);
#endif /* EFSYS_OPT_QSTATS */

		void
ef10_ev_rxlabel_init(
	__in		efx_evq_t *eep,
	__in		efx_rxq_t *erp,
	__in		unsigned int label);

		void
ef10_ev_rxlabel_fini(
	__in		efx_evq_t *eep,
	__in		unsigned int label);

/* INTR */

	__checkReturn	efx_rc_t
ef10_intr_init(
	__in		efx_nic_t *enp,
	__in		efx_intr_type_t type,
	__in		efsys_mem_t *esmp);

			void
ef10_intr_enable(
	__in		efx_nic_t *enp);

			void
ef10_intr_disable(
	__in		efx_nic_t *enp);

			void
ef10_intr_disable_unlocked(
	__in		efx_nic_t *enp);

	__checkReturn	efx_rc_t
ef10_intr_trigger(
	__in		efx_nic_t *enp,
	__in		unsigned int level);

			void
ef10_intr_status_line(
	__in		efx_nic_t *enp,
	__out		boolean_t *fatalp,
	__out		uint32_t *qmaskp);

			void
ef10_intr_status_message(
	__in		efx_nic_t *enp,
	__in		unsigned int message,
	__out		boolean_t *fatalp);

			void
ef10_intr_fatal(
	__in		efx_nic_t *enp);
			void
ef10_intr_fini(
	__in		efx_nic_t *enp);

/* NIC */

extern	__checkReturn	efx_rc_t
ef10_nic_probe(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
ef10_nic_set_drv_limits(
	__inout		efx_nic_t *enp,
	__in		efx_drv_limits_t *edlp);

extern	__checkReturn	efx_rc_t
ef10_nic_get_vi_pool(
	__in		efx_nic_t *enp,
	__out		uint32_t *vi_countp);

extern	__checkReturn	efx_rc_t
ef10_nic_get_bar_region(
	__in		efx_nic_t *enp,
	__in		efx_nic_region_t region,
	__out		uint32_t *offsetp,
	__out		size_t *sizep);

extern	__checkReturn	efx_rc_t
ef10_nic_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
ef10_nic_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

extern	__checkReturn	efx_rc_t
ef10_nic_register_test(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern			void
ef10_nic_fini(
	__in		efx_nic_t *enp);

extern			void
ef10_nic_unprobe(
	__in		efx_nic_t *enp);


/* MAC */

extern	__checkReturn	efx_rc_t
ef10_mac_poll(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *link_modep);

extern	__checkReturn	efx_rc_t
ef10_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp);

extern	__checkReturn	efx_rc_t
ef10_mac_addr_set(
	__in	efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
ef10_mac_pdu_set(
	__in	efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
ef10_mac_reconfigure(
	__in	efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
ef10_mac_multicast_list_set(
	__in				efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
ef10_mac_filter_default_rxq_set(
	__in		efx_nic_t *enp,
	__in		efx_rxq_t *erp,
	__in		boolean_t using_rss);

extern			void
ef10_mac_filter_default_rxq_clear(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_LOOPBACK

extern	__checkReturn	efx_rc_t
ef10_mac_loopback_set(
	__in		efx_nic_t *enp,
	__in		efx_link_mode_t link_mode,
	__in		efx_loopback_type_t loopback_type);

#endif	/* EFSYS_OPT_LOOPBACK */

#if EFSYS_OPT_MAC_STATS

extern	__checkReturn			efx_rc_t
ef10_mac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MAC_NSTATS)	efsys_stat_t *stat,
	__inout_opt			uint32_t *generationp);

#endif	/* EFSYS_OPT_MAC_STATS */


/* MCDI */

#if EFSYS_OPT_MCDI

extern	__checkReturn	efx_rc_t
ef10_mcdi_init(
	__in		efx_nic_t *enp,
	__in		const efx_mcdi_transport_t *mtp);

extern			void
ef10_mcdi_fini(
	__in		efx_nic_t *enp);

extern			void
ef10_mcdi_send_request(
	__in		efx_nic_t *enp,
	__in		void *hdrp,
	__in		size_t hdr_len,
	__in		void *sdup,
	__in		size_t sdu_len);

extern	__checkReturn	boolean_t
ef10_mcdi_poll_response(
	__in		efx_nic_t *enp);

extern			void
ef10_mcdi_read_response(
	__in			efx_nic_t *enp,
	__out_bcount(length)	void *bufferp,
	__in			size_t offset,
	__in			size_t length);

extern			efx_rc_t
ef10_mcdi_poll_reboot(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
ef10_mcdi_feature_supported(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_feature_id_t id,
	__out		boolean_t *supportedp);

#endif /* EFSYS_OPT_MCDI */

/* NVRAM */

#if EFSYS_OPT_NVRAM || EFSYS_OPT_VPD

extern	__checkReturn		efx_rc_t
ef10_nvram_buf_read_tlv(
	__in				efx_nic_t *enp,
	__in_bcount(max_seg_size)	caddr_t seg_data,
	__in				size_t max_seg_size,
	__in				uint32_t tag,
	__deref_out_bcount_opt(*sizep)	caddr_t *datap,
	__out				size_t *sizep);

extern	__checkReturn		efx_rc_t
ef10_nvram_buf_write_tlv(
	__inout_bcount(partn_size)	caddr_t partn_data,
	__in				size_t partn_size,
	__in				uint32_t tag,
	__in_bcount(tag_size)		caddr_t tag_data,
	__in				size_t tag_size,
	__out				size_t *total_lengthp);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_read_tlv(
	__in				efx_nic_t *enp,
	__in				uint32_t partn,
	__in				uint32_t tag,
	__deref_out_bcount_opt(*sizep)	caddr_t *datap,
	__out				size_t *sizep);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_write_tlv(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			uint32_t tag,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_write_segment_tlv(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			uint32_t tag,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			boolean_t all_segments);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_lock(
	__in			efx_nic_t *enp,
	__in			uint32_t partn);

extern				void
ef10_nvram_partn_unlock(
	__in			efx_nic_t *enp,
	__in			uint32_t partn);

#endif /* EFSYS_OPT_NVRAM || EFSYS_OPT_VPD */

#if EFSYS_OPT_NVRAM

#if EFSYS_OPT_DIAG

extern	__checkReturn		efx_rc_t
ef10_nvram_test(
	__in			efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern	__checkReturn		efx_rc_t
ef10_nvram_type_to_partn(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint32_t *partnp);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_size(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__out			size_t *sizep);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_rw_start(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__out			size_t *chunk_sizep);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_read_mode(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			uint32_t mode);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_read(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_erase(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			unsigned int offset,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_write(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
ef10_nvram_partn_rw_finish(
	__in			efx_nic_t *enp,
	__in			uint32_t partn);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_get_version(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4]);

extern	__checkReturn		efx_rc_t
ef10_nvram_partn_set_version(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in_ecount(4)		uint16_t version[4]);

extern	__checkReturn		efx_rc_t
ef10_nvram_buffer_validate(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in_bcount(buffer_size)
				caddr_t bufferp,
	__in			size_t buffer_size);

extern	__checkReturn		efx_rc_t
ef10_nvram_buffer_create(
	__in			efx_nic_t *enp,
	__in			uint16_t partn_type,
	__in_bcount(buffer_size)
				caddr_t bufferp,
	__in			size_t buffer_size);

extern	__checkReturn		efx_rc_t
ef10_nvram_buffer_find_item_start(
	__in_bcount(buffer_size)
				caddr_t bufferp,
	__in			size_t buffer_size,
	__out			uint32_t *startp
	);

extern	__checkReturn		efx_rc_t
ef10_nvram_buffer_find_end(
	__in_bcount(buffer_size)
				caddr_t bufferp,
	__in			size_t buffer_size,
	__in			uint32_t offset,
	__out			uint32_t *endp
	);

extern	__checkReturn	__success(return != B_FALSE)	boolean_t
ef10_nvram_buffer_find_item(
	__in_bcount(buffer_size)
				caddr_t bufferp,
	__in			size_t buffer_size,
	__in			uint32_t offset,
	__out			uint32_t *startp,
	__out			uint32_t *lengthp
	);

extern	__checkReturn		efx_rc_t
ef10_nvram_buffer_get_item(
	__in_bcount(buffer_size)
				caddr_t bufferp,
	__in			size_t buffer_size,
	__in			uint32_t offset,
	__in			uint32_t length,
	__out_bcount_part(item_max_size, *lengthp)
				caddr_t itemp,
	__in			size_t item_max_size,
	__out			uint32_t *lengthp
	);

extern	__checkReturn		efx_rc_t
ef10_nvram_buffer_insert_item(
	__in_bcount(buffer_size)
				caddr_t bufferp,
	__in			size_t buffer_size,
	__in			uint32_t offset,
	__in_bcount(length)	caddr_t keyp,
	__in			uint32_t length,
	__out			uint32_t *lengthp
	);

extern	__checkReturn		efx_rc_t
ef10_nvram_buffer_delete_item(
	__in_bcount(buffer_size)
				caddr_t bufferp,
	__in			size_t buffer_size,
	__in			uint32_t offset,
	__in			uint32_t length,
	__in			uint32_t end
	);

extern	__checkReturn		efx_rc_t
ef10_nvram_buffer_finish(
	__in_bcount(buffer_size)
				caddr_t bufferp,
	__in			size_t buffer_size
	);

#endif	/* EFSYS_OPT_NVRAM */


/* PHY */

typedef struct ef10_link_state_s {
	uint32_t		els_adv_cap_mask;
	uint32_t		els_lp_cap_mask;
	unsigned int		els_fcntl;
	efx_link_mode_t		els_link_mode;
#if EFSYS_OPT_LOOPBACK
	efx_loopback_type_t	els_loopback;
#endif
	boolean_t		els_mac_up;
} ef10_link_state_t;

extern			void
ef10_phy_link_ev(
	__in		efx_nic_t *enp,
	__in		efx_qword_t *eqp,
	__out		efx_link_mode_t *link_modep);

extern	__checkReturn	efx_rc_t
ef10_phy_get_link(
	__in		efx_nic_t *enp,
	__out		ef10_link_state_t *elsp);

extern	__checkReturn	efx_rc_t
ef10_phy_power(
	__in		efx_nic_t *enp,
	__in		boolean_t on);

extern	__checkReturn	efx_rc_t
ef10_phy_reconfigure(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
ef10_phy_verify(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
ef10_phy_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip);

#if EFSYS_OPT_PHY_STATS

extern	__checkReturn			efx_rc_t
ef10_phy_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat);

#endif	/* EFSYS_OPT_PHY_STATS */


/* TX */

extern	__checkReturn	efx_rc_t
ef10_tx_init(
	__in		efx_nic_t *enp);

extern			void
ef10_tx_fini(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
ef10_tx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		uint16_t flags,
	__in		efx_evq_t *eep,
	__in		efx_txq_t *etp,
	__out		unsigned int *addedp);

extern		void
ef10_tx_qdestroy(
	__in		efx_txq_t *etp);

extern	__checkReturn	efx_rc_t
ef10_tx_qpost(
	__in		efx_txq_t *etp,
	__in_ecount(n)	efx_buffer_t *eb,
	__in		unsigned int n,
	__in		unsigned int completed,
	__inout		unsigned int *addedp);

extern			void
ef10_tx_qpush(
	__in		efx_txq_t *etp,
	__in		unsigned int added,
	__in		unsigned int pushed);

extern	__checkReturn	efx_rc_t
ef10_tx_qpace(
	__in		efx_txq_t *etp,
	__in		unsigned int ns);

extern	__checkReturn	efx_rc_t
ef10_tx_qflush(
	__in		efx_txq_t *etp);

extern			void
ef10_tx_qenable(
	__in		efx_txq_t *etp);

extern	__checkReturn	efx_rc_t
ef10_tx_qpio_enable(
	__in		efx_txq_t *etp);

extern			void
ef10_tx_qpio_disable(
	__in		efx_txq_t *etp);

extern	__checkReturn	efx_rc_t
ef10_tx_qpio_write(
	__in			efx_txq_t *etp,
	__in_ecount(buf_length)	uint8_t *buffer,
	__in			size_t buf_length,
	__in			size_t pio_buf_offset);

extern	__checkReturn	efx_rc_t
ef10_tx_qpio_post(
	__in			efx_txq_t *etp,
	__in			size_t pkt_length,
	__in			unsigned int completed,
	__inout			unsigned int *addedp);

extern	__checkReturn	efx_rc_t
ef10_tx_qdesc_post(
	__in		efx_txq_t *etp,
	__in_ecount(n)	efx_desc_t *ed,
	__in		unsigned int n,
	__in		unsigned int completed,
	__inout		unsigned int *addedp);

extern	void
ef10_tx_qdesc_dma_create(
	__in	efx_txq_t *etp,
	__in	efsys_dma_addr_t addr,
	__in	size_t size,
	__in	boolean_t eop,
	__out	efx_desc_t *edp);

extern	void
ef10_tx_qdesc_tso_create(
	__in	efx_txq_t *etp,
	__in	uint16_t ipv4_id,
	__in	uint32_t tcp_seq,
	__in	uint8_t	 tcp_flags,
	__out	efx_desc_t *edp);

extern	void
ef10_tx_qdesc_tso2_create(
	__in			efx_txq_t *etp,
	__in			uint16_t ipv4_id,
	__in			uint32_t tcp_seq,
	__in			uint16_t tcp_mss,
	__out_ecount(count)	efx_desc_t *edp,
	__in			int count);

extern	void
ef10_tx_qdesc_vlantci_create(
	__in	efx_txq_t *etp,
	__in	uint16_t vlan_tci,
	__out	efx_desc_t *edp);


#if EFSYS_OPT_QSTATS

extern			void
ef10_tx_qstats_update(
	__in				efx_txq_t *etp,
	__inout_ecount(TX_NQSTATS)	efsys_stat_t *stat);

#endif /* EFSYS_OPT_QSTATS */

typedef uint32_t	efx_piobuf_handle_t;

#define	EFX_PIOBUF_HANDLE_INVALID	((efx_piobuf_handle_t) -1)

extern	__checkReturn	efx_rc_t
ef10_nic_pio_alloc(
	__inout		efx_nic_t *enp,
	__out		uint32_t *bufnump,
	__out		efx_piobuf_handle_t *handlep,
	__out		uint32_t *blknump,
	__out		uint32_t *offsetp,
	__out		size_t *sizep);

extern	__checkReturn	efx_rc_t
ef10_nic_pio_free(
	__inout		efx_nic_t *enp,
	__in		uint32_t bufnum,
	__in		uint32_t blknum);

extern	__checkReturn	efx_rc_t
ef10_nic_pio_link(
	__inout		efx_nic_t *enp,
	__in		uint32_t vi_index,
	__in		efx_piobuf_handle_t handle);

extern	__checkReturn	efx_rc_t
ef10_nic_pio_unlink(
	__inout		efx_nic_t *enp,
	__in		uint32_t vi_index);


/* VPD */

#if EFSYS_OPT_VPD

extern	__checkReturn		efx_rc_t
ef10_vpd_init(
	__in			efx_nic_t *enp);

extern	__checkReturn		efx_rc_t
ef10_vpd_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep);

extern	__checkReturn		efx_rc_t
ef10_vpd_read(
	__in			efx_nic_t *enp,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
ef10_vpd_verify(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
ef10_vpd_reinit(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
ef10_vpd_get(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__inout			efx_vpd_value_t *evvp);

extern	__checkReturn		efx_rc_t
ef10_vpd_set(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			efx_vpd_value_t *evvp);

extern	__checkReturn		efx_rc_t
ef10_vpd_next(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__out			efx_vpd_value_t *evvp,
	__inout			unsigned int *contp);

extern __checkReturn		efx_rc_t
ef10_vpd_write(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
ef10_vpd_fini(
	__in			efx_nic_t *enp);

#endif	/* EFSYS_OPT_VPD */


/* RX */

extern	__checkReturn	efx_rc_t
ef10_rx_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_RX_SCATTER
extern	__checkReturn	efx_rc_t
ef10_rx_scatter_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int buf_size);
#endif	/* EFSYS_OPT_RX_SCATTER */


#if EFSYS_OPT_RX_SCALE

extern	__checkReturn	efx_rc_t
ef10_rx_scale_mode_set(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t alg,
	__in		efx_rx_hash_type_t type,
	__in		boolean_t insert);

extern	__checkReturn	efx_rc_t
ef10_rx_scale_key_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n);

extern	__checkReturn	efx_rc_t
ef10_rx_scale_tbl_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	unsigned int *table,
	__in		size_t n);

extern	__checkReturn	uint32_t
ef10_rx_prefix_hash(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t func,
	__in		uint8_t *buffer);

#endif /* EFSYS_OPT_RX_SCALE */

extern	__checkReturn	efx_rc_t
ef10_rx_prefix_pktlen(
	__in		efx_nic_t *enp,
	__in		uint8_t *buffer,
	__out		uint16_t *lengthp);

extern			void
ef10_rx_qpost(
	__in		efx_rxq_t *erp,
	__in_ecount(n)	efsys_dma_addr_t *addrp,
	__in		size_t size,
	__in		unsigned int n,
	__in		unsigned int completed,
	__in		unsigned int added);

extern			void
ef10_rx_qpush(
	__in		efx_rxq_t *erp,
	__in		unsigned int added,
	__inout		unsigned int *pushedp);

extern	__checkReturn	efx_rc_t
ef10_rx_qflush(
	__in		efx_rxq_t *erp);

extern		void
ef10_rx_qenable(
	__in		efx_rxq_t *erp);

extern	__checkReturn	efx_rc_t
ef10_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep,
	__in		efx_rxq_t *erp);

extern			void
ef10_rx_qdestroy(
	__in		efx_rxq_t *erp);

extern			void
ef10_rx_fini(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_FILTER

typedef struct ef10_filter_handle_s {
	uint32_t	efh_lo;
	uint32_t	efh_hi;
} ef10_filter_handle_t;

typedef struct ef10_filter_entry_s {
	uintptr_t efe_spec; /* pointer to filter spec plus busy bit */
	ef10_filter_handle_t efe_handle;
} ef10_filter_entry_t;

/*
 * BUSY flag indicates that an update is in progress.
 * AUTO_OLD flag is used to mark and sweep MAC packet filters.
 */
#define	EFX_EF10_FILTER_FLAG_BUSY	1U
#define	EFX_EF10_FILTER_FLAG_AUTO_OLD	2U
#define	EFX_EF10_FILTER_FLAGS		3U

/*
 * Size of the hash table used by the driver. Doesn't need to be the
 * same size as the hardware's table.
 */
#define	EFX_EF10_FILTER_TBL_ROWS 8192

/* Only need to allow for one directed and one unknown unicast filter */
#define	EFX_EF10_FILTER_UNICAST_FILTERS_MAX	2

/* Allow for the broadcast address to be added to the multicast list */
#define	EFX_EF10_FILTER_MULTICAST_FILTERS_MAX	(EFX_MAC_MULTICAST_LIST_MAX + 1)

typedef struct ef10_filter_table_s {
	ef10_filter_entry_t	eft_entry[EFX_EF10_FILTER_TBL_ROWS];
	efx_rxq_t *		eft_default_rxq;
	boolean_t		eft_using_rss;
	uint32_t		eft_unicst_filter_indexes[
	    EFX_EF10_FILTER_UNICAST_FILTERS_MAX];
	boolean_t		eft_unicst_filter_count;
	uint32_t		eft_mulcst_filter_indexes[
	    EFX_EF10_FILTER_MULTICAST_FILTERS_MAX];
	uint32_t		eft_mulcst_filter_count;
	boolean_t		eft_using_all_mulcst;
} ef10_filter_table_t;

	__checkReturn	efx_rc_t
ef10_filter_init(
	__in		efx_nic_t *enp);

			void
ef10_filter_fini(
	__in		efx_nic_t *enp);

	__checkReturn	efx_rc_t
ef10_filter_restore(
	__in		efx_nic_t *enp);

	__checkReturn	efx_rc_t
ef10_filter_add(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec,
	__in		boolean_t may_replace);

	__checkReturn	efx_rc_t
ef10_filter_delete(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec);

extern	__checkReturn	efx_rc_t
ef10_filter_supported_filters(
	__in		efx_nic_t *enp,
	__out		uint32_t *list,
	__out		size_t *length);

extern	__checkReturn	efx_rc_t
ef10_filter_reconfigure(
	__in				efx_nic_t *enp,
	__in_ecount(6)			uint8_t const *mac_addr,
	__in				boolean_t all_unicst,
	__in				boolean_t mulcst,
	__in				boolean_t all_mulcst,
	__in				boolean_t brdcst,
	__in_ecount(6*count)		uint8_t const *addrs,
	__in				uint32_t count);

extern		void
ef10_filter_get_default_rxq(
	__in		efx_nic_t *enp,
	__out		efx_rxq_t **erpp,
	__out		boolean_t *using_rss);

extern		void
ef10_filter_default_rxq_set(
	__in		efx_nic_t *enp,
	__in		efx_rxq_t *erp,
	__in		boolean_t using_rss);

extern		void
ef10_filter_default_rxq_clear(
	__in		efx_nic_t *enp);


#endif /* EFSYS_OPT_FILTER */

extern	__checkReturn			efx_rc_t
efx_mcdi_get_function_info(
	__in				efx_nic_t *enp,
	__out				uint32_t *pfp,
	__out_opt			uint32_t *vfp);

extern	__checkReturn		efx_rc_t
efx_mcdi_privilege_mask(
	__in			efx_nic_t *enp,
	__in			uint32_t pf,
	__in			uint32_t vf,
	__out			uint32_t *maskp);

extern	__checkReturn	efx_rc_t
efx_mcdi_get_port_assignment(
	__in		efx_nic_t *enp,
	__out		uint32_t *portp);

extern	__checkReturn	efx_rc_t
efx_mcdi_get_port_modes(
	__in		efx_nic_t *enp,
	__out		uint32_t *modesp);

extern	__checkReturn	efx_rc_t
efx_mcdi_get_mac_address_pf(
	__in			efx_nic_t *enp,
	__out_ecount_opt(6)	uint8_t mac_addrp[6]);

extern	__checkReturn	efx_rc_t
efx_mcdi_get_mac_address_vf(
	__in			efx_nic_t *enp,
	__out_ecount_opt(6)	uint8_t mac_addrp[6]);

extern	__checkReturn	efx_rc_t
efx_mcdi_get_clock(
	__in		efx_nic_t *enp,
	__out		uint32_t *sys_freqp);

extern	__checkReturn	efx_rc_t
efx_mcdi_get_vector_cfg(
	__in		efx_nic_t *enp,
	__out_opt	uint32_t *vec_basep,
	__out_opt	uint32_t *pf_nvecp,
	__out_opt	uint32_t *vf_nvecp);

extern	__checkReturn	efx_rc_t
ef10_get_datapath_caps(
	__in		efx_nic_t *enp);

extern	__checkReturn		efx_rc_t
ef10_get_privilege_mask(
	__in			efx_nic_t *enp,
	__out			uint32_t *maskp);

extern	__checkReturn	efx_rc_t
ef10_external_port_mapping(
	__in		efx_nic_t *enp,
	__in		uint32_t port,
	__out		uint8_t *external_portp);


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EF10_IMPL_H */
