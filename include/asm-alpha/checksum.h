#ifndef _ALPHA_CHECKSUM_H
#define _ALPHA_CHECKSUM_H


/*
 *	This is a version of ip_compute_csum() optimized for IP headers,
 *	which always checksum on 4 octet boundaries.
 */
extern unsigned short ip_fast_csum(unsigned char * iph, unsigned int ihl);

/*
 * computes the checksum of the TCP/UDP pseudo-header
 * returns a 16-bit checksum, already complemented
 */
extern unsigned short int csum_tcpudp_magic(unsigned long saddr,
					   unsigned long daddr,
					   unsigned short len,
					   unsigned short proto,
					   unsigned int sum);

unsigned int csum_tcpudp_nofold(unsigned long saddr, unsigned long daddr,
				unsigned short len, unsigned short proto,
				unsigned int sum);

/*
 * computes the checksum of a memory block at buff, length len,
 * and adds in "sum" (32-bit)
 *
 * returns a 32-bit number suitable for feeding into itself
 * or csum_tcpudp_magic
 *
 * this function must be called with even lengths, except
 * for the last fragment, which may be odd
 *
 * it's best to have buff aligned on a 32-bit boundary
 */
extern unsigned int csum_partial(const unsigned char * buff, int len, unsigned int sum);

/*
 * the same as csum_partial, but copies from src while it
 * checksums
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */
unsigned int csum_partial_copy(const char *src, char *dst, int len, unsigned int sum);

/*
 * the same as csum_partial, but copies from user space (but on the alpha
 * we have just one address space, so this is identical to the above)
 *
 * this is obsolete and will go away.
 */
#define csum_partial_copy_fromuser csum_partial_copy

/*
 * this is a new version of the above that records errors it finds in *errp,
 * but continues and zeros the rest of the buffer.
 */
unsigned int csum_partial_copy_from_user(const char *src, char *dst, int len, unsigned int sum, int *errp);

unsigned int csum_partial_copy_nocheck(const char *src, char *dst, int len, unsigned int sum);


/*
 * this routine is used for miscellaneous IP-like checksums, mainly
 * in icmp.c
 */

extern unsigned short ip_compute_csum(unsigned char * buff, int len);

/*
 *	Fold a partial checksum without adding pseudo headers
 */

static inline unsigned short csum_fold(unsigned int sum)
{
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);
	return ~sum;
}

#define _HAVE_ARCH_IPV6_CSUM
extern unsigned short int csum_ipv6_magic(struct in6_addr *saddr,
                                          struct in6_addr *daddr,
                                          __u32 len,
                                          unsigned short proto,
                                          unsigned int sum);

#endif
