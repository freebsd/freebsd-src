/*-
 * Copyright (c) 2000 Mark R V Murray
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
 * $FreeBSD$
 */

#define ENTROPYBIN	256	/* buckets to harvest entropy events */
#define ENTROPYSOURCE	2	/* entropy sources (actually classes)    */
				/* The entropy classes will as follows:  */
				/*    0 - Keyboard                       */
				/*    1 - Mouse                          */
				/* to start with. More will be added     */

#define TIMEBIN		16	/* max value for Pt/t */
#define KEYSIZE		32	/* 32 bytes == 256 bits */

#define FAST		0
#define SLOW		1

void random_init(void);
void random_deinit(void);
void random_init_harvester(void (*)(struct timespec *, u_int64_t, u_int, u_int, u_int));
void random_deinit_harvester(void);

/* This is the beasite that needs protecting. It contains all of the
 * state that we are excited about.
 * This is a biiig structure. It may move over to a malloc(9)ed
 * replacement.
 */
struct random_state {
	u_int64_t counter;	/* C */
	BF_KEY key;		/* K */
	int gengateinterval;	/* Pg */
	int bins;		/* Pt/t */
	u_char ivec[8];		/* Blowfish internal */
	int outputblocks;	/* count output blocks for gates */
	u_int slowoverthresh;	/* slow pool overthreshhold reseed count */
	struct pool {
		struct source {
			struct entropy {
				struct timespec	nanotime;
				u_int64_t data;
			} entropy[ENTROPYBIN];	/* entropy units - must each
					   	be <= KEYSIZE */
			u_int bits;	/* estimated bits of entropy */
			u_int frac;	/* fractional bits of entropy
					   (given as 1024/n) */
			u_int current;	/* next insertion point */
		} source[ENTROPYSOURCE];
		u_int thresh;	/* pool reseed threshhold */
	} pool[2];		/* pool[0] is fast, pool[1] is slow */
	int which;		/* toggle - shows the current insertion pool */
};

extern struct random_state random_state;
