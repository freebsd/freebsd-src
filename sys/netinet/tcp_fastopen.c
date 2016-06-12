/*-
 * Copyright (c) 2015 Patrick Kelsey
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This is a server-side implementation of TCP Fast Open (TFO) [RFC7413].
 *
 * This implementation is currently considered to be experimental and is not
 * included in kernel builds by default.  To include this code, add the
 * following line to your kernel config:
 *
 * options TCP_RFC7413
 *
 * The generated TFO cookies are the 64-bit output of
 * SipHash24(<16-byte-key><client-ip>).  Multiple concurrent valid keys are
 * supported so that time-based rolling cookie invalidation policies can be
 * implemented in the system.  The default number of concurrent keys is 2.
 * This can be adjusted in the kernel config as follows:
 *
 * options TCP_RFC7413_MAX_KEYS=<num-keys>
 *
 *
 * The following TFO-specific sysctls are defined:
 *
 * net.inet.tcp.fastopen.acceptany (RW, default 0)
 *     When non-zero, all client-supplied TFO cookies will be considered to
 *     be valid.
 *
 * net.inet.tcp.fastopen.autokey (RW, default 120)
 *     When this and net.inet.tcp.fastopen.enabled are non-zero, a new key
 *     will be automatically generated after this many seconds.
 *
 * net.inet.tcp.fastopen.enabled (RW, default 0)
 *     When zero, no new TFO connections can be created.  On the transition
 *     from enabled to disabled, all installed keys are removed.  On the 
 *     transition from disabled to enabled, if net.inet.tcp.fastopen.autokey
 *     is non-zero and there are no keys installed, a new key will be 
 *     generated immediately.  The transition from enabled to disabled does
 *     not affect any TFO connections in progress; it only prevents new ones
 *     from being made.
 *
 * net.inet.tcp.fastopen.keylen (RO)
 *     The key length in bytes.
 *
 * net.inet.tcp.fastopen.maxkeys (RO)
 *     The maximum number of keys supported.
 *
 * net.inet.tcp.fastopen.numkeys (RO)
 *     The current number of keys installed.
 *
 * net.inet.tcp.fastopen.setkey (WO)
 *     Install a new key by writing net.inet.tcp.fastopen.keylen bytes to this
 *     sysctl.
 *
 *
 * In order for TFO connections to be created via a listen socket, that
 * socket must have the TCP_FASTOPEN socket option set on it.  This option
 * can be set on the socket either before or after the listen() is invoked.
 * Clearing this option on a listen socket after it has been set has no
 * effect on existing TFO connections or TFO connections in progress; it
 * only prevents new TFO connections from being made.
 *
 * For passively-created sockets, the TCP_FASTOPEN socket option can be
 * queried to determine whether the connection was established using TFO.
 * Note that connections that are established via a TFO SYN, but that fall
 * back to using a non-TFO SYN|ACK will have the TCP_FASTOPEN socket option
 * set.
 *
 * Per the RFC, this implementation limits the number of TFO connections
 * that can be in the SYN_RECEIVED state on a per listen-socket basis.
 * Whenever this limit is exceeded, requests for new TFO connections are
 * serviced as non-TFO requests.  Without such a limit, given a valid TFO
 * cookie, an attacker could keep the listen queue in an overflow condition
 * using a TFO SYN flood.  This implementation sets the limit at half the
 * configured listen backlog.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <crypto/siphash/siphash.h>

#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_fastopen.h>
#include <netinet/tcp_var.h>


#define	TCP_FASTOPEN_KEY_LEN	SIPHASH_KEY_LENGTH

#if !defined(TCP_RFC7413_MAX_KEYS) || (TCP_RFC7413_MAX_KEYS < 1)
#define	TCP_FASTOPEN_MAX_KEYS	2
#else
#define	TCP_FASTOPEN_MAX_KEYS	TCP_RFC7413_MAX_KEYS
#endif

struct tcp_fastopen_keylist {
	unsigned int newest;
	uint8_t key[TCP_FASTOPEN_MAX_KEYS][TCP_FASTOPEN_KEY_LEN];
};

struct tcp_fastopen_callout {
	struct callout c;
	struct vnet *v;
};

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, fastopen, CTLFLAG_RW, 0, "TCP Fast Open");

static VNET_DEFINE(int, tcp_fastopen_acceptany) = 0;
#define	V_tcp_fastopen_acceptany	VNET(tcp_fastopen_acceptany)
SYSCTL_INT(_net_inet_tcp_fastopen, OID_AUTO, acceptany,
    CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(tcp_fastopen_acceptany), 0,
    "Accept any non-empty cookie");

static VNET_DEFINE(unsigned int, tcp_fastopen_autokey) = 120;
#define	V_tcp_fastopen_autokey	VNET(tcp_fastopen_autokey)
static int sysctl_net_inet_tcp_fastopen_autokey(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_net_inet_tcp_fastopen, OID_AUTO, autokey,
    CTLFLAG_VNET | CTLTYPE_UINT | CTLFLAG_RW, NULL, 0,
    &sysctl_net_inet_tcp_fastopen_autokey, "IU",
    "Number of seconds between auto-generation of a new key; zero disables");

VNET_DEFINE(unsigned int, tcp_fastopen_enabled) = 0;
static int sysctl_net_inet_tcp_fastopen_enabled(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_net_inet_tcp_fastopen, OID_AUTO, enabled,
    CTLFLAG_VNET | CTLTYPE_UINT | CTLFLAG_RW, NULL, 0,
    &sysctl_net_inet_tcp_fastopen_enabled, "IU",
    "Enable/disable TCP Fast Open processing");

SYSCTL_INT(_net_inet_tcp_fastopen, OID_AUTO, keylen,
    CTLFLAG_RD, SYSCTL_NULL_INT_PTR, TCP_FASTOPEN_KEY_LEN,
    "Key length in bytes");

SYSCTL_INT(_net_inet_tcp_fastopen, OID_AUTO, maxkeys,
    CTLFLAG_RD, SYSCTL_NULL_INT_PTR, TCP_FASTOPEN_MAX_KEYS,
    "Maximum number of keys supported");

static VNET_DEFINE(unsigned int, tcp_fastopen_numkeys) = 0;
#define	V_tcp_fastopen_numkeys	VNET(tcp_fastopen_numkeys)
SYSCTL_UINT(_net_inet_tcp_fastopen, OID_AUTO, numkeys,
    CTLFLAG_VNET | CTLFLAG_RD, &VNET_NAME(tcp_fastopen_numkeys), 0,
    "Number of keys installed");

static int sysctl_net_inet_tcp_fastopen_setkey(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_net_inet_tcp_fastopen, OID_AUTO, setkey,
    CTLFLAG_VNET | CTLTYPE_OPAQUE | CTLFLAG_WR, NULL, 0,
    &sysctl_net_inet_tcp_fastopen_setkey, "",
    "Install a new key");

static VNET_DEFINE(struct rmlock, tcp_fastopen_keylock);
#define	V_tcp_fastopen_keylock	VNET(tcp_fastopen_keylock)

#define TCP_FASTOPEN_KEYS_RLOCK(t)	rm_rlock(&V_tcp_fastopen_keylock, (t))
#define TCP_FASTOPEN_KEYS_RUNLOCK(t)	rm_runlock(&V_tcp_fastopen_keylock, (t))
#define TCP_FASTOPEN_KEYS_WLOCK()	rm_wlock(&V_tcp_fastopen_keylock)
#define TCP_FASTOPEN_KEYS_WUNLOCK()	rm_wunlock(&V_tcp_fastopen_keylock)

static VNET_DEFINE(struct tcp_fastopen_keylist, tcp_fastopen_keys);
#define V_tcp_fastopen_keys	VNET(tcp_fastopen_keys)

static VNET_DEFINE(struct tcp_fastopen_callout, tcp_fastopen_autokey_ctx);
#define V_tcp_fastopen_autokey_ctx	VNET(tcp_fastopen_autokey_ctx)

static VNET_DEFINE(uma_zone_t, counter_zone);
#define	V_counter_zone			VNET(counter_zone)

void
tcp_fastopen_init(void)
{
	V_counter_zone = uma_zcreate("tfo", sizeof(unsigned int),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	rm_init(&V_tcp_fastopen_keylock, "tfo_keylock");
	callout_init_rm(&V_tcp_fastopen_autokey_ctx.c,
	    &V_tcp_fastopen_keylock, 0);
	V_tcp_fastopen_keys.newest = TCP_FASTOPEN_MAX_KEYS - 1;
}

void
tcp_fastopen_destroy(void)
{
	callout_drain(&V_tcp_fastopen_autokey_ctx.c);
	rm_destroy(&V_tcp_fastopen_keylock);
	uma_zdestroy(V_counter_zone);
}

unsigned int *
tcp_fastopen_alloc_counter(void)
{
	unsigned int *counter;
	counter = uma_zalloc(V_counter_zone, M_NOWAIT);
	if (counter)
		*counter = 1;
	return (counter);
}

void
tcp_fastopen_decrement_counter(unsigned int *counter)
{
	if (*counter == 1)
		uma_zfree(V_counter_zone, counter);
	else
		atomic_subtract_int(counter, 1);
}

static void
tcp_fastopen_addkey_locked(uint8_t *key)
{

	V_tcp_fastopen_keys.newest++;
	if (V_tcp_fastopen_keys.newest == TCP_FASTOPEN_MAX_KEYS)
		V_tcp_fastopen_keys.newest = 0;
	memcpy(V_tcp_fastopen_keys.key[V_tcp_fastopen_keys.newest], key,
	    TCP_FASTOPEN_KEY_LEN);
	if (V_tcp_fastopen_numkeys < TCP_FASTOPEN_MAX_KEYS)
		V_tcp_fastopen_numkeys++;
}

static void
tcp_fastopen_autokey_locked(void)
{
	uint8_t newkey[TCP_FASTOPEN_KEY_LEN];

	arc4rand(newkey, TCP_FASTOPEN_KEY_LEN, 0);
	tcp_fastopen_addkey_locked(newkey);
}

static void
tcp_fastopen_autokey_callout(void *arg)
{
	struct tcp_fastopen_callout *ctx = arg;

	CURVNET_SET(ctx->v);
	tcp_fastopen_autokey_locked();
	callout_reset(&ctx->c, V_tcp_fastopen_autokey * hz,
		      tcp_fastopen_autokey_callout, ctx);
	CURVNET_RESTORE();
}


static uint64_t
tcp_fastopen_make_cookie(uint8_t key[SIPHASH_KEY_LENGTH], struct in_conninfo *inc)
{
	SIPHASH_CTX ctx;
	uint64_t siphash;

	SipHash24_Init(&ctx);
	SipHash_SetKey(&ctx, key);
	switch (inc->inc_flags & INC_ISIPV6) {
#ifdef INET
	case 0:
		SipHash_Update(&ctx, &inc->inc_faddr, sizeof(inc->inc_faddr));
		break;
#endif
#ifdef INET6
	case INC_ISIPV6:
		SipHash_Update(&ctx, &inc->inc6_faddr, sizeof(inc->inc6_faddr));
		break;
#endif
	}
	SipHash_Final((u_int8_t *)&siphash, &ctx);

	return (siphash);
}


/*
 * Return values:
 *	-1	the cookie is invalid and no valid cookie is available
 *	 0	the cookie is invalid and the latest cookie has been returned
 *	 1	the cookie is valid and the latest cookie has been returned
 */
int
tcp_fastopen_check_cookie(struct in_conninfo *inc, uint8_t *cookie,
    unsigned int len, uint64_t *latest_cookie)
{
	struct rm_priotracker tracker;
	unsigned int i, key_index;
	uint64_t cur_cookie;

	if (V_tcp_fastopen_acceptany) {
		*latest_cookie = 0;
		return (1);
	}

	if (len != TCP_FASTOPEN_COOKIE_LEN) {
		if (V_tcp_fastopen_numkeys > 0) {
			*latest_cookie =
			    tcp_fastopen_make_cookie(
				V_tcp_fastopen_keys.key[V_tcp_fastopen_keys.newest],
				inc);
			return (0);
		}
 		return (-1);
	}

	/*
	 * Check against each available key, from newest to oldest.
	 */
	TCP_FASTOPEN_KEYS_RLOCK(&tracker);
	key_index = V_tcp_fastopen_keys.newest;
	for (i = 0; i < V_tcp_fastopen_numkeys; i++) {
		cur_cookie =
		    tcp_fastopen_make_cookie(V_tcp_fastopen_keys.key[key_index],
			inc);
		if (i == 0)
			*latest_cookie = cur_cookie;
		if (memcmp(cookie, &cur_cookie, TCP_FASTOPEN_COOKIE_LEN) == 0) {
			TCP_FASTOPEN_KEYS_RUNLOCK(&tracker);
			return (1);
		}
		if (key_index == 0)
			key_index = TCP_FASTOPEN_MAX_KEYS - 1;
		else
			key_index--;
	}
	TCP_FASTOPEN_KEYS_RUNLOCK(&tracker);

	return (0);
}

static int
sysctl_net_inet_tcp_fastopen_autokey(SYSCTL_HANDLER_ARGS)
{
	int error;
	unsigned int new;

	new = V_tcp_fastopen_autokey;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		if (new > (INT_MAX / hz))
			return (EINVAL);

		TCP_FASTOPEN_KEYS_WLOCK();
		if (V_tcp_fastopen_enabled) {
			if (V_tcp_fastopen_autokey && !new)
				callout_stop(&V_tcp_fastopen_autokey_ctx.c);
			else if (new)
				callout_reset(&V_tcp_fastopen_autokey_ctx.c,
				    new * hz, tcp_fastopen_autokey_callout,
				    &V_tcp_fastopen_autokey_ctx);
		}
		V_tcp_fastopen_autokey = new;
		TCP_FASTOPEN_KEYS_WUNLOCK();
	}

	return (error);
}

static int
sysctl_net_inet_tcp_fastopen_enabled(SYSCTL_HANDLER_ARGS)
{
	int error;
	unsigned int new;

	new = V_tcp_fastopen_enabled;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		if (V_tcp_fastopen_enabled && !new) {
			/* enabled -> disabled */
			TCP_FASTOPEN_KEYS_WLOCK();
			V_tcp_fastopen_numkeys = 0;
			V_tcp_fastopen_keys.newest = TCP_FASTOPEN_MAX_KEYS - 1;
			if (V_tcp_fastopen_autokey)
				callout_stop(&V_tcp_fastopen_autokey_ctx.c);
			V_tcp_fastopen_enabled = 0;
			TCP_FASTOPEN_KEYS_WUNLOCK();
		} else if (!V_tcp_fastopen_enabled && new) {
			/* disabled -> enabled */
			TCP_FASTOPEN_KEYS_WLOCK();
			if (V_tcp_fastopen_autokey &&
			    (V_tcp_fastopen_numkeys == 0)) {
				tcp_fastopen_autokey_locked();
				callout_reset(&V_tcp_fastopen_autokey_ctx.c,
				    V_tcp_fastopen_autokey * hz,
				    tcp_fastopen_autokey_callout,
				    &V_tcp_fastopen_autokey_ctx);
			}
			V_tcp_fastopen_enabled = 1;
			TCP_FASTOPEN_KEYS_WUNLOCK();
		}
	}
	return (error);
}

static int
sysctl_net_inet_tcp_fastopen_setkey(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint8_t newkey[TCP_FASTOPEN_KEY_LEN];

	if (req->oldptr != NULL || req->oldlen != 0)
		return (EINVAL);
	if (req->newptr == NULL)
		return (EPERM);
	if (req->newlen != sizeof(newkey))
		return (EINVAL);
	error = SYSCTL_IN(req, newkey, sizeof(newkey));
	if (error)
		return (error);

	TCP_FASTOPEN_KEYS_WLOCK();
	tcp_fastopen_addkey_locked(newkey);
	TCP_FASTOPEN_KEYS_WUNLOCK();

	return (0);
}
