/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
 *
 * Quite extensively rewritten by Poul-Henning Kamp of the FreeBSD
 * project, to make these variables more userfriendly.
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
 *	@(#)kern_sysctl.c	8.4 (Berkeley) 4/14/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/kern_sysctl.c,v 1.177.6.2 2008/12/19 16:08:40 kib Exp $");

#include "opt_compat.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/sysproto.h>
#include <sys/uio.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

static MALLOC_DEFINE(M_SYSCTL, "sysctl", "sysctl internal magic");
static MALLOC_DEFINE(M_SYSCTLOID, "sysctloid", "sysctl dynamic oids");
static MALLOC_DEFINE(M_SYSCTLTMP, "sysctltmp", "sysctl temp output buffer");

/*
 * Locking - this locks the sysctl tree in memory.
 */
static struct sx sysctllock;

#define	SYSCTL_LOCK()		sx_xlock(&sysctllock)
#define	SYSCTL_UNLOCK()		sx_xunlock(&sysctllock)
#define	SYSCTL_INIT()		sx_init(&sysctllock, "sysctl lock")

static int sysctl_root(SYSCTL_HANDLER_ARGS);

struct sysctl_oid_list sysctl__children; /* root list */

static struct sysctl_oid *
sysctl_find_oidname(const char *name, struct sysctl_oid_list *list)
{
	struct sysctl_oid *oidp;

	SLIST_FOREACH(oidp, list, oid_link) {
		if (strcmp(oidp->oid_name, name) == 0) {
			return (oidp);
		}
	}
	return (NULL);
}

/*
 * Initialization of the MIB tree.
 *
 * Order by number in each list.
 */

void
sysctl_register_oid(struct sysctl_oid *oidp)
{
	struct sysctl_oid_list *parent = oidp->oid_parent;
	struct sysctl_oid *p;
	struct sysctl_oid *q;

	/*
	 * First check if another oid with the same name already
	 * exists in the parent's list.
	 */
	p = sysctl_find_oidname(oidp->oid_name, parent);
	if (p != NULL) {
		if ((p->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
			p->oid_refcnt++;
			return;
		} else {
			printf("can't re-use a leaf (%s)!\n", p->oid_name);
			return;
		}
	}
	/*
	 * If this oid has a number OID_AUTO, give it a number which
	 * is greater than any current oid.
	 * NOTE: DO NOT change the starting value here, change it in
	 * <sys/sysctl.h>, and make sure it is at least 256 to
	 * accomodate e.g. net.inet.raw as a static sysctl node.
	 */
	if (oidp->oid_number == OID_AUTO) {
		static int newoid = CTL_AUTO_START;

		oidp->oid_number = newoid++;
		if (newoid == 0x7fffffff)
			panic("out of oids");
	}
#if 0
	else if (oidp->oid_number >= CTL_AUTO_START) {
		/* do not panic; this happens when unregistering sysctl sets */
		printf("static sysctl oid too high: %d", oidp->oid_number);
	}
#endif

	/*
	 * Insert the oid into the parent's list in order.
	 */
	q = NULL;
	SLIST_FOREACH(p, parent, oid_link) {
		if (oidp->oid_number < p->oid_number)
			break;
		q = p;
	}
	if (q)
		SLIST_INSERT_AFTER(q, oidp, oid_link);
	else
		SLIST_INSERT_HEAD(parent, oidp, oid_link);
}

void
sysctl_unregister_oid(struct sysctl_oid *oidp)
{
	struct sysctl_oid *p;
	int error;

	error = ENOENT;
	if (oidp->oid_number == OID_AUTO) {
		error = EINVAL;
	} else {
		SLIST_FOREACH(p, oidp->oid_parent, oid_link) {
			if (p == oidp) {
				SLIST_REMOVE(oidp->oid_parent, oidp,
				    sysctl_oid, oid_link);
				error = 0;
				break;
			}
		}
	}

	/* 
	 * This can happen when a module fails to register and is
	 * being unloaded afterwards.  It should not be a panic()
	 * for normal use.
	 */
	if (error)
		printf("%s: failed to unregister sysctl\n", __func__);
}

/* Initialize a new context to keep track of dynamically added sysctls. */
int
sysctl_ctx_init(struct sysctl_ctx_list *c)
{

	if (c == NULL) {
		return (EINVAL);
	}
	TAILQ_INIT(c);
	return (0);
}

/* Free the context, and destroy all dynamic oids registered in this context */
int
sysctl_ctx_free(struct sysctl_ctx_list *clist)
{
	struct sysctl_ctx_entry *e, *e1;
	int error;

	error = 0;
	/*
	 * First perform a "dry run" to check if it's ok to remove oids.
	 * XXX FIXME
	 * XXX This algorithm is a hack. But I don't know any
	 * XXX better solution for now...
	 */
	TAILQ_FOREACH(e, clist, link) {
		error = sysctl_remove_oid(e->entry, 0, 0);
		if (error)
			break;
	}
	/*
	 * Restore deregistered entries, either from the end,
	 * or from the place where error occured.
	 * e contains the entry that was not unregistered
	 */
	if (error)
		e1 = TAILQ_PREV(e, sysctl_ctx_list, link);
	else
		e1 = TAILQ_LAST(clist, sysctl_ctx_list);
	while (e1 != NULL) {
		sysctl_register_oid(e1->entry);
		e1 = TAILQ_PREV(e1, sysctl_ctx_list, link);
	}
	if (error)
		return(EBUSY);
	/* Now really delete the entries */
	e = TAILQ_FIRST(clist);
	while (e != NULL) {
		e1 = TAILQ_NEXT(e, link);
		error = sysctl_remove_oid(e->entry, 1, 0);
		if (error)
			panic("sysctl_remove_oid: corrupt tree, entry: %s",
			    e->entry->oid_name);
		free(e, M_SYSCTLOID);
		e = e1;
	}
	return (error);
}

/* Add an entry to the context */
struct sysctl_ctx_entry *
sysctl_ctx_entry_add(struct sysctl_ctx_list *clist, struct sysctl_oid *oidp)
{
	struct sysctl_ctx_entry *e;

	if (clist == NULL || oidp == NULL)
		return(NULL);
	e = malloc(sizeof(struct sysctl_ctx_entry), M_SYSCTLOID, M_WAITOK);
	e->entry = oidp;
	TAILQ_INSERT_HEAD(clist, e, link);
	return (e);
}

/* Find an entry in the context */
struct sysctl_ctx_entry *
sysctl_ctx_entry_find(struct sysctl_ctx_list *clist, struct sysctl_oid *oidp)
{
	struct sysctl_ctx_entry *e;

	if (clist == NULL || oidp == NULL)
		return(NULL);
	TAILQ_FOREACH(e, clist, link) {
		if(e->entry == oidp)
			return(e);
	}
	return (e);
}

/*
 * Delete an entry from the context.
 * NOTE: this function doesn't free oidp! You have to remove it
 * with sysctl_remove_oid().
 */
int
sysctl_ctx_entry_del(struct sysctl_ctx_list *clist, struct sysctl_oid *oidp)
{
	struct sysctl_ctx_entry *e;

	if (clist == NULL || oidp == NULL)
		return (EINVAL);
	e = sysctl_ctx_entry_find(clist, oidp);
	if (e != NULL) {
		TAILQ_REMOVE(clist, e, link);
		free(e, M_SYSCTLOID);
		return (0);
	} else
		return (ENOENT);
}

/*
 * Remove dynamically created sysctl trees.
 * oidp - top of the tree to be removed
 * del - if 0 - just deregister, otherwise free up entries as well
 * recurse - if != 0 traverse the subtree to be deleted
 */
int
sysctl_remove_oid(struct sysctl_oid *oidp, int del, int recurse)
{
	struct sysctl_oid *p;
	int error;

	if (oidp == NULL)
		return(EINVAL);
	if ((oidp->oid_kind & CTLFLAG_DYN) == 0) {
		printf("can't remove non-dynamic nodes!\n");
		return (EINVAL);
	}
	/*
	 * WARNING: normal method to do this should be through
	 * sysctl_ctx_free(). Use recursing as the last resort
	 * method to purge your sysctl tree of leftovers...
	 * However, if some other code still references these nodes,
	 * it will panic.
	 */
	if ((oidp->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
		if (oidp->oid_refcnt == 1) {
			SLIST_FOREACH(p, SYSCTL_CHILDREN(oidp), oid_link) {
				if (!recurse)
					return (ENOTEMPTY);
				error = sysctl_remove_oid(p, del, recurse);
				if (error)
					return (error);
			}
			if (del)
				free(SYSCTL_CHILDREN(oidp), M_SYSCTLOID);
		}
	}
	if (oidp->oid_refcnt > 1 ) {
		oidp->oid_refcnt--;
	} else {
		if (oidp->oid_refcnt == 0) {
			printf("Warning: bad oid_refcnt=%u (%s)!\n",
				oidp->oid_refcnt, oidp->oid_name);
			return (EINVAL);
		}
		sysctl_unregister_oid(oidp);
		if (del) {
			if (oidp->oid_descr)
				free((void *)(uintptr_t)(const void *)oidp->oid_descr, M_SYSCTLOID);
			free((void *)(uintptr_t)(const void *)oidp->oid_name,
			     M_SYSCTLOID);
			free(oidp, M_SYSCTLOID);
		}
	}
	return (0);
}

/*
 * Create new sysctls at run time.
 * clist may point to a valid context initialized with sysctl_ctx_init().
 */
struct sysctl_oid *
sysctl_add_oid(struct sysctl_ctx_list *clist, struct sysctl_oid_list *parent,
	int number, const char *name, int kind, void *arg1, int arg2,
	int (*handler)(SYSCTL_HANDLER_ARGS), const char *fmt, const char *descr)
{
	struct sysctl_oid *oidp;
	ssize_t len;
	char *newname;

	/* You have to hook up somewhere.. */
	if (parent == NULL)
		return(NULL);
	/* Check if the node already exists, otherwise create it */
	oidp = sysctl_find_oidname(name, parent);
	if (oidp != NULL) {
		if ((oidp->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
			oidp->oid_refcnt++;
			/* Update the context */
			if (clist != NULL)
				sysctl_ctx_entry_add(clist, oidp);
			return (oidp);
		} else {
			printf("can't re-use a leaf (%s)!\n", name);
			return (NULL);
		}
	}
	oidp = malloc(sizeof(struct sysctl_oid), M_SYSCTLOID, M_WAITOK|M_ZERO);
	oidp->oid_parent = parent;
	SLIST_NEXT(oidp, oid_link) = NULL;
	oidp->oid_number = number;
	oidp->oid_refcnt = 1;
	len = strlen(name);
	newname = malloc(len + 1, M_SYSCTLOID, M_WAITOK);
	bcopy(name, newname, len + 1);
	newname[len] = '\0';
	oidp->oid_name = newname;
	oidp->oid_handler = handler;
	oidp->oid_kind = CTLFLAG_DYN | kind;
	if ((kind & CTLTYPE) == CTLTYPE_NODE) {
		/* Allocate space for children */
		SYSCTL_CHILDREN_SET(oidp, malloc(sizeof(struct sysctl_oid_list),
		    M_SYSCTLOID, M_WAITOK));
		SLIST_INIT(SYSCTL_CHILDREN(oidp));
	} else {
		oidp->oid_arg1 = arg1;
		oidp->oid_arg2 = arg2;
	}
	oidp->oid_fmt = fmt;
	if (descr) {
		int len = strlen(descr) + 1;
		oidp->oid_descr = malloc(len, M_SYSCTLOID, M_WAITOK);
		if (oidp->oid_descr)
			strcpy((char *)(uintptr_t)(const void *)oidp->oid_descr, descr);
	}
	/* Update the context, if used */
	if (clist != NULL)
		sysctl_ctx_entry_add(clist, oidp);
	/* Register this oid */
	sysctl_register_oid(oidp);
	return (oidp);
}

/*
 * Reparent an existing oid.
 */
int
sysctl_move_oid(struct sysctl_oid *oid, struct sysctl_oid_list *parent)
{
	struct sysctl_oid *oidp;

	if (oid->oid_parent == parent)
		return (0);
	oidp = sysctl_find_oidname(oid->oid_name, parent);
	if (oidp != NULL)
		return (EEXIST);
	sysctl_unregister_oid(oid);
	oid->oid_parent = parent;
	oid->oid_number = OID_AUTO;
	sysctl_register_oid(oid);
	return (0);
}

/*
 * Register the kernel's oids on startup.
 */
SET_DECLARE(sysctl_set, struct sysctl_oid);

static void
sysctl_register_all(void *arg)
{
	struct sysctl_oid **oidp;

	SYSCTL_INIT();
	SET_FOREACH(oidp, sysctl_set)
		sysctl_register_oid(*oidp);
}
SYSINIT(sysctl, SI_SUB_KMEM, SI_ORDER_ANY, sysctl_register_all, 0);

/*
 * "Staff-functions"
 *
 * These functions implement a presently undocumented interface 
 * used by the sysctl program to walk the tree, and get the type
 * so it can print the value.
 * This interface is under work and consideration, and should probably
 * be killed with a big axe by the first person who can find the time.
 * (be aware though, that the proper interface isn't as obvious as it
 * may seem, there are various conflicting requirements.
 *
 * {0,0}	printf the entire MIB-tree.
 * {0,1,...}	return the name of the "..." OID.
 * {0,2,...}	return the next OID.
 * {0,3}	return the OID of the name in "new"
 * {0,4,...}	return the kind & format info for the "..." OID.
 * {0,5,...}	return the description the "..." OID.
 */

#ifdef SYSCTL_DEBUG
static void
sysctl_sysctl_debug_dump_node(struct sysctl_oid_list *l, int i)
{
	int k;
	struct sysctl_oid *oidp;

	SLIST_FOREACH(oidp, l, oid_link) {

		for (k=0; k<i; k++)
			printf(" ");

		printf("%d %s ", oidp->oid_number, oidp->oid_name);

		printf("%c%c",
			oidp->oid_kind & CTLFLAG_RD ? 'R':' ',
			oidp->oid_kind & CTLFLAG_WR ? 'W':' ');

		if (oidp->oid_handler)
			printf(" *Handler");

		switch (oidp->oid_kind & CTLTYPE) {
			case CTLTYPE_NODE:
				printf(" Node\n");
				if (!oidp->oid_handler) {
					sysctl_sysctl_debug_dump_node(
						oidp->oid_arg1, i+2);
				}
				break;
			case CTLTYPE_INT:    printf(" Int\n"); break;
			case CTLTYPE_STRING: printf(" String\n"); break;
			case CTLTYPE_QUAD:   printf(" Quad\n"); break;
			case CTLTYPE_OPAQUE: printf(" Opaque/struct\n"); break;
			default:	     printf("\n");
		}

	}
}

static int
sysctl_sysctl_debug(SYSCTL_HANDLER_ARGS)
{
	int error;

	error = priv_check(req->td, PRIV_SYSCTL_DEBUG);
	if (error)
		return (error);
	sysctl_sysctl_debug_dump_node(&sysctl__children, 0);
	return (ENOENT);
}

SYSCTL_PROC(_sysctl, 0, debug, CTLTYPE_STRING|CTLFLAG_RD,
	0, 0, sysctl_sysctl_debug, "-", "");
#endif

static int
sysctl_sysctl_name(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *) arg1;
	u_int namelen = arg2;
	int error = 0;
	struct sysctl_oid *oid;
	struct sysctl_oid_list *lsp = &sysctl__children, *lsp2;
	char buf[10];

	while (namelen) {
		if (!lsp) {
			snprintf(buf,sizeof(buf),"%d",*name);
			if (req->oldidx)
				error = SYSCTL_OUT(req, ".", 1);
			if (!error)
				error = SYSCTL_OUT(req, buf, strlen(buf));
			if (error)
				return (error);
			namelen--;
			name++;
			continue;
		}
		lsp2 = 0;
		SLIST_FOREACH(oid, lsp, oid_link) {
			if (oid->oid_number != *name)
				continue;

			if (req->oldidx)
				error = SYSCTL_OUT(req, ".", 1);
			if (!error)
				error = SYSCTL_OUT(req, oid->oid_name,
					strlen(oid->oid_name));
			if (error)
				return (error);

			namelen--;
			name++;

			if ((oid->oid_kind & CTLTYPE) != CTLTYPE_NODE) 
				break;

			if (oid->oid_handler)
				break;

			lsp2 = (struct sysctl_oid_list *)oid->oid_arg1;
			break;
		}
		lsp = lsp2;
	}
	return (SYSCTL_OUT(req, "", 1));
}

static SYSCTL_NODE(_sysctl, 1, name, CTLFLAG_RD, sysctl_sysctl_name, "");

static int
sysctl_sysctl_next_ls(struct sysctl_oid_list *lsp, int *name, u_int namelen, 
	int *next, int *len, int level, struct sysctl_oid **oidpp)
{
	struct sysctl_oid *oidp;

	*len = level;
	SLIST_FOREACH(oidp, lsp, oid_link) {
		*next = oidp->oid_number;
		*oidpp = oidp;

		if (oidp->oid_kind & CTLFLAG_SKIP)
			continue;

		if (!namelen) {
			if ((oidp->oid_kind & CTLTYPE) != CTLTYPE_NODE) 
				return (0);
			if (oidp->oid_handler) 
				/* We really should call the handler here...*/
				return (0);
			lsp = (struct sysctl_oid_list *)oidp->oid_arg1;
			if (!sysctl_sysctl_next_ls(lsp, 0, 0, next+1, 
				len, level+1, oidpp))
				return (0);
			goto emptynode;
		}

		if (oidp->oid_number < *name)
			continue;

		if (oidp->oid_number > *name) {
			if ((oidp->oid_kind & CTLTYPE) != CTLTYPE_NODE)
				return (0);
			if (oidp->oid_handler)
				return (0);
			lsp = (struct sysctl_oid_list *)oidp->oid_arg1;
			if (!sysctl_sysctl_next_ls(lsp, name+1, namelen-1, 
				next+1, len, level+1, oidpp))
				return (0);
			goto next;
		}
		if ((oidp->oid_kind & CTLTYPE) != CTLTYPE_NODE)
			continue;

		if (oidp->oid_handler)
			continue;

		lsp = (struct sysctl_oid_list *)oidp->oid_arg1;
		if (!sysctl_sysctl_next_ls(lsp, name+1, namelen-1, next+1, 
			len, level+1, oidpp))
			return (0);
	next:
		namelen = 1;
	emptynode:
		*len = level;
	}
	return (1);
}

static int
sysctl_sysctl_next(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *) arg1;
	u_int namelen = arg2;
	int i, j, error;
	struct sysctl_oid *oid;
	struct sysctl_oid_list *lsp = &sysctl__children;
	int newoid[CTL_MAXNAME];

	i = sysctl_sysctl_next_ls(lsp, name, namelen, newoid, &j, 1, &oid);
	if (i)
		return (ENOENT);
	error = SYSCTL_OUT(req, newoid, j * sizeof (int));
	return (error);
}

static SYSCTL_NODE(_sysctl, 2, next, CTLFLAG_RD, sysctl_sysctl_next, "");

static int
name2oid (char *name, int *oid, int *len, struct sysctl_oid **oidpp)
{
	int i;
	struct sysctl_oid *oidp;
	struct sysctl_oid_list *lsp = &sysctl__children;
	char *p;

	if (!*name)
		return (ENOENT);

	p = name + strlen(name) - 1 ;
	if (*p == '.')
		*p = '\0';

	*len = 0;

	for (p = name; *p && *p != '.'; p++) 
		;
	i = *p;
	if (i == '.')
		*p = '\0';

	oidp = SLIST_FIRST(lsp);

	while (oidp && *len < CTL_MAXNAME) {
		if (strcmp(name, oidp->oid_name)) {
			oidp = SLIST_NEXT(oidp, oid_link);
			continue;
		}
		*oid++ = oidp->oid_number;
		(*len)++;

		if (!i) {
			if (oidpp)
				*oidpp = oidp;
			return (0);
		}

		if ((oidp->oid_kind & CTLTYPE) != CTLTYPE_NODE)
			break;

		if (oidp->oid_handler)
			break;

		lsp = (struct sysctl_oid_list *)oidp->oid_arg1;
		oidp = SLIST_FIRST(lsp);
		name = p+1;
		for (p = name; *p && *p != '.'; p++) 
				;
		i = *p;
		if (i == '.')
			*p = '\0';
	}
	return (ENOENT);
}

static int
sysctl_sysctl_name2oid(SYSCTL_HANDLER_ARGS)
{
	char *p;
	int error, oid[CTL_MAXNAME], len;
	struct sysctl_oid *op = 0;

	if (!req->newlen) 
		return (ENOENT);
	if (req->newlen >= MAXPATHLEN)	/* XXX arbitrary, undocumented */
		return (ENAMETOOLONG);

	p = malloc(req->newlen+1, M_SYSCTL, M_WAITOK);

	error = SYSCTL_IN(req, p, req->newlen);
	if (error) {
		free(p, M_SYSCTL);
		return (error);
	}

	p [req->newlen] = '\0';

	error = name2oid(p, oid, &len, &op);

	free(p, M_SYSCTL);

	if (error)
		return (error);

	error = SYSCTL_OUT(req, oid, len * sizeof *oid);
	return (error);
}

SYSCTL_PROC(_sysctl, 3, name2oid, CTLFLAG_RW|CTLFLAG_ANYBODY, 0, 0, 
	sysctl_sysctl_name2oid, "I", "");

static int
sysctl_sysctl_oidfmt(SYSCTL_HANDLER_ARGS)
{
	struct sysctl_oid *oid;
	int error;

	error = sysctl_find_oid(arg1, arg2, &oid, NULL, req);
	if (error)
		return (error);

	if (!oid->oid_fmt)
		return (ENOENT);
	error = SYSCTL_OUT(req, &oid->oid_kind, sizeof(oid->oid_kind));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, oid->oid_fmt, strlen(oid->oid_fmt) + 1);
	return (error);
}


static SYSCTL_NODE(_sysctl, 4, oidfmt, CTLFLAG_RD, sysctl_sysctl_oidfmt, "");

static int
sysctl_sysctl_oiddescr(SYSCTL_HANDLER_ARGS)
{
	struct sysctl_oid *oid;
	int error;

	error = sysctl_find_oid(arg1, arg2, &oid, NULL, req);
	if (error)
		return (error);

	if (!oid->oid_descr)
		return (ENOENT);
	error = SYSCTL_OUT(req, oid->oid_descr, strlen(oid->oid_descr) + 1);
	return (error);
}

static SYSCTL_NODE(_sysctl, 5, oiddescr, CTLFLAG_RD, sysctl_sysctl_oiddescr, "");

/*
 * Default "handler" functions.
 */

/*
 * Handle an int, signed or unsigned.
 * Two cases:
 *     a variable:  point arg1 at it.
 *     a constant:  pass it in arg2.
 */

int
sysctl_handle_int(SYSCTL_HANDLER_ARGS)
{
	int tmpout, error = 0;

	/*
	 * Attempt to get a coherent snapshot by making a copy of the data.
	 */
	if (arg1)
		tmpout = *(int *)arg1;
	else
		tmpout = arg2;
	error = SYSCTL_OUT(req, &tmpout, sizeof(int));

	if (error || !req->newptr)
		return (error);

	if (!arg1)
		error = EPERM;
	else
		error = SYSCTL_IN(req, arg1, sizeof(int));
	return (error);
}


/*
 * Based on on sysctl_handle_int() convert milliseconds into ticks.
 */

int
sysctl_msec_to_ticks(SYSCTL_HANDLER_ARGS)
{
	int error, s, tt;

	tt = *(int *)oidp->oid_arg1;
	s = (int)((int64_t)tt * 1000 / hz);

	error = sysctl_handle_int(oidp, &s, 0, req);
	if (error || !req->newptr)
		return (error);

	tt = (int)((int64_t)s * hz / 1000);
	if (tt < 1)
		return (EINVAL);

	*(int *)oidp->oid_arg1 = tt;
	return (0);
}


/*
 * Handle a long, signed or unsigned.  arg1 points to it.
 */

int
sysctl_handle_long(SYSCTL_HANDLER_ARGS)
{
	int error = 0;
	long tmplong;
#ifdef SCTL_MASK32
	int tmpint;
#endif

	/*
	 * Attempt to get a coherent snapshot by making a copy of the data.
	 */
	if (!arg1)
		return (EINVAL);
	tmplong = *(long *)arg1;
#ifdef SCTL_MASK32
	if (req->flags & SCTL_MASK32) {
		tmpint = tmplong;
		error = SYSCTL_OUT(req, &tmpint, sizeof(int));
	} else
#endif
		error = SYSCTL_OUT(req, &tmplong, sizeof(long));

	if (error || !req->newptr)
		return (error);

#ifdef SCTL_MASK32
	if (req->flags & SCTL_MASK32) {
		error = SYSCTL_IN(req, &tmpint, sizeof(int));
		*(long *)arg1 = (long)tmpint;
	} else
#endif
		error = SYSCTL_IN(req, arg1, sizeof(long));
	return (error);
}

/*
 * Handle a 64 bit int, signed or unsigned.  arg1 points to it.
 */

int
sysctl_handle_quad(SYSCTL_HANDLER_ARGS)
{
	int error = 0;
	uint64_t tmpout;

	/*
	 * Attempt to get a coherent snapshot by making a copy of the data.
	 */
	if (!arg1)
		return (EINVAL);
	tmpout = *(uint64_t *)arg1;
	error = SYSCTL_OUT(req, &tmpout, sizeof(uint64_t));

	if (error || !req->newptr)
		return (error);

	error = SYSCTL_IN(req, arg1, sizeof(uint64_t));
	return (error);
}

/*
 * Handle our generic '\0' terminated 'C' string.
 * Two cases:
 * 	a variable string:  point arg1 at it, arg2 is max length.
 * 	a constant string:  point arg1 at it, arg2 is zero.
 */

int
sysctl_handle_string(SYSCTL_HANDLER_ARGS)
{
	int error=0;
	char *tmparg;
	size_t outlen;

	/*
	 * Attempt to get a coherent snapshot by copying to a
	 * temporary kernel buffer.
	 */
retry:
	outlen = strlen((char *)arg1)+1;
	tmparg = malloc(outlen, M_SYSCTLTMP, M_WAITOK);

	if (strlcpy(tmparg, (char *)arg1, outlen) >= outlen) {
		free(tmparg, M_SYSCTLTMP);
		goto retry;
	}

	error = SYSCTL_OUT(req, tmparg, outlen);
	free(tmparg, M_SYSCTLTMP);

	if (error || !req->newptr)
		return (error);

	if ((req->newlen - req->newidx) >= arg2) {
		error = EINVAL;
	} else {
		arg2 = (req->newlen - req->newidx);
		error = SYSCTL_IN(req, arg1, arg2);
		((char *)arg1)[arg2] = '\0';
	}

	return (error);
}

/*
 * Handle any kind of opaque data.
 * arg1 points to it, arg2 is the size.
 */

int
sysctl_handle_opaque(SYSCTL_HANDLER_ARGS)
{
	int error, tries;
	u_int generation;
	struct sysctl_req req2;

	/*
	 * Attempt to get a coherent snapshot, by using the thread
	 * pre-emption counter updated from within mi_switch() to
	 * determine if we were pre-empted during a bcopy() or
	 * copyout(). Make 3 attempts at doing this before giving up.
	 * If we encounter an error, stop immediately.
	 */
	tries = 0;
	req2 = *req;
retry:
	generation = curthread->td_generation;
	error = SYSCTL_OUT(req, arg1, arg2);
	if (error)
		return (error);
	tries++;
	if (generation != curthread->td_generation && tries < 3) {
		*req = req2;
		goto retry;
	}

	error = SYSCTL_IN(req, arg1, arg2);

	return (error);
}

/*
 * Transfer functions to/from kernel space.
 * XXX: rather untested at this point
 */
static int
sysctl_old_kernel(struct sysctl_req *req, const void *p, size_t l)
{
	size_t i = 0;

	if (req->oldptr) {
		i = l;
		if (req->oldlen <= req->oldidx)
			i = 0;
		else
			if (i > req->oldlen - req->oldidx)
				i = req->oldlen - req->oldidx;
		if (i > 0)
			bcopy(p, (char *)req->oldptr + req->oldidx, i);
	}
	req->oldidx += l;
	if (req->oldptr && i != l)
		return (ENOMEM);
	return (0);
}

static int
sysctl_new_kernel(struct sysctl_req *req, void *p, size_t l)
{
	if (!req->newptr)
		return (0);
	if (req->newlen - req->newidx < l)
		return (EINVAL);
	bcopy((char *)req->newptr + req->newidx, p, l);
	req->newidx += l;
	return (0);
}

int
kernel_sysctl(struct thread *td, int *name, u_int namelen, void *old,
    size_t *oldlenp, void *new, size_t newlen, size_t *retval, int flags)
{
	int error = 0;
	struct sysctl_req req;

	bzero(&req, sizeof req);

	req.td = td;
	req.flags = flags;

	if (oldlenp) {
		req.oldlen = *oldlenp;
	}
	req.validlen = req.oldlen;

	if (old) {
		req.oldptr= old;
	}

	if (new != NULL) {
		req.newlen = newlen;
		req.newptr = new;
	}

	req.oldfunc = sysctl_old_kernel;
	req.newfunc = sysctl_new_kernel;
	req.lock = REQ_LOCKED;

	SYSCTL_LOCK();

	error = sysctl_root(0, name, namelen, &req);

	if (req.lock == REQ_WIRED && req.validlen > 0)
		vsunlock(req.oldptr, req.validlen);

	SYSCTL_UNLOCK();

	if (error && error != ENOMEM)
		return (error);

	if (retval) {
		if (req.oldptr && req.oldidx > req.validlen)
			*retval = req.validlen;
		else
			*retval = req.oldidx;
	}
	return (error);
}

int
kernel_sysctlbyname(struct thread *td, char *name, void *old, size_t *oldlenp,
    void *new, size_t newlen, size_t *retval, int flags)
{
        int oid[CTL_MAXNAME];
        size_t oidlen, plen;
	int error;

	oid[0] = 0;		/* sysctl internal magic */
	oid[1] = 3;		/* name2oid */
	oidlen = sizeof(oid);

	error = kernel_sysctl(td, oid, 2, oid, &oidlen,
	    (void *)name, strlen(name), &plen, flags);
	if (error)
		return (error);

	error = kernel_sysctl(td, oid, plen / sizeof(int), old, oldlenp,
	    new, newlen, retval, flags);
	return (error);
}

/*
 * Transfer function to/from user space.
 */
static int
sysctl_old_user(struct sysctl_req *req, const void *p, size_t l)
{
	int error = 0;
	size_t i, len, origidx;

	origidx = req->oldidx;
	req->oldidx += l;
	if (req->oldptr == NULL)
		return (0);
	/*
	 * If we have not wired the user supplied buffer and we are currently
	 * holding locks, drop a witness warning, as it's possible that
	 * write operations to the user page can sleep.
	 */
	if (req->lock != REQ_WIRED)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "sysctl_old_user()");
	i = l;
	len = req->validlen;
	if (len <= origidx)
		i = 0;
	else {
		if (i > len - origidx)
			i = len - origidx;
		error = copyout(p, (char *)req->oldptr + origidx, i);
	}
	if (error)
		return (error);
	if (i < l)
		return (ENOMEM);
	return (0);
}

static int
sysctl_new_user(struct sysctl_req *req, void *p, size_t l)
{
	int error;

	if (!req->newptr)
		return (0);
	if (req->newlen - req->newidx < l)
		return (EINVAL);
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "sysctl_new_user()");
	error = copyin((char *)req->newptr + req->newidx, p, l);
	req->newidx += l;
	return (error);
}

/*
 * Wire the user space destination buffer.  If set to a value greater than
 * zero, the len parameter limits the maximum amount of wired memory.
 */
int
sysctl_wire_old_buffer(struct sysctl_req *req, size_t len)
{
	int ret;
	size_t i, wiredlen;
	char *cp, dummy;

	wiredlen = (len > 0 && len < req->oldlen) ? len : req->oldlen;
	ret = 0;
	if (req->lock == REQ_LOCKED && req->oldptr &&
	    req->oldfunc == sysctl_old_user) {
		if (wiredlen != 0) {
			ret = vslock(req->oldptr, wiredlen);
			if (ret != 0) {
				if (ret != ENOMEM)
					return (ret);
				wiredlen = 0;
			}
			/*
			 * Touch all the wired pages to avoid PTE modified
			 * bit emulation traps on Alpha while holding locks
			 * in the sysctl handler.
			 */
			for (i = (wiredlen + PAGE_SIZE - 1) / PAGE_SIZE,
			    cp = req->oldptr; i > 0; i--, cp += PAGE_SIZE) {
				copyin(cp, &dummy, 1);
				copyout(&dummy, cp, 1);
			}
		}
		req->lock = REQ_WIRED;
		req->validlen = wiredlen;
	}
	return (0);
}

int
sysctl_find_oid(int *name, u_int namelen, struct sysctl_oid **noid,
    int *nindx, struct sysctl_req *req)
{
	struct sysctl_oid *oid;
	int indx;

	oid = SLIST_FIRST(&sysctl__children);
	indx = 0;
	while (oid && indx < CTL_MAXNAME) {
		if (oid->oid_number == name[indx]) {
			indx++;
			if (oid->oid_kind & CTLFLAG_NOLOCK)
				req->lock = REQ_UNLOCKED;
			if ((oid->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
				if (oid->oid_handler != NULL ||
				    indx == namelen) {
					*noid = oid;
					if (nindx != NULL)
						*nindx = indx;
					return (0);
				}
				oid = SLIST_FIRST(
				    (struct sysctl_oid_list *)oid->oid_arg1);
			} else if (indx == namelen) {
				*noid = oid;
				if (nindx != NULL)
					*nindx = indx;
				return (0);
			} else {
				return (ENOTDIR);
			}
		} else {
			oid = SLIST_NEXT(oid, oid_link);
		}
	}
	return (ENOENT);
}

/*
 * Traverse our tree, and find the right node, execute whatever it points
 * to, and return the resulting error code.
 */

static int
sysctl_root(SYSCTL_HANDLER_ARGS)
{
	struct sysctl_oid *oid;
	int error, indx, lvl;

	error = sysctl_find_oid(arg1, arg2, &oid, &indx, req);
	if (error)
		return (error);

	if ((oid->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
		/*
		 * You can't call a sysctl when it's a node, but has
		 * no handler.  Inform the user that it's a node.
		 * The indx may or may not be the same as namelen.
		 */
		if (oid->oid_handler == NULL)
			return (EISDIR);
	}

	/* Is this sysctl writable? */
	if (req->newptr && !(oid->oid_kind & CTLFLAG_WR))
		return (EPERM);

	KASSERT(req->td != NULL, ("sysctl_root(): req->td == NULL"));

	/* Is this sysctl sensitive to securelevels? */
	if (req->newptr && (oid->oid_kind & CTLFLAG_SECURE)) {
		lvl = (oid->oid_kind & CTLMASK_SECURE) >> CTLSHIFT_SECURE;
		error = securelevel_gt(req->td->td_ucred, lvl);
		if (error)
			return (error);
	}

	/* Is this sysctl writable by only privileged users? */
	if (req->newptr && !(oid->oid_kind & CTLFLAG_ANYBODY)) {
		if (oid->oid_kind & CTLFLAG_PRISON)
			error = priv_check(req->td, PRIV_SYSCTL_WRITEJAIL);
		else
			error = priv_check(req->td, PRIV_SYSCTL_WRITE);
		if (error)
			return (error);
	}

	if (!oid->oid_handler)
		return (EINVAL);

	if ((oid->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
		arg1 = (int *)arg1 + indx;
		arg2 -= indx;
	} else {
		arg1 = oid->oid_arg1;
		arg2 = oid->oid_arg2;
	}
#ifdef MAC
	error = mac_check_system_sysctl(req->td->td_ucred, oid, arg1, arg2,
	    req);
	if (error != 0)
		return (error);
#endif
	error = oid->oid_handler(oid, arg1, arg2, req);

	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct sysctl_args {
	int	*name;
	u_int	namelen;
	void	*old;
	size_t	*oldlenp;
	void	*new;
	size_t	newlen;
};
#endif
int
__sysctl(struct thread *td, struct sysctl_args *uap)
{
	int error, name[CTL_MAXNAME];
	size_t j;

	if (uap->namelen > CTL_MAXNAME || uap->namelen < 2)
		return (EINVAL);

 	error = copyin(uap->name, &name, uap->namelen * sizeof(int));
 	if (error)
		return (error);

	mtx_lock(&Giant);

	error = userland_sysctl(td, name, uap->namelen,
		uap->old, uap->oldlenp, 0,
		uap->new, uap->newlen, &j, 0);
	if (error && error != ENOMEM)
		goto done2;
	if (uap->oldlenp) {
		int i = copyout(&j, uap->oldlenp, sizeof(j));
		if (i)
			error = i;
	}
done2:
	mtx_unlock(&Giant);
	return (error);
}

/*
 * This is used from various compatibility syscalls too.  That's why name
 * must be in kernel space.
 */
int
userland_sysctl(struct thread *td, int *name, u_int namelen, void *old,
    size_t *oldlenp, int inkernel, void *new, size_t newlen, size_t *retval,
    int flags)
{
	int error = 0;
	struct sysctl_req req;

	bzero(&req, sizeof req);

	req.td = td;
	req.flags = flags;

	if (oldlenp) {
		if (inkernel) {
			req.oldlen = *oldlenp;
		} else {
			error = copyin(oldlenp, &req.oldlen, sizeof(*oldlenp));
			if (error)
				return (error);
		}
	}
	req.validlen = req.oldlen;

	if (old) {
		if (!useracc(old, req.oldlen, VM_PROT_WRITE))
			return (EFAULT);
		req.oldptr= old;
	}

	if (new != NULL) {
		if (!useracc(new, newlen, VM_PROT_READ))
			return (EFAULT);
		req.newlen = newlen;
		req.newptr = new;
	}

	req.oldfunc = sysctl_old_user;
	req.newfunc = sysctl_new_user;
	req.lock = REQ_LOCKED;

	SYSCTL_LOCK();

	for (;;) {
		req.oldidx = 0;
		req.newidx = 0;
		error = sysctl_root(0, name, namelen, &req);
		if (error != EAGAIN)
			break;
		uio_yield();
	}

	if (req.lock == REQ_WIRED && req.validlen > 0)
		vsunlock(req.oldptr, req.validlen);

	SYSCTL_UNLOCK();

	if (error && error != ENOMEM)
		return (error);

	if (retval) {
		if (req.oldptr && req.oldidx > req.validlen)
			*retval = req.validlen;
		else
			*retval = req.oldidx;
	}
	return (error);
}

#ifdef COMPAT_43
#include <sys/socket.h>
#include <vm/vm_param.h>

#define	KINFO_PROC		(0<<8)
#define	KINFO_RT		(1<<8)
#define	KINFO_VNODE		(2<<8)
#define	KINFO_FILE		(3<<8)
#define	KINFO_METER		(4<<8)
#define	KINFO_LOADAVG		(5<<8)
#define	KINFO_CLOCKRATE		(6<<8)

/* Non-standard BSDI extension - only present on their 4.3 net-2 releases */
#define	KINFO_BSDI_SYSINFO	(101<<8)

/*
 * XXX this is bloat, but I hope it's better here than on the potentially
 * limited kernel stack...  -Peter
 */

static struct {
	int	bsdi_machine;		/* "i386" on BSD/386 */
/*      ^^^ this is an offset to the string, relative to the struct start */
	char	*pad0;
	long	pad1;
	long	pad2;
	long	pad3;
	u_long	pad4;
	u_long	pad5;
	u_long	pad6;

	int	bsdi_ostype;		/* "BSD/386" on BSD/386 */
	int	bsdi_osrelease;		/* "1.1" on BSD/386 */
	long	pad7;
	long	pad8;
	char	*pad9;

	long	pad10;
	long	pad11;
	int	pad12;
	long	pad13;
	quad_t	pad14;
	long	pad15;

	struct	timeval pad16;
	/* we dont set this, because BSDI's uname used gethostname() instead */
	int	bsdi_hostname;		/* hostname on BSD/386 */

	/* the actual string data is appended here */

} bsdi_si;

/*
 * this data is appended to the end of the bsdi_si structure during copyout.
 * The "char *" offsets are relative to the base of the bsdi_si struct.
 * This contains "FreeBSD\02.0-BUILT-nnnnnn\0i386\0", and these strings
 * should not exceed the length of the buffer here... (or else!! :-)
 */
static char bsdi_strings[80];	/* It had better be less than this! */

#ifndef _SYS_SYSPROTO_H_
struct getkerninfo_args {
	int	op;
	char	*where;
	size_t	*size;
	int	arg;
};
#endif
int
ogetkerninfo(struct thread *td, struct getkerninfo_args *uap)
{
	int error, name[6];
	size_t size;
	u_int needed = 0;

	mtx_lock(&Giant);

	switch (uap->op & 0xff00) {

	case KINFO_RT:
		name[0] = CTL_NET;
		name[1] = PF_ROUTE;
		name[2] = 0;
		name[3] = (uap->op & 0xff0000) >> 16;
		name[4] = uap->op & 0xff;
		name[5] = uap->arg;
		error = userland_sysctl(td, name, 6, uap->where, uap->size,
			0, 0, 0, &size, 0);
		break;

	case KINFO_VNODE:
		name[0] = CTL_KERN;
		name[1] = KERN_VNODE;
		error = userland_sysctl(td, name, 2, uap->where, uap->size,
			0, 0, 0, &size, 0);
		break;

	case KINFO_PROC:
		name[0] = CTL_KERN;
		name[1] = KERN_PROC;
		name[2] = uap->op & 0xff;
		name[3] = uap->arg;
		error = userland_sysctl(td, name, 4, uap->where, uap->size,
			0, 0, 0, &size, 0);
		break;

	case KINFO_FILE:
		name[0] = CTL_KERN;
		name[1] = KERN_FILE;
		error = userland_sysctl(td, name, 2, uap->where, uap->size,
			0, 0, 0, &size, 0);
		break;

	case KINFO_METER:
		name[0] = CTL_VM;
		name[1] = VM_TOTAL;
		error = userland_sysctl(td, name, 2, uap->where, uap->size,
			0, 0, 0, &size, 0);
		break;

	case KINFO_LOADAVG:
		name[0] = CTL_VM;
		name[1] = VM_LOADAVG;
		error = userland_sysctl(td, name, 2, uap->where, uap->size,
			0, 0, 0, &size, 0);
		break;

	case KINFO_CLOCKRATE:
		name[0] = CTL_KERN;
		name[1] = KERN_CLOCKRATE;
		error = userland_sysctl(td, name, 2, uap->where, uap->size,
			0, 0, 0, &size, 0);
		break;

	case KINFO_BSDI_SYSINFO: {
		/*
		 * this is pretty crude, but it's just enough for uname()
		 * from BSDI's 1.x libc to work.
		 *
		 * *size gives the size of the buffer before the call, and
		 * the amount of data copied after a successful call.
		 * If successful, the return value is the amount of data
		 * available, which can be larger than *size.
		 *
		 * BSDI's 2.x product apparently fails with ENOMEM if *size
		 * is too small.
		 */

		u_int left;
		char *s;

		bzero((char *)&bsdi_si, sizeof(bsdi_si));
		bzero(bsdi_strings, sizeof(bsdi_strings));

		s = bsdi_strings;

		bsdi_si.bsdi_ostype = (s - bsdi_strings) + sizeof(bsdi_si);
		strcpy(s, ostype);
		s += strlen(s) + 1;

		bsdi_si.bsdi_osrelease = (s - bsdi_strings) + sizeof(bsdi_si);
		strcpy(s, osrelease);
		s += strlen(s) + 1;

		bsdi_si.bsdi_machine = (s - bsdi_strings) + sizeof(bsdi_si);
		strcpy(s, machine);
		s += strlen(s) + 1;

		needed = sizeof(bsdi_si) + (s - bsdi_strings);

		if ((uap->where == NULL) || (uap->size == NULL)) {
			/* process is asking how much buffer to supply.. */
			size = needed;
			error = 0;
			break;
		}

		if ((error = copyin(uap->size, &size, sizeof(size))) != 0)
			break;

		/* if too much buffer supplied, trim it down */
		if (size > needed)
			size = needed;

		/* how much of the buffer is remaining */
		left = size;

		if ((error = copyout((char *)&bsdi_si, uap->where, left)) != 0)
			break;

		/* is there any point in continuing? */
		if (left > sizeof(bsdi_si)) {
			left -= sizeof(bsdi_si);
			error = copyout(&bsdi_strings,
					uap->where + sizeof(bsdi_si), left);
		}
		break;
	}

	default:
		error = EOPNOTSUPP;
		break;
	}
	if (error == 0) {
		td->td_retval[0] = needed ? needed : size;
		if (uap->size) {
			error = copyout(&size, uap->size, sizeof(size));
		}
	}
	mtx_unlock(&Giant);
	return (error);
}
#endif /* COMPAT_43 */
