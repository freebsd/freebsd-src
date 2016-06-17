/* Amanda extension for IP connection tracking, Version 0.2
 * (C) 2002 by Brian J. Murrell <netfilter@interlinx.bc.ca>
 * based on HW's ip_conntrack_irc.c as well as other modules
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *	Module load syntax:
 * 	insmod ip_conntrack_amanda.o [master_timeout=n]
 *	
 *	Where master_timeout is the timeout (in seconds) of the master
 *	connection (port 10080).  This defaults to 5 minutes but if
 *	your clients take longer than 5 minutes to do their work
 *	before getting back to the Amanda server, you can increase
 *	this value.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <net/checksum.h>
#include <net/udp.h>

#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_conntrack_amanda.h>

static unsigned int master_timeout = 300;

MODULE_AUTHOR("Brian J. Murrell <netfilter@interlinx.bc.ca>");
MODULE_DESCRIPTION("Amanda connection tracking module");
MODULE_LICENSE("GPL");
MODULE_PARM(master_timeout, "i");
MODULE_PARM_DESC(master_timeout, "timeout for the master connection");

static struct { char *match; int len; } conns[] = {
	{ "DATA ", 5},
	{ "MESG ", 5},
	{ "INDEX ", 6},
};

#define NUM_MSGS 3


static int help(const struct iphdr *iph, size_t len,
                struct ip_conntrack *ct, enum ip_conntrack_info ctinfo)
{
	struct ip_conntrack_expect exp;
	struct ip_ct_amanda_expect *exp_amanda_info;
	struct udphdr *udph = (void *)iph + iph->ihl * 4;
	u_int32_t udplen = len - iph->ihl * 4;
	u_int32_t datalen = udplen - sizeof(struct udphdr);
	char *data = (char *)udph + sizeof(struct udphdr);
	char *data_limit = data + datalen;
	char *start = data, *tmp;
	int i;

	/* Only look at packets from the Amanda server */
	if (CTINFO2DIR(ctinfo) == IP_CT_DIR_ORIGINAL)
		return NF_ACCEPT;

	if (udplen < sizeof(struct udphdr)) {
		if (net_ratelimit())
			printk("amanda_help: udplen = %u\n", udplen);
		return NF_ACCEPT;
	}

	if (udph->check && 
	    csum_tcpudp_magic(iph->saddr, iph->daddr, udplen, IPPROTO_UDP,
	                      csum_partial((char *)udph, udplen, 0)))
		return NF_ACCEPT;

	/* increase the UDP timeout of the master connection as replies from
	 * Amanda clients to the server can be quite delayed */
	ip_ct_refresh(ct, master_timeout * HZ);
	
	/* Search for "CONNECT " string */
	do {
		if (data + 8 >= data_limit)
			return NF_ACCEPT;
		if (!memcmp(data, "CONNECT ", 8)) {
			data += 8;
			break;
		}
		data++;
	} while(1);

	memset(&exp, 0, sizeof(exp));
	exp.tuple.src.ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip;
	exp.tuple.dst.ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;
	exp.tuple.dst.protonum = IPPROTO_TCP;
	exp.mask.src.ip = 0xFFFFFFFF;
	exp.mask.dst.ip = 0xFFFFFFFF;
	exp.mask.dst.protonum = 0xFFFF;
	exp.mask.dst.u.tcp.port = 0xFFFF;

	exp_amanda_info = &exp.help.exp_amanda_info;
	for (i = 0; data + conns[i].len < data_limit && *data != '\n'; data++) {
		if (memcmp(data, conns[i].match, conns[i].len))
			continue;
		tmp = data += conns[i].len;
		exp_amanda_info->offset = data - start;
		exp_amanda_info->port   = simple_strtoul(data, &data, 10);
		exp_amanda_info->len    = data - tmp;
		if (exp_amanda_info->port == 0 || exp_amanda_info->len > 5)
			break;

		exp.tuple.dst.u.tcp.port = htons(exp_amanda_info->port);
		ip_conntrack_expect_related(ct, &exp);
		if (++i == NUM_MSGS)
			break;
	}

	return NF_ACCEPT;
}

static struct ip_conntrack_helper amanda_helper;

static void __exit fini(void)
{
	ip_conntrack_helper_unregister(&amanda_helper);
}

static int __init init(void)
{
	amanda_helper.tuple.src.u.udp.port = htons(10080);
	amanda_helper.tuple.dst.protonum = IPPROTO_UDP;
	amanda_helper.mask.src.u.udp.port = 0xFFFF;
	amanda_helper.mask.dst.protonum = 0xFFFF;
	amanda_helper.max_expected = NUM_MSGS;
	amanda_helper.timeout = 180;
	amanda_helper.flags = IP_CT_HELPER_F_REUSE_EXPECT;
	amanda_helper.me = THIS_MODULE;
	amanda_helper.help = help;
	amanda_helper.name = "amanda";

	return ip_conntrack_helper_register(&amanda_helper);
}

module_init(init);
module_exit(fini);
