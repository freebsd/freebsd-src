/*
 * This is a module which is used for logging packets.
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/spinlock.h>
#include <linux/icmpv6.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/ipv6.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

MODULE_AUTHOR("Jan Rekorajski <baggins@pld.org.pl>");
MODULE_DESCRIPTION("IP6 tables LOG target module");
MODULE_LICENSE("GPL");

struct in_device;
#include <net/route.h>
#include <linux/netfilter_ipv6/ip6t_LOG.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#define NIP6(addr) \
	ntohs((addr).s6_addr16[0]), \
	ntohs((addr).s6_addr16[1]), \
	ntohs((addr).s6_addr16[2]), \
	ntohs((addr).s6_addr16[3]), \
	ntohs((addr).s6_addr16[4]), \
	ntohs((addr).s6_addr16[5]), \
	ntohs((addr).s6_addr16[6]), \
	ntohs((addr).s6_addr16[7])

struct esphdr {
	__u32   spi;
}; /* FIXME evil kludge */
        
/* Use lock to serialize, so printks don't overlap */
static spinlock_t log_lock = SPIN_LOCK_UNLOCKED;

/* takes in current header and pointer to the header */
/* if another header exists, sets hdrptr to the next header
   and returns the new header value, else returns 0 */
static u_int8_t ip6_nexthdr(u_int8_t currenthdr, u_int8_t **hdrptr)
{
	u_int8_t hdrlen, nexthdr = 0;

	switch(currenthdr){
		case IPPROTO_AH:
		/* whoever decided to do the length of AUTH for ipv6
		in 32bit units unlike other headers should be beaten...
		repeatedly...with a large stick...no, an even LARGER
		stick...no, you're still not thinking big enough */
			nexthdr = **hdrptr;
			hdrlen = *hdrptr[1] * 4 + 8;
			*hdrptr = *hdrptr + hdrlen;
			break;
		/*stupid rfc2402 */
		case IPPROTO_DSTOPTS:
		case IPPROTO_ROUTING:
		case IPPROTO_HOPOPTS:
			nexthdr = **hdrptr;
			hdrlen = *hdrptr[1] * 8 + 8;
			*hdrptr = *hdrptr + hdrlen;
			break;
		case IPPROTO_FRAGMENT:
			nexthdr = **hdrptr;
			*hdrptr = *hdrptr + 8;
			break;
	}	
	return nexthdr;

}

/* One level of recursion won't kill us */
static void dump_packet(const struct ip6t_log_info *info,
			struct ipv6hdr *ipv6h, int recurse)
{
	u_int8_t currenthdr = ipv6h->nexthdr;
	u_int8_t *hdrptr;
	int fragment;

	/* Max length: 88 "SRC=0000.0000.0000.0000.0000.0000.0000.0000 DST=0000.0000.0000.0000.0000.0000.0000.0000" */
	printk("SRC=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x ", NIP6(ipv6h->saddr));
	printk("DST=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x ", NIP6(ipv6h->daddr));

	/* Max length: 44 "LEN=65535 TC=255 HOPLIMIT=255 FLOWLBL=FFFFF " */
	printk("LEN=%Zu TC=%u HOPLIMIT=%u FLOWLBL=%u ",
	       ntohs(ipv6h->payload_len) + sizeof(struct ipv6hdr),
	       (ntohl(*(u_int32_t *)ipv6h) & 0x0ff00000) >> 20,
	       ipv6h->hop_limit,
	       (ntohl(*(u_int32_t *)ipv6h) & 0x000fffff));

	fragment = 0;
	hdrptr = (u_int8_t *)(ipv6h + 1);
	while (currenthdr) {
		if ((currenthdr == IPPROTO_TCP) ||
		    (currenthdr == IPPROTO_UDP) ||
		    (currenthdr == IPPROTO_ICMPV6))
			break;
		/* Max length: 48 "OPT (...) " */
		printk("OPT ( ");
		switch (currenthdr) {
		case IPPROTO_FRAGMENT: {
			struct frag_hdr *fhdr = (struct frag_hdr *)hdrptr;

			/* Max length: 11 "FRAG:65535 " */
			printk("FRAG:%u ", ntohs(fhdr->frag_off) & 0xFFF8);

			/* Max length: 11 "INCOMPLETE " */
			if (fhdr->frag_off & htons(0x0001))
				printk("INCOMPLETE ");

			printk("ID:%08x ", fhdr->identification);

			if (ntohs(fhdr->frag_off) & 0xFFF8)
				fragment = 1;

			break;
		}
		case IPPROTO_DSTOPTS:
		case IPPROTO_ROUTING:
		case IPPROTO_HOPOPTS:
			break;
		/* Max Length */
		case IPPROTO_AH:
		case IPPROTO_ESP:
			if (info->logflags & IP6T_LOG_IPOPT) {
				struct esphdr *esph = (struct esphdr *)hdrptr;
				int esp = (currenthdr == IPPROTO_ESP);

				/* Max length: 4 "ESP " */
				printk("%s ",esp ? "ESP" : "AH");

				/* Length: 15 "SPI=0xF1234567 " */
				printk("SPI=0x%x ", ntohl(esph->spi) );
				break;
			}
		default:
			break;
		}
		printk(") ");
		currenthdr = ip6_nexthdr(currenthdr, &hdrptr);
	}

	switch (currenthdr) {
	case IPPROTO_TCP: {
		struct tcphdr *tcph = (struct tcphdr *)hdrptr;

		/* Max length: 10 "PROTO=TCP " */
		printk("PROTO=TCP ");

		if (fragment)
			break;

		/* Max length: 20 "SPT=65535 DPT=65535 " */
		printk("SPT=%u DPT=%u ",
		       ntohs(tcph->source), ntohs(tcph->dest));
		/* Max length: 30 "SEQ=4294967295 ACK=4294967295 " */
		if (info->logflags & IP6T_LOG_TCPSEQ)
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

		if ((info->logflags & IP6T_LOG_TCPOPT)
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
		struct udphdr *udph = (struct udphdr *)hdrptr;

		/* Max length: 10 "PROTO=UDP " */
		printk("PROTO=UDP ");

		if (fragment)
			break;

		/* Max length: 20 "SPT=65535 DPT=65535 " */
		printk("SPT=%u DPT=%u LEN=%u ",
		       ntohs(udph->source), ntohs(udph->dest),
		       ntohs(udph->len));
		break;
	}
	case IPPROTO_ICMPV6: {
		struct icmp6hdr *icmp6h = (struct icmp6hdr *)hdrptr;

		/* Max length: 13 "PROTO=ICMPv6 " */
		printk("PROTO=ICMPv6 ");

		if (fragment)
			break;

		/* Max length: 18 "TYPE=255 CODE=255 " */
		printk("TYPE=%u CODE=%u ", icmp6h->icmp6_type, icmp6h->icmp6_code);

		switch (icmp6h->icmp6_type) {
		case ICMPV6_ECHO_REQUEST:
		case ICMPV6_ECHO_REPLY:
			/* Max length: 19 "ID=65535 SEQ=65535 " */
			printk("ID=%u SEQ=%u ",
				ntohs(icmp6h->icmp6_identifier),
				ntohs(icmp6h->icmp6_sequence));
			break;
		case ICMPV6_MGM_QUERY:
		case ICMPV6_MGM_REPORT:
		case ICMPV6_MGM_REDUCTION:
			break;

		case ICMPV6_PARAMPROB:
			/* Max length: 17 "POINTER=ffffffff " */
			printk("POINTER=%08x ", ntohl(icmp6h->icmp6_pointer));
			/* Fall through */
		case ICMPV6_DEST_UNREACH:
		case ICMPV6_PKT_TOOBIG:
		case ICMPV6_TIME_EXCEED:
			/* Max length: 3+maxlen */
			if (recurse) {
				printk("[");
				dump_packet(info, (struct ipv6hdr *)(icmp6h + 1), 0);
				printk("] ");
			}

			/* Max length: 10 "MTU=65535 " */
			if (icmp6h->icmp6_type == ICMPV6_PKT_TOOBIG)
				printk("MTU=%u ", ntohl(icmp6h->icmp6_mtu));
		}
		break;
	}
	/* Max length: 10 "PROTO 255 " */
	default:
		printk("PROTO=%u ", currenthdr);
	}
}

static unsigned int
ip6t_log_target(struct sk_buff **pskb,
		unsigned int hooknum,
		const struct net_device *in,
		const struct net_device *out,
		const void *targinfo,
		void *userinfo)
{
	struct ipv6hdr *ipv6h = (*pskb)->nh.ipv6h;
	const struct ip6t_log_info *loginfo = targinfo;
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
		if ((*pskb)->dev && (*pskb)->dev->hard_header_len && (*pskb)->mac.raw != (void*)ipv6h) {
			if ((*pskb)->dev->type != ARPHRD_SIT){
			  int i;
			  unsigned char *p = (*pskb)->mac.raw;
			  for (i = 0; i < (*pskb)->dev->hard_header_len; i++,p++)
				printk("%02x%c", *p,
			       		i==(*pskb)->dev->hard_header_len - 1
			       		? ' ':':');
			} else {
			  int i;
			  unsigned char *p = (*pskb)->mac.raw;
			  if ( p - (ETH_ALEN*2+2) > (*pskb)->head ){
			    p -= (ETH_ALEN+2);
			    for (i = 0; i < (ETH_ALEN); i++,p++)
				printk("%02x%s", *p,
					i == ETH_ALEN-1 ? "->" : ":");
			    p -= (ETH_ALEN*2);
			    for (i = 0; i < (ETH_ALEN); i++,p++)
				printk("%02x%c", *p,
					i == ETH_ALEN-1 ? ' ' : ':');
			  }
			  
			  if (((*pskb)->dev->addr_len == 4) &&
			      (*pskb)->dev->hard_header_len > 20){
			    printk("TUNNEL=");
			    p = (*pskb)->mac.raw + 12;
			    for (i = 0; i < 4; i++,p++)
				printk("%3d%s", *p,
					i == 3 ? "->" : ".");
			    for (i = 0; i < 4; i++,p++)
				printk("%3d%c", *p,
					i == 3 ? ' ' : '.');
			  }
			}
		} else
			printk(" ");
	}

	dump_packet(loginfo, ipv6h, 1);
	printk("\n");
	spin_unlock_bh(&log_lock);

	return IP6T_CONTINUE;
}

static int ip6t_log_checkentry(const char *tablename,
			       const struct ip6t_entry *e,
			       void *targinfo,
			       unsigned int targinfosize,
			       unsigned int hook_mask)
{
	const struct ip6t_log_info *loginfo = targinfo;

	if (targinfosize != IP6T_ALIGN(sizeof(struct ip6t_log_info))) {
		DEBUGP("LOG: targinfosize %u != %u\n",
		       targinfosize, IP6T_ALIGN(sizeof(struct ip6t_log_info)));
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

static struct ip6t_target ip6t_log_reg
= { { NULL, NULL }, "LOG", ip6t_log_target, ip6t_log_checkentry, NULL, 
    THIS_MODULE };

static int __init init(void)
{
	if (ip6t_register_target(&ip6t_log_reg))
		return -EINVAL;

	return 0;
}

static void __exit fini(void)
{
	ip6t_unregister_target(&ip6t_log_reg);
}

module_init(init);
module_exit(fini);
