/*
 * Copyright (c) 2017-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "pt_sb_session.h"
#include "pt_sb_context.h"
#include "pt_sb_decoder.h"

#include "libipt-sb.h"
#include "intel-pt.h"

#include <string.h>
#include <stdlib.h>

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#  define snprintf _snprintf_c
#endif


struct pt_sb_session *pt_sb_alloc(struct pt_image_section_cache *iscache)
{
	struct pt_sb_session *session;
	struct pt_image *kernel;

	kernel = pt_image_alloc("kernel");
	if (!kernel)
		return NULL;

	session = malloc(sizeof(*session));
	if (!session) {
		pt_image_free(kernel);
		return NULL;
	}

	memset(session, 0, sizeof(*session));
	session->iscache = iscache;
	session->kernel = kernel;

	return session;
}

static void pt_sb_free_decoder(struct pt_sb_decoder *decoder)
{
	void (*dtor)(void *);

	if (!decoder)
		return;

	dtor = decoder->dtor;
	if (dtor)
		dtor(decoder->priv);

	free(decoder);
}

static void pt_sb_free_decoder_list(struct pt_sb_decoder *list)
{
	while (list) {
		struct pt_sb_decoder *trash;

		trash = list;
		list = trash->next;

		pt_sb_free_decoder(trash);
	}
}

void pt_sb_free(struct pt_sb_session *session)
{
	struct pt_sb_context *context;

	if (!session)
		return;

	pt_sb_free_decoder_list(session->decoders);
	pt_sb_free_decoder_list(session->waiting);
	pt_sb_free_decoder_list(session->retired);
	pt_sb_free_decoder_list(session->removed);

	context = session->contexts;
	while (context) {
		struct pt_sb_context *trash;

		trash = context;
		context = trash->next;

		(void) pt_sb_ctx_put(trash);
	}

	pt_image_free(session->kernel);

	free(session);
}

struct pt_image_section_cache *pt_sb_iscache(struct pt_sb_session *session)
{
	if (!session)
		return NULL;

	return session->iscache;
}

struct pt_image *pt_sb_kernel_image(struct pt_sb_session *session)
{
	if (!session)
		return NULL;

	return session->kernel;
}

static int pt_sb_add_context_by_pid(struct pt_sb_context **pcontext,
				    struct pt_sb_session *session, uint32_t pid)
{
	struct pt_sb_context *context;
	struct pt_image *kernel;
	char iname[16];
	int errcode;

	if (!pcontext || !session)
		return -pte_invalid;

	kernel = pt_sb_kernel_image(session);
	if (!kernel)
		return -pte_internal;

	memset(iname, 0, sizeof(iname));
	(void) snprintf(iname, sizeof(iname), "pid-%x", pid);

	context = pt_sb_ctx_alloc(iname);
	if (!context)
		return -pte_nomem;

	errcode = pt_image_copy(context->image, kernel);
	if (errcode < 0) {
		(void) pt_sb_ctx_put(context);
		return errcode;
	}

	context->next = session->contexts;
	context->pid = pid;

	session->contexts = context;
	*pcontext = context;

	return 0;
}

int pt_sb_get_context_by_pid(struct pt_sb_context **context,
			     struct pt_sb_session *session, uint32_t pid)
{
	int status;

	if (!context || !session)
		return -pte_invalid;

	status = pt_sb_find_context_by_pid(context, session, pid);
	if (status < 0)
		return status;

	if (*context)
		return 0;

	return pt_sb_add_context_by_pid(context, session, pid);
}

int pt_sb_find_context_by_pid(struct pt_sb_context **pcontext,
			      struct pt_sb_session *session, uint32_t pid)
{
	struct pt_sb_context *ctx;

	if (!pcontext || !session)
		return -pte_invalid;

	for (ctx = session->contexts; ctx; ctx = ctx->next) {
		if (ctx->pid == pid)
			break;
	}

	*pcontext = ctx;

	return 0;
}

int pt_sb_remove_context(struct pt_sb_session *session,
			 struct pt_sb_context *context)
{
	struct pt_sb_context **pnext, *ctx;

	if (!session || !context)
		return -pte_invalid;

	pnext = &session->contexts;
	for (ctx = *pnext; ctx; pnext = &ctx->next, ctx = *pnext) {
		if (ctx == context)
			break;
	}

	if (!ctx)
		return -pte_nosync;

	*pnext = ctx->next;

	return pt_sb_ctx_put(ctx);
}

int pt_sb_alloc_decoder(struct pt_sb_session *session,
			const struct pt_sb_decoder_config *config)
{
	struct pt_sb_decoder *decoder;

	if (!session || !config)
		return -pte_invalid;

	decoder = malloc(sizeof(*decoder));
	if (!decoder)
		return -pte_nomem;

	memset(decoder, 0, sizeof(*decoder));
	decoder->next = session->waiting;
	decoder->fetch = config->fetch;
	decoder->apply = config->apply;
	decoder->print = config->print;
	decoder->dtor = config->dtor;
	decoder->priv = config->priv;
	decoder->primary = config->primary;

	session->waiting = decoder;

	return 0;
}

/* Add a new decoder to a list of decoders.
 *
 * Decoders in @list are ordered by their @tsc (ascending) and @list does not
 * contain @decoder.  Find the right place for @decoder at add it to @list.
 *
 * Returns zero on success, a negative error code otherwise.
 */
static int pt_sb_add_decoder(struct pt_sb_decoder **list,
			     struct pt_sb_decoder *decoder)
{
	struct pt_sb_decoder *cand;
	uint64_t tsc;

	if (!list || !decoder || decoder->next)
		return -pte_internal;

	tsc = decoder->tsc;
	for (cand = *list; cand; list = &cand->next, cand = *list) {
		if (tsc <= cand->tsc)
			break;
	}

	decoder->next = cand;
	*list = decoder;

	return 0;
}

static int pt_sb_fetch(struct pt_sb_session *session,
		       struct pt_sb_decoder *decoder)
{
	int (*fetch)(struct pt_sb_session *, uint64_t *, void *);

	if (!decoder)
		return -pte_internal;

	fetch = decoder->fetch;
	if (!fetch)
		return -pte_bad_config;

	return fetch(session, &decoder->tsc, decoder->priv);
}

static int pt_sb_print(struct pt_sb_session *session,
		       struct pt_sb_decoder *decoder, FILE *stream,
		       uint32_t flags)
{
	int (*print)(struct pt_sb_session *, FILE *, uint32_t, void *);

	if (!decoder)
		return -pte_internal;

	print = decoder->print;
	if (!print)
		return -pte_bad_config;

	return print(session, stream, flags, decoder->priv);
}

static int pt_sb_apply(struct pt_sb_session *session, struct pt_image **image,
		       struct pt_sb_decoder *decoder,
		       const struct pt_event *event)
{
	int (*apply)(struct pt_sb_session *, struct pt_image **,
		     const struct pt_event *, void *);

	if (!decoder || !event)
		return -pte_internal;

	apply = decoder->apply;
	if (!apply)
		return -pte_bad_config;

	if (!decoder->primary)
		image = NULL;

	return apply(session, image, event, decoder->priv);
}

int pt_sb_init_decoders(struct pt_sb_session *session)
{
	struct pt_sb_decoder *decoder;

	if (!session)
		return -pte_invalid;

	decoder = session->waiting;
	while (decoder) {
		int errcode;

		session->waiting = decoder->next;
		decoder->next = NULL;

		errcode = pt_sb_fetch(session, decoder);
		if (errcode < 0) {
			/* Fetch errors remove @decoder.  In this case, they
			 * prevent it from being added in the first place.
			 */
			pt_sb_free_decoder(decoder);
		} else {
			errcode = pt_sb_add_decoder(&session->decoders,
						    decoder);
			if (errcode < 0)
				return errcode;
		}

		decoder = session->waiting;
	}

	return 0;
}

/* Copy an event provided by an unknown version of libipt.
 *
 * Copy at most @size bytes of @uevent into @event and zero-initialize any
 * additional bytes in @event not covered by @uevent.
 *
 * Returns zero on success, a negative error code otherwise.
 */
static int pt_sb_event_from_user(struct pt_event *event,
				 const struct pt_event *uevent, size_t size)
{
	if (!event || !uevent)
		return -pte_internal;

	if (size < offsetof(struct pt_event, reserved))
		return -pte_invalid;

	/* Ignore fields in the user's event we don't know; zero out fields the
	 * user didn't know about.
	 */
	if (sizeof(*event) < size)
		size = sizeof(*event);
	else
		memset(((uint8_t *) event) + size, 0, sizeof(*event) - size);

	/* Copy (portions of) the user's event. */
	memcpy(event, uevent, size);

	return 0;
}

static int pt_sb_event_present(struct pt_sb_session *session,
			       struct pt_image **image,
			       struct pt_sb_decoder **pnext,
			       const struct pt_event *event)
{
	struct pt_sb_decoder *decoder;
	int errcode;

	if (!session || !pnext)
		return -pte_internal;

	decoder = *pnext;
	while (decoder) {
		errcode = pt_sb_apply(session, image, decoder, event);
		if (errcode < 0) {
			struct pt_sb_decoder *trash;

			trash = decoder;
			decoder = trash->next;
			*pnext = decoder;

			trash->next = session->removed;
			session->removed = trash;
			continue;
		}

		pnext = &decoder->next;
		decoder = *pnext;
	}

	return 0;
}

int pt_sb_event(struct pt_sb_session *session, struct pt_image **image,
		const struct pt_event *uevent, size_t size, FILE *stream,
		uint32_t flags)
{
	struct pt_sb_decoder *decoder;
	struct pt_event event;
	int errcode;

	if (!session || !uevent)
		return -pte_invalid;

	errcode = pt_sb_event_from_user(&event, uevent, size);
	if (errcode < 0)
		return errcode;

	/* In the initial round, we present the event to all decoders with
	 * records for a smaller or equal timestamp.
	 *
	 * We only need to look at the first decoder.  We remove it from the
	 * list and ask it to apply the event.  Then, we ask it to fetch the
	 * next record and re-add it to the list according to that next record's
	 * timestamp.
	 */
	for (;;) {
		decoder = session->decoders;
		if (!decoder)
			break;

		/* We don't check @event.has_tsc to support sideband
		 * correlation based on relative (non-wall clock) time.
		 */
		if (event.tsc < decoder->tsc)
			break;

		session->decoders = decoder->next;
		decoder->next = NULL;

		if (stream) {
			errcode = pt_sb_print(session, decoder, stream, flags);
			if (errcode < 0) {
				decoder->next = session->removed;
				session->removed = decoder;
				continue;
			}
		}

		errcode = pt_sb_apply(session, image, decoder, &event);
		if (errcode < 0) {
			decoder->next = session->removed;
			session->removed = decoder;
			continue;
		}

		errcode = pt_sb_fetch(session, decoder);
		if (errcode < 0) {
			if (errcode == -pte_eos) {
				decoder->next = session->retired;
				session->retired = decoder;
			} else {
				decoder->next = session->removed;
				session->removed = decoder;
			}

			continue;
		}

		errcode = pt_sb_add_decoder(&session->decoders, decoder);
		if (errcode < 0)
			return errcode;
	}

	/* In the second round, we present the event to all decoders.
	 *
	 * This allows decoders to postpone actions until an appropriate event,
	 * e.g entry into or exit from the kernel.
	 */
	errcode = pt_sb_event_present(session, image, &session->decoders,
				      &event);
	if (errcode < 0)
		return errcode;

	return pt_sb_event_present(session, image, &session->retired, &event);
}

int pt_sb_dump(struct pt_sb_session *session, FILE *stream, uint32_t flags,
	       uint64_t tsc)
{
	struct pt_sb_decoder *decoder;
	int errcode;

	if (!session || !stream)
		return -pte_invalid;

	for (;;) {
		decoder = session->decoders;
		if (!decoder)
			break;

		if (tsc < decoder->tsc)
			break;

		session->decoders = decoder->next;
		decoder->next = NULL;

		errcode = pt_sb_print(session, decoder, stream, flags);
		if (errcode < 0) {
			decoder->next = session->removed;
			session->removed = decoder;
			continue;
		}

		errcode = pt_sb_fetch(session, decoder);
		if (errcode < 0) {
			decoder->next = session->removed;
			session->removed = decoder;
			continue;
		}

		errcode = pt_sb_add_decoder(&session->decoders, decoder);
		if (errcode < 0)
			return errcode;
	}

	return 0;
}

pt_sb_ctx_switch_notifier_t *
pt_sb_notify_switch(struct pt_sb_session *session,
		    pt_sb_ctx_switch_notifier_t *notifier, void *priv)
{
	pt_sb_ctx_switch_notifier_t *old;

	if (!session)
		return NULL;

	old = session->notify_switch_to;

	session->notify_switch_to = notifier;
	session->priv_switch_to = priv;

	return old;
}

pt_sb_error_notifier_t *
pt_sb_notify_error(struct pt_sb_session *session,
		   pt_sb_error_notifier_t *notifier, void *priv)
{
	pt_sb_error_notifier_t *old;

	if (!session)
		return NULL;

	old = session->notify_error;

	session->notify_error = notifier;
	session->priv_error = priv;

	return old;
}

int pt_sb_error(const struct pt_sb_session *session, int errcode,
		const char *filename, uint64_t offset)
{
	pt_sb_error_notifier_t *notifier;

	if (!session)
		return -pte_internal;

	notifier = session->notify_error;
	if (!notifier)
		return 0;

	return notifier(errcode, filename, offset, session->priv_error);
}

const char *pt_sb_errstr(enum pt_sb_error_code errcode)
{
	switch (errcode) {
	case ptse_ok:
		return "OK";

	case ptse_lost:
		return "sideband lost";

	case ptse_trace_lost:
		return "trace lost";

	case ptse_section_lost:
		return "image section lost";
	}

	return "bad errcode";
}
