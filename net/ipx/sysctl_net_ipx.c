/* -*- linux-c -*-
 * sysctl_net_ipx.c: sysctl interface to net IPX subsystem.
 *
 * Begun April 1, 1996, Mike Shaver.
 * Added /proc/sys/net/ipx directory entry (empty =) ). [MS]
 * Added /proc/sys/net/ipx/ipx_pprop_broadcasting - acme March 4, 2001
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/sysctl.h>

#ifndef CONFIG_SYSCTL
#error This file should not be compiled without CONFIG_SYSCTL defined
#endif

/* From af_ipx.c */
extern int sysctl_ipx_pprop_broadcasting;

ctl_table ipx_table[] = {
	{ NET_IPX_PPROP_BROADCASTING, "ipx_pprop_broadcasting",
	  &sysctl_ipx_pprop_broadcasting, sizeof(int), 0644, NULL,
	  &proc_dointvec },
	{ 0 }
};

static ctl_table ipx_dir_table[] = {
	{ NET_IPX, "ipx", NULL, 0, 0555, ipx_table },
	{ 0 }
};

static ctl_table ipx_root_table[] = {
	{ CTL_NET, "net", NULL, 0, 0555, ipx_dir_table },
	{ 0 }
};

static struct ctl_table_header *ipx_table_header;

void ipx_register_sysctl(void)
{
	ipx_table_header = register_sysctl_table(ipx_root_table, 1);
}

void ipx_unregister_sysctl(void)
{
	unregister_sysctl_table(ipx_table_header);
}
