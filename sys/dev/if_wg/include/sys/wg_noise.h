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
 * ======== wg_noise.h ========
 *
 * This file provides a thread safe interface to the Noise protocol as used in
 * WireGuard. The three user facing components are:
 *
 *   * noise_local
 *           Stores the local state for a noise peer.
 *   * noise_remote
 *           Stores the remote state for a noise peer.
 *   * noise_upcall
 *           Stores callback routines for index and peers
 *
 * Additionally a noise_counter, which is invsible to the user is used to track
 * message nonces, to prevent message replay.
 *
 * This module uses Curve25519 for asymmetric crypto, and ChaCha20Poly1305 for
 * symmetric crypto. The handshake uses ephemeral keys, which provide perfect
 * forward secrecy.  Keys are NOISE_KEY_SIZE (32) bytes long and can be
 * generated with a CSRNG. While this module will clamp the key to form a valid
 * Curve25519 key, it is recommended that keys are stored in Curve25519 form to
 * preserve interoperability with other systems. Additionally, there is an
 * optional PresharedKey of length NOISE_PSK_SIZE (also 32 bytes), which when
 * used, will provide protection against known quantum attacks. Without it,
 * Curve25519 is broken by Shor's algorithm.
 *
 * -------- noise_local --------
 *
 * void noise_local_init(noise_local *, noise_upcall *)
 *   - Initialise noise_local, should only be called once and before use.
 *
 * int noise_local_set_private(noise_local *, uint8_t *private)
 *   - Set the local private key. This will also calculate the corresponding
 *     public key.
 *
 * int  noise_local_keys(noise_local *, uint8_t *public, uint8_t *private)
 *   - Get the local keys. It will ensure that a key has been set and if
 *     not, will return ENXIO.
 *
 * -------- noise_remote --------
 *
 * void noise_remote_init(noise_remote *, uint8_t *public)
 *   - Initialise noise_local, should only be called once and before use. Key
 *     must be provided and it cannot be changed once set.
 *
 * void noise_remote_set_psk(noise_remote *, uint8_t *psk)
 *   - Set the shared key. To remove the shared key, set a key of all 0x00.
 *
 * void noise_remote_keys(noise_remote *, uint8_t *public, uint8_t *psk)
 *   - Get the remote keys.
 *
 * -------- noise_upcall --------
 *
 * The noise_upcall struct is used to lookup incoming public keys, as well as
 * allocate and deallocate index for a remote. The allocation and deallocation
 * are serialised per noise_remote and guaranteed to only have 3 allocated
 * indexes at once.
 *
 * u_arg	- passed to callback functions as void *
 * u_get_remote	- lookup noise_remote based on public key.
 * u_set_index	- allocate index for noise_remote. any further packets that
 * 		  arrive with this index should be passed to noise_* functions
 * 		  with the corresponding noise_remote.
 * u_drop_index	- dealloate index passed to callback.
 *
 * -------- crypto --------
 *
 * The following functions are used for the crypto side of things:
 *
 * int noise_create_initiation(noise_remote *, noise_initiation *)
 * int noise_consume_initiation(noise_local *, noise_remote **, noise_initiation *)
 * int noise_create_response(noise_remote *, noise_response *)
 * int noise_consume_response(noise_remote *, noise_response *)
 *
 * int noise_remote_promote(noise_remote *)
 * void noise_remote_clear(noise_remote *)
 * void noise_remote_expire_current(noise_remote *)
 * int noise_remote_encrypt(noise_remote *, noise_data *, size_t)
 * int noise_remote_decrypt(noise_remote *, noise_data *, size_t)
 *
 * $FreeBSD$
 */

#ifndef __NOISE_H__
#define __NOISE_H__

#include <sys/types.h>
#include <sys/time.h>
#include <sys/rwlock.h>
#include <sys/support.h>

#include <crypto/blake2s.h>
#include <zinc/chacha20poly1305.h>
#include <crypto/curve25519.h>

#define NOISE_KEY_SIZE		CURVE25519_KEY_SIZE
#define NOISE_PSK_SIZE		32
#define NOISE_MAC_SIZE		CHACHA20POLY1305_AUTHTAG_SIZE
#define NOISE_HASH_SIZE		BLAKE2S_HASH_SIZE
#define NOISE_SYMMETRIC_SIZE	CHACHA20POLY1305_KEY_SIZE
#define NOISE_TIMESTAMP_SIZE	12

/* Protocol string constants */
#define NOISE_HANDSHAKE_NAME	"Noise_IKpsk2_25519_ChaChaPoly_BLAKE2s"
#define NOISE_IDENTIFIER_NAME	"WireGuard v1 zx2c4 Jason@zx2c4.com"

/* Constants for the counter */
#define COUNTER_TYPE		size_t
#define COUNTER_BITS_TOTAL	512
#define COUNTER_TYPE_BITS	(sizeof(COUNTER_TYPE) * 8)
#define COUNTER_TYPE_NUM	(COUNTER_BITS_TOTAL / COUNTER_TYPE_BITS)
#define COUNTER_WINDOW_SIZE	(COUNTER_BITS_TOTAL - COUNTER_TYPE_BITS)

/* Constants for the keypair */
#define REKEY_AFTER_MESSAGES	(1ull << 60)
#define REJECT_AFTER_MESSAGES	(UINT64_MAX - COUNTER_WINDOW_SIZE - 1)
#define REKEY_AFTER_TIME	120
#define REKEY_AFTER_TIME_RECV	165
#define REJECT_AFTER_TIME	180
#define REJECT_INTERVAL		(1000000000 / 50) /* fifty times per sec */
/* 24 = floor(log2(REJECT_INTERVAL)) */
#define REJECT_INTERVAL_MASK	(~((1ull<<24)-1))

enum noise_state_hs {
	HS_ZEROED = 0,
	CREATED_INITIATION,
	CONSUMED_INITIATION,
	CREATED_RESPONSE,
	CONSUMED_RESPONSE,
};

struct noise_handshake {
	enum noise_state_hs	 hs_state;
	uint32_t		 hs_local_index;
	uint32_t		 hs_remote_index;
	uint8_t		 	 hs_e[NOISE_KEY_SIZE];
	uint8_t		 	 hs_hash[NOISE_HASH_SIZE];
	uint8_t		 	 hs_ck[NOISE_HASH_SIZE];
};

struct noise_counter {
	struct rwlock		 c_lock;
	uint64_t		 c_send;
	uint64_t		 c_recv;
	COUNTER_TYPE		 c_backtrack[COUNTER_TYPE_NUM];
};

enum noise_state_kp {
	KP_ZEROED = 0,
	INITIATOR,
	RESPONDER,
};

struct noise_keypair {
	SLIST_ENTRY(noise_keypair)	kp_entry;
	int				kp_valid;
	int				kp_is_initiator;
	uint32_t			kp_local_index;
	uint32_t			kp_remote_index;
	uint8_t				kp_send[NOISE_SYMMETRIC_SIZE];
	uint8_t				kp_recv[NOISE_SYMMETRIC_SIZE];
	struct timespec			kp_birthdate; /* nanouptime */
	struct noise_counter		kp_ctr;
};

struct noise_remote {
	uint8_t				 r_public[NOISE_KEY_SIZE];
	struct noise_local		*r_local;
	uint8_t		 		 r_ss[NOISE_KEY_SIZE];

	struct rwlock			 r_handshake_lock;
	struct noise_handshake		 r_handshake;
	uint8_t				 r_psk[NOISE_PSK_SIZE];
	uint8_t				 r_timestamp[NOISE_TIMESTAMP_SIZE];
	struct timespec			 r_last_init; /* nanouptime */

	struct rwlock			 r_keypair_lock;
	SLIST_HEAD(,noise_keypair)	 r_unused_keypairs;
	struct noise_keypair		*r_next, *r_current, *r_previous;
	struct noise_keypair		 r_keypair[3]; /* 3: next, current, previous. */

};

struct noise_local {
	struct rwlock		l_identity_lock;
	int			l_has_identity;
	uint8_t			l_public[NOISE_KEY_SIZE];
	uint8_t			l_private[NOISE_KEY_SIZE];

	struct noise_upcall {
		void	 *u_arg;
		struct noise_remote *
			(*u_remote_get)(void *, uint8_t[NOISE_KEY_SIZE]);
		uint32_t
			(*u_index_set)(void *, struct noise_remote *);
		void	(*u_index_drop)(void *, uint32_t);
	}			l_upcall;
};

struct noise_initiation {
	uint32_t	s_idx;
	uint8_t		ue[NOISE_KEY_SIZE];
	uint8_t		es[NOISE_KEY_SIZE + NOISE_MAC_SIZE];
	uint8_t		ets[NOISE_TIMESTAMP_SIZE + NOISE_MAC_SIZE];
} __packed;

struct noise_response {
	uint32_t	s_idx;
	uint32_t	r_idx;
	uint8_t		ue[NOISE_KEY_SIZE];
	uint8_t		en[0 + NOISE_MAC_SIZE];
} __packed;

struct noise_data {
	uint32_t	r_idx;
	uint64_t	nonce;
	uint8_t		buf[];
} __packed;


/* Set/Get noise parameters */
void	noise_local_init(struct noise_local *, struct noise_upcall *);
void	noise_local_lock_identity(struct noise_local *);
void	noise_local_unlock_identity(struct noise_local *);
int	noise_local_set_private(struct noise_local *, uint8_t[NOISE_KEY_SIZE]);
int	noise_local_keys(struct noise_local *, uint8_t[NOISE_KEY_SIZE],
	    uint8_t[NOISE_KEY_SIZE]);

void	noise_remote_init(struct noise_remote *, const uint8_t[NOISE_KEY_SIZE],
	    struct noise_local *);
int	noise_remote_set_psk(struct noise_remote *, const uint8_t[NOISE_PSK_SIZE]);
int	noise_remote_keys(struct noise_remote *, uint8_t[NOISE_KEY_SIZE],
	    uint8_t[NOISE_PSK_SIZE]);

/* Should be called anytime noise_local_set_private is called */
void	noise_remote_precompute(struct noise_remote *);

/* Cryptographic functions */
int	noise_create_initiation(
	    struct noise_remote *,
	    struct noise_initiation *);

int	noise_consume_initiation(
	    struct noise_local *,
	    struct noise_remote **,
	    struct noise_initiation *);

int	noise_create_response(
	    struct noise_remote *,
	    struct noise_response *);

int	noise_consume_response(
	    struct noise_remote *,
	    struct noise_response *);

 int	noise_remote_begin_session(struct noise_remote *);
void	noise_remote_clear(struct noise_remote *);
void	noise_remote_expire_current(struct noise_remote *);

int	noise_remote_ready(struct noise_remote *);

int	noise_remote_encrypt(
	    struct noise_remote *,
	    struct noise_data *,
	    size_t);
int	noise_remote_decrypt(
	    struct noise_remote *,
	    struct noise_data *,
	    size_t);

#endif /* __NOISE_H__ */
