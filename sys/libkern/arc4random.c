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

#include <sys/types.h>
#include <sys/random.h>
#include <sys/libkern.h>
#include <sys/time.h>

#define	ARC4_MAXRUNS 16384
#define	ARC4_RESEED_SECONDS 300
#define	ARC4_KEYBYTES 32 /* 256 bit key */

static u_int8_t arc4_i, arc4_j;
static int arc4_initialized = 0;
static int arc4_numruns = 0;
static u_int8_t arc4_sbox[256];
static struct timeval arc4_tv_nextreseed;

static u_int8_t arc4_randbyte(void);

static __inline void
arc4_swap(u_int8_t *a, u_int8_t *b)
{
	u_int8_t c;

	c = *a;
	*a = *b;
	*b = c;
}	

/*
 * Stir our S-box.
 */
static void
arc4_randomstir (void)
{
	u_int8_t key[256];
	int r, n;

	/*
	 * XXX read_random() returns unsafe numbers if the entropy
	 * device is not loaded -- MarkM.
	 */
	r = read_random_unlimited(key, ARC4_KEYBYTES);
	/* If r == 0 || -1, just use what was on the stack. */
	if (r > 0)
	{
		for (n = r; n < sizeof(key); n++)
			key[n] = key[n % r];
	}

	for (n = 0; n < 256; n++)
	{
		arc4_j = (arc4_j + arc4_sbox[n] + key[n]) % 256;
		arc4_swap(&arc4_sbox[n], &arc4_sbox[arc4_j]);
	}

	/* Reset for next reseed cycle. */
	getmicrotime(&arc4_tv_nextreseed);
	arc4_tv_nextreseed.tv_sec += ARC4_RESEED_SECONDS;
	arc4_numruns = 0;
}

/*
 * Initialize our S-box to its beginning defaults.
 */
static void
arc4_init(void)
{
	int n;

	arc4_i = arc4_j = 0;
	for (n = 0; n < 256; n++)
		arc4_sbox[n] = (u_int8_t) n;

	arc4_randomstir();
	arc4_initialized = 1;

	/*
	 * Throw away the first N words of output, as suggested in the
	 * paper "Weaknesses in the Key Scheduling Algorithm of RC4"
	 * by Fluher, Mantin, and Shamir.  (N = 256 in our case.)
	 */
	for (n = 0; n < 256*4; n++)
		arc4_randbyte();
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
	struct timeval tv_now;

	/* Initialize array if needed. */
	if (!arc4_initialized)
		arc4_init();

	getmicrotime(&tv_now);
	if ((++arc4_numruns > ARC4_MAXRUNS) || 
	    (tv_now.tv_sec > arc4_tv_nextreseed.tv_sec))
	{
		arc4_randomstir();
	}

	ret = arc4_randbyte();
	ret |= arc4_randbyte() << 8;
	ret |= arc4_randbyte() << 16;
	ret |= arc4_randbyte() << 24;

	return ret;
}
