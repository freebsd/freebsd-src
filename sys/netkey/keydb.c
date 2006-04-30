/*	$KAME: keydb.c,v 1.82 2003/09/07 07:47:33 itojun Exp $	*/

/*-
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>

#include <net/pfkeyv2.h>
#include <netkey/keydb.h>
#include <netkey/key.h>
#include <netinet6/ipsec.h>

#include <net/net_osdep.h>

MALLOC_DEFINE(M_SECA, "key mgmt", "security associations, key management");

/*
 * secpolicy management
 */
struct secpolicy *
keydb_newsecpolicy()
{
	struct secpolicy *p;

	p = (struct secpolicy *)malloc(sizeof(*p), M_SECA, M_NOWAIT);
	if (!p)
		return p;
	bzero(p, sizeof(*p));
	TAILQ_INSERT_TAIL(&sptailq, p, tailq);

	return p;
}

u_int32_t
keydb_newspid(void)
{
	u_int32_t newid = 0;
	static u_int32_t lastalloc = IPSEC_MANUAL_POLICYID_MAX;
	struct secpolicy *sp;

	newid = lastalloc + 1;
	/* XXX possible infinite loop */
again:
	TAILQ_FOREACH(sp, &sptailq, tailq) {
		if (sp->id == newid)
			break;
	}
	if (sp != NULL) {
		if (newid + 1 < newid)	/* wraparound */
			newid = IPSEC_MANUAL_POLICYID_MAX + 1;
		else
			newid++;
		goto again;
	}
	lastalloc = newid;

	return newid;
}

void
keydb_delsecpolicy(p)
	struct secpolicy *p;
{

	TAILQ_REMOVE(&sptailq, p, tailq);
	if (p->spidx)
		free(p->spidx, M_SECA);
	free(p, M_SECA);
}

int
keydb_setsecpolicyindex(p, idx)
	struct secpolicy *p;
	struct secpolicyindex *idx;
{

	if (!p->spidx)
		p->spidx = (struct secpolicyindex *)malloc(sizeof(*p->spidx),
		    M_SECA, M_NOWAIT);
	if (!p->spidx)
		return ENOMEM;
	memcpy(p->spidx, idx, sizeof(*p->spidx));
	return 0;
}

/*
 * secashead management
 */
struct secashead *
keydb_newsecashead()
{
	struct secashead *p;
	int i;

	p = (struct secashead *)malloc(sizeof(*p), M_SECA, M_NOWAIT);
	if (!p)
		return p;
	bzero(p, sizeof(*p));
	for (i = 0; i < sizeof(p->savtree)/sizeof(p->savtree[0]); i++)
		LIST_INIT(&p->savtree[i]);
	return p;
}

void
keydb_delsecashead(p)
	struct secashead *p;
{

	free(p, M_SECA);
}

/*
 * secasvar management (reference counted)
 */
struct secasvar *
keydb_newsecasvar()
{
	struct secasvar *p, *q;
	static u_int32_t said = 0;

	p = (struct secasvar *)malloc(sizeof(*p), M_SECA, M_NOWAIT);
	if (!p)
		return p;

again:
	said++;
	if (said == 0)
		said++;
	TAILQ_FOREACH(q, &satailq, tailq) {
		if (q->id == said)
			goto again;
		if (TAILQ_NEXT(q, tailq)) {
			if (q->id < said && said < TAILQ_NEXT(q, tailq)->id)
				break;
			if (q->id + 1 < TAILQ_NEXT(q, tailq)->id) {
				said = q->id + 1;
				break;
			}
		}
	}

	bzero(p, sizeof(*p));
	p->id = said;
	if (q)
		TAILQ_INSERT_AFTER(&satailq, q, p, tailq);
	else
		TAILQ_INSERT_TAIL(&satailq, p, tailq);
	return p;
}

void
keydb_delsecasvar(p)
	struct secasvar *p;
{

	TAILQ_REMOVE(&satailq, p, tailq);

	free(p, M_SECA);
}

/*
 * secreplay management
 */
struct secreplay *
keydb_newsecreplay(wsize)
	size_t wsize;
{
	struct secreplay *p;

	p = (struct secreplay *)malloc(sizeof(*p), M_SECA, M_NOWAIT);
	if (!p)
		return p;

	bzero(p, sizeof(*p));
	if (wsize != 0) {
		p->bitmap = malloc(wsize, M_SECA, M_NOWAIT);
		if (!p->bitmap) {
			free(p, M_SECA);
			return NULL;
		}
		bzero(p->bitmap, wsize);
	}
	p->wsize = wsize;
	return p;
}

void
keydb_delsecreplay(p)
	struct secreplay *p;
{

	if (p->bitmap)
		free(p->bitmap, M_SECA);
	free(p, M_SECA);
}

/*
 * secreg management
 */
struct secreg *
keydb_newsecreg()
{
	struct secreg *p;

	p = (struct secreg *)malloc(sizeof(*p), M_SECA, M_NOWAIT);
	if (p)
		bzero(p, sizeof(*p));
	return p;
}

void
keydb_delsecreg(p)
	struct secreg *p;
{

	free(p, M_SECA);
}
