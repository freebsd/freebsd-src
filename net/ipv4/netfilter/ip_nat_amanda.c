/* Amanda extension for TCP NAT alteration.
 * (C) 2002 by Brian J. Murrell <netfilter@interlinx.bc.ca>
 * based on a copy of HW's ip_nat_irc.c as well as other modules
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *	Module load syntax:
 * 	insmod ip_nat_amanda.o
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <net/tcp.h>
#include <net/udp.h>

#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_nat_helper.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_conntrack_amanda.h>


MODULE_AUTHOR("Brian J. Murrell <netfilter@interlinx.bc.ca>");
MODULE_DESCRIPTION("Amanda NAT helper");
MODULE_LICENSE("GPL");

static unsigned int
amanda_nat_expected(struct sk_buff **pskb,
                    unsigned int hooknum,
                    struct ip_conntrack *ct,
                    struct ip_nat_info *info)
{
	struct ip_conntrack *master = master_ct(ct);
	struct ip_ct_amanda_expect *exp_amanda_info;
	struct ip_nat_multi_range mr;
	u_int32_t newip;

	IP_NF_ASSERT(info);
	IP_NF_ASSERT(master);
	IP_NF_ASSERT(!(info->initialized & (1 << HOOK2MANIP(hooknum))));

	if (HOOK2MANIP(hooknum) == IP_NAT_MANIP_SRC)
		newip = master->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
	else
		newip = master->tuplehash[IP_CT_DIR_REPLY].tuple.src.ip;

	mr.rangesize = 1;
	/* We don't want to manip the per-protocol, just the IPs. */
	mr.range[0].flags = IP_NAT_RANGE_MAP_IPS;
	mr.range[0].min_ip = mr.range[0].max_ip = newip;

	if (HOOK2MANIP(hooknum) == IP_NAT_MANIP_DST) {
		exp_amanda_info = &ct->master->help.exp_amanda_info;
		mr.range[0].flags |= IP_NAT_RANGE_PROTO_SPECIFIED;
		mr.range[0].min = mr.range[0].max
			= ((union ip_conntrack_manip_proto)
				{ .udp = { htons(exp_amanda_info->port) } });
	}

	return ip_nat_setup_info(ct, &mr, hooknum);
}

static int amanda_data_fixup(struct ip_conntrack *ct,
                             struct sk_buff **pskb,
                             enum ip_conntrack_info ctinfo,
                             struct ip_conntrack_expect *exp)
{
	struct ip_ct_amanda_expect *exp_amanda_info;
	struct ip_conntrack_tuple t = exp->tuple;
	char buffer[sizeof("65535")];
	u_int16_t port;

	/* Alter conntrack's expectations. */
	exp_amanda_info = &exp->help.exp_amanda_info;
	t.dst.ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;
	for (port = exp_amanda_info->port; port != 0; port++) {
		t.dst.u.tcp.port = htons(port);
		if (ip_conntrack_change_expect(exp, &t) == 0)
			break;
	}
	if (port == 0)
		return 0;

	sprintf(buffer, "%u", port);
	return ip_nat_mangle_udp_packet(pskb, ct, ctinfo,
	                                exp_amanda_info->offset,
	                                exp_amanda_info->len,
	                                buffer, strlen(buffer));
}

static unsigned int help(struct ip_conntrack *ct,
                         struct ip_conntrack_expect *exp,
                         struct ip_nat_info *info,
                         enum ip_conntrack_info ctinfo,
                         unsigned int hooknum,
                         struct sk_buff **pskb)
{
	int dir = CTINFO2DIR(ctinfo);
	int ret = NF_ACCEPT;

	/* Only mangle things once: original direction in POST_ROUTING
	   and reply direction on PRE_ROUTING. */
	if (!((hooknum == NF_IP_POST_ROUTING && dir == IP_CT_DIR_ORIGINAL)
	      || (hooknum == NF_IP_PRE_ROUTING && dir == IP_CT_DIR_REPLY)))
		return NF_ACCEPT;

	/* if this exectation has a "offset" the packet needs to be mangled */
	if (exp->help.exp_amanda_info.offset != 0)
		if (!amanda_data_fixup(ct, pskb, ctinfo, exp))
			ret = NF_DROP;
	exp->help.exp_amanda_info.offset = 0;

	return ret;
}

static struct ip_nat_helper ip_nat_amanda_helper;

static void __exit fini(void)
{
	ip_nat_helper_unregister(&ip_nat_amanda_helper);
}

static int __init init(void)
{
	struct ip_nat_helper *hlpr = &ip_nat_amanda_helper;

	hlpr->tuple.dst.protonum = IPPROTO_UDP;
	hlpr->tuple.src.u.udp.port = htons(10080);
	hlpr->mask.src.u.udp.port = 0xFFFF;
	hlpr->mask.dst.protonum = 0xFFFF;
	hlpr->help = help;
	hlpr->flags = 0;
	hlpr->me = THIS_MODULE;
	hlpr->expect = amanda_nat_expected;
	hlpr->name = "amanda";

	return ip_nat_helper_register(hlpr);
}

module_init(init);
module_exit(fini);
