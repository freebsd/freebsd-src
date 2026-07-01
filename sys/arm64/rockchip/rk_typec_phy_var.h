/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw
 */

#ifndef _ARM64_ROCKCHIP_RK_TYPEC_PHY_VAR_H_
#define _ARM64_ROCKCHIP_RK_TYPEC_PHY_VAR_H_

#include <sys/types.h>

/*
 * DisplayPort-altmode entry points implemented by rk_typec_phy(4)
 * and consumed by rk_cdn_dp(4) during USB-C DP altmode bring-up
 * and link training.
 */
int	rk_typec_phy_dp_set_signal_levels_first(int link_rate, int lane_count,
	    uint8_t swing, uint8_t pre_emp);
int	rk_typec_phy_dp_set_link_rate_first(int link_rate, bool ssc_on);
int	rk_typec_phy_dp_set_lane_count_first(int lane_count);
int	rk_typec_phy_dp_refresh_orientation_first(void);

#endif /* _ARM64_ROCKCHIP_RK_TYPEC_PHY_VAR_H_ */
