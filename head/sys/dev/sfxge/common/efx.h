/*-
 * Copyright 2006-2009 Solarflare Communications Inc.  All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef	_SYS_EFX_H
#define	_SYS_EFX_H

#include "efsys.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define	EFX_STATIC_ASSERT(_cond) ((void)sizeof(char[(_cond) ? 1 : -1]))

#define	EFX_ARRAY_SIZE(_array) (sizeof(_array) / sizeof((_array)[0]))

#ifndef EFSYS_MEM_IS_NULL
#define	EFSYS_MEM_IS_NULL(_esmp) ((_esmp)->esm_base == NULL)
#endif

typedef enum efx_family_e {
	EFX_FAMILY_INVALID,
	EFX_FAMILY_FALCON,
	EFX_FAMILY_SIENA,
	EFX_FAMILY_NTYPES
} efx_family_t;

extern	__checkReturn	int
efx_family(
	__in		uint16_t venid,
	__in		uint16_t devid,
	__out		efx_family_t *efp);

extern	__checkReturn	int
efx_infer_family(
	__in		efsys_bar_t *esbp,
	__out		efx_family_t *efp);

#define	EFX_PCI_VENID_SFC		0x1924
#define	EFX_PCI_DEVID_FALCON		0x0710
#define	EFX_PCI_DEVID_BETHPAGE		0x0803
#define	EFX_PCI_DEVID_SIENA		0x0813
#define	EFX_PCI_DEVID_SIENA_F1_UNINIT	0x0810

#define	EFX_MEM_BAR	2

/* Error codes */

enum {
	EFX_ERR_INVALID,
	EFX_ERR_SRAM_OOB,
	EFX_ERR_BUFID_DC_OOB,
	EFX_ERR_MEM_PERR,
	EFX_ERR_RBUF_OWN,
	EFX_ERR_TBUF_OWN,
	EFX_ERR_RDESQ_OWN,
	EFX_ERR_TDESQ_OWN,
	EFX_ERR_EVQ_OWN,
	EFX_ERR_EVFF_OFLO,
	EFX_ERR_ILL_ADDR,
	EFX_ERR_SRAM_PERR,
	EFX_ERR_NCODES
};

/* NIC */

typedef struct efx_nic_s	efx_nic_t;

extern	__checkReturn	int
efx_nic_create(
	__in		efx_family_t family,
	__in		efsys_identifier_t *esip,
	__in		efsys_bar_t *esbp,
	__in		efsys_lock_t *eslp,
	__deref_out	efx_nic_t **enpp);

extern	__checkReturn	int
efx_nic_probe(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_PCIE_TUNE

extern	__checkReturn	int
efx_nic_pcie_tune(
	__in		efx_nic_t *enp,
	unsigned int	nlanes);

extern	__checkReturn	int
efx_nic_pcie_extended_sync(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_PCIE_TUNE */

extern 	__checkReturn	int
efx_nic_init(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
efx_nic_reset(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

extern	__checkReturn	int
efx_nic_register_test(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern		void
efx_nic_fini(
	__in		efx_nic_t *enp);

extern		void
efx_nic_unprobe(
	__in		efx_nic_t *enp);

extern 		void
efx_nic_destroy(
	__in	efx_nic_t *enp);

#if EFSYS_OPT_MCDI

typedef struct efx_mcdi_req_s efx_mcdi_req_t;

typedef enum efx_mcdi_exception_e {
	EFX_MCDI_EXCEPTION_MC_REBOOT,
	EFX_MCDI_EXCEPTION_MC_BADASSERT,
} efx_mcdi_exception_t;

typedef struct efx_mcdi_transport_s {
	void		*emt_context;
	void		(*emt_execute)(void *, efx_mcdi_req_t *);
	void		(*emt_ev_cpl)(void *);
	void		(*emt_exception)(void *, efx_mcdi_exception_t);
} efx_mcdi_transport_t;

extern	__checkReturn	int
efx_mcdi_init(
	__in		efx_nic_t *enp,
	__in		const efx_mcdi_transport_t *mtp);

extern	__checkReturn	int
efx_mcdi_reboot(
	__in		efx_nic_t *enp);

extern			void
efx_mcdi_request_start(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__in		boolean_t ev_cpl);

extern	__checkReturn	boolean_t
efx_mcdi_request_poll(
	__in		efx_nic_t *enp);

extern	__checkReturn	boolean_t
efx_mcdi_request_abort(
	__in		efx_nic_t *enp);

extern			void
efx_mcdi_fini(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_MCDI */

/* INTR */

#define	EFX_NINTR_FALCON 64
#define	EFX_NINTR_SIENA 1024

typedef enum efx_intr_type_e {
	EFX_INTR_INVALID = 0,
	EFX_INTR_LINE,
	EFX_INTR_MESSAGE,
	EFX_INTR_NTYPES
} efx_intr_type_t;

#define	EFX_INTR_SIZE	(sizeof (efx_oword_t))

extern	__checkReturn	int
efx_intr_init(
	__in		efx_nic_t *enp,
	__in		efx_intr_type_t type,
	__in		efsys_mem_t *esmp);

extern 			void
efx_intr_enable(
	__in		efx_nic_t *enp);

extern 			void
efx_intr_disable(
	__in		efx_nic_t *enp);

extern 			void
efx_intr_disable_unlocked(
	__in		efx_nic_t *enp);

#define	EFX_INTR_NEVQS	32

extern __checkReturn	int
efx_intr_trigger(
	__in		efx_nic_t *enp,
	__in		unsigned int level);

extern			void
efx_intr_status_line(
	__in		efx_nic_t *enp,
	__out		boolean_t *fatalp,
	__out		uint32_t *maskp);

extern			void
efx_intr_status_message(
	__in		efx_nic_t *enp,
	__in		unsigned int message,
	__out		boolean_t *fatalp);

extern			void
efx_intr_fatal(
	__in		efx_nic_t *enp);

extern			void
efx_intr_fini(
	__in		efx_nic_t *enp);

/* MAC */

#if EFSYS_OPT_MAC_STATS

/* START MKCONFIG GENERATED EfxHeaderMacBlock bb8d39428b6fdcf5 */
typedef enum efx_mac_stat_e {
	EFX_MAC_RX_OCTETS,
	EFX_MAC_RX_PKTS,
	EFX_MAC_RX_UNICST_PKTS,
	EFX_MAC_RX_MULTICST_PKTS,
	EFX_MAC_RX_BRDCST_PKTS,
	EFX_MAC_RX_PAUSE_PKTS,
	EFX_MAC_RX_LE_64_PKTS,
	EFX_MAC_RX_65_TO_127_PKTS,
	EFX_MAC_RX_128_TO_255_PKTS,
	EFX_MAC_RX_256_TO_511_PKTS,
	EFX_MAC_RX_512_TO_1023_PKTS,
	EFX_MAC_RX_1024_TO_15XX_PKTS,
	EFX_MAC_RX_GE_15XX_PKTS,
	EFX_MAC_RX_ERRORS,
	EFX_MAC_RX_FCS_ERRORS,
	EFX_MAC_RX_DROP_EVENTS,
	EFX_MAC_RX_FALSE_CARRIER_ERRORS,
	EFX_MAC_RX_SYMBOL_ERRORS,
	EFX_MAC_RX_ALIGN_ERRORS,
	EFX_MAC_RX_INTERNAL_ERRORS,
	EFX_MAC_RX_JABBER_PKTS,
	EFX_MAC_RX_LANE0_CHAR_ERR,
	EFX_MAC_RX_LANE1_CHAR_ERR,
	EFX_MAC_RX_LANE2_CHAR_ERR,
	EFX_MAC_RX_LANE3_CHAR_ERR,
	EFX_MAC_RX_LANE0_DISP_ERR,
	EFX_MAC_RX_LANE1_DISP_ERR,
	EFX_MAC_RX_LANE2_DISP_ERR,
	EFX_MAC_RX_LANE3_DISP_ERR,
	EFX_MAC_RX_MATCH_FAULT,
	EFX_MAC_RX_NODESC_DROP_CNT,
	EFX_MAC_TX_OCTETS,
	EFX_MAC_TX_PKTS,
	EFX_MAC_TX_UNICST_PKTS,
	EFX_MAC_TX_MULTICST_PKTS,
	EFX_MAC_TX_BRDCST_PKTS,
	EFX_MAC_TX_PAUSE_PKTS,
	EFX_MAC_TX_LE_64_PKTS,
	EFX_MAC_TX_65_TO_127_PKTS,
	EFX_MAC_TX_128_TO_255_PKTS,
	EFX_MAC_TX_256_TO_511_PKTS,
	EFX_MAC_TX_512_TO_1023_PKTS,
	EFX_MAC_TX_1024_TO_15XX_PKTS,
	EFX_MAC_TX_GE_15XX_PKTS,
	EFX_MAC_TX_ERRORS,
	EFX_MAC_TX_SGL_COL_PKTS,
	EFX_MAC_TX_MULT_COL_PKTS,
	EFX_MAC_TX_EX_COL_PKTS,
	EFX_MAC_TX_LATE_COL_PKTS,
	EFX_MAC_TX_DEF_PKTS,
	EFX_MAC_TX_EX_DEF_PKTS,
	EFX_MAC_NSTATS
} efx_mac_stat_t;

/* END MKCONFIG GENERATED EfxHeaderMacBlock */

#endif	/* EFSYS_OPT_MAC_STATS */

typedef enum efx_link_mode_e {
	EFX_LINK_UNKNOWN = 0,
	EFX_LINK_DOWN,
	EFX_LINK_10HDX,
	EFX_LINK_10FDX,
	EFX_LINK_100HDX,
	EFX_LINK_100FDX,
	EFX_LINK_1000HDX,
	EFX_LINK_1000FDX,
	EFX_LINK_10000FDX,
	EFX_LINK_NMODES
} efx_link_mode_t;

#define	EFX_MAC_SDU_MAX	9202

#define	EFX_MAC_PDU(_sdu) 				\
	P2ROUNDUP(((_sdu)				\
		    + /* EtherII */ 14			\
		    + /* VLAN */ 4			\
		    + /* CRC */ 4			\
		    + /* bug16011 */ 16),		\
		    (1 << 3))

#define	EFX_MAC_PDU_MIN	60
#define	EFX_MAC_PDU_MAX	EFX_MAC_PDU(EFX_MAC_SDU_MAX)

extern	__checkReturn	int
efx_mac_pdu_set(
	__in		efx_nic_t *enp,
	__in		size_t pdu);

extern	__checkReturn	int
efx_mac_addr_set(
	__in		efx_nic_t *enp,
	__in		uint8_t *addr);

extern	__checkReturn	int
efx_mac_filter_set(
	__in		efx_nic_t *enp,
	__in		boolean_t unicst,
	__in		boolean_t brdcst);

extern	__checkReturn	int
efx_mac_drain(
	__in		efx_nic_t *enp,
	__in		boolean_t enabled);

extern	__checkReturn	int
efx_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp);

#define	EFX_FCNTL_RESPOND	0x00000001
#define	EFX_FCNTL_GENERATE	0x00000002

extern	__checkReturn	int
efx_mac_fcntl_set(
	__in		efx_nic_t *enp,
	__in		unsigned int fcntl,
	__in		boolean_t autoneg);

extern			void
efx_mac_fcntl_get(
	__in		efx_nic_t *enp,
	__out		unsigned int *fcntl_wantedp,
	__out		unsigned int *fcntl_linkp);

#define	EFX_MAC_HASH_BITS	(1 << 8)

extern	__checkReturn			int
efx_mac_hash_set(
	__in				efx_nic_t *enp,
	__in_ecount(EFX_MAC_HASH_BITS)	unsigned int const *bucket);

#if EFSYS_OPT_MAC_STATS

#if EFSYS_OPT_NAMES

extern	__checkReturn			const char __cs *
efx_mac_stat_name(
	__in				efx_nic_t *enp,
	__in				unsigned int id);

#endif	/* EFSYS_OPT_NAMES */

#define	EFX_MAC_STATS_SIZE 0x400

/*
 * Upload mac statistics supported by the hardware into the given buffer.
 *
 * The reference buffer must be at least %EFX_MAC_STATS_SIZE bytes,
 * and page aligned.
 *
 * The hardware will only DMA statistics that it understands (of course).
 * Drivers should not make any assumptions about which statistics are
 * supported, especially when the statistics are generated by firmware.
 *
 * Thus, drivers should zero this buffer before use, so that not-understood
 * statistics read back as zero.
 */
extern	__checkReturn			int
efx_mac_stats_upload(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp);

extern	__checkReturn			int
efx_mac_stats_periodic(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__in				uint16_t period_ms,
	__in				boolean_t events);

extern	__checkReturn			int
efx_mac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MAC_NSTATS)	efsys_stat_t *stat,
	__out_opt			uint32_t *generationp);

#endif	/* EFSYS_OPT_MAC_STATS */

/* MON */

typedef enum efx_mon_type_e {
	EFX_MON_INVALID = 0,
	EFX_MON_NULL,
	EFX_MON_LM87,
	EFX_MON_MAX6647,
	EFX_MON_SFC90X0,
	EFX_MON_NTYPES
} efx_mon_type_t;

#if EFSYS_OPT_NAMES

extern		const char __cs *
efx_mon_name(
	__in	efx_nic_t *enp);

#endif	/* EFSYS_OPT_NAMES */

extern	__checkReturn	int
efx_mon_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_MON_STATS

#define	EFX_MON_STATS_SIZE 0x100

/* START MKCONFIG GENERATED MonitorHeaderStatsBlock 16a14e61aa4f8d80 */
typedef enum efx_mon_stat_e {
	EFX_MON_STAT_2_5V,
	EFX_MON_STAT_VCCP1,
	EFX_MON_STAT_VCC,
	EFX_MON_STAT_5V,
	EFX_MON_STAT_12V,
	EFX_MON_STAT_VCCP2,
	EFX_MON_STAT_EXT_TEMP,
	EFX_MON_STAT_INT_TEMP,
	EFX_MON_STAT_AIN1,
	EFX_MON_STAT_AIN2,
	EFX_MON_STAT_INT_COOLING,
	EFX_MON_STAT_EXT_COOLING,
	EFX_MON_STAT_1V,
	EFX_MON_STAT_1_2V,
	EFX_MON_STAT_1_8V,
	EFX_MON_STAT_3_3V,
	EFX_MON_NSTATS
} efx_mon_stat_t;

/* END MKCONFIG GENERATED MonitorHeaderStatsBlock */

typedef enum efx_mon_stat_state_e {
	EFX_MON_STAT_STATE_OK = 0,
	EFX_MON_STAT_STATE_WARNING = 1,
	EFX_MON_STAT_STATE_FATAL = 2,
	EFX_MON_STAT_STATE_BROKEN = 3,
} efx_mon_stat_state_t;

typedef struct efx_mon_stat_value_t {
	uint16_t	emsv_value;
	uint16_t	emsv_state;
} efx_mon_stat_value_t;

#if EFSYS_OPT_NAMES

extern					const char __cs *
efx_mon_stat_name(
	__in				efx_nic_t *enp,
	__in				efx_mon_stat_t id);

#endif	/* EFSYS_OPT_NAMES */

extern	__checkReturn			int
efx_mon_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values);

#endif	/* EFSYS_OPT_MON_STATS */

extern		void
efx_mon_fini(
	__in	efx_nic_t *enp);

/* PHY */

#define	PMA_PMD_MMD	1
#define	PCS_MMD		3
#define	PHY_XS_MMD	4
#define	DTE_XS_MMD	5
#define	AN_MMD		7
#define	CL22EXT_MMD	29

#define	MAXMMD		((1 << 5) - 1)

/* PHY types */
#define	EFX_PHY_NULL		0x0
#define	EFX_PHY_TXC43128	0x1
#define	EFX_PHY_SFX7101		0x3
#define	EFX_PHY_QT2022C2	0x4
#define	EFX_PHY_SFT9001A	0x8
#define	EFX_PHY_QT2025C		0x9
#define	EFX_PHY_SFT9001B	0xa
#define	EFX_PHY_QLX111V		0xc

extern	__checkReturn	int
efx_phy_verify(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_PHY_LED_CONTROL

typedef enum efx_phy_led_mode_e {
	EFX_PHY_LED_DEFAULT = 0,
	EFX_PHY_LED_OFF,
	EFX_PHY_LED_ON,
	EFX_PHY_LED_FLASH,
	EFX_PHY_LED_NMODES
} efx_phy_led_mode_t;

extern	__checkReturn	int
efx_phy_led_set(
	__in	efx_nic_t *enp,
	__in	efx_phy_led_mode_t mode);

#endif	/* EFSYS_OPT_PHY_LED_CONTROL */

extern	__checkReturn	int
efx_port_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_LOOPBACK

typedef enum efx_loopback_type_e {
	EFX_LOOPBACK_OFF = 0,
	EFX_LOOPBACK_DATA = 1,
	EFX_LOOPBACK_GMAC = 2,
	EFX_LOOPBACK_XGMII = 3,
	EFX_LOOPBACK_XGXS = 4,
	EFX_LOOPBACK_XAUI = 5,
	EFX_LOOPBACK_GMII = 6,
	EFX_LOOPBACK_SGMII = 7,
	EFX_LOOPBACK_XGBR = 8,
	EFX_LOOPBACK_XFI = 9,
	EFX_LOOPBACK_XAUI_FAR = 10,
	EFX_LOOPBACK_GMII_FAR = 11,
	EFX_LOOPBACK_SGMII_FAR = 12,
	EFX_LOOPBACK_XFI_FAR = 13,
	EFX_LOOPBACK_GPHY = 14,
	EFX_LOOPBACK_PHY_XS = 15,
	EFX_LOOPBACK_PCS = 16,
	EFX_LOOPBACK_PMA_PMD = 17,
	EFX_LOOPBACK_NTYPES
} efx_loopback_type_t;

#define	EFX_LOOPBACK_MAC_MASK			\
	((1 << EFX_LOOPBACK_DATA) |		\
	    (1 << EFX_LOOPBACK_GMAC) | 		\
	    (1 << EFX_LOOPBACK_XGMII) | 	\
	    (1 << EFX_LOOPBACK_XGXS) | 		\
	    (1 << EFX_LOOPBACK_XAUI) |		\
	    (1 << EFX_LOOPBACK_GMII) |		\
	    (1 << EFX_LOOPBACK_SGMII) |		\
	    (1 << EFX_LOOPBACK_XGBR) |		\
	    (1 << EFX_LOOPBACK_XFI) |		\
	    (1 << EFX_LOOPBACK_XAUI_FAR) |	\
	    (1 << EFX_LOOPBACK_GMII_FAR) |	\
	    (1 << EFX_LOOPBACK_SGMII_FAR) |	\
	    (1 << EFX_LOOPBACK_XFI_FAR))

#define EFX_LOOPBACK_MASK			\
	((1 << EFX_LOOPBACK_NTYPES) - 1)

extern	__checkReturn	int
efx_port_loopback_set(
	__in	efx_nic_t *enp,
	__in	efx_link_mode_t link_mode,
	__in	efx_loopback_type_t type);

#if EFSYS_OPT_NAMES

extern	__checkReturn	const char __cs *
efx_loopback_type_name(
	__in		efx_nic_t *enp,
	__in		efx_loopback_type_t type);

#endif	/* EFSYS_OPT_NAMES */

#endif	/* EFSYS_OPT_LOOPBACK */

extern	__checkReturn	int
efx_port_poll(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t	*link_modep);

extern 		void
efx_port_fini(
	__in	efx_nic_t *enp);

typedef enum efx_phy_cap_type_e {
	EFX_PHY_CAP_INVALID = 0,
	EFX_PHY_CAP_10HDX,
	EFX_PHY_CAP_10FDX,
	EFX_PHY_CAP_100HDX,
	EFX_PHY_CAP_100FDX,
	EFX_PHY_CAP_1000HDX,
	EFX_PHY_CAP_1000FDX,
	EFX_PHY_CAP_10000FDX,
	EFX_PHY_CAP_PAUSE,
	EFX_PHY_CAP_ASYM,
	EFX_PHY_CAP_AN,
	EFX_PHY_CAP_NTYPES
} efx_phy_cap_type_t;


#define	EFX_PHY_CAP_CURRENT	0x00000000
#define	EFX_PHY_CAP_DEFAULT	0x00000001
#define	EFX_PHY_CAP_PERM	0x00000002

extern		void
efx_phy_adv_cap_get(
	__in		efx_nic_t *enp,
	__in            uint32_t flag,
	__out		uint32_t *maskp);

extern	__checkReturn	int
efx_phy_adv_cap_set(
	__in		efx_nic_t *enp,
	__in		uint32_t mask);

extern			void
efx_phy_lp_cap_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *maskp);

extern	__checkReturn	int
efx_phy_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip);

typedef enum efx_phy_media_type_e {
	EFX_PHY_MEDIA_INVALID = 0,
	EFX_PHY_MEDIA_XAUI,
	EFX_PHY_MEDIA_CX4,
	EFX_PHY_MEDIA_KX4,
	EFX_PHY_MEDIA_XFP,
	EFX_PHY_MEDIA_SFP_PLUS,
	EFX_PHY_MEDIA_BASE_T,
	EFX_PHY_MEDIA_NTYPES
} efx_phy_media_type_t;

/* Get the type of medium currently used.  If the board has ports for
 * modules, a module is present, and we recognise the media type of
 * the module, then this will be the media type of the module.
 * Otherwise it will be the media type of the port.
 */
extern			void
efx_phy_media_type_get(
	__in		efx_nic_t *enp,
	__out		efx_phy_media_type_t *typep);

#if EFSYS_OPT_PHY_STATS

/* START MKCONFIG GENERATED PhyHeaderStatsBlock 30ed56ad501f8e36 */
typedef enum efx_phy_stat_e {
	EFX_PHY_STAT_OUI,
	EFX_PHY_STAT_PMA_PMD_LINK_UP,
	EFX_PHY_STAT_PMA_PMD_RX_FAULT,
	EFX_PHY_STAT_PMA_PMD_TX_FAULT,
	EFX_PHY_STAT_PMA_PMD_REV_A,
	EFX_PHY_STAT_PMA_PMD_REV_B,
	EFX_PHY_STAT_PMA_PMD_REV_C,
	EFX_PHY_STAT_PMA_PMD_REV_D,
	EFX_PHY_STAT_PCS_LINK_UP,
	EFX_PHY_STAT_PCS_RX_FAULT,
	EFX_PHY_STAT_PCS_TX_FAULT,
	EFX_PHY_STAT_PCS_BER,
	EFX_PHY_STAT_PCS_BLOCK_ERRORS,
	EFX_PHY_STAT_PHY_XS_LINK_UP,
	EFX_PHY_STAT_PHY_XS_RX_FAULT,
	EFX_PHY_STAT_PHY_XS_TX_FAULT,
	EFX_PHY_STAT_PHY_XS_ALIGN,
	EFX_PHY_STAT_PHY_XS_SYNC_A,
	EFX_PHY_STAT_PHY_XS_SYNC_B,
	EFX_PHY_STAT_PHY_XS_SYNC_C,
	EFX_PHY_STAT_PHY_XS_SYNC_D,
	EFX_PHY_STAT_AN_LINK_UP,
	EFX_PHY_STAT_AN_MASTER,
	EFX_PHY_STAT_AN_LOCAL_RX_OK,
	EFX_PHY_STAT_AN_REMOTE_RX_OK,
	EFX_PHY_STAT_CL22EXT_LINK_UP,
	EFX_PHY_STAT_SNR_A,
	EFX_PHY_STAT_SNR_B,
	EFX_PHY_STAT_SNR_C,
	EFX_PHY_STAT_SNR_D,
	EFX_PHY_STAT_PMA_PMD_SIGNAL_A,
	EFX_PHY_STAT_PMA_PMD_SIGNAL_B,
	EFX_PHY_STAT_PMA_PMD_SIGNAL_C,
	EFX_PHY_STAT_PMA_PMD_SIGNAL_D,
	EFX_PHY_STAT_AN_COMPLETE,
	EFX_PHY_STAT_PMA_PMD_REV_MAJOR,
	EFX_PHY_STAT_PMA_PMD_REV_MINOR,
	EFX_PHY_STAT_PMA_PMD_REV_MICRO,
	EFX_PHY_STAT_PCS_FW_VERSION_0,
	EFX_PHY_STAT_PCS_FW_VERSION_1,
	EFX_PHY_STAT_PCS_FW_VERSION_2,
	EFX_PHY_STAT_PCS_FW_VERSION_3,
	EFX_PHY_STAT_PCS_FW_BUILD_YY,
	EFX_PHY_STAT_PCS_FW_BUILD_MM,
	EFX_PHY_STAT_PCS_FW_BUILD_DD,
	EFX_PHY_STAT_PCS_OP_MODE,
	EFX_PHY_NSTATS
} efx_phy_stat_t;

/* END MKCONFIG GENERATED PhyHeaderStatsBlock */

#if EFSYS_OPT_NAMES

extern					const char __cs *
efx_phy_stat_name(
	__in				efx_nic_t *enp,
	__in				efx_phy_stat_t stat);

#endif	/* EFSYS_OPT_NAMES */

#define	EFX_PHY_STATS_SIZE 0x100

extern	__checkReturn			int
efx_phy_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_PHY_NSTATS)	uint32_t *stat);

#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_PROPS

#if EFSYS_OPT_NAMES

extern		const char __cs *
efx_phy_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id);

#endif	/* EFSYS_OPT_NAMES */

#define	EFX_PHY_PROP_DEFAULT	0x00000001

extern	__checkReturn	int
efx_phy_prop_get(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t flags,
	__out		uint32_t *valp);

extern	__checkReturn	int
efx_phy_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val);

#endif	/* EFSYS_OPT_PHY_PROPS */

#if EFSYS_OPT_PHY_BIST

typedef enum efx_phy_bist_type_e {
	EFX_PHY_BIST_TYPE_UNKNOWN,
	EFX_PHY_BIST_TYPE_NORMAL,
	EFX_PHY_BIST_TYPE_CABLE_SHORT,
	EFX_PHY_BIST_TYPE_CABLE_LONG,
	EFX_PHY_BIST_TYPE_NTYPES,
} efx_phy_bist_type_t;

typedef enum efx_phy_bist_result_e {
	EFX_PHY_BIST_RESULT_UNKNOWN,
	EFX_PHY_BIST_RESULT_RUNNING,
	EFX_PHY_BIST_RESULT_PASSED,
	EFX_PHY_BIST_RESULT_FAILED,
} efx_phy_bist_result_t;

typedef enum efx_phy_cable_status_e {
	EFX_PHY_CABLE_STATUS_OK,
	EFX_PHY_CABLE_STATUS_INVALID,
	EFX_PHY_CABLE_STATUS_OPEN,
	EFX_PHY_CABLE_STATUS_INTRAPAIRSHORT,
	EFX_PHY_CABLE_STATUS_INTERPAIRSHORT,
	EFX_PHY_CABLE_STATUS_BUSY,
} efx_phy_cable_status_t;

typedef enum efx_phy_bist_value_e {
	EFX_PHY_BIST_CABLE_LENGTH_A,
	EFX_PHY_BIST_CABLE_LENGTH_B,
	EFX_PHY_BIST_CABLE_LENGTH_C,
	EFX_PHY_BIST_CABLE_LENGTH_D,
	EFX_PHY_BIST_CABLE_STATUS_A,
	EFX_PHY_BIST_CABLE_STATUS_B,
	EFX_PHY_BIST_CABLE_STATUS_C,
	EFX_PHY_BIST_CABLE_STATUS_D,
	EFX_PHY_BIST_FAULT_CODE,
	EFX_PHY_BIST_NVALUES,
} efx_phy_bist_value_t;

extern	__checkReturn		int
efx_phy_bist_start(
	__in			efx_nic_t *enp,
	__in			efx_phy_bist_type_t type);

extern	__checkReturn		int
efx_phy_bist_poll(
	__in			efx_nic_t *enp,
	__in			efx_phy_bist_type_t type,
	__out			efx_phy_bist_result_t *resultp,
	__out_opt		uint32_t *value_maskp,
	__out_ecount_opt(count)	unsigned long *valuesp,
	__in			size_t count);

extern				void
efx_phy_bist_stop(
	__in			efx_nic_t *enp,
	__in			efx_phy_bist_type_t type);

#endif	/* EFSYS_OPT_PHY_BIST */

#define	EFX_FEATURE_IPV6		0x00000001
#define	EFX_FEATURE_LFSR_HASH_INSERT	0x00000002
#define	EFX_FEATURE_LINK_EVENTS		0x00000004
#define	EFX_FEATURE_PERIODIC_MAC_STATS	0x00000008
#define	EFX_FEATURE_WOL			0x00000010
#define	EFX_FEATURE_MCDI		0x00000020
#define	EFX_FEATURE_LOOKAHEAD_SPLIT	0x00000040
#define	EFX_FEATURE_MAC_HEADER_FILTERS	0x00000080

typedef struct efx_nic_cfg_s {
	uint32_t		enc_board_type;
	uint32_t		enc_phy_type;
#if EFSYS_OPT_NAMES
	char			enc_phy_name[21];
#endif
	char			enc_phy_revision[21];
	efx_mon_type_t		enc_mon_type;
#if EFSYS_OPT_MON_STATS
	uint32_t		enc_mon_stat_mask;
#endif
	unsigned int		enc_features;
	uint8_t			enc_mac_addr[6];
	uint8_t			enc_port;
	uint32_t		enc_evq_limit;
	uint32_t		enc_txq_limit;
	uint32_t		enc_rxq_limit;
	uint32_t		enc_buftbl_limit;
	uint32_t		enc_evq_moderation_max;
#if EFSYS_OPT_LOOPBACK
	uint32_t		enc_loopback_types[EFX_LINK_NMODES];
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_PHY_FLAGS
	uint32_t		enc_phy_flags_mask;
#endif	/* EFSYS_OPT_PHY_FLAGS */
#if EFSYS_OPT_PHY_LED_CONTROL
	uint32_t		enc_led_mask;
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */
#if EFSYS_OPT_PHY_STATS
	uint64_t		enc_phy_stat_mask;
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
	unsigned int		enc_phy_nprops;
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_SIENA
	uint8_t			enc_siena_channel;
#if EFSYS_OPT_PHY_STATS
	uint32_t		enc_siena_phy_stat_mask;
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_MON_STATS
	uint32_t		enc_siena_mon_stat_mask;
#endif	/* EFSYS_OPT_MON_STATS */
#endif	/* EFSYS_OPT_SIENA */
#if EFSYS_OPT_PHY_BIST
	uint32_t		enc_bist_mask;
#endif	/* EFSYS_OPT_PHY_BIST */
} efx_nic_cfg_t;

extern			const efx_nic_cfg_t *
efx_nic_cfg_get(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_VPD

typedef enum efx_vpd_tag_e {
	EFX_VPD_ID = 0x02,
	EFX_VPD_END = 0x0f,
	EFX_VPD_RO = 0x10,
	EFX_VPD_RW = 0x11,
} efx_vpd_tag_t;

typedef uint16_t efx_vpd_keyword_t;

typedef struct efx_vpd_value_s {
	efx_vpd_tag_t		evv_tag;
	efx_vpd_keyword_t	evv_keyword;
	uint8_t			evv_length;
	uint8_t			evv_value[0x100];
} efx_vpd_value_t;


#define	EFX_VPD_KEYWORD(x, y) ((x) | ((y) << 8))

extern	__checkReturn		int
efx_vpd_init(
	__in			efx_nic_t *enp);

extern	__checkReturn		int
efx_vpd_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep);

extern	__checkReturn		int
efx_vpd_read(
	__in			efx_nic_t *enp,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		int
efx_vpd_verify(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern  __checkReturn		int
efx_vpd_reinit(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		int
efx_vpd_get(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__inout			efx_vpd_value_t *evvp);

extern	__checkReturn		int
efx_vpd_set(
	__in			efx_nic_t *enp,
	__inout_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			efx_vpd_value_t *evvp);

extern	__checkReturn		int
efx_vpd_next(
	__in			efx_nic_t *enp,
	__inout_bcount(size)	caddr_t data,
	__in			size_t size,
	__out			efx_vpd_value_t *evvp,
	__inout			unsigned int *contp);

extern __checkReturn		int
efx_vpd_write(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
efx_vpd_fini(
	__in			efx_nic_t *enp);

#endif	/* EFSYS_OPT_VPD */

/* NVRAM */

#if EFSYS_OPT_NVRAM

typedef enum efx_nvram_type_e {
	EFX_NVRAM_INVALID = 0,
	EFX_NVRAM_BOOTROM,
	EFX_NVRAM_BOOTROM_CFG,
	EFX_NVRAM_MC_FIRMWARE,
	EFX_NVRAM_MC_GOLDEN,
	EFX_NVRAM_PHY,
	EFX_NVRAM_NULLPHY,
	EFX_NVRAM_NTYPES,
} efx_nvram_type_t;

extern	__checkReturn		int
efx_nvram_init(
	__in			efx_nic_t *enp);

#if EFSYS_OPT_DIAG

extern	__checkReturn		int
efx_nvram_test(
	__in			efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern	__checkReturn		int
efx_nvram_size(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *sizep);

extern	__checkReturn		int
efx_nvram_rw_start(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out_opt		size_t *pref_chunkp);

extern				void
efx_nvram_rw_finish(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type);

extern	__checkReturn		int
efx_nvram_get_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4]);

extern	__checkReturn		int
efx_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		int
efx_nvram_set_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint16_t version[4]);

extern	 __checkReturn		int
efx_nvram_erase(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type);

extern	__checkReturn		int
efx_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
efx_nvram_fini(
	__in			efx_nic_t *enp);

#endif	/* EFSYS_OPT_NVRAM */

#if EFSYS_OPT_BOOTCFG

extern				int
efx_bootcfg_read(
	__in			efx_nic_t *enp,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern				int
efx_bootcfg_write(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

#endif	/* EFSYS_OPT_BOOTCFG */

#if EFSYS_OPT_WOL

typedef enum efx_wol_type_e {
	EFX_WOL_TYPE_INVALID,
	EFX_WOL_TYPE_MAGIC,
	EFX_WOL_TYPE_BITMAP,
	EFX_WOL_TYPE_LINK,
	EFX_WOL_NTYPES,
} efx_wol_type_t;

typedef enum efx_lightsout_offload_type_e {
	EFX_LIGHTSOUT_OFFLOAD_TYPE_INVALID,
	EFX_LIGHTSOUT_OFFLOAD_TYPE_ARP,
	EFX_LIGHTSOUT_OFFLOAD_TYPE_NS,
} efx_lightsout_offload_type_t;

#define	EFX_WOL_BITMAP_MASK_SIZE    (48)
#define	EFX_WOL_BITMAP_VALUE_SIZE   (128)

typedef union efx_wol_param_u {
	struct {
		uint8_t mac_addr[6];
	} ewp_magic;
	struct {
		uint8_t mask[EFX_WOL_BITMAP_MASK_SIZE];   /* 1 bit per byte */
		uint8_t value[EFX_WOL_BITMAP_VALUE_SIZE]; /* value to match */
		uint8_t value_len;
	} ewp_bitmap;
} efx_wol_param_t;

typedef union efx_lightsout_offload_param_u {
	struct {
		uint8_t mac_addr[6];
		uint32_t ip;
	} elop_arp;
	struct {
		uint8_t mac_addr[6];
		uint32_t solicited_node[4];
		uint32_t ip[4];
	} elop_ns;
} efx_lightsout_offload_param_t;

extern	__checkReturn	int
efx_wol_init(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
efx_wol_filter_clear(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
efx_wol_filter_add(
	__in		efx_nic_t *enp,
	__in		efx_wol_type_t type,
	__in		efx_wol_param_t *paramp,
	__out		uint32_t *filter_idp);

extern	__checkReturn	int
efx_wol_filter_remove(
	__in		efx_nic_t *enp,
	__in		uint32_t filter_id);

extern	__checkReturn	int
efx_lightsout_offload_add(
	__in		efx_nic_t *enp,
	__in		efx_lightsout_offload_type_t type,
	__in		efx_lightsout_offload_param_t *paramp,
	__out		uint32_t *filter_idp);

extern	__checkReturn	int
efx_lightsout_offload_remove(
	__in		efx_nic_t *enp,
	__in		efx_lightsout_offload_type_t type,
	__in		uint32_t filter_id);

extern			void
efx_wol_fini(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_WOL */

#if EFSYS_OPT_DIAG

typedef enum efx_pattern_type_t {
	EFX_PATTERN_BYTE_INCREMENT = 0,
	EFX_PATTERN_ALL_THE_SAME,
	EFX_PATTERN_BIT_ALTERNATE,
	EFX_PATTERN_BYTE_ALTERNATE,
	EFX_PATTERN_BYTE_CHANGING,
	EFX_PATTERN_BIT_SWEEP,
	EFX_PATTERN_NTYPES
} efx_pattern_type_t;

typedef 		void
(*efx_sram_pattern_fn_t)(
	__in		size_t row,
	__in		boolean_t negate,
	__out		efx_qword_t *eqp);

extern	__checkReturn	int
efx_sram_test(
	__in		efx_nic_t *enp,
	__in		efx_pattern_type_t type);

#endif	/* EFSYS_OPT_DIAG */

extern	__checkReturn	int
efx_sram_buf_tbl_set(
	__in		efx_nic_t *enp,
	__in		uint32_t id,
	__in		efsys_mem_t *esmp,
	__in		size_t n);

extern		void
efx_sram_buf_tbl_clear(
	__in	efx_nic_t *enp,
	__in	uint32_t id,
	__in	size_t n);

#define	EFX_BUF_TBL_SIZE	0x20000

#define	EFX_BUF_SIZE		4096

/* EV */

typedef struct efx_evq_s	efx_evq_t;

#if EFSYS_OPT_QSTATS

/* START MKCONFIG GENERATED EfxHeaderEventQueueBlock d5614a5d669c8ca3 */
typedef enum efx_ev_qstat_e {
	EV_ALL,
	EV_RX,
	EV_RX_OK,
	EV_RX_RECOVERY,
	EV_RX_FRM_TRUNC,
	EV_RX_TOBE_DISC,
	EV_RX_PAUSE_FRM_ERR,
	EV_RX_BUF_OWNER_ID_ERR,
	EV_RX_IPV4_HDR_CHKSUM_ERR,
	EV_RX_TCP_UDP_CHKSUM_ERR,
	EV_RX_ETH_CRC_ERR,
	EV_RX_IP_FRAG_ERR,
	EV_RX_MCAST_PKT,
	EV_RX_MCAST_HASH_MATCH,
	EV_RX_TCP_IPV4,
	EV_RX_TCP_IPV6,
	EV_RX_UDP_IPV4,
	EV_RX_UDP_IPV6,
	EV_RX_OTHER_IPV4,
	EV_RX_OTHER_IPV6,
	EV_RX_NON_IP,
	EV_RX_OVERRUN,
	EV_TX,
	EV_TX_WQ_FF_FULL,
	EV_TX_PKT_ERR,
	EV_TX_PKT_TOO_BIG,
	EV_TX_UNEXPECTED,
	EV_GLOBAL,
	EV_GLOBAL_PHY,
	EV_GLOBAL_MNT,
	EV_GLOBAL_RX_RECOVERY,
	EV_DRIVER,
	EV_DRIVER_SRM_UPD_DONE,
	EV_DRIVER_TX_DESCQ_FLS_DONE,
	EV_DRIVER_RX_DESCQ_FLS_DONE,
	EV_DRIVER_RX_DESCQ_FLS_FAILED,
	EV_DRIVER_RX_DSC_ERROR,
	EV_DRIVER_TX_DSC_ERROR,
	EV_DRV_GEN,
	EV_MCDI_RESPONSE,
	EV_NQSTATS
} efx_ev_qstat_t;

/* END MKCONFIG GENERATED EfxHeaderEventQueueBlock */

#endif	/* EFSYS_OPT_QSTATS */

extern	__checkReturn	int
efx_ev_init(
	__in		efx_nic_t *enp);

extern		void
efx_ev_fini(
	__in		efx_nic_t *enp);

#define	EFX_MASK(_max, _min)	(-((_max) << 1) ^ -(_min))

#define	EFX_EVQ_MAXNEVS		32768
#define	EFX_EVQ_MINNEVS		512

#define	EFX_EVQ_NEVS_MASK	EFX_MASK(EFX_EVQ_MAXNEVS, EFX_EVQ_MINNEVS)

#define	EFX_EVQ_SIZE(_nevs)	((_nevs) * sizeof (efx_qword_t))
#define	EFX_EVQ_NBUFS(_nevs)	(EFX_EVQ_SIZE(_nevs) / EFX_BUF_SIZE)

extern	__checkReturn	int
efx_ev_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__deref_out	efx_evq_t **eepp);

extern		void
efx_ev_qpost(
	__in		efx_evq_t *eep,
	__in		uint16_t data);

typedef __checkReturn	boolean_t
(*efx_initialized_ev_t)(
	__in_opt	void *arg);

#define	EFX_PKT_UNICAST		0x0004
#define	EFX_PKT_START		0x0008

#define	EFX_PKT_VLAN_TAGGED	0x0010
#define	EFX_CKSUM_TCPUDP	0x0020
#define	EFX_CKSUM_IPV4		0x0040
#define	EFX_PKT_CONT		0x0080

#define	EFX_CHECK_VLAN		0x0100
#define	EFX_PKT_TCP		0x0200
#define	EFX_PKT_UDP		0x0400
#define	EFX_PKT_IPV4		0x0800

#define	EFX_PKT_IPV6		0x1000
#define	EFX_ADDR_MISMATCH	0x4000
#define	EFX_DISCARD		0x8000

#define	EFX_EV_RX_NLABELS	32
#define	EFX_EV_TX_NLABELS	32

typedef	__checkReturn	boolean_t
(*efx_rx_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label,
	__in		uint32_t id,
	__in		uint32_t size,
	__in		uint16_t flags);

typedef	__checkReturn	boolean_t
(*efx_tx_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label,
	__in		uint32_t id);

#define	EFX_EXCEPTION_RX_RECOVERY	0x00000001
#define	EFX_EXCEPTION_RX_DSC_ERROR	0x00000002
#define	EFX_EXCEPTION_TX_DSC_ERROR	0x00000003
#define	EFX_EXCEPTION_UNKNOWN_SENSOREVT	0x00000004
#define	EFX_EXCEPTION_FWALERT_SRAM	0x00000005
#define	EFX_EXCEPTION_UNKNOWN_FWALERT	0x00000006

typedef	__checkReturn	boolean_t
(*efx_exception_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label,
	__in		uint32_t data);

typedef	__checkReturn	boolean_t
(*efx_rxq_flush_done_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label);

typedef	__checkReturn	boolean_t
(*efx_rxq_flush_failed_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label);

typedef	__checkReturn	boolean_t
(*efx_txq_flush_done_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label);

typedef	__checkReturn	boolean_t
(*efx_software_ev_t)(
	__in_opt	void *arg,
	__in		uint16_t magic);

typedef	__checkReturn	boolean_t
(*efx_sram_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t code);

#define	EFX_SRAM_CLEAR		0
#define	EFX_SRAM_UPDATE		1
#define	EFX_SRAM_ILLEGAL_CLEAR	2

typedef	__checkReturn	boolean_t
(*efx_wake_up_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label);

typedef	__checkReturn	boolean_t
(*efx_timer_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label);

typedef __checkReturn	boolean_t
(*efx_link_change_ev_t)(
	__in_opt	void *arg,
	__in		efx_link_mode_t	link_mode);

#if EFSYS_OPT_MON_STATS

typedef __checkReturn	boolean_t
(*efx_monitor_ev_t)(
	__in_opt	void *arg,
	__in		efx_mon_stat_t id,
	__in		efx_mon_stat_value_t value);

#endif	/* EFSYS_OPT_MON_STATS */

#if EFSYS_OPT_MAC_STATS

typedef __checkReturn	boolean_t
(*efx_mac_stats_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t generation
	);

#endif	/* EFSYS_OPT_MAC_STATS */

typedef struct efx_ev_callbacks_s {
	efx_initialized_ev_t		eec_initialized;
	efx_rx_ev_t			eec_rx;
	efx_tx_ev_t			eec_tx;
	efx_exception_ev_t		eec_exception;
	efx_rxq_flush_done_ev_t		eec_rxq_flush_done;
	efx_rxq_flush_failed_ev_t	eec_rxq_flush_failed;
	efx_txq_flush_done_ev_t		eec_txq_flush_done;
	efx_software_ev_t		eec_software;
	efx_sram_ev_t			eec_sram;
	efx_wake_up_ev_t		eec_wake_up;
	efx_timer_ev_t			eec_timer;
	efx_link_change_ev_t		eec_link_change;
#if EFSYS_OPT_MON_STATS
	efx_monitor_ev_t		eec_monitor;
#endif	/* EFSYS_OPT_MON_STATS */
#if EFSYS_OPT_MAC_STATS
	efx_mac_stats_ev_t		eec_mac_stats;
#endif	/* EFSYS_OPT_MON_STATS */
} efx_ev_callbacks_t;

extern	__checkReturn	boolean_t
efx_ev_qpending(
	__in		efx_evq_t *eep,
	__in		unsigned int count);

#if EFSYS_OPT_EV_PREFETCH

extern			void
efx_ev_qprefetch(
	__in		efx_evq_t *eep,
	__in		unsigned int count);

#endif	/* EFSYS_OPT_EV_PREFETCH */

extern			void
efx_ev_qpoll(
	__in		efx_evq_t *eep,
	__inout		unsigned int *countp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

extern	__checkReturn	int
efx_ev_qmoderate(
	__in		efx_evq_t *eep,
	__in		unsigned int us);

extern	__checkReturn	int
efx_ev_qprime(
	__in		efx_evq_t *eep,
	__in		unsigned int count);

#if EFSYS_OPT_QSTATS

#if EFSYS_OPT_NAMES

extern		const char __cs *
efx_ev_qstat_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id);

#endif	/* EFSYS_OPT_NAMES */

extern					void
efx_ev_qstats_update(
	__in				efx_evq_t *eep,
	__inout_ecount(EV_NQSTATS)	efsys_stat_t *stat);

#endif	/* EFSYS_OPT_QSTATS */

extern		void
efx_ev_qdestroy(
	__in	efx_evq_t *eep);

/* RX */

typedef struct efx_rxq_s	efx_rxq_t;

extern	__checkReturn	int
efx_rx_init(
	__in		efx_nic_t *enp);

extern		void
efx_rx_fini(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_RX_HDR_SPLIT
	__checkReturn	int
efx_rx_hdr_split_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int hdr_buf_size,
	__in		unsigned int pld_buf_size);

#endif	/* EFSYS_OPT_RX_HDR_SPLIT */

#if EFSYS_OPT_RX_SCATTER
	__checkReturn	int
efx_rx_scatter_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int buf_size);
#endif	/* EFSYS_OPT_RX_SCATTER */

#if EFSYS_OPT_RX_SCALE

typedef enum efx_rx_hash_alg_e {
	EFX_RX_HASHALG_LFSR = 0,
	EFX_RX_HASHALG_TOEPLITZ
} efx_rx_hash_alg_t;

typedef enum efx_rx_hash_type_e {
	EFX_RX_HASH_IPV4 = 0,
	EFX_RX_HASH_TCPIPV4,
	EFX_RX_HASH_IPV6,
	EFX_RX_HASH_TCPIPV6,
} efx_rx_hash_type_t;

#define	EFX_RSS_TBL_SIZE	128	/* Rows in RX indirection table */
#define	EFX_MAXRSS	    	64	/* RX indirection entry range */
#define	EFX_MAXRSS_LEGACY   	16 	/* See bug16611 and bug17213 */

extern	__checkReturn	int
efx_rx_scale_mode_set(
	__in	efx_nic_t *enp,
	__in	efx_rx_hash_alg_t alg,
	__in	efx_rx_hash_type_t type,
	__in	boolean_t insert);

extern	__checkReturn	int
efx_rx_scale_tbl_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	unsigned int *table,
	__in		size_t n);

extern	__checkReturn	int
efx_rx_scale_toeplitz_ipv4_key_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n);

extern	__checkReturn	int
efx_rx_scale_toeplitz_ipv6_key_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n);

/*
 * The prefix is a byte array of one of the forms:
 *
 *  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 * XX.XX.XX.XX.XX.XX.XX.XX.XX.XX.XX.XX.TT.TT.TT.TT
 * XX.XX.XX.XX.XX.XX.XX.XX.XX.XX.XX.XX.XX.XX.LL.LL
 *
 * where:
 *
 * TT.TT.TT.TT is a 32-bit Toeplitz hash
 * LL.LL is a 16-bit LFSR hash
 *
 * Hash values are in network (big-endian) byte order.
 */

#define	EFX_RX_PREFIX_SIZE	16

#define	EFX_RX_HASH_VALUE(_func, _buffer)				\
	(((_func) == EFX_RX_HASHALG_LFSR) ?				\
		((uint16_t)(((_buffer)[14] << 8) | (_buffer)[15])) :	\
		((uint32_t)(((_buffer)[12] << 24) |			\
		    ((_buffer)[13] << 16) |				\
		    ((_buffer)[14] << 8) |				\
		    (_buffer)[15])))

#define	EFX_RX_HASH_SIZE(_func)						\
	(((_func) == EFX_RX_HASHALG_LFSR) ?				\
		sizeof (uint16_t) : 					\
		sizeof (uint32_t))

#endif	/* EFSYS_OPT_RX_SCALE */

#define	EFX_RXQ_MAXNDESCS	4096
#define	EFX_RXQ_MINNDESCS	512

#define	EFX_RXQ_NDESCS_MASK	EFX_MASK(EFX_RXQ_MAXNDESCS, EFX_RXQ_MINNDESCS)

#define	EFX_RXQ_SIZE(_ndescs)	((_ndescs) * sizeof (efx_qword_t))
#define	EFX_RXQ_NBUFS(_ndescs)	(EFX_RXQ_SIZE(_ndescs) / EFX_BUF_SIZE)
#define	EFX_RXQ_LIMIT(_ndescs)  ((_ndescs) - 16)

typedef enum efx_rxq_type_e {
	EFX_RXQ_TYPE_DEFAULT,
	EFX_RXQ_TYPE_SPLIT_HEADER,
	EFX_RXQ_TYPE_SPLIT_PAYLOAD,
	EFX_RXQ_TYPE_SCATTER,
	EFX_RXQ_NTYPES
} efx_rxq_type_t;

extern	__checkReturn	int
efx_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep,
	__deref_out	efx_rxq_t **erpp);

typedef struct efx_buffer_s {
	efsys_dma_addr_t	eb_addr;
	size_t			eb_size;
	boolean_t		eb_eop;
} efx_buffer_t;

extern			void
efx_rx_qpost(
	__in		efx_rxq_t *erp,
	__in_ecount(n)	efsys_dma_addr_t *addrp,
	__in		size_t size,
	__in		unsigned int n,
	__in		unsigned int completed,
	__in		unsigned int added);

extern		void
efx_rx_qpush(
	__in	efx_rxq_t *erp,
	__in	unsigned int added);

extern		void
efx_rx_qflush(
	__in	efx_rxq_t *erp);

extern		void
efx_rx_qenable(
	__in	efx_rxq_t *erp);

extern		void
efx_rx_qdestroy(
	__in	efx_rxq_t *erp);

/* TX */

typedef struct efx_txq_s	efx_txq_t;

#if EFSYS_OPT_QSTATS

/* START MKCONFIG GENERATED EfxHeaderTransmitQueueBlock 536c5fa5014944bf */
typedef enum efx_tx_qstat_e {
	TX_POST,
	TX_UNALIGNED_SPLIT,
	TX_NQSTATS
} efx_tx_qstat_t;

/* END MKCONFIG GENERATED EfxHeaderTransmitQueueBlock */

#endif	/* EFSYS_OPT_QSTATS */

extern	__checkReturn	int
efx_tx_init(
	__in		efx_nic_t *enp);

extern		void
efx_tx_fini(
	__in	efx_nic_t *enp);

#define	EFX_TXQ_MAXNDESCS	4096
#define	EFX_TXQ_MINNDESCS	512

#define	EFX_TXQ_NDESCS_MASK	EFX_MASK(EFX_TXQ_MAXNDESCS, EFX_TXQ_MINNDESCS)

#define	EFX_TXQ_SIZE(_ndescs)	((_ndescs) * sizeof (efx_qword_t))
#define	EFX_TXQ_NBUFS(_ndescs)	(EFX_TXQ_SIZE(_ndescs) / EFX_BUF_SIZE)
#define	EFX_TXQ_LIMIT(_ndescs)  ((_ndescs) - 16)

extern	__checkReturn	int
efx_tx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		uint16_t flags,
	__in		efx_evq_t *eep,
	__deref_out	efx_txq_t **etpp);

extern	__checkReturn	int
efx_tx_qpost(
	__in		efx_txq_t *etp,
	__in_ecount(n)	efx_buffer_t *eb,
	__in		unsigned int n,
	__in		unsigned int completed,
	__inout		unsigned int *addedp);

extern		void
efx_tx_qpush(
	__in	efx_txq_t *etp,
	__in	unsigned int added);

extern		void
efx_tx_qflush(
	__in	efx_txq_t *etp);

extern		void
efx_tx_qenable(
	__in	efx_txq_t *etp);

#if EFSYS_OPT_QSTATS

#if EFSYS_OPT_NAMES

extern		const char __cs *
efx_tx_qstat_name(
	__in	efx_nic_t *etp,
	__in	unsigned int id);

#endif	/* EFSYS_OPT_NAMES */

extern					void
efx_tx_qstats_update(
	__in				efx_txq_t *etp,
	__inout_ecount(TX_NQSTATS)	efsys_stat_t *stat);

#endif	/* EFSYS_OPT_QSTATS */

extern		void
efx_tx_qdestroy(
	__in	efx_txq_t *etp);


/* FILTER */

#if EFSYS_OPT_FILTER

typedef enum efx_filter_flag_e {
	EFX_FILTER_FLAG_RX_RSS = 0x01,		/* use RSS to spread across
						 * multiple queues */
	EFX_FILTER_FLAG_RX_SCATTER = 0x02,	/* enable RX scatter */
	EFX_FILTER_FLAG_RX_OVERRIDE_IP = 0x04,	/* MAC filter overrides
						 * any matching IP filter */
} efx_filter_flag_t;

typedef struct efx_filter_spec_s {
	uint8_t		efs_type;
	uint8_t		efs_flags;
	uint16_t	efs_dmaq_id;
	uint32_t	efs_dword[3];
} efx_filter_spec_t;

extern	__checkReturn	int
efx_filter_init(
	__in		efx_nic_t *enp);

extern			void
efx_filter_fini(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
efx_rx_filter_insert(
	__in		efx_rxq_t *erp,
	__inout		efx_filter_spec_t *spec);

extern	__checkReturn	int
efx_rx_filter_remove(
	__in		efx_rxq_t *erp,
	__inout		efx_filter_spec_t *spec);

			void
efx_filter_restore(
	__in		efx_nic_t *enp);

extern			void
efx_filter_spec_rx_ipv4_tcp_full(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint32_t src_ip,
	__in		uint16_t src_tcp,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_tcp);

extern			void
efx_filter_spec_rx_ipv4_tcp_wild(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_tcp);

extern			void
efx_filter_spec_rx_ipv4_udp_full(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint32_t src_ip,
	__in		uint16_t src_udp,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_udp);

extern			void
efx_filter_spec_rx_ipv4_udp_wild(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_udp);

extern			void
efx_filter_spec_rx_mac_full(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint16_t vlan_id,
	__in		uint8_t *dest_mac);

extern			void
efx_filter_spec_rx_mac_wild(
	__inout		efx_filter_spec_t *spec,
	__in		unsigned int flags,
	__in		uint8_t *dest_mac);


extern	__checkReturn	int
efx_tx_filter_insert(
	__in		efx_txq_t *etp,
	__inout		efx_filter_spec_t *spec);

extern	__checkReturn	int
efx_tx_filter_remove(
	__in		efx_txq_t *etp,
	__inout		efx_filter_spec_t *spec);

extern			void
efx_filter_spec_tx_ipv4_tcp_full(
	__inout		efx_filter_spec_t *spec,
	__in		uint32_t src_ip,
	__in		uint16_t src_tcp,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_tcp);

extern			void
efx_filter_spec_tx_ipv4_tcp_wild(
	__inout		efx_filter_spec_t *spec,
	__in		uint32_t src_ip,
	__in		uint16_t src_tcp);

extern			void
efx_filter_spec_tx_ipv4_udp_full(
	__inout		efx_filter_spec_t *spec,
	__in		uint32_t src_ip,
	__in		uint16_t src_udp,
	__in		uint32_t dest_ip,
	__in		uint16_t dest_udp);

extern			void
efx_filter_spec_tx_ipv4_udp_wild(
	__inout		efx_filter_spec_t *spec,
	__in		uint32_t src_ip,
	__in		uint16_t src_udp);

extern			void
efx_filter_spec_tx_mac_full(
	__inout		efx_filter_spec_t *spec,
	__in		uint16_t vlan_id,
	__in		uint8_t *src_mac);

extern			void
efx_filter_spec_tx_mac_wild(
	__inout		efx_filter_spec_t *spec,
	__in		uint8_t *src_mac);

#endif	/* EFSYS_OPT_FILTER */


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EFX_H */
