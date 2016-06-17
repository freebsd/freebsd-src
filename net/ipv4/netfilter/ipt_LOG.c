/*
 * This is a module which is used for logging packets.
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/ip.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/route.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_LOG.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

/* FIXME: move to ip.h like in 2.5 */
struct ahhdr {
	__u8    nexthdr;
	__u8    hdrlen;
	__u16   reserved;
	__u32   spi;
	__u32   seq_no;
};

struct esphdr {
	__u32   spi;
	__u32   seq_no;
};

/* Use lock to serialize, so printks don't overlap */
static spinlock_t log_lock = SPIN_LOCK_UNLOCKED;

/* One level of recursion won't kill us */
static void dump_packet(const struct ipt_log_info *info,
			struct iphdr *iph, unsigned int len, int recurse)
{
	void *protoh = (u_int32_t *)iph + iph->ihl;
	unsigned int datalen = len - iph->ihl * 4;

	/* Important fields:
	 * TOS, len, DF/MF, fragment offset, TTL, src, dst, options. */
	/* Max length: 40 "SRC=255.255.255.255 DST=255.255.255.255 " */
	printk("SRC=%u.%u.%u.%u DST=%u.%u.%u.%u ",
	       NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));

	/* Max length: 46 "LEN=65535 TOS=0xFF PREC=0xFF TTL=255 ID=65535 " */
	printk("LEN=%u TOS=0x%02X PREC=0x%02X TTL=%u ID=%u ",
	       ntohs(iph->tot_len), iph->tos & IPTOS_TOS_MASK,
	       iph->tos & IPTOS_PREC_MASK, iph->ttl, ntohs(iph->id));

	/* Max length: 6 "CE DF MF " */
	if (ntohs(iph->frag_off) & IP_CE)
		printk("CE ");
	if (ntohs(iph->frag_off) & IP_DF)
		printk("DF ");
	if (ntohs(iph->frag_off) & IP_MF)
		printk("MF ");

	/* Max length: 11 "FRAG:65535 " */
	if (ntohs(iph->frag_off) & IP_OFFSET)
		printk("FRAG:%u ", ntohs(iph->frag_off) & IP_OFFSET);

	if ((info->logflags & IPT_LOG_IPOPT)
	    && iph->ihl * 4 != sizeof(struct iphdr)
	    && iph->ihl * 4 >= datalen) {
		unsigned int i;

		/* Max length: 127 "OPT (" 15*4*2chars ") " */
		printk("OPT (");
		for (i = sizeof(struct iphdr); i < iph->ihl * 4; i++)
			printk("%02X", ((u_int8_t *)iph)[i]);
		printk(") ");
	}

	switch (iph->protocol) {
	case IPPROTO_TCP: {
		struct tcphdr *tcph = protoh;

		/* Max length: 10 "PROTO=TCP " */
		printk("PROTO=TCP ");

		if (ntohs(iph->frag_off) & IP_OFFSET)
			break;

		/* Max length: 25 "INCOMPLETE [65535 bytes] " */
		if (datalen < sizeof (*tcph)) {
			printk("INCOMPLETE [%u bytes] ", datalen);
			break;
		}

		/* Max length: 20 "SPT=65535 DPT=65535 " */
		printk("SPT=%u DPT=%u ",
		       ntohs(tcph->source), ntohs(tcph->dest));
		/* Max length: 30 "SEQ=4294967295 ACK=4294967295 " */
		if (info->logflags & IPT_LOG_TCPSEQ)
			printk("SEQ=%u ACK=%u ",
			       ntohl(tcph->seq), ntohl(tcph->ack_seq));
		/* Max length: 13 "WINDOW=65535 " */
		printk("WINDOW=%u ", ntohs(tcph->window));
		/* Max length: 9 "RES=0x3F " */
		printk("RES=0x%02x ", (u_int8_t)(ntohl(tcp_flag_word(tcph) & TCP_RESERVED_BITS) >> 22));
		/* Max length: 32 "CWR ECE URG ACK PSH RST SYN FIN " */
		if (tcph->cwr)
			printk("CWR ");
		if (tcph->ece)
			printk("ECE ");
		if (tcph->urg)
			printk("URG ");
		if (tcph->ack)
			printk("ACK ");
		if (tcph->psh)
			printk("PSH ");
		if (tcph->rst)
			printk("RST ");
		if (tcph->syn)
			printk("SYN ");
		if (tcph->fin)
			printk("FIN ");
		/* Max length: 11 "URGP=65535 " */
		printk("URGP=%u ", ntohs(tcph->urg_ptr));

		if ((info->logflags & IPT_LOG_TCPOPT)
		    && tcph->doff * 4 != sizeof(struct tcphdr)) {
			unsigned int i;

			/* Max length: 127 "OPT (" 15*4*2chars ") " */
			printk("OPT (");
			for (i =sizeof(struct tcphdr); i < tcph->doff * 4; i++)
				printk("%02X", ((u_int8_t *)tcph)[i]);
			printk(") ");
		}
		break;
	}
	case IPPROTO_UDP: {
		struct udphdr *udph = protoh;

		/* Max length: 10 "PROTO=UDP " */
		printk("PROTO=UDP ");

		if (ntohs(iph->frag_off) & IP_OFFSET)
			break;

		/* Max length: 25 "INCOMPLETE [65535 bytes] " */
		if (datalen < sizeof (*udph)) {
			printk("INCOMPLETE [%u bytes] ", datalen);
			break;
		}

		/* Max length: 20 "SPT=65535 DPT=65535 " */
		printk("SPT=%u DPT=%u LEN=%u ",
		       ntohs(udph->source), ntohs(udph->dest),
		       ntohs(udph->len));
		break;
	}
	case IPPROTO_ICMP: {
		struct icmphdr *icmph = protoh;
		static size_t required_len[NR_ICMP_TYPES+1]
			= { [ICMP_ECHOREPLY] = 4,
			    [ICMP_DEST_UNREACH]
			    = 8 + sizeof(struct iphdr) + 8,
			    [ICMP_SOURCE_QUENCH]
			    = 8 + sizeof(struct iphdr) + 8,
			    [ICMP_REDIRECT]
			    = 8 + sizeof(struct iphdr) + 8,
			    [ICMP_ECHO] = 4,
			    [ICMP_TIME_EXCEEDED]
			    = 8 + sizeof(struct iphdr) + 8,
			    [ICMP_PARAMETERPROB]
			    = 8 + sizeof(struct iphdr) + 8,
			    [ICMP_TIMESTAMP] = 20,
			    [ICMP_TIMESTAMPREPLY] = 20,
			    [ICMP_ADDRESS] = 12,
			    [ICMP_ADDRESSREPLY] = 12 };

		/* Max length: 11 "PROTO=ICMP " */
		printk("PROTO=ICMP ");

		if (ntohs(iph->frag_off) & IP_OFFSET)
			break;

		/* Max length: 25 "INCOMPLETE [65535 bytes] " */
		if (datalen < 4) {
			printk("INCOMPLETE [%u bytes] ", datalen);
			break;
		}

		/* Max length: 18 "TYPE=255 CODE=255 " */
		printk("TYPE=%u CODE=%u ", icmph->type, icmph->code);

		/* Max length: 25 "INCOMPLETE [65535 bytes] " */
		if (icmph->type <= NR_ICMP_TYPES
		    && required_len[icmph->type]
		    && datalen < required_len[icmph->type]) {
			printk("INCOMPLETE [%u bytes] ", datalen);
			break;
		}

		switch (icmph->type) {
		case ICMP_ECHOREPLY:
		case ICMP_ECHO:
			/* Max length: 19 "ID=65535 SEQ=65535 " */
			printk("ID=%u SEQ=%u ",
			       ntohs(icmph->un.echo.id),
			       ntohs(icmph->un.echo.sequence));
			break;

		case ICMP_PARAMETERPROB:
			/* Max length: 14 "PARAMETER=255 " */
			printk("PARAMETER=%u ",
			       ntohl(icmph->un.gateway) >> 24);
			break;
		case ICMP_REDIRECT:
			/* Max length: 24 "GATEWAY=255.255.255.255 " */
			printk("GATEWAY=%u.%u.%u.%u ", NIPQUAD(icmph->un.gateway));
			/* Fall through */
		case ICMP_DEST_UNREACH:
		case ICMP_SOURCE_QUENCH:
		case ICMP_TIME_EXCEEDED:
			/* Max length: 3+maxlen */
			if (recurse) {
				printk("[");
				dump_packet(info,
					    (struct iphdr *)(icmph + 1),
					    datalen-sizeof(struct icmphdr),
					    0);
				printk("] ");
			}

			/* Max length: 10 "MTU=65535 " */
			if (icmph->type == ICMP_DEST_UNREACH
			    && icmph->code == ICMP_FRAG_NEEDED)
				printk("MTU=%u ", ntohs(icmph->un.frag.mtu));
		}
		break;
	}
	/* Max Length */
	case IPPROTO_AH: {
		struct ahhdr *ah = protoh;

		/* Max length: 9 "PROTO=AH " */
		printk("PROTO=AH ");

		if (ntohs(iph->frag_off) & IP_OFFSET)
			break;

		/* Max length: 25 "INCOMPLETE [65535 bytes] " */
		if (datalen < sizeof (*ah)) {
			printk("INCOMPLETE [%u bytes] ", datalen);
			break;
		}

		/* Length: 15 "SPI=0xF1234567 " */
		printk("SPI=0x%x ", ntohl(ah->spi) );
		break;
	}
	case IPPROTO_ESP: {
		struct esphdr *esph = protoh;

		/* Max length: 10 "PROTO=ESP " */
		printk("PROTO=ESP ");

		if (ntohs(iph->frag_off) & IP_OFFSET)
			break;

		/* Max length: 25 "INCOMPLETE [65535 bytes] " */
		if (datalen < sizeof (*esph)) {
			printk("INCOMPLETE [%u bytes] ", datalen);
			break;
		}

		/* Length: 15 "SPI=0xF1234567 " */
		printk("SPI=0x%x ", ntohl(esph->spi) );
		break;
	}
	/* Max length: 10 "PROTO 255 " */
	default:
		printk("PROTO=%u ", iph->protocol);
	}

	/* Proto    Max log string length */
	/* IP:      40+46+6+11+127 = 230 */
	/* TCP:     10+max(25,20+30+13+9+32+11+127) = 252 */
	/* UDP:     10+max(25,20) = 35 */
	/* ICMP:    11+max(25, 18+25+max(19,14,24+3+n+10,3+n+10)) = 91+n */
	/* ESP:     10+max(25)+15 = 50 */
	/* AH:      9+max(25)+15 = 49 */
	/* unknown: 10 */

	/* (ICMP allows recursion one level deep) */
	/* maxlen =  IP + ICMP +  IP + max(TCP,UDP,ICMP,unknown) */
	/* maxlen = 230+   91  + 230 + 252 = 803 */
}

static unsigned int
ipt_log_target(struct sk_buff **pskb,
	       unsigned int hooknum,
	       const struct net_device *in,
	       const struct net_device *out,
	       const void *targinfo,
	       void *userinfo)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	const struct ipt_log_info *loginfo = targinfo;
	char level_string[4] = "< >";

	level_string[1] = '0' + (loginfo->level % 8);
	spin_lock_bh(&log_lock);
	printk(level_string);
	printk("%sIN=%s OUT=%s ",
	       loginfo->prefix,
	       in ? in->name : "",
	       out ? out->name : "");
	if (in && !out) {
		/* MAC logging for input chain only. */
		printk("MAC=");
		if ((*pskb)->dev && (*pskb)->dev->hard_header_len && (*pskb)->mac.raw != (void*)iph) {
			int i;
			unsigned char *p = (*pskb)->mac.raw;
			for (i = 0; i < (*pskb)->dev->hard_header_len; i++,p++)
				printk("%02x%c", *p,
				       i==(*pskb)->dev->hard_header_len - 1
				       ? ' ':':');
		} else
			printk(" ");
	}

	dump_packet(loginfo, iph, (*pskb)->len, 1);
	printk("\n");
	spin_unlock_bh(&log_lock);

	return IPT_CONTINUE;
}

static int ipt_log_checkentry(const char *tablename,
			      const struct ipt_entry *e,
			      void *targinfo,
			      unsigned int targinfosize,
			      unsigned int hook_mask)
{
	const struct ipt_log_info *loginfo = targinfo;

	if (targinfosize != IPT_ALIGN(sizeof(struct ipt_log_info))) {
		DEBUGP("LOG: targinfosize %u != %u\n",
		       targinfosize, IPT_ALIGN(sizeof(struct ipt_log_info)));
		return 0;
	}

	if (loginfo->level >= 8) {
		DEBUGP("LOG: level %u >= 8\n", loginfo->level);
		return 0;
	}

	if (loginfo->prefix[sizeof(loginfo->prefix)-1] != '\0') {
		DEBUGP("LOG: prefix term %i\n",
		       loginfo->prefix[sizeof(loginfo->prefix)-1]);
		return 0;
	}

	return 1;
}

static struct ipt_target ipt_log_reg
= { { NULL, NULL }, "LOG", ipt_log_target, ipt_log_checkentry, NULL, 
    THIS_MODULE };

static int __init init(void)
{
	if (ipt_register_target(&ipt_log_reg))
		return -EINVAL;

	return 0;
}

static void __exit fini(void)
{
	ipt_unregister_target(&ipt_log_reg);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
