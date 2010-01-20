/*-
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	From: @(#)if.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

#ifndef	_NET_IF_CLONE_H_
#define	_NET_IF_CLONE_H_

#ifdef _KERNEL

#define IFC_CLONE_INITIALIZER(name, data, maxunit,			\
    attach, match, create, destroy)					\
    { { 0 }, name, maxunit, NULL, 0, data, attach, match, create, destroy }

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
	LIST_ENTRY(if_clone) ifc_list;	/* (e) On list of cloners */
	const char *ifc_name;		/* (c) Name of device, e.g. `gif' */
	int ifc_maxunit;		/* (c) Maximum unit number */
	unsigned char *ifc_units;	/* (i) Bitmap to handle units. */
					/*     Considered private, access */
					/*     via ifc_(alloc|free)_unit(). */
	int ifc_bmlen;			/* (c) Bitmap length. */
	void *ifc_data;			/* (*) Data for ifc_* functions. */

	/* (c) Driver specific cloning functions.  Called with no locks held. */
	void	(*ifc_attach)(struct if_clone *);
	int	(*ifc_match)(struct if_clone *, const char *);
	int	(*ifc_create)(struct if_clone *, char *, size_t, caddr_t);
	int	(*ifc_destroy)(struct if_clone *, struct ifnet *);

	long ifc_refcnt;		/* (i) Refrence count. */
	struct mtx ifc_mtx;		/* Muted to protect members. */
	LIST_HEAD(, ifnet) ifc_iflist;	/* (i) List of cloned interfaces */
};

void	if_clone_init(void);
void	if_clone_attach(struct if_clone *);
void	if_clone_detach(struct if_clone *);
void	vnet_if_clone_init(void);

int	if_clone_create(char *, size_t, caddr_t);
int	if_clone_destroy(const char *);
int	if_clone_destroyif(struct if_clone *, struct ifnet *);
int	if_clone_list(struct if_clonereq *);

int	ifc_name2unit(const char *name, int *unit);
int	ifc_alloc_unit(struct if_clone *, int *);
void	ifc_free_unit(struct if_clone *, int);

/*
 * The ifc_simple functions, structures, and macros implement basic
 * cloning as in 5.[012].
 */

struct ifc_simple_data {
	int ifcs_minifs;		/* minimum number of interfaces */

	int	(*ifcs_create)(struct if_clone *, int, caddr_t);
	void	(*ifcs_destroy)(struct ifnet *);
};

/* interface clone event */
typedef void (*if_clone_event_handler_t)(void *, struct if_clone *);
EVENTHANDLER_DECLARE(if_clone_event, if_clone_event_handler_t);

#define IFC_SIMPLE_DECLARE(name, minifs)				\
struct ifc_simple_data name##_cloner_data =				\
    {minifs, name##_clone_create, name##_clone_destroy};		\
struct if_clone name##_cloner =						\
    IFC_CLONE_INITIALIZER(#name, &name##_cloner_data, IF_MAXUNIT,	\
    ifc_simple_attach, ifc_simple_match, ifc_simple_create, ifc_simple_destroy)

void	ifc_simple_attach(struct if_clone *);
int	ifc_simple_match(struct if_clone *, const char *);
int	ifc_simple_create(struct if_clone *, char *, size_t, caddr_t);
int	ifc_simple_destroy(struct if_clone *, struct ifnet *);

#endif /* _KERNEL */

#endif /* !_NET_IF_CLONE_H_ */
