/*	$FreeBSD: src/sys/netinet6/scope6.c,v 1.17 2007/07/05 16:23:48 delphij Exp $	*/
/*	$KAME: scope6.c,v 1.10 2000/07/24 13:29:31 itojun Exp $	*/

/*-
 * Copyright (C) 2000 WIDE Project.
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

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/syslog.h>

#include <net/route.h>
#include <net/if.h>

#include <netinet/in.h>

#include <netinet6/in6_var.h>
#include <netinet6/scope6_var.h>

#ifdef ENABLE_DEFAULT_SCOPE
int ip6_use_defzone = 1;
#else
int ip6_use_defzone = 0;
#endif

/*
 * The scope6_lock protects the global sid default stored in
 * sid_default below.
 */
static struct mtx scope6_lock;
#define	SCOPE6_LOCK_INIT()	mtx_init(&scope6_lock, "scope6_lock", NULL, MTX_DEF)
#define	SCOPE6_LOCK()		mtx_lock(&scope6_lock)
#define	SCOPE6_UNLOCK()		mtx_unlock(&scope6_lock)
#define	SCOPE6_LOCK_ASSERT()	mtx_assert(&scope6_lock, MA_OWNED)

static struct scope6_id sid_default;
#define SID(ifp) \
	(((struct in6_ifextra *)(ifp)->if_afdata[AF_INET6])->scope6_id)

void
scope6_init(void)
{

	SCOPE6_LOCK_INIT();
	bzero(&sid_default, sizeof(sid_default));
}

struct scope6_id *
scope6_ifattach(struct ifnet *ifp)
{
	struct scope6_id *sid;

	sid = (struct scope6_id *)malloc(sizeof(*sid), M_IFADDR, M_WAITOK);
	bzero(sid, sizeof(*sid));

	/*
	 * XXX: IPV6_ADDR_SCOPE_xxx macros are not standard.
	 * Should we rather hardcode here?
	 */
	sid->s6id_list[IPV6_ADDR_SCOPE_INTFACELOCAL] = ifp->if_index;
	sid->s6id_list[IPV6_ADDR_SCOPE_LINKLOCAL] = ifp->if_index;
#ifdef MULTI_SCOPE
	/* by default, we don't care about scope boundary for these scopes. */
	sid->s6id_list[IPV6_ADDR_SCOPE_SITELOCAL] = 1;
	sid->s6id_list[IPV6_ADDR_SCOPE_ORGLOCAL] = 1;
#endif

	return sid;
}

void
scope6_ifdetach(struct scope6_id *sid)
{

	free(sid, M_IFADDR);
}

int
scope6_set(struct ifnet *ifp, struct scope6_id *idlist)
{
	int i;
	int error = 0;
	struct scope6_id *sid = NULL;

	IF_AFDATA_LOCK(ifp);
	sid = SID(ifp);

	if (!sid) {	/* paranoid? */
		IF_AFDATA_UNLOCK(ifp);
		return (EINVAL);
	}

	/*
	 * XXX: We need more consistency checks of the relationship among
	 * scopes (e.g. an organization should be larger than a site).
	 */

	/*
	 * TODO(XXX): after setting, we should reflect the changes to
	 * interface addresses, routing table entries, PCB entries...
	 */

	SCOPE6_LOCK();
	for (i = 0; i < 16; i++) {
		if (idlist->s6id_list[i] &&
		    idlist->s6id_list[i] != sid->s6id_list[i]) {
			/*
			 * An interface zone ID must be the corresponding
			 * interface index by definition.
			 */
			if (i == IPV6_ADDR_SCOPE_INTFACELOCAL &&
			    idlist->s6id_list[i] != ifp->if_index) {
				IF_AFDATA_UNLOCK(ifp);
				SCOPE6_UNLOCK();
				return (EINVAL);
			}

			if (i == IPV6_ADDR_SCOPE_LINKLOCAL &&
			    idlist->s6id_list[i] > if_index) {
				/*
				 * XXX: theoretically, there should be no
				 * relationship between link IDs and interface
				 * IDs, but we check the consistency for
				 * safety in later use.
				 */
				IF_AFDATA_UNLOCK(ifp);
				SCOPE6_UNLOCK();
				return (EINVAL);
			}

			/*
			 * XXX: we must need lots of work in this case,
			 * but we simply set the new value in this initial
			 * implementation.
			 */
			sid->s6id_list[i] = idlist->s6id_list[i];
		}
	}
	SCOPE6_UNLOCK();
	IF_AFDATA_UNLOCK(ifp);

	return (error);
}

int
scope6_get(struct ifnet *ifp, struct scope6_id *idlist)
{
	/* We only need to lock the interface's afdata for SID() to work. */
	IF_AFDATA_LOCK(ifp);
	struct scope6_id *sid = SID(ifp);

	if (sid == NULL) {	/* paranoid? */
		IF_AFDATA_UNLOCK(ifp);
		return (EINVAL);
	}

	SCOPE6_LOCK();
	*idlist = *sid;
	SCOPE6_UNLOCK();

	IF_AFDATA_UNLOCK(ifp);
	return (0);
}


/*
 * Get a scope of the address. Node-local, link-local, site-local or global.
 */
int
in6_addrscope(struct in6_addr *addr)
{
	int scope;

	if (addr->s6_addr[0] == 0xfe) {
		scope = addr->s6_addr[1] & 0xc0;

		switch (scope) {
		case 0x80:
			return IPV6_ADDR_SCOPE_LINKLOCAL;
			break;
		case 0xc0:
			return IPV6_ADDR_SCOPE_SITELOCAL;
			break;
		default:
			return IPV6_ADDR_SCOPE_GLOBAL; /* just in case */
			break;
		}
	}


	if (addr->s6_addr[0] == 0xff) {
		scope = addr->s6_addr[1] & 0x0f;

		/*
		 * due to other scope such as reserved,
		 * return scope doesn't work.
		 */
		switch (scope) {
		case IPV6_ADDR_SCOPE_INTFACELOCAL:
			return IPV6_ADDR_SCOPE_INTFACELOCAL;
			break;
		case IPV6_ADDR_SCOPE_LINKLOCAL:
			return IPV6_ADDR_SCOPE_LINKLOCAL;
			break;
		case IPV6_ADDR_SCOPE_SITELOCAL:
			return IPV6_ADDR_SCOPE_SITELOCAL;
			break;
		default:
			return IPV6_ADDR_SCOPE_GLOBAL;
			break;
		}
	}

	/*
	 * Regard loopback and unspecified addresses as global, since
	 * they have no ambiguity.
	 */
	if (bcmp(&in6addr_loopback, addr, sizeof(*addr) - 1) == 0) {
		if (addr->s6_addr[15] == 1) /* loopback */
			return IPV6_ADDR_SCOPE_LINKLOCAL;
		if (addr->s6_addr[15] == 0) /* unspecified */
			return IPV6_ADDR_SCOPE_GLOBAL; /* XXX: correct? */
	}

	return IPV6_ADDR_SCOPE_GLOBAL;
}

/*
 * ifp - note that this might be NULL
 */

void
scope6_setdefault(struct ifnet *ifp)
{
	/*
	 * Currently, this function just sets the default "interfaces"
	 * and "links" according to the given interface.
	 * We might eventually have to separate the notion of "link" from
	 * "interface" and provide a user interface to set the default.
	 */
	SCOPE6_LOCK();
	if (ifp) {
		sid_default.s6id_list[IPV6_ADDR_SCOPE_INTFACELOCAL] =
			ifp->if_index;
		sid_default.s6id_list[IPV6_ADDR_SCOPE_LINKLOCAL] =
			ifp->if_index;
	} else {
		sid_default.s6id_list[IPV6_ADDR_SCOPE_INTFACELOCAL] = 0;
		sid_default.s6id_list[IPV6_ADDR_SCOPE_LINKLOCAL] = 0;
	}
	SCOPE6_UNLOCK();
}

int
scope6_get_default(struct scope6_id *idlist)
{

	SCOPE6_LOCK();
	*idlist = sid_default;
	SCOPE6_UNLOCK();

	return (0);
}

u_int32_t
scope6_addr2default(struct in6_addr *addr)
{
	u_int32_t id;

	/*
	 * special case: The loopback address should be considered as
	 * link-local, but there's no ambiguity in the syntax.
	 */
	if (IN6_IS_ADDR_LOOPBACK(addr))
		return (0);

	/*
	 * XXX: 32-bit read is atomic on all our platforms, is it OK
	 * not to lock here?
	 */
	SCOPE6_LOCK();
	id = sid_default.s6id_list[in6_addrscope(addr)];
	SCOPE6_UNLOCK();
	return (id);
}

/*
 * Validate the specified scope zone ID in the sin6_scope_id field.  If the ID
 * is unspecified (=0), needs to be specified, and the default zone ID can be
 * used, the default value will be used.
 * This routine then generates the kernel-internal form: if the address scope
 * of is interface-local or link-local, embed the interface index in the
 * address.
 */
int
sa6_embedscope(struct sockaddr_in6 *sin6, int defaultok)
{
	struct ifnet *ifp;
	u_int32_t zoneid;

	if ((zoneid = sin6->sin6_scope_id) == 0 && defaultok)
		zoneid = scope6_addr2default(&sin6->sin6_addr);

	if (zoneid != 0 &&
	    (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr) ||
	    IN6_IS_ADDR_MC_INTFACELOCAL(&sin6->sin6_addr))) {
		/*
		 * At this moment, we only check interface-local and
		 * link-local scope IDs, and use interface indices as the
		 * zone IDs assuming a one-to-one mapping between interfaces
		 * and links.
		 */
		if (if_index < zoneid)
			return (ENXIO);
		ifp = ifnet_byindex(zoneid);
		if (ifp == NULL) /* XXX: this can happen for some OS */
			return (ENXIO);

		/* XXX assignment to 16bit from 32bit variable */
		sin6->sin6_addr.s6_addr16[1] = htons(zoneid & 0xffff);

		sin6->sin6_scope_id = 0;
	}

	return 0;
}

/*
 * generate standard sockaddr_in6 from embedded form.
 */
int
sa6_recoverscope(struct sockaddr_in6 *sin6)
{
	char ip6buf[INET6_ADDRSTRLEN];
	u_int32_t zoneid;

	if (sin6->sin6_scope_id != 0) {
		log(LOG_NOTICE,
		    "sa6_recoverscope: assumption failure (non 0 ID): %s%%%d\n",
		    ip6_sprintf(ip6buf, &sin6->sin6_addr), sin6->sin6_scope_id);
		/* XXX: proceed anyway... */
	}
	if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr) ||
	    IN6_IS_ADDR_MC_INTFACELOCAL(&sin6->sin6_addr)) {
		/*
		 * KAME assumption: link id == interface id
		 */
		zoneid = ntohs(sin6->sin6_addr.s6_addr16[1]);
		if (zoneid) {
			/* sanity check */
			if (zoneid < 0 || if_index < zoneid)
				return (ENXIO);
			if (!ifnet_byindex(zoneid))
				return (ENXIO);
			sin6->sin6_addr.s6_addr16[1] = 0;
			sin6->sin6_scope_id = zoneid;
		}
	}

	return 0;
}

/*
 * Determine the appropriate scope zone ID for in6 and ifp.  If ret_id is
 * non NULL, it is set to the zone ID.  If the zone ID needs to be embedded
 * in the in6_addr structure, in6 will be modified.
 *
 * ret_id - unnecessary?
 */
int
in6_setscope(struct in6_addr *in6, struct ifnet *ifp, u_int32_t *ret_id)
{
	int scope;
	u_int32_t zoneid = 0;
	struct scope6_id *sid;

	IF_AFDATA_LOCK(ifp);

	sid = SID(ifp);

#ifdef DIAGNOSTIC
	if (sid == NULL) { /* should not happen */
		panic("in6_setscope: scope array is NULL");
		/* NOTREACHED */
	}
#endif

	/*
	 * special case: the loopback address can only belong to a loopback
	 * interface.
	 */
	if (IN6_IS_ADDR_LOOPBACK(in6)) {
		if (!(ifp->if_flags & IFF_LOOPBACK)) {
			IF_AFDATA_UNLOCK(ifp);
			return (EINVAL);
		} else {
			if (ret_id != NULL)
				*ret_id = 0; /* there's no ambiguity */
			IF_AFDATA_UNLOCK(ifp);
			return (0);
		}
	}

	scope = in6_addrscope(in6);

	SCOPE6_LOCK();
	switch (scope) {
	case IPV6_ADDR_SCOPE_INTFACELOCAL: /* should be interface index */
		zoneid = sid->s6id_list[IPV6_ADDR_SCOPE_INTFACELOCAL];
		break;

	case IPV6_ADDR_SCOPE_LINKLOCAL:
		zoneid = sid->s6id_list[IPV6_ADDR_SCOPE_LINKLOCAL];
		break;

	case IPV6_ADDR_SCOPE_SITELOCAL:
		zoneid = sid->s6id_list[IPV6_ADDR_SCOPE_SITELOCAL];
		break;

	case IPV6_ADDR_SCOPE_ORGLOCAL:
		zoneid = sid->s6id_list[IPV6_ADDR_SCOPE_ORGLOCAL];
		break;

	default:
		zoneid = 0;	/* XXX: treat as global. */
		break;
	}
	SCOPE6_UNLOCK();
	IF_AFDATA_UNLOCK(ifp);

	if (ret_id != NULL)
		*ret_id = zoneid;

	if (IN6_IS_SCOPE_LINKLOCAL(in6) || IN6_IS_ADDR_MC_INTFACELOCAL(in6))
		in6->s6_addr16[1] = htons(zoneid & 0xffff); /* XXX */

	return (0);
}

/*
 * Just clear the embedded scope identifier.  Return 0 if the original address
 * is intact; return non 0 if the address is modified.
 */
int
in6_clearscope(struct in6_addr *in6)
{
	int modified = 0;

	if (IN6_IS_SCOPE_LINKLOCAL(in6) || IN6_IS_ADDR_MC_INTFACELOCAL(in6)) {
		if (in6->s6_addr16[1] != 0)
			modified = 1;
		in6->s6_addr16[1] = 0;
	}

	return (modified);
}
