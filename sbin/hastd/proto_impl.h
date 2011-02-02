/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_PROTO_IMPL_H_
#define	_PROTO_IMPL_H_

#include <sys/queue.h>

#include <stdbool.h>	/* bool */
#include <stdlib.h>	/* size_t */

#define	__constructor	__attribute__((constructor))

typedef int hp_client_t(const char *, void **);
typedef int hp_connect_t(void *, int);
typedef int hp_connect_wait_t(void *, int);
typedef int hp_server_t(const char *, void **);
typedef int hp_accept_t(void *, void **);
typedef int hp_wrap_t(int, bool, void **);
typedef int hp_send_t(void *, const unsigned char *, size_t, int);
typedef int hp_recv_t(void *, unsigned char *, size_t, int *);
typedef int hp_descriptor_t(const void *);
typedef bool hp_address_match_t(const void *, const char *);
typedef void hp_local_address_t(const void *, char *, size_t);
typedef void hp_remote_address_t(const void *, char *, size_t);
typedef void hp_close_t(void *);

struct hast_proto {
	const char		*hp_name;
	hp_client_t		*hp_client;
	hp_connect_t		*hp_connect;
	hp_connect_wait_t	*hp_connect_wait;
	hp_server_t		*hp_server;
	hp_accept_t		*hp_accept;
	hp_wrap_t		*hp_wrap;
	hp_send_t		*hp_send;
	hp_recv_t		*hp_recv;
	hp_descriptor_t		*hp_descriptor;
	hp_address_match_t	*hp_address_match;
	hp_local_address_t	*hp_local_address;
	hp_remote_address_t	*hp_remote_address;
	hp_close_t		*hp_close;
	TAILQ_ENTRY(hast_proto)	 hp_next;
};

void proto_register(struct hast_proto *proto, bool isdefault);

int proto_common_send(int sock, const unsigned char *data, size_t size, int fd);
int proto_common_recv(int sock, unsigned char *data, size_t size, int *fdp);

#endif	/* !_PROTO_IMPL_H_ */
