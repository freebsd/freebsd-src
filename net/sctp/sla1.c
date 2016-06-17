/* SCTP kernel reference Implementation
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * 
 * This file is part of the SCTP kernel reference Implementation
 * 
 * (It's really SHA-1 but Hey I was tired when I created this 
 * file, and on a plane to France :-)
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
 *    kmorneau@cisco.com
 *    qxie1@email.mot.com
 *
 * Based on:
 *    Randy Stewart, et al. SCTP Reference Implementation which is licenced
 *    under the GPL. 
 * 
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <asm/string.h>		/* for memcpy */
#include <linux/sched.h>	/* dead chicken for in.h */
#include <linux/in.h>		/* for htonl and ntohl */

#include <net/sctp/sla1.h>

void SLA1_Init(struct SLA_1_Context *ctx)
{
	/* Init the SLA-1 context structure.  */
	ctx->A = 0;
	ctx->B = 0;
	ctx->C = 0;
	ctx->D = 0;
	ctx->E = 0;
	ctx->H0 = H0INIT;
	ctx->H1 = H1INIT;
	ctx->H2 = H2INIT;
	ctx->H3 = H3INIT;
	ctx->H4 = H4INIT;
	ctx->TEMP = 0;
	memset(ctx->words, 0, sizeof(ctx->words));
	ctx->howManyInBlock = 0;
	ctx->runningTotal = 0;
}

void SLA1processABlock(struct SLA_1_Context *ctx,unsigned int *block)
{
	int i;
	/* init the W0-W15 to the block of words being
	 * hashed.
	 */
	/* step a) */

	for (i = 0; i < 16; i++)
		ctx->words[i] = ntohl(block[i]);

	/* now init the rest based on the SLA-1 formula, step b) */
	for (i = 16; i < 80; i++)
		ctx->words[i] =
			CSHIFT(1, ((ctx->words[(i-3)]) ^
				   (ctx->words[(i-8)]) ^
				   (ctx->words[(i-14)]) ^
				   (ctx->words[(i-16)])));

	/* step c) */
	ctx->A = ctx->H0;
	ctx->B = ctx->H1;
	ctx->C = ctx->H2;
	ctx->D = ctx->H3;
	ctx->E = ctx->H4;

	/* step d) */
	for (i = 0; i < 80; i++) {
		if (i < 20) {
			ctx->TEMP = ((CSHIFT(5, ctx->A)) +
				     (F1(ctx->B, ctx->C, ctx->D)) +
				     (ctx->E) +
				     ctx->words[i] +
				     K1
				);
		} else if (i < 40) {
			ctx->TEMP = ((CSHIFT(5, ctx->A)) +
				     (F2(ctx->B, ctx->C, ctx->D)) +
				     (ctx->E) +
				     (ctx->words[i]) +
				     K2
				);
		} else if (i < 60) {
			ctx->TEMP = ((CSHIFT(5, ctx->A)) +
				     (F3(ctx->B, ctx->C, ctx->D)) +
				     (ctx->E) +
				     (ctx->words[i]) +
				     K3
				);
		} else {
			ctx->TEMP = ((CSHIFT(5, ctx->A)) +
				     (F4(ctx->B, ctx->C, ctx->D)) +
				     (ctx->E) +
				     (ctx->words[i]) +
				     K4
				);
		}
		ctx->E = ctx->D;
		ctx->D = ctx->C;
		ctx->C = CSHIFT(30, ctx->B);
		ctx->B = ctx->A;
		ctx->A = ctx->TEMP;
	}

	/* step e) */
	ctx->H0 = (ctx->H0) + (ctx->A);
	ctx->H1 = (ctx->H1) + (ctx->B);
	ctx->H2 = (ctx->H2) + (ctx->C);
	ctx->H3 = (ctx->H3) + (ctx->D);
	ctx->H4 = (ctx->H4) + (ctx->E);
}

void SLA1_Process(struct SLA_1_Context *ctx, const unsigned char *ptr, int siz)
{
	int numberLeft, leftToFill;

	numberLeft = siz;
	while (numberLeft > 0) {
		leftToFill = sizeof(ctx->SLAblock) - ctx->howManyInBlock;
		if (leftToFill > numberLeft) {
			/* can only partially fill up this one */
			memcpy(&ctx->SLAblock[ctx->howManyInBlock],
			       ptr, numberLeft);
			ctx->howManyInBlock += siz;
			ctx->runningTotal += siz;
			break;
		} else {
			/* block is now full, process it */
			memcpy(&ctx->SLAblock[ctx->howManyInBlock],
			       ptr, leftToFill);
			SLA1processABlock(ctx, (unsigned int *) ctx->SLAblock);
			numberLeft -= leftToFill;
			ctx->runningTotal += leftToFill;
			ctx->howManyInBlock = 0;
		}
	}
}

void SLA1_Final(struct SLA_1_Context *ctx, unsigned char *digestBuf)
{
	/* if any left in block fill with padding
	 * and process. Then transfer the digest to
	 * the pointer. At the last block some special
	 * rules need to apply. We must add a 1 bit
	 * following the message, then we pad with
	 * 0's. The total size is encoded as a 64 bit
	 * number at the end. Now if the last buffer has
	 * more than 55 octets in it we cannot fit
	 * the 64 bit number + 10000000 pad on the end
	 * and must add the 10000000 pad, pad the rest
	 * of the message with 0's and then create a
	 * all 0 message with just the 64 bit size
	 * at the end and run this block through by itself.
	 * Also the 64 bit int must be in network byte
	 * order.
	 */
	int i, leftToFill;
	unsigned int *ptr;

	if (ctx->howManyInBlock > 55) {
		/* special case, we need to process two
		 * blocks here. One for the current stuff
		 * plus possibly the pad. The other for
		 * the size.
		 */
		leftToFill = sizeof(ctx->SLAblock) - ctx->howManyInBlock;
		if (leftToFill == 0) {
			/* Should not really happen but I am paranoid */
			/* Not paranoid enough!  It is possible for leftToFill
			 * to become negative!  AAA!!!!  This is another reason
			 * to pick MD5 :-)...
			 */
			SLA1processABlock(ctx, (unsigned int *) ctx->SLAblock);
			/* init last block, a bit different then the rest :-) */
			ctx->SLAblock[0] = 0x80;
			for (i = 1; i < sizeof(ctx->SLAblock); i++) {
				ctx->SLAblock[i] = 0x0;
			}
		} else if (leftToFill == 1) {
			ctx->SLAblock[ctx->howManyInBlock] = 0x80;
			SLA1processABlock(ctx, (unsigned int *) ctx->SLAblock);
			/* init last block */
			memset(ctx->SLAblock, 0, sizeof(ctx->SLAblock));
		} else {
			ctx->SLAblock[ctx->howManyInBlock] = 0x80;
			for (i = (ctx->howManyInBlock + 1);
			     i < sizeof(ctx->SLAblock);
			     i++) {
				ctx->SLAblock[i] = 0x0;
			}
			SLA1processABlock(ctx, (unsigned int *) ctx->SLAblock);
			/* init last block */
			memset(ctx->SLAblock, 0, sizeof(ctx->SLAblock));
		}
		/* This is in bits so multiply by 8 */
		ctx->runningTotal *= 8;
		ptr = (unsigned int *) &ctx->SLAblock[60];
		*ptr = htonl(ctx->runningTotal);
		SLA1processABlock(ctx, (unsigned int *) ctx->SLAblock);
	} else {
		/* easy case, we just pad this
		 * message to size - end with 0
		 * add the magic 0x80 to the next
		 * word and then put the network byte
		 * order size in the last spot and
		 * process the block.
		 */
		ctx->SLAblock[ctx->howManyInBlock] = 0x80;
		for (i = (ctx->howManyInBlock + 1);
		     i < sizeof(ctx->SLAblock);
		     i++) {
			ctx->SLAblock[i] = 0x0;
		}
		/* get last int spot */
		ctx->runningTotal *= 8;
		ptr = (unsigned int *) &ctx->SLAblock[60];
		*ptr = htonl(ctx->runningTotal);
		SLA1processABlock(ctx, (unsigned int *) ctx->SLAblock);
	}
	/* Now at this point all we need do is transfer the
	 * digest back to the user
	 */
	digestBuf[3] = (ctx->H0 & 0xff);
	digestBuf[2] = ((ctx->H0 >> 8) & 0xff);
	digestBuf[1] = ((ctx->H0 >> 16) & 0xff);
	digestBuf[0] = ((ctx->H0 >> 24) & 0xff);

	digestBuf[7] = (ctx->H1 & 0xff);
	digestBuf[6] = ((ctx->H1 >> 8) & 0xff);
	digestBuf[5] = ((ctx->H1 >> 16) & 0xff);
	digestBuf[4] = ((ctx->H1 >> 24) & 0xff);

	digestBuf[11] = (ctx->H2 & 0xff);
	digestBuf[10] = ((ctx->H2 >> 8) & 0xff);
	digestBuf[9] = ((ctx->H2 >> 16) & 0xff);
	digestBuf[8] = ((ctx->H2 >> 24) & 0xff);

	digestBuf[15] = (ctx->H3 & 0xff);
	digestBuf[14] = ((ctx->H3 >> 8) & 0xff);
	digestBuf[13] = ((ctx->H3 >> 16) & 0xff);
	digestBuf[12] = ((ctx->H3 >> 24) & 0xff);

	digestBuf[19] = (ctx->H4 & 0xff);
	digestBuf[18] = ((ctx->H4 >> 8) & 0xff);
	digestBuf[17] = ((ctx->H4 >> 16) & 0xff);
	digestBuf[16] = ((ctx->H4 >> 24) & 0xff);
}
