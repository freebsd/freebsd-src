/*-
 * Copyright (c) 2011 Adrian Chadd, Xenion Lty Ltd
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef	__IEEE80211_ALQ_H__
#define	__IEEE80211_ALQ_H__

#define	IEEE80211_ALQ_PAYLOAD_SIZE	24

/*
 * timestamp
 * wlan interface
 * operation
 * sub-operation
 * rest of structure - operation specific
 */
struct ieee80211_alq_rec {
	uint32_t	r_timestamp;	/* XXX may wrap! */
	uint16_t	r_wlan;		/* wlan interface number */
	uint8_t		r_version;	/* version */
	uint8_t		r_op;		/* top-level operation id */
	u_char		r_payload[IEEE80211_ALQ_PAYLOAD_SIZE];
					/* operation-specific payload */
};

/* General logging function */
extern void ieee80211_alq_log(struct ieee80211vap *vap, uint8_t op, u_char *p, int l);

#endif	/* __IEEE80211_ALQ_H__ */
