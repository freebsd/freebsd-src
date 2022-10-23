/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#ifndef	_FBSDRUN_H_
#define	_FBSDRUN_H_

#define	VMEXIT_CONTINUE		(0)
#define	VMEXIT_ABORT		(-1)

extern int guest_ncpus;
extern uint16_t cores, sockets, threads;

struct vmctx;
struct vm_exit;

void *paddr_guest2host(struct vmctx *ctx, uintptr_t addr, size_t len);
#ifdef BHYVE_SNAPSHOT
uintptr_t paddr_host2guest(struct vmctx *ctx, void *addr);
#endif

void fbsdrun_set_capabilities(struct vmctx *ctx, int cpu);
int  fbsdrun_virtio_msix(void);

int vmexit_task_switch(struct vmctx *, struct vm_exit *, int *vcpu);

#endif
