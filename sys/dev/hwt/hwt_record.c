/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2025 Ruslan Bukin <br@bsdpad.com>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/hwt.h>
#include <sys/linker.h>
#include <sys/pmckern.h> /* linker_hwpmc_list_objects */

#include <vm/vm.h>
#include <vm/uma.h>

#include <dev/hwt/hwt_hook.h>
#include <dev/hwt/hwt_context.h>
#include <dev/hwt/hwt_contexthash.h>
#include <dev/hwt/hwt_config.h>
#include <dev/hwt/hwt_thread.h>
#include <dev/hwt/hwt_record.h>

#define	HWT_RECORD_DEBUG
#undef	HWT_RECORD_DEBUG

#ifdef	HWT_RECORD_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

static MALLOC_DEFINE(M_HWT_RECORD, "hwt_record", "Hardware Trace");
static uma_zone_t record_zone = NULL;

static struct hwt_record_entry *
hwt_record_clone(struct hwt_record_entry *ent, int flags)
{
	struct hwt_record_entry *entry;

	entry = uma_zalloc(record_zone, flags);
	if (entry == NULL)
		return (NULL);
	memcpy(entry, ent, sizeof(struct hwt_record_entry));
	switch (ent->record_type) {
	case HWT_RECORD_MMAP:
	case HWT_RECORD_EXECUTABLE:
	case HWT_RECORD_KERNEL:
		entry->fullpath = strdup(ent->fullpath, M_HWT_RECORD);
		break;
	default:
		break;
	}

	return (entry);
}

static void
hwt_record_to_user(struct hwt_record_entry *ent,
    struct hwt_record_user_entry *usr)
{
	usr->record_type = ent->record_type;
	switch (ent->record_type) {
	case HWT_RECORD_MMAP:
	case HWT_RECORD_EXECUTABLE:
	case HWT_RECORD_KERNEL:
		usr->addr = ent->addr;
		usr->baseaddr = ent->baseaddr;
		strncpy(usr->fullpath, ent->fullpath, MAXPATHLEN);
		break;
	case HWT_RECORD_BUFFER:
		usr->buf_id = ent->buf_id;
		usr->curpage = ent->curpage;
		usr->offset = ent->offset;
		break;
	case HWT_RECORD_THREAD_CREATE:
	case HWT_RECORD_THREAD_SET_NAME:
		usr->thread_id = ent->thread_id;
		break;
	default:
		break;
	}
}

void
hwt_record_load(void)
{
	record_zone = uma_zcreate("HWT records",
	    sizeof(struct hwt_record_entry), NULL, NULL, NULL, NULL, 0, 0);
}

void
hwt_record_unload(void)
{
	uma_zdestroy(record_zone);
}

void
hwt_record_ctx(struct hwt_context *ctx, struct hwt_record_entry *ent, int flags)
{
	struct hwt_record_entry *entry;

	KASSERT(ent != NULL, ("ent is NULL"));
	entry = hwt_record_clone(ent, flags);
	if (entry == NULL) {
		/* XXX: Not sure what to do here other than logging an error. */
		return;
	}

	HWT_CTX_LOCK(ctx);
	TAILQ_INSERT_TAIL(&ctx->records, entry, next);
	HWT_CTX_UNLOCK(ctx);
	hwt_record_wakeup(ctx);
}

void
hwt_record_td(struct thread *td, struct hwt_record_entry *ent, int flags)
{
	struct hwt_record_entry *entry;
	struct hwt_context *ctx;
	struct proc *p;

	p = td->td_proc;

	KASSERT(ent != NULL, ("ent is NULL"));
	entry = hwt_record_clone(ent, flags);
	if (entry == NULL) {
		/* XXX: Not sure what to do here other than logging an error. */
		return;
	}
	ctx = hwt_contexthash_lookup(p);
	if (ctx == NULL) {
		hwt_record_entry_free(entry);
		return;
	}
	HWT_CTX_LOCK(ctx);
	TAILQ_INSERT_TAIL(&ctx->records, entry, next);
	HWT_CTX_UNLOCK(ctx);
	hwt_record_wakeup(ctx);

	hwt_ctx_put(ctx);
}

struct hwt_record_entry *
hwt_record_entry_alloc(void)
{
	return (uma_zalloc(record_zone, M_WAITOK | M_ZERO));
}

void
hwt_record_entry_free(struct hwt_record_entry *entry)
{

	switch (entry->record_type) {
	case HWT_RECORD_MMAP:
	case HWT_RECORD_EXECUTABLE:
	case HWT_RECORD_KERNEL:
		free(entry->fullpath, M_HWT_RECORD);
		break;
	default:
		break;
	}

	uma_zfree(record_zone, entry);
}

static int
hwt_record_grab(struct hwt_context *ctx,
    struct hwt_record_user_entry *user_entry, int nitems_req, int wait)
{
	struct hwt_record_entry *entry;
	int i;

	if (wait) {
		mtx_lock(&ctx->rec_mtx);
		if (TAILQ_FIRST(&ctx->records) == NULL) {
			/* Wait until we have new records. */
			msleep(ctx, &ctx->rec_mtx, PCATCH, "recsnd", 0);
		}
		mtx_unlock(&ctx->rec_mtx);
	}

	for (i = 0; i < nitems_req; i++) {
		HWT_CTX_LOCK(ctx);
		entry = TAILQ_FIRST(&ctx->records);
		if (entry)
			TAILQ_REMOVE_HEAD(&ctx->records, next);
		HWT_CTX_UNLOCK(ctx);

		if (entry == NULL)
			break;
		hwt_record_to_user(entry, &user_entry[i]);
		hwt_record_entry_free(entry);
	}

	return (i);
}

void
hwt_record_free_all(struct hwt_context *ctx)
{
	struct hwt_record_entry *entry;

	while (1) {
		HWT_CTX_LOCK(ctx);
		entry = TAILQ_FIRST(&ctx->records);
		if (entry)
			TAILQ_REMOVE_HEAD(&ctx->records, next);
		HWT_CTX_UNLOCK(ctx);

		if (entry == NULL)
			break;

		hwt_record_entry_free(entry);
	}
}

int
hwt_record_send(struct hwt_context *ctx, struct hwt_record_get *record_get)
{
	struct hwt_record_user_entry *user_entry;
	int nitems_req;
	int error;
	int i;

	nitems_req = 0;

	error = copyin(record_get->nentries, &nitems_req, sizeof(int));
	if (error)
		return (error);

	if (nitems_req < 1 || nitems_req > 1024)
		return (ENXIO);

	user_entry = malloc(sizeof(struct hwt_record_user_entry) * nitems_req,
	    M_HWT_RECORD, M_WAITOK | M_ZERO);

	i = hwt_record_grab(ctx, user_entry, nitems_req, record_get->wait);
	if (i > 0)
		error = copyout(user_entry, record_get->records,
		    sizeof(struct hwt_record_user_entry) * i);

	if (error == 0)
		error = copyout(&i, record_get->nentries, sizeof(int));

	free(user_entry, M_HWT_RECORD);

	return (error);
}

void
hwt_record_kernel_objects(struct hwt_context *ctx)
{
	struct hwt_record_entry *entry;
	struct pmckern_map_in *kobase;
	int i;

	kobase = linker_hwpmc_list_objects();
	for (i = 0; kobase[i].pm_file != NULL; i++) {
		entry = hwt_record_entry_alloc();
		entry->record_type = HWT_RECORD_KERNEL;
		entry->fullpath = strdup(kobase[i].pm_file, M_HWT_RECORD);
		entry->addr = kobase[i].pm_address;

		HWT_CTX_LOCK(ctx);
		TAILQ_INSERT_HEAD(&ctx->records, entry, next);
		HWT_CTX_UNLOCK(ctx);
	}
	free(kobase, M_LINKER);
}

void
hwt_record_wakeup(struct hwt_context *ctx)
{
	wakeup(ctx);
}
