/*
 * random_machdep.c -- A strong random number generator
 *
 * $Id: random_machdep.c,v 1.24 1998/04/06 09:30:32 phk Exp $
 *
 * Version 0.95, last modified 18-Oct-95
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/md5.h>

#include <machine/random.h>

#include <i386/isa/icu.h>

#define MAX_BLKDEV 4

/*
 * The pool is stirred with a primitive polynomial of degree 128
 * over GF(2), namely x^128 + x^99 + x^59 + x^31 + x^9 + x^7 + 1.
 * For a pool of size 64, try x^64+x^62+x^38+x^10+x^6+x+1.
 */
#define POOLWORDS 128    /* Power of 2 - note that this is 32-bit words */
#define POOLBITS (POOLWORDS*32)

#if POOLWORDS == 128
#define TAP1    99     /* The polynomial taps */
#define TAP2    59
#define TAP3    31
#define TAP4    9
#define TAP5    7
#elif POOLWORDS == 64
#define TAP1    62      /* The polynomial taps */
#define TAP2    38
#define TAP3    10
#define TAP4    6
#define TAP5    1
#else
#error No primitive polynomial available for chosen POOLWORDS
#endif

#define WRITEBUFFER 512 /* size in bytes */

/* There is actually only one of these, globally. */
struct random_bucket {
	u_int	add_ptr;
	u_int	entropy_count;
	int	input_rotate;
	u_int32_t *pool;
	struct	selinfo rsel;
};

/* There is one of these per entropy source */
struct timer_rand_state {
	u_long	last_time;
	int 	last_delta;
	int 	nbits;
};

static struct random_bucket random_state;
static u_int32_t random_pool[POOLWORDS];
static struct timer_rand_state keyboard_timer_state;
static struct timer_rand_state extract_timer_state;
static struct timer_rand_state irq_timer_state[ICU_LEN];
#ifdef notyet
static struct timer_rand_state blkdev_timer_state[MAX_BLKDEV];
#endif
static struct wait_queue *random_wait;

inthand2_t *sec_intr_handler[ICU_LEN];
int sec_intr_unit[ICU_LEN];

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif
	
void
rand_initialize(void)
{
	random_state.add_ptr = 0;
	random_state.entropy_count = 0;
	random_state.pool = random_pool;
	random_wait = NULL;
	random_state.rsel.si_flags = 0;
	random_state.rsel.si_pid = 0;
}

/*
 * This function adds an int into the entropy "pool".  It does not
 * update the entropy estimate.  The caller must do this if appropriate.
 *
 * The pool is stirred with a primitive polynomial of degree 128
 * over GF(2), namely x^128 + x^99 + x^59 + x^31 + x^9 + x^7 + 1.
 * For a pool of size 64, try x^64+x^62+x^38+x^10+x^6+x+1.
 * 
 * We rotate the input word by a changing number of bits, to help
 * assure that all bits in the entropy get toggled.  Otherwise, if we
 * consistently feed the entropy pool small numbers (like ticks and
 * scancodes, for example), the upper bits of the entropy pool don't
 * get affected. --- TYT, 10/11/95
 */
static __inline void
add_entropy_word(struct random_bucket *r, const u_int32_t input)
{
	u_int i;
	u_int32_t w;

	w = (input << r->input_rotate) | (input >> (32 - r->input_rotate));
	i = r->add_ptr = (r->add_ptr - 1) & (POOLWORDS-1);
	if (i)
		r->input_rotate = (r->input_rotate + 7) & 31;
	else
		/*
		 * At the beginning of the pool, add an extra 7 bits
		 * rotation, so that successive passes spread the
		 * input bits across the pool evenly.
		 */
		r->input_rotate = (r->input_rotate + 14) & 31;

	/* XOR in the various taps */
	w ^= r->pool[(i+TAP1)&(POOLWORDS-1)];
	w ^= r->pool[(i+TAP2)&(POOLWORDS-1)];
	w ^= r->pool[(i+TAP3)&(POOLWORDS-1)];
	w ^= r->pool[(i+TAP4)&(POOLWORDS-1)];
	w ^= r->pool[(i+TAP5)&(POOLWORDS-1)];
	w ^= r->pool[i];
	/* Rotate w left 1 bit (stolen from SHA) and store */
	r->pool[i] = (w << 1) | (w >> 31);
}

/*
 * This function adds entropy to the entropy "pool" by using timing
 * delays.  It uses the timer_rand_state structure to make an estimate
 * of how  any bits of entropy this call has added to the pool.
 *
 * The number "num" is also added to the pool - it should somehow describe
 * the type of event which just happened.  This is currently 0-255 for
 * keyboard scan codes, and 256 upwards for interrupts.
 * On the i386, this is assumed to be at most 16 bits, and the high bits
 * are used for a high-resolution timer.
 */
static void
add_timer_randomness(struct random_bucket *r, struct timer_rand_state *state,
	u_int num)
{
	int		delta, delta2;
	u_int		nbits;
	u_int32_t	time;

	num ^= timecounter->get_timecount() << 16;
	r->entropy_count += 2;
		
	time = ticks;

	add_entropy_word(r, (u_int32_t) num);
	add_entropy_word(r, time);

	/*
	 * Calculate number of bits of randomness we probably
	 * added.  We take into account the first and second order
	 * deltas in order to make our estimate.
	 */
	delta = time - state->last_time;
	state->last_time = time;

	delta2 = delta - state->last_delta;
	state->last_delta = delta;

	if (delta < 0) delta = -delta;
	if (delta2 < 0) delta2 = -delta2;
	delta = MIN(delta, delta2) >> 1;
	for (nbits = 0; delta; nbits++)
		delta >>= 1;

	r->entropy_count += nbits;
	
	/* Prevent overflow */
	if (r->entropy_count > POOLBITS)
		r->entropy_count = POOLBITS;

	if (r->entropy_count >= 8)
		selwakeup(&random_state.rsel);
}

void
add_keyboard_randomness(u_char scancode)
{
	add_timer_randomness(&random_state, &keyboard_timer_state, scancode);
}

void
add_interrupt_randomness(int irq)
{
	(sec_intr_handler[irq])(sec_intr_unit[irq]);
	add_timer_randomness(&random_state, &irq_timer_state[irq], irq);
}

#ifdef notused
void
add_blkdev_randomness(int major)
{
	if (major >= MAX_BLKDEV)
		return;

	add_timer_randomness(&random_state, &blkdev_timer_state[major],
			     0x200+major);
}
#endif /* notused */

#if POOLWORDS % 16
#error extract_entropy() assumes that POOLWORDS is a multiple of 16 words.
#endif
/*
 * This function extracts randomness from the "entropy pool", and
 * returns it in a buffer.  This function computes how many remaining
 * bits of entropy are left in the pool, but it does not restrict the
 * number of bytes that are actually obtained.
 */
static __inline int
extract_entropy(struct random_bucket *r, char *buf, int nbytes)
{
	int ret, i;
	u_int32_t tmp[4];
	
	add_timer_randomness(r, &extract_timer_state, nbytes);
	
	/* Redundant, but just in case... */
	if (r->entropy_count > POOLBITS) 
		r->entropy_count = POOLBITS;
	/* Why is this here?  Left in from Ted Ts'o.  Perhaps to limit time. */
	if (nbytes > 32768)
		nbytes = 32768;

	ret = nbytes;
	if (r->entropy_count / 8 >= nbytes)
		r->entropy_count -= nbytes*8;
	else
		r->entropy_count = 0;

	while (nbytes) {
		/* Hash the pool to get the output */
		tmp[0] = 0x67452301;
		tmp[1] = 0xefcdab89;
		tmp[2] = 0x98badcfe;
		tmp[3] = 0x10325476;
		for (i = 0; i < POOLWORDS; i += 16)
			MD5Transform(tmp, (char *)(r->pool+i));
		/* Modify pool so next hash will produce different results */
		add_entropy_word(r, tmp[0]);
		add_entropy_word(r, tmp[1]);
		add_entropy_word(r, tmp[2]);
		add_entropy_word(r, tmp[3]);
		/*
		 * Run the MD5 Transform one more time, since we want
		 * to add at least minimal obscuring of the inputs to
		 * add_entropy_word().  --- TYT
		 */
		MD5Transform(tmp, (char *)(r->pool));
		
		/* Copy data to destination buffer */
		i = MIN(nbytes, 16);
		bcopy(tmp, buf, i);
		nbytes -= i;
		buf += i;
	}

	/* Wipe data from memory */
	bzero(tmp, sizeof(tmp));
	
	return ret;
}

#ifdef notused /* XXX NOT the exported kernel interface */
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
#endif /* notused */

u_int
read_random(void *buf, u_int nbytes)
{
	if ((nbytes * 8) > random_state.entropy_count)
		nbytes = random_state.entropy_count / 8;
	
	return extract_entropy(&random_state, (char *)buf, nbytes);
}

u_int
read_random_unlimited(void *buf, u_int nbytes)
{
	return extract_entropy(&random_state, (char *)buf, nbytes);
}

#ifdef notused
u_int
write_random(const char *buf, u_int nbytes)
{
	u_int i;
	u_int32_t word, *p;

	for (i = nbytes, p = (u_int32_t *)buf;
	     i >= sizeof(u_int32_t);
	     i-= sizeof(u_int32_t), p++)
		add_entropy_word(&random_state, *p);
	if (i) {
		word = 0;
		bcopy(p, &word, i);
		add_entropy_word(&random_state, word);
	}
	return nbytes;
}
#endif /* notused */

int
random_poll(dev_t dev, int events, struct proc *p)
{
	int s;
	int revents = 0;

	s = splhigh();
	if (events & (POLLIN | POLLRDNORM))
		if (random_state.entropy_count >= 8)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &random_state.rsel);

	splx(s);
	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);	/* heh */

	return (revents);
}

