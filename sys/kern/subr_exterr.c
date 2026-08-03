/*-
 * Copyright (c) 2026 Capabilities Limited
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This software was developed by Capabilities Limited with funding from
 * Innovate UK and the Department for Science, Innovation and Technology
 * for the adoption and diffusion of CHERI technology under project
 * 10168042 (“CheriBSD feature extraction, maturity, and testing”).
 *
 */

#define	EXTERR_CATEGORY_DYNAMIC	"kern/subr_exterr.c"

#include <sys/param.h>
#include <sys/exterrvar.h>
#include <sys/exterr_cat.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/linker_set.h>
#include <sys/malloc.h>
#include <sys/linker.h>	/* Need MALLOC_DECLARE */
#include <sys/rwlock.h>
#include <sys/stddef.h>
#include <sys/sysctl.h>

struct exterr_cat_span {
	unsigned int			first;
	unsigned int 			count;
	struct exterr_cat		**cat_sets;
	TAILQ_ENTRY(exterr_cat_span)	entries;
};

TAILQ_HEAD(exterr_cat_span_head, exterr_cat_span) cat_span_head;

SET_DECLARE(exterr_cats, struct exterr_cat);

static struct exterr_cat_span kern_cats;
unsigned int ncats;
struct rwlock cat_lock;

static bool
exterr_cat_register_set(struct exterr_cat_span *span, struct exterr_cat **start,
    struct exterr_cat **stop)
{
	struct exterr_cat **catp;
	ptrdiff_t count;

	count = stop - start;
	if (count < 1)
		return (true);

	rw_wlock(&cat_lock);

	if (ncats + count < ncats) {
		printf("too many exterror categories\n");
		rw_wunlock(&cat_lock);
		return (false);
	}

	span->first = ncats + 1;
	for (catp = start; catp < stop; catp++)
		(*catp)->cat = ++ncats;
	span->count = count;
	span->cat_sets = start;
	TAILQ_INSERT_TAIL(&cat_span_head, span, entries);

	rw_wunlock(&cat_lock);

	return (true);
}

void
exterr_cat_register_module(struct exterr_cat **start, struct exterr_cat **stop)
{
	struct exterr_cat_span *span;

	span = malloc(sizeof(*span), M_LINKER, M_WAITOK | M_ZERO);
	if (!exterr_cat_register_set(span, start, stop))
		free(span, M_LINKER);
}

void
exterr_cat_unregister_module(struct exterr_cat **start,
    struct exterr_cat **stop)
{
	struct exterr_cat_span *span;

	if (stop - start < 1)
		return;

	rw_wlock(&cat_lock);

	TAILQ_FOREACH(span, &cat_span_head, entries) {
		if (span->cat_sets == start) {
			MPASS(span->first > 1);
			MPASS(span->count == stop - start);
			TAILQ_REMOVE(&cat_span_head, span, entries);
			break;
		}
	}
	KASSERT(span != NULL, ("start not found in spans"));

	/*
	 * NB: we leak category numbers on module unload because we can't
	 * reasonably know which ones are in use in running software.
	 */

	rw_wunlock(&cat_lock);

	free(span, M_LINKER);
}

static void
exterr_cat_register_kern(void *arg)
{
	rw_init(&cat_lock, "exterr dynamic categories");

	TAILQ_INIT(&cat_span_head);

	if (SET_COUNT(exterr_cats) == 0)
		return;

	exterr_cat_register_set(&kern_cats, SET_BEGIN(exterr_cats),
	    SET_LIMIT(exterr_cats));
}
SYSINIT(exterr, SI_SUB_KMEM, SI_ORDER_FIRST, exterr_cat_register_kern, NULL);

static int
sysctl_exterr_categories(SYSCTL_HANDLER_ARGS)
{
	struct exterr_cat_span *span;
	const struct exterr_cat *cat = NULL;
	int idx;

	if (arg2 != 1)
		return (EXTERROR(EINVAL,
		    "too many args to kern.exterr.categories %d", arg2));

	idx = *(int *)arg1;

	rw_rlock(&cat_lock);
	if (idx < 1 || idx > ncats) {
		rw_runlock(&cat_lock);
		return (EXTERROR(EINVAL, "category %d out of range (1...%d)",
		    idx, ncats));
	}

	TAILQ_FOREACH(span, &cat_span_head, entries) {
		if (idx < span->first)
			break;	/* Not here any more */

		if (idx < span->first + span->count)
			cat = span->cat_sets[idx - span->first];
	}
	rw_runlock(&cat_lock);

	if (cat == NULL)
		return (EXTERROR(ENOENT, "category not found %d", idx));
	MPASS(cat->cat == idx);
	return (SYSCTL_OUT(req, cat->file, strlen(cat->file) + 1));
}

SYSCTL_NODE(_kern, OID_AUTO, exterr, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Extended error information");
SYSCTL_UINT(_kern_exterr, OID_AUTO, ncategories, CTLFLAG_RD | CTLFLAG_MPSAFE,
    &ncats, 0, "Number of dynamic categories");
SYSCTL_NODE(_kern_exterr, OID_AUTO, categories, CTLFLAG_RD | CTLFLAG_MPSAFE,
    sysctl_exterr_categories, "Extended error categories");
