/*
 * Copyright (c) 1996-1999 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* ev_waits.c - implement deferred function calls for the eventlib
 * vix 05dec95 [initial]
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: ev_waits.c,v 8.11 2000/07/20 18:17:52 vixie Exp $";
#endif

#include "port_before.h"
#include "fd_setsize.h"

#include <errno.h>

#include <isc/eventlib.h>
#include <isc/assertions.h>
#include "eventlib_p.h"

#include "port_after.h"

/* Forward. */

static void		print_waits(evContext_p *ctx);
static evWaitList *	evNewWaitList(evContext_p *);
static void		evFreeWaitList(evContext_p *, evWaitList *);
static evWaitList *	evGetWaitList(evContext_p *, const void *, int);


/* Public. */

/*
 * Enter a new wait function on the queue.
 */
int
evWaitFor(evContext opaqueCtx, const void *tag,
	  evWaitFunc func, void *uap, evWaitID *id)
{
	evContext_p *ctx = opaqueCtx.opaque;
	evWait *new;
	evWaitList *wl = evGetWaitList(ctx, tag, 1);

	OKNEW(new);
	new->func = func;
	new->uap = uap;
	new->tag = tag;
	new->next = NULL;
	if (wl->last != NULL)
		wl->last->next = new;
	else
		wl->first = new;
	wl->last = new;
	if (id != NULL)
		id->opaque = new;
	if (ctx->debug >= 9)
		print_waits(ctx);
	return (0);
}

/*
 * Mark runnable all waiting functions having a certain tag.
 */
int
evDo(evContext opaqueCtx, const void *tag) {
	evContext_p *ctx = opaqueCtx.opaque;
	evWaitList *wl = evGetWaitList(ctx, tag, 0);
	evWait *first;

	if (!wl) {
		errno = ENOENT;
		return (-1);
	}

	first = wl->first;
	INSIST(first != NULL);

	if (ctx->waitDone.last != NULL)
		ctx->waitDone.last->next = first;
	else
		ctx->waitDone.first = first;
	ctx->waitDone.last = wl->last;
	evFreeWaitList(ctx, wl);

	return (0);
}

/*
 * Remove a waiting (or ready to run) function from the queue.
 */
int
evUnwait(evContext opaqueCtx, evWaitID id) {
	evContext_p *ctx = opaqueCtx.opaque;
	evWait *this, *prev;
	evWaitList *wl;
	int found = 0;

	this = id.opaque;
	INSIST(this != NULL);
	wl = evGetWaitList(ctx, this->tag, 0);
	if (wl != NULL) {
		for (prev = NULL, this = wl->first;
		     this != NULL;
		     prev = this, this = this->next)
			if (this == (evWait *)id.opaque) {
				found = 1;
				if (prev != NULL)
					prev->next = this->next;
				else
					wl->first = this->next;
				if (wl->last == this)
					wl->last = prev;
				if (wl->first == NULL)
					evFreeWaitList(ctx, wl);
				break;
			}
	}

	if (!found) {
		/* Maybe it's done */
		for (prev = NULL, this = ctx->waitDone.first;
		     this != NULL;
		     prev = this, this = this->next)
			if (this == (evWait *)id.opaque) {
				found = 1;
				if (prev != NULL)
					prev->next = this->next;
				else
					ctx->waitDone.first = this->next;
				if (ctx->waitDone.last == this)
					ctx->waitDone.last = prev;
				break;
			}
	}

	if (!found) {
		errno = ENOENT;
		return (-1);
	}

	FREE(this);

	if (ctx->debug >= 9)
		print_waits(ctx);

	return (0);
}

int
evDefer(evContext opaqueCtx, evWaitFunc func, void *uap) {
	evContext_p *ctx = opaqueCtx.opaque;
	evWait *new;

	OKNEW(new);
	new->func = func;
	new->uap = uap;
	new->tag = NULL;
	new->next = NULL;
	if (ctx->waitDone.last != NULL)
		ctx->waitDone.last->next = new;
	else
		ctx->waitDone.first = new;
	ctx->waitDone.last = new;
	if (ctx->debug >= 9)
		print_waits(ctx);
	return (0);
}

/* Private. */

static void
print_waits(evContext_p *ctx) {
	evWaitList *wl;
	evWait *this;

	evPrintf(ctx, 9, "wait waiting:\n");
	for (wl = ctx->waitLists; wl != NULL; wl = wl->next) {
		INSIST(wl->first != NULL);
		evPrintf(ctx, 9, "  tag %#x:", wl->first->tag);
		for (this = wl->first; this != NULL; this = this->next)
			evPrintf(ctx, 9, " %#x", this);
		evPrintf(ctx, 9, "\n");
	}
	evPrintf(ctx, 9, "wait done:");
	for (this = ctx->waitDone.first; this != NULL; this = this->next)
		evPrintf(ctx, 9, " %#x", this);
	evPrintf(ctx, 9, "\n");
}

static evWaitList *
evNewWaitList(evContext_p *ctx) {
	evWaitList *new;

	NEW(new);
	if (new == NULL)
		return (NULL);
	new->first = new->last = NULL;
	new->prev = NULL;
	new->next = ctx->waitLists;
	if (new->next != NULL)
		new->next->prev = new;
	ctx->waitLists = new;
	return (new);
}

static void
evFreeWaitList(evContext_p *ctx, evWaitList *this) {

	INSIST(this != NULL);

	if (this->prev != NULL)
		this->prev->next = this->next;
	else
		ctx->waitLists = this->next;
	if (this->next != NULL)
		this->next->prev = this->prev;
	FREE(this);
}

static evWaitList *
evGetWaitList(evContext_p *ctx, const void *tag, int should_create) {
	evWaitList *this;

	for (this = ctx->waitLists; this != NULL; this = this->next) {
		if (this->first != NULL && this->first->tag == tag)
			break;
	}
	if (this == NULL && should_create)
		this = evNewWaitList(ctx);
	return (this);
}
