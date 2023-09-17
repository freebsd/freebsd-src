/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)random.c	8.2 (Berkeley) 5/19/95";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
#include "namespace.h"
#include <sys/param.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include "un-namespace.h"

#include "random.h"

/*
 * random.c:
 *
 * An improved random number generation package.  In addition to the standard
 * rand()/srand() like interface, this package also has a special state info
 * interface.  The initstate() routine is called with a seed, an array of
 * bytes, and a count of how many bytes are being passed in; this array is
 * then initialized to contain information for random number generation with
 * that much state information.  Good sizes for the amount of state
 * information are 32, 64, 128, and 256 bytes.  The state can be switched by
 * calling the setstate() routine with the same array as was initiallized
 * with initstate().  By default, the package runs with 128 bytes of state
 * information and generates far better random numbers than a linear
 * congruential generator.  If the amount of state information is less than
 * 32 bytes, a simple linear congruential R.N.G. is used.
 *
 * Internally, the state information is treated as an array of uint32_t's; the
 * zeroeth element of the array is the type of R.N.G. being used (small
 * integer); the remainder of the array is the state information for the
 * R.N.G.  Thus, 32 bytes of state information will give 7 ints worth of
 * state information, which will allow a degree seven polynomial.  (Note:
 * the zeroeth word of state information also has some other information
 * stored in it -- see setstate() for details).
 *
 * The random number generation technique is a linear feedback shift register
 * approach, employing trinomials (since there are fewer terms to sum up that
 * way).  In this approach, the least significant bit of all the numbers in
 * the state table will act as a linear feedback shift register, and will
 * have period 2^deg - 1 (where deg is the degree of the polynomial being
 * used, assuming that the polynomial is irreducible and primitive).  The
 * higher order bits will have longer periods, since their values are also
 * influenced by pseudo-random carries out of the lower bits.  The total
 * period of the generator is approximately deg*(2**deg - 1); thus doubling
 * the amount of state information has a vast influence on the period of the
 * generator.  Note: the deg*(2**deg - 1) is an approximation only good for
 * large deg, when the period of the shift is the dominant factor.
 * With deg equal to seven, the period is actually much longer than the
 * 7*(2**7 - 1) predicted by this formula.
 *
 * Modified 28 December 1994 by Jacob S. Rosenberg.
 * The following changes have been made:
 * All references to the type u_int have been changed to unsigned long.
 * All references to type int have been changed to type long.  Other
 * cleanups have been made as well.  A warning for both initstate and
 * setstate has been inserted to the effect that on Sparc platforms
 * the 'arg_state' variable must be forced to begin on word boundaries.
 * This can be easily done by casting a long integer array to char *.
 * The overall logic has been left STRICTLY alone.  This software was
 * tested on both a VAX and Sun SpacsStation with exactly the same
 * results.  The new version and the original give IDENTICAL results.
 * The new version is somewhat faster than the original.  As the
 * documentation says:  "By default, the package runs with 128 bytes of
 * state information and generates far better random numbers than a linear
 * congruential generator.  If the amount of state information is less than
 * 32 bytes, a simple linear congruential R.N.G. is used."  For a buffer of
 * 128 bytes, this new version runs about 19 percent faster and for a 16
 * byte buffer it is about 5 percent faster.
 */

#define NSHUFF 50       /* to drop some "seed -> 1st value" linearity */

static const int degrees[MAX_TYPES] =	{ DEG_0, DEG_1, DEG_2, DEG_3, DEG_4 };
static const int seps[MAX_TYPES] =	{ SEP_0, SEP_1, SEP_2, SEP_3, SEP_4 };
static const int breaks[MAX_TYPES] = {
	BREAK_0, BREAK_1, BREAK_2, BREAK_3, BREAK_4
};

/*
 * Initially, everything is set up as if from:
 *
 *	initstate(1, randtbl, 128);
 *
 * Note that this initialization takes advantage of the fact that srandom()
 * advances the front and rear pointers 10*rand_deg times, and hence the
 * rear pointer which starts at 0 will also end up at zero; thus the zeroeth
 * element of the state information, which contains info about the current
 * position of the rear pointer is just
 *
 *	MAX_TYPES * (rptr - state) + TYPE_3 == TYPE_3.
 */
static struct __random_state implicit = {
	.rst_randtbl = {
		TYPE_3,
		0x2cf41758, 0x27bb3711, 0x4916d4d1, 0x7b02f59f, 0x9b8e28eb, 0xc0e80269,
		0x696f5c16, 0x878f1ff5, 0x52d9c07f, 0x916a06cd, 0xb50b3a20, 0x2776970a,
		0xee4eb2a6, 0xe94640ec, 0xb1d65612, 0x9d1ed968, 0x1043f6b7, 0xa3432a76,
		0x17eacbb9, 0x3c09e2eb, 0x4f8c2b3,  0x708a1f57, 0xee341814, 0x95d0e4d2,
		0xb06f216c, 0x8bd2e72e, 0x8f7c38d7, 0xcfc6a8fc, 0x2a59495,  0xa20d2a69,
		0xe29d12d1
	},

	/*
	 * fptr and rptr are two pointers into the state info, a front and a rear
	 * pointer.  These two pointers are always rand_sep places aparts, as they
	 * cycle cyclically through the state information.  (Yes, this does mean we
	 * could get away with just one pointer, but the code for random() is more
	 * efficient this way).  The pointers are left positioned as they would be
	 * from the call
	 *
	 *	initstate(1, randtbl, 128);
	 *
	 * (The position of the rear pointer, rptr, is really 0 (as explained above
	 * in the initialization of randtbl) because the state table pointer is set
	 * to point to randtbl[1] (as explained below).
	 */
	.rst_fptr = &implicit.rst_randtbl[SEP_3 + 1],
	.rst_rptr = &implicit.rst_randtbl[1],

	/*
	 * The following things are the pointer to the state information table, the
	 * type of the current generator, the degree of the current polynomial being
	 * used, and the separation between the two pointers.  Note that for efficiency
	 * of random(), we remember the first location of the state information, not
	 * the zeroeth.  Hence it is valid to access state[-1], which is used to
	 * store the type of the R.N.G.  Also, we remember the last location, since
	 * this is more efficient than indexing every time to find the address of
	 * the last element to see if the front and rear pointers have wrapped.
	 */
	.rst_state = &implicit.rst_randtbl[1],
	.rst_type = TYPE_3,
	.rst_deg = DEG_3,
	.rst_sep = SEP_3,
	.rst_end_ptr = &implicit.rst_randtbl[DEG_3 + 1],
};

/*
 * This is the same low quality PRNG used in rand(3) in FreeBSD 12 and prior.
 * It may be sufficient for distributing bits and expanding a small seed
 * integer into a larger state.
 */
static inline uint32_t
parkmiller32(uint32_t ctx)
{
/*
 * Compute x = (7^5 * x) mod (2^31 - 1)
 * wihout overflowing 31 bits:
 *      (2^31 - 1) = 127773 * (7^5) + 2836
 * From "Random number generators: good ones are hard to find",
 * Park and Miller, Communications of the ACM, vol. 31, no. 10,
 * October 1988, p. 1195.
 */
	int32_t hi, lo, x;

	/* Transform to [1, 0x7ffffffe] range. */
	x = (ctx % 0x7ffffffe) + 1;
	hi = x / 127773;
	lo = x % 127773;
	x = 16807 * lo - 2836 * hi;
	if (x < 0)
		x += 0x7fffffff;
	/* Transform to [0, 0x7ffffffd] range. */
	return (x - 1);
}

/*
 * srandom:
 *
 * Initialize the random number generator based on the given seed.  If the
 * type is the trivial no-state-information type, just remember the seed.
 * Otherwise, initializes state[] based on the given "seed" via a linear
 * congruential generator.  Then, the pointers are set to known locations
 * that are exactly rand_sep places apart.  Lastly, it cycles the state
 * information a given number of times to get rid of any initial dependencies
 * introduced by the L.C.R.N.G.  Note that the initialization of randtbl[]
 * for default usage relies on values produced by this routine.
 */
void
srandom_r(struct __random_state *estate, unsigned x)
{
	int i, lim;

	estate->rst_state[0] = (uint32_t)x;
	if (estate->rst_type == TYPE_0)
		lim = NSHUFF;
	else {
		for (i = 1; i < estate->rst_deg; i++)
			estate->rst_state[i] =
			    parkmiller32(estate->rst_state[i - 1]);
		estate->rst_fptr = &estate->rst_state[estate->rst_sep];
		estate->rst_rptr = &estate->rst_state[0];
		lim = 10 * estate->rst_deg;
	}
	for (i = 0; i < lim; i++)
		(void)random_r(estate);
}

void
srandom(unsigned x)
{
	srandom_r(&implicit, x);
}

/*
 * srandomdev:
 *
 * Many programs choose the seed value in a totally predictable manner.
 * This often causes problems.  We seed the generator using pseudo-random
 * data from the kernel.
 *
 * Note that this particular seeding procedure can generate states
 * which are impossible to reproduce by calling srandom() with any
 * value, since the succeeding terms in the state buffer are no longer
 * derived from the LC algorithm applied to a fixed seed.
 */
void
srandomdev_r(struct __random_state *estate)
{
	int mib[2];
	size_t expected, len;

	if (estate->rst_type == TYPE_0)
		len = sizeof(estate->rst_state[0]);
	else
		len = estate->rst_deg * sizeof(estate->rst_state[0]);
	expected = len;

	mib[0] = CTL_KERN;
	mib[1] = KERN_ARND;
	if (sysctl(mib, 2, estate->rst_state, &len, NULL, 0) == -1 ||
	    len != expected) {
		/*
		 * The sysctl cannot fail. If it does fail on some FreeBSD
		 * derivative or after some future change, just abort so that
		 * the problem will be found and fixed. abort is not normally
		 * suitable for a library but makes sense here.
		 */
		abort();
	}

	if (estate->rst_type != TYPE_0) {
		estate->rst_fptr = &estate->rst_state[estate->rst_sep];
		estate->rst_rptr = &estate->rst_state[0];
	}
}

void
srandomdev(void)
{
	srandomdev_r(&implicit);
}

/*
 * initstate_r:
 *
 * Initialize the state information in the given array of n bytes for future
 * random number generation.  Based on the number of bytes we are given, and
 * the break values for the different R.N.G.'s, we choose the best (largest)
 * one we can and set things up for it.  srandom() is then called to
 * initialize the state information.
 *
 * Returns zero on success, or an error number on failure.
 *
 * Note: There is no need for a setstate_r(); just use a new context.
 */
int
initstate_r(struct __random_state *estate, unsigned seed, uint32_t *arg_state,
    size_t sz)
{
	if (sz < BREAK_0)
		return (EINVAL);

	if (sz < BREAK_1) {
		estate->rst_type = TYPE_0;
		estate->rst_deg = DEG_0;
		estate->rst_sep = SEP_0;
	} else if (sz < BREAK_2) {
		estate->rst_type = TYPE_1;
		estate->rst_deg = DEG_1;
		estate->rst_sep = SEP_1;
	} else if (sz < BREAK_3) {
		estate->rst_type = TYPE_2;
		estate->rst_deg = DEG_2;
		estate->rst_sep = SEP_2;
	} else if (sz < BREAK_4) {
		estate->rst_type = TYPE_3;
		estate->rst_deg = DEG_3;
		estate->rst_sep = SEP_3;
	} else {
		estate->rst_type = TYPE_4;
		estate->rst_deg = DEG_4;
		estate->rst_sep = SEP_4;
	}
	estate->rst_state = arg_state + 1;
	estate->rst_end_ptr = &estate->rst_state[estate->rst_deg];
	srandom_r(estate, seed);
	return (0);
}

/*
 * initstate:
 *
 * Note: the first thing we do is save the current state, if any, just like
 * setstate() so that it doesn't matter when initstate is called.
 *
 * Note that on return from initstate_r(), we set state[-1] to be the type
 * multiplexed with the current value of the rear pointer; this is so
 * successive calls to initstate() won't lose this information and will be able
 * to restart with setstate().
 *
 * Returns a pointer to the old state.
 *
 * Despite the misleading "char *" type, arg_state must alias an array of
 * 32-bit unsigned integer values.  Naturally, such an array is 32-bit aligned.
 * Usually objects are naturally aligned to at least 32-bits on all platforms,
 * but if you treat the provided 'state' as char* you may inadvertently
 * misalign it.  Don't do that.
 */
char *
initstate(unsigned int seed, char *arg_state, size_t n)
{
	char *ostate = (char *)(&implicit.rst_state[-1]);
	uint32_t *int_arg_state = (uint32_t *)arg_state;
	int error;

	/*
	 * Persist rptr offset and rst_type in the first word of the prior
	 * state we are replacing.
	 */
	if (implicit.rst_type == TYPE_0)
		implicit.rst_state[-1] = implicit.rst_type;
	else
		implicit.rst_state[-1] = MAX_TYPES *
		    (implicit.rst_rptr - implicit.rst_state) +
		    implicit.rst_type;

	error = initstate_r(&implicit, seed, int_arg_state, n);
	if (error != 0)
		return (NULL);

	/*
	 * Persist rptr offset and rst_type of the new state in its first word.
	 */
	if (implicit.rst_type == TYPE_0)
		int_arg_state[0] = implicit.rst_type;
	else
		int_arg_state[0] = MAX_TYPES *
		    (implicit.rst_rptr - implicit.rst_state) +
		    implicit.rst_type;

	return (ostate);
}

/*
 * setstate:
 *
 * Restore the state from the given state array.
 *
 * Note: it is important that we also remember the locations of the pointers
 * in the current state information, and restore the locations of the pointers
 * from the old state information.  This is done by multiplexing the pointer
 * location into the zeroeth word of the state information.
 *
 * Note that due to the order in which things are done, it is OK to call
 * setstate() with the same state as the current state.
 *
 * Returns a pointer to the old state information.
 *
 * Note: The Sparc platform requires that arg_state begin on an int
 * word boundary; otherwise a bus error will occur. Even so, lint will
 * complain about mis-alignment, but you should disregard these messages.
 */
char *
setstate(char *arg_state)
{
	uint32_t *new_state = (uint32_t *)arg_state;
	uint32_t type = new_state[0] % MAX_TYPES;
	uint32_t rear = new_state[0] / MAX_TYPES;
	char *ostate = (char *)(&implicit.rst_state[-1]);

	if (type != TYPE_0 && rear >= degrees[type])
		return (NULL);
	if (implicit.rst_type == TYPE_0)
		implicit.rst_state[-1] = implicit.rst_type;
	else
		implicit.rst_state[-1] = MAX_TYPES *
		    (implicit.rst_rptr - implicit.rst_state) +
		    implicit.rst_type;
	implicit.rst_type = type;
	implicit.rst_deg = degrees[type];
	implicit.rst_sep = seps[type];
	implicit.rst_state = new_state + 1;
	if (implicit.rst_type != TYPE_0) {
		implicit.rst_rptr = &implicit.rst_state[rear];
		implicit.rst_fptr = &implicit.rst_state[
		    (rear + implicit.rst_sep) % implicit.rst_deg];
	}
	implicit.rst_end_ptr = &implicit.rst_state[implicit.rst_deg];
	return (ostate);
}

/*
 * random:
 *
 * If we are using the trivial TYPE_0 R.N.G., just do the old linear
 * congruential bit.  Otherwise, we do our fancy trinomial stuff, which is
 * the same in all the other cases due to all the global variables that have
 * been set up.  The basic operation is to add the number at the rear pointer
 * into the one at the front pointer.  Then both pointers are advanced to
 * the next location cyclically in the table.  The value returned is the sum
 * generated, reduced to 31 bits by throwing away the "least random" low bit.
 *
 * Note: the code takes advantage of the fact that both the front and
 * rear pointers can't wrap on the same call by not testing the rear
 * pointer if the front one has wrapped.
 *
 * Returns a 31-bit random number.
 */
long
random_r(struct __random_state *estate)
{
	uint32_t i;
	uint32_t *f, *r;

	if (estate->rst_type == TYPE_0) {
		i = estate->rst_state[0];
		i = parkmiller32(i);
		estate->rst_state[0] = i;
	} else {
		/*
		 * Use local variables rather than static variables for speed.
		 */
		f = estate->rst_fptr;
		r = estate->rst_rptr;
		*f += *r;
		i = *f >> 1;	/* chucking least random bit */
		if (++f >= estate->rst_end_ptr) {
			f = estate->rst_state;
			++r;
		}
		else if (++r >= estate->rst_end_ptr) {
			r = estate->rst_state;
		}

		estate->rst_fptr = f;
		estate->rst_rptr = r;
	}
	return ((long)i);
}

long
random(void)
{
	return (random_r(&implicit));
}

struct __random_state *
allocatestate(unsigned type)
{
	size_t asize;

	/* No point using this interface to get the Park-Miller LCG. */
	if (type < TYPE_1)
		abort();
	/* Clamp to widest supported variant. */
	if (type > (MAX_TYPES - 1))
		type = (MAX_TYPES - 1);

	asize = sizeof(struct __random_state) + (size_t)breaks[type];
	return (malloc(asize));
}
