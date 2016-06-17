/* -*- linux-c -*-
 * sysctl_net_rose.c: sysctl interface to net ROSE subsystem.
 *
 * Begun April 1, 1996, Mike Shaver.
 * Added /proc/sys/net/rose directory entry (empty =) ). [MS]
 */

#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/init.h>
#include <net/ax25.h>
#include <net/rose.h>

static int min_timer[]  = {1 * HZ};
static int max_timer[]  = {300 * HZ};
static int min_idle[]   = {0 * HZ};
static int max_idle[]   = {65535 * HZ};
static int min_route[]  = {0}, max_route[] = {1};
static int min_ftimer[] = {60 * HZ};
static int max_ftimer[] = {600 * HZ};
static int min_maxvcs[] = {1}, max_maxvcs[] = {254};
static int min_window[] = {1}, max_window[] = {7};

static struct ctl_table_header *rose_table_header;

static ctl_table rose_table[] = {
        {NET_ROSE_RESTART_REQUEST_TIMEOUT, "restart_request_timeout",
         &sysctl_rose_restart_request_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_timer, &max_timer},
        {NET_ROSE_CALL_REQUEST_TIMEOUT, "call_request_timeout",
         &sysctl_rose_call_request_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_timer, &max_timer},
        {NET_ROSE_RESET_REQUEST_TIMEOUT, "reset_request_timeout",
         &sysctl_rose_reset_request_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_timer, &max_timer},
        {NET_ROSE_CLEAR_REQUEST_TIMEOUT, "clear_request_timeout",
         &sysctl_rose_clear_request_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_timer, &max_timer},
        {NET_ROSE_NO_ACTIVITY_TIMEOUT, "no_activity_timeout",
         &sysctl_rose_no_activity_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_idle, &max_idle},
        {NET_ROSE_ACK_HOLD_BACK_TIMEOUT, "acknowledge_hold_back_timeout",
         &sysctl_rose_ack_hold_back_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_timer, &max_timer},
        {NET_ROSE_ROUTING_CONTROL, "routing_control",
         &sysctl_rose_routing_control, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_route, &max_route},
        {NET_ROSE_LINK_FAIL_TIMEOUT, "link_fail_timeout",
         &sysctl_rose_link_fail_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_ftimer, &max_ftimer},
        {NET_ROSE_MAX_VCS, "maximum_virtual_circuits",
         &sysctl_rose_maximum_vcs, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_maxvcs, &max_maxvcs},
        {NET_ROSE_WINDOW_SIZE, "window_size",
         &sysctl_rose_window_size, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_window, &max_window},
	{0}
};

static ctl_table rose_dir_table[] = {
	{NET_ROSE, "rose", NULL, 0, 0555, rose_table},
	{0}
};

static ctl_table rose_root_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, rose_dir_table},
	{0}
};

void __init rose_register_sysctl(void)
{
	rose_table_header = register_sysctl_table(rose_root_table, 1);
}

void rose_unregister_sysctl(void)
{
	unregister_sysctl_table(rose_table_header);
}
