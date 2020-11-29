/*
 * Copyright (c) 2019 Matt Dunwoodie <ncon@noconroy.net>
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
 * $FreeBSD$
 */

#ifndef __IF_WG_H__
#define __IF_WG_H__

#include <net/if.h>
#include <netinet/in.h>

/*
 * This is the public interface to the WireGuard network interface.
 *
 * It is designed to be used by tools such as ifconfig(8) and wg(4).
 */

#define WG_KEY_SIZE 32

#define WG_DEVICE_HAS_PUBKEY		(1 << 0)
#define WG_DEVICE_HAS_PRIVKEY		(1 << 1)
#define WG_DEVICE_HAS_MASKED_PRIVKEY	(1 << 2)
#define WG_DEVICE_HAS_PORT		(1 << 3)
#define WG_DEVICE_HAS_RDOMAIN		(1 << 4)
#define WG_DEVICE_REPLACE_PEERS		(1 << 5)

#define WG_PEER_HAS_PUBKEY		(1 << 0)
#define WG_PEER_HAS_SHAREDKEY		(1 << 1)
#define WG_PEER_HAS_MASKED_SHAREDKEY	(1 << 2)
#define WG_PEER_HAS_ENDPOINT		(1 << 3)
#define WG_PEER_HAS_PERSISTENTKEEPALIVE	(1 << 4)
#define WG_PEER_REPLACE_CIDRS		(1 << 5)
#define WG_PEER_REMOVE			(1 << 6)

#define SIOCSWG _IOWR('i', 200, struct wg_device_io)
#define SIOCGWG _IOWR('i', 201, struct wg_device_io)

#define WG_PEERS_FOREACH(p, d) \
	for (p = (d)->d_peers; p < (d)->d_peers + (d)->d_num_peers; p++)
#define WG_CIDRS_FOREACH(c, p) \
	for (c = (p)->p_cidrs; c < (p)->p_cidrs + (p)->p_num_cidrs; c++)

struct wg_allowedip {
	struct sockaddr_storage a_addr;
	struct sockaddr_storage a_mask;
};

enum {
	WG_PEER_CTR_TX_BYTES,
	WG_PEER_CTR_RX_BYTES,
	WG_PEER_CTR_NUM,
};

struct wg_device_io {
	char			 d_name[IFNAMSIZ];
	uint8_t			 d_flags;
	in_port_t		 d_port;
	int			 d_rdomain;
	uint8_t			 d_pubkey[WG_KEY_SIZE];
	uint8_t			 d_privkey[WG_KEY_SIZE];
	size_t			 d_num_peers;
	size_t			 d_num_cidrs;
	struct wg_peer_io	*d_peers;
};


#ifndef ENOKEY
#define	ENOKEY	ENOTCAPABLE
#endif

typedef enum {
	WGC_GET = 0x5,
	WGC_SET = 0x6,
} wg_cmd_t;

#endif /* __IF_WG_H__ */
