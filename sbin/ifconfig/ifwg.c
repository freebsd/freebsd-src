/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Rubicon Communications, LLC (Netgate)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef RESCUE
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/nv.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/route.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <dev/if_wg/if_wg.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stddef.h>		/* NB: for offsetof */
#include <locale.h>
#include <langinfo.h>
#include <resolv.h>

#include "ifconfig.h"

static void wgfinish(int s, void *arg);

static bool wgfinish_registered;

static int allowed_ips_count;
static int allowed_ips_max;
static nvlist_t **allowed_ips, *nvl_peer;

#define	ALLOWEDIPS_START 16
#define	WG_KEY_SIZE_BASE64 ((((WG_KEY_SIZE) + 2) / 3) * 4 + 1)
#define	WG_KEY_SIZE_HEX (WG_KEY_SIZE * 2 + 1)
#define	WG_MAX_STRLEN 64

struct allowedip {
	union {
		struct in_addr ip4;
		struct in6_addr ip6;
	};
};

static void
register_wgfinish(void)
{

	if (wgfinish_registered)
		return;
	callback_register(wgfinish, NULL);
	wgfinish_registered = true;
}

static nvlist_t *
nvl_device(void)
{
	static nvlist_t *_nvl_device;

	if (_nvl_device == NULL)
		_nvl_device = nvlist_create(0);
	register_wgfinish();
	return (_nvl_device);
}

static bool
key_from_base64(uint8_t key[static WG_KEY_SIZE], const char *base64)
{

	if (strlen(base64) != WG_KEY_SIZE_BASE64 - 1) {
		warnx("bad key len - need %d got %zu\n", WG_KEY_SIZE_BASE64 - 1, strlen(base64));
		return false;
	}
	if (base64[WG_KEY_SIZE_BASE64 - 2] != '=') {
		warnx("bad key terminator, expected '=' got '%c'", base64[WG_KEY_SIZE_BASE64 - 2]);
		return false;
	}
	return (b64_pton(base64, key, WG_KEY_SIZE));
}

static void
parse_endpoint(const char *endpoint_)
{
	int err;
	char *base, *endpoint, *port, *colon, *tmp;
	struct addrinfo hints, *res;

	endpoint = base = strdup(endpoint_);
	colon = rindex(endpoint, ':');
	if (colon == NULL)
		errx(1, "bad endpoint format %s - no port delimiter found", endpoint);
	*colon = '\0';
	port = colon + 1;

	/* [::]:<> */
	if (endpoint[0] == '[') {
		endpoint++;
		tmp = index(endpoint, ']');
		if (tmp == NULL)
			errx(1, "bad endpoint format %s - '[' found with no matching ']'", endpoint);
		*tmp = '\0';
	}
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	err = getaddrinfo(endpoint, port, &hints, &res);
	if (err)
		errx(1, "%s", gai_strerror(err));
	nvlist_add_binary(nvl_peer, "endpoint", res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	free(base);
}

static void
in_len2mask(struct in_addr *mask, u_int len)
{
	u_int i;
	u_char *p;

	p = (u_char *)mask;
	memset(mask, 0, sizeof(*mask));
	for (i = 0; i < len / NBBY; i++)
		p[i] = 0xff;
	if (len % NBBY)
		p[i] = (0xff00 >> (len % NBBY)) & 0xff;
}

static u_int
in_mask2len(struct in_addr *mask)
{
	u_int x, y;
	u_char *p;

	p = (u_char *)mask;
	for (x = 0; x < sizeof(*mask); x++) {
		if (p[x] != 0xff)
			break;
	}
	y = 0;
	if (x < sizeof(*mask)) {
		for (y = 0; y < NBBY; y++) {
			if ((p[x] & (0x80 >> y)) == 0)
				break;
		}
	}
	return x * NBBY + y;
}

static void
in6_prefixlen2mask(struct in6_addr *maskp, int len)
{
	static const u_char maskarray[NBBY] = {0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
	int bytelen, bitlen, i;

	/* sanity check */
	if (len < 0 || len > 128) {
		errx(1, "in6_prefixlen2mask: invalid prefix length(%d)\n",
		    len);
		return;
	}

	memset(maskp, 0, sizeof(*maskp));
	bytelen = len / NBBY;
	bitlen = len % NBBY;
	for (i = 0; i < bytelen; i++)
		maskp->s6_addr[i] = 0xff;
	if (bitlen)
		maskp->s6_addr[bytelen] = maskarray[bitlen - 1];
}

static int
in6_mask2len(struct in6_addr *mask, u_char *lim0)
{
	int x = 0, y;
	u_char *lim = lim0, *p;

	/* ignore the scope_id part */
	if (lim0 == NULL || lim0 - (u_char *)mask > sizeof(*mask))
		lim = (u_char *)mask + sizeof(*mask);
	for (p = (u_char *)mask; p < lim; x++, p++) {
		if (*p != 0xff)
			break;
	}
	y = 0;
	if (p < lim) {
		for (y = 0; y < NBBY; y++) {
			if ((*p & (0x80 >> y)) == 0)
				break;
		}
	}

	/*
	 * when the limit pointer is given, do a stricter check on the
	 * remaining bits.
	 */
	if (p < lim) {
		if (y != 0 && (*p & (0x00ff >> y)) != 0)
			return -1;
		for (p = p + 1; p < lim; p++)
			if (*p != 0)
				return -1;
	}

	return x * NBBY + y;
}

static bool
parse_ip(struct allowedip *aip, uint16_t *family, const char *value)
{
	struct addrinfo hints, *res;
	int err;
	bool ret;

	ret = true;
	bzero(aip, sizeof(*aip));
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST;
	err = getaddrinfo(value, NULL, &hints, &res);
	if (err)
		errx(1, "%s", gai_strerror(err));

	*family = res->ai_family;
	if (res->ai_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;

		aip->ip4 = sin->sin_addr;
	} else if (res->ai_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)res->ai_addr;

		aip->ip6 = sin6->sin6_addr;
	} else {
		ret = false;
	}

	freeaddrinfo(res);
	return (ret);
}

static void
sa_ntop(const struct sockaddr *sa, char *buf, int *port)
{
	const struct sockaddr_in *sin;
	const struct sockaddr_in6 *sin6;
	int err;

	err = getnameinfo(sa, sa->sa_len, buf, INET6_ADDRSTRLEN, NULL,
	    0, NI_NUMERICHOST);

	if (sa->sa_family == AF_INET) {
		sin = (const struct sockaddr_in *)sa;
		if (port)
			*port = sin->sin_port;
	} else if (sa->sa_family == AF_INET6) {
		sin6 = (const struct sockaddr_in6 *)sa;
		if (port)
			*port = sin6->sin6_port;
	}

	if (err)
		errx(1, "%s", gai_strerror(err));
}

static void
dump_peer(const nvlist_t *nvl_peer_cfg)
{
	const void *key;
	const struct sockaddr *endpoint;
	char outbuf[WG_MAX_STRLEN];
	char addr_buf[INET6_ADDRSTRLEN];
	size_t aip_count, size;
	int port;
	uint16_t persistent_keepalive;
	const nvlist_t * const *nvl_aips;

	printf("[Peer]\n");
	if (nvlist_exists_binary(nvl_peer_cfg, "public-key")) {
		key = nvlist_get_binary(nvl_peer_cfg, "public-key", &size);
		b64_ntop((const uint8_t *)key, size, outbuf, WG_MAX_STRLEN);
		printf("PublicKey = %s\n", outbuf);
	}
	if (nvlist_exists_binary(nvl_peer_cfg, "preshared-key")) {
		key = nvlist_get_binary(nvl_peer_cfg, "preshared-key", &size);
		b64_ntop((const uint8_t *)key, size, outbuf, WG_MAX_STRLEN);
		printf("PresharedKey = %s\n", outbuf);
	}
	if (nvlist_exists_binary(nvl_peer_cfg, "endpoint")) {
		endpoint = nvlist_get_binary(nvl_peer_cfg, "endpoint", &size);
		sa_ntop(endpoint, addr_buf, &port);
		printf("Endpoint = %s:%d\n", addr_buf, ntohs(port));
	}
	if (nvlist_exists_number(nvl_peer_cfg,
	    "persistent-keepalive-interval")) {
		persistent_keepalive = nvlist_get_number(nvl_peer_cfg,
		    "persistent-keepalive-interval");
		printf("PersistentKeepalive = %d\n", persistent_keepalive);
	}
	if (!nvlist_exists_nvlist_array(nvl_peer_cfg, "allowed-ips"))
		return;

	nvl_aips = nvlist_get_nvlist_array(nvl_peer_cfg, "allowed-ips", &aip_count);
	if (nvl_aips == NULL || aip_count == 0)
		return;

	printf("AllowedIPs = ");
	for (size_t i = 0; i < aip_count; i++) {
		uint8_t cidr;
		struct sockaddr_storage ss;
		sa_family_t family;

		if (!nvlist_exists_number(nvl_aips[i], "cidr"))
			continue;
		cidr = nvlist_get_number(nvl_aips[i], "cidr");
		if (nvlist_exists_binary(nvl_aips[i], "ipv4")) {
			struct sockaddr_in *sin = (struct sockaddr_in *)&ss;
			const struct in_addr *ip4;

			ip4 = nvlist_get_binary(nvl_aips[i], "ipv4", &size);
			if (ip4 == NULL || cidr > 32)
				continue;
			sin->sin_len = sizeof(*sin);
			sin->sin_family = AF_INET;
			sin->sin_addr = *ip4;
		} else if (nvlist_exists_binary(nvl_aips[i], "ipv6")) {
			struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&ss;
			const struct in6_addr *ip6;

			ip6 = nvlist_get_binary(nvl_aips[i], "ipv6", &size);
			if (ip6 == NULL || cidr > 128)
				continue;
			sin6->sin6_len = sizeof(*sin6);
			sin6->sin6_family = AF_INET6;
			sin6->sin6_addr = *ip6;
		} else {
			continue;
		}

		family = ss.ss_family;
		getnameinfo((struct sockaddr *)&ss, ss.ss_len, addr_buf,
		    INET6_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);
		printf("%s/%d", addr_buf, cidr);
		if (i < aip_count - 1)
			printf(", ");
	}
	printf("\n");
}

static int
get_nvl_out_size(int sock, u_long op, size_t *size)
{
	struct wg_data_io wgd;
	int err;

	memset(&wgd, 0, sizeof(wgd));

	strlcpy(wgd.wgd_name, name, sizeof(wgd.wgd_name));
	wgd.wgd_size = 0;
	wgd.wgd_data = NULL;

	err = ioctl(sock, op, &wgd);
	if (err)
		return (err);
	*size = wgd.wgd_size;
	return (0);
}

static int
do_cmd(int sock, u_long op, void *arg, size_t argsize, int set)
{
	struct wg_data_io wgd;

	memset(&wgd, 0, sizeof(wgd));

	strlcpy(wgd.wgd_name, name, sizeof(wgd.wgd_name));
	wgd.wgd_size = argsize;
	wgd.wgd_data = arg;

	return (ioctl(sock, op, &wgd));
}

static
DECL_CMD_FUNC(peerlist, val, d)
{
	size_t size, peercount;
	void *packed;
	const nvlist_t *nvl;
	const nvlist_t *const *nvl_peerlist;

	if (get_nvl_out_size(s, SIOCGWG, &size))
		errx(1, "can't get peer list size");
	if ((packed = malloc(size)) == NULL)
		errx(1, "malloc failed for peer list");
	if (do_cmd(s, SIOCGWG, packed, size, 0))
		errx(1, "failed to obtain peer list");

	nvl = nvlist_unpack(packed, size, 0);
	if (!nvlist_exists_nvlist_array(nvl, "peers"))
		return;
	nvl_peerlist = nvlist_get_nvlist_array(nvl, "peers", &peercount);

	for (int i = 0; i < peercount; i++, nvl_peerlist++) {
		dump_peer(*nvl_peerlist);
	}
}

static void
wgfinish(int s, void *arg)
{
	void *packed;
	size_t size;
	static nvlist_t *nvl_dev;

	nvl_dev = nvl_device();
	if (nvl_peer != NULL) {
		if (!nvlist_exists_binary(nvl_peer, "public-key"))
			errx(1, "must specify a public-key for adding peer");
		if (allowed_ips_count != 0) {
			nvlist_add_nvlist_array(nvl_peer, "allowed-ips",
			    (const nvlist_t * const *)allowed_ips,
			    allowed_ips_count);
			for (size_t i = 0; i < allowed_ips_count; i++) {
				nvlist_destroy(allowed_ips[i]);
			}

			free(allowed_ips);
		}

		nvlist_add_nvlist_array(nvl_dev, "peers",
		    (const nvlist_t * const *)&nvl_peer, 1);
	}

	packed = nvlist_pack(nvl_dev, &size);

	if (do_cmd(s, SIOCSWG, packed, size, true))
		errx(1, "failed to configure");
}

static
DECL_CMD_FUNC(peerstart, val, d)
{

	if (nvl_peer != NULL)
		errx(1, "cannot both add and remove a peer");
	register_wgfinish();
	nvl_peer = nvlist_create(0);
	allowed_ips = calloc(ALLOWEDIPS_START, sizeof(*allowed_ips));
	allowed_ips_max = ALLOWEDIPS_START;
	if (allowed_ips == NULL)
		errx(1, "failed to allocate array for allowedips");
}

static
DECL_CMD_FUNC(peerdel, val, d)
{

	if (nvl_peer != NULL)
		errx(1, "cannot both add and remove a peer");
	register_wgfinish();
	nvl_peer = nvlist_create(0);
	nvlist_add_bool(nvl_peer, "remove", true);
}

static
DECL_CMD_FUNC(setwglistenport, val, d)
{
	struct addrinfo hints, *res;
	const struct sockaddr_in *sin;
	const struct sockaddr_in6 *sin6;

	u_long ul;
	int err;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST;
	err = getaddrinfo(NULL, val, &hints, &res);
	if (err)
		errx(1, "%s", gai_strerror(err));

	if (res->ai_family == AF_INET) {
		sin = (struct sockaddr_in *)res->ai_addr;
		ul = sin->sin_port;
	} else if (res->ai_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)res->ai_addr;
		ul = sin6->sin6_port;
	} else {
		errx(1, "unknown family");
	}
	ul = ntohs((u_short)ul);
	nvlist_add_number(nvl_device(), "listen-port", ul);
}

static
DECL_CMD_FUNC(setwgprivkey, val, d)
{
	uint8_t key[WG_KEY_SIZE];

	if (!key_from_base64(key, val))
		errx(1, "invalid key %s", val);
	nvlist_add_binary(nvl_device(), "private-key", key, WG_KEY_SIZE);
}

static
DECL_CMD_FUNC(setwgpubkey, val, d)
{
	uint8_t key[WG_KEY_SIZE];

	if (nvl_peer == NULL)
		errx(1, "setting public key only valid when adding peer");

	if (!key_from_base64(key, val))
		errx(1, "invalid key %s", val);
	nvlist_add_binary(nvl_peer, "public-key", key, WG_KEY_SIZE);
}

static
DECL_CMD_FUNC(setwgpresharedkey, val, d)
{
	uint8_t key[WG_KEY_SIZE];

	if (nvl_peer == NULL)
		errx(1, "setting preshared-key only valid when adding peer");

	if (!key_from_base64(key, val))
		errx(1, "invalid key %s", val);
	nvlist_add_binary(nvl_peer, "preshared-key", key, WG_KEY_SIZE);
}


static
DECL_CMD_FUNC(setwgpersistentkeepalive, val, d)
{
	unsigned long persistent_keepalive;
	char *endp;

	if (nvl_peer == NULL)
		errx(1, "setting persistent keepalive only valid when adding peer");

	errno = 0;
	persistent_keepalive = strtoul(val, &endp, 0);
	if (errno != 0 || *endp != '\0')
		errx(1, "persistent-keepalive must be numeric (seconds)");
	if (persistent_keepalive > USHRT_MAX)
		errx(1, "persistent-keepalive '%lu' too large",
		    persistent_keepalive);
	nvlist_add_number(nvl_peer, "persistent-keepalive-interval",
	    persistent_keepalive);
}

static
DECL_CMD_FUNC(setallowedips, val, d)
{
	char *base, *allowedip, *mask;
	u_long ul;
	char *endp;
	struct allowedip aip;
	nvlist_t *nvl_aip;
	uint16_t family;

	if (nvl_peer == NULL)
		errx(1, "setting allowed ip only valid when adding peer");
	if (allowed_ips_count == allowed_ips_max) {
		allowed_ips_max *= 2;
		allowed_ips = reallocarray(allowed_ips, allowed_ips_max,
		    sizeof(*allowed_ips));
		if (allowed_ips == NULL)
			errx(1, "failed to grow allowed ip array");
	}

	allowed_ips[allowed_ips_count] = nvl_aip = nvlist_create(0);
	if (nvl_aip == NULL)
		errx(1, "failed to create new allowedip nvlist");

	base = allowedip = strdup(val);
	mask = index(allowedip, '/');
	if (mask == NULL)
		errx(1, "mask separator not found in allowedip %s", val);
	*mask = '\0';
	mask++;

	parse_ip(&aip, &family, allowedip);
	ul = strtoul(mask, &endp, 0);
	if (*endp != '\0')
		errx(1, "invalid value for allowedip mask");

	nvlist_add_number(nvl_aip, "cidr", ul);
	if (family == AF_INET) {
		nvlist_add_binary(nvl_aip, "ipv4", &aip.ip4, sizeof(aip.ip4));
	} else if (family == AF_INET6) {
		nvlist_add_binary(nvl_aip, "ipv6", &aip.ip6, sizeof(aip.ip6));
	} else {
		/* Shouldn't happen */
		nvlist_destroy(nvl_aip);
		goto out;
	}

	allowed_ips_count++;

out:
	free(base);
}

static
DECL_CMD_FUNC(setendpoint, val, d)
{
	if (nvl_peer == NULL)
		errx(1, "setting endpoint only valid when adding peer");
	parse_endpoint(val);
}

static void
wireguard_status(int s)
{
	size_t size;
	void *packed;
	nvlist_t *nvl;
	char buf[WG_KEY_SIZE_BASE64];
	const void *key;
	uint16_t listen_port;

	if (get_nvl_out_size(s, SIOCGWG, &size))
		return;
	if ((packed = malloc(size)) == NULL)
		return;
	if (do_cmd(s, SIOCGWG, packed, size, 0))
		return;
	nvl = nvlist_unpack(packed, size, 0);
	if (nvlist_exists_number(nvl, "listen-port")) {
		listen_port = nvlist_get_number(nvl, "listen-port");
		printf("\tlisten-port: %d\n", listen_port);
	}
	if (nvlist_exists_binary(nvl, "private-key")) {
		key = nvlist_get_binary(nvl, "private-key", &size);
		b64_ntop((const uint8_t *)key, size, buf, WG_MAX_STRLEN);
		printf("\tprivate-key: %s\n", buf);
	}
	if (nvlist_exists_binary(nvl, "public-key")) {
		key = nvlist_get_binary(nvl, "public-key", &size);
		b64_ntop((const uint8_t *)key, size, buf, WG_MAX_STRLEN);
		printf("\tpublic-key:  %s\n", buf);
	}
}

static struct cmd wireguard_cmds[] = {
    DEF_CMD_ARG("listen-port",  setwglistenport),
    DEF_CMD_ARG("private-key",  setwgprivkey),
    /* XXX peer-list is deprecated. */
    DEF_CMD("peer-list",  0, peerlist),
    DEF_CMD("peers",  0, peerlist),
    DEF_CMD("peer",  0, peerstart),
    DEF_CMD("-peer",  0, peerdel),
    DEF_CMD_ARG("preshared-key",  setwgpresharedkey),
    DEF_CMD_ARG("public-key",  setwgpubkey),
    DEF_CMD_ARG("persistent-keepalive",  setwgpersistentkeepalive),
    DEF_CMD_ARG("allowed-ips",  setallowedips),
    DEF_CMD_ARG("endpoint",  setendpoint),
};

static struct afswtch af_wireguard = {
	.af_name	= "af_wireguard",
	.af_af		= AF_UNSPEC,
	.af_other_status = wireguard_status,
};

static void
wg_create(int s, struct ifreq *ifr)
{

	setproctitle("ifconfig %s create ...\n", name);

	ifr->ifr_data = NULL;
	if (ioctl(s, SIOCIFCREATE, ifr) < 0)
		err(1, "SIOCIFCREATE");
}

static __constructor void
wireguard_ctor(void)
{
	int i;

	for (i = 0; i < nitems(wireguard_cmds);  i++)
		cmd_register(&wireguard_cmds[i]);
	af_register(&af_wireguard);
	clone_setdefcallback_prefix("wg", wg_create);
}

#endif
