/*-
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _ARM64_ROCKCHIP_RK3399_TYPEC_ALTMODE_VAR_H_
#define _ARM64_ROCKCHIP_RK3399_TYPEC_ALTMODE_VAR_H_

#include <sys/bus.h>
#include <sys/types.h>

struct rk3399_typec_dp_altmode_status {
	bool		valid;
	bool		dp_ready;
	int		usb_ss;		/* -1 unknown, 0 DP-only, 1 USB3+DP */
	uint32_t	pin_assignment;
	uint32_t	dp_status;
};

int	rk3399_typec_dp_altmode_get_state(device_t dev,
	    struct rk3399_typec_dp_altmode_status *status);

#endif
