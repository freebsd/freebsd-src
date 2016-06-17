/*
 * sysctl_net_ipv4.c: sysctl interface to net IPV4 subsystem.
 *
 * $Id: sysctl_net_ipv4.c,v 1.50 2001/10/20 00:00:11 davem Exp $
 *
 * Begun April 1, 1996, Mike Shaver.
 * Added /proc/sys/net/ipv4 directory entry (empty =) ). [MS]
 */

#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/config.h>
#include <net/snmp.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/tcp.h>

/* From af_inet.c */
extern int sysctl_ip_nonlocal_bind;

/* From icmp.c */
extern int sysctl_icmp_echo_ignore_all;
extern int sysctl_icmp_echo_ignore_broadcasts;
extern int sysctl_icmp_ignore_bogus_error_responses;

/* From ip_fragment.c */
extern int sysctl_ipfrag_low_thresh;
extern int sysctl_ipfrag_high_thresh; 
extern int sysctl_ipfrag_time;
extern int sysctl_ipfrag_secret_interval;

/* From ip_output.c */
extern int sysctl_ip_dynaddr;

/* From icmp.c */
extern int sysctl_icmp_ratelimit;
extern int sysctl_icmp_ratemask;

/* From igmp.c */
extern int sysctl_igmp_max_memberships;
extern int sysctl_igmp_max_msf;

/* From inetpeer.c */
extern int inet_peer_threshold;
extern int inet_peer_minttl;
extern int inet_peer_maxttl;
extern int inet_peer_gc_mintime;
extern int inet_peer_gc_maxtime;

#ifdef CONFIG_SYSCTL
static int tcp_retr1_max = 255; 
static int ip_local_port_range_min[] = { 1, 1 };
static int ip_local_port_range_max[] = { 65535, 65535 };
#endif

/* From tcp_input.c */
extern int sysctl_tcp_westwood;

struct ipv4_config ipv4_config;

extern ctl_table ipv4_route_table[];

#ifdef CONFIG_SYSCTL

static
int ipv4_sysctl_forward(ctl_table *ctl, int write, struct file * filp,
			void *buffer, size_t *lenp)
{
	int val = ipv4_devconf.forwarding;
	int ret;

	ret = proc_dointvec(ctl, write, filp, buffer, lenp);

	if (write && ipv4_devconf.forwarding != val)
		inet_forward_change(ipv4_devconf.forwarding);

	return ret;
}

static int ipv4_sysctl_forward_strategy(ctl_table *table, int *name, int nlen,
			 void *oldval, size_t *oldlenp,
			 void *newval, size_t newlen, 
			 void **context)
{
	int new;
	if (newlen != sizeof(int))
		return -EINVAL;
	if (get_user(new,(int *)newval))
		return -EFAULT; 
	if (new != ipv4_devconf.forwarding) 
		inet_forward_change(new); 
	return 0; /* caller does change again and handles handles oldval */ 
}

ctl_table ipv4_table[] = {
        {NET_IPV4_TCP_TIMESTAMPS, "tcp_timestamps",
         &sysctl_tcp_timestamps, sizeof(int), 0644, NULL,
         &proc_dointvec},
        {NET_IPV4_TCP_WINDOW_SCALING, "tcp_window_scaling",
         &sysctl_tcp_window_scaling, sizeof(int), 0644, NULL,
         &proc_dointvec},
        {NET_IPV4_TCP_SACK, "tcp_sack",
         &sysctl_tcp_sack, sizeof(int), 0644, NULL,
         &proc_dointvec},
        {NET_IPV4_TCP_RETRANS_COLLAPSE, "tcp_retrans_collapse",
         &sysctl_tcp_retrans_collapse, sizeof(int), 0644, NULL,
         &proc_dointvec},
        {NET_IPV4_FORWARD, "ip_forward",
         &ipv4_devconf.forwarding, sizeof(int), 0644, NULL,
         &ipv4_sysctl_forward,&ipv4_sysctl_forward_strategy},
        {NET_IPV4_DEFAULT_TTL, "ip_default_ttl",
         &sysctl_ip_default_ttl, sizeof(int), 0644, NULL,
         &proc_dointvec},
        {NET_IPV4_AUTOCONFIG, "ip_autoconfig",
         &ipv4_config.autoconfig, sizeof(int), 0644, NULL,
         &proc_dointvec},
        {NET_IPV4_NO_PMTU_DISC, "ip_no_pmtu_disc",
         &ipv4_config.no_pmtu_disc, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{NET_IPV4_NONLOCAL_BIND, "ip_nonlocal_bind",
	 &sysctl_ip_nonlocal_bind, sizeof(int), 0644, NULL,
	 &proc_dointvec},
	{NET_IPV4_TCP_SYN_RETRIES, "tcp_syn_retries",
	 &sysctl_tcp_syn_retries, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_SYNACK_RETRIES, "tcp_synack_retries",
	 &sysctl_tcp_synack_retries, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_MAX_ORPHANS, "tcp_max_orphans",
	 &sysctl_tcp_max_orphans, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_MAX_TW_BUCKETS, "tcp_max_tw_buckets",
	 &sysctl_tcp_max_tw_buckets, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV4_IPFRAG_HIGH_THRESH, "ipfrag_high_thresh",
	 &sysctl_ipfrag_high_thresh, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV4_IPFRAG_LOW_THRESH, "ipfrag_low_thresh",
	 &sysctl_ipfrag_low_thresh, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV4_DYNADDR, "ip_dynaddr",
	 &sysctl_ip_dynaddr, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV4_IPFRAG_TIME, "ipfrag_time",
	 &sysctl_ipfrag_time, sizeof(int), 0644, NULL, &proc_dointvec_jiffies, 
	 &sysctl_jiffies},
	{NET_IPV4_TCP_KEEPALIVE_TIME, "tcp_keepalive_time",
	 &sysctl_tcp_keepalive_time, sizeof(int), 0644, NULL, 
	 &proc_dointvec_jiffies, &sysctl_jiffies},
	{NET_IPV4_TCP_KEEPALIVE_PROBES, "tcp_keepalive_probes",
	 &sysctl_tcp_keepalive_probes, sizeof(int), 0644, NULL, 
	 &proc_dointvec},
	{NET_IPV4_TCP_KEEPALIVE_INTVL, "tcp_keepalive_intvl",
	 &sysctl_tcp_keepalive_intvl, sizeof(int), 0644, NULL,
	 &proc_dointvec_jiffies, &sysctl_jiffies},
	{NET_IPV4_TCP_RETRIES1, "tcp_retries1",
	 &sysctl_tcp_retries1, sizeof(int), 0644, NULL, &proc_dointvec_minmax, 
	 &sysctl_intvec, NULL, NULL, &tcp_retr1_max},
	{NET_IPV4_TCP_RETRIES2, "tcp_retries2",
	 &sysctl_tcp_retries2, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV4_TCP_FIN_TIMEOUT, "tcp_fin_timeout",
	 &sysctl_tcp_fin_timeout, sizeof(int), 0644, NULL, 
	 &proc_dointvec_jiffies, &sysctl_jiffies},
#ifdef CONFIG_SYN_COOKIES
	{NET_TCP_SYNCOOKIES, "tcp_syncookies",
	 &sysctl_tcp_syncookies, sizeof(int), 0644, NULL, &proc_dointvec},
#endif
	{NET_TCP_TW_RECYCLE, "tcp_tw_recycle",
	 &sysctl_tcp_tw_recycle, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_ABORT_ON_OVERFLOW, "tcp_abort_on_overflow",
	 &sysctl_tcp_abort_on_overflow, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_STDURG, "tcp_stdurg", &sysctl_tcp_stdurg,
	 sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_RFC1337, "tcp_rfc1337", &sysctl_tcp_rfc1337,
	 sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_MAX_SYN_BACKLOG, "tcp_max_syn_backlog", &sysctl_max_syn_backlog,
	 sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV4_LOCAL_PORT_RANGE, "ip_local_port_range",
	 &sysctl_local_port_range, sizeof(sysctl_local_port_range), 0644, 
	 NULL, &proc_dointvec_minmax, &sysctl_intvec, NULL,
	 ip_local_port_range_min, ip_local_port_range_max },
	{NET_IPV4_ICMP_ECHO_IGNORE_ALL, "icmp_echo_ignore_all",
	 &sysctl_icmp_echo_ignore_all, sizeof(int), 0644, NULL,
	 &proc_dointvec},
	{NET_IPV4_ICMP_ECHO_IGNORE_BROADCASTS, "icmp_echo_ignore_broadcasts",
	 &sysctl_icmp_echo_ignore_broadcasts, sizeof(int), 0644, NULL,
	 &proc_dointvec},
	{NET_IPV4_ICMP_IGNORE_BOGUS_ERROR_RESPONSES, "icmp_ignore_bogus_error_responses",
	 &sysctl_icmp_ignore_bogus_error_responses, sizeof(int), 0644, NULL,
	 &proc_dointvec},
	{NET_IPV4_ROUTE, "route", NULL, 0, 0555, ipv4_route_table},
#ifdef CONFIG_IP_MULTICAST
	{NET_IPV4_IGMP_MAX_MEMBERSHIPS, "igmp_max_memberships",
	 &sysctl_igmp_max_memberships, sizeof(int), 0644, NULL, &proc_dointvec},
#endif
	{NET_IPV4_IGMP_MAX_MSF, "igmp_max_msf",
	 &sysctl_igmp_max_msf, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV4_INET_PEER_THRESHOLD, "inet_peer_threshold",
	 &inet_peer_threshold, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV4_INET_PEER_MINTTL, "inet_peer_minttl",
	 &inet_peer_minttl, sizeof(int), 0644, NULL,
	 &proc_dointvec_jiffies, &sysctl_jiffies},
	{NET_IPV4_INET_PEER_MAXTTL, "inet_peer_maxttl",
	 &inet_peer_maxttl, sizeof(int), 0644, NULL,
	 &proc_dointvec_jiffies, &sysctl_jiffies},
	{NET_IPV4_INET_PEER_GC_MINTIME, "inet_peer_gc_mintime",
	 &inet_peer_gc_mintime, sizeof(int), 0644, NULL,
	 &proc_dointvec_jiffies, &sysctl_jiffies},
	{NET_IPV4_INET_PEER_GC_MAXTIME, "inet_peer_gc_maxtime",
	 &inet_peer_gc_maxtime, sizeof(int), 0644, NULL,
	 &proc_dointvec_jiffies, &sysctl_jiffies},
	{NET_TCP_ORPHAN_RETRIES, "tcp_orphan_retries",
	 &sysctl_tcp_orphan_retries, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_FACK, "tcp_fack",
	 &sysctl_tcp_fack, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_REORDERING, "tcp_reordering",
	 &sysctl_tcp_reordering, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_ECN, "tcp_ecn",
	 &sysctl_tcp_ecn, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_DSACK, "tcp_dsack",
	 &sysctl_tcp_dsack, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_MEM, "tcp_mem",
	 &sysctl_tcp_mem, sizeof(sysctl_tcp_mem), 0644, NULL, &proc_dointvec},
	{NET_TCP_WMEM, "tcp_wmem",
	 &sysctl_tcp_wmem, sizeof(sysctl_tcp_wmem), 0644, NULL, &proc_dointvec},
	{NET_TCP_RMEM, "tcp_rmem",
	 &sysctl_tcp_rmem, sizeof(sysctl_tcp_rmem), 0644, NULL, &proc_dointvec},
	{NET_TCP_APP_WIN, "tcp_app_win",
	 &sysctl_tcp_app_win, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_ADV_WIN_SCALE, "tcp_adv_win_scale",
	 &sysctl_tcp_adv_win_scale, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV4_ICMP_RATELIMIT, "icmp_ratelimit",
	 &sysctl_icmp_ratelimit, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV4_ICMP_RATEMASK, "icmp_ratemask",
	 &sysctl_icmp_ratemask, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_TW_REUSE, "tcp_tw_reuse",
	 &sysctl_tcp_tw_reuse, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_FRTO, "tcp_frto",
	 &sysctl_tcp_frto, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_TCP_LOW_LATENCY, "tcp_low_latency",
	 &sysctl_tcp_low_latency, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_IPV4_IPFRAG_SECRET_INTERVAL, "ipfrag_secret_interval",
	 &sysctl_ipfrag_secret_interval, sizeof(int), 0644, NULL, &proc_dointvec_jiffies, 
	 &sysctl_jiffies},
        {NET_TCP_WESTWOOD, "tcp_westwood",
         &sysctl_tcp_westwood, sizeof(int), 0644, NULL,
         &proc_dointvec},
	{0}
};

#endif /* CONFIG_SYSCTL */
