/* -*- linux-c -*-
 * sysctl_net_netrom.c: sysctl interface to net NET/ROM subsystem.
 *
 * Begun April 1, 1996, Mike Shaver.
 * Added /proc/sys/net/netrom directory entry (empty =) ). [MS]
 */

#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/init.h>
#include <net/ax25.h>
#include <net/netrom.h>

/*
 *	Values taken from NET/ROM documentation.
 */
static int min_quality[] = {0}, max_quality[] = {255};
static int min_obs[]     = {0}, max_obs[]     = {255};
static int min_ttl[]     = {0}, max_ttl[]     = {255};
static int min_t1[]      = {5 * HZ};
static int max_t1[]      = {600 * HZ};
static int min_n2[]      = {2}, max_n2[]      = {127};
static int min_t2[]      = {1 * HZ};
static int max_t2[]      = {60 * HZ};
static int min_t4[]      = {1 * HZ};
static int max_t4[]      = {1000 * HZ};
static int min_window[]  = {1}, max_window[]  = {127};
static int min_idle[]    = {0 * HZ};
static int max_idle[]    = {65535 * HZ};
static int min_route[]   = {0}, max_route[]   = {1};
static int min_fails[]   = {1}, max_fails[]   = {10};

static struct ctl_table_header *nr_table_header;

static ctl_table nr_table[] = {
        {NET_NETROM_DEFAULT_PATH_QUALITY, "default_path_quality",
         &sysctl_netrom_default_path_quality, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_quality, &max_quality},
        {NET_NETROM_OBSOLESCENCE_COUNT_INITIALISER, "obsolescence_count_initialiser",
         &sysctl_netrom_obsolescence_count_initialiser, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_obs, &max_obs},
        {NET_NETROM_NETWORK_TTL_INITIALISER, "network_ttl_initialiser",
         &sysctl_netrom_network_ttl_initialiser, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_ttl, &max_ttl},
        {NET_NETROM_TRANSPORT_TIMEOUT, "transport_timeout",
         &sysctl_netrom_transport_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_t1, &max_t1},
        {NET_NETROM_TRANSPORT_MAXIMUM_TRIES, "transport_maximum_tries",
         &sysctl_netrom_transport_maximum_tries, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_n2, &max_n2},
        {NET_NETROM_TRANSPORT_ACKNOWLEDGE_DELAY, "transport_acknowledge_delay",
         &sysctl_netrom_transport_acknowledge_delay, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_t2, &max_t2},
        {NET_NETROM_TRANSPORT_BUSY_DELAY, "transport_busy_delay",
         &sysctl_netrom_transport_busy_delay, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_t4, &max_t4},
        {NET_NETROM_TRANSPORT_REQUESTED_WINDOW_SIZE, "transport_requested_window_size",
         &sysctl_netrom_transport_requested_window_size, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_window, &max_window},
        {NET_NETROM_TRANSPORT_NO_ACTIVITY_TIMEOUT, "transport_no_activity_timeout",
         &sysctl_netrom_transport_no_activity_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_idle, &max_idle},
        {NET_NETROM_ROUTING_CONTROL, "routing_control",
         &sysctl_netrom_routing_control, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_route, &max_route},
        {NET_NETROM_LINK_FAILS_COUNT, "link_fails_count",
         &sysctl_netrom_link_fails_count, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_fails, &max_fails},
	{0}
};

static ctl_table nr_dir_table[] = {
	{NET_NETROM, "netrom", NULL, 0, 0555, nr_table},
	{0}
};

static ctl_table nr_root_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, nr_dir_table},
	{0}
};

void __init nr_register_sysctl(void)
{
	nr_table_header = register_sysctl_table(nr_root_table, 1);
}

void nr_unregister_sysctl(void)
{
	unregister_sysctl_table(nr_table_header);
}
