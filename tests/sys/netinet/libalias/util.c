#include <stdio.h>
#include <stdlib.h>

#include <netinet/in.h>

#include "util.h"

#define REQUIRE(x)	do {				\
	if (!(x)) {					\
		fprintf(stderr, "Failed in %s %s:%d.\n",\
		    __FUNCTION__, __FILE__, __LINE__);	\
		exit(-1);				\
	}						\
} while(0)

int
randcmp(const void *a, const void *b)
{
	int res, r = rand();

	(void)a;
	(void)b;
	res = (r/4 < RAND_MAX/9) ? 1
	    : (r/5 < RAND_MAX/9) ? 0
	    : -1;
	return (res);
}

void
hexdump(void *p, size_t len)
{
	size_t i;
	unsigned char *c = p;
	
	for (i = 0; i < len; i++) {
		printf(" %02x", c[i]);
		switch (i & 0xf) {
		case 0xf: printf("\n"); break;
		case 0x7: printf(" "); break;
		default:  break;
		}
	}
	if ((i & 0xf) != 0x0)
		printf("\n");
}

struct ip *
ip_packet(struct in_addr src, struct in_addr dst, u_char protocol, size_t len)
{
	struct ip * p;

	REQUIRE(len >= 64 && len <= IP_MAXPACKET);

	p = calloc(1, len);
	REQUIRE(p != NULL);

	p->ip_v = IPVERSION;
	p->ip_hl = sizeof(*p)/4;
	p->ip_len = htons(len);
	p->ip_ttl = IPDEFTTL;
	p->ip_src = src;
	p->ip_dst = dst;
	p->ip_p = protocol;
	REQUIRE(p->ip_hl == 5);

	return (p);
}

struct udphdr *
set_udp(struct ip *p, u_short sport, u_short dport) {
	uint32_t *up = (void *)p;
	struct udphdr *u = (void *)&(up[p->ip_hl]);
	int payload = ntohs(p->ip_len) - 4*p->ip_hl;

	REQUIRE(payload >= (int)sizeof(*u));
	p->ip_p = IPPROTO_UDP;
	u->uh_sport = htons(sport);
	u->uh_dport = htons(dport);
	u->uh_ulen = htons(payload);
	return (u);
}
