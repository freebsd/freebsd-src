/*
 * ng_sppp.h Netgraph to Sppp module.
 */

/*-
 * Copyright (C) 2002-2004 Cronyx Engineering.
 * Copyright (C) 2002-2004 Roman Kurakin <rik@cronyx.ru>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations a permission to use,
 * modify and redistribute this software in source and binary forms,
 * as long as this message is kept with the software, all derivative
 * works or modified versions.
 *
 * $FreeBSD$
 * Cronyx Id: ng_sppp.h,v 1.1.2.6 2004/03/01 15:17:21 rik Exp $
 */

#ifndef _NETGRAPH_SPPP_H_
#define _NETGRAPH_SPPP_H_

/* Node type name and magic cookie */
#define NG_SPPP_NODE_TYPE		"sppp"
#define NGM_SPPP_COOKIE			1040804655

/* Interface base name */
#define NG_SPPP_IFACE_NAME		"sppp"
#define NG_SPPP_IFACE_NAME_MAX		15

/* My hook names */
#define NG_SPPP_HOOK_DOWNSTREAM		"downstream"

/* Netgraph commands */
enum {
	NGM_SPPP_GET_IFNAME = 1,	/* returns struct ng_sppp_ifname */
};

struct ng_sppp_ifname {
	char    ngif_name[NG_SPPP_IFACE_NAME_MAX + 1];
};

#endif /* _NETGRAPH_SPPP_H_ */
