/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2012 Gleb Smirnoff <glebius@FreeBSD.org>
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 *	@(#)if.c	8.5 (Berkeley) 1/9/95
 * $FreeBSD$
 */

#include "opt_netlink.h"

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_clone.h>
#include <net/radix.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_route.h>
#include <netlink/route/route_var.h>

/* Current IF_MAXUNIT expands maximum to 5 characters. */
#define	IFCLOSIZ	(IFNAMSIZ - 5)

/*
 * Structure describing a `cloning' interface.
 *
 * List of locks
 * (c)		const until freeing
 * (d)		driver specific data, may need external protection.
 * (e)		locked by if_cloners_mtx
 * (i)		locked by ifc_mtx mtx
 */
struct if_clone {
	char ifc_name[IFCLOSIZ];	/* (c) Name of device, e.g. `gif' */
	struct unrhdr *ifc_unrhdr;	/* (c) alloc_unr(9) header */
	int ifc_maxunit;		/* (c) maximum unit number */
	int ifc_flags;
	long ifc_refcnt;		/* (i) Reference count. */
	LIST_HEAD(, ifnet) ifc_iflist;	/* (i) List of cloned interfaces */
	struct mtx ifc_mtx;		/* Mutex to protect members. */

	ifc_match_f *ifc_match;		/* (c) Matcher function */
	ifc_create_f *ifc_create;	/* (c) Creates new interface */
	ifc_destroy_f *ifc_destroy;	/* (c) Destroys cloned interface */

	ifc_create_nl_f	*create_nl;	/* (c) Netlink creation handler */
	ifc_modify_nl_f	*modify_nl;	/* (c) Netlink modification handler */
	ifc_dump_nl_f	*dump_nl;	/* (c) Netlink dump handler */

#ifdef CLONE_COMPAT_13
	/* (c) Driver specific cloning functions.  Called with no locks held. */
	union {
		struct {	/* advanced cloner */
			ifc_create_t	*_ifc_create;
			ifc_destroy_t	*_ifc_destroy;
		} A;
		struct {	/* simple cloner */
			ifcs_create_t	*_ifcs_create;
			ifcs_destroy_t	*_ifcs_destroy;
			int		_ifcs_minifs;	/* minimum ifs */

		} S;
	} U;
#define	ifca_create	U.A._ifc_create
#define	ifca_destroy	U.A._ifc_destroy
#define	ifcs_create	U.S._ifcs_create
#define	ifcs_destroy	U.S._ifcs_destroy
#define	ifcs_minifs	U.S._ifcs_minifs
#endif

	LIST_ENTRY(if_clone) ifc_list;	/* (e) On list of cloners */
};



static void	if_clone_free(struct if_clone *ifc);
static int	if_clone_createif_nl(struct if_clone *ifc, const char *name,
		    struct ifc_data_nl *ifd);

static int ifc_simple_match(struct if_clone *ifc, const char *name);
static int ifc_handle_unit(struct if_clone *ifc, char *name, size_t len, int *punit);
static struct if_clone *ifc_find_cloner(const char *name);
static struct if_clone *ifc_find_cloner_match(const char *name);

#ifdef CLONE_COMPAT_13
static int ifc_simple_create_wrapper(struct if_clone *ifc, char *name, size_t maxlen,
    struct ifc_data *ifc_data, struct ifnet **ifpp);
static int ifc_advanced_create_wrapper(struct if_clone *ifc, char *name, size_t maxlen,
    struct ifc_data *ifc_data, struct ifnet **ifpp);
#endif

static struct mtx if_cloners_mtx;
MTX_SYSINIT(if_cloners_lock, &if_cloners_mtx, "if_cloners lock", MTX_DEF);
VNET_DEFINE_STATIC(int, if_cloners_count);
VNET_DEFINE(LIST_HEAD(, if_clone), if_cloners);

#define	V_if_cloners_count	VNET(if_cloners_count)
#define	V_if_cloners		VNET(if_cloners)

#define IF_CLONERS_LOCK_ASSERT()	mtx_assert(&if_cloners_mtx, MA_OWNED)
#define IF_CLONERS_LOCK()		mtx_lock(&if_cloners_mtx)
#define IF_CLONERS_UNLOCK()		mtx_unlock(&if_cloners_mtx)

#define IF_CLONE_LOCK_INIT(ifc)		\
    mtx_init(&(ifc)->ifc_mtx, "if_clone lock", NULL, MTX_DEF)
#define IF_CLONE_LOCK_DESTROY(ifc)	mtx_destroy(&(ifc)->ifc_mtx)
#define IF_CLONE_LOCK_ASSERT(ifc)	mtx_assert(&(ifc)->ifc_mtx, MA_OWNED)
#define IF_CLONE_LOCK(ifc)		mtx_lock(&(ifc)->ifc_mtx)
#define IF_CLONE_UNLOCK(ifc)		mtx_unlock(&(ifc)->ifc_mtx)

#define IF_CLONE_ADDREF(ifc)						\
	do {								\
		IF_CLONE_LOCK(ifc);					\
		IF_CLONE_ADDREF_LOCKED(ifc);				\
		IF_CLONE_UNLOCK(ifc);					\
	} while (0)
#define IF_CLONE_ADDREF_LOCKED(ifc)					\
	do {								\
		IF_CLONE_LOCK_ASSERT(ifc);				\
		KASSERT((ifc)->ifc_refcnt >= 0,				\
		    ("negative refcnt %ld", (ifc)->ifc_refcnt));	\
		(ifc)->ifc_refcnt++;					\
	} while (0)
#define IF_CLONE_REMREF(ifc)						\
	do {								\
		IF_CLONE_LOCK(ifc);					\
		IF_CLONE_REMREF_LOCKED(ifc);				\
	} while (0)
#define IF_CLONE_REMREF_LOCKED(ifc)					\
	do {								\
		IF_CLONE_LOCK_ASSERT(ifc);				\
		KASSERT((ifc)->ifc_refcnt > 0,				\
		    ("bogus refcnt %ld", (ifc)->ifc_refcnt));		\
		if (--(ifc)->ifc_refcnt == 0) {				\
			IF_CLONE_UNLOCK(ifc);				\
			if_clone_free(ifc);				\
		} else {						\
			/* silently free the lock */			\
			IF_CLONE_UNLOCK(ifc);				\
		}							\
	} while (0)

#define IFC_IFLIST_INSERT(_ifc, _ifp)					\
	LIST_INSERT_HEAD(&_ifc->ifc_iflist, _ifp, if_clones)
#define IFC_IFLIST_REMOVE(_ifc, _ifp)					\
	LIST_REMOVE(_ifp, if_clones)

static MALLOC_DEFINE(M_CLONE, "clone", "interface cloning framework");

void
vnet_if_clone_init(void)
{

	LIST_INIT(&V_if_cloners);
}

/*
 * Lookup and create a clone network interface.
 */
int
ifc_create_ifp(const char *name, struct ifc_data *ifd, struct ifnet **ifpp)
{
	struct if_clone *ifc = ifc_find_cloner_match(name);

	if (ifc == NULL)
		return (EINVAL);

	struct ifc_data_nl ifd_new = {
		.flags = ifd->flags,
		.unit = ifd->unit,
		.params = ifd->params,
	};

	int error = if_clone_createif_nl(ifc, name, &ifd_new);

	if (ifpp != NULL)
		*ifpp = ifd_new.ifp;

	return (error);
}

bool
ifc_create_ifp_nl(const char *name, struct ifc_data_nl *ifd)
{
	struct if_clone *ifc = ifc_find_cloner_match(name);
	if (ifc == NULL) {
		ifd->error = EINVAL;
		return (false);
	}

	ifd->error = if_clone_createif_nl(ifc, name, ifd);

	return (true);
}

int
if_clone_create(char *name, size_t len, caddr_t params)
{
	struct ifc_data ifd = { .params = params };
	struct ifnet *ifp;

	int error = ifc_create_ifp(name, &ifd, &ifp);

	if (error == 0)
		strlcpy(name, if_name(ifp), len);

	return (error);
}

bool
ifc_modify_ifp_nl(struct ifnet *ifp, struct ifc_data_nl *ifd)
{
	struct if_clone *ifc = ifc_find_cloner(ifp->if_dname);
	if (ifc == NULL) {
		ifd->error = EINVAL;
		return (false);
	}

	ifd->error = (*ifc->modify_nl)(ifp, ifd);
	return (true);
}

bool
ifc_dump_ifp_nl(struct ifnet *ifp, struct nl_writer *nw)
{
	struct if_clone *ifc = ifc_find_cloner(ifp->if_dname);
	if (ifc == NULL)
		return (false);

	(*ifc->dump_nl)(ifp, nw);
	return (true);
}

static int
ifc_create_ifp_nl_default(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data_nl *ifd)
{
	struct ifc_data ifd_new = {
		.flags = ifd->flags,
		.unit = ifd->unit,
		.params = ifd->params,
	};

	return ((*ifc->ifc_create)(ifc, name, len, &ifd_new, &ifd->ifp));
}

static int
ifc_modify_ifp_nl_default(struct ifnet *ifp, struct ifc_data_nl *ifd)
{
	if (ifd->lattrs != NULL)
		return (nl_modify_ifp_generic(ifp, ifd->lattrs, ifd->bm, ifd->npt));
	return (0);
}

static void
ifc_dump_ifp_nl_default(struct ifnet *ifp, struct nl_writer *nw)
{
	int off = nlattr_add_nested(nw, IFLA_LINKINFO);

	if (off != 0) {
		nlattr_add_string(nw, IFLA_INFO_KIND, ifp->if_dname);
		nlattr_set_len(nw, off);
	}
}

void
ifc_link_ifp(struct if_clone *ifc, struct ifnet *ifp)
{

	if_addgroup(ifp, ifc->ifc_name);

	IF_CLONE_LOCK(ifc);
	IFC_IFLIST_INSERT(ifc, ifp);
	IF_CLONE_UNLOCK(ifc);
}

void
if_clone_addif(struct if_clone *ifc, struct ifnet *ifp)
{
	ifc_link_ifp(ifc, ifp);
}

bool
ifc_unlink_ifp(struct if_clone *ifc, struct ifnet *ifp)
{
	struct ifnet *ifcifp;

	IF_CLONE_LOCK(ifc);
	LIST_FOREACH(ifcifp, &ifc->ifc_iflist, if_clones) {
		if (ifcifp == ifp) {
			IFC_IFLIST_REMOVE(ifc, ifp);
			break;
		}
	}
	IF_CLONE_UNLOCK(ifc);

	if (ifcifp != NULL)
		if_delgroup(ifp, ifc->ifc_name);

	return (ifcifp != NULL);
}

static struct if_clone *
ifc_find_cloner_match(const char *name)
{
	struct if_clone *ifc;

	IF_CLONERS_LOCK();
	LIST_FOREACH(ifc, &V_if_cloners, ifc_list) {
		if (ifc->ifc_match(ifc, name))
			break;
	}
	IF_CLONERS_UNLOCK();

	return (ifc);
}

static struct if_clone *
ifc_find_cloner(const char *name)
{
	struct if_clone *ifc;

	IF_CLONERS_LOCK();
	LIST_FOREACH(ifc, &V_if_cloners, ifc_list) {
		if (strcmp(ifc->ifc_name, name) == 0) {
			break;
		}
	}
	IF_CLONERS_UNLOCK();

	return (ifc);
}

static struct if_clone *
ifc_find_cloner_in_vnet(const char *name, struct vnet *vnet)
{
	CURVNET_SET_QUIET(vnet);
	struct if_clone *ifc = ifc_find_cloner(name);
	CURVNET_RESTORE();

	return (ifc);
}

/*
 * Create a clone network interface.
 */
static int
if_clone_createif_nl(struct if_clone *ifc, const char *ifname, struct ifc_data_nl *ifd)
{
	char name[IFNAMSIZ];
	int error;

	strlcpy(name, ifname, sizeof(name));

	if (ifunit(name) != NULL)
		return (EEXIST);

	if (ifc->ifc_flags & IFC_F_AUTOUNIT) {
		if ((error = ifc_handle_unit(ifc, name, sizeof(name), &ifd->unit)) != 0)
			return (error);
	}

	if (ifd->lattrs != NULL)
		error = (*ifc->create_nl)(ifc, name, sizeof(name), ifd);
	else
		error = ifc_create_ifp_nl_default(ifc, name, sizeof(name), ifd);
	if (error != 0) {
		if (ifc->ifc_flags & IFC_F_AUTOUNIT)
			ifc_free_unit(ifc, ifd->unit);
		return (error);
	}

	MPASS(ifd->ifp != NULL);
	if_clone_addif(ifc, ifd->ifp);

	if (ifd->lattrs != NULL)
		error = (*ifc->modify_nl)(ifd->ifp, ifd);

	return (error);
}

/*
 * Lookup and destroy a clone network interface.
 */
int
if_clone_destroy(const char *name)
{
	int err;
	struct if_clone *ifc;
	struct ifnet *ifp;

	ifp = ifunit_ref(name);
	if (ifp == NULL)
		return (ENXIO);

	ifc = ifc_find_cloner_in_vnet(ifp->if_dname, ifp->if_home_vnet);
	if (ifc == NULL) {
		if_rele(ifp);
		return (EINVAL);
	}

	err = if_clone_destroyif(ifc, ifp);
	if_rele(ifp);
	return err;
}

/*
 * Destroy a clone network interface.
 */
static int
if_clone_destroyif_flags(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	int err;

	/*
	 * Given that the cloned ifnet might be attached to a different
	 * vnet from where its cloner was registered, we have to
	 * switch to the vnet context of the target vnet.
	 */
	CURVNET_SET_QUIET(ifp->if_vnet);

	if (!ifc_unlink_ifp(ifc, ifp)) {
		CURVNET_RESTORE();
		return (ENXIO);		/* ifp is not on the list. */
	}

	int unit = ifp->if_dunit;
	err = (*ifc->ifc_destroy)(ifc, ifp, flags);

	if (err != 0)
		ifc_link_ifp(ifc, ifp);
	else if (ifc->ifc_flags & IFC_F_AUTOUNIT)
		ifc_free_unit(ifc, unit);
	CURVNET_RESTORE();
	return (err);
}

int
if_clone_destroyif(struct if_clone *ifc, struct ifnet *ifp)
{
	return (if_clone_destroyif_flags(ifc, ifp, 0));
}

static struct if_clone *
if_clone_alloc(const char *name, int maxunit)
{
	struct if_clone *ifc;

	KASSERT(name != NULL, ("%s: no name\n", __func__));

	ifc = malloc(sizeof(struct if_clone), M_CLONE, M_WAITOK | M_ZERO);
	strncpy(ifc->ifc_name, name, IFCLOSIZ-1);
	IF_CLONE_LOCK_INIT(ifc);
	IF_CLONE_ADDREF(ifc);
	ifc->ifc_maxunit = maxunit ? maxunit : IF_MAXUNIT;
	ifc->ifc_unrhdr = new_unrhdr(0, ifc->ifc_maxunit, &ifc->ifc_mtx);
	LIST_INIT(&ifc->ifc_iflist);

	ifc->create_nl = ifc_create_ifp_nl_default;
	ifc->modify_nl = ifc_modify_ifp_nl_default;
	ifc->dump_nl = ifc_dump_ifp_nl_default;

	return (ifc);
}

static int
if_clone_attach(struct if_clone *ifc)
{
	struct if_clone *ifc1;

	IF_CLONERS_LOCK();
	LIST_FOREACH(ifc1, &V_if_cloners, ifc_list)
		if (strcmp(ifc->ifc_name, ifc1->ifc_name) == 0) {
			IF_CLONERS_UNLOCK();
			IF_CLONE_REMREF(ifc);
			return (EEXIST);
		}
	LIST_INSERT_HEAD(&V_if_cloners, ifc, ifc_list);
	V_if_cloners_count++;
	IF_CLONERS_UNLOCK();

	return (0);
}

struct if_clone *
ifc_attach_cloner(const char *name, struct if_clone_addreq *req)
{
	if (req->create_f == NULL || req->destroy_f == NULL)
		return (NULL);
	if (strnlen(name, IFCLOSIZ) >= (IFCLOSIZ - 1))
		return (NULL);

	struct if_clone *ifc = if_clone_alloc(name, req->maxunit);
	ifc->ifc_match = req->match_f != NULL ? req->match_f : ifc_simple_match;
	ifc->ifc_create = req->create_f;
	ifc->ifc_destroy = req->destroy_f;
	ifc->ifc_flags = (req->flags & IFC_F_AUTOUNIT);

	if (req->version == 2) {
		struct if_clone_addreq_v2 *req2 = (struct if_clone_addreq_v2 *)req;

		ifc->create_nl = req2->create_nl_f;
		ifc->modify_nl = req2->modify_nl_f;
		ifc->dump_nl = req2->dump_nl_f;
	}

	ifc->dump_nl = ifc_dump_ifp_nl_default;

	if (if_clone_attach(ifc) != 0)
		return (NULL);

	EVENTHANDLER_INVOKE(if_clone_event, ifc);

	return (ifc);
}

void
ifc_detach_cloner(struct if_clone *ifc)
{
	if_clone_detach(ifc);
}


#ifdef CLONE_COMPAT_13

static int
ifc_advanced_create_wrapper(struct if_clone *ifc, char *name, size_t maxlen,
    struct ifc_data *ifc_data, struct ifnet **ifpp)
{
	int error = ifc->ifca_create(ifc, name, maxlen, ifc_data->params);

	if (error == 0)
		*ifpp = ifunit(name);
	return (error);
}

static int
ifc_advanced_destroy_wrapper(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	if (ifc->ifca_destroy == NULL)
		return (ENOTSUP);
	return (ifc->ifca_destroy(ifc, ifp));
}

struct if_clone *
if_clone_advanced(const char *name, u_int maxunit, ifc_match_t match,
	ifc_create_t create, ifc_destroy_t destroy)
{
	struct if_clone *ifc;

	ifc = if_clone_alloc(name, maxunit);
	ifc->ifc_match = match;
	ifc->ifc_create = ifc_advanced_create_wrapper;
	ifc->ifc_destroy = ifc_advanced_destroy_wrapper;
	ifc->ifca_destroy = destroy;
	ifc->ifca_create = create;

	if (if_clone_attach(ifc) != 0)
		return (NULL);

	EVENTHANDLER_INVOKE(if_clone_event, ifc);

	return (ifc);
}

static int
ifc_simple_create_wrapper(struct if_clone *ifc, char *name, size_t maxlen,
    struct ifc_data *ifc_data, struct ifnet **ifpp)
{
	int unit = 0;

	ifc_name2unit(name, &unit);
	int error = ifc->ifcs_create(ifc, unit, ifc_data->params);
	if (error == 0)
		*ifpp = ifunit(name);
	return (error);
}

static int
ifc_simple_destroy_wrapper(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	if (ifp->if_dunit < ifc->ifcs_minifs && (flags & IFC_F_FORCE) == 0)
		return (EINVAL);

	ifc->ifcs_destroy(ifp);
	return (0);
}

struct if_clone *
if_clone_simple(const char *name, ifcs_create_t create, ifcs_destroy_t destroy,
	u_int minifs)
{
	struct if_clone *ifc;
	u_int unit;

	ifc = if_clone_alloc(name, 0);
	ifc->ifc_match = ifc_simple_match;
	ifc->ifc_create = ifc_simple_create_wrapper;
	ifc->ifc_destroy = ifc_simple_destroy_wrapper;
	ifc->ifcs_create = create;
	ifc->ifcs_destroy = destroy;
	ifc->ifcs_minifs = minifs;
	ifc->ifc_flags = IFC_F_AUTOUNIT;

	if (if_clone_attach(ifc) != 0)
		return (NULL);

	for (unit = 0; unit < minifs; unit++) {
		char name[IFNAMSIZ];
		int error __unused;
		struct ifc_data_nl ifd = {};

		snprintf(name, IFNAMSIZ, "%s%d", ifc->ifc_name, unit);
		error = if_clone_createif_nl(ifc, name, &ifd);
		KASSERT(error == 0,
		    ("%s: failed to create required interface %s",
		    __func__, name));
	}

	EVENTHANDLER_INVOKE(if_clone_event, ifc);

	return (ifc);
}
#endif

/*
 * Unregister a network interface cloner.
 */
void
if_clone_detach(struct if_clone *ifc)
{

	IF_CLONERS_LOCK();
	LIST_REMOVE(ifc, ifc_list);
	V_if_cloners_count--;
	IF_CLONERS_UNLOCK();

	/* destroy all interfaces for this cloner */
	while (!LIST_EMPTY(&ifc->ifc_iflist))
		if_clone_destroyif_flags(ifc, LIST_FIRST(&ifc->ifc_iflist), IFC_F_FORCE);

	IF_CLONE_REMREF(ifc);
}

static void
if_clone_free(struct if_clone *ifc)
{

	KASSERT(LIST_EMPTY(&ifc->ifc_iflist),
	    ("%s: ifc_iflist not empty", __func__));

	IF_CLONE_LOCK_DESTROY(ifc);
	delete_unrhdr(ifc->ifc_unrhdr);
	free(ifc, M_CLONE);
}

/*
 * Provide list of interface cloners to userspace.
 */
int
if_clone_list(struct if_clonereq *ifcr)
{
	char *buf, *dst, *outbuf = NULL;
	struct if_clone *ifc;
	int buf_count, count, err = 0;

	if (ifcr->ifcr_count < 0)
		return (EINVAL);

	IF_CLONERS_LOCK();
	/*
	 * Set our internal output buffer size.  We could end up not
	 * reporting a cloner that is added between the unlock and lock
	 * below, but that's not a major problem.  Not caping our
	 * allocation to the number of cloners actually in the system
	 * could be because that would let arbitrary users cause us to
	 * allocate arbitrary amounts of kernel memory.
	 */
	buf_count = (V_if_cloners_count < ifcr->ifcr_count) ?
	    V_if_cloners_count : ifcr->ifcr_count;
	IF_CLONERS_UNLOCK();

	outbuf = malloc(IFNAMSIZ*buf_count, M_CLONE, M_WAITOK | M_ZERO);

	IF_CLONERS_LOCK();

	ifcr->ifcr_total = V_if_cloners_count;
	if ((dst = ifcr->ifcr_buffer) == NULL) {
		/* Just asking how many there are. */
		goto done;
	}
	count = (V_if_cloners_count < buf_count) ?
	    V_if_cloners_count : buf_count;

	for (ifc = LIST_FIRST(&V_if_cloners), buf = outbuf;
	    ifc != NULL && count != 0;
	    ifc = LIST_NEXT(ifc, ifc_list), count--, buf += IFNAMSIZ) {
		strlcpy(buf, ifc->ifc_name, IFNAMSIZ);
	}

done:
	IF_CLONERS_UNLOCK();
	if (err == 0 && dst != NULL)
		err = copyout(outbuf, dst, buf_count*IFNAMSIZ);
	if (outbuf != NULL)
		free(outbuf, M_CLONE);
	return (err);
}

#ifdef VIMAGE
/*
 * if_clone_restoregroup() is used in context of if_vmove().
 *
 * Since if_detach_internal() has removed the interface from ALL groups, we
 * need to "restore" interface membership in the cloner's group.  Note that
 * interface belongs to cloner in its home vnet, so we first find the original
 * cloner, and then we confirm that cloner with the same name exists in the
 * current vnet.
 */
void
if_clone_restoregroup(struct ifnet *ifp)
{
	struct if_clone *ifc;
	struct ifnet *ifcifp;
	char ifc_name[IFCLOSIZ] = { [0] = '\0' };

	CURVNET_SET_QUIET(ifp->if_home_vnet);
	IF_CLONERS_LOCK();
	LIST_FOREACH(ifc, &V_if_cloners, ifc_list) {
		IF_CLONE_LOCK(ifc);
		LIST_FOREACH(ifcifp, &ifc->ifc_iflist, if_clones) {
			if (ifp == ifcifp) {
				strncpy(ifc_name, ifc->ifc_name, IFCLOSIZ-1);
				break;
			}
		}
		IF_CLONE_UNLOCK(ifc);
		if (ifc_name[0] != '\0')
			break;
	}
	CURVNET_RESTORE();
	LIST_FOREACH(ifc, &V_if_cloners, ifc_list)
		if (strcmp(ifc->ifc_name, ifc_name) == 0)
			break;
	IF_CLONERS_UNLOCK();

	if (ifc != NULL)
		if_addgroup(ifp, ifc_name);
}
#endif

/*
 * A utility function to extract unit numbers from interface names of
 * the form name###.
 *
 * Returns 0 on success and an error on failure.
 */
int
ifc_name2unit(const char *name, int *unit)
{
	const char	*cp;
	int		cutoff = INT_MAX / 10;
	int		cutlim = INT_MAX % 10;

	for (cp = name; *cp != '\0' && (*cp < '0' || *cp > '9'); cp++)
		;
	if (*cp == '\0') {
		*unit = -1;
	} else if (cp[0] == '0' && cp[1] != '\0') {
		/* Disallow leading zeroes. */
		return (EINVAL);
	} else {
		for (*unit = 0; *cp != '\0'; cp++) {
			if (*cp < '0' || *cp > '9') {
				/* Bogus unit number. */
				return (EINVAL);
			}
			if (*unit > cutoff ||
			    (*unit == cutoff && *cp - '0' > cutlim))
				return (EINVAL);
			*unit = (*unit * 10) + (*cp - '0');
		}
	}

	return (0);
}

static int
ifc_alloc_unit_specific(struct if_clone *ifc, int *unit)
{
	char name[IFNAMSIZ];

	if (*unit > ifc->ifc_maxunit)
		return (ENOSPC);

	if (alloc_unr_specific(ifc->ifc_unrhdr, *unit) == -1)
		return (EEXIST);

	snprintf(name, IFNAMSIZ, "%s%d", ifc->ifc_name, *unit);
	if (ifunit(name) != NULL) {
		free_unr(ifc->ifc_unrhdr, *unit);
		return (EEXIST);
	}

	IF_CLONE_ADDREF(ifc);

	return (0);
}

static int
ifc_alloc_unit_next(struct if_clone *ifc, int *unit)
{
	int error;

	*unit = alloc_unr(ifc->ifc_unrhdr);
	if (*unit == -1)
		return (ENOSPC);

	free_unr(ifc->ifc_unrhdr, *unit);
	for (;;) {
		error = ifc_alloc_unit_specific(ifc, unit);
		if (error != EEXIST)
			break;

		(*unit)++;
	}

	return (error);
}

int
ifc_alloc_unit(struct if_clone *ifc, int *unit)
{
	if (*unit < 0)
		return (ifc_alloc_unit_next(ifc, unit));
	else
		return (ifc_alloc_unit_specific(ifc, unit));
}

void
ifc_free_unit(struct if_clone *ifc, int unit)
{

	free_unr(ifc->ifc_unrhdr, unit);
	IF_CLONE_REMREF(ifc);
}

static int
ifc_simple_match(struct if_clone *ifc, const char *name)
{
	const char *cp;
	int i;

	/* Match the name */
	for (cp = name, i = 0; i < strlen(ifc->ifc_name); i++, cp++) {
		if (ifc->ifc_name[i] != *cp)
			return (0);
	}

	/* Make sure there's a unit number or nothing after the name */
	for (; *cp != '\0'; cp++) {
		if (*cp < '0' || *cp > '9')
			return (0);
	}

	return (1);
}

static int
ifc_handle_unit(struct if_clone *ifc, char *name, size_t len, int *punit)
{
	char *dp;
	int wildcard;
	int unit;
	int err;

	err = ifc_name2unit(name, &unit);
	if (err != 0)
		return (err);

	wildcard = (unit < 0);

	err = ifc_alloc_unit(ifc, &unit);
	if (err != 0)
		return (err);

	/* In the wildcard case, we need to update the name. */
	if (wildcard) {
		for (dp = name; *dp != '\0'; dp++);
		if (snprintf(dp, len - (dp-name), "%d", unit) >
		    len - (dp-name) - 1) {
			/*
			 * This can only be a programmer error and
			 * there's no straightforward way to recover if
			 * it happens.
			 */
			panic("if_clone_create(): interface name too long");
		}
	}
	*punit = unit;

	return (0);
}

int
ifc_copyin(const struct ifc_data *ifd, void *target, size_t len)
{
	if (ifd->params == NULL)
		return (EINVAL);

	if (ifd->flags & IFC_F_SYSSPACE) {
		memcpy(target, ifd->params, len);
		return (0);
	} else
		return (copyin(ifd->params, target, len));
}
