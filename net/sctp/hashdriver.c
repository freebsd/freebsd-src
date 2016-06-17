/* SCTP reference Implementation Copyright (C) 1999 Cisco And Motorola
 * 
 * This file origiantes from Randy Stewart's SCTP reference Implementation.
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
 *    Randy Stewart  rstewar1@email.mot.com
 *    Ken Morneau    kmorneau@cisco.com
 *    Qiaobing Xie   qxie1@email.mot.com
 * 
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorperated into the next SCTP release.
 * 
 * There are still LOTS of bugs in this code... I always run on the motto
 * "it is a wonder any code ever works :)"
 */

#include <linux/types.h>
#include <asm/string.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sla1.h>

/* SCTP Main driver.
 * passing a two pointers and two lengths,
 * returning a digest pointer filled. The md5 code
 * was taken directly from the RFC (2104) so to understand it
 * you may want to go look at the RFC referenced in the
 * SCTP spec. We did modify this code to either user OUR
 * implementation of SLA1 or the MD5 that comes from its
 * RFC. SLA1 may have IPR issues so you need to check in
 * to this if you wish to use it... Or at least that is
 * what the FIP-180.1 web page says.
 */

void sctp_hash_digest(const char *key, const int in_key_len,
		      const char *text, const int text_len,
		      __u8 *digest)
{
	int key_len = in_key_len;
	struct SLA_1_Context context;

	__u8 k_ipad[65];	/* inner padding -
				 * key XORd with ipad
				 */
	__u8 k_opad[65];	/* outer padding -
				 * key XORd with opad
				 */
	__u8 tk[20];
	int i;

	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if (key_len > 64) {
		struct SLA_1_Context tctx;

		SLA1_Init(&tctx);
		SLA1_Process(&tctx, key, key_len);
		SLA1_Final(&tctx,tk);
		key = tk;
		key_len = 20;
        }

	/*
	 * the HMAC_MD5 transform looks like:
	 *
	 * MD5(K XOR opad, MD5(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in pads */
	memset(k_ipad, 0, sizeof k_ipad);
	memset(k_opad, 0, sizeof k_opad);
	memcpy(k_ipad, key, key_len);
	memcpy(k_opad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/* perform inner hash */
	SLA1_Init(&context);                     /* init context for 1st
						  * pass
						  */
	SLA1_Process(&context, k_ipad, 64);      /* start with inner pad */
	SLA1_Process(&context, text, text_len);  /* then text of datagram */
	SLA1_Final(&context,digest);             /* finish up 1st pass */

	/*
         * perform outer hash
         */
	SLA1_Init(&context);                   /* init context for 2nd
						* pass
						*/
	SLA1_Process(&context, k_opad, 64);     /* start with outer pad */
	SLA1_Process(&context, digest, 20);     /* then results of 1st
						 * hash
						 */
	SLA1_Final(&context, digest);          /* finish up 2nd pass */
}

