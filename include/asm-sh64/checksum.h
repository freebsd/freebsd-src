#ifndef __ASM_SH64_CHECKSUM_H
#define __ASM_SH64_CHECKSUM_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/checksum.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 */

#include <asm/registers.h>

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
asmlinkage unsigned int csum_partial(const unsigned char * buff, int len, unsigned int sum);
//x

/*
 *	Note: when you get a NULL pointer exception here this means someone
 *	passed in an incorrect kernel address to one of these functions. 
 *	
 *	If you use these functions directly please don't forget the 
 *	verify_area().
 */


#ifdef DJM_USE_ASM_CHECKSUM

/*
 * the same as csum_partial, but copies from src while it
 * checksums, and handles user-space pointer exceptions correctly, when needed.
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */

asmlinkage unsigned int csum_partial_copy_generic( const char *src, char *dst, int len, int sum,
						   int *src_err_ptr, int *dst_err_ptr);
extern __inline__
unsigned int csum_partial_copy_nocheck ( const char *src, char *dst,
					int len, int sum)
{
	return csum_partial_copy_generic ( src, dst, len, sum, NULL, NULL);
}
#else
// Pre SIM:
//#define csum_partial_copy_nocheck csum_partial_copy
// Post SIM:
unsigned int
csum_partial_copy_nocheck(const char *src, char *dst, int len, unsigned int sum);
#endif

#ifdef DJM_USE_ASM_CHECKSUM

extern __inline__
unsigned int csum_partial_copy_from_user ( const char *src, char *dst,
						int len, unsigned int sum, int *err_ptr)
{
	return csum_partial_copy_generic ( src, dst, len, sum, err_ptr, NULL);
}
#else 
unsigned int csum_partial_copy_from_user ( const char *src, char *dst,
					   int len, int sum, int *err_ptr);
//x
#endif


/*
 * These are the old (and unsafe) way of doing checksums, a warning message will be
 * printed if they are used and an exeption occurs.
 *
 * these functions should go away after some time.
 */

#define csum_partial_copy_fromuser csum_partial_copy
//x
unsigned int csum_partial_copy( const char *src, char *dst, int len, unsigned int sum);
//x

/*
 *	Fold a partial checksum
 */

#ifdef DJM_USE_ASM_CHECKSUM
static __inline__ unsigned int csum_fold(unsigned int sum)
{
	unsigned long long __dummy;
	__asm__("mshflo.w	r63, %0, %0\n\t"
		"mshflo.w	%0, r63, %1\n\t"
		"mshfhi.w	%0, r63, %0\n\t"
		"add		%0, %1, %0\n\t"
		"shlli		%0, 16, %1\n\t"
		"add		%0, %1, %0\n\t"
		"sub		r63, %0, %0\n\t"
		"shlri		%0, 48, %0"
		: "=r" (sum), "=&r" (__dummy)
		: "0" (sum));
	return sum;
}
#else

static inline unsigned short csum_fold(unsigned int sum)
//x
{
        sum = (sum & 0xffff) + (sum >> 16);
        sum = (sum & 0xffff) + (sum >> 16);
        return ~(sum);
}
#endif




#ifdef DJM_USE_ASM_CHECKSUM
/*
 *	This is a version of ip_compute_csum() optimized for IP headers,
 *	which always checksum on 4 octet boundaries.
 *
 *      i386 version by Jorge Cwik <jorge@laser.satlink.net>, adapted
 *      for linux by * Arnt Gulbrandsen.
 */
static __inline__ unsigned short ip_fast_csum(unsigned char * iph, unsigned int ihl)
{
	unsigned long long sum, __dummy0, __dummy1;

	__asm__ __volatile__(
		"or	r63, r63, %0\n\t"	/* Clear upper part */
		"or	r63, r63, %3\n\t"	/* Clear upper part */
		"gettr	" __t0 ", %4\n\t"		/* t0 saving */

		/* Less waste using .UW than removing sign extension */
		"shlli	%2, 1, %2\n\t"		/* Longs -> Shorts */
		"addi	%2, -2, %2\n\t"		/* Last Short to start from */
		"ldx.uw	%1, %2, %0\n\t"		/* Get last */
		"addi	%2, -2, %2\n\t"		/* Do it reverse */
		"_pta	4, " __t0 "\n\t"

		"ldx.uw	%1, %2, %3\n\t"
		"add	%0, %3, %0\n\t"		/* Do the sum */
		"shlri	%0, 16, %3\n\t"
		"add	%0, %3, %0\n\t"		/* Fold it now */
		"shlli	%3, 16, %3\n\t"
		"sub	%0, %3, %0\n\t"		/* Remove carry, if any */
		"addi	%2, -2, %2\n\t"		/* Do it reverse */
		"bnei	%2, -2, " __t0 "\n\t"

		"shlli	%0, 48, %0\n\t"		/* Not sure this is required */
		"sub	r63, %0, %0\n\t"	/* Not */
		"shlri	%0, 48, %0\n\t"		/* Not sure this is required */
		"ptabs	%4, " __t0 "\n\t"
	
	: "=r" (sum), "=r" (iph), "=r" (ihl), "=r" (__dummy0), "=r" (__dummy1)
	: "1" (iph), "2" (ihl));

	return (unsigned short) sum;
}
#else
unsigned short ip_fast_csum(unsigned char * iph, unsigned int ihl);
//x
#endif


#ifdef DJM_USE_ASM_CHECKSUM

static __inline__ unsigned long csum_tcpudp_nofold(unsigned long saddr,
						   unsigned long daddr,
						   unsigned short len,
						   unsigned short proto,
						   unsigned int sum) 
{
#ifdef __LITTLE_ENDIAN__
	unsigned long len_proto = (ntohs(len)<<16)+proto*256;
#else
	unsigned long len_proto = (proto<<16)+len;
#endif
	__asm__("add	%0, %1, %0\n\t"
		"add	%0, %2, %0\n\t"
		"add	%0, %3, %0\n\t"
		"shlri	%0, 32, %1\n\t"
		"add	%0, %1, %0\n\t"		/* Fold it to a long */
		"shlli	%1, 32, %1\n\t"
		"sub	%0, %1, %0\n\t"		/* Remove carry, if any */
		: "=r" (sum), "=r" (len_proto)
		: "r" (daddr), "r" (saddr), "1" (len_proto), "0" (sum));

	return sum;
}
#else
unsigned long csum_tcpudp_nofold(unsigned long saddr,
						   unsigned long daddr,
						   unsigned short len,
						   unsigned short proto,
				 unsigned int sum) ;
//x
#endif

/*
 * computes the checksum of the TCP/UDP pseudo-header
 * returns a 16-bit checksum, already complemented
 */
static __inline__ unsigned short int csum_tcpudp_magic(unsigned long saddr,
						       unsigned long daddr,
						       unsigned short len,
						       unsigned short proto,
						       unsigned int sum) 
//x
{
  unsigned short int x;
  //printk("csum_tcpudp_magic saddr %08x daddr %08x len %04x proto %04x sum %08x\n",
  //	 saddr,daddr,len,proto,sum);
  x = csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
  //printk("csum_tcpudp_magic returning %04x\n", x);
  return x;
}

/*
 * this routine is used for miscellaneous IP-like checksums, mainly
 * in icmp.c
 */

static __inline__ unsigned short ip_compute_csum(unsigned char * buff, int len)
//x
{
    return csum_fold (csum_partial(buff, len, 0));
}


#ifdef DJM_USE_ASM_CHECKSUM
#define _HAVE_ARCH_IPV6_CSUM
static __inline__ unsigned short int csum_ipv6_magic(struct in6_addr *saddr,
						     struct in6_addr *daddr,
						     __u32 len,
						     unsigned short proto,
						     unsigned int sum) 
{
	unsigned int __dummy;

	__asm__("ld.l	%2, 0, %1\n\t"
		"add	%0, %1, %0\n\t"
		"ld.l	%2, 4, %1\n\t"
		"add	%0, %1, %0\n\t"
		"ld.l	%2, 8, %1\n\t"
		"add	%0, %1, %0\n\t"
		"ld.l	%2, 12, %1\n\t"
		"add	%0, %1, %0\n\t"
		"ld.l	%3, 0, %1\n\t"
		"add	%0, %1, %0\n\t"
		"ld.l	%3, 4, %1\n\t"
		"add	%0, %1, %0\n\t"
		"ld.l	%3, 8, %1\n\t"
		"add	%0, %1, %0\n\t"
		"ld.l	%3, 12, %1\n\t"
		"add	%0, %1, %0\n\t"
		"add	%0, %4, %0\n\t"
		"add	%0, %5, %0\n\t"
		"shlri	%0, 32, %1\n\t"
		"add	%0, %1, %0\n\t"		/* Fold it to a long */
		"shlli	%1, 32, %1\n\t"
		"sub	%0, %1, %0\n\t"		/* Remove carry, if any */
		: "=r" (sum), "=&r" (__dummy)
		: "r" (saddr), "r" (daddr), 
		  "r" (htonl(len)), "r" (htonl(proto)), "0" (sum));

	return csum_fold(sum);
}
#endif


#ifdef DJM_USE_ASM_CHECKSUM
/* 
 *	Copy and checksum to user
 */


#define HAVE_CSUM_COPY_USER
static __inline__ unsigned int csum_and_copy_to_user (const char *src, char *dst,
				    int len, int sum, int *err_ptr)
{
	if (access_ok(VERIFY_WRITE, dst, len))
		return csum_partial_copy_generic(src, dst, len, sum, NULL, err_ptr);

	if (len)
		*err_ptr = -EFAULT;

	return -1; /* invalid checksum */
}

#endif


#endif /* __ASM_SH64_CHECKSUM_H */
















