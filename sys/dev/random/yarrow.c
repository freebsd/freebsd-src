/*-
 * Copyright (c) 2000 Mark Murray
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

/* NOTE NOTE NOTE - This is not finished! It will supply numbers, but
                    it is not yet cryptographically secure!! */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/linker.h>
#include <sys/libkern.h>
#include <sys/mbuf.h>
#include <sys/random.h>
#include <sys/types.h>
#include <crypto/blowfish/blowfish.h>

#include <dev/randomdev/yarrow.h>

void generator_gate(void);
void reseed(void);
void randominit(void);

/* This is the beastie that needs protecting. It contains all of the
 * state that we are excited about.
 */
struct state state;

void
randominit(void)
{
	/* XXX much more to come */
	state.gengateinterval = 10;
}

void
reseed(void)
{
	unsigned char v[BINS][KEYSIZE];	/* v[i] */
	unsigned char hash[KEYSIZE];	/* h' */
	BF_KEY hashkey;
	unsigned char ivec[8];
	unsigned char temp[KEYSIZE];
	int i, j;

	/* 1. Hash the accumulated entropy into v[0] */

	/* XXX to be done properly */
	bzero((void *)&v[0], KEYSIZE);
	for (j = 0; j < sizeof(state.randomstuff); j += KEYSIZE) {
		BF_set_key(&hashkey, KEYSIZE, &state.randomstuff[j]);
		BF_cbc_encrypt(v[0], temp, KEYSIZE, &hashkey,
					ivec, BF_ENCRYPT);
		memcpy(&v[0], temp, KEYSIZE);
	}

	/* 2. Compute hash values for all v. _Supposed_ to be computationally */
	/*    intensive.                                                      */

	for (i = 1; i < BINS; i++) {
		bzero((void *)&v[i], KEYSIZE);
		for (j = 0; j < sizeof(state.randomstuff); j += KEYSIZE) {
			/* v[i] #= h(v[i-1]) */
			BF_set_key(&hashkey, KEYSIZE, v[i - 1]);
			BF_cbc_encrypt(v[i], temp, KEYSIZE, &hashkey,
						ivec, BF_ENCRYPT);
			memcpy(&v[i], temp, KEYSIZE);
			/* v[i] #= h(v[0]) */
			BF_set_key(&hashkey, KEYSIZE, v[0]);
			BF_cbc_encrypt(v[i], temp, KEYSIZE, &hashkey,
						ivec, BF_ENCRYPT);
			memcpy(&v[i], temp, KEYSIZE);
			/* v[i] #= h(i) */
			BF_set_key(&hashkey, sizeof(int), (unsigned char *)&i);
			BF_cbc_encrypt(v[i], temp, KEYSIZE, &hashkey,
						ivec, BF_ENCRYPT);
			memcpy(&v[i], temp, KEYSIZE);
		}
	}

	/* 3. Compute a new Key. */

	bzero((void *)hash, KEYSIZE);
	BF_set_key(&hashkey, KEYSIZE, (unsigned char *)&state.key);
	BF_cbc_encrypt(hash, temp, KEYSIZE, &hashkey,
				ivec, BF_ENCRYPT);
	memcpy(hash, temp, KEYSIZE);
	for (i = 1; i < BINS; i++) {
		BF_set_key(&hashkey, KEYSIZE, v[i]);
		BF_cbc_encrypt(hash, temp, KEYSIZE, &hashkey,
					ivec, BF_ENCRYPT);
		memcpy(hash, temp, KEYSIZE);
	}

	BF_set_key(&state.key, KEYSIZE, hash);

	/* 4. Recompute the counter */

	state.counter = 0;
	BF_cbc_encrypt((unsigned char *)&state.counter, temp,
		sizeof(state.counter), &state.key, state.ivec,
		BF_ENCRYPT);
	memcpy(&state.counter, temp, state.counter);

	/* 5. Reset all entropy estimate accumulators to zero */

	bzero((void *)state.randomstuff, sizeof(state.randomstuff));

	/* 6. Wipe memory of intermediate values */

	bzero((void *)v, sizeof(v));
	bzero((void *)temp, sizeof(temp));
	bzero((void *)hash, sizeof(hash));

	/* 7. Dump to seed file (XXX done by external process?) */

}

u_int
read_random(char *buf, u_int count)
{
	static int cur = 0;
	static int gate = 1;
	u_int i;
	u_int retval;
	u_int64_t genval;

	if (gate) {
		generator_gate();
		state.outputblocks = 0;
		gate = 0;
	}
	if (count >= sizeof(state.counter)) {
		retval = 0;
		for (i = 0; i < count; i += sizeof(state.counter)) {
			state.counter++;
			BF_cbc_encrypt((unsigned char *)&state.counter,
				(unsigned char *)&genval, sizeof(state.counter),
				&state.key, state.ivec, BF_ENCRYPT);
			memcpy(&buf[i], &genval, sizeof(state.counter));
			if (++state.outputblocks >= state.gengateinterval) {
				generator_gate();
				state.outputblocks = 0;
			}
			retval += sizeof(state.counter);
		}
	}
	else {
		if (!cur) {
			state.counter++;
			BF_cbc_encrypt((unsigned char *)&state.counter,
				(unsigned char *)&genval, sizeof(state.counter),
				&state.key, state.ivec, BF_ENCRYPT);
			memcpy(buf, &genval, count);
			cur = sizeof(state.counter) - count;
			if (++state.outputblocks >= state.gengateinterval) {
				generator_gate();
				state.outputblocks = 0;
			}
			retval = count;
		}
		else {
			retval = cur < count ? cur : count;
			memcpy(buf,
				(char *)&state.counter +
					(sizeof(state.counter) - retval),
				retval);
			cur -= retval;
		}
	}
	return retval;
}

void
generator_gate(void)
{
	int i;
	unsigned char temp[KEYSIZE];

	for (i = 0; i < KEYSIZE; i += sizeof(state.counter)) {
		state.counter++;
		BF_cbc_encrypt((unsigned char *)&state.counter, &temp[i],
			sizeof(state.counter), &state.key, state.ivec,
			BF_ENCRYPT);
	}

	BF_set_key(&state.key, KEYSIZE, temp);
	bzero((void *)temp, KEYSIZE);
}
