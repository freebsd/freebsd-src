/*
 * hostapd / VLAN netlink api
 * Copyright (c) 2012, Michael Braun <michael-dev@fami-braun.de>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef VLAN_UTIL_H
#define VLAN_UTIL_H

int vlan_add(const char *if_name, int vid, const char *vlan_if_name);
int vlan_rem(const char *if_name);

#endif /* VLAN_UTIL_H */
