/*
 * Copyright (c) 2026 Rick Macklem
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_RPC_CLNTRDMA_H_
#define	_RPC_CLNTRDMA_H_

#ifdef _KERNEL
#define	RPCRDMA_IO_NUMBUFS	64	/* Max. # of concurrent RPCs. */

struct rpcrdma_reduce_pg {
	uint32_t	xid;
	uint32_t	pos;
	uint32_t	len;
	uint32_t	npg;
	uint32_t	into_mem;
	vm_page_t	pg[];
};

/* Structure for a connection. */
struct rpcrdma_xprt {
	struct mtx	mtx;
	uint32_t	credits;
	uint32_t	maxrpc;
	uint32_t	maxbck;
	uint32_t	maxio;
	uint32_t	maxsge;
	struct mbuf	*reply_small[RPCRDMA_IO_NUMBUFS];
	struct mbuf	*reply_large[RPCRDMA_IO_NUMBUFS];
	void		*ep;
};

#define	RPCRDMA_MAX_SEGMENTS	16	/* Limit from RFC8267. */
#define	RPCRDMA_MAX_INLINE	1024	/* Limit from RFC8267. */

#define	RPCRDMA_MAX_SGE		(16 + 2)
struct rpcrdma_chunk {
	int			ind;
	unsigned int		first_off;
	uint32_t		last_len;
	uint32_t		num_segment;
	uint32_t		into_mem;
	uint32_t		sge_cnt[RPCRDMA_MAX_SEGMENTS];
	uint32_t		handle[RPCRDMA_MAX_SEGMENTS];
	uint32_t		length[RPCRDMA_MAX_SEGMENTS];
	uint64_t		offset[RPCRDMA_MAX_SEGMENTS];
	struct mbuf		*mextpg;
};

#define	RPCRDMA_DEBUG(level, ...)	do {				\
		if (rpcrdma_debuglevel >= (level))			\
			printf(__VA_ARGS__);				\
	} while (0)

extern int rpcrdma_debuglevel;

/*
 * Upcall to krpc or similar.  Passes mbuf chain and context up.
 */
typedef void (*xprt_rdma_upcall)(struct rpcrdma_xprt *xp, struct mbuf *mp);

void xprt_rdma_init(struct rpcrdma_xprt *xp, xprt_rdma_upcall rdma_upcall);

int xprt_rdma_check_route(struct vnet *net, struct sockaddr *saddr,
    uint32_t cbslots);

int xprt_rdma_connect(struct vnet *net, struct sockaddr *saddr,
    struct rpcrdma_xprt *xp, size_t buflen, uint32_t cbslots);

void xprt_rdma_disconnect(struct rpcrdma_xprt *xp);

int xprt_rdma_send(struct rpcrdma_xprt *xp, struct mbuf *mreq, int ind);

void xprt_rdma_release_send(struct rpcrdma_xprt *xp, int ind,
    struct rpcrdma_chunk *extern_chp, struct rpcrdma_chunk *extern_reply_chp,
    struct rpcrdma_chunk *extern_request_chp);

int xprt_rdma_recv(struct rpcrdma_xprt *xp);

void xprt_rdma_release_ep(struct rpcrdma_xprt *xp);

struct rpcrdma_chunk *xprt_rdma_create_chunk(struct rpcrdma_xprt *xp,
    uint32_t num_pg, struct rpcrdma_reduce_pg *rb, struct mbuf *mextpg,
    bool into_mem, int ind);

int xprt_rdma_disconnected(struct rpcrdma_xprt *xp);

int xprt_rdma_acquire_buf(struct rpcrdma_xprt *xp, int start, int end);

int xprt_rdma_rekey_chunk(struct rpcrdma_xprt *xp, struct rpcrdma_chunk *chp);

struct mbuf *rpc_reduce_pg(int len, int pos, bool to_mem);

void rpc_free_rdma_reduction(struct mbuf *mr);

int rpc_copy_uio_pages(struct mbuf *mr, struct uio *uiop, int len,
    bool from_pages);

void rpc_copy_mbuf_to_rb(struct mbuf *m, struct rpcrdma_reduce_pg *rb);
#endif	/* _KERNEL */

#endif	/* _RPC_CLNTRDMA_H_ */
