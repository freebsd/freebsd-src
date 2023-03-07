/*
 * util/proxy_protocol.h - PROXY protocol
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
 * This file contains PROXY protocol structs and functions.
 * Only v2 is supported. TLVs are not currently supported.
 */
#ifndef PROXY_PROTOCOL_H
#define PROXY_PROTOCOL_H

#include "sldns/sbuffer.h"

/** PROXYv2 minimum header size */
#define PP2_HEADER_SIZE 16

/** PROXYv2 header signature */
#define PP2_SIG "\x0D\x0A\x0D\x0A\x00\x0D\x0A\x51\x55\x49\x54\x0A"
#define PP2_SIG_LEN 12

/** PROXYv2 version */
#define PP2_VERSION 0x2

/**
 * PROXYv2 command.
 */
enum pp2_command {
	PP2_CMD_LOCAL = 0x0,
	PP2_CMD_PROXY = 0x1
};

/**
 * PROXYv2 address family.
 */
enum pp2_af {
	PP2_AF_UNSPEC = 0x0,
	PP2_AF_INET = 0x1,
	PP2_AF_INET6 = 0x2,
	PP2_AF_UNIX = 0x3
};

/**
 * PROXYv2 protocol.
 */
enum pp2_protocol {
	PP2_PROT_UNSPEC = 0x0,
	PP2_PROT_STREAM = 0x1,
	PP2_PROT_DGRAM = 0x2
};

/**
 * PROXYv2 header.
 */
struct pp2_header {
	uint8_t sig[PP2_SIG_LEN];
	uint8_t ver_cmd;
	uint8_t fam_prot;
	uint16_t len;
	union {
		struct {  /* for TCP/UDP over IPv4, len = 12 */
			uint32_t src_addr;
			uint32_t dst_addr;
			uint16_t src_port;
			uint16_t dst_port;
		} addr4;
		struct {  /* for TCP/UDP over IPv6, len = 36 */
			uint8_t  src_addr[16];
			uint8_t  dst_addr[16];
			uint16_t src_port;
			uint16_t dst_port;
		} addr6;
		struct {  /* for AF_UNIX sockets, len = 216 */
			uint8_t src_addr[108];
			uint8_t dst_addr[108];
		} addru;
	} addr;
};

/**
 * Write a PROXYv2 header at the current position of the buffer.
 * @param buf: the buffer to write to.
 * @param src: the source address.
 * @param stream: if the protocol is stream or datagram.
 * @return 1 on success, 0 on failure.
 */
int pp2_write_to_buf(struct sldns_buffer* buf, struct sockaddr_storage* src,
	int stream);

/**
 * Read a PROXYv2 header from the current position of the buffer.
 * It does initial validation and returns a pointer to the buffer position on
 * success.
 * @param buf: the buffer to read from.
 * @return the pointer to the buffer position on success, NULL on error.
 */
struct pp2_header* pp2_read_header(struct sldns_buffer* buf);

#endif /* PROXY_PROTOCOL_H */
