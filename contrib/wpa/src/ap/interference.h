/*
 * Management of interference due to incumbent signals in 6 GHz band
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */
#ifndef INTERFERENCE_H
#define INTERFERENCE_H

struct hostapd_iface;

int hostapd_incumbt_sig_intf_detected(struct hostapd_iface *iface, int freq,
				      int chan_width,
				      int cf1, int cf2,
				      u32 chan_bw_interference_bitmap);

#endif /* INTERFERENCE_H */
