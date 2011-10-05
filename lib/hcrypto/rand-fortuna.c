/*
 * fortuna.c
 *		Fortuna-like PRNG.
 *
 * Copyright (c) 2005 Marko Kreen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $PostgreSQL: pgsql/contrib/pgcrypto/fortuna.c,v 1.8 2006/10/04 00:29:46 momjian Exp $
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <rand.h>
#include <heim_threads.h>

#ifdef KRB5
#include <krb5-types.h>
#endif
#include <roken.h>

#include "randi.h"
#include "aes.h"
#include "sha.h"

/*
 * Why Fortuna-like: There does not seem to be any definitive reference
 * on Fortuna in the net.  Instead this implementation is based on
 * following references:
 *
 * http://en.wikipedia.org/wiki/Fortuna_(PRNG)
 *	 - Wikipedia article
 * http://jlcooke.ca/random/
 *	 - Jean-Luc Cooke Fortuna-based /dev/random driver for Linux.
 */

/*
 * There is some confusion about whether and how to carry forward
 * the state of the pools.	Seems like original Fortuna does not
 * do it, resetting hash after each request.  I guess expecting
 * feeding to happen more often that requesting.   This is absolutely
 * unsuitable for pgcrypto, as nothing asynchronous happens here.
 *
 * J.L. Cooke fixed this by feeding previous hash to new re-initialized
 * hash context.
 *
 * Fortuna predecessor Yarrow requires ability to query intermediate
 * 'final result' from hash, without affecting it.
 *
 * This implementation uses the Yarrow method - asking intermediate
 * results, but continuing with old state.
 */


/*
 * Algorithm parameters
 */

#define NUM_POOLS		32

/* in microseconds */
#define RESEED_INTERVAL 100000	/* 0.1 sec */

/* for one big request, reseed after this many bytes */
#define RESEED_BYTES	(1024*1024)

/*
 * Skip reseed if pool 0 has less than this many
 * bytes added since last reseed.
 */
#define POOL0_FILL		(256/8)

/*
 * Algorithm constants
 */

/* Both cipher key size and hash result size */
#define BLOCK			32

/* cipher block size */
#define CIPH_BLOCK		16

/* for internal wrappers */
#define MD_CTX			SHA256_CTX
#define CIPH_CTX		AES_KEY

struct fortuna_state
{
    unsigned char	counter[CIPH_BLOCK];
    unsigned char	result[CIPH_BLOCK];
    unsigned char	key[BLOCK];
    MD_CTX		pool[NUM_POOLS];
    CIPH_CTX		ciph;
    unsigned		reseed_count;
    struct timeval	last_reseed_time;
    unsigned		pool0_bytes;
    unsigned		rnd_pos;
    int			tricks_done;
    pid_t		pid;
};
typedef struct fortuna_state FState;


/*
 * Use our own wrappers here.
 * - Need to get intermediate result from digest, without affecting it.
 * - Need re-set key on a cipher context.
 * - Algorithms are guaranteed to exist.
 * - No memory allocations.
 */

static void
ciph_init(CIPH_CTX * ctx, const unsigned char *key, int klen)
{
    AES_set_encrypt_key(key, klen * 8, ctx);
}

static void
ciph_encrypt(CIPH_CTX * ctx, const unsigned char *in, unsigned char *out)
{
    AES_encrypt(in, out, ctx);
}

static void
md_init(MD_CTX * ctx)
{
    SHA256_Init(ctx);
}

static void
md_update(MD_CTX * ctx, const unsigned char *data, int len)
{
    SHA256_Update(ctx, data, len);
}

static void
md_result(MD_CTX * ctx, unsigned char *dst)
{
    SHA256_CTX	tmp;

    memcpy(&tmp, ctx, sizeof(*ctx));
    SHA256_Final(dst, &tmp);
    memset(&tmp, 0, sizeof(tmp));
}

/*
 * initialize state
 */
static void
init_state(FState * st)
{
    int			i;

    memset(st, 0, sizeof(*st));
    for (i = 0; i < NUM_POOLS; i++)
	md_init(&st->pool[i]);
    st->pid = getpid();
}

/*
 * Endianess does not matter.
 * It just needs to change without repeating.
 */
static void
inc_counter(FState * st)
{
    uint32_t   *val = (uint32_t *) st->counter;

    if (++val[0])
	return;
    if (++val[1])
	return;
    if (++val[2])
	return;
    ++val[3];
}

/*
 * This is called 'cipher in counter mode'.
 */
static void
encrypt_counter(FState * st, unsigned char *dst)
{
    ciph_encrypt(&st->ciph, st->counter, dst);
    inc_counter(st);
}


/*
 * The time between reseed must be at least RESEED_INTERVAL
 * microseconds.
 */
static int
enough_time_passed(FState * st)
{
    int			ok;
    struct timeval tv;
    struct timeval *last = &st->last_reseed_time;

    gettimeofday(&tv, NULL);

    /* check how much time has passed */
    ok = 0;
    if (tv.tv_sec > last->tv_sec + 1)
	ok = 1;
    else if (tv.tv_sec == last->tv_sec + 1)
    {
	if (1000000 + tv.tv_usec - last->tv_usec >= RESEED_INTERVAL)
	    ok = 1;
    }
    else if (tv.tv_usec - last->tv_usec >= RESEED_INTERVAL)
	ok = 1;

    /* reseed will happen, update last_reseed_time */
    if (ok)
	memcpy(last, &tv, sizeof(tv));

    memset(&tv, 0, sizeof(tv));

    return ok;
}

/*
 * generate new key from all the pools
 */
static void
reseed(FState * st)
{
    unsigned	k;
    unsigned	n;
    MD_CTX		key_md;
    unsigned char	buf[BLOCK];

    /* set pool as empty */
    st->pool0_bytes = 0;

    /*
     * Both #0 and #1 reseed would use only pool 0. Just skip #0 then.
     */
    n = ++st->reseed_count;

    /*
     * The goal: use k-th pool only 1/(2^k) of the time.
     */
    md_init(&key_md);
    for (k = 0; k < NUM_POOLS; k++)
    {
	md_result(&st->pool[k], buf);
	md_update(&key_md, buf, BLOCK);

	if (n & 1 || !n)
	    break;
	n >>= 1;
    }

    /* add old key into mix too */
    md_update(&key_md, st->key, BLOCK);

    /* add pid to make output diverse after fork() */
    md_update(&key_md, (const unsigned char *)&st->pid, sizeof(st->pid));

    /* now we have new key */
    md_result(&key_md, st->key);

    /* use new key */
    ciph_init(&st->ciph, st->key, BLOCK);

    memset(&key_md, 0, sizeof(key_md));
    memset(buf, 0, BLOCK);
}

/*
 * Pick a random pool.	This uses key bytes as random source.
 */
static unsigned
get_rand_pool(FState * st)
{
    unsigned	rnd;

    /*
     * This slightly prefers lower pools - thats OK.
     */
    rnd = st->key[st->rnd_pos] % NUM_POOLS;

    st->rnd_pos++;
    if (st->rnd_pos >= BLOCK)
	st->rnd_pos = 0;

    return rnd;
}

/*
 * update pools
 */
static void
add_entropy(FState * st, const unsigned char *data, unsigned len)
{
    unsigned		pos;
    unsigned char	hash[BLOCK];
    MD_CTX		md;

    /* hash given data */
    md_init(&md);
    md_update(&md, data, len);
    md_result(&md, hash);

    /*
     * Make sure the pool 0 is initialized, then update randomly.
     */
    if (st->reseed_count == 0)
	pos = 0;
    else
	pos = get_rand_pool(st);
    md_update(&st->pool[pos], hash, BLOCK);

    if (pos == 0)
	st->pool0_bytes += len;

    memset(hash, 0, BLOCK);
    memset(&md, 0, sizeof(md));
}

/*
 * Just take 2 next blocks as new key
 */
static void
rekey(FState * st)
{
    encrypt_counter(st, st->key);
    encrypt_counter(st, st->key + CIPH_BLOCK);
    ciph_init(&st->ciph, st->key, BLOCK);
}

/*
 * Hide public constants. (counter, pools > 0)
 *
 * This can also be viewed as spreading the startup
 * entropy over all of the components.
 */
static void
startup_tricks(FState * st)
{
    int			i;
    unsigned char	buf[BLOCK];

    /* Use next block as counter. */
    encrypt_counter(st, st->counter);

    /* Now shuffle pools, excluding #0 */
    for (i = 1; i < NUM_POOLS; i++)
    {
	encrypt_counter(st, buf);
	encrypt_counter(st, buf + CIPH_BLOCK);
	md_update(&st->pool[i], buf, BLOCK);
    }
    memset(buf, 0, BLOCK);

    /* Hide the key. */
    rekey(st);

    /* This can be done only once. */
    st->tricks_done = 1;
}

static void
extract_data(FState * st, unsigned count, unsigned char *dst)
{
    unsigned	n;
    unsigned	block_nr = 0;
    pid_t	pid = getpid();

    /* Should we reseed? */
    if (st->pool0_bytes >= POOL0_FILL || st->reseed_count == 0)
	if (enough_time_passed(st))
	    reseed(st);

    /* Do some randomization on first call */
    if (!st->tricks_done)
	startup_tricks(st);

    /* If we forked, force a reseed again */
    if (pid != st->pid) {
	st->pid = pid;
	reseed(st);
    }

    while (count > 0)
    {
	/* produce bytes */
	encrypt_counter(st, st->result);

	/* copy result */
	if (count > CIPH_BLOCK)
	    n = CIPH_BLOCK;
	else
	    n = count;
	memcpy(dst, st->result, n);
	dst += n;
	count -= n;

	/* must not give out too many bytes with one key */
	block_nr++;
	if (block_nr > (RESEED_BYTES / CIPH_BLOCK))
	{
	    rekey(st);
	    block_nr = 0;
	}
    }
    /* Set new key for next request. */
    rekey(st);
}

/*
 * public interface
 */

static FState	main_state;
static int	init_done;
static int	have_entropy;
#define FORTUNA_RESEED_BYTE	10000
static unsigned	resend_bytes;

/*
 * This mutex protects all of the above static elements from concurrent
 * access by multiple threads
 */
static HEIMDAL_MUTEX fortuna_mutex = HEIMDAL_MUTEX_INITIALIZER;

/*
 * Try our best to do an inital seed
 */
#define INIT_BYTES	128

/*
 * fortuna_mutex must be held across calls to this function
 */

static int
fortuna_reseed(void)
{
    int entropy_p = 0;

    if (!init_done)
	abort();

#ifndef NO_RAND_UNIX_METHOD
    {
	unsigned char buf[INIT_BYTES];
	if ((*hc_rand_unix_method.bytes)(buf, sizeof(buf)) == 1) {
	    add_entropy(&main_state, buf, sizeof(buf));
	    entropy_p = 1;
	    memset(buf, 0, sizeof(buf));
	}
    }
#endif
#ifdef HAVE_ARC4RANDOM
    {
	uint32_t buf[INIT_BYTES / sizeof(uint32_t)];
	int i;

	for (i = 0; i < sizeof(buf)/sizeof(buf[0]); i++)
	    buf[i] = arc4random();
	add_entropy(&main_state, (void *)buf, sizeof(buf));
	entropy_p = 1;
    }
#endif
#ifndef NO_RAND_EGD_METHOD
    /*
     * Only to get egd entropy if /dev/random or arc4rand failed since
     * it can be horribly slow to generate new bits.
     */
    if (!entropy_p) {
	unsigned char buf[INIT_BYTES];
	if ((*hc_rand_egd_method.bytes)(buf, sizeof(buf)) == 1) {
	    add_entropy(&main_state, buf, sizeof(buf));
	    entropy_p = 1;
	    memset(buf, 0, sizeof(buf));
	}
    }
#endif
    /*
     * Fall back to gattering data from timer and secret files, this
     * is really the last resort.
     */
    if (!entropy_p) {
	/* to save stackspace */
	union {
	    unsigned char buf[INIT_BYTES];
	    unsigned char shad[1001];
	} u;
	int fd;

	/* add timer info */
	if ((*hc_rand_timer_method.bytes)(u.buf, sizeof(u.buf)) == 1)
	    add_entropy(&main_state, u.buf, sizeof(u.buf));
	/* add /etc/shadow */
	fd = open("/etc/shadow", O_RDONLY, 0);
	if (fd >= 0) {
	    ssize_t n;
	    rk_cloexec(fd);
	    /* add_entropy will hash the buf */
	    while ((n = read(fd, (char *)u.shad, sizeof(u.shad))) > 0)
		add_entropy(&main_state, u.shad, sizeof(u.shad));
	    close(fd);
	}

	memset(&u, 0, sizeof(u));

	entropy_p = 1; /* sure about this ? */
    }
    {
	pid_t pid = getpid();
	add_entropy(&main_state, (void *)&pid, sizeof(pid));
    }
    {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	add_entropy(&main_state, (void *)&tv, sizeof(tv));
    }
#ifdef HAVE_GETUID
    {
	uid_t u = getuid();
	add_entropy(&main_state, (void *)&u, sizeof(u));
    }
#endif
    return entropy_p;
}

/*
 * fortuna_mutex must be held by callers of this function
 */
static int
fortuna_init(void)
{
    if (!init_done)
    {
	init_state(&main_state);
	init_done = 1;
    }
    if (!have_entropy)
	have_entropy = fortuna_reseed();
    return (init_done && have_entropy);
}



static void
fortuna_seed(const void *indata, int size)
{
    HEIMDAL_MUTEX_lock(&fortuna_mutex);

    fortuna_init();
    add_entropy(&main_state, indata, size);
    if (size >= INIT_BYTES)
	have_entropy = 1;

    HEIMDAL_MUTEX_unlock(&fortuna_mutex);
}

static int
fortuna_bytes(unsigned char *outdata, int size)
{
    int ret = 0;

    HEIMDAL_MUTEX_lock(&fortuna_mutex);

    if (!fortuna_init())
	goto out;

    resend_bytes += size;
    if (resend_bytes > FORTUNA_RESEED_BYTE || resend_bytes < size) {
	resend_bytes = 0;
	fortuna_reseed();
    }
    extract_data(&main_state, size, outdata);
    ret = 1;

out:
    HEIMDAL_MUTEX_unlock(&fortuna_mutex);

    return ret;
}

static void
fortuna_cleanup(void)
{
    HEIMDAL_MUTEX_lock(&fortuna_mutex);

    init_done = 0;
    have_entropy = 0;
    memset(&main_state, 0, sizeof(main_state));

    HEIMDAL_MUTEX_unlock(&fortuna_mutex);
}

static void
fortuna_add(const void *indata, int size, double entropi)
{
    fortuna_seed(indata, size);
}

static int
fortuna_pseudorand(unsigned char *outdata, int size)
{
    return fortuna_bytes(outdata, size);
}

static int
fortuna_status(void)
{
    int result;

    HEIMDAL_MUTEX_lock(&fortuna_mutex);
    result = fortuna_init();
    HEIMDAL_MUTEX_unlock(&fortuna_mutex);

    return result ? 1 : 0;
}

const RAND_METHOD hc_rand_fortuna_method = {
    fortuna_seed,
    fortuna_bytes,
    fortuna_cleanup,
    fortuna_add,
    fortuna_pseudorand,
    fortuna_status
};

const RAND_METHOD *
RAND_fortuna_method(void)
{
    return &hc_rand_fortuna_method;
}
