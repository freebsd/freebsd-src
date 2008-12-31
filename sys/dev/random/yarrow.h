/*-
 * Copyright (c) 2000-2004 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/random/yarrow.h,v 1.18.26.1 2008/11/25 02:59:29 kensmith Exp $
 */

/* This contains Yarrow-specific declarations.
 * See http://www.counterpane.com/yarrow.html
 */

#define TIMEBIN		16	/* max value for Pt/t */

#define FAST		0
#define SLOW		1

/* This is the beastie that needs protecting. It contains all of the
 * state that we are excited about.
 * Exactly one will be instantiated.
 */
struct random_state {
	u_int64_t counter[4];	/* C - 256 bits */
	struct yarrowkey key;	/* K */
	u_int gengateinterval;	/* Pg */
	u_int bins;		/* Pt/t */
	u_int outputblocks;	/* count output blocks for gates */
	u_int slowoverthresh;	/* slow pool overthreshhold reseed count */
	struct pool {
		struct source {
			u_int bits;	/* estimated bits of entropy */
			u_int frac;	/* fractional bits of entropy
					   (given as 1024/n) */
		} source[ENTROPYSOURCE];
		u_int thresh;	/* pool reseed threshhold */
		struct yarrowhash hash;	/* accumulated entropy */
	} pool[2];		/* pool[0] is fast, pool[1] is slow */
	u_int which;		/* toggle - sets the current insertion pool */
};
