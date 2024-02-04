/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/udplite.h>
#include <netinet/sctp.h>
#include <netinet/sctp_header.h>

#include <err.h>
#include <sysexits.h>
#include <inttypes.h>
#include <poll.h>
#include <assert.h>
#include <stdlib.h>

#include "traceroute.h"

static void *
pkt_make_icmp6(struct context *ctx, uint8_t seq, size_t pktlen) {
	assert(pktlen >= sizeof(struct icmp6_hdr));

	void *pkt = calloc(1, pktlen);
	if (pkt == NULL)
		err(EX_OSERR, "calloc");
	
	struct icmp6_hdr *hdr = (struct icmp6_hdr *)pkt;

	hdr->icmp6_type = ICMP6_ECHO_REQUEST;
	hdr->icmp6_code = 0;
	hdr->icmp6_cksum = 0;
	hdr->icmp6_id = ctx->ident;
	hdr->icmp6_seq = htons(seq);

	return pkt;
}

static void *
pkt_make_icmp4(struct context *ctx, uint8_t seq, size_t pktlen) {
	assert(pktlen >= sizeof(struct icmp));

	void *pkt = calloc(1, pktlen);
	if (pkt == NULL)
		err(EX_OSERR, "calloc");
	
	struct icmp *hdr = (struct icmp *)pkt;

	hdr->icmp_type = ICMP_ECHO;
	hdr->icmp_code = 0;
	hdr->icmp_cksum = 0;
	hdr->icmp_id = ctx->ident;
	hdr->icmp_seq = htons(seq);
	hdr->icmp_cksum = in_cksum(pkt, pktlen);
	if (hdr->icmp_cksum == 0)
		hdr->icmp_cksum = 0xffffU;

	return pkt;
}

void *
pkt_make_icmp(struct context *ctx, uint8_t seq, size_t pktlen) {
	switch (ctx->destination->sa_family) {
	case AF_INET6:
		return pkt_make_icmp6(ctx, seq, pktlen);
	case AF_INET:
		return pkt_make_icmp4(ctx, seq, pktlen);
	default:
		abort();
	}
}

/*
 * CRC32C routine for the Stream Control Transmission Protocol
 */

#define CRC32C(c, d) ((c) = ((c) >> 8) ^ crc_c[((c) ^ (d)) & 0xFF])

static uint32_t crc_c[256] = {
	0x00000000, 0xF26B8303, 0xE13B70F7, 0x1350F3F4,
	0xC79A971F, 0x35F1141C, 0x26A1E7E8, 0xD4CA64EB,
	0x8AD958CF, 0x78B2DBCC, 0x6BE22838, 0x9989AB3B,
	0x4D43CFD0, 0xBF284CD3, 0xAC78BF27, 0x5E133C24,
	0x105EC76F, 0xE235446C, 0xF165B798, 0x030E349B,
	0xD7C45070, 0x25AFD373, 0x36FF2087, 0xC494A384,
	0x9A879FA0, 0x68EC1CA3, 0x7BBCEF57, 0x89D76C54,
	0x5D1D08BF, 0xAF768BBC, 0xBC267848, 0x4E4DFB4B,
	0x20BD8EDE, 0xD2D60DDD, 0xC186FE29, 0x33ED7D2A,
	0xE72719C1, 0x154C9AC2, 0x061C6936, 0xF477EA35,
	0xAA64D611, 0x580F5512, 0x4B5FA6E6, 0xB93425E5,
	0x6DFE410E, 0x9F95C20D, 0x8CC531F9, 0x7EAEB2FA,
	0x30E349B1, 0xC288CAB2, 0xD1D83946, 0x23B3BA45,
	0xF779DEAE, 0x05125DAD, 0x1642AE59, 0xE4292D5A,
	0xBA3A117E, 0x4851927D, 0x5B016189, 0xA96AE28A,
	0x7DA08661, 0x8FCB0562, 0x9C9BF696, 0x6EF07595,
	0x417B1DBC, 0xB3109EBF, 0xA0406D4B, 0x522BEE48,
	0x86E18AA3, 0x748A09A0, 0x67DAFA54, 0x95B17957,
	0xCBA24573, 0x39C9C670, 0x2A993584, 0xD8F2B687,
	0x0C38D26C, 0xFE53516F, 0xED03A29B, 0x1F682198,
	0x5125DAD3, 0xA34E59D0, 0xB01EAA24, 0x42752927,
	0x96BF4DCC, 0x64D4CECF, 0x77843D3B, 0x85EFBE38,
	0xDBFC821C, 0x2997011F, 0x3AC7F2EB, 0xC8AC71E8,
	0x1C661503, 0xEE0D9600, 0xFD5D65F4, 0x0F36E6F7,
	0x61C69362, 0x93AD1061, 0x80FDE395, 0x72966096,
	0xA65C047D, 0x5437877E, 0x4767748A, 0xB50CF789,
	0xEB1FCBAD, 0x197448AE, 0x0A24BB5A, 0xF84F3859,
	0x2C855CB2, 0xDEEEDFB1, 0xCDBE2C45, 0x3FD5AF46,
	0x7198540D, 0x83F3D70E, 0x90A324FA, 0x62C8A7F9,
	0xB602C312, 0x44694011, 0x5739B3E5, 0xA55230E6,
	0xFB410CC2, 0x092A8FC1, 0x1A7A7C35, 0xE811FF36,
	0x3CDB9BDD, 0xCEB018DE, 0xDDE0EB2A, 0x2F8B6829,
	0x82F63B78, 0x709DB87B, 0x63CD4B8F, 0x91A6C88C,
	0x456CAC67, 0xB7072F64, 0xA457DC90, 0x563C5F93,
	0x082F63B7, 0xFA44E0B4, 0xE9141340, 0x1B7F9043,
	0xCFB5F4A8, 0x3DDE77AB, 0x2E8E845F, 0xDCE5075C,
	0x92A8FC17, 0x60C37F14, 0x73938CE0, 0x81F80FE3,
	0x55326B08, 0xA759E80B, 0xB4091BFF, 0x466298FC,
	0x1871A4D8, 0xEA1A27DB, 0xF94AD42F, 0x0B21572C,
	0xDFEB33C7, 0x2D80B0C4, 0x3ED04330, 0xCCBBC033,
	0xA24BB5A6, 0x502036A5, 0x4370C551, 0xB11B4652,
	0x65D122B9, 0x97BAA1BA, 0x84EA524E, 0x7681D14D,
	0x2892ED69, 0xDAF96E6A, 0xC9A99D9E, 0x3BC21E9D,
	0xEF087A76, 0x1D63F975, 0x0E330A81, 0xFC588982,
	0xB21572C9, 0x407EF1CA, 0x532E023E, 0xA145813D,
	0x758FE5D6, 0x87E466D5, 0x94B49521, 0x66DF1622,
	0x38CC2A06, 0xCAA7A905, 0xD9F75AF1, 0x2B9CD9F2,
	0xFF56BD19, 0x0D3D3E1A, 0x1E6DCDEE, 0xEC064EED,
	0xC38D26C4, 0x31E6A5C7, 0x22B65633, 0xD0DDD530,
	0x0417B1DB, 0xF67C32D8, 0xE52CC12C, 0x1747422F,
	0x49547E0B, 0xBB3FFD08, 0xA86F0EFC, 0x5A048DFF,
	0x8ECEE914, 0x7CA56A17, 0x6FF599E3, 0x9D9E1AE0,
	0xD3D3E1AB, 0x21B862A8, 0x32E8915C, 0xC083125F,
	0x144976B4, 0xE622F5B7, 0xF5720643, 0x07198540,
	0x590AB964, 0xAB613A67, 0xB831C993, 0x4A5A4A90,
	0x9E902E7B, 0x6CFBAD78, 0x7FAB5E8C, 0x8DC0DD8F,
	0xE330A81A, 0x115B2B19, 0x020BD8ED, 0xF0605BEE,
	0x24AA3F05, 0xD6C1BC06, 0xC5914FF2, 0x37FACCF1,
	0x69E9F0D5, 0x9B8273D6, 0x88D28022, 0x7AB90321,
	0xAE7367CA, 0x5C18E4C9, 0x4F48173D, 0xBD23943E,
	0xF36E6F75, 0x0105EC76, 0x12551F82, 0xE03E9C81,
	0x34F4F86A, 0xC69F7B69, 0xD5CF889D, 0x27A40B9E,
	0x79B737BA, 0x8BDCB4B9, 0x988C474D, 0x6AE7C44E,
	0xBE2DA0A5, 0x4C4623A6, 0x5F16D052, 0xAD7D5351
};

uint32_t
sctp_crc32c(void *packet, uint32_t len)
{
	uint32_t i, crc32c;
	uint8_t byte0, byte1, byte2, byte3;
	uint8_t *buf = (uint8_t *)packet;

	crc32c = ~0;
	for (i = 0; i < len; i++)
		CRC32C(crc32c, buf[i]);
	crc32c = ~crc32c;
	byte0  = crc32c & 0xff;
	byte1  = (crc32c >> 8) & 0xff;
	byte2  = (crc32c >> 16) & 0xff;
	byte3  = (crc32c >> 24) & 0xff;
	crc32c = ((byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3);
	return (htonl(crc32c));
}

void *
pkt_make_sctp(struct context *ctx, uint8_t seq, size_t pktlen) {
	assert(pktlen >= sizeof(struct sctphdr));

	void *pkt = calloc(1, pktlen);
	if (pkt == NULL)
		err(EX_OSERR, "calloc");
	
	struct sctphdr *sctp = (struct sctphdr *)pkt;

	sctp->src_port = htons(ctx->ident);
	sctp->dest_port = htons(ctx->options->port + seq);

	if (pktlen >= (u_long)(sizeof(struct sctphdr) +
	    sizeof(struct sctp_init_chunk))) {
		sctp->v_tag = 0;
	} else {
		sctp->v_tag = (sctp->src_port << 16) | sctp->dest_port;
	}

	sctp->checksum = htonl(0);

	if (pktlen >= (size_t)(sizeof(struct sctphdr)
				+ sizeof(struct sctp_init_chunk))) {
		/*
		 * Send a packet containing an INIT chunk. This works
		 * better in case of firewalls on the path, but
		 * results in a probe packet containing at least
		 * 32 bytes of payload. For shorter payloads, use
		 * SHUTDOWN-ACK chunks.
		 */
		struct sctp_init_chunk *init =
			(struct sctp_init_chunk *)(sctp + 1);
		init->ch.chunk_type = SCTP_INITIATION;
		init->ch.chunk_flags = 0;
		init->ch.chunk_length = htons((uint16_t)(pktlen -
		    sizeof(struct sctphdr)));
		init->init.initiate_tag = (sctp->src_port << 16) |
		    sctp->dest_port;
		init->init.a_rwnd = htonl(1500);
		init->init.num_outbound_streams = htons(1);
		init->init.num_inbound_streams = htons(1);
		init->init.initial_tsn = htonl(0);
		if (pktlen >= (u_long)(sizeof(struct sctphdr) +
		    sizeof(struct sctp_init_chunk) +
		    sizeof(struct sctp_paramhdr))) {
			struct sctp_paramhdr *param =
				(struct sctp_paramhdr *)(init + 1);

			param->param_type = htons(SCTP_PAD);
			param->param_length =
			    htons((uint16_t)(pktlen -
			    sizeof(struct sctphdr) -
			    sizeof(struct sctp_init_chunk)));
		}
	} else {
		/*
		 * Send a packet containing a SHUTDOWN-ACK chunk,
		 * possibly followed by a PAD chunk.
		 */
		struct sctp_chunkhdr *chk = NULL;

		if (pktlen >= (u_long)(sizeof(struct sctphdr) +
		    sizeof(struct sctp_chunkhdr))) {
			chk = (struct sctp_chunkhdr *)(sctp + 1);
			chk->chunk_type = SCTP_SHUTDOWN_ACK;
			chk->chunk_flags = 0;
			chk->chunk_length = htons(4);
		}

		if (pktlen >= (u_long)(sizeof(struct sctphdr) +
		    2 * sizeof(struct sctp_chunkhdr))) {
			++chk;
			chk->chunk_type = SCTP_PAD_CHUNK;
			chk->chunk_flags = 0;
			chk->chunk_length = htons((uint16_t)(pktlen -
			    sizeof(struct sctphdr) -
			    sizeof(struct sctp_chunkhdr)));
		}
	}

	sctp->checksum = sctp_crc32c(pkt, pktlen);

	return pkt;
}

static uint16_t
tcp_chksum(struct context *ctx, void const *pkt, size_t len)
{
	struct {
		struct in6_addr src;
		struct in6_addr dst;
		uint32_t len;
		uint8_t zero[3];
		uint8_t next;
	} pseudo_hdr;
	uint16_t sum[2];

	struct sockaddr_in6 const *src =
		(struct sockaddr_in6 const *)&ctx->source;

	struct sockaddr_in6 const *dst =
		(struct sockaddr_in6 const *)&ctx->destination;

	pseudo_hdr.src = src->sin6_addr;
	pseudo_hdr.dst = dst->sin6_addr;
	pseudo_hdr.len = htonl(len);
	pseudo_hdr.zero[0] = 0;
	pseudo_hdr.zero[1] = 0;
	pseudo_hdr.zero[2] = 0;
	pseudo_hdr.next = IPPROTO_TCP;

	sum[1] = in_cksum((uint8_t const *)&pseudo_hdr, sizeof(pseudo_hdr));
	sum[0] = in_cksum((uint8_t const *)pkt, len);

	return (~in_cksum((uint8_t const *)sum, sizeof(sum)));
}

void *
pkt_make_tcp(struct context *ctx, uint8_t seq, size_t pktlen) {
	assert(pktlen >= sizeof(struct tcphdr));

	void *pkt = calloc(1, pktlen);
	if (pkt == NULL)
		err(EX_OSERR, "calloc");

	struct tcphdr *hdr = (struct tcphdr *)pkt;

	hdr->th_sport = htons(ctx->ident);
	hdr->th_dport = htons(ctx->options->port + seq);
	hdr->th_seq = (hdr->th_sport << 16) | hdr->th_dport;
	hdr->th_ack = 0;
	hdr->th_off = 5;
	hdr->th_flags = TH_SYN;
	hdr->th_sum = 0;
	hdr->th_sum = tcp_chksum(ctx, pkt, pktlen);

	return pkt;
}

static uint16_t
udp_cksum(struct context const *ctx, void const *payload, uint32_t len)
{
	struct {
		struct in6_addr src;
		struct in6_addr dst;
		uint32_t len;
		uint8_t zero[3];
		uint8_t next;
	} pseudo_hdr;
	uint16_t sum[2];

	struct sockaddr_in6 const *src =
		(struct sockaddr_in6 const *)&ctx->source;

	struct sockaddr_in6 const *dst =
		(struct sockaddr_in6 const *)&ctx->destination;

	pseudo_hdr.src = src->sin6_addr;
	pseudo_hdr.dst = dst->sin6_addr;
	pseudo_hdr.len = htonl(len);
	pseudo_hdr.zero[0] = 0;
	pseudo_hdr.zero[1] = 0;
	pseudo_hdr.zero[2] = 0;
	pseudo_hdr.next = IPPROTO_UDP;

	sum[1] = in_cksum((uint8_t const *)&pseudo_hdr, sizeof(pseudo_hdr));
	sum[0] = in_cksum((uint8_t const *)payload, len);

	return (~in_cksum((uint8_t const *)sum, sizeof(sum)));
}

void *
pkt_make_udp(struct context *ctx, uint8_t seq, size_t pktlen) {
	assert(pktlen >= sizeof(struct udphdr));

	void *pkt = calloc(1, pktlen);
	if (pkt == NULL)
		err(EX_OSERR, "calloc");
	
	struct udphdr *udp = (struct udphdr *)pkt;

	udp->uh_sport = htons(ctx->ident);
	udp->uh_dport = htons(ctx->options->port + seq);
	udp->uh_ulen = htons(pktlen);
	udp->uh_sum = 0;
	udp->uh_sum = udp_cksum(ctx, pkt, pktlen);

	return pkt;
}

void *
pkt_make_udplite(struct context *ctx, uint8_t seq, size_t pktlen) {
	assert(pktlen >= sizeof(struct udplitehdr));

	void *pkt = calloc(1, pktlen);
	if (pkt == NULL)
		err(EX_OSERR, "calloc");
	
	struct udplitehdr *hdr = (struct udplitehdr *)pkt;

	hdr->udplite_sport = htons(ctx->ident);
	hdr->udplite_dport = htons(ctx->options->port + seq);
	hdr->udplite_coverage = pktlen;
	hdr->udplite_checksum = udp_cksum(ctx, pkt, pktlen);

	return pkt;
}

void *
pkt_make_none(struct context *ctx, uint8_t seq, size_t pktlen) {
	if (pktlen == 0)
		pktlen = 1;

	void *pkt = calloc(1, pktlen);
	if (pkt == NULL)
		err(EX_OSERR, "calloc");
	
	return pkt;
}

void
send_probe(struct context *ctx, int seq, unsigned hops)
{
	if (hops > 255)
		errx(EX_SOFTWARE, "too many hops: %u > 255", hops);

	int i = hops;

	// set request hop limit (ttl for IPv4)
	switch (ctx->destination->sa_family) {
	case AF_INET6:
		if (setsockopt(ctx->sendsock, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
			       &i, sizeof(i)) < 0)
			err(EX_OSERR, "setsockopt(IPV6_UNICAST_HOPS)");
		break;

	case AF_INET:
		if (setsockopt(ctx->sendsock, IPPROTO_IP, IP_TTL,
			       &i, sizeof(i)) < 0)
			err(EX_OSERR, "setsockopt(IP_TTL)");
		break;

	default:
		abort();
	}

	void *pkt = ctx->protocol->pkt_make(ctx, seq, ctx->options->packetlen);

	ssize_t r = send(ctx->sendsock, pkt, ctx->options->packetlen, 0);
	if (r < 0 || (size_t)r != ctx->options->packetlen)  {
		if (r < 0)
			perror("send");
		(void)printf("wrote %"PRIu16" octets, ret=%zd\n",
			     ctx->options->packetlen, r);
		(void)fflush(stdout);
	}

	free(pkt);
}

int
wait_for_reply(struct context *ctx, struct msghdr *mhdr)
{
	struct pollfd pfd[1];
	int cc = 0;

	pfd[0].fd = ctx->rcvsock;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;

	if (poll(pfd, 1, ctx->options->wait_time * 1000) > 0 &&
	    pfd[0].revents & POLLIN)
		cc = recvmsg(ctx->rcvsock, mhdr, 0);

	return (cc);
}

