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

typedef enum {
	WGC_GET = 0x5,
	WGC_SET = 0x6,
} wg_cmd_t;

static nvlist_t *nvl_params;
static bool do_peer;
static int allowed_ips_count;
static int allowed_ips_max;
struct allowedip {
	struct sockaddr_storage a_addr;
	struct sockaddr_storage a_mask;
};
struct allowedip *allowed_ips;

#define	ALLOWEDIPS_START 16
#define	WG_KEY_LEN 32
#define	WG_KEY_LEN_BASE64 ((((WG_KEY_LEN) + 2) / 3) * 4 + 1)
#define	WG_KEY_LEN_HEX (WG_KEY_LEN * 2 + 1)
#define	WG_MAX_STRLEN 64

static bool
key_from_base64(uint8_t key[static WG_KEY_LEN], const char *base64)
{

	if (strlen(base64) != WG_KEY_LEN_BASE64 - 1) {
		warnx("bad key len - need %d got %zu\n", WG_KEY_LEN_BASE64 - 1, strlen(base64));
		return false;
	}
	if (base64[WG_KEY_LEN_BASE64 - 2] != '=') {
		warnx("bad key terminator, expected '=' got '%c'", base64[WG_KEY_LEN_BASE64 - 2]);
		return false;
	}
	return (b64_pton(base64, key, WG_KEY_LEN));
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
	nvlist_add_binary(nvl_params, "endpoint", res->ai_addr, res->ai_addrlen);
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
parse_ip(struct allowedip *aip, const char *value)
{
	struct addrinfo hints, *res;
	int err;

	bzero(&aip->a_addr, sizeof(aip->a_addr));
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST;
	err = getaddrinfo(value, NULL, &hints, &res);
	if (err)
		errx(1, "%s", gai_strerror(err));

	memcpy(&aip->a_addr, res->ai_addr, res->ai_addrlen);

	freeaddrinfo(res);
	return (true);
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
dump_peer(const nvlist_t *nvl_peer)
{
	const void *key;
	const struct allowedip *aips;
	const struct sockaddr *endpoint;
	char outbuf[WG_MAX_STRLEN];
	char addr_buf[INET6_ADDRSTRLEN];
	size_t size;
	int count, port;

	printf("[Peer]\n");
	if (nvlist_exists_binary(nvl_peer, "public-key")) {
		key = nvlist_get_binary(nvl_peer, "public-key", &size);
		b64_ntop((const uint8_t *)key, size, outbuf, WG_MAX_STRLEN);
		printf("PublicKey = %s\n", outbuf);
	}
	if (nvlist_exists_binary(nvl_peer, "endpoint")) {
		endpoint = nvlist_get_binary(nvl_peer, "endpoint", &size);
		sa_ntop(endpoint, addr_buf, &port);
		printf("Endpoint = %s:%d\n", addr_buf, ntohs(port));
	}

	if (!nvlist_exists_binary(nvl_peer, "allowed-ips"))
		return;
	aips = nvlist_get_binary(nvl_peer, "allowed-ips", &size);
	if (size == 0 || size % sizeof(struct allowedip) != 0) {
		errx(1, "size %zu not integer multiple of allowedip", size);
	}
	printf("AllowedIPs = ");
	count = size / sizeof(struct allowedip);
	for (int i = 0; i < count; i++) {
		int mask;
		sa_family_t family;
		void *bitmask;
		struct sockaddr *sa;

		sa = __DECONST(void *, &aips[i].a_addr);
		bitmask = __DECONST(void *,
		    ((const struct sockaddr *)&(&aips[i])->a_mask)->sa_data);
		family = aips[i].a_addr.ss_family;
		getnameinfo(sa, sa->sa_len, addr_buf, INET6_ADDRSTRLEN, NULL,
		    0, NI_NUMERICHOST);
		if (family == AF_INET)
			mask = in_mask2len(bitmask);
		else if (family == AF_INET6)
			mask = in6_mask2len(bitmask, NULL);
		else
			errx(1, "bad family in peer %d\n", family);
		printf("%s/%d", addr_buf, mask);
		if (i < count -1)
			printf(", ");
	}
	printf("\n");
}

static int
get_nvl_out_size(int sock, u_long op, size_t *size)
{
	struct ifdrv ifd;
	int err;

	memset(&ifd, 0, sizeof(ifd));

	strlcpy(ifd.ifd_name, name, sizeof(ifd.ifd_name));
	ifd.ifd_cmd = op;
	ifd.ifd_len = 0;
	ifd.ifd_data = NULL;

	err = ioctl(sock, SIOCGDRVSPEC, &ifd);
	if (err)
		return (err);
	*size = ifd.ifd_len;
	return (0);
}

static int
do_cmd(int sock, u_long op, void *arg, size_t argsize, int set)
{
	struct ifdrv ifd;

	memset(&ifd, 0, sizeof(ifd));

	strlcpy(ifd.ifd_name, name, sizeof(ifd.ifd_name));
	ifd.ifd_cmd = op;
	ifd.ifd_len = argsize;
	ifd.ifd_data = arg;

	return (ioctl(sock, set ? SIOCSDRVSPEC : SIOCGDRVSPEC, &ifd));
}

static
DECL_CMD_FUNC(peerlist, val, d)
{
	size_t size, peercount;
	void *packed;
	const nvlist_t *nvl, *nvl_peer;
	const nvlist_t *const *nvl_peerlist;

	if (get_nvl_out_size(s, WGC_GET, &size))
		errx(1, "can't get peer list size");
	if ((packed = malloc(size)) == NULL)
		errx(1, "malloc failed for peer list");
	if (do_cmd(s, WGC_GET, packed, size, 0))
		errx(1, "failed to obtain peer list");

	nvl = nvlist_unpack(packed, size, 0);
	if (!nvlist_exists_nvlist_array(nvl, "peer-list"))
		return;
	nvl_peerlist = nvlist_get_nvlist_array(nvl, "peer-list", &peercount);

	for (int i = 0; i < peercount; i++, nvl_peerlist++) {
		nvl_peer = *nvl_peerlist;
		dump_peer(nvl_peer);
	}
}

static void
peerfinish(int s, void *arg)
{
	nvlist_t *nvl, **nvl_array;
	void *packed;
	size_t size;

	if ((nvl = nvlist_create(0)) == NULL)
		errx(1, "failed to allocate nvlist");
	if ((nvl_array = calloc(sizeof(void *), 1)) == NULL)
		errx(1, "failed to allocate nvl_array");
	if (!nvlist_exists_binary(nvl_params, "public-key"))
		errx(1, "must specify a public-key for adding peer");
	if (!nvlist_exists_binary(nvl_params, "endpoint"))
		errx(1, "must specify an endpoint for adding peer");
	if (allowed_ips_count == 0)
		errx(1, "must specify at least one range of allowed-ips to add a peer");

	nvl_array[0] = nvl_params;
	nvlist_add_nvlist_array(nvl, "peer-list", (const nvlist_t * const *)nvl_array, 1);
	packed = nvlist_pack(nvl, &size);

	if (do_cmd(s, WGC_SET, packed, size, true))
		errx(1, "failed to install peer");
}

static
DECL_CMD_FUNC(peerstart, val, d)
{
	do_peer = true;
	callback_register(peerfinish, NULL);
	allowed_ips = malloc(ALLOWEDIPS_START * sizeof(struct allowedip));
	allowed_ips_max = ALLOWEDIPS_START;
	if (allowed_ips == NULL)
		errx(1, "failed to allocate array for allowedips");
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
	nvlist_add_number(nvl_params, "listen-port", ul);
}

static
DECL_CMD_FUNC(setwgprivkey, val, d)
{
	uint8_t key[WG_KEY_LEN];

	if (!key_from_base64(key, val))
		errx(1, "invalid key %s", val);
	nvlist_add_binary(nvl_params, "private-key", key, WG_KEY_LEN);
}

static
DECL_CMD_FUNC(setwgpubkey, val, d)
{
	uint8_t key[WG_KEY_LEN];

	if (!do_peer)
		errx(1, "setting public key only valid when adding peer");

	if (!key_from_base64(key, val))
		errx(1, "invalid key %s", val);
	nvlist_add_binary(nvl_params, "public-key", key, WG_KEY_LEN);
}

static
DECL_CMD_FUNC(setallowedips, val, d)
{
	char *base, *allowedip, *mask;
	u_long ul;
	char *endp;
	struct allowedip *aip;

	if (!do_peer)
		errx(1, "setting allowed ip only valid when adding peer");
	if (allowed_ips_count == allowed_ips_max) {
		/* XXX grow array */
	}
	aip = &allowed_ips[allowed_ips_count];
	base = allowedip = strdup(val);
	mask = index(allowedip, '/');
	if (mask == NULL)
		errx(1, "mask separator not found in allowedip %s", val);
	*mask = '\0';
	mask++;
	parse_ip(aip, allowedip);
	ul = strtoul(mask, &endp, 0);
	if (*endp != '\0')
		errx(1, "invalid value for allowedip mask");
	bzero(&aip->a_mask, sizeof(aip->a_mask));
	if (aip->a_addr.ss_family == AF_INET)
		in_len2mask((struct in_addr *)&((struct sockaddr *)&aip->a_mask)->sa_data, ul);
	else if (aip->a_addr.ss_family == AF_INET6)
		in6_prefixlen2mask((struct in6_addr *)&((struct sockaddr *)&aip->a_mask)->sa_data, ul);
	else
		errx(1, "invalid address family %d\n", aip->a_addr.ss_family);
	allowed_ips_count++;
	if (allowed_ips_count > 1)
		nvlist_free_binary(nvl_params, "allowed-ips");
	nvlist_add_binary(nvl_params, "allowed-ips", allowed_ips,
					  allowed_ips_count*sizeof(*aip));

	dump_peer(nvl_params);
	free(base);
}

static
DECL_CMD_FUNC(setendpoint, val, d)
{
	if (!do_peer)
		errx(1, "setting endpoint only valid when adding peer");
	parse_endpoint(val);
}

static void
wireguard_status(int s)
{
	size_t size;
	void *packed;
	nvlist_t *nvl;
	char buf[WG_KEY_LEN_BASE64];
	const void *key;
	uint16_t listen_port;

	if (get_nvl_out_size(s, WGC_GET, &size))
		return;
	if ((packed = malloc(size)) == NULL)
		return;
	if (do_cmd(s, WGC_GET, packed, size, 0))
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
    DEF_CLONE_CMD_ARG("listen-port",  setwglistenport),
    DEF_CLONE_CMD_ARG("private-key",  setwgprivkey),
    DEF_CMD("peer-list",  0, peerlist),
    DEF_CMD("peer",  0, peerstart),
    DEF_CMD_ARG("public-key",  setwgpubkey),
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
	struct iovec iov;
	void *packed;
	size_t size;

	setproctitle("ifconfig %s create ...\n", name);
	if (!nvlist_exists_number(nvl_params, "listen-port"))
		goto legacy;
	if (!nvlist_exists_binary(nvl_params, "private-key"))
		goto legacy;

	packed = nvlist_pack(nvl_params, &size);
	if (packed == NULL)
		errx(1, "failed to setup create request");
	iov.iov_len = size;
	iov.iov_base = packed;
	ifr->ifr_data = (caddr_t)&iov;
	if (ioctl(s, SIOCIFCREATE2, ifr) < 0)
		err(1, "SIOCIFCREATE2");
	return;
legacy:
	ifr->ifr_data == NULL;
	if (ioctl(s, SIOCIFCREATE, ifr) < 0)
		err(1, "SIOCIFCREATE");
}

static __constructor void
wireguard_ctor(void)
{
	int i;

	nvl_params = nvlist_create(0);
	for (i = 0; i < nitems(wireguard_cmds);  i++)
		cmd_register(&wireguard_cmds[i]);
	af_register(&af_wireguard);
	clone_setdefcallback_prefix("wg", wg_create);
}

#endif
