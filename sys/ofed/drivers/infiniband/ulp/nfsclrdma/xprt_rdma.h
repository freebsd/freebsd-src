/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2014-2017 Oracle.  All rights reserved.
 * Copyright (c) 2003-2007 Network Appliance, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the BSD-type
 * license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *      Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *      Neither the name of the Network Appliance, Inc. nor the names of
 *      its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file is loosely based on net/sunrpc/xprtrdma/xprt_rdma.h
 * in Linux.
 */

#ifndef _RDMA_XPRT_RDMA_H
#define _RDMA_XPRT_RDMA_H

#ifdef _KERNEL
#define RDMA_RESOLVE_TIMEOUT	(5000)	/* 5 seconds */
#define RDMA_SEND_TIMEOUT	(30000)	/* 30 seconds */
#define RDMA_REG_TIMEOUT	(500)	/* 0.5 seconds */
#define RDMA_KEYINV_TIMEOUT	(5000)	/* 5.0 seconds */
#define RDMA_DISCONNECT_TIMEOUT	(5000)	/* 5 seconds */
#define RDMA_CONNECT_RETRY_MAX	(2)	/* retries if no listener backlog */

#define RPCRDMA_BIND_TO		(60U * HZ)
#define RPCRDMA_INIT_REEST_TO	(5U * HZ)
#define RPCRDMA_MAX_REEST_TO	(30U * HZ)
#define RPCRDMA_IDLE_DISC_TO	(5U * 60 * HZ)

/* send/receive buffer stuff. */

struct rpcrdma_buf {
	uint32_t	pgcnt;
	uint8_t		into_mem;
	struct iovec	iov[];
};

struct rpcrdma_regwr {
	struct ib_reg_wr	wr;
	struct ib_cqe		cqe;
	int			async_rc;
	struct completion	done;
};

struct _rpcrdma_chunk_priv {
	struct rpcrdma_chunk	ch;
	uint32_t		rbpages;
	struct scatterlist	sge[RPCRDMA_MAX_SEGMENTS][RPCRDMA_MAX_SGE];
};

#endif	/* _KERNEL */

#endif	/* _RDMA_XPRT_RDMA_H */
