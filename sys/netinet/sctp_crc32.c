/*-
 * Copyright (c) 2001-2007, by Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $KAME: sctp_crc32.c,v 1.12 2005/03/06 16:04:17 itojun Exp $	 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <netinet/sctp_os.h>
#include <netinet/sctp.h>
#include <netinet/sctp_crc32.h>
#include <netinet/sctp_pcb.h>


#if !defined(SCTP_WITH_NO_CSUM)

static uint32_t
sctp_finalize_crc32c(uint32_t crc32c)
{
	uint32_t result;

#if BYTE_ORDER == BIG_ENDIAN
	uint8_t byte0, byte1, byte2, byte3;

#endif
	/* Complement the result */
	result = ~crc32c;
#if BYTE_ORDER == BIG_ENDIAN
	/*
	 * For BIG-ENDIAN.. aka Motorola byte order the result is in
	 * little-endian form. So we must manually swap the bytes. Then we
	 * can call htonl() which does nothing...
	 */
	byte0 = result & 0x000000ff;
	byte1 = (result >> 8) & 0x000000ff;
	byte2 = (result >> 16) & 0x000000ff;
	byte3 = (result >> 24) & 0x000000ff;
	crc32c = ((byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3);
#else
	/*
	 * For INTEL platforms the result comes out in network order. No
	 * htonl is required or the swap above. So we optimize out both the
	 * htonl and the manual swap above.
	 */
	crc32c = result;
#endif
	return (crc32c);
}

uint32_t
sctp_calculate_cksum(struct mbuf *m, uint32_t offset)
{
	/*
	 * given a mbuf chain with a packetheader offset by 'offset'
	 * pointing at a sctphdr (with csum set to 0) go through the chain
	 * of SCTP_BUF_NEXT()'s and calculate the SCTP checksum. This also
	 * has a side bonus as it will calculate the total length of the
	 * mbuf chain. Note: if offset is greater than the total mbuf
	 * length, checksum=1, pktlen=0 is returned (ie. no real error code)
	 */
	uint32_t base = 0xffffffff;
	struct mbuf *at;

	at = m;
	/* find the correct mbuf and offset into mbuf */
	while ((at != NULL) && (offset > (uint32_t) SCTP_BUF_LEN(at))) {
		offset -= SCTP_BUF_LEN(at);	/* update remaining offset
						 * left */
		at = SCTP_BUF_NEXT(at);
	}
	while (at != NULL) {
		if ((SCTP_BUF_LEN(at) - offset) > 0) {
			base = calculate_crc32c(base,
			    (unsigned char *)(SCTP_BUF_AT(at, offset)),
			    (unsigned int)(SCTP_BUF_LEN(at) - offset));
		}
		if (offset) {
			/* we only offset once into the first mbuf */
			if (offset < (uint32_t) SCTP_BUF_LEN(at))
				offset = 0;
			else
				offset -= SCTP_BUF_LEN(at);
		}
		at = SCTP_BUF_NEXT(at);
	}
	base = sctp_finalize_crc32c(base);
	return (base);
}

#endif				/* !defined(SCTP_WITH_NO_CSUM) */


void
sctp_delayed_cksum(struct mbuf *m, uint32_t offset)
{
#if defined(SCTP_WITH_NO_CSUM)
	panic("sctp_delayed_cksum() called when using no SCTP CRC.");
#else
	uint32_t checksum;

	checksum = sctp_calculate_cksum(m, offset);
	SCTP_STAT_DECR(sctps_sendhwcrc);
	SCTP_STAT_INCR(sctps_sendswcrc);
	offset += offsetof(struct sctphdr, checksum);

	if (offset + sizeof(uint32_t) > (uint32_t) (m->m_len)) {
		printf("sctp_delayed_cksum(): m->len: %d,  off: %d.\n",
		    (uint32_t) m->m_len, offset);
		/*
		 * XXX this shouldn't happen, but if it does, the correct
		 * behavior may be to insert the checksum in the appropriate
		 * next mbuf in the chain.
		 */
		return;
	}
	*(uint32_t *) (m->m_data + offset) = checksum;
#endif
}
