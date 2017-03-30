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
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/random.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/time.h>
#include <sys/smp.h>
#include <sys/malloc.h>

#define	ARC4_RESEED_BYTES 65536
#define	ARC4_RESEED_SECONDS 300
#define	ARC4_KEYBYTES 256

int arc4rand_iniseed_state = ARC4_ENTR_NONE;

MALLOC_DEFINE(M_ARC4RANDOM, "arc4random", "arc4random structures");

struct arc4_s {
	struct mtx mtx;
	u_int8_t i, j;
	int numruns;
	u_int8_t sbox[256];
	time_t t_reseed;

} __aligned(CACHE_LINE_SIZE);

static struct arc4_s *arc4inst = NULL;

#define ARC4_FOREACH(_arc4) \
	for (_arc4 = &arc4inst[0]; _arc4 <= &arc4inst[mp_maxid]; _arc4++)

static u_int8_t arc4_randbyte(struct arc4_s *arc4);

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
arc4_randomstir(struct arc4_s* arc4)
{
	u_int8_t key[ARC4_KEYBYTES];
	int n;
	struct timeval tv_now;

	/*
	 * XXX: FIX!! This isn't brilliant. Need more confidence.
	 * This returns zero entropy before random(4) is seeded.
	 */
	(void)read_random(key, ARC4_KEYBYTES);
	getmicrouptime(&tv_now);
	mtx_lock(&arc4->mtx);
	for (n = 0; n < 256; n++) {
		arc4->j = (arc4->j + arc4->sbox[n] + key[n]) % 256;
		arc4_swap(&arc4->sbox[n], &arc4->sbox[arc4->j]);
	}
	arc4->i = arc4->j = 0;
	/* Reset for next reseed cycle. */
	arc4->t_reseed = tv_now.tv_sec + ARC4_RESEED_SECONDS;
	arc4->numruns = 0;
	/*
	 * Throw away the first N words of output, as suggested in the
	 * paper "Weaknesses in the Key Scheduling Algorithm of RC4"
	 * by Fluher, Mantin, and Shamir.  (N = 768 in our case.)
	 *
	 * http://dl.acm.org/citation.cfm?id=646557.694759
	 */
	for (n = 0; n < 768*4; n++)
		arc4_randbyte(arc4);

	mtx_unlock(&arc4->mtx);
}

/*
 * Initialize our S-box to its beginning defaults.
 */
static void
arc4_init(void)
{
	struct arc4_s *arc4;
	int n;

	arc4inst = malloc((mp_maxid + 1) * sizeof(struct arc4_s),
			M_ARC4RANDOM, M_NOWAIT | M_ZERO);
	KASSERT(arc4inst != NULL, ("arc4_init: memory allocation error"));

	ARC4_FOREACH(arc4) {
		mtx_init(&arc4->mtx, "arc4_mtx", NULL, MTX_DEF);

		arc4->i = arc4->j = 0;
		for (n = 0; n < 256; n++)
			arc4->sbox[n] = (u_int8_t) n;

		arc4->t_reseed = -1;
		arc4->numruns = 0;
	}
}
SYSINIT(arc4, SI_SUB_LOCK, SI_ORDER_ANY, arc4_init, NULL);


static void
arc4_uninit(void)
{
	struct arc4_s *arc4;

	ARC4_FOREACH(arc4) {
		mtx_destroy(&arc4->mtx);
	}

	free(arc4inst, M_ARC4RANDOM);
}

SYSUNINIT(arc4, SI_SUB_LOCK, SI_ORDER_ANY, arc4_uninit, NULL);


/*
 * Generate a random byte.
 */
static u_int8_t
arc4_randbyte(struct arc4_s *arc4)
{
	u_int8_t arc4_t;

	arc4->i = (arc4->i + 1) % 256;
	arc4->j = (arc4->j + arc4->sbox[arc4->i]) % 256;

	arc4_swap(&arc4->sbox[arc4->i], &arc4->sbox[arc4->j]);

	arc4_t = (arc4->sbox[arc4->i] + arc4->sbox[arc4->j]) % 256;
	return arc4->sbox[arc4_t];
}

/*
 * MPSAFE
 */
void
arc4rand(void *ptr, u_int len, int reseed)
{
	u_char *p;
	struct timeval tv;
	struct arc4_s *arc4;

	if (reseed || atomic_cmpset_int(&arc4rand_iniseed_state,
			ARC4_ENTR_HAVE, ARC4_ENTR_SEED)) {
		ARC4_FOREACH(arc4)
			arc4_randomstir(arc4);
	}

	arc4 = &arc4inst[curcpu];
	getmicrouptime(&tv);
	if ((arc4->numruns > ARC4_RESEED_BYTES) ||
		(tv.tv_sec > arc4->t_reseed))
		arc4_randomstir(arc4);

	mtx_lock(&arc4->mtx);
	arc4->numruns += len;
	p = ptr;
	while (len--)
		*p++ = arc4_randbyte(arc4);
	mtx_unlock(&arc4->mtx);
}

uint32_t
arc4random(void)
{
	uint32_t ret;

	arc4rand(&ret, sizeof ret, 0);
	return ret;
}
