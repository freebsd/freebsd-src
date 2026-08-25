/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * A minimal regression test for a buffer overflow in alias_rtsp_out().
 */

#include <sys/types.h>
#include <sys/sbuf.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp_var.h>

#include <stdlib.h>
#include <string.h>

#include <alias.h>

int
main(void)
{
	uint8_t *packet;
	struct ip ip;
	struct tcphdr tcp;
	struct sbuf sb;
	struct libalias *la;

	sbuf_new(&sb, NULL, 0, SBUF_AUTOEXTEND);
	sbuf_printf(&sb, "SETUP rtsp://example.com/media.mp4 RTSP/1.0\r\n");
	sbuf_printf(&sb, "CSeq: 1\r\n");
	sbuf_printf(&sb, "Transport: RTP/AVP;unicast;");
	for (int i = 0; i < 200; i++)
		sbuf_printf(&sb, "client_port=%d-%d;", 2 * i, 2 * i + 1);
	sbuf_printf(&sb, "\r\n\r\n");
	sbuf_finish(&sb);

	memset(&tcp, 0, sizeof(tcp));
	tcp.th_sport = htons(1234);
	tcp.th_dport = htons(554);
	tcp.th_off = 5;

	memset(&ip, 0, sizeof(ip));
	ip.ip_v = IPVERSION;
	ip.ip_hl = sizeof(ip) / 4;
	ip.ip_len = htons(sizeof(ip) + sizeof(tcp) + sbuf_len(&sb));
	ip.ip_id = htons(1);
	ip.ip_ttl = 64;
	ip.ip_p = IPPROTO_TCP;
	ip.ip_src.s_addr = inet_addr("127.0.0.1");
	ip.ip_dst.s_addr = inet_addr("127.0.0.2");

	packet = malloc(sizeof(ip) + sizeof(tcp) + sbuf_len(&sb));
	memcpy(packet, &ip, sizeof(ip));
	memcpy(packet + sizeof(ip), &tcp, sizeof(tcp));
	memcpy(packet + sizeof(ip) + sizeof(tcp), sbuf_data(&sb),
	    sbuf_len(&sb));

	la = LibAliasInit(NULL);
	LibAliasSetAddress(la,
	    (struct in_addr){.s_addr = inet_addr("127.0.0.1")});
	if (LibAliasOut(la, packet, sizeof(ip) + sizeof(tcp) + sbuf_len(&sb)) !=
	    PKT_ALIAS_OK)
		return (1);
	LibAliasUninit(la);
	return (0);
}
