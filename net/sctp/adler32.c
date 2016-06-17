/* SCTP kernel reference Implementation
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2003 International Business Machines, Corp.
 *
 * This file is part of the SCTP kernel reference Implementation
 *
 * This file has direct heritage from the SCTP user-level reference
 * implementation by R. Stewart, et al.  These functions implement the
 * Adler-32 algorithm as specified by RFC 2960.
 *
 * The SCTP reference implementation is free software;
 * you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * The SCTP reference implementation is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 *
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 * Written or modified by:
 *    Randall Stewart <rstewar1@email.mot.com>
 *    Ken Morneau     <kmorneau@cisco.com>
 *    Qiaobing Xie    <qxie1@email.mot.com>
 *    Sridhar Samudrala <sri@us.ibm.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

/* This is an entry point for external calls
 * Define this function in the header file. This is
 * direct from rfc1950, ...
 *
 * The following C code computes the Adler-32 checksum of a data buffer.
 * It is written for clarity, not for speed.  The sample code is in the
 * ANSI C programming language. Non C users may find it easier to read
 * with these hints:
 *
 *    &      Bitwise AND operator.
 *    >>     Bitwise right shift operator. When applied to an
 *           unsigned quantity, as here, right shift inserts zero bit(s)
 *           at the left.
 *    <<     Bitwise left shift operator. Left shift inserts zero
 *           bit(s) at the right.
 *    ++     "n++" increments the variable n.
 *    %      modulo operator: a % b is the remainder of a divided by b.
 *
 * Well, the above is a bit of a lie, I have optimized this a small
 * tad, but I have commented the original lines below
 */

#include <linux/types.h>
#include <net/sctp/sctp.h>

#define BASE 65521 /* largest prime smaller than 65536 */


/* Performance work as shown this pig to be the
 * worst CPU wise guy. I have done what I could think
 * of on my flight to Australia but I am sure some
 * clever assembly could speed this up, but of
 * course this would require the dreaded #ifdef's for
 * architecture. If you can speed this up more, pass
 * it back and we will incorporate it :-)
 */

unsigned long update_adler32(unsigned long adler,
			     unsigned char *buf, int len)
{
	__u32 s1 = adler & 0xffff;
	__u32 s2 = (adler >> 16) & 0xffff;
        int n;

	for (n = 0; n < len; n++,buf++) {
		/* s1 = (s1 + buf[n]) % BASE */
		/* first we add */
		s1 = (s1 + *buf);

		/* Now if we need to, we do a mod by
		 * subtracting. It seems a bit faster
		 * since I really will only ever do
		 * one subtract at the MOST, since buf[n]
		 * is a max of 255.
		 */
		if (s1 >= BASE)
			s1 -= BASE;

		/* s2 = (s2 + s1) % BASE */
		/* first we add */
		s2 = (s2 + s1);

		/* again, it is more efficient (it seems) to
		 * subtract since the most s2 will ever be
		 * is (BASE-1 + BASE-1) in the worse case.
		 * This would then be (2 * BASE) - 2, which
		 * will still only do one subtract. On Intel
		 * this is much better to do this way and
		 * avoid the divide. Have not -pg'd on
		 * sparc.
		 */
		if (s2 >= BASE) {
			/*      s2 %= BASE;*/
			s2 -= BASE;
		}
	}

	/* Return the adler32 of the bytes buf[0..len-1] */
	return (s2 << 16) + s1;
}

__u32 sctp_start_cksum(__u8 *ptr, __u16 count)
{
	/*
	 * Update a running Adler-32 checksum with the bytes
	 * buf[0..len-1] and return the updated checksum. The Adler-32
	 * checksum should be initialized to 1.
	 */
	__u32 adler = 1L;
	__u32 zero = 0L;

	/* Calculate the CRC up to the checksum field. */
	adler = update_adler32(adler, ptr,
			       sizeof(struct sctphdr) - sizeof(__u32));
	/* Skip over the checksum field. */
	adler = update_adler32(adler, (unsigned char *) &zero,
			       sizeof(__u32));
	ptr += sizeof(struct sctphdr);
	count -= sizeof(struct sctphdr);

	/* Calculate the rest of the Adler-32. */
	adler = update_adler32(adler, ptr, count);

        return adler;
}

__u32 sctp_update_cksum(__u8 *ptr, __u16 count, __u32 adler)
{
	adler = update_adler32(adler, ptr, count);

	return adler;
}

__u32 sctp_update_copy_cksum(__u8 *to, __u8 *from, __u16 count, __u32 adler)
{
	/* Its not worth it to try harder.  Adler32 is obsolescent. */
	adler = update_adler32(adler, from, count);
	memcpy(to, from, count);

	return adler;
}

__u32 sctp_end_cksum(__u32 adler)
{
	return adler;
}
