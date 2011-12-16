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
 *
 * $FreeBSD$
 */

#ifndef	_SYS_EFX_IMPL_H
#define	_SYS_EFX_IMPL_H

#include "efsys.h"
#include "efx.h"
#include "efx_regs.h"

#if EFSYS_OPT_FALCON
#include "falcon_impl.h"
#endif	/* EFSYS_OPT_FALCON */

#if EFSYS_OPT_SIENA
#include "siena_impl.h"
#endif	/* EFSYS_OPT_SIENA */

#ifdef	__cplusplus
extern "C" {
#endif

#define	EFX_MOD_MCDI	0x00000001
#define	EFX_MOD_PROBE	0x00000002
#define	EFX_MOD_NVRAM	0x00000004
#define	EFX_MOD_VPD	0x00000008
#define	EFX_MOD_NIC	0x00000010
#define	EFX_MOD_INTR	0x00000020
#define	EFX_MOD_EV	0x00000040
#define	EFX_MOD_RX	0x00000080
#define	EFX_MOD_TX	0x00000100
#define	EFX_MOD_PORT	0x00000200
#define	EFX_MOD_MON	0x00000400
#define	EFX_MOD_WOL	0x00000800
#define	EFX_MOD_FILTER	0x00001000

#define	EFX_RESET_MAC	0x00000001
#define	EFX_RESET_PHY	0x00000002

typedef enum efx_mac_type_e {
	EFX_MAC_INVALID = 0,
	EFX_MAC_FALCON_GMAC,
	EFX_MAC_FALCON_XMAC,
	EFX_MAC_SIENA,
	EFX_MAC_NTYPES
} efx_mac_type_t;

typedef struct efx_mac_ops_s {
	int		(*emo_reset)(efx_nic_t *); /* optional */
	int		(*emo_poll)(efx_nic_t *, efx_link_mode_t *);
	int		(*emo_up)(efx_nic_t *, boolean_t *);
	int		(*emo_reconfigure)(efx_nic_t *);
#if EFSYS_OPT_LOOPBACK
	int		(*emo_loopback_set)(efx_nic_t *, efx_link_mode_t,
					    efx_loopback_type_t);
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_MAC_STATS
	int		(*emo_stats_upload)(efx_nic_t *, efsys_mem_t *);
	int		(*emo_stats_periodic)(efx_nic_t *, efsys_mem_t *,
					      uint16_t, boolean_t);
	int		(*emo_stats_update)(efx_nic_t *, efsys_mem_t *,
					    efsys_stat_t *, uint32_t *);
#endif	/* EFSYS_OPT_MAC_STATS */
} efx_mac_ops_t;

typedef struct efx_phy_ops_s {
	int		(*epo_power)(efx_nic_t *, boolean_t); /* optional */
	int		(*epo_reset)(efx_nic_t *);
	int		(*epo_reconfigure)(efx_nic_t *);
	int		(*epo_verify)(efx_nic_t *);
	int		(*epo_uplink_check)(efx_nic_t *,
					    boolean_t *); /* optional */
	int		(*epo_downlink_check)(efx_nic_t *, efx_link_mode_t *,
					      unsigned int *, uint32_t *);
	int		(*epo_oui_get)(efx_nic_t *, uint32_t *);
#if EFSYS_OPT_PHY_STATS
	int		(*epo_stats_update)(efx_nic_t *, efsys_mem_t *,
					    uint32_t *);
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
#if EFSYS_OPT_NAMES
	const char	__cs *(*epo_prop_name)(efx_nic_t *, unsigned int);
#endif	/* EFSYS_OPT_PHY_PROPS */
	int		(*epo_prop_get)(efx_nic_t *, unsigned int, uint32_t,
					uint32_t *);
	int		(*epo_prop_set)(efx_nic_t *, unsigned int, uint32_t);
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_PHY_BIST
	int		(*epo_bist_start)(efx_nic_t *, efx_phy_bist_type_t);
	int		(*epo_bist_poll)(efx_nic_t *, efx_phy_bist_type_t,
					 efx_phy_bist_result_t *, uint32_t *,
					 unsigned long *, size_t);
	void		(*epo_bist_stop)(efx_nic_t *, efx_phy_bist_type_t);
#endif	/* EFSYS_OPT_PHY_BIST */
} efx_phy_ops_t;

typedef struct efx_port_s {
	efx_mac_type_t		ep_mac_type;
	uint32_t  		ep_phy_type;
	uint8_t			ep_port;
	uint32_t		ep_mac_pdu;
	uint8_t			ep_mac_addr[6];
	efx_link_mode_t		ep_link_mode;
	boolean_t		ep_unicst;
	boolean_t		ep_brdcst;
	unsigned int		ep_fcntl;
	boolean_t		ep_fcntl_autoneg;
	efx_oword_t		ep_multicst_hash[2];
#if EFSYS_OPT_LOOPBACK
	efx_loopback_type_t	ep_loopback_type;
	efx_link_mode_t		ep_loopback_link_mode;
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_PHY_FLAGS
	uint32_t		ep_phy_flags;
#endif	/* EFSYS_OPT_PHY_FLAGS */
#if EFSYS_OPT_PHY_LED_CONTROL
	efx_phy_led_mode_t	ep_phy_led_mode;
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */
	efx_phy_media_type_t	ep_fixed_port_type;
	efx_phy_media_type_t	ep_module_type;
	uint32_t		ep_adv_cap_mask;
	uint32_t		ep_lp_cap_mask;
	uint32_t		ep_default_adv_cap_mask;
	uint32_t		ep_phy_cap_mask;
#if EFSYS_OPT_PHY_TXC43128 || EFSYS_OPT_PHY_QT2025C
	union {
		struct {
			unsigned int	bug10934_count;
		} ep_txc43128;
		struct {
			unsigned int	bug17190_count;
		} ep_qt2025c;
	};
#endif
	boolean_t		ep_mac_poll_needed; /* falcon only */
	boolean_t		ep_mac_up; /* falcon only */
	uint32_t		ep_fwver; /* falcon only */
	boolean_t		ep_mac_drain;
	boolean_t		ep_mac_stats_pending;
#if EFSYS_OPT_PHY_BIST
	efx_phy_bist_type_t	ep_current_bist;
#endif
	efx_mac_ops_t		*ep_emop;
	efx_phy_ops_t		*ep_epop;
} efx_port_t;

typedef struct efx_mon_ops_s {
	int	(*emo_reset)(efx_nic_t *);
	int	(*emo_reconfigure)(efx_nic_t *);
#if EFSYS_OPT_MON_STATS
	int	(*emo_stats_update)(efx_nic_t *, efsys_mem_t *,
				    efx_mon_stat_value_t *);
#endif	/* EFSYS_OPT_MON_STATS */
} efx_mon_ops_t;

typedef struct efx_mon_s {
	efx_mon_type_t	em_type;
	efx_mon_ops_t	*em_emop;
} efx_mon_t;

typedef struct efx_intr_s {
	efx_intr_type_t	ei_type;
	efsys_mem_t	*ei_esmp;
	unsigned int	ei_level;
} efx_intr_t;

typedef struct efx_nic_ops_s {
	int	(*eno_probe)(efx_nic_t *);
	int	(*eno_reset)(efx_nic_t *);
	int	(*eno_init)(efx_nic_t *);
#if EFSYS_OPT_DIAG
	int	(*eno_sram_test)(efx_nic_t *, efx_sram_pattern_fn_t);
	int	(*eno_register_test)(efx_nic_t *);
#endif	/* EFSYS_OPT_DIAG */
	void	(*eno_fini)(efx_nic_t *);
	void	(*eno_unprobe)(efx_nic_t *);
} efx_nic_ops_t;

#define EFX_TXQ_LIMIT_TARGET 259
#define EFX_RXQ_LIMIT_TARGET 768

#if EFSYS_OPT_FILTER

typedef enum efx_filter_type_e {
	EFX_FILTER_RX_TCP_FULL,	/* TCP/IPv4 4-tuple {dIP,dTCP,sIP,sTCP} */
	EFX_FILTER_RX_TCP_WILD,	/* TCP/IPv4 dest    {dIP,dTCP,  -,   -} */
	EFX_FILTER_RX_UDP_FULL,	/* UDP/IPv4 4-tuple {dIP,dUDP,sIP,sUDP} */
	EFX_FILTER_RX_UDP_WILD,	/* UDP/IPv4 dest    {dIP,dUDP,  -,   -} */

#if EFSYS_OPT_SIENA
	EFX_FILTER_RX_MAC_FULL,	/* Ethernet {dMAC,VLAN} */
	EFX_FILTER_RX_MAC_WILD,	/* Ethernet {dMAC,   -} */

	EFX_FILTER_TX_TCP_FULL,		/* TCP/IPv4 {dIP,dTCP,sIP,sTCP} */
	EFX_FILTER_TX_TCP_WILD,		/* TCP/IPv4 {  -,   -,sIP,sTCP} */
	EFX_FILTER_TX_UDP_FULL,		/* UDP/IPv4 {dIP,dTCP,sIP,sTCP} */
	EFX_FILTER_TX_UDP_WILD,		/* UDP/IPv4 source (host, port) */

	EFX_FILTER_TX_MAC_FULL,		/* Ethernet source (MAC address, VLAN ID) */
	EFX_FILTER_TX_MAC_WILD,		/* Ethernet source (MAC address) */
#endif /* EFSYS_OPT_SIENA */

	EFX_FILTER_NTYPES
} efx_filter_type_t;

typedef enum efx_filter_tbl_id_e {
	EFX_FILTER_TBL_RX_IP = 0,
	EFX_FILTER_TBL_RX_MAC,
	EFX_FILTER_TBL_TX_IP,
	EFX_FILTER_TBL_TX_MAC,
	EFX_FILTER_NTBLS
} efx_filter_tbl_id_t;

typedef struct efx_filter_tbl_s {
	int			eft_size;	/* number of entries */
	int			eft_used;	/* active count */
	uint32_t		*eft_bitmap;	/* active bitmap */
	efx_filter_spec_t	*eft_spec;	/* array of saved specs */
} efx_filter_tbl_t;

typedef struct efx_filter_s {
	efx_filter_tbl_t	ef_tbl[EFX_FILTER_NTBLS];
	unsigned int		ef_depth[EFX_FILTER_NTYPES];
} efx_filter_t;


extern	__checkReturn	int
efx_filter_insert_filter(
	__in		efx_nic_t *enp,
	__in		efx_filter_spec_t *spec,
	__in		boolean_t replace);

extern	__checkReturn	int
efx_filter_remove_filter(
	__in		efx_nic_t *enp,
	__in		efx_filter_spec_t *spec);

extern			void
efx_filter_remove_index(
	__inout		efx_nic_t *enp,
	__in		efx_filter_type_t type,
	__in		int filter_idx);

extern			void
efx_filter_redirect_index(
	__inout		efx_nic_t *enp,
	__in		efx_filter_type_t type,
	__in		int filter_index,
	__in		int rxq_index);

extern	__checkReturn	int
efx_filter_clear_tbl(
	__in		efx_nic_t *enp,
	__in		efx_filter_tbl_id_t tbl);

#endif	/* EFSYS_OPT_FILTER */

#if EFSYS_OPT_NVRAM
typedef struct efx_nvram_ops_s {
#if EFSYS_OPT_DIAG
	int	(*envo_test)(efx_nic_t *);
#endif	/* EFSYS_OPT_DIAG */
	int	(*envo_size)(efx_nic_t *, efx_nvram_type_t, size_t *);
	int	(*envo_get_version)(efx_nic_t *, efx_nvram_type_t,
				    uint32_t *, uint16_t *);
	int	(*envo_rw_start)(efx_nic_t *, efx_nvram_type_t, size_t *);
	int	(*envo_read_chunk)(efx_nic_t *, efx_nvram_type_t,
				    unsigned int, caddr_t, size_t);
	int	(*envo_erase)(efx_nic_t *, efx_nvram_type_t);
	int	(*envo_write_chunk)(efx_nic_t *, efx_nvram_type_t,
				    unsigned int, caddr_t, size_t);
	void	(*envo_rw_finish)(efx_nic_t *, efx_nvram_type_t);
	int	(*envo_set_version)(efx_nic_t *, efx_nvram_type_t, uint16_t *);

} efx_nvram_ops_t;
#endif /* EFSYS_OPT_NVRAM */

#if EFSYS_OPT_VPD
typedef struct efx_vpd_ops_s {
	int	(*evpdo_init)(efx_nic_t *);
	int	(*evpdo_size)(efx_nic_t *, size_t *);
	int	(*evpdo_read)(efx_nic_t *, caddr_t, size_t);
	int	(*evpdo_verify)(efx_nic_t *, caddr_t, size_t);
	int	(*evpdo_reinit)(efx_nic_t *, caddr_t, size_t);
	int	(*evpdo_get)(efx_nic_t *, caddr_t, size_t, efx_vpd_value_t *);
	int	(*evpdo_set)(efx_nic_t *, caddr_t, size_t, efx_vpd_value_t *);
	int	(*evpdo_next)(efx_nic_t *, caddr_t, size_t, efx_vpd_value_t *,
			    unsigned int *);
	int	(*evpdo_write)(efx_nic_t *, caddr_t, size_t);
	void	(*evpdo_fini)(efx_nic_t *);
} efx_vpd_ops_t;
#endif	/* EFSYS_OPT_VPD */

struct efx_nic_s {
	uint32_t		en_magic;
	efx_family_t		en_family;
	uint32_t		en_features;
	efsys_identifier_t	*en_esip;
	efsys_lock_t		*en_eslp;
	efsys_bar_t 		*en_esbp;
	unsigned int		en_mod_flags;
	unsigned int		en_reset_flags;
	efx_nic_cfg_t		en_nic_cfg;
	efx_port_t		en_port;
	efx_mon_t		en_mon;
	efx_intr_t		en_intr;
	uint32_t		en_ev_qcount;
	uint32_t		en_rx_qcount;
	uint32_t		en_tx_qcount;
	efx_nic_ops_t		*en_enop;
#if EFSYS_OPT_FILTER
	efx_filter_t		en_filter;
#endif	/* EFSYS_OPT_FILTER */
#if EFSYS_OPT_NVRAM
	efx_nvram_type_t	en_nvram_locked;
	efx_nvram_ops_t		*en_envop;
#endif	/* EFSYS_OPT_NVRAM */
#if EFSYS_OPT_VPD
	efx_vpd_ops_t		*en_evpdop;
#endif	/* EFSYS_OPT_VPD */
	union {
#if EFSYS_OPT_FALCON
		struct {
			falcon_spi_dev_t	enu_fsd[FALCON_SPI_NTYPES];
			falcon_i2c_t		enu_fip;
			boolean_t		enu_i2c_locked;
#if EFSYS_OPT_FALCON_NIC_CFG_OVERRIDE
			const uint8_t		*enu_forced_cfg;
#endif	/* EFSYS_OPT_FALCON_NIC_CFG_OVERRIDE */
			uint8_t			enu_mon_devid;
#if EFSYS_OPT_PCIE_TUNE
			unsigned int 		enu_nlanes;
#endif	/* EFSYS_OPT_PCIE_TUNE */
			uint16_t		enu_board_rev;
			boolean_t		enu_internal_sram;
			uint8_t			enu_sram_num_bank;
			uint8_t			enu_sram_bank_size;
		} falcon;
#endif	/* EFSYS_OPT_FALCON */
#if EFSYS_OPT_SIENA
		struct {
#if EFSYS_OPT_MCDI
			efx_mcdi_iface_t	enu_mip;
#endif	/* EFSYS_OPT_MCDI */
#if EFSYS_OPT_NVRAM || EFSYS_OPT_VPD
			unsigned int		enu_partn_mask;
#endif	/* EFSYS_OPT_NVRAM || EFSYS_OPT_VPD */
#if EFSYS_OPT_VPD
			caddr_t			enu_svpd;
			size_t			enu_svpd_length;
#endif	/* EFSYS_OPT_VPD */
		} siena;
#endif	/* EFSYS_OPT_SIENA */
	} en_u;
};


#define	EFX_NIC_MAGIC	0x02121996

typedef	boolean_t (*efx_ev_handler_t)(efx_evq_t *, efx_qword_t *,
    const efx_ev_callbacks_t *, void *);

struct efx_evq_s {
	uint32_t			ee_magic;
	efx_nic_t			*ee_enp;
	unsigned int			ee_index;
	unsigned int			ee_mask;
	efsys_mem_t			*ee_esmp;
#if EFSYS_OPT_QSTATS
	uint32_t			ee_stat[EV_NQSTATS];
#endif	/* EFSYS_OPT_QSTATS */
	efx_ev_handler_t		ee_handler[1 << FSF_AZ_EV_CODE_WIDTH];
};

#define	EFX_EVQ_MAGIC	0x08081997

#define	EFX_EV_TIMER_QUANTUM	5

struct efx_rxq_s {
	uint32_t			er_magic;
	efx_nic_t			*er_enp;
	unsigned int			er_index;
	unsigned int			er_mask;
	efsys_mem_t			*er_esmp;
};

#define	EFX_RXQ_MAGIC	0x15022005

struct efx_txq_s {
	uint32_t			et_magic;
	efx_nic_t			*et_enp;
	unsigned int			et_index;
	unsigned int			et_mask;
	efsys_mem_t			*et_esmp;
#if EFSYS_OPT_QSTATS
	uint32_t			et_stat[TX_NQSTATS];
#endif	/* EFSYS_OPT_QSTATS */
};

#define	EFX_TXQ_MAGIC	0x05092005

#define	EFX_MAC_ADDR_COPY(_dst, _src)					\
	do {								\
		(_dst)[0] = (_src)[0];					\
		(_dst)[1] = (_src)[1];					\
		(_dst)[2] = (_src)[2];					\
		(_dst)[3] = (_src)[3];					\
		(_dst)[4] = (_src)[4];					\
		(_dst)[5] = (_src)[5];					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#if EFSYS_OPT_CHECK_REG
#define	EFX_CHECK_REG(_enp, _reg)					\
	do {								\
		const char __cs *name = #_reg;				\
		char min = name[4];					\
		char max = name[5];					\
		char rev;						\
									\
		switch ((_enp)->en_family) {				\
		case EFX_FAMILY_FALCON:					\
			rev = 'B';					\
			break;						\
									\
		case EFX_FAMILY_SIENA:					\
			rev = 'C';					\
			break;						\
									\
		default:						\
			rev = '?';					\
			break;						\
		}							\
									\
		EFSYS_ASSERT3S(rev, >=, min);				\
		EFSYS_ASSERT3S(rev, <=, max);				\
									\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)
#else
#define	EFX_CHECK_REG(_enp, _reg) do {					\
	_NOTE(CONSTANTCONDITION)					\
	} while(B_FALSE)
#endif

#define	EFX_BAR_READD(_enp, _reg, _edp, _lock)				\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READD((_enp)->en_esbp, _reg ## _OFST,		\
		    (_edp), (_lock));					\
		EFSYS_PROBE3(efx_bar_readd, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_WRITED(_enp, _reg, _edp, _lock)				\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE3(efx_bar_writed, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
		EFSYS_BAR_WRITED((_enp)->en_esbp, _reg ## _OFST,	\
		    (_edp), (_lock));					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_READQ(_enp, _reg, _eqp)					\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READQ((_enp)->en_esbp, _reg ## _OFST,		\
		    (_eqp));						\
		EFSYS_PROBE4(efx_bar_readq, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_WRITEQ(_enp, _reg, _eqp)				\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE4(efx_bar_writeq, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
		EFSYS_BAR_WRITEQ((_enp)->en_esbp, _reg ## _OFST,	\
		    (_eqp));						\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_READO(_enp, _reg, _eop)					\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READO((_enp)->en_esbp, _reg ## _OFST,		\
		    (_eop), B_TRUE);					\
		EFSYS_PROBE6(efx_bar_reado, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_WRITEO(_enp, _reg, _eop)				\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE6(efx_bar_writeo, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
		EFSYS_BAR_WRITEO((_enp)->en_esbp, _reg ## _OFST,	\
		    (_eop), B_TRUE);					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_READD(_enp, _reg, _index, _edp, _lock)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READD((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_edp), (_lock));					\
		EFSYS_PROBE4(efx_bar_tbl_readd, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_WRITED(_enp, _reg, _index, _edp, _lock)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE4(efx_bar_tbl_writed, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
		EFSYS_BAR_WRITED((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_edp), (_lock));					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_WRITED3(_enp, _reg, _index, _edp, _lock)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE4(efx_bar_tbl_writed, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
		EFSYS_BAR_WRITED((_enp)->en_esbp,			\
		    (_reg ## _OFST +					\
		    (3 * sizeof (efx_dword_t)) + 			\
		    ((_index) * _reg ## _STEP)),			\
		    (_edp), (_lock));					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_READQ(_enp, _reg, _index, _eqp)			\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READQ((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_eqp));						\
		EFSYS_PROBE5(efx_bar_tbl_readq, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_WRITEQ(_enp, _reg, _index, _eqp)			\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE5(efx_bar_tbl_writeq, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
		EFSYS_BAR_WRITEQ((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_eqp));						\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_READO(_enp, _reg, _index, _eop)			\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READO((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_eop), B_TRUE);					\
		EFSYS_PROBE7(efx_bar_tbl_reado, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_WRITEO(_enp, _reg, _index, _eop)			\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE7(efx_bar_tbl_writeo, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
		EFSYS_BAR_WRITEO((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_eop), B_TRUE);					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

extern	__checkReturn	int
efx_mac_select(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
efx_phy_probe(
	__in		efx_nic_t *enp);

extern			void
efx_phy_unprobe(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_VPD

/* VPD utility functions */

extern	__checkReturn		int
efx_vpd_hunk_length(
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__out			size_t *lengthp);

extern	__checkReturn		int
efx_vpd_hunk_verify(
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__out_opt		boolean_t *cksummedp);

extern	__checkReturn		int
efx_vpd_hunk_reinit(
	__in			caddr_t data,
	__in			size_t size,
	__in			boolean_t wantpid);

extern	__checkReturn		int
efx_vpd_hunk_get(
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			efx_vpd_tag_t tag,
	__in			efx_vpd_keyword_t keyword,
	__out			unsigned int *payloadp,
	__out			uint8_t *paylenp);

extern	__checkReturn			int
efx_vpd_hunk_next(
	__in_bcount(size)		caddr_t data,
	__in				size_t size,
	__out				efx_vpd_tag_t *tagp,
	__out				efx_vpd_keyword_t *keyword,
	__out_bcount_opt(*paylenp)	unsigned int *payloadp,
	__out_opt			uint8_t *paylenp,
	__inout				unsigned int *contp);

extern	__checkReturn		int
efx_vpd_hunk_set(
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			efx_vpd_value_t *evvp);

#endif	/* EFSYS_OPT_VPD */

#if EFSYS_OPT_DIAG

extern	efx_sram_pattern_fn_t	__cs __efx_sram_pattern_fns[];

typedef struct efx_register_set_s {
	unsigned int		address;
	unsigned int		step;
	unsigned int		rows;
	efx_oword_t		mask;
} efx_register_set_t;

extern	__checkReturn	int
efx_nic_test_registers(
	__in		efx_nic_t *enp,
	__in		efx_register_set_t *rsp,
	__in		size_t count);

extern	__checkReturn	int
efx_nic_test_tables(
	__in		efx_nic_t *enp,
	__in		efx_register_set_t *rsp,
	__in		efx_pattern_type_t pattern,
	__in		size_t count);

#endif	/* EFSYS_OPT_DIAG */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EFX_IMPL_H */
