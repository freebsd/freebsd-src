/* FTP extension for TCP NAT alteration. */
#include <linux/module.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_nat_helper.h>
#include <linux/netfilter_ipv4/ip_nat_rule.h>
#include <linux/netfilter_ipv4/ip_conntrack_ftp.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#define MAX_PORTS 8
static int ports[MAX_PORTS];
static int ports_c = 0;

#ifdef MODULE_PARM
MODULE_PARM(ports, "1-" __MODULE_STRING(MAX_PORTS) "i");
#endif

DECLARE_LOCK_EXTERN(ip_ftp_lock);

/* FIXME: Time out? --RR */

static unsigned int
ftp_nat_expected(struct sk_buff **pskb,
		 unsigned int hooknum,
		 struct ip_conntrack *ct,
		 struct ip_nat_info *info)
{
	struct ip_nat_multi_range mr;
	u_int32_t newdstip, newsrcip, newip;
	struct ip_ct_ftp_expect *exp_ftp_info;

	struct ip_conntrack *master = master_ct(ct);
	
	IP_NF_ASSERT(info);
	IP_NF_ASSERT(master);

	IP_NF_ASSERT(!(info->initialized & (1<<HOOK2MANIP(hooknum))));

	DEBUGP("nat_expected: We have a connection!\n");
	exp_ftp_info = &ct->master->help.exp_ftp_info;

	LOCK_BH(&ip_ftp_lock);

	if (exp_ftp_info->ftptype == IP_CT_FTP_PORT
	    || exp_ftp_info->ftptype == IP_CT_FTP_EPRT) {
		/* PORT command: make connection go to the client. */
		newdstip = master->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip;
		newsrcip = master->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;
		DEBUGP("nat_expected: PORT cmd. %u.%u.%u.%u->%u.%u.%u.%u\n",
		       NIPQUAD(newsrcip), NIPQUAD(newdstip));
	} else {
		/* PASV command: make the connection go to the server */
		newdstip = master->tuplehash[IP_CT_DIR_REPLY].tuple.src.ip;
		newsrcip = master->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
		DEBUGP("nat_expected: PASV cmd. %u.%u.%u.%u->%u.%u.%u.%u\n",
		       NIPQUAD(newsrcip), NIPQUAD(newdstip));
	}
	UNLOCK_BH(&ip_ftp_lock);

	if (HOOK2MANIP(hooknum) == IP_NAT_MANIP_SRC)
		newip = newsrcip;
	else
		newip = newdstip;

	DEBUGP("nat_expected: IP to %u.%u.%u.%u\n", NIPQUAD(newip));

	mr.rangesize = 1;
	/* We don't want to manip the per-protocol, just the IPs... */
	mr.range[0].flags = IP_NAT_RANGE_MAP_IPS;
	mr.range[0].min_ip = mr.range[0].max_ip = newip;

	/* ... unless we're doing a MANIP_DST, in which case, make
	   sure we map to the correct port */
	if (HOOK2MANIP(hooknum) == IP_NAT_MANIP_DST) {
		mr.range[0].flags |= IP_NAT_RANGE_PROTO_SPECIFIED;
		mr.range[0].min = mr.range[0].max
			= ((union ip_conntrack_manip_proto)
				{ .tcp = { htons(exp_ftp_info->port) } });
	}
	return ip_nat_setup_info(ct, &mr, hooknum);
}

static int
mangle_rfc959_packet(struct sk_buff **pskb,
		     u_int32_t newip,
		     u_int16_t port,
		     unsigned int matchoff,
		     unsigned int matchlen,
		     struct ip_conntrack *ct,
		     enum ip_conntrack_info ctinfo)
{
	char buffer[sizeof("nnn,nnn,nnn,nnn,nnn,nnn")];

	MUST_BE_LOCKED(&ip_ftp_lock);

	sprintf(buffer, "%u,%u,%u,%u,%u,%u",
		NIPQUAD(newip), port>>8, port&0xFF);

	DEBUGP("calling ip_nat_mangle_tcp_packet\n");

	return ip_nat_mangle_tcp_packet(pskb, ct, ctinfo, matchoff, 
					matchlen, buffer, strlen(buffer));
}

/* |1|132.235.1.2|6275| */
static int
mangle_eprt_packet(struct sk_buff **pskb,
		   u_int32_t newip,
		   u_int16_t port,
		   unsigned int matchoff,
		   unsigned int matchlen,
		   struct ip_conntrack *ct,
		   enum ip_conntrack_info ctinfo)
{
	char buffer[sizeof("|1|255.255.255.255|65535|")];

	MUST_BE_LOCKED(&ip_ftp_lock);

	sprintf(buffer, "|1|%u.%u.%u.%u|%u|", NIPQUAD(newip), port);

	DEBUGP("calling ip_nat_mangle_tcp_packet\n");

	return ip_nat_mangle_tcp_packet(pskb, ct, ctinfo, matchoff, 
					matchlen, buffer, strlen(buffer));
}

/* |1|132.235.1.2|6275| */
static int
mangle_epsv_packet(struct sk_buff **pskb,
		   u_int32_t newip,
		   u_int16_t port,
		   unsigned int matchoff,
		   unsigned int matchlen,
		   struct ip_conntrack *ct,
		   enum ip_conntrack_info ctinfo)
{
	char buffer[sizeof("|||65535|")];

	MUST_BE_LOCKED(&ip_ftp_lock);

	sprintf(buffer, "|||%u|", port);

	DEBUGP("calling ip_nat_mangle_tcp_packet\n");

	return ip_nat_mangle_tcp_packet(pskb, ct, ctinfo, matchoff, 
					matchlen, buffer, strlen(buffer));
}

static int (*mangle[])(struct sk_buff **, u_int32_t, u_int16_t,
		     unsigned int,
		     unsigned int,
		     struct ip_conntrack *,
		     enum ip_conntrack_info)
= { [IP_CT_FTP_PORT] mangle_rfc959_packet,
    [IP_CT_FTP_PASV] mangle_rfc959_packet,
    [IP_CT_FTP_EPRT] mangle_eprt_packet,
    [IP_CT_FTP_EPSV] mangle_epsv_packet
};

static int ftp_data_fixup(const struct ip_ct_ftp_expect *ct_ftp_info,
			  struct ip_conntrack *ct,
			  struct sk_buff **pskb,
			  enum ip_conntrack_info ctinfo,
			  struct ip_conntrack_expect *expect)
{
	u_int32_t newip;
	struct iphdr *iph = (*pskb)->nh.iph;
	struct tcphdr *tcph = (void *)iph + iph->ihl*4;
	u_int16_t port;
	struct ip_conntrack_tuple newtuple;

	MUST_BE_LOCKED(&ip_ftp_lock);
	DEBUGP("FTP_NAT: seq %u + %u in %u\n",
	       expect->seq, ct_ftp_info->len,
	       ntohl(tcph->seq));

	/* Change address inside packet to match way we're mapping
	   this connection. */
	if (ct_ftp_info->ftptype == IP_CT_FTP_PASV
	    || ct_ftp_info->ftptype == IP_CT_FTP_EPSV) {
		/* PASV/EPSV response: must be where client thinks server
		   is */
		newip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;
		/* Expect something from client->server */
		newtuple.src.ip = 
			ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip;
		newtuple.dst.ip = 
			ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;
	} else {
		/* PORT command: must be where server thinks client is */
		newip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
		/* Expect something from server->client */
		newtuple.src.ip = 
			ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.ip;
		newtuple.dst.ip = 
			ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
	}
	newtuple.dst.protonum = IPPROTO_TCP;
	newtuple.src.u.tcp.port = expect->tuple.src.u.tcp.port;

	/* Try to get same port: if not, try to change it. */
	for (port = ct_ftp_info->port; port != 0; port++) {
		newtuple.dst.u.tcp.port = htons(port);

		if (ip_conntrack_change_expect(expect, &newtuple) == 0)
			break;
	}
	if (port == 0)
		return 0;

	if (!mangle[ct_ftp_info->ftptype](pskb, newip, port,
					  expect->seq - ntohl(tcph->seq),
					  ct_ftp_info->len, ct, ctinfo))
		return 0;

	return 1;
}

static unsigned int help(struct ip_conntrack *ct,
			 struct ip_conntrack_expect *exp,
			 struct ip_nat_info *info,
			 enum ip_conntrack_info ctinfo,
			 unsigned int hooknum,
			 struct sk_buff **pskb)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	struct tcphdr *tcph = (void *)iph + iph->ihl*4;
	unsigned int datalen;
	int dir;
	struct ip_ct_ftp_expect *ct_ftp_info;

	if (!exp)
		DEBUGP("ip_nat_ftp: no exp!!");

	ct_ftp_info = &exp->help.exp_ftp_info;

	/* Only mangle things once: original direction in POST_ROUTING
	   and reply direction on PRE_ROUTING. */
	dir = CTINFO2DIR(ctinfo);
	if (!((hooknum == NF_IP_POST_ROUTING && dir == IP_CT_DIR_ORIGINAL)
	      || (hooknum == NF_IP_PRE_ROUTING && dir == IP_CT_DIR_REPLY))) {
		DEBUGP("nat_ftp: Not touching dir %s at hook %s\n",
		       dir == IP_CT_DIR_ORIGINAL ? "ORIG" : "REPLY",
		       hooknum == NF_IP_POST_ROUTING ? "POSTROUTING"
		       : hooknum == NF_IP_PRE_ROUTING ? "PREROUTING"
		       : hooknum == NF_IP_LOCAL_OUT ? "OUTPUT" : "???");
		return NF_ACCEPT;
	}

	datalen = (*pskb)->len - iph->ihl * 4 - tcph->doff * 4;
	LOCK_BH(&ip_ftp_lock);
	/* If it's in the right range... */
	if (between(exp->seq + ct_ftp_info->len,
		    ntohl(tcph->seq),
		    ntohl(tcph->seq) + datalen)) {
		if (!ftp_data_fixup(ct_ftp_info, ct, pskb, ctinfo, exp)) {
			UNLOCK_BH(&ip_ftp_lock);
			return NF_DROP;
		}
	} else {
		/* Half a match?  This means a partial retransmisison.
		   It's a cracker being funky. */
		if (net_ratelimit()) {
			printk("FTP_NAT: partial packet %u/%u in %u/%u\n",
			       exp->seq, ct_ftp_info->len,
			       ntohl(tcph->seq),
			       ntohl(tcph->seq) + datalen);
		}
		UNLOCK_BH(&ip_ftp_lock);
		return NF_DROP;
	}
	UNLOCK_BH(&ip_ftp_lock);

	return NF_ACCEPT;
}

static struct ip_nat_helper ftp[MAX_PORTS];
static char ftp_names[MAX_PORTS][10];

/* Not __exit: called from init() */
static void fini(void)
{
	int i;

	for (i = 0; i < ports_c; i++) {
		DEBUGP("ip_nat_ftp: unregistering port %d\n", ports[i]);
		ip_nat_helper_unregister(&ftp[i]);
	}
}

static int __init init(void)
{
	int i, ret = 0;
	char *tmpname;

	if (ports[0] == 0)
		ports[0] = FTP_PORT;

	for (i = 0; (i < MAX_PORTS) && ports[i]; i++) {
		ftp[i].tuple.dst.protonum = IPPROTO_TCP;
		ftp[i].tuple.src.u.tcp.port = htons(ports[i]);
		ftp[i].mask.dst.protonum = 0xFFFF;
		ftp[i].mask.src.u.tcp.port = 0xFFFF;
		ftp[i].help = help;
		ftp[i].me = THIS_MODULE;
		ftp[i].flags = 0;
		ftp[i].expect = ftp_nat_expected;

		tmpname = &ftp_names[i][0];
		if (ports[i] == FTP_PORT)
			sprintf(tmpname, "ftp");
		else
			sprintf(tmpname, "ftp-%d", i);
		ftp[i].name = tmpname;

		DEBUGP("ip_nat_ftp: Trying to register for port %d\n",
				ports[i]);
		ret = ip_nat_helper_register(&ftp[i]);

		if (ret) {
			printk("ip_nat_ftp: error registering "
			       "helper for port %d\n", ports[i]);
			fini();
			return ret;
		}
		ports_c++;
	}

	return ret;
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
