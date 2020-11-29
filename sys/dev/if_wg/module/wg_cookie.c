/*
 * Copyright (C) 2015-2020 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 * Copyright (C) 2019-2020 Matt Dunwoodie <ncon@noconroy.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/rwlock.h>
#include <sys/malloc.h> /* Because systm doesn't include M_NOWAIT, M_DEVBUF */
#include <sys/socket.h>

#include <sys/wg_cookie.h>
#include <zinc/chacha20poly1305.h>

static void	cookie_precompute_key(uint8_t *,
			const uint8_t[COOKIE_INPUT_SIZE], const char *);
static void	cookie_macs_mac1(struct cookie_macs *, const void *, size_t,
			const uint8_t[COOKIE_KEY_SIZE]);
static void	cookie_macs_mac2(struct cookie_macs *, const void *, size_t,
			const uint8_t[COOKIE_COOKIE_SIZE]);
static int	cookie_timer_expired(struct timespec *, time_t, long);
static void	cookie_checker_make_cookie(struct cookie_checker *,
			uint8_t[COOKIE_COOKIE_SIZE], struct sockaddr *);
static void	ratelimit_gc(struct ratelimit *, int);
static int	ratelimit_allow(struct ratelimit *, struct sockaddr *);

/* Public Functions */
void
cookie_maker_init(struct cookie_maker *cp, const uint8_t key[COOKIE_INPUT_SIZE])
{
	bzero(cp, sizeof(*cp));
	cookie_precompute_key(cp->cp_mac1_key, key, COOKIE_MAC1_KEY_LABEL);
	cookie_precompute_key(cp->cp_cookie_key, key, COOKIE_COOKIE_KEY_LABEL);
	rw_init(&cp->cp_lock, "cookie_maker");
}

int
cookie_checker_init(struct cookie_checker *cc, uma_zone_t zone)
{
	struct ratelimit *rl = &cc->cc_ratelimit;
	bzero(cc, sizeof(*cc));

	rw_init(&cc->cc_key_lock, "cookie_checker_key");
	rw_init(&cc->cc_secret_lock, "cookie_checker_secret");

	rw_init(&rl->rl_lock, "ratelimit_lock");
	arc4random_buf(&rl->rl_secret, sizeof(rl->rl_secret));
	rl->rl_table = hashinit(RATELIMIT_SIZE, M_DEVBUF, &rl->rl_table_mask);
	rl->rl_zone = zone;

	return (0);
}

void
cookie_checker_update(struct cookie_checker *cc,
    uint8_t key[COOKIE_INPUT_SIZE])
{
	rw_enter_write(&cc->cc_key_lock);
	if (key) {
		cookie_precompute_key(cc->cc_mac1_key, key, COOKIE_MAC1_KEY_LABEL);
		cookie_precompute_key(cc->cc_cookie_key, key, COOKIE_COOKIE_KEY_LABEL);
	} else {
		bzero(cc->cc_mac1_key, sizeof(cc->cc_mac1_key));
		bzero(cc->cc_cookie_key, sizeof(cc->cc_cookie_key));
	}
	rw_exit_write(&cc->cc_key_lock);
}

void
cookie_checker_deinit(struct cookie_checker *cc)
{
	struct ratelimit *rl = &cc->cc_ratelimit;

	rw_enter_write(&rl->rl_lock);
	ratelimit_gc(rl, 1);
	hashdestroy(rl->rl_table, M_DEVBUF, rl->rl_table_mask);
	rw_exit_write(&rl->rl_lock);
}

void
cookie_checker_create_payload(struct cookie_checker *cc,
    struct cookie_macs *cm, uint8_t nonce[COOKIE_XNONCE_SIZE],
    uint8_t ecookie[COOKIE_ENCRYPTED_SIZE], struct sockaddr *sa)
{
	uint8_t cookie[COOKIE_COOKIE_SIZE];

	cookie_checker_make_cookie(cc, cookie, sa);
	arc4random_buf(nonce, COOKIE_XNONCE_SIZE);

	rw_enter_read(&cc->cc_key_lock);
	xchacha20poly1305_encrypt(ecookie, cookie, COOKIE_COOKIE_SIZE,
	    cm->mac1, COOKIE_MAC_SIZE, nonce, cc->cc_cookie_key);
	rw_exit_read(&cc->cc_key_lock);

	explicit_bzero(cookie, sizeof(cookie));
}

int
cookie_maker_consume_payload(struct cookie_maker *cp,
    uint8_t nonce[COOKIE_XNONCE_SIZE], uint8_t ecookie[COOKIE_ENCRYPTED_SIZE])
{
	int ret = 0;
	uint8_t cookie[COOKIE_COOKIE_SIZE];

	rw_enter_write(&cp->cp_lock);

	if (cp->cp_mac1_valid == 0) {
		ret = ETIMEDOUT;
		goto error;
	}

	if (xchacha20poly1305_decrypt(cookie, ecookie, COOKIE_ENCRYPTED_SIZE,
	    cp->cp_mac1_last, COOKIE_MAC_SIZE, nonce, cp->cp_cookie_key) == 0) {
		ret = EINVAL;
		goto error;
	}

	memcpy(cp->cp_cookie, cookie, COOKIE_COOKIE_SIZE);
	getnanouptime(&cp->cp_birthdate);
	cp->cp_mac1_valid = 0;

error:
	rw_exit_write(&cp->cp_lock);
	return ret;
}

void
cookie_maker_mac(struct cookie_maker *cp, struct cookie_macs *cm, void *buf,
		size_t len)
{
	rw_enter_read(&cp->cp_lock);

	cookie_macs_mac1(cm, buf, len, cp->cp_mac1_key);

	memcpy(cp->cp_mac1_last, cm->mac1, COOKIE_MAC_SIZE);
	cp->cp_mac1_valid = 1;

	if (!cookie_timer_expired(&cp->cp_birthdate,
	    COOKIE_SECRET_MAX_AGE - COOKIE_SECRET_LATENCY, 0))
		cookie_macs_mac2(cm, buf, len, cp->cp_cookie);
	else
		bzero(cm->mac2, COOKIE_MAC_SIZE);

	rw_exit_read(&cp->cp_lock);
}

int
cookie_checker_validate_macs(struct cookie_checker *cc, struct cookie_macs *cm,
		void *buf, size_t len, int busy, struct sockaddr *sa)
{
	struct cookie_macs our_cm;
	uint8_t cookie[COOKIE_COOKIE_SIZE];

	/* Validate incoming MACs */
	rw_enter_read(&cc->cc_key_lock);
	cookie_macs_mac1(&our_cm, buf, len, cc->cc_mac1_key);
	rw_exit_read(&cc->cc_key_lock);

	/* If mac1 is invald, we want to drop the packet */
	if (timingsafe_bcmp(our_cm.mac1, cm->mac1, COOKIE_MAC_SIZE) != 0)
		return EINVAL;

	if (busy != 0) {
		cookie_checker_make_cookie(cc, cookie, sa);
		cookie_macs_mac2(&our_cm, buf, len, cookie);

		/* If the mac2 is invalid, we want to send a cookie response */
		if (timingsafe_bcmp(our_cm.mac2, cm->mac2, COOKIE_MAC_SIZE) != 0)
			return EAGAIN;

		/* If the mac2 is valid, we may want rate limit the peer. 
		 * ratelimit_allow will return either 0 or ECONNREFUSED,
		 * implying there is no ratelimiting, or we should ratelimit
		 * (refuse) respectively. */
		return ratelimit_allow(&cc->cc_ratelimit, sa);
	}
	return 0;
}

/* Private functions */
static void
cookie_precompute_key(uint8_t *key, const uint8_t input[COOKIE_INPUT_SIZE],
    const char *label)
{
	struct blake2s_state blake;

	blake2s_init(&blake, COOKIE_KEY_SIZE);
	blake2s_update(&blake, label, strlen(label));
	blake2s_update(&blake, input, COOKIE_INPUT_SIZE);
	blake2s_final(&blake, key, COOKIE_KEY_SIZE);
}

static void
cookie_macs_mac1(struct cookie_macs *cm, const void *buf, size_t len,
    const uint8_t key[COOKIE_KEY_SIZE])
{
	struct blake2s_state state;
	blake2s_init_key(&state, COOKIE_MAC_SIZE, key, COOKIE_KEY_SIZE);
	blake2s_update(&state, buf, len);
	blake2s_final(&state, cm->mac1, COOKIE_MAC_SIZE);
}

static void
cookie_macs_mac2(struct cookie_macs *cm, const void *buf, size_t len,
		const uint8_t key[COOKIE_COOKIE_SIZE])
{
	struct blake2s_state state;
	blake2s_init_key(&state, COOKIE_MAC_SIZE, key, COOKIE_COOKIE_SIZE);
	blake2s_update(&state, buf, len);
	blake2s_update(&state, cm->mac1, COOKIE_MAC_SIZE);
	blake2s_final(&state, cm->mac2, COOKIE_MAC_SIZE);
}

static int
cookie_timer_expired(struct timespec *birthdate, time_t sec, long nsec)
{
	struct timespec	uptime;
	struct timespec	expire = { .tv_sec = sec, .tv_nsec = nsec };

	if (birthdate->tv_sec == 0 && birthdate->tv_nsec == 0)
		return ETIMEDOUT;

	getnanouptime(&uptime);
	timespecadd(birthdate, &expire, &expire);
	return timespeccmp(&uptime, &expire, >) ? ETIMEDOUT : 0;
}

static void
cookie_checker_make_cookie(struct cookie_checker *cc,
		uint8_t cookie[COOKIE_COOKIE_SIZE], struct sockaddr *sa)
{
	struct blake2s_state state;

	rw_enter_write(&cc->cc_secret_lock);
	if (cookie_timer_expired(&cc->cc_secret_birthdate,
	    COOKIE_SECRET_MAX_AGE, 0)) {
		arc4random_buf(cc->cc_secret, COOKIE_SECRET_SIZE);
		getnanouptime(&cc->cc_secret_birthdate);
	}
	blake2s_init_key(&state, COOKIE_COOKIE_SIZE, cc->cc_secret,
	    COOKIE_SECRET_SIZE);
	rw_exit_write(&cc->cc_secret_lock);

	if (sa->sa_family == AF_INET) {
		blake2s_update(&state, (uint8_t *)&satosin(sa)->sin_addr,
				sizeof(struct in_addr));
		blake2s_update(&state, (uint8_t *)&satosin(sa)->sin_port, 
				sizeof(in_port_t));
		blake2s_final(&state, cookie, COOKIE_COOKIE_SIZE);
	} else if (sa->sa_family == AF_INET6) {
		blake2s_update(&state, (uint8_t *)&satosin6(sa)->sin6_addr,
				sizeof(struct in6_addr));
		blake2s_update(&state, (uint8_t *)&satosin6(sa)->sin6_port,
				sizeof(in_port_t));
		blake2s_final(&state, cookie, COOKIE_COOKIE_SIZE);
	} else {
		arc4random_buf(cookie, COOKIE_COOKIE_SIZE);
	}
}

static void
ratelimit_gc(struct ratelimit *rl, int force)
{
	size_t i;
	struct ratelimit_entry *r, *tr;
	struct timespec expiry;

	rw_assert(&rl->rl_lock, RA_WLOCKED);

	if (force) {
		for (i = 0; i < RATELIMIT_SIZE; i++) {
			LIST_FOREACH_SAFE(r, &rl->rl_table[i], r_entry, tr) {
				rl->rl_table_num--;
				LIST_REMOVE(r, r_entry);
				uma_zfree(rl->rl_zone, r);
			}
		}
		return;
	}

	if ((cookie_timer_expired(&rl->rl_last_gc, ELEMENT_TIMEOUT, 0) &&
	    rl->rl_table_num > 0)) {
		getnanouptime(&rl->rl_last_gc);
		getnanouptime(&expiry);
		expiry.tv_sec -= ELEMENT_TIMEOUT;

		for (i = 0; i < RATELIMIT_SIZE; i++) {
			LIST_FOREACH_SAFE(r, &rl->rl_table[i], r_entry, tr) {
				if (timespeccmp(&r->r_last_time, &expiry, <)) {
					rl->rl_table_num--;
					LIST_REMOVE(r, r_entry);
					uma_zfree(rl->rl_zone, r);
				}
			}
		}
	}
}

static int
ratelimit_allow(struct ratelimit *rl, struct sockaddr *sa)
{
	uint64_t key, tokens;
	struct timespec diff;
	struct ratelimit_entry *r;
	int ret = ECONNREFUSED;

	if (sa->sa_family == AF_INET)
		key = siphash24(&rl->rl_secret, &satosin(sa)->sin_addr,
				IPV4_MASK_SIZE);
	else if (sa->sa_family == AF_INET6)
		key = siphash24(&rl->rl_secret, &satosin6(sa)->sin6_addr,
				IPV6_MASK_SIZE);
	else
		return ret;

	rw_enter_write(&rl->rl_lock);

	LIST_FOREACH(r, &rl->rl_table[key & rl->rl_table_mask], r_entry) {
		if (r->r_af != sa->sa_family)
			continue;

		if (r->r_af == AF_INET && bcmp(&r->r_in,
		    &satosin(sa)->sin_addr, IPV4_MASK_SIZE) != 0)
			continue;

		if (r->r_af == AF_INET6 && bcmp(&r->r_in6,
		    &satosin6(sa)->sin6_addr, IPV6_MASK_SIZE) != 0)
			continue;

		/* If we get to here, we've found an entry for the endpoint.
		 * We apply standard token bucket, by calculating the time
		 * lapsed since our last_time, adding that, ensuring that we
		 * cap the tokens at TOKEN_MAX. If the endpoint has no tokens
		 * left (that is tokens <= INITIATION_COST) then we block the
		 * request, otherwise we subtract the INITITIATION_COST and
		 * return OK. */
		diff = r->r_last_time;
		getnanouptime(&r->r_last_time);
		timespecsub(&r->r_last_time, &diff, &diff);

		tokens = r->r_tokens + diff.tv_sec * NSEC_PER_SEC + diff.tv_nsec;

		if (tokens > TOKEN_MAX)
			tokens = TOKEN_MAX;

		if (tokens > INITIATION_COST) {
			r->r_tokens = tokens - INITIATION_COST;
			goto ok;
		} else {
			r->r_tokens = tokens;
			goto error;
		}
	}

	/* If we get to here, we didn't have an entry for the endpoint. */
	ratelimit_gc(rl, 0);

	/* Hard limit on number of entries */
	if (rl->rl_table_num >= RATELIMIT_SIZE_MAX * 8)
		goto error;

	/* Goto error if out of memory */
	if ((r = uma_zalloc(rl->rl_zone, M_NOWAIT)) == NULL)
		goto error;

	rl->rl_table_num++;

	/* Insert entry into the hashtable and ensure it's initialised */
	LIST_INSERT_HEAD(&rl->rl_table[key & rl->rl_table_mask], r, r_entry);
	r->r_af = sa->sa_family;
	if (r->r_af == AF_INET)
		memcpy(&r->r_in, &satosin(sa)->sin_addr, IPV4_MASK_SIZE);
	else if (r->r_af == AF_INET6)
		memcpy(&r->r_in6, &satosin6(sa)->sin6_addr, IPV6_MASK_SIZE);

	getnanouptime(&r->r_last_time);
	r->r_tokens = TOKEN_MAX - INITIATION_COST;
ok:
	ret = 0;
error:
	rw_exit_write(&rl->rl_lock);
	return ret;
}
