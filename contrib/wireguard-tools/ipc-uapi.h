// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2015-2020 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "containers.h"
#include "curve25519.h"
#include "encoding.h"
#include "ctype.h"

#ifdef _WIN32
#include "ipc-uapi-windows.h"
#else
#include "ipc-uapi-unix.h"
#endif

static int userspace_set_device(struct wgdevice *dev)
{
	char hex[WG_KEY_LEN_HEX], ip[INET6_ADDRSTRLEN], host[4096 + 1], service[512 + 1];
	struct wgpeer *peer;
	struct wgallowedip *allowedip;
	FILE *f;
	int ret, set_errno = -EPROTO;
	socklen_t addr_len;
	size_t line_buffer_len = 0, line_len;
	char *key = NULL, *value;

	f = userspace_interface_file(dev->name);
	if (!f)
		return -errno;
	fprintf(f, "set=1\n");

	if (dev->flags & WGDEVICE_HAS_PRIVATE_KEY) {
		key_to_hex(hex, dev->private_key);
		fprintf(f, "private_key=%s\n", hex);
	}
	if (dev->flags & WGDEVICE_HAS_LISTEN_PORT)
		fprintf(f, "listen_port=%u\n", dev->listen_port);
	if (dev->flags & WGDEVICE_HAS_FWMARK)
		fprintf(f, "fwmark=%u\n", dev->fwmark);
	if (dev->flags & WGDEVICE_REPLACE_PEERS)
		fprintf(f, "replace_peers=true\n");

	for_each_wgpeer(dev, peer) {
		key_to_hex(hex, peer->public_key);
		fprintf(f, "public_key=%s\n", hex);
		if (peer->flags & WGPEER_REMOVE_ME) {
			fprintf(f, "remove=true\n");
			continue;
		}
		if (peer->flags & WGPEER_HAS_PRESHARED_KEY) {
			key_to_hex(hex, peer->preshared_key);
			fprintf(f, "preshared_key=%s\n", hex);
		}
		if (peer->endpoint.addr.sa_family == AF_INET || peer->endpoint.addr.sa_family == AF_INET6) {
			addr_len = 0;
			if (peer->endpoint.addr.sa_family == AF_INET)
				addr_len = sizeof(struct sockaddr_in);
			else if (peer->endpoint.addr.sa_family == AF_INET6)
				addr_len = sizeof(struct sockaddr_in6);
			if (!getnameinfo(&peer->endpoint.addr, addr_len, host, sizeof(host), service, sizeof(service), NI_DGRAM | NI_NUMERICSERV | NI_NUMERICHOST)) {
				if (peer->endpoint.addr.sa_family == AF_INET6 && strchr(host, ':'))
					fprintf(f, "endpoint=[%s]:%s\n", host, service);
				else
					fprintf(f, "endpoint=%s:%s\n", host, service);
			}
		}
		if (peer->flags & WGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL)
			fprintf(f, "persistent_keepalive_interval=%u\n", peer->persistent_keepalive_interval);
		if (peer->flags & WGPEER_REPLACE_ALLOWEDIPS)
			fprintf(f, "replace_allowed_ips=true\n");
		for_each_wgallowedip(peer, allowedip) {
			if (allowedip->family == AF_INET) {
				if (!inet_ntop(AF_INET, &allowedip->ip4, ip, INET6_ADDRSTRLEN))
					continue;
			} else if (allowedip->family == AF_INET6) {
				if (!inet_ntop(AF_INET6, &allowedip->ip6, ip, INET6_ADDRSTRLEN))
					continue;
			} else
				continue;
			fprintf(f, "allowed_ip=%s/%d\n", ip, allowedip->cidr);
		}
	}
	fprintf(f, "\n");
	fflush(f);

	while (getline(&key, &line_buffer_len, f) > 0) {
		line_len = strlen(key);
		ret = set_errno;
		if (line_len == 1 && key[0] == '\n')
			goto out;
		value = strchr(key, '=');
		if (!value || line_len == 0 || key[line_len - 1] != '\n')
			break;
		*value++ = key[--line_len] = '\0';

		if (!strcmp(key, "errno")) {
			long long num;
			char *end;
			if (value[0] != '-' && !char_is_digit(value[0]))
				break;
			num = strtoll(value, &end, 10);
			if (*end || num > INT_MAX || num < INT_MIN)
				break;
			set_errno = num;
		}
	}
	ret = errno ? -errno : -EPROTO;
out:
	free(key);
	fclose(f);
	errno = -ret;
	return ret;
}

#define NUM(max) ({ \
	unsigned long long num; \
	char *end; \
	if (!char_is_digit(value[0])) \
		break; \
	num = strtoull(value, &end, 10); \
	if (*end || num > max) \
		break; \
	num; \
})

static int userspace_get_device(struct wgdevice **out, const char *iface)
{
	struct wgdevice *dev;
	struct wgpeer *peer = NULL;
	struct wgallowedip *allowedip = NULL;
	size_t line_buffer_len = 0, line_len;
	char *key = NULL, *value;
	FILE *f;
	int ret = -EPROTO;

	*out = dev = calloc(1, sizeof(*dev));
	if (!dev)
		return -errno;

	f = userspace_interface_file(iface);
	if (!f) {
		ret = -errno;
		free(dev);
		*out = NULL;
		return ret;
	}

	fprintf(f, "get=1\n\n");
	fflush(f);

	strncpy(dev->name, iface, IFNAMSIZ - 1);
	dev->name[IFNAMSIZ - 1] = '\0';

	while (getline(&key, &line_buffer_len, f) > 0) {
		line_len = strlen(key);
		if (line_len == 1 && key[0] == '\n')
			goto err;
		value = strchr(key, '=');
		if (!value || line_len == 0 || key[line_len - 1] != '\n')
			break;
		*value++ = key[--line_len] = '\0';

		if (!peer && !strcmp(key, "private_key")) {
			if (!key_from_hex(dev->private_key, value))
				break;
			curve25519_generate_public(dev->public_key, dev->private_key);
			dev->flags |= WGDEVICE_HAS_PRIVATE_KEY | WGDEVICE_HAS_PUBLIC_KEY;
		} else if (!peer && !strcmp(key, "listen_port")) {
			dev->listen_port = NUM(0xffffU);
			dev->flags |= WGDEVICE_HAS_LISTEN_PORT;
		} else if (!peer && !strcmp(key, "fwmark")) {
			dev->fwmark = NUM(0xffffffffU);
			dev->flags |= WGDEVICE_HAS_FWMARK;
		} else if (!strcmp(key, "public_key")) {
			struct wgpeer *new_peer = calloc(1, sizeof(*new_peer));

			if (!new_peer) {
				ret = -ENOMEM;
				goto err;
			}
			allowedip = NULL;
			if (peer)
				peer->next_peer = new_peer;
			else
				dev->first_peer = new_peer;
			peer = new_peer;
			if (!key_from_hex(peer->public_key, value))
				break;
			peer->flags |= WGPEER_HAS_PUBLIC_KEY;
		} else if (peer && !strcmp(key, "preshared_key")) {
			if (!key_from_hex(peer->preshared_key, value))
				break;
			if (!key_is_zero(peer->preshared_key))
				peer->flags |= WGPEER_HAS_PRESHARED_KEY;
		} else if (peer && !strcmp(key, "endpoint")) {
			char *begin, *end;
			struct addrinfo *resolved;
			struct addrinfo hints = {
				.ai_family = AF_UNSPEC,
				.ai_socktype = SOCK_DGRAM,
				.ai_protocol = IPPROTO_UDP
			};
			if (!strlen(value))
				break;
			if (value[0] == '[') {
				begin = &value[1];
				end = strchr(value, ']');
				if (!end)
					break;
				*end++ = '\0';
				if (*end++ != ':' || !*end)
					break;
			} else {
				begin = value;
				end = strrchr(value, ':');
				if (!end || !*(end + 1))
					break;
				*end++ = '\0';
			}
			if (getaddrinfo(begin, end, &hints, &resolved) != 0) {
				ret = ENETUNREACH;
				goto err;
			}
			if ((resolved->ai_family == AF_INET && resolved->ai_addrlen == sizeof(struct sockaddr_in)) ||
			    (resolved->ai_family == AF_INET6 && resolved->ai_addrlen == sizeof(struct sockaddr_in6)))
				memcpy(&peer->endpoint.addr, resolved->ai_addr, resolved->ai_addrlen);
			else  {
				freeaddrinfo(resolved);
				break;
			}
			freeaddrinfo(resolved);
		} else if (peer && !strcmp(key, "persistent_keepalive_interval")) {
			peer->persistent_keepalive_interval = NUM(0xffffU);
			peer->flags |= WGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL;
		} else if (peer && !strcmp(key, "allowed_ip")) {
			struct wgallowedip *new_allowedip;
			char *end, *mask = value, *ip = strsep(&mask, "/");

			if (!mask || !char_is_digit(mask[0]))
				break;
			new_allowedip = calloc(1, sizeof(*new_allowedip));
			if (!new_allowedip) {
				ret = -ENOMEM;
				goto err;
			}
			if (allowedip)
				allowedip->next_allowedip = new_allowedip;
			else
				peer->first_allowedip = new_allowedip;
			allowedip = new_allowedip;
			allowedip->family = AF_UNSPEC;
			if (strchr(ip, ':')) {
				if (inet_pton(AF_INET6, ip, &allowedip->ip6) == 1)
					allowedip->family = AF_INET6;
			} else {
				if (inet_pton(AF_INET, ip, &allowedip->ip4) == 1)
					allowedip->family = AF_INET;
			}
			allowedip->cidr = strtoul(mask, &end, 10);
			if (*end || allowedip->family == AF_UNSPEC || (allowedip->family == AF_INET6 && allowedip->cidr > 128) || (allowedip->family == AF_INET && allowedip->cidr > 32))
				break;
		} else if (peer && !strcmp(key, "last_handshake_time_sec"))
			peer->last_handshake_time.tv_sec = NUM(0x7fffffffffffffffULL);
		else if (peer && !strcmp(key, "last_handshake_time_nsec"))
			peer->last_handshake_time.tv_nsec = NUM(0x7fffffffffffffffULL);
		else if (peer && !strcmp(key, "rx_bytes"))
			peer->rx_bytes = NUM(0xffffffffffffffffULL);
		else if (peer && !strcmp(key, "tx_bytes"))
			peer->tx_bytes = NUM(0xffffffffffffffffULL);
		else if (!strcmp(key, "errno"))
			ret = -NUM(0x7fffffffU);
	}
	ret = -EPROTO;
err:
	free(key);
	if (ret) {
		free_wgdevice(dev);
		*out = NULL;
	}
	fclose(f);
	errno = -ret;
	return ret;

}
#undef NUM
