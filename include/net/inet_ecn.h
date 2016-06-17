#ifndef _INET_ECN_H_
#define _INET_ECN_H_

static inline int INET_ECN_is_ce(__u8 dsfield)
{
	return (dsfield&3) == 3;
}

static inline int INET_ECN_is_not_ce(__u8 dsfield)
{
	return (dsfield&3) == 2;
}

static inline int INET_ECN_is_capable(__u8 dsfield)
{
	return (dsfield&2);
}

static inline __u8 INET_ECN_encapsulate(__u8 outer, __u8 inner)
{
	outer &= ~3;
	if (INET_ECN_is_capable(inner))
		outer |= (inner & 3);
	return outer;
}

#define	INET_ECN_xmit(sk) do { (sk)->protinfo.af_inet.tos |= 2; } while (0)
#define	INET_ECN_dontxmit(sk) do { (sk)->protinfo.af_inet.tos &= ~3; } while (0)

#define IP6_ECN_flow_init(label) do {	\
      (label) &= ~htonl(3<<20);		\
    } while (0)

#define	IP6_ECN_flow_xmit(sk, label) do {			\
	if (INET_ECN_is_capable((sk)->protinfo.af_inet.tos))	\
		(label) |= __constant_htons(2 << 4);		\
    } while (0)

static inline void IP_ECN_set_ce(struct iphdr *iph)
{
	u32 check = iph->check;
	check += __constant_htons(0xFFFE);
	iph->check = check + (check>=0xFFFF);
	iph->tos |= 1;
}

struct ipv6hdr;

static inline void IP6_ECN_set_ce(struct ipv6hdr *iph)
{
	*(u32*)iph |= htonl(1<<20);
}

#define ip6_get_dsfield(iph) ((ntohs(*(u16*)(iph)) >> 4) & 0xFF)

#endif
