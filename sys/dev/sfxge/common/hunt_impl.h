/*-
 * Copyright (c) 2012-2015 Solarflare Communications Inc.
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

#ifndef _SYS_HUNT_IMPL_H
#define	_SYS_HUNT_IMPL_H

#include "efx.h"
#include "efx_regs.h"
#include "efx_regs_ef10.h"
#include "efx_mcdi.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define	HUNTINGTON_NVRAM_CHUNK 0x80

/* Alignment requirement for value written to RX WPTR:
 *  the WPTR must be aligned to an 8 descriptor boundary
 */
#define	HUNTINGTON_RX_WPTR_ALIGN 8

/* Invalid RSS context handle */
#define	HUNTINGTON_RSS_CONTEXT_INVALID	(0xffffffff)


/* EV */

	__checkReturn	efx_rc_t
hunt_ev_init(
	__in		efx_nic_t *enp);

			void
hunt_ev_fini(
	__in		efx_nic_t *enp);

	__checkReturn	efx_rc_t
hunt_ev_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep);

			void
hunt_ev_qdestroy(
	__in		efx_evq_t *eep);

	__checkReturn	efx_rc_t
hunt_ev_qprime(
	__in		efx_evq_t *eep,
	__in		unsigned int count);

			void
hunt_ev_qpost(
	__in	efx_evq_t *eep,
	__in	uint16_t data);

	__checkReturn	efx_rc_t
hunt_ev_qmoderate(
	__in		efx_evq_t *eep,
	__in		unsigned int us);

#if EFSYS_OPT_QSTATS
			void
hunt_ev_qstats_update(
	__in				efx_evq_t *eep,
	__inout_ecount(EV_NQSTATS)	efsys_stat_t *stat);
#endif /* EFSYS_OPT_QSTATS */

		void
hunt_ev_rxlabel_init(
	__in		efx_evq_t *eep,
	__in		efx_rxq_t *erp,
	__in		unsigned int label);

		void
hunt_ev_rxlabel_fini(
	__in		efx_evq_t *eep,
	__in		unsigned int label);

/* INTR */

	__checkReturn	efx_rc_t
hunt_intr_init(
	__in		efx_nic_t *enp,
	__in		efx_intr_type_t type,
	__in		efsys_mem_t *esmp);

			void
hunt_intr_enable(
	__in		efx_nic_t *enp);

			void
hunt_intr_disable(
	__in		efx_nic_t *enp);

			void
hunt_intr_disable_unlocked(
	__in		efx_nic_t *enp);

	__checkReturn	efx_rc_t
hunt_intr_trigger(
	__in		efx_nic_t *enp,
	__in		unsigned int level);

			void
hunt_intr_fini(
	__in		efx_nic_t *enp);

/* NIC */

extern	__checkReturn	efx_rc_t
hunt_nic_probe(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
hunt_nic_set_drv_limits(
	__inout		efx_nic_t *enp,
	__in		efx_drv_limits_t *edlp);

extern	__checkReturn	efx_rc_t
hunt_nic_get_vi_pool(
	__in		efx_nic_t *enp,
	__out		uint32_t *vi_countp);

extern	__checkReturn	efx_rc_t
hunt_nic_get_bar_region(
	__in		efx_nic_t *enp,
	__in		efx_nic_region_t region,
	__out		uint32_t *offsetp,
	__out		size_t *sizep);

extern	__checkReturn	efx_rc_t
hunt_nic_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
hunt_nic_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

extern	__checkReturn	efx_rc_t
hunt_nic_register_test(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern			void
hunt_nic_fini(
	__in		efx_nic_t *enp);

extern			void
hunt_nic_unprobe(
	__in		efx_nic_t *enp);


/* MAC */

extern	__checkReturn	efx_rc_t
hunt_mac_poll(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *link_modep);

extern	__checkReturn	efx_rc_t
hunt_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp);

extern	__checkReturn	efx_rc_t
hunt_mac_addr_set(
	__in	efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
hunt_mac_reconfigure(
	__in	efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
hunt_mac_multicast_list_set(
	__in				efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
hunt_mac_filter_default_rxq_set(
	__in		efx_nic_t *enp,
	__in		efx_rxq_t *erp,
	__in		boolean_t using_rss);

extern			void
hunt_mac_filter_default_rxq_clear(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_LOOPBACK

extern	__checkReturn	efx_rc_t
hunt_mac_loopback_set(
	__in		efx_nic_t *enp,
	__in		efx_link_mode_t link_mode,
	__in		efx_loopback_type_t loopback_type);

#endif	/* EFSYS_OPT_LOOPBACK */

#if EFSYS_OPT_MAC_STATS

extern	__checkReturn			efx_rc_t
hunt_mac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MAC_NSTATS)	efsys_stat_t *stat,
	__inout_opt			uint32_t *generationp);

#endif	/* EFSYS_OPT_MAC_STATS */


/* MCDI */

#if EFSYS_OPT_MCDI

extern	__checkReturn	efx_rc_t
hunt_mcdi_init(
	__in		efx_nic_t *enp,
	__in		const efx_mcdi_transport_t *mtp);

extern			void
hunt_mcdi_fini(
	__in		efx_nic_t *enp);

extern			void
hunt_mcdi_request_copyin(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__in		unsigned int seq,
	__in		boolean_t ev_cpl,
	__in		boolean_t new_epoch);

extern	__checkReturn	boolean_t
hunt_mcdi_request_poll(
	__in		efx_nic_t *enp);

extern			void
hunt_mcdi_request_copyout(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp);

extern			efx_rc_t
hunt_mcdi_poll_reboot(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
hunt_mcdi_fw_update_supported(
	__in		efx_nic_t *enp,
	__out		boolean_t *supportedp);

extern	__checkReturn	efx_rc_t
hunt_mcdi_macaddr_change_supported(
	__in		efx_nic_t *enp,
	__out		boolean_t *supportedp);

extern	__checkReturn	efx_rc_t
hunt_mcdi_link_control_supported(
	__in		efx_nic_t *enp,
	__out		boolean_t *supportedp);

#endif /* EFSYS_OPT_MCDI */

/* NVRAM */

#if EFSYS_OPT_NVRAM || EFSYS_OPT_VPD

extern	__checkReturn		efx_rc_t
hunt_nvram_buf_read_tlv(
	__in				efx_nic_t *enp,
	__in_bcount(max_seg_size)	caddr_t seg_data,
	__in				size_t max_seg_size,
	__in				uint32_t tag,
	__deref_out_bcount_opt(*sizep)	caddr_t *datap,
	__out				size_t *sizep);

extern	__checkReturn		efx_rc_t
hunt_nvram_buf_write_tlv(
	__inout_bcount(partn_size)	caddr_t partn_data,
	__in				size_t partn_size,
	__in				uint32_t tag,
	__in_bcount(tag_size)		caddr_t tag_data,
	__in				size_t tag_size,
	__out				size_t *total_lengthp);

extern	__checkReturn		efx_rc_t
hunt_nvram_partn_read_tlv(
	__in				efx_nic_t *enp,
	__in				uint32_t partn,
	__in				uint32_t tag,
	__deref_out_bcount_opt(*sizep)	caddr_t *datap,
	__out				size_t *sizep);

extern	__checkReturn		efx_rc_t
hunt_nvram_partn_write_tlv(
	__in		   	efx_nic_t *enp,
	__in		    	uint32_t partn,
	__in		     	uint32_t tag,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
hunt_nvram_partn_write_segment_tlv(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			uint32_t tag,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			boolean_t all_segments);

extern	__checkReturn		efx_rc_t
hunt_nvram_partn_size(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__out			size_t *sizep);

extern	__checkReturn		efx_rc_t
hunt_nvram_partn_lock(
	__in			efx_nic_t *enp,
	__in			unsigned int partn);

extern	__checkReturn		efx_rc_t
hunt_nvram_partn_read(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
hunt_nvram_partn_erase(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
hunt_nvram_partn_write(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
hunt_nvram_partn_unlock(
	__in			efx_nic_t *enp,
	__in			unsigned int partn);

#endif /* EFSYS_OPT_NVRAM || EFSYS_OPT_VPD */

#if EFSYS_OPT_NVRAM

#if EFSYS_OPT_DIAG

extern	__checkReturn		efx_rc_t
hunt_nvram_test(
	__in			efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern	__checkReturn		efx_rc_t
hunt_nvram_size(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *sizep);

extern	__checkReturn		efx_rc_t
hunt_nvram_get_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4]);

extern	__checkReturn		efx_rc_t
hunt_nvram_rw_start(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *pref_chunkp);

extern	__checkReturn		efx_rc_t
hunt_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	 __checkReturn		efx_rc_t
hunt_nvram_erase(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type);

extern	__checkReturn		efx_rc_t
hunt_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
hunt_nvram_rw_finish(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type);

extern	__checkReturn		efx_rc_t
hunt_nvram_partn_set_version(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in_ecount(4)		uint16_t version[4]);

extern	__checkReturn		efx_rc_t
hunt_nvram_set_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in_ecount(4)		uint16_t version[4]);

#endif	/* EFSYS_OPT_NVRAM */


/* PHY */

typedef struct hunt_link_state_s {
	uint32_t		hls_adv_cap_mask;
	uint32_t		hls_lp_cap_mask;
	unsigned int 		hls_fcntl;
	efx_link_mode_t		hls_link_mode;
#if EFSYS_OPT_LOOPBACK
	efx_loopback_type_t	hls_loopback;
#endif
	boolean_t		hls_mac_up;
} hunt_link_state_t;

extern			void
hunt_phy_link_ev(
	__in		efx_nic_t *enp,
	__in		efx_qword_t *eqp,
	__out		efx_link_mode_t *link_modep);

extern	__checkReturn	efx_rc_t
hunt_phy_get_link(
	__in		efx_nic_t *enp,
	__out		hunt_link_state_t *hlsp);

extern	__checkReturn	efx_rc_t
hunt_phy_power(
	__in		efx_nic_t *enp,
	__in		boolean_t on);

extern	__checkReturn	efx_rc_t
hunt_phy_reconfigure(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
hunt_phy_verify(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
hunt_phy_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip);

#if EFSYS_OPT_PHY_STATS

extern	__checkReturn			efx_rc_t
hunt_phy_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat);

#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_PROPS

#if EFSYS_OPT_NAMES

extern		const char *
hunt_phy_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id);

#endif	/* EFSYS_OPT_NAMES */

extern	__checkReturn	efx_rc_t
hunt_phy_prop_get(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t flags,
	__out		uint32_t *valp);

extern	__checkReturn	efx_rc_t
hunt_phy_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val);

#endif	/* EFSYS_OPT_PHY_PROPS */

#if EFSYS_OPT_BIST

extern	__checkReturn		efx_rc_t
hunt_bist_enable_offline(
	__in			efx_nic_t *enp);

extern	__checkReturn		efx_rc_t
hunt_bist_start(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type);

extern	__checkReturn		efx_rc_t
hunt_bist_poll(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type,
	__out			efx_bist_result_t *resultp,
	__out_opt __drv_when(count > 0, __notnull)
	uint32_t 	*value_maskp,
	__out_ecount_opt(count)	__drv_when(count > 0, __notnull)
	unsigned long	*valuesp,
	__in			size_t count);

extern				void
hunt_bist_stop(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type);

#endif	/* EFSYS_OPT_BIST */


/* SRAM */

#if EFSYS_OPT_DIAG

extern	__checkReturn	efx_rc_t
hunt_sram_test(
	__in		efx_nic_t *enp,
	__in		efx_sram_pattern_fn_t func);

#endif	/* EFSYS_OPT_DIAG */


/* TX */

extern	__checkReturn	efx_rc_t
hunt_tx_init(
	__in		efx_nic_t *enp);

extern			void
hunt_tx_fini(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
hunt_tx_qcreate(
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
hunt_tx_qdestroy(
	__in		efx_txq_t *etp);

extern	__checkReturn	efx_rc_t
hunt_tx_qpost(
	__in		efx_txq_t *etp,
	__in_ecount(n)	efx_buffer_t *eb,
	__in		unsigned int n,
	__in		unsigned int completed,
	__inout		unsigned int *addedp);

extern			void
hunt_tx_qpush(
	__in		efx_txq_t *etp,
	__in		unsigned int added,
	__in		unsigned int pushed);

extern	__checkReturn	efx_rc_t
hunt_tx_qpace(
	__in		efx_txq_t *etp,
	__in		unsigned int ns);

extern	__checkReturn	efx_rc_t
hunt_tx_qflush(
	__in		efx_txq_t *etp);

extern			void
hunt_tx_qenable(
	__in		efx_txq_t *etp);

extern	__checkReturn	efx_rc_t
hunt_tx_qpio_enable(
	__in		efx_txq_t *etp);

extern			void
hunt_tx_qpio_disable(
	__in		efx_txq_t *etp);

extern	__checkReturn	efx_rc_t
hunt_tx_qpio_write(
	__in			efx_txq_t *etp,
	__in_ecount(buf_length)	uint8_t *buffer,
	__in			size_t buf_length,
	__in                    size_t pio_buf_offset);

extern	__checkReturn	efx_rc_t
hunt_tx_qpio_post(
	__in			efx_txq_t *etp,
	__in			size_t pkt_length,
	__in			unsigned int completed,
	__inout			unsigned int *addedp);

extern	__checkReturn	efx_rc_t
hunt_tx_qdesc_post(
	__in		efx_txq_t *etp,
	__in_ecount(n)	efx_desc_t *ed,
	__in		unsigned int n,
	__in		unsigned int completed,
	__inout		unsigned int *addedp);

extern	void
hunt_tx_qdesc_dma_create(
	__in	efx_txq_t *etp,
	__in	efsys_dma_addr_t addr,
	__in	size_t size,
	__in	boolean_t eop,
	__out	efx_desc_t *edp);

extern	void
hunt_tx_qdesc_tso_create(
	__in	efx_txq_t *etp,
	__in	uint16_t ipv4_id,
	__in	uint32_t tcp_seq,
	__in	uint8_t  tcp_flags,
	__out	efx_desc_t *edp);

extern	void
hunt_tx_qdesc_vlantci_create(
	__in	efx_txq_t *etp,
	__in	uint16_t vlan_tci,
	__out	efx_desc_t *edp);


#if EFSYS_OPT_QSTATS

extern			void
hunt_tx_qstats_update(
	__in				efx_txq_t *etp,
	__inout_ecount(TX_NQSTATS)	efsys_stat_t *stat);

#endif /* EFSYS_OPT_QSTATS */

/* PIO */

/* Missing register definitions */
#ifndef	ER_DZ_TX_PIOBUF_OFST
#define	ER_DZ_TX_PIOBUF_OFST 0x00001000
#endif
#ifndef	ER_DZ_TX_PIOBUF_STEP
#define	ER_DZ_TX_PIOBUF_STEP 8192
#endif
#ifndef	ER_DZ_TX_PIOBUF_ROWS
#define	ER_DZ_TX_PIOBUF_ROWS 2048
#endif

#ifndef	ER_DZ_TX_PIOBUF_SIZE
#define	ER_DZ_TX_PIOBUF_SIZE 2048
#endif

#define	HUNT_PIOBUF_NBUFS	(16)
#define	HUNT_PIOBUF_SIZE	(ER_DZ_TX_PIOBUF_SIZE)

#define	HUNT_MIN_PIO_ALLOC_SIZE	(HUNT_PIOBUF_SIZE / 32)

#define HUNT_LEGACY_PF_PRIVILEGE_MASK					\
	(MC_CMD_PRIVILEGE_MASK_IN_GRP_ADMIN			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_LINK			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_ONLOAD			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_PTP			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_INSECURE_FILTERS		|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_MAC_SPOOFING		|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_UNICAST			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_MULTICAST			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_BROADCAST			|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_ALL_MULTICAST		|	\
	MC_CMD_PRIVILEGE_MASK_IN_GRP_PROMISCUOUS)

#define HUNT_LEGACY_VF_PRIVILEGE_MASK	0

typedef uint32_t	efx_piobuf_handle_t;

#define	EFX_PIOBUF_HANDLE_INVALID	((efx_piobuf_handle_t) -1)

extern	__checkReturn	efx_rc_t
hunt_nic_pio_alloc(
	__inout		efx_nic_t *enp,
	__out		uint32_t *bufnump,
	__out		efx_piobuf_handle_t *handlep,
	__out		uint32_t *blknump,
	__out		uint32_t *offsetp,
	__out		size_t *sizep);

extern	__checkReturn	efx_rc_t
hunt_nic_pio_free(
	__inout		efx_nic_t *enp,
	__in		uint32_t bufnum,
	__in		uint32_t blknum);

extern	__checkReturn	efx_rc_t
hunt_nic_pio_link(
	__inout		efx_nic_t *enp,
	__in		uint32_t vi_index,
	__in		efx_piobuf_handle_t handle);

extern	__checkReturn	efx_rc_t
hunt_nic_pio_unlink(
	__inout		efx_nic_t *enp,
	__in		uint32_t vi_index);


/* VPD */

#if EFSYS_OPT_VPD

extern	__checkReturn		efx_rc_t
hunt_vpd_init(
	__in			efx_nic_t *enp);

extern	__checkReturn		efx_rc_t
hunt_vpd_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep);

extern	__checkReturn		efx_rc_t
hunt_vpd_read(
	__in			efx_nic_t *enp,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
hunt_vpd_verify(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
hunt_vpd_reinit(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		efx_rc_t
hunt_vpd_get(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__inout			efx_vpd_value_t *evvp);

extern	__checkReturn		efx_rc_t
hunt_vpd_set(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			efx_vpd_value_t *evvp);

extern	__checkReturn		efx_rc_t
hunt_vpd_next(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__out			efx_vpd_value_t *evvp,
	__inout			unsigned int *contp);

extern __checkReturn		efx_rc_t
hunt_vpd_write(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
hunt_vpd_fini(
	__in			efx_nic_t *enp);

#endif	/* EFSYS_OPT_VPD */


/* RX */

extern	__checkReturn	efx_rc_t
hunt_rx_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_RX_HDR_SPLIT
extern	__checkReturn	efx_rc_t
hunt_rx_hdr_split_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int hdr_buf_size,
	__in		unsigned int pld_buf_size);
#endif	/* EFSYS_OPT_RX_HDR_SPLIT */

#if EFSYS_OPT_RX_SCATTER
extern	__checkReturn	efx_rc_t
hunt_rx_scatter_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int buf_size);
#endif	/* EFSYS_OPT_RX_SCATTER */


#if EFSYS_OPT_RX_SCALE

extern	__checkReturn	efx_rc_t
hunt_rx_scale_mode_set(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t alg,
	__in		efx_rx_hash_type_t type,
	__in		boolean_t insert);

extern	__checkReturn	efx_rc_t
hunt_rx_scale_key_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n);

extern	__checkReturn	efx_rc_t
hunt_rx_scale_tbl_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	unsigned int *table,
	__in		size_t n);

#endif /* EFSYS_OPT_RX_SCALE */

extern			void
hunt_rx_qpost(
	__in		efx_rxq_t *erp,
	__in_ecount(n)	efsys_dma_addr_t *addrp,
	__in		size_t size,
	__in		unsigned int n,
	__in		unsigned int completed,
	__in		unsigned int added);

extern			void
hunt_rx_qpush(
	__in		efx_rxq_t *erp,
	__in		unsigned int added,
	__inout		unsigned int *pushedp);

extern	__checkReturn	efx_rc_t
hunt_rx_qflush(
	__in		efx_rxq_t *erp);

extern		void
hunt_rx_qenable(
	__in		efx_rxq_t *erp);

extern	__checkReturn	efx_rc_t
hunt_rx_qcreate(
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
hunt_rx_qdestroy(
	__in		efx_rxq_t *erp);

extern			void
hunt_rx_fini(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_FILTER

typedef struct hunt_filter_handle_s {
	uint32_t	hfh_lo;
	uint32_t	hfh_hi;
} hunt_filter_handle_t;

typedef struct hunt_filter_entry_s {
	uintptr_t hfe_spec; /* pointer to filter spec plus busy bit */
	hunt_filter_handle_t hfe_handle;
} hunt_filter_entry_t;

/*
 * BUSY flag indicates that an update is in progress.
 * AUTO_OLD flag is used to mark and sweep MAC packet filters.
 */
#define	EFX_HUNT_FILTER_FLAG_BUSY	1U
#define	EFX_HUNT_FILTER_FLAG_AUTO_OLD	2U
#define	EFX_HUNT_FILTER_FLAGS		3U

#define	EFX_HUNT_FILTER_TBL_ROWS 8192

/* Allow for the broadcast address to be added to the multicast list */
#define	EFX_HUNT_FILTER_MULTICAST_FILTERS_MAX	(EFX_MAC_MULTICAST_LIST_MAX + 1)

typedef struct hunt_filter_table_s {
	hunt_filter_entry_t	hft_entry[EFX_HUNT_FILTER_TBL_ROWS];
	efx_rxq_t *		hft_default_rxq;
	boolean_t 		hft_using_rss;
	uint32_t 		hft_unicst_filter_index;
	boolean_t 		hft_unicst_filter_set;
	uint32_t 		hft_mulcst_filter_indexes[
	    EFX_HUNT_FILTER_MULTICAST_FILTERS_MAX];
	uint32_t 		hft_mulcst_filter_count;
} hunt_filter_table_t;

	__checkReturn	efx_rc_t
hunt_filter_init(
	__in		efx_nic_t *enp);

			void
hunt_filter_fini(
	__in		efx_nic_t *enp);

	__checkReturn	efx_rc_t
hunt_filter_restore(
	__in		efx_nic_t *enp);

	__checkReturn	efx_rc_t
hunt_filter_add(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec,
	__in		boolean_t may_replace);

	__checkReturn	efx_rc_t
hunt_filter_delete(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec);

extern	__checkReturn	efx_rc_t
hunt_filter_supported_filters(
	__in		efx_nic_t *enp,
	__out		uint32_t *list,
	__out		size_t *length);

extern	__checkReturn	efx_rc_t
hunt_filter_reconfigure(
	__in				efx_nic_t *enp,
	__in_ecount(6)			uint8_t const *mac_addr,
	__in				boolean_t all_unicst,
	__in				boolean_t mulcst,
	__in				boolean_t all_mulcst,
	__in				boolean_t brdcst,
	__in_ecount(6*count)		uint8_t const *addrs,
	__in				int count);

extern		void
hunt_filter_get_default_rxq(
	__in		efx_nic_t *enp,
	__out		efx_rxq_t **erpp,
	__out		boolean_t *using_rss);

extern		void
hunt_filter_default_rxq_set(
	__in		efx_nic_t *enp,
	__in		efx_rxq_t *erp,
	__in		boolean_t using_rss);

extern		void
hunt_filter_default_rxq_clear(
	__in		efx_nic_t *enp);


#endif /* EFSYS_OPT_FILTER */

extern	__checkReturn			efx_rc_t
hunt_pktfilter_set(
	__in				efx_nic_t *enp,
	__in				boolean_t unicst,
	__in				boolean_t brdcst);

#if EFSYS_OPT_MCAST_FILTER_LIST

extern	__checkReturn			efx_rc_t
hunt_pktfilter_mcast_set(
	__in				efx_nic_t *enp,
	__in				uint8_t const *addrs,
	__in				int count);

#endif /* EFSYS_OPT_MCAST_FILTER_LIST */

extern	__checkReturn			efx_rc_t
hunt_pktfilter_mcast_all(
	__in				efx_nic_t *enp);

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

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_HUNT_IMPL_H */
