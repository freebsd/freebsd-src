/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __HYPERV_PRIV_H__
#define __HYPERV_PRIV_H__

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <sys/_iovec.h>

#include <dev/hyperv/vmbus/vmbus_chanvar.h>

struct vmbus_softc;

/*
 * Private, VM Bus functions
 */
struct sysctl_ctx_list;
struct sysctl_oid;

void	vmbus_br_sysctl_create(struct sysctl_ctx_list *ctx,
	    struct sysctl_oid *br_tree, hv_vmbus_ring_buffer_info *br,
	    const char *name);

int			hv_vmbus_ring_buffer_init(
				hv_vmbus_ring_buffer_info	*ring_info,
				void				*buffer,
				uint32_t			buffer_len);

void			hv_ring_buffer_cleanup(
				hv_vmbus_ring_buffer_info	*ring_info);

int			hv_ring_buffer_write(
				hv_vmbus_ring_buffer_info	*ring_info,
				const struct iovec		iov[],
				uint32_t			iovlen,
				boolean_t			*need_sig);

int			hv_ring_buffer_peek(
				hv_vmbus_ring_buffer_info	*ring_info,
				void				*buffer,
				uint32_t			buffer_len);

int			hv_ring_buffer_read(
				hv_vmbus_ring_buffer_info	*ring_info,
				void				*buffer,
				uint32_t			buffer_len,
				uint32_t			offset);

void			hv_ring_buffer_read_begin(
				hv_vmbus_ring_buffer_info	*ring_info);

uint32_t		hv_ring_buffer_read_end(
				hv_vmbus_ring_buffer_info	*ring_info);

#endif  /* __HYPERV_PRIV_H__ */
