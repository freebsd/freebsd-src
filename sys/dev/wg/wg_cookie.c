/* SPDX-License-Identifier: ISC
 *
 * Copyright (C) 2015-2021 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 * Copyright (C) 2019-2021 Matt Dunwoodie <ncon@noconroy.net>
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <crypto/siphash/siphash.h>
#include <netinet/in.h>
#include <vm/uma.h>

#include "wg_cookie.h"

#define COOKIE_MAC1_KEY_LABEL	"mac1----"
#define COOKIE_COOKIE_KEY_LABEL	"cookie--"
#define COOKIE_SECRET_MAX_AGE	120
#define COOKIE_SECRET_LATENCY	5

/* Constants for initiation rate limiting */
#define RATELIMIT_SIZE		(1 << 13)
#define RATELIMIT_MASK		(RATELIMIT_SIZE - 1)
#define RATELIMIT_SIZE_MAX	(RATELIMIT_SIZE * 8)
#define INITIATIONS_PER_SECOND	20
#define INITIATIONS_BURSTABLE	5
#define INITIATION_COST		(SBT_1S / INITIATIONS_PER_SECOND)
#define TOKEN_MAX		(INITIATION_COST * INITIATIONS_BURSTABLE)
#define ELEMENT_TIMEOUT		1
#define IPV4_MASK_SIZE		4 /* Use all 4 bytes of IPv4 address */
#define IPV6_MASK_SIZE		8 /* Use top 8 bytes (/64) of IPv6 address */

struct ratelimit_key {
	struct vnet *vnet;
	uint8_t ip[IPV6_MASK_SIZE];
};

struct ratelimit_entry {
	LIST_ENTRY(ratelimit_entry)	r_entry;
	struct ratelimit_key		r_key;
	sbintime_t			r_last_time;	/* sbinuptime */
	uint64_t			r_tokens;
};

struct ratelimit {
	uint8_t				rl_secret[SIPHASH_KEY_LENGTH];
	struct mtx			rl_mtx;
	struct callout			rl_gc;
	LIST_HEAD(, ratelimit_entry)	rl_table[RATELIMIT_SIZE];
	size_t				rl_table_num;
};

static void	precompute_key(uint8_t *,
			const uint8_t[COOKIE_INPUT_SIZE], const char *);
static void	macs_mac1(struct cookie_macs *, const void *, size_t,
			const uint8_t[COOKIE_KEY_SIZE]);
static void	macs_mac2(struct cookie_macs *, const void *, size_t,
			const uint8_t[COOKIE_COOKIE_SIZE]);
static int	timer_expired(sbintime_t, uint32_t, uint32_t);
static void	make_cookie(struct cookie_checker *,
			uint8_t[COOKIE_COOKIE_SIZE], struct sockaddr *);
static void	ratelimit_init(struct ratelimit *);
static void	ratelimit_deinit(struct ratelimit *);
static void	ratelimit_gc_callout(void *);
static void	ratelimit_gc_schedule(struct ratelimit *);
static void	ratelimit_gc(struct ratelimit *, bool);
static int	ratelimit_allow(struct ratelimit *, struct sockaddr *, struct vnet *);
static uint64_t siphash13(const uint8_t [SIPHASH_KEY_LENGTH], const void *, size_t);

static struct ratelimit ratelimit_v4;
#ifdef INET6
static struct ratelimit ratelimit_v6;
#endif
static uma_zone_t ratelimit_zone;

/* Public Functions */
int
cookie_init(void)
{
	if ((ratelimit_zone = uma_zcreate("wg ratelimit",
	    sizeof(struct ratelimit_entry), NULL, NULL, NULL, NULL, 0, 0)) == NULL)
		return ENOMEM;

	ratelimit_init(&ratelimit_v4);
#ifdef INET6
	ratelimit_init(&ratelimit_v6);
#endif
	return (0);
}

void
cookie_deinit(void)
{
	ratelimit_deinit(&ratelimit_v4);
#ifdef INET6
	ratelimit_deinit(&ratelimit_v6);
#endif
	uma_zdestroy(ratelimit_zone);
}

void
cookie_checker_init(struct cookie_checker *cc)
{
	bzero(cc, sizeof(*cc));

	rw_init(&cc->cc_key_lock, "cookie_checker_key");
	mtx_init(&cc->cc_secret_mtx, "cookie_checker_secret", NULL, MTX_DEF);
}

void
cookie_checker_free(struct cookie_checker *cc)
{
	rw_destroy(&cc->cc_key_lock);
	mtx_destroy(&cc->cc_secret_mtx);
	explicit_bzero(cc, sizeof(*cc));
}

void
cookie_checker_update(struct cookie_checker *cc,
    const uint8_t key[COOKIE_INPUT_SIZE])
{
	rw_wlock(&cc->cc_key_lock);
	if (key) {
		precompute_key(cc->cc_mac1_key, key, COOKIE_MAC1_KEY_LABEL);
		precompute_key(cc->cc_cookie_key, key, COOKIE_COOKIE_KEY_LABEL);
	} else {
		bzero(cc->cc_mac1_key, sizeof(cc->cc_mac1_key));
		bzero(cc->cc_cookie_key, sizeof(cc->cc_cookie_key));
	}
	rw_wunlock(&cc->cc_key_lock);
}

void
cookie_checker_create_payload(struct cookie_checker *cc,
    struct cookie_macs *macs, uint8_t nonce[COOKIE_NONCE_SIZE],
    uint8_t ecookie[COOKIE_ENCRYPTED_SIZE], struct sockaddr *sa)
{
	uint8_t cookie[COOKIE_COOKIE_SIZE];

	make_cookie(cc, cookie, sa);
	arc4random_buf(nonce, COOKIE_NONCE_SIZE);

	rw_rlock(&cc->cc_key_lock);
	xchacha20poly1305_encrypt(ecookie, cookie, COOKIE_COOKIE_SIZE,
	    macs->mac1, COOKIE_MAC_SIZE, nonce, cc->cc_cookie_key);
	rw_runlock(&cc->cc_key_lock);

	explicit_bzero(cookie, sizeof(cookie));
}

void
cookie_maker_init(struct cookie_maker *cm, const uint8_t key[COOKIE_INPUT_SIZE])
{
	bzero(cm, sizeof(*cm));
	precompute_key(cm->cm_mac1_key, key, COOKIE_MAC1_KEY_LABEL);
	precompute_key(cm->cm_cookie_key, key, COOKIE_COOKIE_KEY_LABEL);
	rw_init(&cm->cm_lock, "cookie_maker");
}

void
cookie_maker_free(struct cookie_maker *cm)
{
	rw_destroy(&cm->cm_lock);
	explicit_bzero(cm, sizeof(*cm));
}

int
cookie_maker_consume_payload(struct cookie_maker *cm,
    uint8_t nonce[COOKIE_NONCE_SIZE], uint8_t ecookie[COOKIE_ENCRYPTED_SIZE])
{
	uint8_t cookie[COOKIE_COOKIE_SIZE];
	int ret;

	rw_rlock(&cm->cm_lock);
	if (!cm->cm_mac1_sent) {
		ret = ETIMEDOUT;
		goto error;
	}

	if (!xchacha20poly1305_decrypt(cookie, ecookie, COOKIE_ENCRYPTED_SIZE,
	    cm->cm_mac1_last, COOKIE_MAC_SIZE, nonce, cm->cm_cookie_key)) {
		ret = EINVAL;
		goto error;
	}
	rw_runlock(&cm->cm_lock);

	rw_wlock(&cm->cm_lock);
	memcpy(cm->cm_cookie, cookie, COOKIE_COOKIE_SIZE);
	cm->cm_cookie_birthdate = getsbinuptime();
	cm->cm_cookie_valid = true;
	cm->cm_mac1_sent = false;
	rw_wunlock(&cm->cm_lock);

	return 0;
error:
	rw_runlock(&cm->cm_lock);
	return ret;
}

void
cookie_maker_mac(struct cookie_maker *cm, struct cookie_macs *macs, void *buf,
    size_t len)
{
	rw_wlock(&cm->cm_lock);
	macs_mac1(macs, buf, len, cm->cm_mac1_key);
	memcpy(cm->cm_mac1_last, macs->mac1, COOKIE_MAC_SIZE);
	cm->cm_mac1_sent = true;

	if (cm->cm_cookie_valid &&
	    !timer_expired(cm->cm_cookie_birthdate,
	    COOKIE_SECRET_MAX_AGE - COOKIE_SECRET_LATENCY, 0)) {
		macs_mac2(macs, buf, len, cm->cm_cookie);
	} else {
		bzero(macs->mac2, COOKIE_MAC_SIZE);
		cm->cm_cookie_valid = false;
	}
	rw_wunlock(&cm->cm_lock);
}

int
cookie_checker_validate_macs(struct cookie_checker *cc, struct cookie_macs *macs,
    void *buf, size_t len, bool check_cookie, struct sockaddr *sa, struct vnet *vnet)
{
	struct cookie_macs our_macs;
	uint8_t cookie[COOKIE_COOKIE_SIZE];

	/* Validate incoming MACs */
	rw_rlock(&cc->cc_key_lock);
	macs_mac1(&our_macs, buf, len, cc->cc_mac1_key);
	rw_runlock(&cc->cc_key_lock);

	/* If mac1 is invald, we want to drop the packet */
	if (timingsafe_bcmp(our_macs.mac1, macs->mac1, COOKIE_MAC_SIZE) != 0)
		return EINVAL;

	if (check_cookie) {
		make_cookie(cc, cookie, sa);
		macs_mac2(&our_macs, buf, len, cookie);

		/* If the mac2 is invalid, we want to send a cookie response */
		if (timingsafe_bcmp(our_macs.mac2, macs->mac2, COOKIE_MAC_SIZE) != 0)
			return EAGAIN;

		/* If the mac2 is valid, we may want rate limit the peer.
		 * ratelimit_allow will return either 0 or ECONNREFUSED,
		 * implying there is no ratelimiting, or we should ratelimit
		 * (refuse) respectively. */
		if (sa->sa_family == AF_INET)
			return ratelimit_allow(&ratelimit_v4, sa, vnet);
#ifdef INET6
		else if (sa->sa_family == AF_INET6)
			return ratelimit_allow(&ratelimit_v6, sa, vnet);
#endif
		else
			return EAFNOSUPPORT;
	}

	return 0;
}

/* Private functions */
static void
precompute_key(uint8_t *key, const uint8_t input[COOKIE_INPUT_SIZE],
    const char *label)
{
	struct blake2s_state blake;
	blake2s_init(&blake, COOKIE_KEY_SIZE);
	blake2s_update(&blake, label, strlen(label));
	blake2s_update(&blake, input, COOKIE_INPUT_SIZE);
	blake2s_final(&blake, key);
}

static void
macs_mac1(struct cookie_macs *macs, const void *buf, size_t len,
    const uint8_t key[COOKIE_KEY_SIZE])
{
	struct blake2s_state state;
	blake2s_init_key(&state, COOKIE_MAC_SIZE, key, COOKIE_KEY_SIZE);
	blake2s_update(&state, buf, len);
	blake2s_final(&state, macs->mac1);
}

static void
macs_mac2(struct cookie_macs *macs, const void *buf, size_t len,
    const uint8_t key[COOKIE_COOKIE_SIZE])
{
	struct blake2s_state state;
	blake2s_init_key(&state, COOKIE_MAC_SIZE, key, COOKIE_COOKIE_SIZE);
	blake2s_update(&state, buf, len);
	blake2s_update(&state, macs->mac1, COOKIE_MAC_SIZE);
	blake2s_final(&state, macs->mac2);
}

static __inline int
timer_expired(sbintime_t timer, uint32_t sec, uint32_t nsec)
{
	sbintime_t now = getsbinuptime();
	return (now > (timer + sec * SBT_1S + nstosbt(nsec))) ? ETIMEDOUT : 0;
}

static void
make_cookie(struct cookie_checker *cc, uint8_t cookie[COOKIE_COOKIE_SIZE],
    struct sockaddr *sa)
{
	struct blake2s_state state;

	mtx_lock(&cc->cc_secret_mtx);
	if (timer_expired(cc->cc_secret_birthdate,
	    COOKIE_SECRET_MAX_AGE, 0)) {
		arc4random_buf(cc->cc_secret, COOKIE_SECRET_SIZE);
		cc->cc_secret_birthdate = getsbinuptime();
	}
	blake2s_init_key(&state, COOKIE_COOKIE_SIZE, cc->cc_secret,
	    COOKIE_SECRET_SIZE);
	mtx_unlock(&cc->cc_secret_mtx);

	if (sa->sa_family == AF_INET) {
		blake2s_update(&state, (uint8_t *)&satosin(sa)->sin_addr,
				sizeof(struct in_addr));
		blake2s_update(&state, (uint8_t *)&satosin(sa)->sin_port,
				sizeof(in_port_t));
		blake2s_final(&state, cookie);
#ifdef INET6
	} else if (sa->sa_family == AF_INET6) {
		blake2s_update(&state, (uint8_t *)&satosin6(sa)->sin6_addr,
				sizeof(struct in6_addr));
		blake2s_update(&state, (uint8_t *)&satosin6(sa)->sin6_port,
				sizeof(in_port_t));
		blake2s_final(&state, cookie);
#endif
	} else {
		arc4random_buf(cookie, COOKIE_COOKIE_SIZE);
	}
}

static void
ratelimit_init(struct ratelimit *rl)
{
	size_t i;
	mtx_init(&rl->rl_mtx, "ratelimit_lock", NULL, MTX_DEF);
	callout_init_mtx(&rl->rl_gc, &rl->rl_mtx, 0);
	arc4random_buf(rl->rl_secret, sizeof(rl->rl_secret));
	for (i = 0; i < RATELIMIT_SIZE; i++)
		LIST_INIT(&rl->rl_table[i]);
	rl->rl_table_num = 0;
}

static void
ratelimit_deinit(struct ratelimit *rl)
{
	mtx_lock(&rl->rl_mtx);
	callout_stop(&rl->rl_gc);
	ratelimit_gc(rl, true);
	mtx_unlock(&rl->rl_mtx);
	mtx_destroy(&rl->rl_mtx);
}

static void
ratelimit_gc_callout(void *_rl)
{
	/* callout will lock rl_mtx for us */
	ratelimit_gc(_rl, false);
}

static void
ratelimit_gc_schedule(struct ratelimit *rl)
{
	/* Trigger another GC if needed. There is no point calling GC if there
	 * are no entries in the table. We also want to ensure that GC occurs
	 * on a regular interval, so don't override a currently pending GC.
	 *
	 * In the case of a forced ratelimit_gc, there will be no entries left
	 * so we will will not schedule another GC. */
	if (rl->rl_table_num > 0 && !callout_pending(&rl->rl_gc))
		callout_reset(&rl->rl_gc, ELEMENT_TIMEOUT * hz,
		    ratelimit_gc_callout, rl);
}

static void
ratelimit_gc(struct ratelimit *rl, bool force)
{
	size_t i;
	struct ratelimit_entry *r, *tr;
	sbintime_t expiry;

	mtx_assert(&rl->rl_mtx, MA_OWNED);

	if (rl->rl_table_num == 0)
		return;

	expiry = getsbinuptime() - ELEMENT_TIMEOUT * SBT_1S;

	for (i = 0; i < RATELIMIT_SIZE; i++) {
		LIST_FOREACH_SAFE(r, &rl->rl_table[i], r_entry, tr) {
			if (r->r_last_time < expiry || force) {
				rl->rl_table_num--;
				LIST_REMOVE(r, r_entry);
				uma_zfree(ratelimit_zone, r);
			}
		}
	}

	ratelimit_gc_schedule(rl);
}

static int
ratelimit_allow(struct ratelimit *rl, struct sockaddr *sa, struct vnet *vnet)
{
	uint64_t bucket, tokens;
	sbintime_t diff, now;
	struct ratelimit_entry *r;
	int ret = ECONNREFUSED;
	struct ratelimit_key key = { .vnet = vnet };
	size_t len = sizeof(key);

	if (sa->sa_family == AF_INET) {
		memcpy(key.ip, &satosin(sa)->sin_addr, IPV4_MASK_SIZE);
		len -= IPV6_MASK_SIZE - IPV4_MASK_SIZE;
	}
#ifdef INET6
	else if (sa->sa_family == AF_INET6)
		memcpy(key.ip, &satosin6(sa)->sin6_addr, IPV6_MASK_SIZE);
#endif
	else
		return ret;

	bucket = siphash13(rl->rl_secret, &key, len) & RATELIMIT_MASK;
	mtx_lock(&rl->rl_mtx);

	LIST_FOREACH(r, &rl->rl_table[bucket], r_entry) {
		if (bcmp(&r->r_key, &key, len) != 0)
			continue;

		/* If we get to here, we've found an entry for the endpoint.
		 * We apply standard token bucket, by calculating the time
		 * lapsed since our last_time, adding that, ensuring that we
		 * cap the tokens at TOKEN_MAX. If the endpoint has no tokens
		 * left (that is tokens <= INITIATION_COST) then we block the
		 * request, otherwise we subtract the INITITIATION_COST and
		 * return OK. */
		now = getsbinuptime();
		diff = now - r->r_last_time;
		r->r_last_time = now;

		tokens = r->r_tokens + diff;

		if (tokens > TOKEN_MAX)
			tokens = TOKEN_MAX;

		if (tokens >= INITIATION_COST) {
			r->r_tokens = tokens - INITIATION_COST;
			goto ok;
		} else {
			r->r_tokens = tokens;
			goto error;
		}
	}

	/* If we get to here, we didn't have an entry for the endpoint, let's
	 * add one if we have space. */
	if (rl->rl_table_num >= RATELIMIT_SIZE_MAX)
		goto error;

	/* Goto error if out of memory */
	if ((r = uma_zalloc(ratelimit_zone, M_NOWAIT | M_ZERO)) == NULL)
		goto error;

	rl->rl_table_num++;

	/* Insert entry into the hashtable and ensure it's initialised */
	LIST_INSERT_HEAD(&rl->rl_table[bucket], r, r_entry);
	r->r_key = key;
	r->r_last_time = getsbinuptime();
	r->r_tokens = TOKEN_MAX - INITIATION_COST;

	/* If we've added a new entry, let's trigger GC. */
	ratelimit_gc_schedule(rl);
ok:
	ret = 0;
error:
	mtx_unlock(&rl->rl_mtx);
	return ret;
}

static uint64_t siphash13(const uint8_t key[SIPHASH_KEY_LENGTH], const void *src, size_t len)
{
	SIPHASH_CTX ctx;
	return (SipHashX(&ctx, 1, 3, key, src, len));
}

#ifdef SELFTESTS
#include "selftest/cookie.c"
#endif /* SELFTESTS */
