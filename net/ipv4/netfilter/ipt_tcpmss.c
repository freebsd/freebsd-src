/* Kernel module to match TCP MSS values. */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <net/tcp.h>

#include <linux/netfilter_ipv4/ipt_tcpmss.h>
#include <linux/netfilter_ipv4/ip_tables.h>

#define TH_SYN 0x02

/* Returns 1 if the mss option is set and matched by the range, 0 otherwise */
static inline int
mssoption_match(u_int16_t min, u_int16_t max,
		const struct tcphdr *tcp,
		u_int16_t datalen,
		int invert,
		int *hotdrop)
{
	unsigned int i;
	const u_int8_t *opt = (u_int8_t *)tcp;

	/* If we don't have the whole header, drop packet. */
	if (tcp->doff * 4 > datalen) {
		*hotdrop = 1;
		return 0;
	}

	for (i = sizeof(struct tcphdr); i < tcp->doff * 4; ) {
		if ((opt[i] == TCPOPT_MSS)
		    && ((tcp->doff * 4 - i) >= TCPOLEN_MSS)
		    && (opt[i+1] == TCPOLEN_MSS)) {
			u_int16_t mssval;

			mssval = (opt[i+2] << 8) | opt[i+3];
			
			return (mssval >= min && mssval <= max) ^ invert;
		}
		if (opt[i] < 2) i++;
		else i += opt[i+1]?:1;
	}

	return invert;
}

static int
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
      const void *matchinfo,
      int offset,
      const void *hdr,
      u_int16_t datalen,
      int *hotdrop)
{
	const struct ipt_tcpmss_match_info *info = matchinfo;
	const struct tcphdr *tcph = (void *)skb->nh.iph + skb->nh.iph->ihl*4;

	return mssoption_match(info->mss_min, info->mss_max, tcph,
			       skb->len - skb->nh.iph->ihl*4,
			       info->invert, hotdrop);
}

static inline int find_syn_match(const struct ipt_entry_match *m)
{
	const struct ipt_tcp *tcpinfo = (const struct ipt_tcp *)m->data;

	if (strcmp(m->u.kernel.match->name, "tcp") == 0
	    && (tcpinfo->flg_cmp & TH_SYN)
	    && !(tcpinfo->invflags & IPT_TCP_INV_FLAGS))
		return 1;

	return 0;
}

static int
checkentry(const char *tablename,
           const struct ipt_ip *ip,
           void *matchinfo,
           unsigned int matchsize,
           unsigned int hook_mask)
{
	if (matchsize != IPT_ALIGN(sizeof(struct ipt_tcpmss_match_info)))
		return 0;

	/* Must specify -p tcp */
	if (ip->proto != IPPROTO_TCP || (ip->invflags & IPT_INV_PROTO)) {
		printk("tcpmss: Only works on TCP packets\n");
		return 0;
	}

	return 1;
}

static struct ipt_match tcpmss_match
= { { NULL, NULL }, "tcpmss", &match, &checkentry, NULL, THIS_MODULE };

static int __init init(void)
{
	return ipt_register_match(&tcpmss_match);
}

static void __exit fini(void)
{
	ipt_unregister_match(&tcpmss_match);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
