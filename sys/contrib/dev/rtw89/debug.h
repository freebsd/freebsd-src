/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright(c) 2019-2020  Realtek Corporation
 */

#ifndef __RTW89_DEBUG_H__
#define __RTW89_DEBUG_H__

#include "core.h"

#if defined(__FreeBSD__)
#include <linux/printk.h>
#ifndef	DUMP_PREFIX_OFFSET
#define	DUMP_PREFIX_OFFSET	0
#endif
#endif

enum rtw89_debug_mask {
	RTW89_DBG_TXRX = BIT(0),
	RTW89_DBG_RFK = BIT(1),
	RTW89_DBG_RFK_TRACK = BIT(2),
	RTW89_DBG_CFO = BIT(3),
	RTW89_DBG_TSSI = BIT(4),
	RTW89_DBG_TXPWR = BIT(5),
	RTW89_DBG_HCI = BIT(6),
	RTW89_DBG_RA = BIT(7),
	RTW89_DBG_REGD = BIT(8),
	RTW89_DBG_PHY_TRACK = BIT(9),
	RTW89_DBG_DIG = BIT(10),
	RTW89_DBG_SER = BIT(11),
	RTW89_DBG_FW = BIT(12),
	RTW89_DBG_BTC = BIT(13),
	RTW89_DBG_BF = BIT(14),
	RTW89_DBG_HW_SCAN = BIT(15),
	RTW89_DBG_SAR = BIT(16),
	RTW89_DBG_STATE = BIT(17),
	RTW89_DBG_WOW = BIT(18),
	RTW89_DBG_UL_TB = BIT(19),
	RTW89_DBG_CHAN = BIT(20),
	RTW89_DBG_ACPI = BIT(21),
	RTW89_DBG_EDCCA = BIT(22),

#if defined(__FreeBSD__)
	RTW89_DBG_IO_RW = BIT(30),
#endif
	RTW89_DBG_UNEXP = BIT(31),
};

enum rtw89_debug_mac_reg_sel {
	RTW89_DBG_SEL_MAC_00,
	RTW89_DBG_SEL_MAC_30,
	RTW89_DBG_SEL_MAC_40,
	RTW89_DBG_SEL_MAC_80,
	RTW89_DBG_SEL_MAC_C0,
	RTW89_DBG_SEL_MAC_E0,
	RTW89_DBG_SEL_BB,
	RTW89_DBG_SEL_IQK,
	RTW89_DBG_SEL_RFC,
};

#ifdef CONFIG_RTW89_DEBUGFS
void rtw89_debugfs_init(struct rtw89_dev *rtwdev);
void rtw89_debugfs_deinit(struct rtw89_dev *rtwdev);
#else
static inline void rtw89_debugfs_init(struct rtw89_dev *rtwdev) {}
static inline void rtw89_debugfs_deinit(struct rtw89_dev *rtwdev) {}
#endif

#define rtw89_info(rtwdev, a...) dev_info((rtwdev)->dev, ##a)
#define rtw89_warn(rtwdev, a...) dev_warn((rtwdev)->dev, ##a)
#define rtw89_err(rtwdev, a...) dev_err((rtwdev)->dev, ##a)

#ifdef CONFIG_RTW89_DEBUGMSG
extern unsigned int rtw89_debug_mask;

__printf(3, 4)
void rtw89_debug(struct rtw89_dev *rtwdev, enum rtw89_debug_mask mask,
		 const char *fmt, ...);
static inline void rtw89_hex_dump(struct rtw89_dev *rtwdev,
				  enum rtw89_debug_mask mask,
				  const char *prefix_str,
				  const void *buf, size_t len)
{
	if (!(rtw89_debug_mask & mask))
		return;

	print_hex_dump_bytes(prefix_str, DUMP_PREFIX_OFFSET, buf, len);
}

static inline bool rtw89_debug_is_enabled(struct rtw89_dev *rtwdev,
					  enum rtw89_debug_mask mask)
{
	return !!(rtw89_debug_mask & mask);
}
#else
static inline void rtw89_debug(struct rtw89_dev *rtwdev,
			       enum rtw89_debug_mask mask,
			       const char *fmt, ...) {}
static inline void rtw89_hex_dump(struct rtw89_dev *rtwdev,
				  enum rtw89_debug_mask mask,
				  const char *prefix_str,
				  const void *buf, size_t len) {}
static inline bool rtw89_debug_is_enabled(struct rtw89_dev *rtwdev,
					  enum rtw89_debug_mask mask)
{
	return false;
}
#endif

#endif
