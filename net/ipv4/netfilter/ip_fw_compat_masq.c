/* Masquerading compatibility layer.

   Note that there are no restrictions on other programs binding to
   ports 61000:65095 (in 2.0 and 2.2 they get EADDRINUSE).  Just DONT
   DO IT.
 */
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/udp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/module.h>
#include <net/route.h>

#define ASSERT_READ_LOCK(x) MUST_BE_READ_LOCKED(&ip_conntrack_lock)
#define ASSERT_WRITE_LOCK(x) MUST_BE_WRITE_LOCKED(&ip_conntrack_lock)

#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_core.h>
#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_nat_core.h>
#include <linux/netfilter_ipv4/listhelp.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

unsigned int
do_masquerade(struct sk_buff **pskb, const struct net_device *dev)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	struct ip_nat_info *info;
	enum ip_conntrack_info ctinfo;
	struct ip_conntrack *ct;
	unsigned int ret;

	/* Sorry, only ICMP, TCP and UDP. */
	if (iph->protocol != IPPROTO_ICMP
	    && iph->protocol != IPPROTO_TCP
	    && iph->protocol != IPPROTO_UDP)
		return NF_DROP;

	/* Feed it to connection tracking; in fact we're in NF_IP_FORWARD,
           but connection tracking doesn't expect that */
	ret = ip_conntrack_in(NF_IP_POST_ROUTING, pskb, dev, NULL, NULL);
	if (ret != NF_ACCEPT) {
		DEBUGP("ip_conntrack_in returned %u.\n", ret);
		return ret;
	}

	ct = ip_conntrack_get(*pskb, &ctinfo);

	if (!ct) {
		DEBUGP("ip_conntrack_in set to invalid conntrack.\n");
		return NF_DROP;
	}

	info = &ct->nat.info;

	WRITE_LOCK(&ip_nat_lock);
	/* Setup the masquerade, if not already */
	if (!info->initialized) {
		u_int32_t newsrc;
		struct rtable *rt;
		struct ip_nat_multi_range range;

		/* Pass 0 instead of saddr, since it's going to be changed
		   anyway. */
		if (ip_route_output(&rt, iph->daddr, 0, 0, 0) != 0) {
			DEBUGP("ipnat_rule_masquerade: Can't reroute.\n");
			return NF_DROP;
		}
		newsrc = inet_select_addr(rt->u.dst.dev, rt->rt_gateway,
					  RT_SCOPE_UNIVERSE);
		ip_rt_put(rt);
		range = ((struct ip_nat_multi_range)
			 { 1,
			   {{IP_NAT_RANGE_MAP_IPS|IP_NAT_RANGE_PROTO_SPECIFIED,
			     newsrc, newsrc,
			     { htons(61000) }, { htons(65095) } } } });

		ret = ip_nat_setup_info(ct, &range, NF_IP_POST_ROUTING);
		if (ret != NF_ACCEPT) {
			WRITE_UNLOCK(&ip_nat_lock);
			return ret;
		}
	} else
		DEBUGP("Masquerading already done on this conn.\n");
	WRITE_UNLOCK(&ip_nat_lock);

	return do_bindings(ct, ctinfo, info, NF_IP_POST_ROUTING, pskb);
}

void
check_for_masq_error(struct sk_buff *skb)
{
	enum ip_conntrack_info ctinfo;
	struct ip_conntrack *ct;

	ct = ip_conntrack_get(skb, &ctinfo);
	/* Wouldn't be here if not tracked already => masq'ed ICMP
           ping or error related to masq'd connection */
	IP_NF_ASSERT(ct);
	if (ctinfo == IP_CT_RELATED) {
		icmp_reply_translation(skb, ct, NF_IP_PRE_ROUTING,
				       CTINFO2DIR(ctinfo));
		icmp_reply_translation(skb, ct, NF_IP_POST_ROUTING,
				       CTINFO2DIR(ctinfo));
	}
}

unsigned int
check_for_demasq(struct sk_buff **pskb)
{
	struct ip_conntrack_tuple tuple;
	struct iphdr *iph = (*pskb)->nh.iph;
	struct ip_conntrack_protocol *protocol;
	struct ip_conntrack_tuple_hash *h;
	enum ip_conntrack_info ctinfo;
	struct ip_conntrack *ct;
	int ret;

	protocol = ip_ct_find_proto(iph->protocol);

	/* We don't feed packets to conntrack system unless we know
           they're part of an connection already established by an
           explicit masq command. */
	switch (iph->protocol) {
	case IPPROTO_ICMP:
		/* ICMP errors. */
		ct = icmp_error_track(*pskb, &ctinfo, NF_IP_PRE_ROUTING);
		if (ct) {
			/* We only do SNAT in the compatibility layer.
			   So we can manipulate ICMP errors from
			   server here (== DNAT).  Do SNAT icmp manips
			   in POST_ROUTING handling. */
			if (CTINFO2DIR(ctinfo) == IP_CT_DIR_REPLY) {
				icmp_reply_translation(*pskb, ct,
						       NF_IP_PRE_ROUTING,
						       CTINFO2DIR(ctinfo));
				icmp_reply_translation(*pskb, ct,
						       NF_IP_POST_ROUTING,
						       CTINFO2DIR(ctinfo));
			}
			return NF_ACCEPT;
		}
		/* Fall thru... */
	case IPPROTO_TCP:
	case IPPROTO_UDP:
		IP_NF_ASSERT(((*pskb)->nh.iph->frag_off & htons(IP_OFFSET)) == 0);

		if (!get_tuple(iph, (*pskb)->len, &tuple, protocol)) {
			if (net_ratelimit())
				printk("ip_fw_compat_masq: Can't get tuple\n");
			return NF_ACCEPT;
		}
		break;

	default:
		/* Not ours... */
		return NF_ACCEPT;
	}
	h = ip_conntrack_find_get(&tuple, NULL);

	/* MUST be found, and MUST be reply. */
	if (h && DIRECTION(h) == 1) {
		ret = ip_conntrack_in(NF_IP_PRE_ROUTING, pskb,
				      NULL, NULL, NULL);

		/* Put back the reference gained from find_get */
		nf_conntrack_put(&h->ctrack->infos[0]);
		if (ret == NF_ACCEPT) {
			struct ip_conntrack *ct;
			ct = ip_conntrack_get(*pskb, &ctinfo);

			if (ct) {
				struct ip_nat_info *info = &ct->nat.info;

				do_bindings(ct, ctinfo, info,
					    NF_IP_PRE_ROUTING,
					    pskb);
			} else
				if (net_ratelimit()) 
					printk("ip_fw_compat_masq: conntrack"
					       " didn't like\n");
		}
	} else {
		if (h)
			/* Put back the reference gained from find_get */
			nf_conntrack_put(&h->ctrack->infos[0]);
		ret = NF_ACCEPT;
	}

	return ret;
}

int ip_fw_masq_timeouts(void *user, int len)
{
	printk("Sorry: masquerading timeouts set 5DAYS/2MINS/60SECS\n");
	return 0;
}

static const char *masq_proto_name(u_int16_t protonum)
{
	switch (protonum) {
	case IPPROTO_TCP: return "TCP";
	case IPPROTO_UDP: return "UDP";
	case IPPROTO_ICMP: return "ICMP";
	default: return "MORE-CAFFIENE-FOR-RUSTY";
	}
}

static unsigned int
print_masq(char *buffer, const struct ip_conntrack *conntrack)
{
	char temp[129];

	/* This is for backwards compatibility, but ick!.
	   We should never export jiffies to userspace.
	*/
	sprintf(temp,"%s %08X:%04X %08X:%04X %04X %08X %6d %6d %7lu",
		masq_proto_name(conntrack->tuplehash[0].tuple.dst.protonum),
		ntohl(conntrack->tuplehash[0].tuple.src.ip),
		ntohs(conntrack->tuplehash[0].tuple.src.u.all),
		ntohl(conntrack->tuplehash[0].tuple.dst.ip),
		ntohs(conntrack->tuplehash[0].tuple.dst.u.all),
		ntohs(conntrack->tuplehash[1].tuple.dst.u.all),
		/* Sorry, no init_seq, delta or previous_delta (yet). */
		0, 0, 0,
		conntrack->timeout.expires - jiffies);

	return sprintf(buffer, "%-127s\n", temp);
}

/* Returns true when finished. */
static int
masq_iterate(const struct ip_conntrack_tuple_hash *hash,
	     char *buffer, off_t offset, off_t *upto,
	     unsigned int *len, unsigned int maxlen)
{
	unsigned int newlen;

	IP_NF_ASSERT(hash->ctrack);

	/* Only count originals */
	if (DIRECTION(hash))
		return 0;

	if ((*upto)++ < offset)
		return 0;

	newlen = print_masq(buffer + *len, hash->ctrack);
	if (*len + newlen > maxlen)
		return 1;
	else *len += newlen;

	return 0;
}

/* Everything in the hash is masqueraded. */
static int
masq_procinfo(char *buffer, char **start, off_t offset, int length)
{
	unsigned int i;
	int len = 0;
	off_t upto = 1;

	/* Header: first record */
	if (offset == 0) {
		char temp[128];

		sprintf(temp,
			"Prc FromIP   FPrt ToIP     TPrt Masq Init-seq  Delta PDelta Expires (free=0,0,0)");
		len = sprintf(buffer, "%-127s\n", temp);
		offset = 1;
	}

	READ_LOCK(&ip_conntrack_lock);
	/* Traverse hash; print originals then reply. */
	for (i = 0; i < ip_conntrack_htable_size; i++) {
		if (LIST_FIND(&ip_conntrack_hash[i], masq_iterate,
			      struct ip_conntrack_tuple_hash *,
			      buffer, offset, &upto, &len, length))
			break;
	}
	READ_UNLOCK(&ip_conntrack_lock);

	/* `start' hack - see fs/proc/generic.c line ~165 */
	*start = (char *)((unsigned int)upto - offset);
	return len;
}

int __init masq_init(void)
{
	int ret;
	struct proc_dir_entry *proc;

	ret = ip_conntrack_init();
	if (ret == 0) {
		ret = ip_nat_init();
		if (ret == 0) {
			proc = proc_net_create("ip_masquerade",
					       0, masq_procinfo);
			if (proc)
				proc->owner = THIS_MODULE;
			else {
				ip_nat_cleanup();
				ip_conntrack_cleanup();
				ret = -ENOMEM;
			}
		} else
			ip_conntrack_cleanup();
	}

	return ret;
}

void masq_cleanup(void)
{
	ip_nat_cleanup();
	ip_conntrack_cleanup();
	proc_net_remove("ip_masquerade");
}
