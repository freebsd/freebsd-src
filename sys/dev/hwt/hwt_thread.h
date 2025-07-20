/*-
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

#ifndef _DEV_HWT_HWT_THREAD_H_
#define _DEV_HWT_HWT_THREAD_H_

struct hwt_record_entry;

struct hwt_thread {
	struct hwt_vm			*vm;
	struct hwt_context		*ctx;
	struct hwt_backend		*backend;
	struct thread			*td;
	TAILQ_ENTRY(hwt_thread)		next;
	int				thread_id;
	int				state;
#define	HWT_THREAD_STATE_EXITED		(1 << 0)
	struct mtx			mtx;
	u_int				refcnt;
	int				cpu_id; /* last cpu_id */
	void				*private; /* backend-specific private data */
};

/* Thread allocation. */
int hwt_thread_alloc(struct hwt_thread **thr0, char *path, size_t bufsize,
    int kva_req);
void hwt_thread_free(struct hwt_thread *thr);

/* Thread list mgt. */
void hwt_thread_insert(struct hwt_context *ctx, struct hwt_thread *thr, struct hwt_record_entry *entry);
struct hwt_thread * hwt_thread_first(struct hwt_context *ctx);
struct hwt_thread * hwt_thread_lookup(struct hwt_context *ctx,
    struct thread *td);

#define	HWT_THR_LOCK(thr)		mtx_lock(&(thr)->mtx)
#define	HWT_THR_UNLOCK(thr)		mtx_unlock(&(thr)->mtx)
#define	HWT_THR_ASSERT_LOCKED(thr)	mtx_assert(&(thr)->mtx, MA_OWNED)

#endif /* !_DEV_HWT_HWT_THREAD_H_ */
