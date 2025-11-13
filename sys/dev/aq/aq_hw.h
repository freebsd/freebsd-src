/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3)The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AQ_HW_H_
#define _AQ_HW_H_

#include <stdbool.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <machine/cpufunc.h>
#include <sys/endian.h>
#include "aq_common.h"

#define AQ_WRITE_REG(hw, reg, value) writel(((hw)->hw_addr + (reg)), htole32(value))
    
#define AQ_READ_REG(hw, reg) le32toh(readl((hw)->hw_addr + reg))


#define AQ_WRITE_REG_BIT(hw, reg, msk, shift, value) do { \
    if (msk ^ ~0) { \
        u32 reg_old, reg_new = 0U; \
        reg_old = AQ_READ_REG(hw, reg); \
        reg_new = (reg_old & (~msk)) | (value << shift); \
        if (reg_old != reg_new) \
            AQ_WRITE_REG(hw, reg, reg_new); \
    } else { \
        AQ_WRITE_REG(hw, reg, value); \
    } } while(0)


#define AQ_READ_REG_BIT(a, reg, msk, shift) ( \
    ((AQ_READ_REG(a, reg) & msk) >> shift))        

#define AQ_HW_FLUSH() { (void)AQ_READ_REG(hw, 0x10); }

#define aq_hw_write_reg_bit AQ_WRITE_REG_BIT

#define aq_hw_write_reg AQ_WRITE_REG

/* Statistics  */
struct aq_hw_stats {
    u64 crcerrs;
};

struct aq_hw_stats_s {
    u32 uprc;
    u32 mprc;
    u32 bprc;
    u32 erpt;
    u32 uptc;
    u32 mptc;
    u32 bptc;
    u32 erpr;
    u32 mbtc;
    u32 bbtc;
    u32 mbrc;
    u32 bbrc;
    u32 ubrc;
    u32 ubtc;
    u32 ptc;
    u32 prc;
    u32 dpc;
    u32 cprc;
} __attribute__((__packed__));

union ip_addr {
    struct {
        u8 addr[16];
    } v6;
    struct {
        u8 padding[12];
        u8 addr[4];
    } v4;
} __attribute__((__packed__));

struct aq_hw_fw_mbox {
    u32 version;
    u32 transaction_id;
    int error;
    struct aq_hw_stats_s stats;
} __attribute__((__packed__));

typedef struct aq_hw_fw_version {
    union {
        struct {
            u16 build_number;
            u8 minor_version;
            u8 major_version;
        };
        u32 raw;
    };
} aq_hw_fw_version;

enum aq_hw_irq_type {
    aq_irq_invalid = 0,
    aq_irq_legacy = 1,
    aq_irq_msi = 2,
    aq_irq_msix = 3,
};

struct aq_hw_fc_info {
    bool fc_rx;
    bool fc_tx;
};

struct aq_hw {
    void *aq_dev;
    u8 *hw_addr;
    u32 regs_size;

    u8 mac_addr[ETH_MAC_LEN];

    enum aq_hw_irq_type irq_type;
    
    struct aq_hw_fc_info fc;
    u16 link_rate;

    u16 device_id;
    u16 subsystem_vendor_id;
    u16 subsystem_device_id;
    u16 vendor_id;
    u8  revision_id;

    /* Interrupt Moderation value. */
    int itr;

    /* Firmware-related stuff. */
    aq_hw_fw_version fw_version;
    const struct aq_firmware_ops* fw_ops;
    bool rbl_enabled;
    bool fast_start_enabled;
    bool flash_present;
    u32 chip_features;
    u64 fw_caps;

	bool lro_enabled;

    u32 mbox_addr;
    struct aq_hw_fw_mbox mbox;
};

#define aq_hw_s aq_hw

#define AQ_HW_MAC      0U
#define AQ_HW_MAC_MIN  1U
#define AQ_HW_MAC_MAX  33U

#define HW_ATL_B0_MIN_RXD 32U
#define HW_ATL_B0_MIN_TXD 32U
#define HW_ATL_B0_MAX_RXD 4096U /* in fact up to 8184, but closest to power of 2 */
#define HW_ATL_B0_MAX_TXD 4096U /* in fact up to 8184, but closest to power of 2 */

#define HW_ATL_B0_MTU_JUMBO  16352U
#define HW_ATL_B0_TSO_SIZE (160*1024)
#define HW_ATL_B0_RINGS_MAX 32U
#define HW_ATL_B0_LRO_RXD_MAX 16U

#define AQ_HW_FW_SM_RAM        0x2U

#define AQ_HW_MPI_STATE_MSK    0x00FFU
#define AQ_HW_MPI_STATE_SHIFT  0U

#define AQ_HW_MPI_CONTROL_ADR       0x0368U
#define AQ_HW_MPI_STATE_ADR         0x036CU

#define HW_ATL_RSS_INDIRECTION_TABLE_MAX  64U
#define HW_ATL_RSS_HASHKEY_SIZE           40U

/* PCI core control register */
#define AQ_HW_PCI_REG_CONTROL_6_ADR 0x1014U
/* tx dma total request limit */
#define AQ_HW_TX_DMA_TOTAL_REQ_LIMIT_ADR 0x00007b20U

#define AQ_HW_TXBUF_MAX  160U
#define AQ_HW_RXBUF_MAX  320U

#define L2_FILTER_ACTION_DISCARD (0x0)
#define L2_FILTER_ACTION_HOST    (0x1)

#define AQ_HW_UCP_0X370_REG  (0x370)
#define AQ_HW_CHIP_MIPS         0x00000001U
#define AQ_HW_CHIP_TPO2         0x00000002U
#define AQ_HW_CHIP_RPF2         0x00000004U
#define AQ_HW_CHIP_MPI_AQ       0x00000010U
#define AQ_HW_CHIP_REVISION_A0  0x01000000U
#define AQ_HW_CHIP_REVISION_B0  0x02000000U
#define AQ_HW_CHIP_REVISION_B1  0x04000000U
#define IS_CHIP_FEATURE(HW, _F_) (AQ_HW_CHIP_##_F_ & \
    (HW)->chip_features)

#define AQ_HW_FW_VER_EXPECTED 0x01050006U

#define	AQ_RX_RSS_TYPE_NONE		0x0
#define	AQ_RX_RSS_TYPE_IPV4		0x2
#define	AQ_RX_RSS_TYPE_IPV6		0x3
#define	AQ_RX_RSS_TYPE_IPV4_TCP	0x4
#define	AQ_RX_RSS_TYPE_IPV6_TCP	0x5
#define	AQ_RX_RSS_TYPE_IPV4_UDP	0x6
#define	AQ_RX_RSS_TYPE_IPV6_UDP	0x7

enum hw_atl_rx_action_with_traffic {
	HW_ATL_RX_DISCARD,
	HW_ATL_RX_HOST,
	HW_ATL_RX_MNGMNT,
	HW_ATL_RX_HOST_AND_MNGMNT,
	HW_ATL_RX_WOL
};

struct aq_rx_filter_vlan {
	u8 enable;
	u8 location;
	u16 vlan_id;
	u8 queue;
};

#define AQ_HW_VLAN_MAX_FILTERS         16U
#define AQ_HW_ETYPE_MAX_FILTERS        16U

struct aq_rx_filter_l2 {
	u8 enable;
	s8 queue;
	u8 location;
	u8 user_priority_en;
	u8 user_priority;
	u16 ethertype;
};

enum hw_atl_rx_ctrl_registers_l2 {
	HW_ATL_RX_ENABLE_UNICAST_MNGNT_QUEUE_L2 = BIT(19),
	HW_ATL_RX_ENABLE_UNICAST_FLTR_L2        = BIT(31)
};

struct aq_rx_filter_l3l4 {
	u32 cmd;
	u8 location;
	u32 ip_dst[4];
	u32 ip_src[4];
	u16 p_dst;
	u16 p_src;
	bool is_ipv6;
};

enum hw_atl_rx_protocol_value_l3l4 {
	HW_ATL_RX_TCP,
	HW_ATL_RX_UDP,
	HW_ATL_RX_SCTP,
	HW_ATL_RX_ICMP
};

enum hw_atl_rx_ctrl_registers_l3l4 {
	HW_ATL_RX_ENABLE_MNGMNT_QUEUE_L3L4 = BIT(22),
	HW_ATL_RX_ENABLE_QUEUE_L3L4        = BIT(23),
	HW_ATL_RX_ENABLE_ARP_FLTR_L3       = BIT(24),
	HW_ATL_RX_ENABLE_CMP_PROT_L4       = BIT(25),
	HW_ATL_RX_ENABLE_CMP_DEST_PORT_L4  = BIT(26),
	HW_ATL_RX_ENABLE_CMP_SRC_PORT_L4   = BIT(27),
	HW_ATL_RX_ENABLE_CMP_DEST_ADDR_L3  = BIT(28),
	HW_ATL_RX_ENABLE_CMP_SRC_ADDR_L3   = BIT(29),
	HW_ATL_RX_ENABLE_L3_IPv6           = BIT(30),
	HW_ATL_RX_ENABLE_FLTR_L3L4         = BIT(31)
};

#define HW_ATL_RX_BOFFSET_PROT_FL3L4      0U
#define HW_ATL_RX_BOFFSET_QUEUE_FL3L4     8U
#define HW_ATL_RX_BOFFSET_ACTION_FL3F4    16U

#define HW_ATL_RX_CNT_REG_ADDR_IPV6       4U

#define HW_ATL_GET_REG_LOCATION_FL3L4(location) \
	((location) - AQ_RX_FIRST_LOC_FL3L4)

enum aq_hw_fw_mpi_state_e {
    MPI_DEINIT = 0,
    MPI_RESET = 1,
    MPI_INIT = 2,
    MPI_POWER = 4,
};

int aq_hw_get_mac_permanent(struct aq_hw *hw, u8 *mac);

int aq_hw_mac_addr_set(struct aq_hw *hw, u8 *mac_addr, u8 index);

/* link speed in mbps. "0" - no link detected */
int aq_hw_get_link_state(struct aq_hw *hw, u32 *link_speed, struct aq_hw_fc_info *fc_neg);

int aq_hw_set_link_speed(struct aq_hw *hw, u32 speed);

int aq_hw_fw_downld_dwords(struct aq_hw *hw, u32 a, u32 *p, u32 cnt);

int aq_hw_reset(struct aq_hw *hw);

int aq_hw_mpi_create(struct aq_hw *hw);

int aq_hw_mpi_read_stats(struct aq_hw *hw, struct aq_hw_fw_mbox *pmbox);

int aq_hw_init(struct aq_hw *hw, u8 *mac_addr, u8 adm_irq, bool msix);

int aq_hw_start(struct aq_hw *hw);

int aq_hw_interrupt_moderation_set(struct aq_hw *hw);

int aq_hw_get_fw_version(struct aq_hw *hw, u32 *fw_version);

int aq_hw_deinit(struct aq_hw *hw);

int aq_hw_ver_match(const aq_hw_fw_version* ver_expected, const aq_hw_fw_version* ver_actual);

void aq_hw_set_promisc(struct aq_hw_s *self, bool l2_promisc, bool vlan_promisc, bool mc_promisc);

int aq_hw_set_power(struct aq_hw *hw, unsigned int power_state);

int aq_hw_err_from_flags(struct aq_hw *hw);

int hw_atl_b0_hw_vlan_promisc_set(struct aq_hw_s *self, bool promisc);

int hw_atl_b0_hw_vlan_set(struct aq_hw_s *self,
                  struct aq_rx_filter_vlan *aq_vlans);

int aq_hw_rss_hash_set(struct aq_hw_s *self, u8 rss_key[HW_ATL_RSS_HASHKEY_SIZE]);
int aq_hw_rss_hash_get(struct aq_hw_s *self, u8 rss_key[HW_ATL_RSS_HASHKEY_SIZE]);
int aq_hw_rss_set(struct aq_hw_s *self, u8 rss_table[HW_ATL_RSS_INDIRECTION_TABLE_MAX]);
int aq_hw_udp_rss_enable(struct aq_hw_s *self, bool enable);

#endif //_AQ_HW_H_

