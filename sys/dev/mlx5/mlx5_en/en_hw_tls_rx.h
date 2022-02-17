/*-
 * Copyright (c) 2021-2022 NVIDIA corporation & affiliates.
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

#ifndef _MLX5_TLS_RX_H_
#define	_MLX5_TLS_RX_H_

#include <linux/completion.h>

#define	MLX5E_TLS_RX_PROGRESS_BUFFER_SIZE 128

#define	MLX5E_TLS_RX_RESYNC_MAX 32	/* units */
#define	MLX5E_TLS_RX_NUM_MAX (1U << 11)	/* packets */

#define	MLX5E_TLS_RX_TAG_LOCK(tag)	mtx_lock(&(tag)->mtx)
#define	MLX5E_TLS_RX_TAG_UNLOCK(tag)	mtx_unlock(&(tag)->mtx)

#define	MLX5E_TLS_RX_STAT_INC(tag, field, num) \
	counter_u64_add((tag)->tls_rx->stats.field, num)

#if ((MLX5E_TLS_RX_RESYNC_MAX * MLX5E_TLS_RX_NUM_MAX) << 14) > (1U << 30)
#error "Please lower the limits of the TLS record length database."
#endif

enum {
	MLX5E_TLS_RX_PROGRESS_PARAMS_AUTH_STATE_NO_OFFLOAD = 0,
	MLX5E_TLS_RX_PROGRESS_PARAMS_AUTH_STATE_OFFLOAD = 1,
	MLX5E_TLS_RX_PROGRESS_PARAMS_AUTH_STATE_AUTHENTICATION = 2,
};

enum {
	MLX5E_TLS_RX_PROGRESS_PARAMS_RECORD_TRACKER_STATE_START = 0,
	MLX5E_TLS_RX_PROGRESS_PARAMS_RECORD_TRACKER_STATE_TRACKING = 1,
	MLX5E_TLS_RX_PROGRESS_PARAMS_RECORD_TRACKER_STATE_SEARCHING = 2,
};

struct mlx5e_tls_rx;
struct mlx5e_tls_rx_tag {
	struct m_snd_tag tag;
	uint32_t tirn;		/* HW TIR context number */
	uint32_t dek_index;	/* HW TLS context number */
	struct mlx5e_tls_rx *tls_rx; /* parent pointer */
	struct mlx5_flow_rule *flow_rule;
	struct mtx mtx;
	struct completion progress_complete;
	uint32_t state;	/* see MLX5E_TLS_RX_ST_XXX */
#define	MLX5E_TLS_RX_ST_INIT 0
#define	MLX5E_TLS_RX_ST_SETUP 1
#define	MLX5E_TLS_RX_ST_READY 2
#define	MLX5E_TLS_RX_ST_RELEASE 3
#define	MLX5E_TLS_RX_ST_FREED 4

	/*
	 * The following fields are used to store the TCP starting
	 * point of TLS records in the past. When TLS records of same
	 * length are back to back the tcp_resync_num[] is incremented
	 * instead of creating new entries. This way up to
	 * "MLX5E_TLS_RX_RESYNC_MAX" * "MLX5E_TLS_RX_NUM_MAX" * 16
	 * KBytes, around 1GByte worth of TCP data, may be remembered
	 * in the good case. The amount of history should not exceed
	 * 2GBytes of TCP data, because then the TCP sequence numbers
	 * may wrap around.
	 *
	 * This information is used to tell if a given TCP sequence
	 * number is a valid TLS record or not.
	 */
	uint64_t rcd_resync_start;	/* starting TLS record number */
	uint32_t tcp_resync_start;	/* starting TCP sequence number */
	uint32_t tcp_resync_next;	/* next expected TCP sequence number */
	uint32_t tcp_resync_len[MLX5E_TLS_RX_RESYNC_MAX];
	uint32_t tcp_resync_num[MLX5E_TLS_RX_RESYNC_MAX];
	uint16_t tcp_resync_pc;		/* producer counter for arrays above */
	uint16_t tcp_resync_cc;		/* consumer counter for arrays above */

	struct work_struct work;

	uint32_t flowid;
	uint32_t flowtype;
	uint32_t dek_index_ok:1;
	uint32_t tcp_resync_active:1;
	uint32_t tcp_resync_pending:1;

	/* parameters needed */
	uint8_t crypto_params[128] __aligned(4);
	uint8_t rx_progress[MLX5E_TLS_RX_PROGRESS_BUFFER_SIZE * 2];
} __aligned(MLX5E_CACHELINE_SIZE);

static inline void *
mlx5e_tls_rx_get_progress_buffer(struct mlx5e_tls_rx_tag *ptag)
{
	/* return properly aligned RX buffer */
	return (ptag->rx_progress +
	    ((-(uintptr_t)ptag->rx_progress) &
	    (MLX5E_TLS_RX_PROGRESS_BUFFER_SIZE - 1)));
}

#define	MLX5E_TLS_RX_STATS(m) \
  m(+1, u64, rx_resync_ok, "rx_resync_ok", "Successful resync requests")\
  m(+1, u64, rx_resync_err, "rx_resync_err", "Failed resync requests")\
  m(+1, u64, rx_error, "rx_error", "Other errors")

#define	MLX5E_TLS_RX_STATS_NUM (0 MLX5E_TLS_RX_STATS(MLX5E_STATS_COUNT))

struct mlx5e_tls_rx_stats {
	struct	sysctl_ctx_list ctx;
	counter_u64_t	arg[0];
	MLX5E_TLS_RX_STATS(MLX5E_STATS_COUNTER)
};

struct mlx5e_tls_rx {
	struct sysctl_ctx_list ctx;
	struct mlx5e_tls_rx_stats stats;
	struct workqueue_struct *wq;
	uma_zone_t zone;
	uint32_t max_resources;		/* max number of resources */
	volatile uint32_t num_resources;	/* current number of resources */
	int init;			/* set when ready */
	char zname[32];
};

int mlx5e_tls_rx_init(struct mlx5e_priv *);
void mlx5e_tls_rx_cleanup(struct mlx5e_priv *);

if_snd_tag_alloc_t mlx5e_tls_rx_snd_tag_alloc;

#endif		/* _MLX5_TLS_RX_H_ */
