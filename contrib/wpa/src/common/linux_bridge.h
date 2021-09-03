/*
 * Linux bridge configuration kernel interface
 * Copyright (c) 2016, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef LINUX_BRIDGE_H
#define LINUX_BRIDGE_H

/* This ioctl is defined in linux/sockios.h */

#ifndef SIOCBRADDBR
#define SIOCBRADDBR 0x89a0
#endif
#ifndef SIOCBRDELBR
#define SIOCBRDELBR 0x89a1
#endif
#ifndef SIOCBRADDIF
#define SIOCBRADDIF 0x89a2
#endif
#ifndef SIOCBRDELIF
#define SIOCBRDELIF 0x89a3
#endif

/* This interface is defined in linux/if_bridge.h */

#define BRCTL_GET_VERSION 0
#define BRCTL_GET_BRIDGES 1
#define BRCTL_ADD_BRIDGE 2
#define BRCTL_DEL_BRIDGE 3
#define BRCTL_ADD_IF 4
#define BRCTL_DEL_IF 5
#define BRCTL_GET_BRIDGE_INFO 6
#define BRCTL_GET_PORT_LIST 7
#define BRCTL_SET_BRIDGE_FORWARD_DELAY 8

#endif /* LINUX_BRIDGE_H */
