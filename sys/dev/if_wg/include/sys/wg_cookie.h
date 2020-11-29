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
 *
 * ======== wg_cookie.h ========
 *
 * This file provides a thread safe interface to the WireGuard cookie
 * mechanism. It is split into three parts:
 *
 *    * cookie_maker
 *            Used to create MACs for messages.
 *    * cookie_checker
 *            Used to validate MACs for messages.
 *    * cookie_macs
 *            The MACs that authenticate the message.
 *
 * The MACs provide two properties:
 *    * mac1 - That the remote end knows a value.
 *    * mac2 - That the remote end has a specific IP address.
 *
 * void cookie_maker_init(cookie_maker, ipl, input)
 *   - Initialise cookie_maker, should only be called once and before use.
 *     input is the shared value used for mac1.
 *
 * int  cookie_checker_init(cookie_checker, ipl)
 *   - Initialise cookie_checker, should only be called once and before use. It
 *     will return ENOBUFS if it cannot allocate required memory.
 *
 * void cookie_checker_update(cookie_checker, input)
 *   - Set the input value to check mac1 against.
 *
 * void cookie_checker_deinit(cookie_checker)
 *   - Destroy all values associated with cookie_checker. cookie_checker must
 *     not be used after calling this function.
 *
 * void cookie_checker_create_payload(cookie_checker, cookie_macs, nonce,
 *         payload, sockaddr)
 *   - Create a specific payload derived from the sockaddr. The payload is an
 *     encrypted shared secret, that the cookie_maker will decrypt and used to
 *     key the mac2 value.
 *
 * int  cookie_maker_consume_payload(cookie_maker, nonce, payload)
 *   - Have cookie_maker consume the payload.
 *
 * void cookie_maker_mac(cookie_maker, cookie_macs, message, len)
 *   - Create cookie_macs for the message of length len. It will always compute
 *     mac1, however will only compute mac2 if we have recently received a
 *     payload to key it with.
 *
 * int  cookie_checker_validate_macs(cookie_checker, cookie_macs, message, len,
 *         busy, sockaddr)
 *   - Use cookie_checker to validate the cookie_macs of message with length
 *     len. If busy, then ratelimiting will be applied to the sockaddr.
 *
 * ==========================
 * $FreeBSD$
 */

#ifndef __COOKIE_H__
#define __COOKIE_H__

#include <sys/types.h>
#include <sys/time.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/support.h>

#include <netinet/in.h>

#include <crypto/blake2s.h>

#define COOKIE_MAC_SIZE		16
#define COOKIE_KEY_SIZE		32
#define COOKIE_XNONCE_SIZE	24
#define COOKIE_COOKIE_SIZE	16
#define COOKIE_SECRET_SIZE	32
#define COOKIE_INPUT_SIZE	32
#define COOKIE_ENCRYPTED_SIZE	(COOKIE_COOKIE_SIZE + COOKIE_MAC_SIZE)

#define COOKIE_MAC1_KEY_LABEL	"mac1----"
#define COOKIE_COOKIE_KEY_LABEL	"cookie--"
#define COOKIE_SECRET_MAX_AGE	120
#define COOKIE_SECRET_LATENCY	5

/* Constants for initiation rate limiting */
#define RATELIMIT_SIZE		(1 << 10)
#define RATELIMIT_SIZE_MAX	(RATELIMIT_SIZE * 8)
#define NSEC_PER_SEC		1000000000LL
#define INITIATIONS_PER_SECOND	50
#define INITIATIONS_BURSTABLE	10
#define INITIATION_COST		(NSEC_PER_SEC / INITIATIONS_PER_SECOND)
#define TOKEN_MAX		(INITIATION_COST * INITIATIONS_BURSTABLE)
#define ELEMENT_TIMEOUT		1
#define IPV4_MASK_SIZE		4 /* Use all 4 bytes of IPv4 address */
#define IPV6_MASK_SIZE		8 /* Use top 8 bytes (/64) of IPv6 address */

struct cookie_macs {
	uint8_t	mac1[COOKIE_MAC_SIZE];
	uint8_t	mac2[COOKIE_MAC_SIZE];
} __packed;

struct ratelimit_entry {
	LIST_ENTRY(ratelimit_entry)	 r_entry;
	sa_family_t			 r_af;
	union {
		struct in_addr		 r_in;
		struct in6_addr		 r_in6;
	};
	struct timespec			 r_last_time;	/* nanouptime */
	uint64_t			 r_tokens;
};

struct ratelimit {
	SIPHASH_KEY			 rl_secret;
	uma_zone_t			rl_zone;

	struct rwlock			 rl_lock;
	LIST_HEAD(, ratelimit_entry)	*rl_table;
	u_long				 rl_table_mask;
	size_t				 rl_table_num;
	struct timespec			 rl_last_gc;	/* nanouptime */
};

struct cookie_maker {
	uint8_t		cp_mac1_key[COOKIE_KEY_SIZE];
	uint8_t		cp_cookie_key[COOKIE_KEY_SIZE];

	struct rwlock	cp_lock;
	uint8_t		cp_cookie[COOKIE_COOKIE_SIZE];
	struct timespec	cp_birthdate;	/* nanouptime */
	int		cp_mac1_valid;
	uint8_t		cp_mac1_last[COOKIE_MAC_SIZE];
};

struct cookie_checker {
	struct ratelimit	cc_ratelimit;

	struct rwlock		cc_key_lock;
	uint8_t			cc_mac1_key[COOKIE_KEY_SIZE];
	uint8_t			cc_cookie_key[COOKIE_KEY_SIZE];

	struct rwlock		cc_secret_lock;
	struct timespec		cc_secret_birthdate;	/* nanouptime */
	uint8_t			cc_secret[COOKIE_SECRET_SIZE];
};

void	cookie_maker_init(struct cookie_maker *, const uint8_t[COOKIE_INPUT_SIZE]);
int	cookie_checker_init(struct cookie_checker *, uma_zone_t);
void	cookie_checker_update(struct cookie_checker *,
	    uint8_t[COOKIE_INPUT_SIZE]);
void	cookie_checker_deinit(struct cookie_checker *);
void	cookie_checker_create_payload(struct cookie_checker *,
	    struct cookie_macs *cm, uint8_t[COOKIE_XNONCE_SIZE],
	    uint8_t [COOKIE_ENCRYPTED_SIZE], struct sockaddr *);
int	cookie_maker_consume_payload(struct cookie_maker *,
	    uint8_t[COOKIE_XNONCE_SIZE], uint8_t[COOKIE_ENCRYPTED_SIZE]);
void	cookie_maker_mac(struct cookie_maker *, struct cookie_macs *,
	    void *, size_t);
int	cookie_checker_validate_macs(struct cookie_checker *,
	    struct cookie_macs *, void *, size_t, int, struct sockaddr *);

#endif /* __COOKIE_H__ */
