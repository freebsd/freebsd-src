/*-
 * Copyright (c) 2003 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */

#ifndef _NETINET_IP_DIVERT_H_
#define	_NETINET_IP_DIVERT_H_

/*
 * Divert socket definitions.
 */

struct divert_tag {
	u_int32_t	info;		/* port & flags */
	u_int16_t	cookie;		/* ipfw rule number */
};

/*
 * Return the divert cookie associated with the mbuf; if any.
 */
static __inline u_int16_t
divert_cookie(struct m_tag *mtag)
{
	return ((struct divert_tag *)(mtag+1))->cookie;
}
static __inline u_int16_t
divert_find_cookie(struct mbuf *m)
{
	struct m_tag *mtag = m_tag_find(m, PACKET_TAG_DIVERT, NULL);
	return mtag ? divert_cookie(mtag) : 0;
}

/*
 * Return the divert info associated with the mbuf; if any.
 */
static __inline u_int32_t
divert_info(struct m_tag *mtag)
{
	return ((struct divert_tag *)(mtag+1))->info;
}
static __inline u_int32_t
divert_find_info(struct mbuf *m)
{
	struct m_tag *mtag = m_tag_find(m, PACKET_TAG_DIVERT, NULL);
	return mtag ? divert_info(mtag) : 0;
}

extern	void div_init(void);
extern	void div_input(struct mbuf *, int);
extern	void div_ctlinput(int, struct sockaddr *, void *);
extern	void divert_packet(struct mbuf *m, int incoming);
extern	struct mbuf *divert_clone(struct mbuf *);
extern struct pr_usrreqs div_usrreqs;
#endif /* _NETINET_IP_DIVERT_H_ */
