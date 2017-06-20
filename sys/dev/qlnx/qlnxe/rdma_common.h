/*
 * Copyright (c) 2017-2018 Cavium, Inc. 
 * All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef __RDMA_COMMON__
#define __RDMA_COMMON__ 
/************************/
/* RDMA FW CONSTANTS */
/************************/

#define RDMA_RESERVED_LKEY			(0)			//Reserved lkey
#define RDMA_RING_PAGE_SIZE			(0x1000)	//4KB pages

#define	RDMA_MAX_SGE_PER_SQ_WQE		(4)		//max number of SGEs in a single request
#define	RDMA_MAX_SGE_PER_RQ_WQE		(4)		//max number of SGEs in a single request

#define RDMA_MAX_DATA_SIZE_IN_WQE	(0x80000000)	//max size of data in single request

#define RDMA_REQ_RD_ATOMIC_ELM_SIZE		(0x50)
#define RDMA_RESP_RD_ATOMIC_ELM_SIZE	(0x20)

#define RDMA_MAX_CQS				(64*1024)
#define RDMA_MAX_TIDS				(128*1024-1)
#define RDMA_MAX_PDS				(64*1024)

#define RDMA_NUM_STATISTIC_COUNTERS			MAX_NUM_VPORTS
#define RDMA_NUM_STATISTIC_COUNTERS_K2			MAX_NUM_VPORTS_K2
#define RDMA_NUM_STATISTIC_COUNTERS_BB			MAX_NUM_VPORTS_BB

#define RDMA_TASK_TYPE (PROTOCOLID_ROCE)


struct rdma_srq_id
{
	__le16 srq_idx /* SRQ index */;
	__le16 opaque_fid;
};


struct rdma_srq_producers
{
	__le32 sge_prod /* Current produced sge in SRQ */;
	__le32 wqe_prod /* Current produced WQE to SRQ */;
};

#endif /* __RDMA_COMMON__ */
