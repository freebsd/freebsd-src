/*-
 * Copyright 2005, Gleb Smirnoff <glebius@FreeBSD.org>
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
 * $FreeBSD: src/sys/netgraph/ng_ipfw.h,v 1.2 2006/02/17 09:42:49 glebius Exp $
 */

#define NG_IPFW_NODE_TYPE    "ipfw"
#define NGM_IPFW_COOKIE      1105988990

#ifdef _KERNEL

typedef int ng_ipfw_input_t(struct mbuf **, int, struct ip_fw_args *, int);
extern	ng_ipfw_input_t	*ng_ipfw_input_p;
#define	NG_IPFW_LOADED	(ng_ipfw_input_p != NULL)

struct ng_ipfw_tag {
	struct m_tag	mt;		/* tag header */
	struct ip_fw	*rule;		/* matching rule */
	struct ifnet	*ifp;		/* interface, for ip_output */
	int		dir;
#define	NG_IPFW_OUT	0
#define	NG_IPFW_IN	1
};

#define	TAGSIZ	(sizeof(struct ng_ipfw_tag) - sizeof(struct m_tag))

#endif /* _KERNEL */
