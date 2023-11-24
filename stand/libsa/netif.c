/*	$NetBSD: netif.c,v 1.10 1997/09/06 13:57:14 drochner Exp $	*/

/*
 * Copyright (c) 1993 Adam Glass
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <string.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#include "stand.h"
#include "net.h"
#include "netif.h"

typedef TAILQ_HEAD(socket_list, iodesc) socket_list_t;

/*
 * Open socket list. The current implementation and assumption is,
 * we only remove entries from tail and we only add new entries to tail.
 * This decision is to keep iodesc id management simple - we get list
 * entries ordered by continiously growing io_id field.
 * If we do have multiple sockets open and we do close socket not from tail,
 * this entry will be marked unused. netif_open() will reuse unused entry, or
 * netif_close() will free all unused tail entries.
 */
static socket_list_t sockets = TAILQ_HEAD_INITIALIZER(sockets);

#ifdef NETIF_DEBUG
int netif_debug = 0;
#endif

/*
 * netif_init:
 *
 * initialize the generic network interface layer
 */

void
netif_init(void)
{
	struct netif_driver *drv;
	int d, i;

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("netif_init: called\n");
#endif
	for (d = 0; netif_drivers[d]; d++) {
		drv = netif_drivers[d];
		for (i = 0; i < drv->netif_nifs; i++)
			drv->netif_ifs[i].dif_used = 0;
	}
}

int
netif_match(struct netif *nif, void *machdep_hint)
{
	struct netif_driver *drv = nif->nif_driver;

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("%s%d: netif_match (%d)\n", drv->netif_bname,
		    nif->nif_unit, nif->nif_sel);
#endif
	return drv->netif_match(nif, machdep_hint);
}

struct netif *
netif_select(void *machdep_hint)
{
	int d, u, s;
	struct netif_driver *drv;
	struct netif cur_if;
	static struct netif best_if;
	int best_val;
	int val;

	best_val = 0;
	best_if.nif_driver = NULL;

	for (d = 0; netif_drivers[d] != NULL; d++) {
		cur_if.nif_driver = netif_drivers[d];
		drv = cur_if.nif_driver;

		for (u = 0; u < drv->netif_nifs; u++) {
			cur_if.nif_unit = u;

#ifdef NETIF_DEBUG
			if (netif_debug)
				printf("\t%s%d:", drv->netif_bname,
				    cur_if.nif_unit);
#endif

			for (s = 0; s < drv->netif_ifs[u].dif_nsel; s++) {
				cur_if.nif_sel = s;

				if (drv->netif_ifs[u].dif_used & (1 << s)) {
#ifdef NETIF_DEBUG
					if (netif_debug)
						printf(" [%d used]", s);
#endif
					continue;
				}

				val = netif_match(&cur_if, machdep_hint);
#ifdef NETIF_DEBUG
				if (netif_debug)
					printf(" [%d -> %d]", s, val);
#endif
				if (val > best_val) {
					best_val = val;
					best_if = cur_if;
				}
			}
#ifdef NETIF_DEBUG
			if (netif_debug)
				printf("\n");
#endif
		}
	}

	if (best_if.nif_driver == NULL)
		return NULL;

	best_if.nif_driver->
	    netif_ifs[best_if.nif_unit].dif_used |= (1 << best_if.nif_sel);

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("netif_select: %s%d(%d) wins\n",
			best_if.nif_driver->netif_bname,
			best_if.nif_unit, best_if.nif_sel);
#endif
	return &best_if;
}

int
netif_probe(struct netif *nif, void *machdep_hint)
{
	struct netif_driver *drv = nif->nif_driver;

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("%s%d: netif_probe\n", drv->netif_bname, nif->nif_unit);
#endif
	return drv->netif_probe(nif, machdep_hint);
}

void
netif_attach(struct netif *nif, struct iodesc *desc, void *machdep_hint)
{
	struct netif_driver *drv = nif->nif_driver;

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("%s%d: netif_attach\n", drv->netif_bname, nif->nif_unit);
#endif
	desc->io_netif = nif;
#ifdef PARANOID
	if (drv->netif_init == NULL)
		panic("%s%d: no netif_init support", drv->netif_bname,
		    nif->nif_unit);
#endif
	drv->netif_init(desc, machdep_hint);
	bzero(drv->netif_ifs[nif->nif_unit].dif_stats,
	    sizeof(struct netif_stats));
}

void
netif_detach(struct netif *nif)
{
	struct netif_driver *drv = nif->nif_driver;

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("%s%d: netif_detach\n", drv->netif_bname, nif->nif_unit);
#endif
#ifdef PARANOID
	if (drv->netif_end == NULL)
		panic("%s%d: no netif_end support", drv->netif_bname,
		    nif->nif_unit);
#endif
	drv->netif_end(nif);
}

ssize_t
netif_get(struct iodesc *desc, void **pkt, time_t timo)
{
#ifdef NETIF_DEBUG
	struct netif *nif = desc->io_netif;
#endif
	struct netif_driver *drv = desc->io_netif->nif_driver;
	ssize_t rv;

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("%s%d: netif_get\n", drv->netif_bname, nif->nif_unit);
#endif
#ifdef PARANOID
	if (drv->netif_get == NULL)
		panic("%s%d: no netif_get support", drv->netif_bname,
		    nif->nif_unit);
#endif
	rv = drv->netif_get(desc, pkt, timo);
#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("%s%d: netif_get returning %d\n", drv->netif_bname,
		    nif->nif_unit, (int)rv);
#endif
	return (rv);
}

ssize_t
netif_put(struct iodesc *desc, void *pkt, size_t len)
{
#ifdef NETIF_DEBUG
	struct netif *nif = desc->io_netif;
#endif
	struct netif_driver *drv = desc->io_netif->nif_driver;
	ssize_t rv;

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("%s%d: netif_put\n", drv->netif_bname, nif->nif_unit);
#endif
#ifdef PARANOID
	if (drv->netif_put == NULL)
		panic("%s%d: no netif_put support", drv->netif_bname,
		    nif->nif_unit);
#endif
	rv = drv->netif_put(desc, pkt, len);
#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("%s%d: netif_put returning %d\n", drv->netif_bname,
		    nif->nif_unit, (int)rv);
#endif
	return (rv);
}

/*
 * socktodesc_impl:
 *
 * Walk socket list and return pointer to iodesc structure.
 * if id is < 0, return first unused iodesc.
 */
static struct iodesc *
socktodesc_impl(int socket)
{
	struct iodesc *s;

	TAILQ_FOREACH(s, &sockets, io_link) {
		/* search by socket id */
		if (socket >= 0) {
			if (s->io_id == socket)
				break;
			continue;
		}
		/* search for first unused entry */
		if (s->io_netif == NULL)
			break;
	}
	return (s);
}

struct iodesc *
socktodesc(int sock)
{
	struct iodesc *desc;

	if (sock < 0)
		desc = NULL;
	else
		desc = socktodesc_impl(sock);

	if (desc == NULL)
		errno = EBADF;

	return (desc);
}

int
netif_open(void *machdep_hint)
{
	struct iodesc *s;
	struct netif *nif;

	/* find a free socket */
	s = socktodesc_impl(-1);
	if (s == NULL) {
		struct iodesc *last;

		s = calloc(1, sizeof (*s));
		if (s == NULL)
			return (-1);

		last = TAILQ_LAST(&sockets, socket_list);
		if (last != NULL)
			s->io_id = last->io_id + 1;
		TAILQ_INSERT_TAIL(&sockets, s, io_link);
	}

	netif_init();
	nif = netif_select(machdep_hint);
	if (!nif)
		panic("netboot: no interfaces left untried");
	if (netif_probe(nif, machdep_hint)) {
		printf("netboot: couldn't probe %s%d\n",
		    nif->nif_driver->netif_bname, nif->nif_unit);
		errno = EINVAL;
		return (-1);
	}
	netif_attach(nif, s, machdep_hint);

	return (s->io_id);
}

int
netif_close(int sock)
{
	struct iodesc *s, *last;
	int err;

	err = 0;
	s = socktodesc_impl(sock);
	if (s == NULL || sock < 0) {
		err = EBADF;
		return (-1);
	}
	netif_detach(s->io_netif);
	bzero(&s->destip, sizeof (s->destip));
	bzero(&s->myip, sizeof (s->myip));
	s->destport = 0;
	s->myport = 0;
	s->xid = 0;
	bzero(s->myea, sizeof (s->myea));
	s->io_netif = NULL;

	/* free unused entries from tail. */
	TAILQ_FOREACH_REVERSE_SAFE(last, &sockets, socket_list, io_link, s) {
		if (last->io_netif != NULL)
			break;
		TAILQ_REMOVE(&sockets, last, io_link);
		free(last);
	}

	if (err) {
		errno = err;
		return (-1);
	}

	return (0);
}
