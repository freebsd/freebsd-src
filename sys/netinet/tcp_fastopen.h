/*-
 * Copyright (c) 2015 Patrick Kelsey
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
 * $FreeBSD$
 */

#ifndef _TCP_FASTOPEN_H_
#define _TCP_FASTOPEN_H_

#ifdef _KERNEL

#define	TCP_FASTOPEN_COOKIE_LEN	8	/* tied to SipHash24 64-bit output */

VNET_DECLARE(unsigned int, tcp_fastopen_enabled);
#define	V_tcp_fastopen_enabled	VNET(tcp_fastopen_enabled)

void	tcp_fastopen_init(void);
void	tcp_fastopen_destroy(void);
unsigned int *tcp_fastopen_alloc_counter(void);
void	tcp_fastopen_decrement_counter(unsigned int *counter);
int	tcp_fastopen_check_cookie(struct in_conninfo *inc, uint8_t *cookie,
	    unsigned int len, uint64_t *latest_cookie);
#endif /* _KERNEL */

#endif /* _TCP_FASTOPEN_H_ */
