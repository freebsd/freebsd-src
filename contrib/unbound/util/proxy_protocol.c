/*
 * util/proxy_protocol.c - event notification
 *
 * Copyright (c) 2022, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file contains PROXY protocol functions.
 */
#include "config.h"
#include "util/log.h"
#include "util/proxy_protocol.h"

int
pp2_write_to_buf(struct sldns_buffer* buf, struct sockaddr_storage* src,
	int stream)
{
	int af;
	if(!src) return 0;
	af = (int)((struct sockaddr_in*)src)->sin_family;
	if(sldns_buffer_remaining(buf) <
		PP2_HEADER_SIZE + (af==AF_INET?12:36)) {
		return 0;
	}
	/* sig */
	sldns_buffer_write(buf, PP2_SIG, PP2_SIG_LEN);
	/* version and command */
	sldns_buffer_write_u8(buf, (PP2_VERSION << 4) | PP2_CMD_PROXY);
	if(af==AF_INET) {
		/* family and protocol */
		sldns_buffer_write_u8(buf,
			(PP2_AF_INET<<4) |
			(stream?PP2_PROT_STREAM:PP2_PROT_DGRAM));
		/* length */
		sldns_buffer_write_u16(buf, 12);
		/* src addr */
		sldns_buffer_write(buf,
			&((struct sockaddr_in*)src)->sin_addr.s_addr, 4);
		/* dst addr */
		sldns_buffer_write_u32(buf, 0);
		/* src port */
		sldns_buffer_write(buf,
			&((struct sockaddr_in*)src)->sin_port, 2);
		/* dst port */
		sldns_buffer_write_u16(buf, 0);
	} else {
		/* family and protocol */
		sldns_buffer_write_u8(buf,
			(PP2_AF_INET6<<4) |
			(stream?PP2_PROT_STREAM:PP2_PROT_DGRAM));
		/* length */
		sldns_buffer_write_u16(buf, 36);
		/* src addr */
		sldns_buffer_write(buf,
			&((struct sockaddr_in6*)src)->sin6_addr, 16);
		/* dst addr */
		sldns_buffer_set_at(buf,
			sldns_buffer_position(buf), 0, 16);
		sldns_buffer_skip(buf, 16);
		/* src port */
		sldns_buffer_write(buf,
			&((struct sockaddr_in6*)src)->sin6_port, 2);
		/* dst port */
		sldns_buffer_write_u16(buf, 0);
	}
	return 1;
}

struct pp2_header*
pp2_read_header(struct sldns_buffer* buf)
{
	size_t size;
	struct pp2_header* header = (struct pp2_header*)sldns_buffer_begin(buf);
	/* Try to fail all the unsupported cases first. */
	if(sldns_buffer_remaining(buf) < PP2_HEADER_SIZE) {
		log_err("proxy_protocol: not enough space for header");
		return NULL;
	}
	/* Check for PROXYv2 header */
	if(memcmp(header, PP2_SIG, PP2_SIG_LEN) != 0 ||
		((header->ver_cmd & 0xF0)>>4) != PP2_VERSION) {
		log_err("proxy_protocol: could not match PROXYv2 header");
		return NULL;
	}
	/* Check the length */
	size = PP2_HEADER_SIZE + ntohs(header->len);
	if(sldns_buffer_remaining(buf) < size) {
		log_err("proxy_protocol: not enough space for header");
		return NULL;
	}
	/* Check for supported commands */
	if((header->ver_cmd & 0xF) != PP2_CMD_LOCAL &&
		(header->ver_cmd & 0xF) != PP2_CMD_PROXY) {
		log_err("proxy_protocol: unsupported command");
		return NULL;
	}
	/* Check for supported family and protocol */
	if(header->fam_prot != 0x00 /* AF_UNSPEC|UNSPEC */ &&
		header->fam_prot != 0x11 /* AF_INET|STREAM */ &&
		header->fam_prot != 0x12 /* AF_INET|DGRAM */ &&
		header->fam_prot != 0x21 /* AF_INET6|STREAM */ &&
		header->fam_prot != 0x22 /* AF_INET6|DGRAM */) {
		log_err("proxy_protocol: unsupported family and protocol");
		return NULL;
	}
	/* We have a correct header */
	return header;
}
