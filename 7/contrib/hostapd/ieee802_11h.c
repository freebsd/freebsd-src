/*
 * hostapd / IEEE 802.11h
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"

#include "hostapd.h"


int hostapd_check_power_cap(struct hostapd_data *hapd, u8 *power, u8 len)
{
	unsigned int max_pwr;

	if (len < 2) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "Too short power capability IE\n");
		return -1;
	}
	max_pwr = power[1];
	if (max_pwr > hapd->iface->sta_max_power)
		return -1;
	return 0;
}
