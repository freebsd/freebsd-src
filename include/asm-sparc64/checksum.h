/* $Id: checksum.h,v 1.17.2.1 2002/03/01 11:40:54 davem Exp $ */
#ifndef __SPARC64_CHECKSUM_H
#define __SPARC64_CHECKSUM_H

/*  checksum.h:  IP/UDP/TCP checksum routines on the V9.
 *
 *  Copyright(C) 1995 Linus Torvalds
 *  Copyright(C) 1995 Miguel de Icaza
 *  Copyright(C) 1996 David S. Miller
 *  Copyright(C) 1996 Eddie C. Dost
 *  Copyright(C) 1997 Jakub Jelinek
 *
 * derived from:
 *	Alpha checksum c-code
 *      ix86 inline assembly
 *      RFC1071 Computing the Internet Checksum
 */

#include <linux/in6.h> 
#include <asm/uaccess.h> 

/* computes the checksum of a memory block at buff, length len,
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

/* the same as csum_partial, but copies from user space while it
 * checksums
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */
extern unsigned int csum_partial_copy_sparc64(const char *src, char *dst, int len, unsigned int sum);
			
extern __inline__ unsigned int 
csum_partial_copy_nocheck (const char *src, char *dst, int len, 
			   unsigned int sum)
{
	int ret;
	unsigned char cur_ds = current->thread.current_ds.seg;
	__asm__ __volatile__ ("wr %%g0, %0, %%asi" : : "i" (ASI_P));
	ret = csum_partial_copy_sparc64(src, dst, len, sum);
	__asm__ __volatile__ ("wr %%g0, %0, %%asi" : : "r" (cur_ds));
	return ret;
}

extern __inline__ unsigned int 
csum_partial_copy_from_user(const char *src, char *dst, int len, 
			    unsigned int sum, int *err)
{
	__asm__ __volatile__ ("stx	%0, [%%sp + 0x7ff + 128]"
			      : : "r" (err));
	return csum_partial_copy_sparc64(src, dst, len, sum);
}

/* 
 *	Copy and checksum to user
 */
#define HAVE_CSUM_COPY_USER
extern unsigned int csum_partial_copy_user_sparc64(const char *src, char *dst, int len, unsigned int sum);
extern __inline__ unsigned int 
csum_and_copy_to_user(const char *src, char *dst, int len, 
		      unsigned int sum, int *err)
{
	__asm__ __volatile__ ("stx	%0, [%%sp + 0x7ff + 128]"
			      : : "r" (err));
	return csum_partial_copy_user_sparc64(src, dst, len, sum);
}
  
/* ihl is always 5 or greater, almost always is 5, and iph is word aligned
 * the majority of the time.
 */
extern __inline__ unsigned short ip_fast_csum(__const__ unsigned char *iph,
					      unsigned int ihl)
{
	unsigned short sum;

	/* Note: We must read %2 before we touch %0 for the first time,
	 *       because GCC can legitimately use the same register for
	 *       both operands.
	 */
	__asm__ __volatile__(
"	sub		%2, 4, %%g7		! IEU0\n"
"	lduw		[%1 + 0x00], %0		! Load	Group\n"
"	lduw		[%1 + 0x04], %%g2	! Load	Group\n"
"	lduw		[%1 + 0x08], %%g3	! Load	Group\n"
"	addcc		%%g2, %0, %0		! IEU1	1 Load Bubble + Group\n"
"	lduw		[%1 + 0x0c], %%g2	! Load\n"
"	addccc		%%g3, %0, %0		! Sngle	Group no Bubble\n"
"	lduw		[%1 + 0x10], %%g3	! Load	Group\n"
"	addccc		%%g2, %0, %0		! Sngle	Group no Bubble\n"
"	addc		%0, %%g0, %0		! Sngle Group\n"
"1:	addcc		%%g3, %0, %0		! IEU1	Group no Bubble\n"
"	add		%1, 4, %1		! IEU0\n"
"	addccc		%0, %%g0, %0		! Sngle Group no Bubble\n"
"	subcc		%%g7, 1, %%g7		! IEU1	Group\n"
"	be,a,pt		%%icc, 2f		! CTI\n"
"	 sll		%0, 16, %%g2		! IEU0\n"
"	lduw		[%1 + 0x10], %%g3	! Load	Group\n"
"	ba,pt		%%xcc, 1b		! CTI\n"
"	 nop					! IEU0\n"
"2:	addcc		%0, %%g2, %%g2		! IEU1	Group\n"
"	srl		%%g2, 16, %0		! IEU0	Group regdep	XXX Scheisse!\n"
"	addc		%0, %%g0, %0		! Sngle	Group\n"
"	xnor		%%g0, %0, %0		! IEU0	Group\n"
"	srl		%0, 0, %0		! IEU0	Group		XXX Scheisse!\n"
	: "=r" (sum), "=&r" (iph)
	: "r" (ihl), "1" (iph)
	: "g2", "g3", "g7", "cc");
	return sum;
}

/* Fold a partial checksum without adding pseudo headers. */
extern __inline__ unsigned short csum_fold(unsigned int sum)
{
	unsigned int tmp;

	__asm__ __volatile__(
"	addcc		%0, %1, %1\n"
"	srl		%1, 16, %1\n"
"	addc		%1, %%g0, %1\n"
"	xnor		%%g0, %1, %0\n"
	: "=&r" (sum), "=r" (tmp)
	: "0" (sum), "1" (sum<<16)
	: "cc");
	return (sum & 0xffff);
}

extern __inline__ unsigned long csum_tcpudp_nofold(unsigned long saddr,
						   unsigned long daddr,
						   unsigned int len,
						   unsigned short proto,
						   unsigned int sum)
{
	__asm__ __volatile__(
"	addcc		%1, %0, %0\n"
"	addccc		%2, %0, %0\n"
"	addccc		%3, %0, %0\n"
"	addc		%0, %%g0, %0\n"
	: "=r" (sum), "=r" (saddr)
	: "r" (daddr), "r" ((proto<<16)+len), "0" (sum), "1" (saddr)
	: "cc");
	return sum;
}

/*
 * computes the checksum of the TCP/UDP pseudo-header
 * returns a 16-bit checksum, already complemented
 */
static inline unsigned short int csum_tcpudp_magic(unsigned long saddr,
						   unsigned long daddr,
						   unsigned short len,
						   unsigned short proto,
						   unsigned int sum) 
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}

#define _HAVE_ARCH_IPV6_CSUM

static __inline__ unsigned short int csum_ipv6_magic(struct in6_addr *saddr,
						     struct in6_addr *daddr,
						     __u32 len,
						     unsigned short proto,
						     unsigned int sum) 
{
	__asm__ __volatile__ (
"	addcc		%3, %4, %%g7\n"
"	addccc		%5, %%g7, %%g7\n"
"	lduw		[%2 + 0x0c], %%g2\n"
"	lduw		[%2 + 0x08], %%g3\n"
"	addccc		%%g2, %%g7, %%g7\n"
"	lduw		[%2 + 0x04], %%g2\n"
"	addccc		%%g3, %%g7, %%g7\n"
"	lduw		[%2 + 0x00], %%g3\n"
"	addccc		%%g2, %%g7, %%g7\n"
"	lduw		[%1 + 0x0c], %%g2\n"
"	addccc		%%g3, %%g7, %%g7\n"
"	lduw		[%1 + 0x08], %%g3\n"
"	addccc		%%g2, %%g7, %%g7\n"
"	lduw		[%1 + 0x04], %%g2\n"
"	addccc		%%g3, %%g7, %%g7\n"
"	lduw		[%1 + 0x00], %%g3\n"
"	addccc		%%g2, %%g7, %%g7\n"
"	addccc		%%g3, %%g7, %0\n"
"	addc		0, %0, %0\n"
	: "=&r" (sum)
	: "r" (saddr), "r" (daddr), "r"(htonl(len)),
	  "r"(htonl(proto)), "r"(sum)
	: "g2", "g3", "g7", "cc");

	return csum_fold(sum);
}

/* this routine is used for miscellaneous IP-like checksums, mainly in icmp.c */
extern __inline__ unsigned short ip_compute_csum(unsigned char * buff, int len)
{
	return csum_fold(csum_partial(buff, len, 0));
}

#endif /* !(__SPARC64_CHECKSUM_H) */
