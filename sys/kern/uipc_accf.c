/*
 * Copyright (c) 2000 Paycounter, Inc.
 * Author: Alfred Perlstein <alfred@paycounter.com>, <alfred@FreeBSD.org>
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
 *	$FreeBSD$
 */

#define ACCEPT_FILTER_MOD

#include "opt_param.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/queue.h>

static SLIST_HEAD(, accept_filter) accept_filtlsthd =
	SLIST_HEAD_INITIALIZER(&accept_filtlsthd);

MALLOC_DEFINE(M_ACCF, "accf", "accept filter data");

static int unloadable = 0;

SYSCTL_DECL(_net_inet);	/* XXX: some header should do this for me */
SYSCTL_NODE(_net_inet, OID_AUTO, accf, CTLFLAG_RW, 0, "Accept filters");
SYSCTL_INT(_net_inet_accf, OID_AUTO, unloadable, CTLFLAG_RW, &unloadable, 0,
	"Allow unload of accept filters (not recommended)");

/*
 * must be passed a malloc'd structure so we don't explode if the kld
 * is unloaded, we leak the struct on deallocation to deal with this,
 * but if a filter is loaded with the same name as a leaked one we re-use
 * the entry.
 */
int
accept_filt_add(struct accept_filter *filt)
{
	struct accept_filter *p;

	SLIST_FOREACH(p, &accept_filtlsthd, accf_next)
		if (strcmp(p->accf_name, filt->accf_name) == 0)  {
			if (p->accf_callback != NULL) {
				return (EEXIST);
			} else {
				p->accf_callback = filt->accf_callback;
				FREE(filt, M_ACCF);
				return (0);
			}
		}
				
	if (p == NULL)
		SLIST_INSERT_HEAD(&accept_filtlsthd, filt, accf_next);
	return (0);
}

int
accept_filt_del(char *name)
{
	struct accept_filter *p;

	p = accept_filt_get(name);
	if (p == NULL)
		return (ENOENT);

	p->accf_callback = NULL;
	return (0);
}

struct accept_filter *
accept_filt_get(char *name)
{
	struct accept_filter *p;

	SLIST_FOREACH(p, &accept_filtlsthd, accf_next)
		if (strcmp(p->accf_name, name) == 0)
			return (p);

	return (NULL);
}

int
accept_filt_generic_mod_event(module_t mod, int event, void *data)
{
	struct accept_filter *p;
	struct accept_filter *accfp = (struct accept_filter *) data;
	int	s, error;

	switch (event) {
	case MOD_LOAD:
		MALLOC(p, struct accept_filter *, sizeof(*p), M_ACCF, M_WAITOK);
		bcopy(accfp, p, sizeof(*p));
		s = splnet();
		error = accept_filt_add(p);
		splx(s);
		break;

	case MOD_UNLOAD:
		/*
		 * Do not support unloading yet. we don't keep track of refcounts
		 * and unloading an accept filter callback and then having it called
		 * is a bad thing.  A simple fix would be to track the refcount
		 * in the struct accept_filter.
		 */
		if (unloadable != 0) {
			s = splnet();
			error = accept_filt_del(accfp->accf_name);
			splx(s);
		} else
			error = EOPNOTSUPP;
		break;

	case MOD_SHUTDOWN:
		error = 0;
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}
