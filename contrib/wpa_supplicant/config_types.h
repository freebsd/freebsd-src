/*
 * hostapd / Shared configuration file defines
 * Copyright (c) 2003-2005, Jouni Malinen <j@w1.fi>
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

#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

struct hostapd_ip_addr {
	union {
		struct in_addr v4;
#ifdef CONFIG_IPV6
		struct in6_addr v6;
#endif /* CONFIG_IPV6 */
	} u;
	int af; /* AF_INET / AF_INET6 */
};

#endif /* CONFIG_TYPES_H */
