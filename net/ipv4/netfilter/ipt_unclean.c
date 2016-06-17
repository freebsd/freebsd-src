/* Kernel module to match suspect packets. */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <net/checksum.h>

#include <linux/netfilter_ipv4/ip_tables.h>

#define limpk(format, args...)						 \
do {									 \
	if (net_ratelimit())						 \
		printk("ipt_unclean: %s" format,			 \
		       embedded ? "(embedded packet) " : "" , ## args);  \
} while(0)

enum icmp_error_status
{
	ICMP_MAY_BE_ERROR,
	ICMP_IS_ERROR,
	ICMP_NOT_ERROR
};

struct icmp_info
{
	size_t min_len, max_len;
	enum icmp_error_status err;
	u_int8_t min_code, max_code;
};

static int
check_ip(struct iphdr *iph, size_t length, int embedded);

/* ICMP-specific checks. */
static int
check_icmp(const struct icmphdr *icmph,
	   u_int16_t datalen,
	   unsigned int offset,
	   int more_frags,
	   int embedded)
{
	static struct icmp_info info[]
		= { [ICMP_ECHOREPLY]
		    = { 8, 65536, ICMP_NOT_ERROR, 0, 0 },
		    [ICMP_DEST_UNREACH]
		    = { 8 + 28, 65536, ICMP_IS_ERROR, 0, 15 },
		    [ICMP_SOURCE_QUENCH]
		    = { 8 + 28, 65536, ICMP_IS_ERROR, 0, 0 },
		    [ICMP_REDIRECT]
		    = { 8 + 28, 65536, ICMP_IS_ERROR, 0, 3 },
		    [ICMP_ECHO]
		    = { 8, 65536, ICMP_NOT_ERROR, 0, 0  },
		    /* Router advertisement. */
		    [9]
		    = { 8, 8 + 255 * 8, ICMP_NOT_ERROR, 0, 0 },
		    /* Router solicitation. */
		    [10]
		    = { 8, 8, ICMP_NOT_ERROR, 0, 0 },
		    [ICMP_TIME_EXCEEDED]
		    = { 8 + 28, 65536, ICMP_IS_ERROR, 0, 1  },
		    [ICMP_PARAMETERPROB]
		    = { 8 + 28, 65536, ICMP_IS_ERROR, 0, 1 },
		    [ICMP_TIMESTAMP]
		    = { 20, 20, ICMP_NOT_ERROR, 0, 0 },
		    [ICMP_TIMESTAMPREPLY]
		    = { 20, 20, ICMP_NOT_ERROR, 0, 0 },
		    [ICMP_INFO_REQUEST]
		    = { 8, 65536, ICMP_NOT_ERROR, 0, 0 },
		    [ICMP_INFO_REPLY]
		    = { 8, 65536, ICMP_NOT_ERROR, 0, 0 },
		    [ICMP_ADDRESS]
		    = { 12, 12, ICMP_NOT_ERROR, 0, 0 },
		    [ICMP_ADDRESSREPLY]
		    = { 12, 12, ICMP_NOT_ERROR, 0, 0 } };

	/* Can't do anything if it's a fragment. */
	if (offset)
		return 1;

	/* Must cover type and code. */
	if (datalen < 2) {
		limpk("ICMP len=%u too short\n", datalen);
		return 0;
	}

	/* If not embedded. */
	if (!embedded) {
		/* Bad checksum?  Don't print, just ignore. */
		if (!more_frags
		    && ip_compute_csum((unsigned char *) icmph, datalen) != 0)
			return 0;

		/* CHECK: Truncated ICMP (even if first fragment). */
		if (icmph->type < sizeof(info)/sizeof(struct icmp_info)
		    && info[icmph->type].min_len != 0
		    && datalen < info[icmph->type].min_len) {
			limpk("ICMP type %u len %u too short\n",
			      icmph->type, datalen);
			return 0;
		}

		/* CHECK: Check within known error ICMPs. */
		if (icmph->type < sizeof(info)/sizeof(struct icmp_info)
		    && info[icmph->type].err == ICMP_IS_ERROR) {
			/* CHECK: Embedded packet must be at least
			   length of iph + 8 bytes. */
			struct iphdr *inner = (void *)icmph + 8;

			/* datalen > 8 since all ICMP_IS_ERROR types
                           have min length > 8 */
			if (datalen - 8 < sizeof(struct iphdr)) {
				limpk("ICMP error internal way too short\n");
				return 0;
			}
			if (datalen - 8 < inner->ihl*4 + 8) {
				limpk("ICMP error internal too short\n");
				return 0;
			}
			if (!check_ip(inner, datalen - 8, 1))
				return 0;
		}
	} else {
		/* CHECK: Can't embed ICMP unless known non-error. */
		if (icmph->type >= sizeof(info)/sizeof(struct icmp_info)
		    || info[icmph->type].err != ICMP_NOT_ERROR) {
			limpk("ICMP type %u not embeddable\n",
			      icmph->type);
			return 0;
		}
	}

	/* CHECK: Invalid ICMP codes. */
	if (icmph->type < sizeof(info)/sizeof(struct icmp_info)
	    && (icmph->code < info[icmph->type].min_code
		|| icmph->code > info[icmph->type].max_code)) {
		limpk("ICMP type=%u code=%u\n",
		      icmph->type, icmph->code);
		return 0;
	}

	/* CHECK: Above maximum length. */
	if (icmph->type < sizeof(info)/sizeof(struct icmp_info)
	    && info[icmph->type].max_len != 0
	    && datalen > info[icmph->type].max_len) {
		limpk("ICMP type=%u too long: %u bytes\n",
		      icmph->type, datalen);
		return 0;
	}

	switch (icmph->type) {
	case ICMP_PARAMETERPROB: {
		/* CHECK: Problem param must be within error packet's
		 * IP header. */
		struct iphdr *iph = (void *)icmph + 8;
		u_int32_t arg = ntohl(icmph->un.gateway);

		if (icmph->code == 0) {
			/* Code 0 means that upper 8 bits is pointer
                           to problem. */
			if ((arg >> 24) >= iph->ihl*4) {
				limpk("ICMP PARAMETERPROB ptr = %u\n",
				      ntohl(icmph->un.gateway) >> 24);
				return 0;
			}
			arg &= 0x00FFFFFF;
		}

		/* CHECK: Rest must be zero. */
		if (arg) {
			limpk("ICMP PARAMETERPROB nonzero arg = %u\n",
			      arg);
			return 0;
		}
		break;
	}

	case ICMP_TIME_EXCEEDED:
	case ICMP_SOURCE_QUENCH:
		/* CHECK: Unused must be zero. */
		if (icmph->un.gateway != 0) {
			limpk("ICMP type=%u unused = %u\n",
			      icmph->type, ntohl(icmph->un.gateway));
			return 0;
		}
		break;
	}

	return 1;
}

/* UDP-specific checks. */
static int
check_udp(const struct iphdr *iph,
	  const struct udphdr *udph,
	  u_int16_t datalen,
	  unsigned int offset,
	  int more_frags,
	  int embedded)
{
	/* Can't do anything if it's a fragment. */
	if (offset)
		return 1;

	/* CHECK: Must cover UDP header. */
	if (datalen < sizeof(struct udphdr)) {
		limpk("UDP len=%u too short\n", datalen);
		return 0;
	}

	/* Bad checksum?  Don't print, just say it's unclean. */
	/* FIXME: SRC ROUTE packets won't match checksum --RR */
	if (!more_frags && !embedded && udph->check
	    && csum_tcpudp_magic(iph->saddr, iph->daddr, datalen, IPPROTO_UDP,
				 csum_partial((char *)udph, datalen, 0)) != 0)
		return 0;

	/* CHECK: Destination port can't be zero. */
	if (!udph->dest) {
		limpk("UDP zero destination port\n");
		return 0;
	}

	if (!more_frags) {
		if (!embedded) {
			/* CHECK: UDP length must match. */
			if (ntohs(udph->len) != datalen) {
				limpk("UDP len too short %u vs %u\n",
				      ntohs(udph->len), datalen);
				return 0;
			}
		} else {
			/* CHECK: UDP length be >= this truncated pkt. */
			if (ntohs(udph->len) < datalen) {
				limpk("UDP len too long %u vs %u\n",
				      ntohs(udph->len), datalen);
				return 0;
			}
		}
	} else {
		/* CHECK: UDP length must be > this frag's length. */
		if (ntohs(udph->len) <= datalen) {
			limpk("UDP fragment len too short %u vs %u\n",
			      ntohs(udph->len), datalen);
			return 0;
		}
	}

	return 1;
}

#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20
#define	TH_ECE	0x40
#define	TH_CWR	0x80

/* table of valid flag combinations - ECE and CWR are always valid */
static u8 tcp_valid_flags[(TH_FIN|TH_SYN|TH_RST|TH_PUSH|TH_ACK|TH_URG) + 1] =
{
	[TH_SYN]			= 1,
	[TH_SYN|TH_ACK]			= 1,
	[TH_RST]			= 1,
	[TH_RST|TH_ACK]			= 1,
	[TH_RST|TH_ACK|TH_PUSH]		= 1,
	[TH_FIN|TH_ACK]			= 1,
	[TH_ACK]			= 1,
	[TH_ACK|TH_PUSH]		= 1,
	[TH_ACK|TH_URG]			= 1,
	[TH_ACK|TH_URG|TH_PUSH]		= 1,
	[TH_FIN|TH_ACK|TH_PUSH]		= 1,
	[TH_FIN|TH_ACK|TH_URG]		= 1,
	[TH_FIN|TH_ACK|TH_URG|TH_PUSH]	= 1
};

/* TCP-specific checks. */
static int
check_tcp(const struct iphdr *iph,
	  const struct tcphdr *tcph,
	  u_int16_t datalen,
	  unsigned int offset,
	  int more_frags,
	  int embedded)
{
	u_int8_t *opt = (u_int8_t *)tcph;
	u_int8_t *endhdr = (u_int8_t *)tcph + tcph->doff * 4;
	u_int8_t tcpflags;
	int end_of_options = 0;
	size_t i;

	/* CHECK: Can't have offset=1: used to override TCP syn-checks. */
	/* In fact, this is caught below (offset < 516). */

	/* Can't do anything if it's a fragment. */
	if (offset)
		return 1;

	/* CHECK: Smaller than minimal TCP hdr. */
	if (datalen < sizeof(struct tcphdr)) {
		if (!embedded) {
			limpk("Packet length %u < TCP header.\n", datalen);
			return 0;
		}
		/* Must have ports available (datalen >= 8), from
                   check_icmp which set embedded = 1 */
		/* CHECK: TCP ports inside ICMP error */
		if (!tcph->source || !tcph->dest) {
			limpk("Zero TCP ports %u/%u.\n",
			      htons(tcph->source), htons(tcph->dest));
			return 0;
		}
		return 1;
	}

	/* CHECK: Smaller than actual TCP hdr. */
	if (datalen < tcph->doff * 4) {
		if (!embedded) {
			limpk("Packet length %u < actual TCP header.\n",
			      datalen);
			return 0;
		} else
			return 1;
	}

	/* Bad checksum?  Don't print, just say it's unclean. */
	/* FIXME: SRC ROUTE packets won't match checksum --RR */
	if (!more_frags && !embedded
	    && csum_tcpudp_magic(iph->saddr, iph->daddr, datalen, IPPROTO_TCP,
				 csum_partial((char *)tcph, datalen, 0)) != 0)
		return 0;

	/* CHECK: TCP ports non-zero */
	if (!tcph->source || !tcph->dest) {
		limpk("Zero TCP ports %u/%u.\n",
		      htons(tcph->source), htons(tcph->dest));
		return 0;
	}

	/* CHECK: TCP reserved bits zero. */
	if(tcp_flag_word(tcph) & TCP_RESERVED_BITS) {
		limpk("TCP reserved bits not zero\n");
		return 0;
	}

	/* CHECK: TCP flags. */
	tcpflags = (((u_int8_t *)tcph)[13] & ~(TH_ECE|TH_CWR));
	if (!tcp_valid_flags[tcpflags]) {
		limpk("TCP flags bad: %u\n", tcpflags);
		return 0;
	}

	for (i = sizeof(struct tcphdr); i < tcph->doff * 4; ) {
		switch (opt[i]) {
		case 0:
			end_of_options = 1;
			i++;
			break;
		case 1:
			i++;
			break;
		default:
			/* CHECK: options after EOO. */
			if (end_of_options) {
				limpk("TCP option %u after end\n",
				      opt[i]);
				return 0;
			}
			/* CHECK: options at tail. */
			else if (i+1 >= tcph->doff * 4) {
				limpk("TCP option %u at tail\n",
				      opt[i]);
				return 0;
			}
			/* CHECK: zero-length options. */
			else if (opt[i+1] == 0) {
				limpk("TCP option %u 0 len\n",
				      opt[i]);
				return 0;
			}
			/* CHECK: oversize options. */
			else if (&opt[i] + opt[i+1] > endhdr) {
				limpk("TCP option %u at %Zu too long\n",
				      (unsigned int) opt[i], i);
				return 0;
			}
			/* Move to next option */
			i += opt[i+1];
		}
	}

	return 1;
}

/* Returns 1 if ok */
/* Standard IP checks. */
static int
check_ip(struct iphdr *iph, size_t length, int embedded)
{
	u_int8_t *opt = (u_int8_t *)iph;
	u_int8_t *endhdr = (u_int8_t *)iph + iph->ihl * 4;
	int end_of_options = 0;
	void *protoh;
	size_t datalen;
	unsigned int i;
	unsigned int offset;

	/* Should only happen for local outgoing raw-socket packets. */
	/* CHECK: length >= ip header. */
	if (length < sizeof(struct iphdr) || length < iph->ihl * 4) {
		limpk("Packet length %Zu < IP header.\n", length);
		return 0;
	}

	offset = ntohs(iph->frag_off) & IP_OFFSET;
	protoh = (void *)iph + iph->ihl * 4;
	datalen = length - iph->ihl * 4;

	/* CHECK: Embedded fragment. */
	if (embedded && offset) {
		limpk("Embedded fragment.\n");
		return 0;
	}

	for (i = sizeof(struct iphdr); i < iph->ihl * 4; ) {
		switch (opt[i]) {
		case 0:
			end_of_options = 1;
			i++;
			break;
		case 1:
			i++;
			break;
		default:
			/* CHECK: options after EOO. */
			if (end_of_options) {
				limpk("IP option %u after end\n",
				      opt[i]);
				return 0;
			}
			/* CHECK: options at tail. */
			else if (i+1 >= iph->ihl * 4) {
				limpk("IP option %u at tail\n",
				      opt[i]);
				return 0;
			}
			/* CHECK: zero-length or one-length options. */
			else if (opt[i+1] < 2) {
				limpk("IP option %u %u len\n",
				      opt[i], opt[i+1]);
				return 0;
			}
			/* CHECK: oversize options. */
			else if (&opt[i] + opt[i+1] > endhdr) {
				limpk("IP option %u at %u too long\n",
				      opt[i], i);
				return 0;
			}
			/* Move to next option */
			i += opt[i+1];
		}
	}

	/* Fragment checks. */

	/* CHECK: More fragments, but doesn't fill 8-byte boundary. */
	if ((ntohs(iph->frag_off) & IP_MF)
	    && (ntohs(iph->tot_len) % 8) != 0) {
		limpk("Truncated fragment %u long.\n", ntohs(iph->tot_len));
		return 0;
	}

	/* CHECK: Oversize fragment a-la Ping of Death. */
	if (offset * 8 + datalen > 65535) {
		limpk("Oversize fragment to %u.\n", offset * 8);
		return 0;
	}

	/* CHECK: DF set and offset or MF set. */
	if ((ntohs(iph->frag_off) & IP_DF)
	    && (offset || (ntohs(iph->frag_off) & IP_MF))) {
		limpk("DF set and offset=%u, MF=%u.\n",
		      offset, ntohs(iph->frag_off) & IP_MF);
		return 0;
	}

	/* CHECK: Zero-sized fragments. */
	if ((offset || (ntohs(iph->frag_off) & IP_MF))
	    && datalen == 0) {
		limpk("Zero size fragment offset=%u\n", offset);
		return 0;
	}

	/* Note: we can have even middle fragments smaller than this:
	   consider a large packet passing through a 600MTU then
	   576MTU link: this gives a fragment of 24 data bytes.  But
	   everyone packs fragments largest first, hence a fragment
	   can't START before 576 - MAX_IP_HEADER_LEN. */

	/* Used to be min-size 576: I recall Alan Cox saying ax25 goes
	   down to 128 (576 taken from RFC 791: All hosts must be
	   prepared to accept datagrams of up to 576 octets).  Use 128
	   here. */
#define MIN_LIKELY_MTU 128
	/* CHECK: Min size of first frag = 128. */
	if ((ntohs(iph->frag_off) & IP_MF)
	    && offset == 0
	    && ntohs(iph->tot_len) < MIN_LIKELY_MTU) {
		limpk("First fragment size %u < %u\n", ntohs(iph->tot_len),
		      MIN_LIKELY_MTU);
		return 0;
	}

	/* CHECK: Min offset of frag = 128 - IP hdr len. */
	if (offset && offset * 8 < MIN_LIKELY_MTU - iph->ihl * 4) {
		limpk("Fragment starts at %u < %u\n", offset * 8,
		      MIN_LIKELY_MTU - iph->ihl * 4);
		return 0;
	}

	/* CHECK: Protocol specification non-zero. */
	if (iph->protocol == 0) {
		limpk("Zero protocol\n");
		return 0;
	}

	/* CHECK: Do not use what is unused.
	 * First bit of fragmentation flags should be unused.
	 * May be used by OS fingerprinting tools.
	 * 04 Jun 2002, Maciej Soltysiak, solt@dns.toxicfilms.tv
	 */
	if (ntohs(iph->frag_off)>>15) {
		limpk("IP unused bit set\n");
		return 0;
	}

	/* Per-protocol checks. */
	switch (iph->protocol) {
	case IPPROTO_ICMP:
		return check_icmp(protoh, datalen, offset,
				  (ntohs(iph->frag_off) & IP_MF),
				  embedded);

	case IPPROTO_UDP:
		return check_udp(iph, protoh, datalen, offset,
				 (ntohs(iph->frag_off) & IP_MF),
				 embedded);

	case IPPROTO_TCP:
		return check_tcp(iph, protoh, datalen, offset,
				 (ntohs(iph->frag_off) & IP_MF),
				 embedded);
	default:
		/* Ignorance is bliss. */
		return 1;
	}
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
	return !check_ip(skb->nh.iph, skb->len, 0);
}

/* Called when user tries to insert an entry of this type. */
static int
checkentry(const char *tablename,
	   const struct ipt_ip *ip,
	   void *matchinfo,
	   unsigned int matchsize,
	   unsigned int hook_mask)
{
	if (matchsize != IPT_ALIGN(0))
		return 0;

	return 1;
}

static struct ipt_match unclean_match
= { { NULL, NULL }, "unclean", &match, &checkentry, NULL, THIS_MODULE };

static int __init init(void)
{
	return ipt_register_match(&unclean_match);
}

static void __exit fini(void)
{
	ipt_unregister_match(&unclean_match);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
