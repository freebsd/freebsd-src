/*
 * random.c -- A strong random number generator
 *
 * $Id: random.c,v 1.2 1995/11/04 16:00:50 markm Exp $
 *
 * Version 0.92, last modified 21-Sep-95
 * 
 * Copyright Theodore Ts'o, 1994, 1995.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <i386/isa/isa.h>
#include <i386/isa/icu.h>
#include <i386/isa/timerreg.h>
#include <i386/isa/isa_device.h>
#include <machine/random.h>

#define RANDPOOL 512

struct random_bucket {
	int add_ptr;
	int entropy_count;
	int length;
	int bit_length;
	int delay_mix:1;
	u_int8_t *pool;
};

struct timer_rand_state {
	u_int32_t	last_time;
	int 		last_delta;
	int 		nbits;
};

static struct random_bucket random_state;
static u_int32_t rand_pool_key[16];
static u_int8_t random_pool[RANDPOOL];
static u_int32_t random_counter[16];
static struct timer_rand_state keyboard_timer_state;
static struct timer_rand_state irq_timer_state[ICU_LEN];

inthand2_t add_interrupt_randomness;
u_int16_t interrupt_allowed = 0;
	
#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif
	
static void
flush_random(struct random_bucket *random_state)
{
	random_state->add_ptr = 0;
	random_state->bit_length = random_state->length * 8;
	random_state->entropy_count = 0;
	random_state->delay_mix = 0;
}

void
rand_initialize(void)
{

	random_state.length = RANDPOOL;
	random_state.pool = random_pool;
	flush_random(&random_state);

#if 0
	{
	int irq;
	long interrupts;
	/* XXX Dreadful hack - should be replaced by something more elegant */
	interrupts = RANDOM_INTERRUPTS;

	for (irq = 0; irq < ICU_LEN; irq++) {
		interrupt_allowed[irq] = interrupts & 0x0001;
		interrupts >>= 1;
		printf("Randomising irq %d %s\n", irq, interrupt_allowed[irq] ?
			"on" : "off");
	}
	}
#endif
}

/*
 * MD5 transform algorithm, taken from code written by Colin Plumb,
 * and put into the public domain
 */

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void
MD5Transform(u_int32_t buf[4], u_int32_t const in[16])
{
	u_int32_t a, b, c, d;

	a = buf[0];
	b = buf[1];
	c = buf[2];
	d = buf[3];

	MD5STEP(F1, a, b, c, d, in[ 0]+0xd76aa478,  7);
	MD5STEP(F1, d, a, b, c, in[ 1]+0xe8c7b756, 12);
	MD5STEP(F1, c, d, a, b, in[ 2]+0x242070db, 17);
	MD5STEP(F1, b, c, d, a, in[ 3]+0xc1bdceee, 22);
	MD5STEP(F1, a, b, c, d, in[ 4]+0xf57c0faf,  7);
	MD5STEP(F1, d, a, b, c, in[ 5]+0x4787c62a, 12);
	MD5STEP(F1, c, d, a, b, in[ 6]+0xa8304613, 17);
	MD5STEP(F1, b, c, d, a, in[ 7]+0xfd469501, 22);
	MD5STEP(F1, a, b, c, d, in[ 8]+0x698098d8,  7);
	MD5STEP(F1, d, a, b, c, in[ 9]+0x8b44f7af, 12);
	MD5STEP(F1, c, d, a, b, in[10]+0xffff5bb1, 17);
	MD5STEP(F1, b, c, d, a, in[11]+0x895cd7be, 22);
	MD5STEP(F1, a, b, c, d, in[12]+0x6b901122,  7);
	MD5STEP(F1, d, a, b, c, in[13]+0xfd987193, 12);
	MD5STEP(F1, c, d, a, b, in[14]+0xa679438e, 17);
	MD5STEP(F1, b, c, d, a, in[15]+0x49b40821, 22);

	MD5STEP(F2, a, b, c, d, in[ 1]+0xf61e2562,  5);
	MD5STEP(F2, d, a, b, c, in[ 6]+0xc040b340,  9);
	MD5STEP(F2, c, d, a, b, in[11]+0x265e5a51, 14);
	MD5STEP(F2, b, c, d, a, in[ 0]+0xe9b6c7aa, 20);
	MD5STEP(F2, a, b, c, d, in[ 5]+0xd62f105d,  5);
	MD5STEP(F2, d, a, b, c, in[10]+0x02441453,  9);
	MD5STEP(F2, c, d, a, b, in[15]+0xd8a1e681, 14);
	MD5STEP(F2, b, c, d, a, in[ 4]+0xe7d3fbc8, 20);
	MD5STEP(F2, a, b, c, d, in[ 9]+0x21e1cde6,  5);
	MD5STEP(F2, d, a, b, c, in[14]+0xc33707d6,  9);
	MD5STEP(F2, c, d, a, b, in[ 3]+0xf4d50d87, 14);
	MD5STEP(F2, b, c, d, a, in[ 8]+0x455a14ed, 20);
	MD5STEP(F2, a, b, c, d, in[13]+0xa9e3e905,  5);
	MD5STEP(F2, d, a, b, c, in[ 2]+0xfcefa3f8,  9);
	MD5STEP(F2, c, d, a, b, in[ 7]+0x676f02d9, 14);
	MD5STEP(F2, b, c, d, a, in[12]+0x8d2a4c8a, 20);

	MD5STEP(F3, a, b, c, d, in[ 5]+0xfffa3942,  4);
	MD5STEP(F3, d, a, b, c, in[ 8]+0x8771f681, 11);
	MD5STEP(F3, c, d, a, b, in[11]+0x6d9d6122, 16);
	MD5STEP(F3, b, c, d, a, in[14]+0xfde5380c, 23);
	MD5STEP(F3, a, b, c, d, in[ 1]+0xa4beea44,  4);
	MD5STEP(F3, d, a, b, c, in[ 4]+0x4bdecfa9, 11);
	MD5STEP(F3, c, d, a, b, in[ 7]+0xf6bb4b60, 16);
	MD5STEP(F3, b, c, d, a, in[10]+0xbebfbc70, 23);
	MD5STEP(F3, a, b, c, d, in[13]+0x289b7ec6,  4);
	MD5STEP(F3, d, a, b, c, in[ 0]+0xeaa127fa, 11);
	MD5STEP(F3, c, d, a, b, in[ 3]+0xd4ef3085, 16);
	MD5STEP(F3, b, c, d, a, in[ 6]+0x04881d05, 23);
	MD5STEP(F3, a, b, c, d, in[ 9]+0xd9d4d039,  4);
	MD5STEP(F3, d, a, b, c, in[12]+0xe6db99e5, 11);
	MD5STEP(F3, c, d, a, b, in[15]+0x1fa27cf8, 16);
	MD5STEP(F3, b, c, d, a, in[ 2]+0xc4ac5665, 23);

	MD5STEP(F4, a, b, c, d, in[ 0]+0xf4292244,  6);
	MD5STEP(F4, d, a, b, c, in[ 7]+0x432aff97, 10);
	MD5STEP(F4, c, d, a, b, in[14]+0xab9423a7, 15);
	MD5STEP(F4, b, c, d, a, in[ 5]+0xfc93a039, 21);
	MD5STEP(F4, a, b, c, d, in[12]+0x655b59c3,  6);
	MD5STEP(F4, d, a, b, c, in[ 3]+0x8f0ccc92, 10);
	MD5STEP(F4, c, d, a, b, in[10]+0xffeff47d, 15);
	MD5STEP(F4, b, c, d, a, in[ 1]+0x85845dd1, 21);
	MD5STEP(F4, a, b, c, d, in[ 8]+0x6fa87e4f,  6);
	MD5STEP(F4, d, a, b, c, in[15]+0xfe2ce6e0, 10);
	MD5STEP(F4, c, d, a, b, in[ 6]+0xa3014314, 15);
	MD5STEP(F4, b, c, d, a, in[13]+0x4e0811a1, 21);
	MD5STEP(F4, a, b, c, d, in[ 4]+0xf7537e82,  6);
	MD5STEP(F4, d, a, b, c, in[11]+0xbd3af235, 10);
	MD5STEP(F4, c, d, a, b, in[ 2]+0x2ad7d2bb, 15);
	MD5STEP(F4, b, c, d, a, in[ 9]+0xeb86d391, 21);

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}

#undef F1
#undef F2
#undef F3
#undef F4
#undef MD5STEP

static void
mix_bucket(struct random_bucket *v)
{
	struct random_bucket *r = v;
	int	i, num_passes;
	u_int32_t *p;
	u_int32_t iv[4];

	r->delay_mix = 0;
	
	/* Start IV from last block of the random pool */
	memcpy(iv, r->pool + r->length - sizeof(iv), sizeof(iv));

	num_passes = r->length / 16;
	for (i = 0, p = (u_int32_t *) r->pool; i < num_passes; i++) {
		MD5Transform(iv, rand_pool_key);
		iv[0] = (*p++ ^= iv[0]);
		iv[1] = (*p++ ^= iv[1]);
		iv[2] = (*p++ ^= iv[2]);
		iv[3] = (*p++ ^= iv[3]);
	}
	memcpy(rand_pool_key, r->pool, sizeof(rand_pool_key));
	
	/* Wipe iv from memory */
	bzero(iv, sizeof(iv));

	r->add_ptr = 0;
}

/*
 * This function adds a byte into the entropy "pool".  It does not
 * update the entropy estimate.  The caller must do this if appropriate.
 */
static inline void
add_entropy_byte(struct random_bucket *r, const u_int8_t ch, int delay)
{
	if (!delay && r->delay_mix)
		mix_bucket(r);
	r->pool[r->add_ptr++] ^= ch;
	if (r->add_ptr >= r->length) {
		if (delay) {
			r->delay_mix = 1;
			r->add_ptr = 0;
		} else
			mix_bucket(r);
	}
}

/*
 * This function adds some number of bytes into the entropy pool and
 * updates the entropy count as appropriate.
 */
static void
add_entropy(struct random_bucket *r, const u_int8_t *ptr, int length,
	int entropy_level, int delay)
{
	while (length-- > 0)
		add_entropy_byte(r, *ptr++, delay);
		
	r->entropy_count += entropy_level;
	if (r->entropy_count > r->length*8)
		r->entropy_count = r->length * 8;
}

/*
 * This function adds entropy to the entropy "pool" by using timing
 * delays.  It uses the timer_rand_state structure to make an estimate
 * of how many bits of entropy this call has added to the pool.
 */
static void
add_timer_randomness(struct random_bucket *r, struct timer_rand_state *state,
	int delay)
{
	int	delta, delta2;
	int	nbits;

	/*
	 * Calculate number of bits of randomness we probably
	 * added.  We take into account the first and second order
	 * delta's in order to make our estimate.
	 */
	delta = ticks - state->last_time;
	delta2 = delta - state->last_delta;
	state->last_time = ticks;
	state->last_delta = delta;
	if (delta < 0) delta = -delta;
	if (delta2 < 0) delta2 = -delta2;
	delta = MIN(delta, delta2) >> 1;
	for (nbits = 0; delta; nbits++)
		delta >>= 1;
	
	add_entropy(r, (u_int8_t *) &ticks, sizeof(ticks), nbits, delay);

#if defined (__i386__)
	/*
	 * On a 386, read the high resolution timer.  We assume that
	 * this gives us 2 bits of randomness.  XXX This needs
	 * investigation.
	 */ 
	outb(TIMER_LATCH|TIMER_SEL0, TIMER_MODE); /* latch the count ASAP */
	add_entropy_byte(r, inb(TIMER_CNTR0), 1);
	add_entropy_byte(r, inb(TIMER_CNTR0), 1);
	r->entropy_count += 2;
	if (r->entropy_count > r->bit_length)
		r->entropy_count = r->bit_length;
#endif
}

void
add_keyboard_randomness(u_char scancode)
{
	struct random_bucket *r = &random_state;

	add_timer_randomness(r, &keyboard_timer_state, 0);
	add_entropy_byte(r, scancode, 0);
	r->entropy_count += 6;
	if (r->entropy_count > r->bit_length)
		r->entropy_count = r->bit_length;
}

void
add_interrupt_randomness(int irq)
{
	static struct random_bucket *r = &random_state;
	u_int16_t intbit = 1 << irq;

/*	printf("Trapping interrupt %d\n", irq); */

	if (interrupt_allowed & intbit) 
		add_timer_randomness(r, &irq_timer_state[irq], 1);
}

/*
 * This function extracts randomness from the "entropy pool", and
 * returns it in a buffer.  This function computes how many remaining
 * bits of entropy are left in the pool, but it does not restrict the
 * number of bytes that are actually obtained.
 */
static inline u_int
extract_entropy(struct random_bucket *r, char *buf, u_int nbytes)
{
	int passes, i;
	u_int length, ret;
	u_int32_t tmp[4];
	u_int8_t *cp;
	
	add_entropy(r, (u_int8_t *) &ticks, sizeof(ticks), 0, 0);
	
	if (r->entropy_count > r->bit_length) 
		r->entropy_count = r->bit_length;
	if (nbytes > 32768)
		nbytes = 32768;
	ret = nbytes;
	r->entropy_count -= ret * 8;
	if (r->entropy_count < 0)
		r->entropy_count = 0;
	passes = r->length / 64;
	while (nbytes) {
		length = MIN(nbytes, 16);
		for (i=0; i < 16; i++) {
			if (++random_counter[i] != 0)
				break;
		}
		tmp[0] = 0x67452301;
		tmp[1] = 0xefcdab89;
		tmp[2] = 0x98badcfe;
		tmp[3] = 0x10325476;
		MD5Transform(tmp, random_counter);
		for (i = 0, cp = r->pool; i < passes; i++, cp+=64)
			MD5Transform(tmp, (u_int32_t *) cp);
		memcpy(buf, tmp, length);
		nbytes -= length;
		buf += length;
	}
	return ret;
}

/*
 * This function is the exported kernel interface.  It returns some
 * number of good random numbers, suitable for seeding TCP sequence
 * numbers, etc.
 */
void
get_random_bytes(void *buf, u_int nbytes)
{
	extract_entropy(&random_state, (char *) buf, nbytes);
}

u_int
read_random(char * buf, u_int nbytes)
{
	if ((nbytes * 8) > random_state.entropy_count)
		nbytes = random_state.entropy_count / 8;
	
	return extract_entropy(&random_state, buf, nbytes);
}

u_int
read_random_unlimited(char * buf, u_int nbytes)
{
	return extract_entropy(&random_state, buf, nbytes);
}
