/*
 * Hotspot 2.0 AP ANQP processing
 * Copyright (c) 2011-2012, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef HS20_H
#define HS20_H

struct hostapd_data;

u8 * hostapd_eid_hs20_indication(struct hostapd_data *hapd, u8 *eid);

#endif /* HS20_H */
