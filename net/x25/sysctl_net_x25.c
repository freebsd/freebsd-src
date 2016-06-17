/* -*- linux-c -*-
 * sysctl_net_x25.c: sysctl interface to net X.25 subsystem.
 *
 * Begun April 1, 1996, Mike Shaver.
 * Added /proc/sys/net/x25 directory entry (empty =) ). [MS]
 */

#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <net/x25.h>

static int min_timer[] = {1   * HZ};
static int max_timer[] = {300 * HZ};

static struct ctl_table_header *x25_table_header;

static ctl_table x25_table[] = {
        {NET_X25_RESTART_REQUEST_TIMEOUT, "restart_request_timeout",
         &sysctl_x25_restart_request_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_timer, &max_timer},
        {NET_X25_CALL_REQUEST_TIMEOUT, "call_request_timeout",
         &sysctl_x25_call_request_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_timer, &max_timer},
        {NET_X25_RESET_REQUEST_TIMEOUT, "reset_request_timeout",
         &sysctl_x25_reset_request_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_timer, &max_timer},
        {NET_X25_CLEAR_REQUEST_TIMEOUT, "clear_request_timeout",
         &sysctl_x25_clear_request_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_timer, &max_timer},
        {NET_X25_ACK_HOLD_BACK_TIMEOUT, "acknowledgement_hold_back_timeout",
         &sysctl_x25_ack_holdback_timeout, sizeof(int), 0644, NULL,
         &proc_dointvec_minmax, &sysctl_intvec, NULL, &min_timer, &max_timer},
	{0}
};

static ctl_table x25_dir_table[] = {
	{NET_X25, "x25", NULL, 0, 0555, x25_table},
	{0}
};

static ctl_table x25_root_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, x25_dir_table},
	{0}
};

void __init x25_register_sysctl(void)
{
	x25_table_header = register_sysctl_table(x25_root_table, 1);
}

void x25_unregister_sysctl(void)
{
	unregister_sysctl_table(x25_table_header);
}
