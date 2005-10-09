/*	$FreeBSD$	*/

#include "ipf.h"

/*
 * This is meant to work without the IPv6 header files being present or
 * the inet_ntop() library.
 */
void printpacket6(ip)
struct ip *ip;
{
	u_char *buf, p;
	u_short plen, *addrs;
	tcphdr_t *tcp;
	u_32_t flow;

	buf = (u_char *)ip;
	tcp = (tcphdr_t *)(buf + 40);
	p = buf[6];
	flow = ntohl(*(u_32_t *)buf);
	flow &= 0xfffff;
	plen = ntohs(*((u_short *)buf +2));
	addrs = (u_short *)buf + 4;

	printf("ip6/%d %d %#x %d", buf[0] & 0xf, plen, flow, p);
	printf(" %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		ntohs(addrs[0]), ntohs(addrs[1]), ntohs(addrs[2]),
		ntohs(addrs[3]), ntohs(addrs[4]), ntohs(addrs[5]),
		ntohs(addrs[6]), ntohs(addrs[7]));
	if (plen >= 4)
		if (p == IPPROTO_TCP || p == IPPROTO_UDP)
			(void)printf(",%d", ntohs(tcp->th_sport));
	printf(" >");
	addrs += 8;
	printf(" %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		ntohs(addrs[0]), ntohs(addrs[1]), ntohs(addrs[2]),
		ntohs(addrs[3]), ntohs(addrs[4]), ntohs(addrs[5]),
		ntohs(addrs[6]), ntohs(addrs[7]));
	if (plen >= 4)
		if (p == IPPROTO_TCP || p == IPPROTO_UDP)
			(void)printf(",%d", ntohs(tcp->th_dport));
	putchar('\n');
}
