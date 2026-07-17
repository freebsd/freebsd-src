/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw
 */

#ifndef _ARM64_ROCKCHIP_RK_CDN_DP_VAR_H_
#define _ARM64_ROCKCHIP_RK_CDN_DP_VAR_H_

#include <sys/types.h>
#include <sys/bus.h>

/*
 * Public entry points exported by rk_cdn_dp(4) for consumers
 * elsewhere in the kernel (rk_drm video pipeline, sysctl-driven
 * recovery helpers).
 */
int	rk_cdn_dp_get_cached_edid(device_t dev, uint8_t *buf, size_t len);
int	rk_cdn_dp_auto_bringup_default(void);
int	rk_cdn_dp_retrain_default(void);
int	rk_cdn_dp_enable_mode(uint32_t clock, uint16_t hdisplay,
	    uint16_t hsync_start, uint16_t hsync_end, uint16_t htotal,
	    uint16_t vdisplay, uint16_t vsync_start, uint16_t vsync_end,
	    uint16_t vtotal, uint32_t flags);
int	rk_cdn_dp_set_video_active_first(bool active);

#endif /* _ARM64_ROCKCHIP_RK_CDN_DP_VAR_H_ */
