/*-
 * THE BEER-WARE LICENSE
 *
 * <dan@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff.  If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *
 * Dan Moschuk
 *
 * $FreeBSD$
 */

#include <sys/libkern.h>
#include <sys/time.h>

static u_int8_t arc4_i, arc4_j;
static int arc4_initialized = 0;
static u_int8_t arc4_sbox[256];

static __inline void
arc4_swap(u_int8_t *a, u_int8_t *b)
{
	u_int8_t c;

	c = *a;
	*a = *b;
	*b = c;
}	

/*
 * Initialize our S-box to its beginning defaults.
 */
static void
arc4_init(void)
{
	struct timespec ts;
	u_int8_t key[256];
	int n;

	for (n = 0; n < 256; n++)
		arc4_sbox[n] = (u_int8_t) n;

	nanotime(&ts);
	srandom(ts.tv_sec ^ ts.tv_nsec);
	for (n = 0; n < 256; n++)
		key[n] = random() % 256;

	arc4_i = arc4_j = 0;
	for (n = 0; n < 256; n++)
	{
		arc4_j = arc4_j + arc4_sbox[n] + key[n];
		arc4_swap(&arc4_sbox[n], &arc4_sbox[arc4_j]);
	}
	arc4_initialized = 1;
}

/*
 * Generate a random byte.
 */
static u_int8_t
arc4_randbyte(void)
{
	u_int8_t arc4_t;

	arc4_i = (arc4_i + 1) % 256;
	arc4_j = (arc4_j + arc4_sbox[arc4_i]) % 256;

	arc4_swap(&arc4_sbox[arc4_i], &arc4_sbox[arc4_j]);

	arc4_t = (arc4_sbox[arc4_i] + arc4_sbox[arc4_j]) % 256;
	return arc4_sbox[arc4_t];
}

u_int32_t
arc4random(void)
{
	u_int32_t ret;

	/* Initialize array if needed. */
	if (!arc4_initialized)
		arc4_init();

	ret = arc4_randbyte();
	ret |= arc4_randbyte() << 8;
	ret |= arc4_randbyte() << 16;
	ret |= arc4_randbyte() << 24;

	return ret;
}
