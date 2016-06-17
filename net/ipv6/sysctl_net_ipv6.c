/*
 * sysctl_net_ipv6.c: sysctl interface to net IPV6 subsystem.
 *
 * Changes:
 * YOSHIFUJI Hideaki @USAGI:	added icmp sysctl table.
 */

#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/config.h>
#include <linux/in6.h>
#include <linux/ipv6.h>
#include <net/ndisc.h>
#include <net/ipv6.h>
#include <net/addrconf.h>

extern ctl_table ipv6_route_table[];
extern ctl_table ipv6_icmp_table[];

#ifdef CONFIG_SYSCTL

ctl_table ipv6_table[] = {
	{NET_IPV6_ROUTE, "route", NULL, 0, 0555, ipv6_route_table},
	{NET_IPV6_ICMP, "icmp", NULL, 0, 0500, ipv6_icmp_table},
	{NET_IPV6_BINDV6ONLY, "bindv6only",
	 &sysctl_ipv6_bindv6only, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV6_MLD_MAX_MSF, "mld_max_msf",
	 &sysctl_mld_max_msf, sizeof(int), 0644, NULL, &proc_dointvec},
	{0}
};

#ifdef MODULE
static struct ctl_table_header *ipv6_sysctl_header;
static struct ctl_table ipv6_root_table[];
static struct ctl_table ipv6_net_table[];


ctl_table ipv6_root_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, ipv6_net_table},
        {0}
};

ctl_table ipv6_net_table[] = {
	{NET_IPV6, "ipv6", NULL, 0, 0555, ipv6_table},
        {0}
};

void ipv6_sysctl_register(void)
{
	ipv6_sysctl_header = register_sysctl_table(ipv6_root_table, 0);
}

void ipv6_sysctl_unregister(void)
{
	unregister_sysctl_table(ipv6_sysctl_header);
}
#endif	/* MODULE */

#endif /* CONFIG_SYSCTL */



