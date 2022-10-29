// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2020 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#include <arpa/inet.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>

#include "containers.h"
#include "ipc.h"
#include "terminal.h"
#include "encoding.h"
#include "subcommands.h"

static int peer_cmp(const void *first, const void *second)
{
	time_t diff;
	const struct wgpeer *a = *(const void **)first, *b = *(const void **)second;

	if (!a->last_handshake_time.tv_sec && !a->last_handshake_time.tv_nsec && (b->last_handshake_time.tv_sec || b->last_handshake_time.tv_nsec))
		return 1;
	if (!b->last_handshake_time.tv_sec && !b->last_handshake_time.tv_nsec && (a->last_handshake_time.tv_sec || a->last_handshake_time.tv_nsec))
		return -1;
	diff = a->last_handshake_time.tv_sec - b->last_handshake_time.tv_sec;
	if (!diff)
		diff = a->last_handshake_time.tv_nsec - b->last_handshake_time.tv_nsec;
	if (diff < 0)
		return 1;
	if (diff > 0)
		return -1;
	return 0;
}

/* This, hilariously, is not the right way to sort a linked list... */
static void sort_peers(struct wgdevice *device)
{
	size_t peer_count = 0, i = 0;
	struct wgpeer *peer, **peers;

	for_each_wgpeer(device, peer)
		++peer_count;
	if (!peer_count)
		return;
	peers = calloc(peer_count, sizeof(*peers));
	if (!peers)
		return;
	for_each_wgpeer(device, peer)
		peers[i++] = peer;
	qsort(peers, peer_count, sizeof(*peers), peer_cmp);
	device->first_peer = peers[0];
	for (i = 1; i < peer_count; ++i) {
		peers[i - 1]->next_peer = peers[i];
	}
	peers[peer_count - 1]->next_peer = NULL;
	free(peers);
}

static char *key(const uint8_t key[static WG_KEY_LEN])
{
	static char base64[WG_KEY_LEN_BASE64];

	key_to_base64(base64, key);
	return base64;
}

static const char *maybe_key(const uint8_t maybe_key[static WG_KEY_LEN], bool have_it)
{
	if (!have_it)
		return "(none)";
	return key(maybe_key);
}

static const char *masked_key(const uint8_t masked_key[static WG_KEY_LEN])
{
	const char *var = getenv("WG_HIDE_KEYS");

	if (var && !strcmp(var, "never"))
		return key(masked_key);
	return "(hidden)";
}

static char *ip(const struct wgallowedip *ip)
{
	static char buf[INET6_ADDRSTRLEN + 1];

	memset(buf, 0, INET6_ADDRSTRLEN + 1);
	if (ip->family == AF_INET)
		inet_ntop(AF_INET, &ip->ip4, buf, INET6_ADDRSTRLEN);
	else if (ip->family == AF_INET6)
		inet_ntop(AF_INET6, &ip->ip6, buf, INET6_ADDRSTRLEN);
	return buf;
}

static char *endpoint(const struct sockaddr *addr)
{
	char host[4096 + 1];
	char service[512 + 1];
	static char buf[sizeof(host) + sizeof(service) + 4];
	int ret;
	socklen_t addr_len = 0;

	memset(buf, 0, sizeof(buf));
	if (addr->sa_family == AF_INET)
		addr_len = sizeof(struct sockaddr_in);
	else if (addr->sa_family == AF_INET6)
		addr_len = sizeof(struct sockaddr_in6);

	ret = getnameinfo(addr, addr_len, host, sizeof(host), service, sizeof(service), NI_DGRAM | NI_NUMERICSERV | NI_NUMERICHOST);
	if (ret) {
		strncpy(buf, gai_strerror(ret), sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
	} else
		snprintf(buf, sizeof(buf), (addr->sa_family == AF_INET6 && strchr(host, ':')) ? "[%s]:%s" : "%s:%s", host, service);
	return buf;
}

static size_t pretty_time(char *buf, const size_t len, unsigned long long left)
{
	size_t offset = 0;
	unsigned long long years, days, hours, minutes, seconds;

	years = left / (365 * 24 * 60 * 60);
	left = left % (365 * 24 * 60 * 60);
	days = left / (24 * 60 * 60);
	left = left % (24 * 60 * 60);
	hours = left / (60 * 60);
	left = left % (60 * 60);
	minutes = left / 60;
	seconds = left % 60;

	if (years)
		offset += snprintf(buf + offset, len - offset, "%s%llu " TERMINAL_FG_CYAN "year%s" TERMINAL_RESET, offset ? ", " : "", years, years == 1 ? "" : "s");
	if (days)
		offset += snprintf(buf + offset, len - offset, "%s%llu " TERMINAL_FG_CYAN  "day%s" TERMINAL_RESET, offset ? ", " : "", days, days == 1 ? "" : "s");
	if (hours)
		offset += snprintf(buf + offset, len - offset, "%s%llu " TERMINAL_FG_CYAN  "hour%s" TERMINAL_RESET, offset ? ", " : "", hours, hours == 1 ? "" : "s");
	if (minutes)
		offset += snprintf(buf + offset, len - offset, "%s%llu " TERMINAL_FG_CYAN "minute%s" TERMINAL_RESET, offset ? ", " : "", minutes, minutes == 1 ? "" : "s");
	if (seconds)
		offset += snprintf(buf + offset, len - offset, "%s%llu " TERMINAL_FG_CYAN  "second%s" TERMINAL_RESET, offset ? ", " : "", seconds, seconds == 1 ? "" : "s");

	return offset;
}

static char *ago(const struct timespec64 *t)
{
	static char buf[1024];
	size_t offset;
	time_t now = time(NULL);

	if (now == t->tv_sec)
		strncpy(buf, "Now", sizeof(buf) - 1);
	else if (now < t->tv_sec)
		strncpy(buf, "(" TERMINAL_FG_RED "System clock wound backward; connection problems may ensue." TERMINAL_RESET ")", sizeof(buf) - 1);
	else {
		offset = pretty_time(buf, sizeof(buf), now - t->tv_sec);
		strncpy(buf + offset, " ago", sizeof(buf) - offset - 1);
	}
	buf[sizeof(buf) - 1] = '\0';

	return buf;
}

static char *every(uint16_t seconds)
{
	static char buf[1024] = "every ";

	pretty_time(buf + strlen("every "), sizeof(buf) - strlen("every ") - 1, seconds);
	return buf;
}

static char *bytes(uint64_t b)
{
	static char buf[1024];

	if (b < 1024ULL)
		snprintf(buf, sizeof(buf), "%u " TERMINAL_FG_CYAN "B" TERMINAL_RESET, (unsigned int)b);
	else if (b < 1024ULL * 1024ULL)
		snprintf(buf, sizeof(buf), "%.2f " TERMINAL_FG_CYAN "KiB" TERMINAL_RESET, (double)b / 1024);
	else if (b < 1024ULL * 1024ULL * 1024ULL)
		snprintf(buf, sizeof(buf), "%.2f " TERMINAL_FG_CYAN "MiB" TERMINAL_RESET, (double)b / (1024 * 1024));
	else if (b < 1024ULL * 1024ULL * 1024ULL * 1024ULL)
		snprintf(buf, sizeof(buf), "%.2f " TERMINAL_FG_CYAN "GiB" TERMINAL_RESET, (double)b / (1024 * 1024 * 1024));
	else
		snprintf(buf, sizeof(buf), "%.2f " TERMINAL_FG_CYAN "TiB" TERMINAL_RESET, (double)b / (1024 * 1024 * 1024) / 1024);

	return buf;
}

static const char *COMMAND_NAME;
static void show_usage(void)
{
	fprintf(stderr, "Usage: %s %s { <interface> | all | interfaces } [public-key | private-key | listen-port | fwmark | peers | preshared-keys | endpoints | allowed-ips | latest-handshakes | transfer | persistent-keepalive | dump]\n", PROG_NAME, COMMAND_NAME);
}

static void pretty_print(struct wgdevice *device)
{
	struct wgpeer *peer;
	struct wgallowedip *allowedip;

	terminal_printf(TERMINAL_RESET);
	terminal_printf(TERMINAL_FG_GREEN TERMINAL_BOLD "interface" TERMINAL_RESET ": " TERMINAL_FG_GREEN "%s" TERMINAL_RESET "\n", device->name);
	if (device->flags & WGDEVICE_HAS_PUBLIC_KEY)
		terminal_printf("  " TERMINAL_BOLD "public key" TERMINAL_RESET ": %s\n", key(device->public_key));
	if (device->flags & WGDEVICE_HAS_PRIVATE_KEY)
		terminal_printf("  " TERMINAL_BOLD "private key" TERMINAL_RESET ": %s\n", masked_key(device->private_key));
	if (device->listen_port)
		terminal_printf("  " TERMINAL_BOLD "listening port" TERMINAL_RESET ": %u\n", device->listen_port);
	if (device->fwmark)
		terminal_printf("  " TERMINAL_BOLD "fwmark" TERMINAL_RESET ": 0x%x\n", device->fwmark);
	if (device->first_peer) {
		sort_peers(device);
		terminal_printf("\n");
	}
	for_each_wgpeer(device, peer) {
		terminal_printf(TERMINAL_FG_YELLOW TERMINAL_BOLD "peer" TERMINAL_RESET ": " TERMINAL_FG_YELLOW "%s" TERMINAL_RESET "\n", key(peer->public_key));
		if (peer->flags & WGPEER_HAS_PRESHARED_KEY)
			terminal_printf("  " TERMINAL_BOLD "preshared key" TERMINAL_RESET ": %s\n", masked_key(peer->preshared_key));
		if (peer->endpoint.addr.sa_family == AF_INET || peer->endpoint.addr.sa_family == AF_INET6)
			terminal_printf("  " TERMINAL_BOLD "endpoint" TERMINAL_RESET ": %s\n", endpoint(&peer->endpoint.addr));
		terminal_printf("  " TERMINAL_BOLD "allowed ips" TERMINAL_RESET ": ");
		if (peer->first_allowedip) {
			for_each_wgallowedip(peer, allowedip)
				terminal_printf("%s" TERMINAL_FG_CYAN "/" TERMINAL_RESET "%u%s", ip(allowedip), allowedip->cidr, allowedip->next_allowedip ? ", " : "\n");
		} else
			terminal_printf("(none)\n");
		if (peer->last_handshake_time.tv_sec)
			terminal_printf("  " TERMINAL_BOLD "latest handshake" TERMINAL_RESET ": %s\n", ago(&peer->last_handshake_time));
		if (peer->rx_bytes || peer->tx_bytes) {
			terminal_printf("  " TERMINAL_BOLD "transfer" TERMINAL_RESET ": ");
			terminal_printf("%s received, ", bytes(peer->rx_bytes));
			terminal_printf("%s sent\n", bytes(peer->tx_bytes));
		}
		if (peer->persistent_keepalive_interval)
			terminal_printf("  " TERMINAL_BOLD "persistent keepalive" TERMINAL_RESET ": %s\n", every(peer->persistent_keepalive_interval));
		if (peer->next_peer)
			terminal_printf("\n");
	}
}

static void dump_print(struct wgdevice *device, bool with_interface)
{
	struct wgpeer *peer;
	struct wgallowedip *allowedip;

	if (with_interface)
		printf("%s\t", device->name);
	printf("%s\t", maybe_key(device->private_key, device->flags & WGDEVICE_HAS_PRIVATE_KEY));
	printf("%s\t", maybe_key(device->public_key, device->flags & WGDEVICE_HAS_PUBLIC_KEY));
	printf("%u\t", device->listen_port);
	if (device->fwmark)
		printf("0x%x\n", device->fwmark);
	else
		printf("off\n");
	for_each_wgpeer(device, peer) {
		if (with_interface)
			printf("%s\t", device->name);
		printf("%s\t", key(peer->public_key));
		printf("%s\t", maybe_key(peer->preshared_key, peer->flags & WGPEER_HAS_PRESHARED_KEY));
		if (peer->endpoint.addr.sa_family == AF_INET || peer->endpoint.addr.sa_family == AF_INET6)
			printf("%s\t", endpoint(&peer->endpoint.addr));
		else
			printf("(none)\t");
		if (peer->first_allowedip) {
			for_each_wgallowedip(peer, allowedip)
				printf("%s/%u%c", ip(allowedip), allowedip->cidr, allowedip->next_allowedip ? ',' : '\t');
		} else
			printf("(none)\t");
		printf("%llu\t", (unsigned long long)peer->last_handshake_time.tv_sec);
		printf("%" PRIu64 "\t%" PRIu64 "\t", (uint64_t)peer->rx_bytes, (uint64_t)peer->tx_bytes);
		if (peer->persistent_keepalive_interval)
			printf("%u\n", peer->persistent_keepalive_interval);
		else
			printf("off\n");
	}
}

static bool ugly_print(struct wgdevice *device, const char *param, bool with_interface)
{
	struct wgpeer *peer;
	struct wgallowedip *allowedip;

	if (!strcmp(param, "public-key")) {
		if (with_interface)
			printf("%s\t", device->name);
		printf("%s\n", maybe_key(device->public_key, device->flags & WGDEVICE_HAS_PUBLIC_KEY));
	} else if (!strcmp(param, "private-key")) {
		if (with_interface)
			printf("%s\t", device->name);
		printf("%s\n", maybe_key(device->private_key, device->flags & WGDEVICE_HAS_PRIVATE_KEY));
	} else if (!strcmp(param, "listen-port")) {
		if (with_interface)
			printf("%s\t", device->name);
		printf("%u\n", device->listen_port);
	} else if (!strcmp(param, "fwmark")) {
		if (with_interface)
			printf("%s\t", device->name);
		if (device->fwmark)
			printf("0x%x\n", device->fwmark);
		else
			printf("off\n");
	} else if (!strcmp(param, "endpoints")) {
		if (with_interface)
			printf("%s\t", device->name);
		for_each_wgpeer(device, peer) {
			printf("%s\t", key(peer->public_key));
			if (peer->endpoint.addr.sa_family == AF_INET || peer->endpoint.addr.sa_family == AF_INET6)
				printf("%s\n", endpoint(&peer->endpoint.addr));
			else
				printf("(none)\n");
		}
	} else if (!strcmp(param, "allowed-ips")) {
		for_each_wgpeer(device, peer) {
			if (with_interface)
				printf("%s\t", device->name);
			printf("%s\t", key(peer->public_key));
			if (peer->first_allowedip) {
				for_each_wgallowedip(peer, allowedip)
					printf("%s/%u%c", ip(allowedip), allowedip->cidr, allowedip->next_allowedip ? ' ' : '\n');
			} else
				printf("(none)\n");
		}
	} else if (!strcmp(param, "latest-handshakes")) {
		for_each_wgpeer(device, peer) {
			if (with_interface)
				printf("%s\t", device->name);
			printf("%s\t%llu\n", key(peer->public_key), (unsigned long long)peer->last_handshake_time.tv_sec);
		}
	} else if (!strcmp(param, "transfer")) {
		for_each_wgpeer(device, peer) {
			if (with_interface)
				printf("%s\t", device->name);
			printf("%s\t%" PRIu64 "\t%" PRIu64 "\n", key(peer->public_key), (uint64_t)peer->rx_bytes, (uint64_t)peer->tx_bytes);
		}
	} else if (!strcmp(param, "persistent-keepalive")) {
		for_each_wgpeer(device, peer) {
			if (with_interface)
				printf("%s\t", device->name);
			if (peer->persistent_keepalive_interval)
				printf("%s\t%u\n", key(peer->public_key), peer->persistent_keepalive_interval);
			else
				printf("%s\toff\n", key(peer->public_key));
		}
	} else if (!strcmp(param, "preshared-keys")) {
		for_each_wgpeer(device, peer) {
			if (with_interface)
				printf("%s\t", device->name);
			printf("%s\t", key(peer->public_key));
			printf("%s\n", maybe_key(peer->preshared_key, peer->flags & WGPEER_HAS_PRESHARED_KEY));
		}
	} else if (!strcmp(param, "peers")) {
		for_each_wgpeer(device, peer) {
			if (with_interface)
				printf("%s\t", device->name);
			printf("%s\n", key(peer->public_key));
		}
	} else if (!strcmp(param, "dump"))
		dump_print(device, with_interface);
	else {
		fprintf(stderr, "Invalid parameter: `%s'\n", param);
		show_usage();
		return false;
	}
	return true;
}

int show_main(int argc, const char *argv[])
{
	int ret = 0;

	COMMAND_NAME = argv[0];

	if (argc > 3) {
		show_usage();
		return 1;
	}

	if (argc == 1 || !strcmp(argv[1], "all")) {
		char *interfaces = ipc_list_devices(), *interface;

		if (!interfaces) {
			perror("Unable to list interfaces");
			return 1;
		}
		ret = !!*interfaces;
		interface = interfaces;
		for (size_t len = 0; (len = strlen(interface)); interface += len + 1) {
			struct wgdevice *device = NULL;

			if (ipc_get_device(&device, interface) < 0) {
				fprintf(stderr, "Unable to access interface %s: %s\n", interface, strerror(errno));
				continue;
			}
			if (argc == 3) {
				if (!ugly_print(device, argv[2], true)) {
					ret = 1;
					free_wgdevice(device);
					break;
				}
			} else {
				pretty_print(device);
				if (strlen(interface + len + 1))
					printf("\n");
			}
			free_wgdevice(device);
			ret = 0;
		}
		free(interfaces);
	} else if (!strcmp(argv[1], "interfaces")) {
		char *interfaces, *interface;

		if (argc > 2) {
			show_usage();
			return 1;
		}
		interfaces = ipc_list_devices();
		if (!interfaces) {
			perror("Unable to list interfaces");
			return 1;
		}
		interface = interfaces;
		for (size_t len = 0; (len = strlen(interface)); interface += len + 1)
			printf("%s%c", interface, strlen(interface + len + 1) ? ' ' : '\n');
		free(interfaces);
	} else if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "help")))
		show_usage();
	else {
		struct wgdevice *device = NULL;

		if (ipc_get_device(&device, argv[1]) < 0) {
			perror("Unable to access interface");
			return 1;
		}
		if (argc == 3) {
			if (!ugly_print(device, argv[2], false))
				ret = 1;
		} else
			pretty_print(device);
		free_wgdevice(device);
	}
	return ret;
}
