/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright(c) 2024 Realtek Corporation
 */

#ifndef __RTW89_8852BT_RFK_H__
#define __RTW89_8852BT_RFK_H__

#include "core.h"

void rtw8852bt_rck(struct rtw89_dev *rtwdev);
void rtw8852bt_dack(struct rtw89_dev *rtwdev, enum rtw89_chanctx_idx chanctx_idx);
void rtw8852bt_iqk(struct rtw89_dev *rtwdev, enum rtw89_phy_idx phy_idx,
		   enum rtw89_chanctx_idx chanctx_idx);
void rtw8852bt_rx_dck(struct rtw89_dev *rtwdev, enum rtw89_phy_idx phy_idx,
		      enum rtw89_chanctx_idx chanctx_idx);
void rtw8852bt_dpk_init(struct rtw89_dev *rtwdev);
void rtw8852bt_dpk(struct rtw89_dev *rtwdev, enum rtw89_phy_idx phy_idx,
		   enum rtw89_chanctx_idx chanctx_idx);
void rtw8852bt_dpk_track(struct rtw89_dev *rtwdev);
void rtw8852bt_tssi(struct rtw89_dev *rtwdev, enum rtw89_phy_idx phy,
		    bool hwtx_en, enum rtw89_chanctx_idx chanctx_idx);
void rtw8852bt_tssi_scan(struct rtw89_dev *rtwdev, enum rtw89_phy_idx phy,
			 const struct rtw89_chan *chan);
void rtw8852bt_wifi_scan_notify(struct rtw89_dev *rtwdev, bool scan_start,
				enum rtw89_phy_idx phy_idx,
				enum rtw89_chanctx_idx chanctx_idx);
void rtw8852bt_set_channel_rf(struct rtw89_dev *rtwdev,
			      const struct rtw89_chan *chan,
			      enum rtw89_phy_idx phy_idx);

#endif
