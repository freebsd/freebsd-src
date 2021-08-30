/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Netflix Inc.
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

#ifndef __OPENCRYPTO_KTLS_H__
#define	__OPENCRYPTO_KTLS_H__

#define	MAX_TLS_PAGES	(1 + btoc(TLS_MAX_MSG_SIZE_V10_2))

struct ktls_ocf_encrypt_state {
	struct socket *so;
	struct mbuf *m;
	void *cbuf;
	struct iovec dst_iov[MAX_TLS_PAGES + 2];
	vm_paddr_t parray[MAX_TLS_PAGES + 1];

	struct cryptop crp;
	struct uio uio;
	union {
		struct tls_mac_data mac;
		struct tls_aead_data aead;
		struct tls_aead_data_13 aead13;
	};
};

void ktls_encrypt_cb(struct ktls_ocf_encrypt_state *state, int error);
void ktls_ocf_free(struct ktls_session *tls);
int ktls_ocf_try(struct socket *so, struct ktls_session *tls, int direction);

#endif	/* !__OPENCRYPTO_KTLS_H__ */
