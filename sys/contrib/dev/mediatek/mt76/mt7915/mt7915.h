/* SPDX-License-Identifier: ISC */
/* Copyright (C) 2020 MediaTek Inc. */

#ifndef __MT7915_H
#define __MT7915_H

#include <linux/interrupt.h>
#include <linux/ktime.h>
#if defined(__FreeBSD__)
#include <linux/uuid.h>
#endif
#include "../mt76_connac.h"
#include "regs.h"

#define MT7915_MAX_INTERFACES		19
#define MT7915_WTBL_SIZE		288
#define MT7916_WTBL_SIZE		544
#define MT7915_WTBL_RESERVED		(mt7915_wtbl_size(dev) - 1)
#define MT7915_WTBL_STA			(MT7915_WTBL_RESERVED - \
					 MT7915_MAX_INTERFACES)

#define MT7915_WATCHDOG_TIME		(HZ / 10)
#define MT7915_RESET_TIMEOUT		(30 * HZ)

#define MT7915_TX_RING_SIZE		2048
#define MT7915_TX_MCU_RING_SIZE		256
#define MT7915_TX_FWDL_RING_SIZE	128

#define MT7915_RX_RING_SIZE		1536
#define MT7915_RX_MCU_RING_SIZE		512

#define MT7915_FIRMWARE_WA		"mediatek/mt7915_wa.bin"
#define MT7915_FIRMWARE_WM		"mediatek/mt7915_wm.bin"
#define MT7915_ROM_PATCH		"mediatek/mt7915_rom_patch.bin"

#define MT7916_FIRMWARE_WA		"mediatek/mt7916_wa.bin"
#define MT7916_FIRMWARE_WM		"mediatek/mt7916_wm.bin"
#define MT7916_ROM_PATCH		"mediatek/mt7916_rom_patch.bin"

#define MT7981_FIRMWARE_WA		"mediatek/mt7981_wa.bin"
#define MT7981_FIRMWARE_WM		"mediatek/mt7981_wm.bin"
#define MT7981_ROM_PATCH		"mediatek/mt7981_rom_patch.bin"

#define MT7986_FIRMWARE_WA		"mediatek/mt7986_wa.bin"
#define MT7986_FIRMWARE_WM		"mediatek/mt7986_wm.bin"
#define MT7986_FIRMWARE_WM_MT7975	"mediatek/mt7986_wm_mt7975.bin"
#define MT7986_ROM_PATCH		"mediatek/mt7986_rom_patch.bin"
#define MT7986_ROM_PATCH_MT7975		"mediatek/mt7986_rom_patch_mt7975.bin"

#define MT7915_EEPROM_DEFAULT		"mediatek/mt7915_eeprom.bin"
#define MT7915_EEPROM_DEFAULT_DBDC	"mediatek/mt7915_eeprom_dbdc.bin"
#define MT7916_EEPROM_DEFAULT		"mediatek/mt7916_eeprom.bin"

#define MT7981_EEPROM_MT7976_DEFAULT_DBDC	"mediatek/mt7981_eeprom_mt7976_dbdc.bin"

#define MT7986_EEPROM_MT7975_DEFAULT		"mediatek/mt7986_eeprom_mt7975.bin"
#define MT7986_EEPROM_MT7975_DUAL_DEFAULT	"mediatek/mt7986_eeprom_mt7975_dual.bin"
#define MT7986_EEPROM_MT7976_DEFAULT		"mediatek/mt7986_eeprom_mt7976.bin"
#define MT7986_EEPROM_MT7976_DEFAULT_DBDC	"mediatek/mt7986_eeprom_mt7976_dbdc.bin"
#define MT7986_EEPROM_MT7976_DUAL_DEFAULT	"mediatek/mt7986_eeprom_mt7976_dual.bin"

#define MT7915_EEPROM_SIZE		3584
#define MT7916_EEPROM_SIZE		4096

#define MT7915_EEPROM_BLOCK_SIZE	16
#define MT7915_HW_TOKEN_SIZE		4096
#define MT7915_TOKEN_SIZE		8192

#define MT7915_CFEND_RATE_DEFAULT	0x49	/* OFDM 24M */
#define MT7915_CFEND_RATE_11B		0x03	/* 11B LP, 11M */

#define MT7915_THERMAL_THROTTLE_MAX	100
#define MT7915_CDEV_THROTTLE_MAX	99

#define MT7915_SKU_RATE_NUM		161

#define MT7915_MAX_TWT_AGRT		16
#define MT7915_MAX_STA_TWT_AGRT		8
#define MT7915_MIN_TWT_DUR 64
#define MT7915_MAX_QUEUE		(MT_RXQ_BAND2 + __MT_MCUQ_MAX + 2)

#define MT7915_WED_RX_TOKEN_SIZE	12288

#define MT7915_CRIT_TEMP_IDX		0
#define MT7915_MAX_TEMP_IDX		1
#define MT7915_CRIT_TEMP		110
#define MT7915_MAX_TEMP			120

struct mt7915_vif;
struct mt7915_sta;
struct mt7915_dfs_pulse;
struct mt7915_dfs_pattern;

enum mt7915_txq_id {
	MT7915_TXQ_FWDL = 16,
	MT7915_TXQ_MCU_WM,
	MT7915_TXQ_BAND0,
	MT7915_TXQ_BAND1,
	MT7915_TXQ_MCU_WA,
};

enum mt7915_rxq_id {
	MT7915_RXQ_BAND0 = 0,
	MT7915_RXQ_BAND1,
	MT7915_RXQ_MCU_WM = 0,
	MT7915_RXQ_MCU_WA,
	MT7915_RXQ_MCU_WA_EXT,
};

enum mt7916_rxq_id {
	MT7916_RXQ_MCU_WM = 0,
	MT7916_RXQ_MCU_WA,
	MT7916_RXQ_MCU_WA_MAIN,
	MT7916_RXQ_MCU_WA_EXT,
	MT7916_RXQ_BAND0,
	MT7916_RXQ_BAND1,
};

struct mt7915_twt_flow {
	struct list_head list;
	u64 start_tsf;
	u64 tsf;
	u32 duration;
	u16 wcid;
	__le16 mantissa;
	u8 exp;
	u8 table_id;
	u8 id;
	u8 protection:1;
	u8 flowtype:1;
	u8 trigger:1;
	u8 sched:1;
};

DECLARE_EWMA(avg_signal, 10, 8)

struct mt7915_sta {
	struct mt76_wcid wcid; /* must be first */

	struct mt7915_vif *vif;

	struct list_head rc_list;
	u32 airtime_ac[8];

	int ack_signal;
	struct ewma_avg_signal avg_ack_signal;

	unsigned long changed;
	unsigned long jiffies;
	struct mt76_connac_sta_key_conf bip;

	struct {
		u8 flowid_mask;
		struct mt7915_twt_flow flow[MT7915_MAX_STA_TWT_AGRT];
	} twt;
};

struct mt7915_vif_cap {
	bool ht_ldpc:1;
	bool vht_ldpc:1;
	bool he_ldpc:1;
	bool vht_su_ebfer:1;
	bool vht_su_ebfee:1;
	bool vht_mu_ebfer:1;
	bool vht_mu_ebfee:1;
	bool he_su_ebfer:1;
	bool he_su_ebfee:1;
	bool he_mu_ebfer:1;
};

struct mt7915_vif {
	struct mt76_vif_link mt76; /* must be first */

	struct mt7915_vif_cap cap;
	struct mt7915_sta sta;
	struct mt7915_phy *phy;

	struct ieee80211_tx_queue_params queue_params[IEEE80211_NUM_ACS];
	struct cfg80211_bitrate_mask bitrate_mask;
};

/* crash-dump */
struct mt7915_crash_data {
	guid_t guid;
	struct timespec64 timestamp;

	u8 *memdump_buf;
	size_t memdump_buf_len;
};

struct mt7915_hif {
	struct list_head list;

	struct device *dev;
	void __iomem *regs;
	int irq;
	u32 index;
};

struct mt7915_phy {
	struct mt76_phy *mt76;
	struct mt7915_dev *dev;

	struct ieee80211_sband_iftype_data iftype[NUM_NL80211_BANDS][NUM_NL80211_IFTYPES];

	struct ieee80211_vif *monitor_vif;

	struct thermal_cooling_device *cdev;
	u8 cdev_state;
	u8 throttle_state;
	u32 throttle_temp[2]; /* 0: critical high, 1: maximum */

	u32 rxfilter;
	u64 omac_mask;

	u16 noise;

	s16 coverage_class;
	u8 slottime;

	u8 rdd_state;

	u32 trb_ts;

	u32 rx_ampdu_ts;
	u32 ampdu_ref;

	struct mt76_mib_stats mib;
	struct mt76_channel_state state_ts;

#ifdef CONFIG_NL80211_TESTMODE
	struct {
		u32 *reg_backup;

		s32 last_freq_offset;
		u8 last_rcpi[4];
		s8 last_ib_rssi[4];
		s8 last_wb_rssi[4];
		u8 last_snr;

		u8 spe_idx;
	} test;
#endif
};

struct mt7915_dev {
	union { /* must be first */
		struct mt76_dev mt76;
		struct mt76_phy mphy;
	};

	struct mt7915_hif *hif2;
	struct mt7915_reg_desc reg;
	u8 q_id[MT7915_MAX_QUEUE];
	u32 q_int_mask[MT7915_MAX_QUEUE];
	u32 wfdma_mask;

	const struct mt76_bus_ops *bus_ops;
	struct mt7915_phy phy;

	/* monitor rx chain configured channel */
	struct cfg80211_chan_def rdd2_chandef;
	struct mt7915_phy *rdd2_phy;

	u16 chainmask;
	u16 chainshift;
	u32 hif_idx;

	struct work_struct init_work;
	struct work_struct rc_work;
	struct work_struct dump_work;
	struct work_struct reset_work;
	wait_queue_head_t reset_wait;

	struct {
		u32 state;
		u32 wa_reset_count;
		u32 wm_reset_count;
		bool hw_full_reset:1;
		bool hw_init_done:1;
		bool restart:1;
	} recovery;

	/* protects coredump data */
	struct mutex dump_mutex;
#ifdef CONFIG_DEV_COREDUMP
	struct {
		struct mt7915_crash_data *crash_data;
	} coredump;
#endif

	struct list_head sta_rc_list;
	struct list_head twt_list;
	spinlock_t reg_lock;

	u32 hw_pattern;

	bool dbdc_support;
	bool flash_mode;
	bool muru_debug;
	bool ibf;

	u8 monitor_mask;

	struct dentry *debugfs_dir;
	struct rchan *relay_fwlog;

	void *cal;
	u32 cur_prek_offset;
	u8 dpd_chan_num_2g;
	u8 dpd_chan_num_5g;
	u8 dpd_chan_num_6g;

	struct {
		u8 debug_wm;
		u8 debug_wa;
		u8 debug_bin;
	} fw;

	struct {
		u16 table_mask;
		u8 n_agrt;
	} twt;

	struct reset_control *rstc;
	void __iomem *dcm;
	void __iomem *sku;
};

enum {
	WFDMA0 = 0x0,
	WFDMA1,
	WFDMA_EXT,
	__MT_WFDMA_MAX,
};

enum {
	MT_RX_SEL0,
	MT_RX_SEL1,
	MT_RX_SEL2, /* monitor chain */
};

enum mt7915_rdd_cmd {
	RDD_STOP,
	RDD_START,
	RDD_DET_MODE,
	RDD_RADAR_EMULATE,
	RDD_START_TXQ = 20,
	RDD_SET_WF_ANT = 30,
	RDD_CAC_START = 50,
	RDD_CAC_END,
	RDD_NORMAL_START,
	RDD_DISABLE_DFS_CAL,
	RDD_PULSE_DBG,
	RDD_READ_PULSE,
	RDD_RESUME_BF,
	RDD_IRQ_OFF,
};

static inline struct mt7915_phy *
mt7915_hw_phy(struct ieee80211_hw *hw)
{
	struct mt76_phy *phy = hw->priv;

	return phy->priv;
}

static inline struct mt7915_dev *
mt7915_hw_dev(struct ieee80211_hw *hw)
{
	struct mt76_phy *phy = hw->priv;

	return container_of(phy->dev, struct mt7915_dev, mt76);
}

static inline struct mt7915_phy *
mt7915_ext_phy(struct mt7915_dev *dev)
{
	struct mt76_phy *phy = dev->mt76.phys[MT_BAND1];

	if (!phy)
		return NULL;

	return phy->priv;
}

static inline u32 mt7915_check_adie(struct mt7915_dev *dev, bool sku)
{
	u32 mask = sku ? MT_CONNINFRA_SKU_MASK : MT_ADIE_TYPE_MASK;
	if (!is_mt798x(&dev->mt76))
		return 0;

	return mt76_rr(dev, MT_CONNINFRA_SKU_DEC_ADDR) & mask;
}

extern const struct ieee80211_ops mt7915_ops;
extern const struct mt76_testmode_ops mt7915_testmode_ops;
extern struct pci_driver mt7915_pci_driver;
extern struct pci_driver mt7915_hif_driver;
extern struct platform_driver mt798x_wmac_driver;

#ifdef CONFIG_MT798X_WMAC
int mt7986_wmac_enable(struct mt7915_dev *dev);
void mt7986_wmac_disable(struct mt7915_dev *dev);
#else
static inline int mt7986_wmac_enable(struct mt7915_dev *dev)
{
	return 0;
}

static inline void mt7986_wmac_disable(struct mt7915_dev *dev)
{
}
#endif
struct mt7915_dev *mt7915_mmio_probe(struct device *pdev,
				     void __iomem *mem_base, u32 device_id);
void mt7915_wfsys_reset(struct mt7915_dev *dev);
irqreturn_t mt7915_irq_handler(int irq, void *dev_instance);
u64 __mt7915_get_tsf(struct ieee80211_hw *hw, struct mt7915_vif *mvif);
u32 mt7915_wed_init_buf(void *ptr, dma_addr_t phys, int token_id);

int mt7915_register_device(struct mt7915_dev *dev);
void mt7915_unregister_device(struct mt7915_dev *dev);
int mt7915_eeprom_init(struct mt7915_dev *dev);
void mt7915_eeprom_parse_hw_cap(struct mt7915_dev *dev,
				struct mt7915_phy *phy);
int mt7915_eeprom_get_target_power(struct mt7915_dev *dev,
				   struct ieee80211_channel *chan,
				   u8 chain_idx);
s8 mt7915_eeprom_get_power_delta(struct mt7915_dev *dev, int band);
int mt7915_dma_init(struct mt7915_dev *dev, struct mt7915_phy *phy2);
void mt7915_dma_prefetch(struct mt7915_dev *dev);
void mt7915_dma_cleanup(struct mt7915_dev *dev);
int mt7915_dma_reset(struct mt7915_dev *dev, bool force);
int mt7915_dma_start(struct mt7915_dev *dev, bool reset, bool wed_reset);
int mt7915_txbf_init(struct mt7915_dev *dev);
void mt7915_init_txpower(struct mt7915_phy *phy);
void mt7915_reset(struct mt7915_dev *dev);
int mt7915_run(struct ieee80211_hw *hw);
int mt7915_mcu_init(struct mt7915_dev *dev);
int mt7915_mcu_init_firmware(struct mt7915_dev *dev);
int mt7915_mcu_twt_agrt_update(struct mt7915_dev *dev,
			       struct mt7915_vif *mvif,
			       struct mt7915_twt_flow *flow,
			       int cmd);
int mt7915_mcu_add_dev_info(struct mt7915_phy *phy,
			    struct ieee80211_vif *vif, bool enable);
int mt7915_mcu_add_bss_info(struct mt7915_phy *phy,
			    struct ieee80211_vif *vif, int enable);
int mt7915_mcu_add_sta(struct mt7915_dev *dev, struct ieee80211_vif *vif,
		       struct ieee80211_sta *sta, int conn_state, bool newly);
int mt7915_mcu_add_tx_ba(struct mt7915_dev *dev,
			 struct ieee80211_ampdu_params *params,
			 bool add);
int mt7915_mcu_add_rx_ba(struct mt7915_dev *dev,
			 struct ieee80211_ampdu_params *params,
			 bool add);
int mt7915_mcu_update_bss_color(struct mt7915_dev *dev, struct ieee80211_vif *vif,
				struct cfg80211_he_bss_color *he_bss_color);
int mt7915_mcu_add_inband_discov(struct mt7915_dev *dev, struct ieee80211_vif *vif,
				 u32 changed);
int mt7915_mcu_add_beacon(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			  int enable, u32 changed);
int mt7915_mcu_add_obss_spr(struct mt7915_phy *phy, struct ieee80211_vif *vif,
			    struct ieee80211_he_obss_pd *he_obss_pd);
int mt7915_mcu_add_rate_ctrl(struct mt7915_dev *dev, struct ieee80211_vif *vif,
			     struct ieee80211_sta *sta, bool changed);
int mt7915_mcu_add_smps(struct mt7915_dev *dev, struct ieee80211_vif *vif,
			struct ieee80211_sta *sta);
int mt7915_set_channel(struct mt76_phy *mphy);
int mt7915_mcu_set_chan_info(struct mt7915_phy *phy, int cmd);
int mt7915_mcu_set_tx(struct mt7915_dev *dev, struct ieee80211_vif *vif);
int mt7915_mcu_update_edca(struct mt7915_dev *dev, void *req);
int mt7915_mcu_set_fixed_rate_ctrl(struct mt7915_dev *dev,
				   struct ieee80211_vif *vif,
				   struct ieee80211_sta *sta,
				   void *data, u32 field);
int mt7915_mcu_set_eeprom(struct mt7915_dev *dev);
int mt7915_mcu_get_eeprom(struct mt7915_dev *dev, u32 offset);
int mt7915_mcu_get_eeprom_free_block(struct mt7915_dev *dev, u8 *block_num);
int mt7915_mcu_set_mac(struct mt7915_dev *dev, int band, bool enable,
		       bool hdr_trans);
int mt7915_mcu_set_test_param(struct mt7915_dev *dev, u8 param, bool test_mode,
			      u8 en);
int mt7915_mcu_set_ser(struct mt7915_dev *dev, u8 action, u8 set, u8 band);
int mt7915_mcu_set_sku_en(struct mt7915_phy *phy, bool enable);
int mt7915_mcu_set_txpower_sku(struct mt7915_phy *phy);
int mt7915_mcu_get_txpower_sku(struct mt7915_phy *phy, s8 *txpower, int len);
int mt7915_mcu_set_txpower_frame_min(struct mt7915_phy *phy, s8 txpower);
int mt7915_mcu_set_txpower_frame(struct mt7915_phy *phy,
				 struct ieee80211_vif *vif,
				 struct ieee80211_sta *sta, s8 txpower);
int mt7915_mcu_set_txbf(struct mt7915_dev *dev, u8 action);
int mt7915_mcu_set_fcc5_lpn(struct mt7915_dev *dev, int val);
int mt7915_mcu_set_pulse_th(struct mt7915_dev *dev,
			    const struct mt7915_dfs_pulse *pulse);
int mt7915_mcu_set_radar_th(struct mt7915_dev *dev, int index,
			    const struct mt7915_dfs_pattern *pattern);
int mt7915_mcu_set_muru_ctrl(struct mt7915_dev *dev, u32 cmd, u32 val);
int mt7915_mcu_apply_group_cal(struct mt7915_dev *dev);
int mt7915_mcu_apply_tx_dpd(struct mt7915_phy *phy);
int mt7915_mcu_get_chan_mib_info(struct mt7915_phy *phy, bool chan_switch);
int mt7915_mcu_get_temperature(struct mt7915_phy *phy);
int mt7915_mcu_set_thermal_throttling(struct mt7915_phy *phy, u8 state);
int mt7915_mcu_set_thermal_protect(struct mt7915_phy *phy);
int mt7915_mcu_get_rx_rate(struct mt7915_phy *phy, struct ieee80211_vif *vif,
			   struct ieee80211_sta *sta, struct rate_info *rate);
int mt7915_mcu_rdd_background_enable(struct mt7915_phy *phy,
				     struct cfg80211_chan_def *chandef);
int mt7915_mcu_wed_wa_tx_stats(struct mt7915_dev *dev, u16 wcid);
int mt7915_mcu_rf_regval(struct mt7915_dev *dev, u32 regidx, u32 *val, bool set);
int mt7915_mcu_wa_cmd(struct mt7915_dev *dev, int cmd, u32 a1, u32 a2, u32 a3);
int mt7915_mcu_fw_log_2_host(struct mt7915_dev *dev, u8 type, u8 ctrl);
int mt7915_mcu_fw_dbg_ctrl(struct mt7915_dev *dev, u32 module, u8 level);
void mt7915_mcu_rx_event(struct mt7915_dev *dev, struct sk_buff *skb);
void mt7915_mcu_exit(struct mt7915_dev *dev);

static inline u16 mt7915_wtbl_size(struct mt7915_dev *dev)
{
	return is_mt7915(&dev->mt76) ? MT7915_WTBL_SIZE : MT7916_WTBL_SIZE;
}

static inline u16 mt7915_eeprom_size(struct mt7915_dev *dev)
{
	return is_mt7915(&dev->mt76) ? MT7915_EEPROM_SIZE : MT7916_EEPROM_SIZE;
}

void mt7915_dual_hif_set_irq_mask(struct mt7915_dev *dev, bool write_reg,
				  u32 clear, u32 set);

static inline void mt7915_irq_enable(struct mt7915_dev *dev, u32 mask)
{
	if (dev->hif2)
		mt7915_dual_hif_set_irq_mask(dev, false, 0, mask);
	else
		mt76_set_irq_mask(&dev->mt76, 0, 0, mask);

	tasklet_schedule(&dev->mt76.irq_tasklet);
}

static inline void mt7915_irq_disable(struct mt7915_dev *dev, u32 mask)
{
	if (dev->hif2)
		mt7915_dual_hif_set_irq_mask(dev, true, mask, 0);
	else
		mt76_set_irq_mask(&dev->mt76, MT_INT_MASK_CSR, mask, 0);
}

void mt7915_memcpy_fromio(struct mt7915_dev *dev, void *buf, u32 offset,
			  size_t len);

void mt7915_mac_init(struct mt7915_dev *dev);
u32 mt7915_mac_wtbl_lmac_addr(struct mt7915_dev *dev, u16 wcid, u8 dw);
bool mt7915_mac_wtbl_update(struct mt7915_dev *dev, int idx, u32 mask);
void mt7915_mac_reset_counters(struct mt7915_phy *phy);
void mt7915_mac_cca_stats_reset(struct mt7915_phy *phy);
void mt7915_mac_enable_nf(struct mt7915_dev *dev, bool ext_phy);
void mt7915_mac_enable_rtscts(struct mt7915_dev *dev,
			      struct ieee80211_vif *vif, bool enable);
void mt7915_mac_write_txwi(struct mt76_dev *dev, __le32 *txwi,
			   struct sk_buff *skb, struct mt76_wcid *wcid, int pid,
			   struct ieee80211_key_conf *key,
			   enum mt76_txq_id qid, u32 changed);
void mt7915_mac_set_timing(struct mt7915_phy *phy);
int mt7915_mac_sta_add(struct mt76_dev *mdev, struct ieee80211_vif *vif,
		       struct ieee80211_sta *sta);
int mt7915_mac_sta_event(struct mt76_dev *mdev, struct ieee80211_vif *vif,
			 struct ieee80211_sta *sta, enum mt76_sta_event ev);
void mt7915_mac_sta_remove(struct mt76_dev *mdev, struct ieee80211_vif *vif,
			   struct ieee80211_sta *sta);
void mt7915_mac_work(struct work_struct *work);
void mt7915_mac_reset_work(struct work_struct *work);
void mt7915_mac_dump_work(struct work_struct *work);
void mt7915_mac_sta_rc_work(struct work_struct *work);
void mt7915_mac_update_stats(struct mt7915_phy *phy);
void mt7915_mac_twt_teardown_flow(struct mt7915_dev *dev,
				  struct mt7915_sta *msta,
				  u8 flowid);
void mt7915_mac_add_twt_setup(struct ieee80211_hw *hw,
			      struct ieee80211_sta *sta,
			      struct ieee80211_twt_setup *twt);
int mt7915_tx_prepare_skb(struct mt76_dev *mdev, void *txwi_ptr,
			  enum mt76_txq_id qid, struct mt76_wcid *wcid,
			  struct ieee80211_sta *sta,
			  struct mt76_tx_info *tx_info);
void mt7915_queue_rx_skb(struct mt76_dev *mdev, enum mt76_rxq_id q,
			 struct sk_buff *skb, u32 *info);
bool mt7915_rx_check(struct mt76_dev *mdev, void *data, int len);
void mt7915_stats_work(struct work_struct *work);
int mt76_dfs_start_rdd(struct mt7915_dev *dev, bool force);
int mt7915_dfs_init_radar_detector(struct mt7915_phy *phy);
void mt7915_set_stream_he_caps(struct mt7915_phy *phy);
void mt7915_set_stream_vht_txbf_caps(struct mt7915_phy *phy);
void mt7915_update_channel(struct mt76_phy *mphy);
int mt7915_mcu_muru_debug_set(struct mt7915_dev *dev, bool enable);
int mt7915_mcu_muru_debug_get(struct mt7915_phy *phy);
int mt7915_mcu_wed_enable_rx_stats(struct mt7915_dev *dev);
int mt7915_init_debugfs(struct mt7915_phy *phy);
void mt7915_debugfs_rx_fw_monitor(struct mt7915_dev *dev, const void *data, int len);
bool mt7915_debugfs_rx_log(struct mt7915_dev *dev, const void *data, int len);
#ifdef CONFIG_MAC80211_DEBUGFS
void mt7915_sta_add_debugfs(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			    struct ieee80211_sta *sta, struct dentry *dir);
#endif
int mt7915_mmio_wed_init(struct mt7915_dev *dev, void *pdev_ptr,
			 bool pci, int *irq);

#endif
