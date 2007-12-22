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

#ifndef IEEE802_11H_H
#define IEEE802_11H_H

#define SPECT_LOOSE_BINDING	1
#define SPECT_STRICT_BINDING	2

#define CHAN_SWITCH_MODE_NOISY	0
#define CHAN_SWITCH_MODE_QUIET	1

int hostapd_check_power_cap(struct hostapd_data *hapd, u8 *power, u8 len);

#endif /* IEEE802_11H_H */
