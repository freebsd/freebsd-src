/*-
 * Copyright (c) 2019 Mellanox Technologies. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _MLX5_TLS_H_
#define	_MLX5_TLS_H_

#define	MLX5E_TLS_TAG_LOCK(tag)		mtx_lock(&(tag)->mtx)
#define	MLX5E_TLS_TAG_UNLOCK(tag)	mtx_unlock(&(tag)->mtx)

#define	MLX5E_TLS_STAT_INC(tag, field, num) \
	counter_u64_add((tag)->tls->stats.field, num)

enum {
      MLX5E_TLS_LOOP = 0,
      MLX5E_TLS_FAILURE = 1,
      MLX5E_TLS_DEFERRED = 2,
      MLX5E_TLS_CONTINUE = 3,
};

struct mlx5e_tls_tag {
	struct m_snd_tag tag;
	volatile s32 refs;	/* number of pending mbufs */
	uint32_t tisn;		/* HW TIS context number */
	uint32_t dek_index;	/* HW TLS context number */
	struct mlx5e_tls *tls;
	struct m_snd_tag *rl_tag;
	struct mtx mtx;
	uint32_t expected_seq; /* expected TCP sequence number */
	uint32_t state;	/* see MLX5E_TLS_ST_XXX */
#define	MLX5E_TLS_ST_INIT 0
#define	MLX5E_TLS_ST_SETUP 1
#define	MLX5E_TLS_ST_TXRDY 2
#define	MLX5E_TLS_ST_FREED 3
	struct work_struct work;

	uint32_t dek_index_ok:1;

	/* parameters needed */
	uint8_t crypto_params[128] __aligned(4);
} __aligned(MLX5E_CACHELINE_SIZE);

#define	MLX5E_TLS_STATS(m)					\
  m(+1, u64, tx_packets, "tx_packets", "Transmitted packets")	\
  m(+1, u64, tx_bytes, "tx_bytes", "Transmitted bytes")		\
  m(+1, u64, tx_packets_ooo, "tx_packets_ooo", "Transmitted packets out of order") \
  m(+1, u64, tx_bytes_ooo, "tx_bytes_ooo", "Transmitted bytes out of order") \
  m(+1, u64, tx_error, "tx_error", "Transmitted packets with error")

#define	MLX5E_TLS_STATS_NUM (0 MLX5E_TLS_STATS(MLX5E_STATS_COUNT))

struct mlx5e_tls_stats {
	struct	sysctl_ctx_list ctx;
	counter_u64_t	arg[0];
	MLX5E_TLS_STATS(MLX5E_STATS_COUNTER)
};

struct mlx5e_tls {
	struct sysctl_ctx_list ctx;
	struct mlx5e_tls_stats stats;
	struct workqueue_struct *wq;
	uma_zone_t zone;
	uint32_t max_resources;		/* max number of resources */
	volatile uint32_t num_resources;	/* current number of resources */
	int init;			/* set when ready */
	char zname[32];
};

int mlx5e_tls_init(struct mlx5e_priv *);
void mlx5e_tls_cleanup(struct mlx5e_priv *);
int mlx5e_sq_tls_xmit(struct mlx5e_sq *, struct mlx5e_xmit_args *, struct mbuf **);

if_snd_tag_alloc_t mlx5e_tls_snd_tag_alloc;
if_snd_tag_modify_t mlx5e_tls_snd_tag_modify;
if_snd_tag_query_t mlx5e_tls_snd_tag_query;
if_snd_tag_free_t mlx5e_tls_snd_tag_free;

#endif					/* _MLX5_TLS_H_ */
