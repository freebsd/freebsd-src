// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2015-2021 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 */

#include <sys/nv.h>
#include <sys/sockio.h>
#include <dev/wg/if_wg.h>

#define IPC_SUPPORTS_KERNEL_INTERFACE

static int get_dgram_socket(void)
{
	static int sock = -1;
	if (sock < 0)
		sock = socket(AF_INET, SOCK_DGRAM, 0);
	return sock;
}

static int kernel_get_wireguard_interfaces(struct string_list *list)
{
	struct ifgroupreq ifgr = { .ifgr_name = "wg" };
	struct ifg_req *ifg;
	int s = get_dgram_socket(), ret = 0;

	if (s < 0)
		return -errno;

	if (ioctl(s, SIOCGIFGMEMB, (caddr_t)&ifgr) < 0)
		return errno == ENOENT ? 0 : -errno;

	ifgr.ifgr_groups = calloc(1, ifgr.ifgr_len);
	if (!ifgr.ifgr_groups)
		return -errno;
	if (ioctl(s, SIOCGIFGMEMB, (caddr_t)&ifgr) < 0) {
		ret = -errno;
		goto out;
	}

	for (ifg = ifgr.ifgr_groups; ifg && ifgr.ifgr_len > 0; ++ifg) {
		if ((ret = string_list_add(list, ifg->ifgrq_member)) < 0)
			goto out;
		ifgr.ifgr_len -= sizeof(struct ifg_req);
	}

out:
	free(ifgr.ifgr_groups);
	return ret;
}

static int kernel_get_device(struct wgdevice **device, const char *ifname)
{
	struct wg_data_io wgd = { 0 };
	nvlist_t *nvl_device = NULL;
	const nvlist_t *const *nvl_peers;
	struct wgdevice *dev = NULL;
	size_t size, peer_count, i;
	uint64_t number;
	const void *binary;
	int ret = 0, s;

	*device = NULL;
	s = get_dgram_socket();
	if (s < 0)
		goto err;

	strlcpy(wgd.wgd_name, ifname, sizeof(wgd.wgd_name));
	if (ioctl(s, SIOCGWG, &wgd) < 0)
		goto err;

	wgd.wgd_data = malloc(wgd.wgd_size);
	if (!wgd.wgd_data)
		goto err;
	if (ioctl(s, SIOCGWG, &wgd) < 0)
		goto err;

	dev = calloc(1, sizeof(*dev));
	if (!dev)
		goto err;
	strlcpy(dev->name, ifname, sizeof(dev->name));
	nvl_device = nvlist_unpack(wgd.wgd_data, wgd.wgd_size, 0);
	if (!nvl_device)
		goto err;

	if (nvlist_exists_number(nvl_device, "listen-port")) {
		number = nvlist_get_number(nvl_device, "listen-port");
		if (number <= UINT16_MAX) {
			dev->listen_port = number;
			dev->flags |= WGDEVICE_HAS_LISTEN_PORT;
		}
	}
	if (nvlist_exists_number(nvl_device, "user-cookie")) {
		number = nvlist_get_number(nvl_device, "user-cookie");
		if (number <= UINT32_MAX) {
			dev->fwmark = number;
			dev->flags |= WGDEVICE_HAS_FWMARK;
		}
	}
	if (nvlist_exists_binary(nvl_device, "public-key")) {
		binary = nvlist_get_binary(nvl_device, "public-key", &size);
		if (binary && size == sizeof(dev->public_key)) {
			memcpy(dev->public_key, binary, sizeof(dev->public_key));
			dev->flags |= WGDEVICE_HAS_PUBLIC_KEY;
		}
	}
	if (nvlist_exists_binary(nvl_device, "private-key")) {
		binary = nvlist_get_binary(nvl_device, "private-key", &size);
		if (binary && size == sizeof(dev->private_key)) {
			memcpy(dev->private_key, binary, sizeof(dev->private_key));
			dev->flags |= WGDEVICE_HAS_PRIVATE_KEY;
		}
	}
	if (!nvlist_exists_nvlist_array(nvl_device, "peers"))
		goto skip_peers;
	nvl_peers = nvlist_get_nvlist_array(nvl_device, "peers", &peer_count);
	if (!nvl_peers)
		goto skip_peers;
	for (i = 0; i < peer_count; ++i) {
		struct wgpeer *peer;
		struct wgallowedip *aip;
		const nvlist_t *const *nvl_aips;
		size_t aip_count, j;

		peer = calloc(1, sizeof(*peer));
		if (!peer)
			goto err_peer;
		if (nvlist_exists_binary(nvl_peers[i], "public-key")) {
			binary = nvlist_get_binary(nvl_peers[i], "public-key", &size);
			if (binary && size == sizeof(peer->public_key)) {
				memcpy(peer->public_key, binary, sizeof(peer->public_key));
				peer->flags |= WGPEER_HAS_PUBLIC_KEY;
			}
		}
		if (nvlist_exists_binary(nvl_peers[i], "preshared-key")) {
			binary = nvlist_get_binary(nvl_peers[i], "preshared-key", &size);
			if (binary && size == sizeof(peer->preshared_key)) {
				memcpy(peer->preshared_key, binary, sizeof(peer->preshared_key));
				if (!key_is_zero(peer->preshared_key))
					peer->flags |= WGPEER_HAS_PRESHARED_KEY;
			}
		}
		if (nvlist_exists_number(nvl_peers[i], "persistent-keepalive-interval")) {
			number = nvlist_get_number(nvl_peers[i], "persistent-keepalive-interval");
			if (number <= UINT16_MAX) {
				peer->persistent_keepalive_interval = number;
				peer->flags |= WGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL;
			}
		}
		if (nvlist_exists_binary(nvl_peers[i], "endpoint")) {
			const struct sockaddr *endpoint = nvlist_get_binary(nvl_peers[i], "endpoint", &size);
			if (endpoint && size <= sizeof(peer->endpoint) && size >= sizeof(peer->endpoint.addr) &&
			    (endpoint->sa_family == AF_INET || endpoint->sa_family == AF_INET6))
				memcpy(&peer->endpoint.addr, endpoint, size);
		}
		if (nvlist_exists_number(nvl_peers[i], "rx-bytes"))
			peer->rx_bytes = nvlist_get_number(nvl_peers[i], "rx-bytes");
		if (nvlist_exists_number(nvl_peers[i], "tx-bytes"))
			peer->tx_bytes = nvlist_get_number(nvl_peers[i], "tx-bytes");
		if (nvlist_exists_binary(nvl_peers[i], "last-handshake-time")) {
			binary = nvlist_get_binary(nvl_peers[i], "last-handshake-time", &size);
			if (binary && size == sizeof(peer->last_handshake_time))
				memcpy(&peer->last_handshake_time, binary, sizeof(peer->last_handshake_time));
		}

		if (!nvlist_exists_nvlist_array(nvl_peers[i], "allowed-ips"))
			goto skip_allowed_ips;
		nvl_aips = nvlist_get_nvlist_array(nvl_peers[i], "allowed-ips", &aip_count);
		if (!aip_count || !nvl_aips)
			goto skip_allowed_ips;
		for (j = 0; j < aip_count; ++j) {
			aip = calloc(1, sizeof(*aip));
			if (!aip)
				goto err_allowed_ips;
			if (!nvlist_exists_number(nvl_aips[j], "cidr"))
				continue;
			number = nvlist_get_number(nvl_aips[j], "cidr");
			if (nvlist_exists_binary(nvl_aips[j], "ipv4")) {
				binary = nvlist_get_binary(nvl_aips[j], "ipv4", &size);
				if (!binary || number > 32) {
					ret = EINVAL;
					goto err_allowed_ips;
				}
				aip->family = AF_INET;
				aip->cidr = number;
				memcpy(&aip->ip4, binary, sizeof(aip->ip4));
			} else if (nvlist_exists_binary(nvl_aips[j], "ipv6")) {
				binary = nvlist_get_binary(nvl_aips[j], "ipv6", &size);
				if (!binary || number > 128) {
					ret = EINVAL;
					goto err_allowed_ips;
				}
				aip->family = AF_INET6;
				aip->cidr = number;
				memcpy(&aip->ip6, binary, sizeof(aip->ip6));
			} else
				continue;

			if (!peer->first_allowedip)
				peer->first_allowedip = aip;
			else
				peer->last_allowedip->next_allowedip = aip;
			peer->last_allowedip = aip;
			continue;

		err_allowed_ips:
			if (!ret)
				ret = -errno;
			free(aip);
			goto err_peer;
		}
	skip_allowed_ips:
		if (!dev->first_peer)
			dev->first_peer = peer;
		else
			dev->last_peer->next_peer = peer;
		dev->last_peer = peer;
		continue;

	err_peer:
		if (!ret)
			ret = -errno;
		free(peer);
		goto err;
	}

skip_peers:
	free(wgd.wgd_data);
	nvlist_destroy(nvl_device);
	*device = dev;
	return 0;

err:
	if (!ret)
		ret = -errno;
	free(wgd.wgd_data);
	nvlist_destroy(nvl_device);
	free(dev);
	return ret;
}


static int kernel_set_device(struct wgdevice *dev)
{
	struct wg_data_io wgd = { 0 };
	nvlist_t *nvl_device = NULL, **nvl_peers = NULL;
	size_t peer_count = 0, i = 0;
	struct wgpeer *peer;
	int ret = 0, s;

	strlcpy(wgd.wgd_name, dev->name, sizeof(wgd.wgd_name));

	nvl_device = nvlist_create(0);
	if (!nvl_device)
		goto err;

	for_each_wgpeer(dev, peer)
		++peer_count;
	if (peer_count) {
		nvl_peers = calloc(peer_count, sizeof(*nvl_peers));
		if (!nvl_peers)
			goto err;
	}
	if (dev->flags & WGDEVICE_HAS_PRIVATE_KEY)
		nvlist_add_binary(nvl_device, "private-key", dev->private_key, sizeof(dev->private_key));
	if (dev->flags & WGDEVICE_HAS_LISTEN_PORT)
		nvlist_add_number(nvl_device, "listen-port", dev->listen_port);
	if (dev->flags & WGDEVICE_HAS_FWMARK)
		nvlist_add_number(nvl_device, "user-cookie", dev->fwmark);
	if (dev->flags & WGDEVICE_REPLACE_PEERS)
		nvlist_add_bool(nvl_device, "replace-peers", true);

	for_each_wgpeer(dev, peer) {
		size_t aip_count = 0, j = 0;
		nvlist_t **nvl_aips = NULL;
		struct wgallowedip *aip;

		nvl_peers[i]  = nvlist_create(0);
		if (!nvl_peers[i])
			goto err_peer;
		for_each_wgallowedip(peer, aip)
			++aip_count;
		if (aip_count) {
			nvl_aips = calloc(aip_count, sizeof(*nvl_aips));
			if (!nvl_aips)
				goto err_peer;
		}
		nvlist_add_binary(nvl_peers[i], "public-key", peer->public_key, sizeof(peer->public_key));
		if (peer->flags & WGPEER_HAS_PRESHARED_KEY)
			nvlist_add_binary(nvl_peers[i], "preshared-key", peer->preshared_key, sizeof(peer->preshared_key));
		if (peer->flags & WGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL)
			nvlist_add_number(nvl_peers[i], "persistent-keepalive-interval", peer->persistent_keepalive_interval);
		if (peer->endpoint.addr.sa_family == AF_INET || peer->endpoint.addr.sa_family == AF_INET6)
			nvlist_add_binary(nvl_peers[i], "endpoint", &peer->endpoint.addr, peer->endpoint.addr.sa_len);
		if (peer->flags & WGPEER_REPLACE_ALLOWEDIPS)
			nvlist_add_bool(nvl_peers[i], "replace-allowedips", true);
		if (peer->flags & WGPEER_REMOVE_ME)
			nvlist_add_bool(nvl_peers[i], "remove", true);
		for_each_wgallowedip(peer, aip) {
			nvl_aips[j] = nvlist_create(0);
			if (!nvl_aips[j])
				goto err_peer;
			nvlist_add_number(nvl_aips[j], "cidr", aip->cidr);
			if (aip->family == AF_INET)
				nvlist_add_binary(nvl_aips[j], "ipv4", &aip->ip4, sizeof(aip->ip4));
			else if (aip->family == AF_INET6)
				nvlist_add_binary(nvl_aips[j], "ipv6", &aip->ip6, sizeof(aip->ip6));
			++j;
		}
		if (j) {
			nvlist_add_nvlist_array(nvl_peers[i], "allowed-ips", (const nvlist_t *const *)nvl_aips, j);
			for (j = 0; j < aip_count; ++j)
				nvlist_destroy(nvl_aips[j]);
			free(nvl_aips);
		}
		++i;
		continue;

	err_peer:
		ret = -errno;
		for (j = 0; j < aip_count && nvl_aips; ++j)
			nvlist_destroy(nvl_aips[j]);
		free(nvl_aips);
		nvlist_destroy(nvl_peers[i]);
		goto err;
	}
	if (i) {
		nvlist_add_nvlist_array(nvl_device, "peers", (const nvlist_t *const *)nvl_peers, i);
		for (i = 0; i < peer_count; ++i)
			nvlist_destroy(nvl_peers[i]);
		free(nvl_peers);
	}
	wgd.wgd_data = nvlist_pack(nvl_device, &wgd.wgd_size);
	nvlist_destroy(nvl_device);
	if (!wgd.wgd_data)
		goto err;
	s = get_dgram_socket();
	if (s < 0)
		return -errno;
	return ioctl(s, SIOCSWG, &wgd);

err:
	if (!ret)
		ret = -errno;
	for (i = 0; i < peer_count && nvl_peers; ++i)
		nvlist_destroy(nvl_peers[i]);
	free(nvl_peers);
	nvlist_destroy(nvl_device);
	return ret;
}
