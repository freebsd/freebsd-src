/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Alexander Motin <mav@FreeBSD.org>
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
 *
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/endian.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ctld.hh"
#include "isns.hh"

isns_req::isns_req(uint16_t func, uint16_t flags, const char *descr)
    : ir_descr(descr)
{
	struct isns_hdr hdr;

	be16enc(&hdr.ih_version, ISNS_VERSION);
	be16enc(&hdr.ih_function, func);
	be16enc(&hdr.ih_flags, flags);
	append(&hdr, sizeof(hdr));
}

void
isns_req::getspace(uint32_t len)
{
	ir_buf.reserve(ir_buf.size() + len);
}

void
isns_req::append(const void *buf, size_t len)
{
	const char *cp = reinterpret_cast<const char *>(buf);
	ir_buf.insert(ir_buf.end(), cp, cp + len);
}

void
isns_req::add(uint32_t tag, uint32_t len, const void *value)
{
	struct isns_tlv tlv;
	uint32_t vlen;

	vlen = roundup2(len, 4);
	getspace(sizeof(tlv) + vlen);
	be32enc(&tlv.it_tag, tag);
	be32enc(&tlv.it_length, vlen);
	append(&tlv, sizeof(tlv));
	append(value, len);
	if (vlen != len)
		ir_buf.insert(ir_buf.end(), vlen - len, 0);
}

void
isns_req::add_delim()
{
	add(0, 0, nullptr);
}

void
isns_req::add_str(uint32_t tag, const char *value)
{

	add(tag, strlen(value) + 1, value);
}

void
isns_req::add_32(uint32_t tag, uint32_t value)
{
	uint32_t beval;

	be32enc(&beval, value);
	add(tag, sizeof(value), &beval);
}

void
isns_req::add_addr(uint32_t tag, const struct addrinfo *ai)
{
	const struct sockaddr_in *in4;
	const struct sockaddr_in6 *in6;
	uint8_t buf[16];

	switch (ai->ai_addr->sa_family) {
	case AF_INET:
		in4 = (const struct sockaddr_in *)ai->ai_addr;
		memset(buf, 0, 10);
		buf[10] = 0xff;
		buf[11] = 0xff;
		memcpy(&buf[12], &in4->sin_addr, sizeof(in4->sin_addr));
		add(tag, sizeof(buf), buf);
		break;
	case AF_INET6:
		in6 = (const struct sockaddr_in6 *)ai->ai_addr;
		add(tag, sizeof(in6->sin6_addr), &in6->sin6_addr);
		break;
	default:
		log_errx(1, "Unsupported address family %d",
		    ai->ai_addr->sa_family);
	}
}

void
isns_req::add_port(uint32_t tag, const struct addrinfo *ai)
{
	const struct sockaddr_in *in4;
	const struct sockaddr_in6 *in6;
	uint32_t buf;

	switch (ai->ai_addr->sa_family) {
	case AF_INET:
		in4 = (const struct sockaddr_in *)ai->ai_addr;
		be32enc(&buf, ntohs(in4->sin_port));
		add(tag, sizeof(buf), &buf);
		break;
	case AF_INET6:
		in6 = (const struct sockaddr_in6 *)ai->ai_addr;
		be32enc(&buf, ntohs(in6->sin6_port));
		add(tag, sizeof(buf), &buf);
		break;
	default:
		log_errx(1, "Unsupported address family %d",
		    ai->ai_addr->sa_family);
	}
}

bool
isns_req::send(int s)
{
	struct isns_hdr *hdr;
	int res;

	hdr = (struct isns_hdr *)ir_buf.data();
	be16enc(hdr->ih_length, ir_buf.size() - sizeof(*hdr));
	be16enc(hdr->ih_flags, be16dec(hdr->ih_flags) |
	    ISNS_FLAG_LAST | ISNS_FLAG_FIRST);
	be16enc(hdr->ih_transaction, 0);
	be16enc(hdr->ih_sequence, 0);

	res = write(s, ir_buf.data(), ir_buf.size());
	return (res > 0 && (size_t)res == ir_buf.size());
}

bool
isns_req::receive(int s)
{
	struct isns_hdr *hdr;
	ssize_t res, len;

	ir_buf.resize(sizeof(*hdr));
	res = read(s, ir_buf.data(), sizeof(*hdr));
	if (res < (ssize_t)sizeof(*hdr)) {
		ir_buf.clear();
		return (false);
	}
	hdr = (struct isns_hdr *)ir_buf.data();
	if (be16dec(hdr->ih_version) != ISNS_VERSION)
		return (false);
	if ((be16dec(hdr->ih_flags) & (ISNS_FLAG_LAST | ISNS_FLAG_FIRST)) !=
	    (ISNS_FLAG_LAST | ISNS_FLAG_FIRST))
		return (false);
	len = be16dec(hdr->ih_length);
	ir_buf.resize(sizeof(*hdr) + len);
	res = read(s, ir_buf.data() + sizeof(*hdr), len);
	if (res < len)
		return (false);
	return (res == len);
}

uint32_t
isns_req::get_status()
{

	if (ir_buf.size() < sizeof(struct isns_hdr) + 4)
		return (-1);
	return (be32dec(&ir_buf[sizeof(struct isns_hdr)]));
}
