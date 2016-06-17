/* -*- linux-c -*-
 * sysctl_net.c: sysctl interface to net subsystem.
 *
 * Begun April 1, 1996, Mike Shaver.
 * Added /proc/sys/net directories for each protocol family. [MS]
 *
 * $Log: sysctl_net.c,v $
 * Revision 1.2  1996/05/08  20:24:40  shaver
 * Added bits for NET_BRIDGE and the NET_IPV4_ARP stuff and
 * NET_IPV4_IP_FORWARD.
 *
 *
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/sysctl.h>

#ifdef CONFIG_INET
extern ctl_table ipv4_table[];
#endif

extern ctl_table core_table[];

#ifdef CONFIG_NET
extern ctl_table ether_table[], e802_table[];
#endif

#ifdef CONFIG_IPV6
extern ctl_table ipv6_table[];
#endif

#ifdef CONFIG_TR
extern ctl_table tr_table[];
#endif

ctl_table net_table[] = {
	{NET_CORE,   "core",      NULL, 0, 0555, core_table},      
#ifdef CONFIG_NET
	{NET_802,    "802",       NULL, 0, 0555, e802_table},
	{NET_ETHER,  "ethernet",  NULL, 0, 0555, ether_table},
#endif
#ifdef CONFIG_INET
	{NET_IPV4,   "ipv4",      NULL, 0, 0555, ipv4_table},
#endif
#ifdef CONFIG_IPV6
	{NET_IPV6, "ipv6", NULL, 0, 0555, ipv6_table},
#endif
#ifdef CONFIG_TR
	{NET_TR, "token-ring", NULL, 0, 0555, tr_table},
#endif
	{0}
};
