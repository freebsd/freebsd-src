/*
 * hostapd / VLAN initialization
 * Copyright 2003, Instant802 Networks, Inc.
 * Copyright 2005, Devicescape Software, Inc.
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

#ifndef VLAN_INIT_H
#define VLAN_INIT_H

int vlan_init(struct hostapd_data *hapd);
void vlan_deinit(struct hostapd_data *hapd);
int vlan_reconfig(struct hostapd_data *hapd, struct hostapd_config *oldconf,
		  struct hostapd_bss_config *oldbss);
struct hostapd_vlan * vlan_add_dynamic(struct hostapd_data *hapd,
				       struct hostapd_vlan *vlan,
				       int vlan_id);
int vlan_remove_dynamic(struct hostapd_data *hapd, int vlan_id);
int vlan_setup_encryption_dyn(struct hostapd_data *hapd,
			      struct hostapd_ssid *mssid,
			      const char *dyn_vlan);

#endif /* VLAN_INIT_H */
