/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005, 2006, 2007 Cisco Systems.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MLX4_H
#define MLX4_H

#include <stddef.h>

#include <infiniband/driver.h>
#include <infiniband/arch.h>

#ifdef HAVE_VALGRIND_MEMCHECK_H

#  include <valgrind/memcheck.h>

#  if !defined(VALGRIND_MAKE_MEM_DEFINED) || !defined(VALGRIND_MAKE_MEM_UNDEFINED)
#    warning "Valgrind support requested, but VALGRIND_MAKE_MEM_(UN)DEFINED not available"
#  endif

#endif /* HAVE_VALGRIND_MEMCHECK_H */

#ifndef VALGRIND_MAKE_MEM_DEFINED
#  define VALGRIND_MAKE_MEM_DEFINED(addr,len)
#endif

#ifndef VALGRIND_MAKE_MEM_UNDEFINED
#  define VALGRIND_MAKE_MEM_UNDEFINED(addr,len)
#endif

#ifndef rmb
#  define rmb() mb()
#endif

#ifndef wmb
#  define wmb() mb()
#endif

#ifndef wc_wmb

#if defined(__i386__)
#define wc_wmb() asm volatile("lock; addl $0,0(%%esp) " ::: "memory")
#elif defined(__x86_64__)
#define wc_wmb() asm volatile("sfence" ::: "memory")
#elif defined(__ia64__)
#define wc_wmb() asm volatile("fwb" ::: "memory")
#else
#define wc_wmb() wmb()
#endif

#endif

#ifndef HAVE_IBV_MORE_OPS
#undef HAVE_IBV_XRC_OPS
#undef HAVE_IBV_CREATE_QP_EXP
#endif

#define HIDDEN		__attribute__((visibility ("hidden")))

#define PFX		"mlx4: "

#ifndef max
#define max(a,b) \
	({ typeof (a) _a = (a); \
	   typeof (b) _b = (b); \
	   _a > _b ? _a : _b; })
#endif

#ifndef min
#define min(a,b) \
	({ typeof (a) _a = (a); \
	   typeof (b) _b = (b); \
	   _a < _b ? _a : _b; })
#endif

enum {
	MLX4_CQ_ENTRY_SIZE		= 0x20
};

enum {
	MLX4_STAT_RATE_OFFSET		= 5
};

enum {
	MLX4_QP_TABLE_BITS		= 8,
	MLX4_QP_TABLE_SIZE		= 1 << MLX4_QP_TABLE_BITS,
	MLX4_QP_TABLE_MASK		= MLX4_QP_TABLE_SIZE - 1
};

enum {
	MLX4_XRC_SRQ_TABLE_BITS		= 8,
	MLX4_XRC_SRQ_TABLE_SIZE		= 1 << MLX4_XRC_SRQ_TABLE_BITS,
	MLX4_XRC_SRQ_TABLE_MASK		= MLX4_XRC_SRQ_TABLE_SIZE - 1
};

enum {
	MLX4_XRC_QPN_BIT		= (1 << 23)
};

enum mlx4_db_type {
	MLX4_DB_TYPE_CQ,
	MLX4_DB_TYPE_RQ,
	MLX4_NUM_DB_TYPE
};

enum {
	MLX4_OPCODE_NOP			= 0x00,
	MLX4_OPCODE_SEND_INVAL		= 0x01,
	MLX4_OPCODE_RDMA_WRITE		= 0x08,
	MLX4_OPCODE_RDMA_WRITE_IMM	= 0x09,
	MLX4_OPCODE_SEND		= 0x0a,
	MLX4_OPCODE_SEND_IMM		= 0x0b,
	MLX4_OPCODE_LSO			= 0x0e,
	MLX4_OPCODE_RDMA_READ		= 0x10,
	MLX4_OPCODE_ATOMIC_CS		= 0x11,
	MLX4_OPCODE_ATOMIC_FA		= 0x12,
	MLX4_OPCODE_ATOMIC_MASK_CS	= 0x14,
	MLX4_OPCODE_ATOMIC_MASK_FA	= 0x15,
	MLX4_OPCODE_BIND_MW		= 0x18,
	MLX4_OPCODE_FMR			= 0x19,
	MLX4_OPCODE_LOCAL_INVAL		= 0x1b,
	MLX4_OPCODE_CONFIG_CMD		= 0x1f,

	MLX4_RECV_OPCODE_RDMA_WRITE_IMM	= 0x00,
	MLX4_RECV_OPCODE_SEND		= 0x01,
	MLX4_RECV_OPCODE_SEND_IMM	= 0x02,
	MLX4_RECV_OPCODE_SEND_INVAL	= 0x03,

	MLX4_CQE_OPCODE_ERROR		= 0x1e,
	MLX4_CQE_OPCODE_RESIZE		= 0x16,
};

enum {
	MLX4_MAX_WQE_SIZE = 1008
};

struct mlx4_device {
	struct ibv_device		ibv_dev;
	int				page_size;
};

struct mlx4_db_page;

struct mlx4_context {
	struct ibv_context		ibv_ctx;

	void			       *uar;
	pthread_spinlock_t		uar_lock;

	void			       *bf_page;
	int				bf_buf_size;
	int				bf_offset;
	pthread_spinlock_t		bf_lock;

	struct {
		struct mlx4_qp	      **table;
		int			refcnt;
	}				qp_table[MLX4_QP_TABLE_SIZE];
	pthread_mutex_t			qp_table_mutex;
	int				num_qps;
	int				qp_table_shift;
	int				qp_table_mask;
	int				max_qp_wr;
	int				max_sge;
	int				max_cqe;

	struct {
		struct mlx4_srq       **table;
		int			refcnt;
	}				xrc_srq_table[MLX4_XRC_SRQ_TABLE_SIZE];
	pthread_mutex_t			xrc_srq_table_mutex;
	int				num_xrc_srqs;
	int				xrc_srq_table_shift;
	int				xrc_srq_table_mask;

	struct mlx4_db_page	       *db_list[MLX4_NUM_DB_TYPE];
	pthread_mutex_t			db_list_mutex;
};

struct mlx4_buf {
	void			       *buf;
	size_t				length;
};

struct mlx4_pd {
	struct ibv_pd			ibv_pd;
	uint32_t			pdn;
};

struct mlx4_cq {
	struct ibv_cq			ibv_cq;
	struct mlx4_buf			buf;
	struct mlx4_buf			resize_buf;
	pthread_spinlock_t		lock;
	uint32_t			cqn;
	uint32_t			cons_index;
	uint32_t		       *set_ci_db;
	uint32_t		       *arm_db;
	int				arm_sn;
};

struct mlx4_srq {
	struct ibv_srq			ibv_srq;
	struct mlx4_buf			buf;
	pthread_spinlock_t		lock;
	uint64_t		       *wrid;
	uint32_t			srqn;
	int				max;
	int				max_gs;
	int				wqe_shift;
	int				head;
	int				tail;
	uint32_t		       *db;
	uint16_t			counter;
};

struct mlx4_wq {
	uint64_t		       *wrid;
	pthread_spinlock_t		lock;
	int				wqe_cnt;
	int				max_post;
	unsigned			head;
	unsigned			tail;
	int				max_gs;
	int				wqe_shift;
	int				offset;
};

struct mlx4_qp {
	struct ibv_qp			ibv_qp;
	struct mlx4_buf			buf;
	int				max_inline_data;
	int				buf_size;

	uint32_t			doorbell_qpn;
	uint32_t			sq_signal_bits;
	int				sq_spare_wqes;
	struct mlx4_wq			sq;

	uint32_t		       *db;
	struct mlx4_wq			rq;
};

struct mlx4_av {
	uint32_t			port_pd;
	uint8_t				reserved1;
	uint8_t				g_slid;
	uint16_t			dlid;
	uint8_t				reserved2;
	uint8_t				gid_index;
	uint8_t				stat_rate;
	uint8_t				hop_limit;
	uint32_t			sl_tclass_flowlabel;
	uint8_t				dgid[16];
	uint8_t				mac[8];
};

struct mlx4_ah {
	struct ibv_ah			ibv_ah;
	struct mlx4_av			av;
	uint16_t			vlan;
	uint8_t				mac[6];
	uint8_t				tagged;
};

struct mlx4_xrc_domain {
	struct ibv_xrc_domain		ibv_xrcd;
	uint32_t			xrcdn;
};

static inline unsigned long align(unsigned long val, unsigned long align)
{
	return (val + align - 1) & ~(align - 1);
}

#define to_mxxx(xxx, type)						\
	((struct mlx4_##type *)					\
	 ((void *) ib##xxx - offsetof(struct mlx4_##type, ibv_##xxx)))

static inline struct mlx4_device *to_mdev(struct ibv_device *ibdev)
{
	return to_mxxx(dev, device);
}

static inline struct mlx4_context *to_mctx(struct ibv_context *ibctx)
{
	return to_mxxx(ctx, context);
}

static inline struct mlx4_pd *to_mpd(struct ibv_pd *ibpd)
{
	return to_mxxx(pd, pd);
}

static inline struct mlx4_cq *to_mcq(struct ibv_cq *ibcq)
{
	return to_mxxx(cq, cq);
}

static inline struct mlx4_srq *to_msrq(struct ibv_srq *ibsrq)
{
	return to_mxxx(srq, srq);
}

static inline struct mlx4_qp *to_mqp(struct ibv_qp *ibqp)
{
	return to_mxxx(qp, qp);
}

static inline struct mlx4_ah *to_mah(struct ibv_ah *ibah)
{
	return to_mxxx(ah, ah);
}

#ifdef HAVE_IBV_XRC_OPS
static inline struct mlx4_xrc_domain *to_mxrcd(struct ibv_xrc_domain *ibxrcd)
{
	return to_mxxx(xrcd, xrc_domain);
}
#endif

int mlx4_alloc_buf(struct mlx4_buf *buf, size_t size, int page_size);
void mlx4_free_buf(struct mlx4_buf *buf);

uint32_t *mlx4_alloc_db(struct mlx4_context *context, enum mlx4_db_type type);
void mlx4_free_db(struct mlx4_context *context, enum mlx4_db_type type, uint32_t *db);

int mlx4_query_device(struct ibv_context *context,
		       struct ibv_device_attr *attr);
int mlx4_query_port(struct ibv_context *context, uint8_t port,
		     struct ibv_port_attr *attr);

struct ibv_pd *mlx4_alloc_pd(struct ibv_context *context);
int mlx4_free_pd(struct ibv_pd *pd);

struct ibv_mr *mlx4_reg_mr(struct ibv_pd *pd, void *addr,
			    size_t length, enum ibv_access_flags access);
int mlx4_dereg_mr(struct ibv_mr *mr);

struct ibv_cq *mlx4_create_cq(struct ibv_context *context, int cqe,
			       struct ibv_comp_channel *channel,
			       int comp_vector);
int mlx4_alloc_cq_buf(struct mlx4_device *dev, struct mlx4_buf *buf, int nent);
int mlx4_resize_cq(struct ibv_cq *cq, int cqe);
int mlx4_destroy_cq(struct ibv_cq *cq);
int mlx4_poll_cq(struct ibv_cq *cq, int ne, struct ibv_wc *wc);
int mlx4_arm_cq(struct ibv_cq *cq, int solicited);
void mlx4_cq_event(struct ibv_cq *cq);
void __mlx4_cq_clean(struct mlx4_cq *cq, uint32_t qpn, struct mlx4_srq *srq);
void mlx4_cq_clean(struct mlx4_cq *cq, uint32_t qpn, struct mlx4_srq *srq);
int mlx4_get_outstanding_cqes(struct mlx4_cq *cq);
void mlx4_cq_resize_copy_cqes(struct mlx4_cq *cq, void *buf, int new_cqe);

struct ibv_srq *mlx4_create_srq(struct ibv_pd *pd,
				 struct ibv_srq_init_attr *attr);
int mlx4_modify_srq(struct ibv_srq *srq,
		     struct ibv_srq_attr *attr,
		     enum ibv_srq_attr_mask mask);
int mlx4_query_srq(struct ibv_srq *srq,
			   struct ibv_srq_attr *attr);
int mlx4_destroy_srq(struct ibv_srq *srq);
int mlx4_alloc_srq_buf(struct ibv_pd *pd, struct ibv_srq_attr *attr,
			struct mlx4_srq *srq);
void mlx4_free_srq_wqe(struct mlx4_srq *srq, int ind);
int mlx4_post_srq_recv(struct ibv_srq *ibsrq,
		       struct ibv_recv_wr *wr,
		       struct ibv_recv_wr **bad_wr);
struct mlx4_srq *mlx4_find_xrc_srq(struct mlx4_context *ctx, uint32_t xrc_srqn);
int mlx4_store_xrc_srq(struct mlx4_context *ctx, uint32_t xrc_srqn,
		       struct mlx4_srq *srq);
void mlx4_clear_xrc_srq(struct mlx4_context *ctx, uint32_t xrc_srqn);

struct ibv_qp *mlx4_create_qp(struct ibv_pd *pd, struct ibv_qp_init_attr *attr);
int mlx4_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		   enum ibv_qp_attr_mask attr_mask,
		   struct ibv_qp_init_attr *init_attr);
int mlx4_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		    enum ibv_qp_attr_mask attr_mask);
int mlx4_destroy_qp(struct ibv_qp *qp);
void mlx4_init_qp_indices(struct mlx4_qp *qp);
void mlx4_qp_init_sq_ownership(struct mlx4_qp *qp);
int mlx4_post_send(struct ibv_qp *ibqp, struct ibv_send_wr *wr,
			  struct ibv_send_wr **bad_wr);
int mlx4_post_recv(struct ibv_qp *ibqp, struct ibv_recv_wr *wr,
			  struct ibv_recv_wr **bad_wr);
void mlx4_calc_sq_wqe_size(struct ibv_qp_cap *cap, enum ibv_qp_type type,
			   struct mlx4_qp *qp);
int num_inline_segs(int data, enum ibv_qp_type type);
int mlx4_alloc_qp_buf(struct ibv_pd *pd, struct ibv_qp_cap *cap,
		       enum ibv_qp_type type, struct mlx4_qp *qp);
void mlx4_set_sq_sizes(struct mlx4_qp *qp, struct ibv_qp_cap *cap,
		       enum ibv_qp_type type);
struct mlx4_qp *mlx4_find_qp(struct mlx4_context *ctx, uint32_t qpn);
int mlx4_store_qp(struct mlx4_context *ctx, uint32_t qpn, struct mlx4_qp *qp);
void mlx4_clear_qp(struct mlx4_context *ctx, uint32_t qpn);
struct ibv_ah *mlx4_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr);
int mlx4_destroy_ah(struct ibv_ah *ah);
int mlx4_alloc_av(struct mlx4_pd *pd, struct ibv_ah_attr *attr,
		   struct mlx4_ah *ah);
void mlx4_free_av(struct mlx4_ah *ah);
#ifdef HAVE_IBV_XRC_OPS
struct ibv_srq *mlx4_create_xrc_srq(struct ibv_pd *pd,
				    struct ibv_xrc_domain *xrc_domain,
				    struct ibv_cq *xrc_cq,
				    struct ibv_srq_init_attr *attr);
struct ibv_xrc_domain *mlx4_open_xrc_domain(struct ibv_context *context,
					    int fd, int oflag);

int mlx4_close_xrc_domain(struct ibv_xrc_domain *d);
int mlx4_create_xrc_rcv_qp(struct ibv_qp_init_attr *init_attr,
			   uint32_t *xrc_qp_num);
int mlx4_modify_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			   uint32_t xrc_qp_num,
			   struct ibv_qp_attr *attr,
			   int attr_mask);
int mlx4_query_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			  uint32_t xrc_qp_num,
			  struct ibv_qp_attr *attr,
			  int attr_mask,
			  struct ibv_qp_init_attr *init_attr);
int mlx4_reg_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			uint32_t xrc_qp_num);
int mlx4_unreg_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			uint32_t xrc_qp_num);
#endif


#endif /* MLX4_H */
