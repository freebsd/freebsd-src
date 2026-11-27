/*-
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_IICBUS_USB_FUSB302_VAR_H_
#define _DEV_IICBUS_USB_FUSB302_VAR_H_

#include <sys/bus.h>
#include <sys/types.h>
#include <arm64/rockchip/rk3399_typec_altmode_var.h>

enum fusb302_typec_orientation {
	FUSB302_TYPEC_ORIENT_NONE = 0,
	FUSB302_TYPEC_ORIENT_CC1,
	FUSB302_TYPEC_ORIENT_CC2,
	FUSB302_TYPEC_ORIENT_UNKNOWN,
};

enum fusb302_typec_role {
	FUSB302_TYPEC_ROLE_NONE = 0,
	FUSB302_TYPEC_ROLE_SOURCE,
	FUSB302_TYPEC_ROLE_SINK,
	FUSB302_TYPEC_ROLE_ACCESSORY,
	FUSB302_TYPEC_ROLE_UNKNOWN,
};

struct fusb302_typec_status {
	bool				attached;
	bool				vbusok;
	bool				has_irq;
	bool				state_valid;
	uint8_t				togss_raw;
	enum fusb302_typec_orientation	orientation;
	enum fusb302_typec_role	role;
};

int	fusb302_get_typec_status(device_t dev, struct fusb302_typec_status *status);
int	fusb302_get_dp_altmode_state(device_t dev,
	    struct rk3399_typec_dp_altmode_status *status);

/*
 * usbc_pd policy SM hooks into fusb302's TCPC implementation
 * through this ops vector.  Defined in fusb302.c.
 */
struct usbc_tcpc_ops;
extern const struct usbc_tcpc_ops fusb302_tcpc_ops;

#endif
