#include <linux/types.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/netfilter.h>
#include <linux/in.h>
#include <linux/icmp.h>
#include <linux/netfilter_ipv4/ip_conntrack_protocol.h>

unsigned long ip_ct_icmp_timeout = 30*HZ;

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

static int icmp_pkt_to_tuple(const void *datah, size_t datalen,
			     struct ip_conntrack_tuple *tuple)
{
	const struct icmphdr *hdr = datah;

	tuple->dst.u.icmp.type = hdr->type;
	tuple->src.u.icmp.id = hdr->un.echo.id;
	tuple->dst.u.icmp.code = hdr->code;

	return 1;
}

static int icmp_invert_tuple(struct ip_conntrack_tuple *tuple,
			     const struct ip_conntrack_tuple *orig)
{
	/* Add 1; spaces filled with 0. */
	static u_int8_t invmap[]
		= { [ICMP_ECHO] = ICMP_ECHOREPLY + 1,
		    [ICMP_ECHOREPLY] = ICMP_ECHO + 1,
		    [ICMP_TIMESTAMP] = ICMP_TIMESTAMPREPLY + 1,
		    [ICMP_TIMESTAMPREPLY] = ICMP_TIMESTAMP + 1,
		    [ICMP_INFO_REQUEST] = ICMP_INFO_REPLY + 1,
		    [ICMP_INFO_REPLY] = ICMP_INFO_REQUEST + 1,
		    [ICMP_ADDRESS] = ICMP_ADDRESSREPLY + 1,
		    [ICMP_ADDRESSREPLY] = ICMP_ADDRESS + 1};

	if (orig->dst.u.icmp.type >= sizeof(invmap)
	    || !invmap[orig->dst.u.icmp.type])
		return 0;

	tuple->src.u.icmp.id = orig->src.u.icmp.id;
	tuple->dst.u.icmp.type = invmap[orig->dst.u.icmp.type] - 1;
	tuple->dst.u.icmp.code = orig->dst.u.icmp.code;
	return 1;
}

/* Print out the per-protocol part of the tuple. */
static unsigned int icmp_print_tuple(char *buffer,
				     const struct ip_conntrack_tuple *tuple)
{
	return sprintf(buffer, "type=%u code=%u id=%u ",
		       tuple->dst.u.icmp.type,
		       tuple->dst.u.icmp.code,
		       ntohs(tuple->src.u.icmp.id));
}

/* Print out the private part of the conntrack. */
static unsigned int icmp_print_conntrack(char *buffer,
				     const struct ip_conntrack *conntrack)
{
	return 0;
}

/* Returns verdict for packet, or -1 for invalid. */
static int icmp_packet(struct ip_conntrack *ct,
		       struct iphdr *iph, size_t len,
		       enum ip_conntrack_info ctinfo)
{
	/* Try to delete connection immediately after all replies:
           won't actually vanish as we still have skb, and del_timer
           means this will only run once even if count hits zero twice
           (theoretically possible with SMP) */
	if (CTINFO2DIR(ctinfo) == IP_CT_DIR_REPLY) {
		if (atomic_dec_and_test(&ct->proto.icmp.count)
		    && del_timer(&ct->timeout))
			ct->timeout.function((unsigned long)ct);
	} else {
		atomic_inc(&ct->proto.icmp.count);
		ip_ct_refresh(ct, ip_ct_icmp_timeout);
	}

	return NF_ACCEPT;
}

/* Called when a new connection for this protocol found. */
static int icmp_new(struct ip_conntrack *conntrack,
		    struct iphdr *iph, size_t len)
{
	static u_int8_t valid_new[]
		= { [ICMP_ECHO] = 1,
		    [ICMP_TIMESTAMP] = 1,
		    [ICMP_INFO_REQUEST] = 1,
		    [ICMP_ADDRESS] = 1 };

	if (conntrack->tuplehash[0].tuple.dst.u.icmp.type >= sizeof(valid_new)
	    || !valid_new[conntrack->tuplehash[0].tuple.dst.u.icmp.type]) {
		/* Can't create a new ICMP `conn' with this. */
		DEBUGP("icmp: can't create new conn with type %u\n",
		       conntrack->tuplehash[0].tuple.dst.u.icmp.type);
		DUMP_TUPLE(&conntrack->tuplehash[0].tuple);
		return 0;
	}
	atomic_set(&conntrack->proto.icmp.count, 0);
	return 1;
}

struct ip_conntrack_protocol ip_conntrack_protocol_icmp
= { { NULL, NULL }, IPPROTO_ICMP, "icmp",
    icmp_pkt_to_tuple, icmp_invert_tuple, icmp_print_tuple,
    icmp_print_conntrack, icmp_packet, icmp_new, NULL, NULL, NULL };
