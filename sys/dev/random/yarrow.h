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

/* #define ENTROPYSOURCE nn	   entropy sources (actually classes)
 *				   The entropy classes will as follows:
 *					0 - Direct write
 *					1 - Keyboard
 *					2 - Mouse
 */

#define ENTROPYBIN	256	/* buckets to harvest entropy events  */
#define TIMEBIN		16	/* max value for Pt/t */

#define HARVESTSIZE	16	/* max size of each harvested entropy unit */

#define FAST		0
#define SLOW		1

int random_init(void);
void random_deinit(void);
void random_init_harvester(void (*)(u_int64_t, void *, u_int, u_int, u_int, enum esource), u_int (*)(void *, u_int));
void random_deinit_harvester(void);
void random_set_wakeup_exit(void *);

u_int read_random_real(void *, u_int);
void write_random(void *, u_int);

/* This is the beastie that needs protecting. It contains all of the
 * state that we are excited about.
 */
struct random_state {
	u_int64_t counter;	/* C */
	struct yarrowkey key;	/* K */
	int gengateinterval;	/* Pg */
	int bins;		/* Pt/t */
	int outputblocks;	/* count output blocks for gates */
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
	int which;		/* toggle - shows the current insertion pool */
	int seeded;		/* 0 until first reseed, then 1 */
	struct selinfo rsel;	/* For poll(2) */
};

extern struct random_state random_state;
