/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018-2024 Netflix
 * Author: Maksim Yevmenkin <maksim.yevmenkin@gmail.com>
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
 */

#include <sys/cdefs.h>
#define ACCEPT_FILTER_MOD

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/socketvar.h>

static int sbfull(struct sockbuf *sb);
static uint8_t sbmget8(struct mbuf *m, int offset);
static int so_hastls(struct socket *so, void *arg, int waitflag);

ACCEPT_FILTER_DEFINE(accf_tls, "tlsready", so_hastls, NULL, NULL, 1);

static int
sbfull(struct sockbuf *sb)
{

	return (sbused(sb) >= sb->sb_hiwat || sb->sb_mbcnt >= sb->sb_mbmax);
}

static uint8_t
sbmget8(struct mbuf *m, int offset)
{
	struct mbuf *n = m->m_nextpkt;

	while (m != NULL && offset >= m->m_len) {
		offset -= m->m_len;
		m = m->m_next;
		if (m == NULL) {
			m = n;
			n = m->m_nextpkt;
		}
	}

	return *(mtod(m, uint8_t *) + offset);
}

static int
so_hastls(struct socket *so, void *arg, int waitflag)
{
	struct sockbuf	*sb = &so->so_rcv;
	int		avail;
	uint16_t	reclen;

	if ((sb->sb_state & SBS_CANTRCVMORE) || sbfull(sb))
		return (SU_ISCONNECTED); /* can't wait any longer */

	/*
	 * struct {
	 * 	ContentType type;		- 1 byte, 0x16 handshake
	 * 	ProtocolVersion version;	- 2 bytes (major, minor)
	 * 	uint16 length;			- 2 bytes, NBO, 2^14 max
	 * 	opaque fragment[TLSPlaintext.length];
	 * } TLSPlaintext;
	 */

	/* Did we get at least 5 bytes */
	avail = sbavail(sb);
	if (avail < 5)
		return (SU_OK); /* nope */

	/* Does this look like TLS handshake? */
	if (sbmget8(sb->sb_mb, 0) != 0x16)
		return (SU_ISCONNECTED); /* nope */

	/* Did we get a complete TLS record? */
	reclen  = (uint16_t) sbmget8(sb->sb_mb, 3) << 8;
	reclen |= (uint16_t) sbmget8(sb->sb_mb, 4);

	if (reclen <= 16384 && avail < (int) 5 + reclen)
		return (SU_OK); /* nope */

	return (SU_ISCONNECTED);
}
