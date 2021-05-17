#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#ifndef _UTIL_H
#define _UTIL_H

int		randcmp(const void *a, const void *b);
void		hexdump(void *p, size_t len);
struct ip *	ip_packet(struct in_addr src, struct in_addr dst, u_char protocol, size_t len);
struct udphdr * set_udp(struct ip *p, u_short sport, u_short dport);

inline int
addr_eq(struct in_addr a, struct in_addr b)
{
	return a.s_addr == b.s_addr;
}

#define a2h(a)	ntohl(a.s_addr)

inline int
rand_range(int min, int max)
{
	return min + rand()%(max - min);
}

#endif /* _UTIL_H */
