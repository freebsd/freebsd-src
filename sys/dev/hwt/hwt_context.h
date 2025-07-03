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

#ifndef _DEV_HWT_HWT_CONTEXT_H_
#define _DEV_HWT_HWT_CONTEXT_H_

enum hwt_ctx_state {
	CTX_STATE_STOPPED,
	CTX_STATE_RUNNING,
};

struct hwt_context {
	TAILQ_HEAD(, hwt_record_entry)	records;

	LIST_ENTRY(hwt_context)		next_hch; /* Entry in contexthash. */
	LIST_ENTRY(hwt_context)		next_hwts; /* Entry in ho->hwts. */

	int				mode;
	int				ident;

	int				kqueue_fd;
	struct thread			*hwt_td;

	/* CPU mode. */
	cpuset_t			cpu_map;
	TAILQ_HEAD(, hwt_cpu)		cpus;

	/* Thread mode. */
	struct proc			*proc; /* Target proc. */
	pid_t				pid; /* Target pid. */
	TAILQ_HEAD(, hwt_thread)	threads;
	int				thread_counter;
	int				pause_on_mmap;

	size_t				bufsize; /* Trace bufsize for each vm.*/

	void				*config;
	size_t				config_size;
	int				config_version;

	struct hwt_owner		*hwt_owner;
	struct hwt_backend		*hwt_backend;

	struct mtx			mtx;
	struct mtx			rec_mtx;
	enum hwt_ctx_state		state;
	int				refcnt;
};

#define	HWT_CTX_LOCK(ctx)		mtx_lock_spin(&(ctx)->mtx)
#define	HWT_CTX_UNLOCK(ctx)		mtx_unlock_spin(&(ctx)->mtx)
#define	HWT_CTX_ASSERT_LOCKED(ctx)	mtx_assert(&(ctx)->mtx, MA_OWNED)

int hwt_ctx_alloc(struct hwt_context **ctx0);
void hwt_ctx_free(struct hwt_context *ctx);
void hwt_ctx_put(struct hwt_context *ctx);

void hwt_ctx_load(void);
void hwt_ctx_unload(void);

#endif /* !_DEV_HWT_HWT_CONTEXT_H_ */
