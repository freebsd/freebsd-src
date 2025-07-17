/*
 * Linux VLAN configuration kernel interface
 * Copyright (c) 2016, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef LINUX_VLAN_H
#define LINUX_VLAN_H

/* This ioctl is defined in linux/sockios.h */

#ifndef SIOCSIFVLAN
#define SIOCSIFVLAN 0x8983
#endif /* SIOCSIFVLAN */

/* This interface is defined in linux/if_vlan.h */

#define ADD_VLAN_CMD 0
#define DEL_VLAN_CMD 1
#define SET_VLAN_INGRESS_PRIORITY_CMD 2
#define SET_VLAN_EGRESS_PRIORITY_CMD 3
#define GET_VLAN_INGRESS_PRIORITY_CMD 4
#define GET_VLAN_EGRESS_PRIORITY_CMD 5
#define SET_VLAN_NAME_TYPE_CMD 6
#define SET_VLAN_FLAG_CMD 7
#define GET_VLAN_REALDEV_NAME_CMD 8
#define GET_VLAN_VID_CMD 9

#define VLAN_NAME_TYPE_PLUS_VID 0
#define VLAN_NAME_TYPE_RAW_PLUS_VID 1
#define VLAN_NAME_TYPE_PLUS_VID_NO_PAD 2
#define VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD 3

struct vlan_ioctl_args {
	int cmd;
	char device1[24];

	union {
		char device2[24];
		int VID;
		unsigned int skb_priority;
		unsigned int name_type;
		unsigned int bind_type;
		unsigned int flag;
	} u;

	short vlan_qos;
};

#endif /* LINUX_VLAN_H */
