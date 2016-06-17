/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Checksumming functions for IP, TCP, UDP and so on
 *
 * Authors:	Jorge Cwik, <jorge@laser.satlink.net>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *		Borrows very liberally from tcp.c and ip.c, see those
 *		files for more names.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

/*
 *	Fixes:
 *
 *	Ralf Baechle			:	generic ipv6 checksum
 *	<ralf@waldorf-gmbh.de>
 */

#ifndef _CHECKSUM_H
#define _CHECKSUM_H

#include <asm/types.h>
#include <asm/byteorder.h>
#include <net/ip.h>
#include <asm/uaccess.h>
#include <asm/checksum.h>

#ifndef _HAVE_ARCH_IPV6_CSUM

static __inline__ unsigned short int csum_ipv6_magic(struct in6_addr *saddr,
						     struct in6_addr *daddr,
						     __u16 len,
						     unsigned short proto,
						     unsigned int csum) 
{

	int carry;
	__u32 ulen;
	__u32 uproto;

	csum += saddr->s6_addr32[0];
	carry = (csum < saddr->s6_addr32[0]);
	csum += carry;

	csum += saddr->s6_addr32[1];
	carry = (csum < saddr->s6_addr32[1]);
	csum += carry;

	csum += saddr->s6_addr32[2];
	carry = (csum < saddr->s6_addr32[2]);
	csum += carry;

	csum += saddr->s6_addr32[3];
	carry = (csum < saddr->s6_addr32[3]);
	csum += carry;

	csum += daddr->s6_addr32[0];
	carry = (csum < daddr->s6_addr32[0]);
	csum += carry;

	csum += daddr->s6_addr32[1];
	carry = (csum < daddr->s6_addr32[1]);
	csum += carry;

	csum += daddr->s6_addr32[2];
	carry = (csum < daddr->s6_addr32[2]);
	csum += carry;

	csum += daddr->s6_addr32[3];
	carry = (csum < daddr->s6_addr32[3]);
	csum += carry;

	ulen = htonl((__u32) len);
	csum += ulen;
	carry = (csum < ulen);
	csum += carry;

	uproto = htonl(proto);
	csum += uproto;
	carry = (csum < uproto);
	csum += carry;

	return csum_fold(csum);
}

#endif

#ifndef _HAVE_ARCH_COPY_AND_CSUM_FROM_USER
static inline
unsigned int csum_and_copy_from_user (const char *src, char *dst,
				      int len, int sum, int *err_ptr)
{
	if (verify_area(VERIFY_READ, src, len) == 0)
		return csum_partial_copy_from_user(src, dst, len, sum, err_ptr);

	if (len)
		*err_ptr = -EFAULT;

	return sum;
}
#endif

#ifndef HAVE_CSUM_COPY_USER
static __inline__ unsigned int csum_and_copy_to_user
(const char *src, char *dst, int len, unsigned int sum, int *err_ptr)
{
	sum = csum_partial(src, len, sum);

	if (access_ok(VERIFY_WRITE, dst, len)) {
		if (copy_to_user(dst, src, len) == 0)
			return sum;
	}
	if (len)
		*err_ptr = -EFAULT;

	return -1; /* invalid checksum */
}
#endif

static inline unsigned int csum_add(unsigned int csum, unsigned int addend)
{
	csum += addend;
	return csum + (csum < addend);
}

static inline unsigned int csum_sub(unsigned int csum, unsigned int addend)
{
	return csum_add(csum, ~addend);
}

static inline unsigned int
csum_block_add(unsigned int csum, unsigned int csum2, int offset)
{
	if (offset&1)
		csum2 = ((csum2&0xFF00FF)<<8)+((csum2>>8)&0xFF00FF);
	return csum_add(csum, csum2);
}

static inline unsigned int
csum_block_sub(unsigned int csum, unsigned int csum2, int offset)
{
	if (offset&1)
		csum2 = ((csum2&0xFF00FF)<<8)+((csum2>>8)&0xFF00FF);
	return csum_sub(csum, csum2);
}

#endif
