/*-
 * Copyright (c) 2010 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by Lawrence Stewart,
 * made possible in part by a grant from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NETINET_HHOOKS_H_
#define _NETINET_HHOOKS_H_

#define	HHOOK_WAITOK	0x01
#define	HHOOK_NOWAIT	0x02

#define	HHOOK_TYPE_TCP		1

typedef void (*hhook_func_t)(void *udata, void *ctx_data, void *helper_dblock);

struct helper;
struct helper_dblock;
struct hhook_head;

int	register_hhook_head(int hhook_type, int hhook_id, int flags);
int	deregister_hhook_head(int hhook_type, int hhook_id);
int	register_hhook(int hhook_type, int hhook_id, struct helper *helper,
    hhook_func_t hook, void *udata, int flags);
int	deregister_hhook(int hhook_type, int hhook_id, hhook_func_t hook,
    void *udata, int flags);
void	run_hhooks(int hhook_type, int hhook_id, void *ctx_data,
    struct helper_dblock *dblocks, int n_dblocks);

#define	HHOOK_HEAD_LIST_LOCK() mtx_lock(&hhook_head_list_lock)
#define	HHOOK_HEAD_LIST_UNLOCK() mtx_unlock(&hhook_head_list_lock)
#define	HHOOK_HEAD_LIST_LOCK_ASSERT() mtx_assert(&hhook_head_list_lock, MA_OWNED)

#define	HHOOK_HEAD_LOCK_INIT(hh) rm_init(&(hh)->hh_lock, "hhook_head rm lock")
#define	HHOOK_HEAD_LOCK_DESTROY(hh) rm_destroy(&(hh)->hh_lock)
#define	HHOOK_HEAD_WLOCK(hh) rm_wlock(&(hh)->hh_lock)
#define	HHOOK_HEAD_WUNLOCK(hh) rm_wunlock(&(hh)->hh_lock)
#define	HHOOK_HEAD_RLOCK(hh,rmpt) rm_rlock(&(hh)->hh_lock, (rmpt))
#define	HHOOK_HEAD_RUNLOCK(hh,rmpt) rm_runlock(&(hh)->hh_lock, (rmpt))

#endif /* _NETINET_HHOOKS_H_ */

