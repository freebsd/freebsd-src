/*-
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
 * 4. Neither the name of the University nor the names of its contributors
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

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_clone.h>
#if 0
#include <net/if_dl.h>
#endif
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/radix.h>
#include <net/route.h>

static void	if_clone_free(struct if_clone *ifc);
static int	if_clone_createif(struct if_clone *ifc, char *name, size_t len);
static int	if_clone_destroyif(struct if_clone *ifc, struct ifnet *ifp);

static struct mtx	if_cloners_mtx;
static int		if_cloners_count;
LIST_HEAD(, if_clone)	if_cloners = LIST_HEAD_INITIALIZER(if_cloners);

#define IF_CLONERS_LOCK_INIT()		\
    mtx_init(&if_cloners_mtx, "if_cloners lock", NULL, MTX_DEF)
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
if_clone_init(void)
{
	IF_CLONERS_LOCK_INIT();
}

/*
 * Lookup and create a clone network interface.
 */
int
if_clone_create(char *name, size_t len)
{
	struct if_clone *ifc;

	/* Try to find an applicable cloner for this request */
	IF_CLONERS_LOCK();
	LIST_FOREACH(ifc, &if_cloners, ifc_list) {
		if (ifc->ifc_match(ifc, name)) {
			break;
		}
	}
	IF_CLONERS_UNLOCK();

	if (ifc == NULL)
		return (EINVAL);

	return (if_clone_createif(ifc, name, len));
}

/*
 * Create a clone network interface.
 */
static int
if_clone_createif(struct if_clone *ifc, char *name, size_t len)
{
	int err;
	struct ifnet *ifp;

	if (ifunit(name) != NULL)
		return (EEXIST);

	err = (*ifc->ifc_create)(ifc, name, len);
	
	if (!err) {
		ifp = ifunit(name);
		if (ifp == NULL)
			panic("%s: lookup failed for %s", __func__, name);

		IF_CLONE_LOCK(ifc);
		IFC_IFLIST_INSERT(ifc, ifp);
		IF_CLONE_UNLOCK(ifc);
	}

	return (err);
}

/*
 * Lookup and destroy a clone network interface.
 */
int
if_clone_destroy(const char *name)
{
	struct if_clone *ifc;
	struct ifnet *ifp;

	ifp = ifunit(name);
	if (ifp == NULL)
		return (ENXIO);

	/* Find the cloner for this interface */
	IF_CLONERS_LOCK();
	LIST_FOREACH(ifc, &if_cloners, ifc_list) {
		if (strcmp(ifc->ifc_name, ifp->if_dname) == 0) {
			break;
		}
	}
	IF_CLONERS_UNLOCK();
	if (ifc == NULL)
		return (EINVAL);

	return (if_clone_destroyif(ifc, ifp));
}

/*
 * Destroy a clone network interface.
 */
static int
if_clone_destroyif(struct if_clone *ifc, struct ifnet *ifp)
{
	int err;

	if (ifc->ifc_destroy == NULL) {
		err = EOPNOTSUPP;
		goto done;
	}

	IF_CLONE_LOCK(ifc);
	IFC_IFLIST_REMOVE(ifc, ifp);
	IF_CLONE_UNLOCK(ifc);

	err =  (*ifc->ifc_destroy)(ifc, ifp);

	if (err != 0) {
		IF_CLONE_LOCK(ifc);
		IFC_IFLIST_INSERT(ifc, ifp);
		IF_CLONE_UNLOCK(ifc);
	}

done:
	return (err);
}

/*
 * Register a network interface cloner.
 */
void
if_clone_attach(struct if_clone *ifc)
{
	int len, maxclone;

	/*
	 * Compute bitmap size and allocate it.
	 */
	maxclone = ifc->ifc_maxunit + 1;
	len = maxclone >> 3;
	if ((len << 3) < maxclone)
		len++;
	ifc->ifc_units = malloc(len, M_CLONE, M_WAITOK | M_ZERO);
	ifc->ifc_bmlen = len;
	IF_CLONE_LOCK_INIT(ifc);
	IF_CLONE_ADDREF(ifc);

	IF_CLONERS_LOCK();
	LIST_INSERT_HEAD(&if_cloners, ifc, ifc_list);
	if_cloners_count++;
	IF_CLONERS_UNLOCK();

	LIST_INIT(&ifc->ifc_iflist);

	if (ifc->ifc_attach != NULL)
		(*ifc->ifc_attach)(ifc);
	EVENTHANDLER_INVOKE(if_clone_event, ifc);
}

/*
 * Unregister a network interface cloner.
 */
void
if_clone_detach(struct if_clone *ifc)
{
	struct ifc_simple_data *ifcs = ifc->ifc_data;

	IF_CLONERS_LOCK();
	LIST_REMOVE(ifc, ifc_list);
	if_cloners_count--;
	IF_CLONERS_UNLOCK();

	/* Allow all simples to be destroyed */
	if (ifc->ifc_attach == ifc_simple_attach)
		ifcs->ifcs_minifs = 0;

	/* destroy all interfaces for this cloner */
	while (!LIST_EMPTY(&ifc->ifc_iflist))
		if_clone_destroyif(ifc, LIST_FIRST(&ifc->ifc_iflist));
	
	IF_CLONE_REMREF(ifc);
}

static void
if_clone_free(struct if_clone *ifc)
{
	for (int bytoff = 0; bytoff < ifc->ifc_bmlen; bytoff++) {
		KASSERT(ifc->ifc_units[bytoff] == 0x00,
		    ("ifc_units[%d] is not empty", bytoff));
	}

	KASSERT(LIST_EMPTY(&ifc->ifc_iflist),
	    ("%s: ifc_iflist not empty", __func__));

	IF_CLONE_LOCK_DESTROY(ifc);
	free(ifc->ifc_units, M_CLONE);
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
	 * allocate abritrary amounts of kernel memory.
	 */
	buf_count = (if_cloners_count < ifcr->ifcr_count) ?
	    if_cloners_count : ifcr->ifcr_count;
	IF_CLONERS_UNLOCK();

	outbuf = malloc(IFNAMSIZ*buf_count, M_CLONE, M_WAITOK | M_ZERO);

	IF_CLONERS_LOCK();

	ifcr->ifcr_total = if_cloners_count;
	if ((dst = ifcr->ifcr_buffer) == NULL) {
		/* Just asking how many there are. */
		goto done;
	}
	count = (if_cloners_count < buf_count) ?
	    if_cloners_count : buf_count;

	for (ifc = LIST_FIRST(&if_cloners), buf = outbuf;
	    ifc != NULL && count != 0;
	    ifc = LIST_NEXT(ifc, ifc_list), count--, buf += IFNAMSIZ) {
		strlcpy(buf, ifc->ifc_name, IFNAMSIZ);
	}

done:
	IF_CLONERS_UNLOCK();
	if (err == 0)
		err = copyout(outbuf, dst, buf_count*IFNAMSIZ);
	if (outbuf != NULL)
		free(outbuf, M_CLONE);
	return (err);
}

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

	for (cp = name; *cp != '\0' && (*cp < '0' || *cp > '9'); cp++);
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

int
ifc_alloc_unit(struct if_clone *ifc, int *unit)
{
	int wildcard, bytoff, bitoff;
	int err = 0;

	IF_CLONE_LOCK(ifc);

	bytoff = bitoff = 0;
	wildcard = (*unit < 0);
	/*
	 * Find a free unit if none was given.
	 */
	if (wildcard) {
		while ((bytoff < ifc->ifc_bmlen)
		    && (ifc->ifc_units[bytoff] == 0xff))
			bytoff++;
		if (bytoff >= ifc->ifc_bmlen) {
			err = ENOSPC;
			goto done;
		}
		while ((ifc->ifc_units[bytoff] & (1 << bitoff)) != 0)
			bitoff++;
		*unit = (bytoff << 3) + bitoff;
	}

	if (*unit > ifc->ifc_maxunit) {
		err = ENOSPC;
		goto done;
	}

	if (!wildcard) {
		bytoff = *unit >> 3;
		bitoff = *unit - (bytoff << 3);
	}

	if((ifc->ifc_units[bytoff] & (1 << bitoff)) != 0) {
		err = EEXIST;
		goto done;
	}
	/*
	 * Allocate the unit in the bitmap.
	 */
	KASSERT((ifc->ifc_units[bytoff] & (1 << bitoff)) == 0,
	    ("%s: bit is already set", __func__));
	ifc->ifc_units[bytoff] |= (1 << bitoff);
	IF_CLONE_ADDREF_LOCKED(ifc);

done:
	IF_CLONE_UNLOCK(ifc);
	return (err);
}

void
ifc_free_unit(struct if_clone *ifc, int unit)
{
	int bytoff, bitoff;


	/*
	 * Compute offset in the bitmap and deallocate the unit.
	 */
	bytoff = unit >> 3;
	bitoff = unit - (bytoff << 3);

	IF_CLONE_LOCK(ifc);
	KASSERT((ifc->ifc_units[bytoff] & (1 << bitoff)) != 0,
	    ("%s: bit is already cleared", __func__));
	ifc->ifc_units[bytoff] &= ~(1 << bitoff);
	IF_CLONE_REMREF_LOCKED(ifc);	/* releases lock */
}

void
ifc_simple_attach(struct if_clone *ifc)
{
	int err;
	int unit;
	char name[IFNAMSIZ];
	struct ifc_simple_data *ifcs = ifc->ifc_data;

	KASSERT(ifcs->ifcs_minifs - 1 <= ifc->ifc_maxunit,
	    ("%s: %s requested more units than allowed (%d > %d)",
	    __func__, ifc->ifc_name, ifcs->ifcs_minifs,
	    ifc->ifc_maxunit + 1));

	for (unit = 0; unit < ifcs->ifcs_minifs; unit++) {
		snprintf(name, IFNAMSIZ, "%s%d", ifc->ifc_name, unit);
		err = if_clone_createif(ifc, name, IFNAMSIZ);
		KASSERT(err == 0,
		    ("%s: failed to create required interface %s",
		    __func__, name));
	}
}

int
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

int
ifc_simple_create(struct if_clone *ifc, char *name, size_t len)
{
	char *dp;
	int wildcard;
	int unit;
	int err;
	struct ifc_simple_data *ifcs = ifc->ifc_data;

	err = ifc_name2unit(name, &unit);
	if (err != 0)
		return (err);

	wildcard = (unit < 0);

	err = ifc_alloc_unit(ifc, &unit);
	if (err != 0)
		return (err);

	err = ifcs->ifcs_create(ifc, unit);
	if (err != 0) {
		ifc_free_unit(ifc, unit);
		return (err);
	}

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

	return (0);
}

int
ifc_simple_destroy(struct if_clone *ifc, struct ifnet *ifp)
{
	int unit;
	struct ifc_simple_data *ifcs = ifc->ifc_data;

	unit = ifp->if_dunit;

	if (unit < ifcs->ifcs_minifs) 
		return (EINVAL);

	ifcs->ifcs_destroy(ifp);

	ifc_free_unit(ifc, unit);

	return (0);
}
