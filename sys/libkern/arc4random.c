/*-
 * THE BEER-WARE LICENSE
 *
 * <dan@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff.  If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *
 * Dan Moschuk
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/libkern/arc4random.c,v 1.12 2003/08/15 06:34:47 silby Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/random.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/time.h>

#define	ARC4_RESEED_BYTES 65536
#define	ARC4_RESEED_SECONDS 300
#define	ARC4_KEYBYTES (256 / 8)

static u_int8_t arc4_i, arc4_j;
static int arc4_numruns = 0;
static u_int8_t arc4_sbox[256];
static time_t arc4_t_reseed;
static struct mtx arc4_mtx;

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
	struct timeval tv_now;

	/*
	 * XXX read_random() returns unsafe numbers if the entropy
	 * device is not loaded -- MarkM.
	 */
	r = read_random(key, ARC4_KEYBYTES);
	getmicrouptime(&tv_now);
	mtx_lock(&arc4_mtx);
	/* If r == 0 || -1, just use what was on the stack. */
	if (r > 0) {
		for (n = r; n < sizeof(key); n++)
			key[n] = key[n % r];
	}

	for (n = 0; n < 256; n++) {
		arc4_j = (arc4_j + arc4_sbox[n] + key[n]) % 256;
		arc4_swap(&arc4_sbox[n], &arc4_sbox[arc4_j]);
	}

	/* Reset for next reseed cycle. */
	arc4_t_reseed = tv_now.tv_sec + ARC4_RESEED_SECONDS;
	arc4_numruns = 0;

	/*
	 * Throw away the first N words of output, as suggested in the
	 * paper "Weaknesses in the Key Scheduling Algorithm of RC4"
	 * by Fluher, Mantin, and Shamir.  (N = 256 in our case.)
	 */
	for (n = 0; n < 256*4; n++)
		arc4_randbyte();
	mtx_unlock(&arc4_mtx);
}

/*
 * Initialize our S-box to its beginning defaults.
 */
static void
arc4_init(void)
{
	int n;

	mtx_init(&arc4_mtx, "arc4_mtx", NULL, MTX_DEF);
	arc4_i = arc4_j = 0;
	for (n = 0; n < 256; n++)
		arc4_sbox[n] = (u_int8_t) n;

	arc4_t_reseed = 0;
}

SYSINIT(arc4_init, SI_SUB_LOCK, SI_ORDER_ANY, arc4_init, NULL);

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

/*
 * MPSAFE
 */
void
arc4rand(void *ptr, u_int len, int reseed)
{
	u_char *p;
	struct timeval tv;

	getmicrouptime(&tv);
	if (reseed || 
	   (arc4_numruns > ARC4_RESEED_BYTES) ||
	   (tv.tv_sec > arc4_t_reseed))
		arc4_randomstir();

	mtx_lock(&arc4_mtx);
	arc4_numruns += len;
	p = ptr;
	while (len--)
		*p++ = arc4_randbyte();
	mtx_unlock(&arc4_mtx);
}

uint32_t
arc4random(void)
{
	uint32_t ret;

	arc4rand(&ret, sizeof ret, 0);
	return ret;
}
