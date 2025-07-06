/*-
 * Copyright (c) 2023 NVIDIA corporation & affiliates.
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
 */

#ifndef __MLX5_ACCEL_IPSEC_H__
#define __MLX5_ACCEL_IPSEC_H__

#include <sys/mbuf.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/qp.h>
#include <dev/mlx5/mlx5_core/mlx5_core.h>
#include <dev/mlx5/mlx5_en/en.h>
#include <dev/mlx5/mlx5_lib/aso.h>

#define MLX5E_IPSEC_SADB_RX_BITS 10
#define MLX5_IPSEC_METADATA_MARKER(ipsec_metadata) ((ipsec_metadata >> 31) & 0x1)

#define VLAN_NONE 0xfff

struct mlx5e_priv;
struct mlx5e_tx_wqe;
struct mlx5e_ipsec_tx;
struct mlx5e_ipsec_rx;
struct mlx5e_ipsec_rx_ip_type;

struct aes_gcm_keymat {
	u64   seq_iv;

	u32   salt;
	u32   icv_len;

	u32   key_len;
	u32   aes_key[256 / 32];
};

struct mlx5e_ipsec_priv_bothdir {
	struct mlx5e_ipsec_sa_entry *priv_in;
	struct mlx5e_ipsec_sa_entry *priv_out;
};

struct mlx5e_ipsec_work {
        struct work_struct work;
        struct mlx5e_ipsec_sa_entry *sa_entry;
        void *data;
};

struct mlx5e_ipsec_dwork {
	struct delayed_work dwork;
	struct mlx5e_ipsec_sa_entry *sa_entry;
	struct mlx5e_ipsec_priv_bothdir *pb;
};

struct mlx5e_ipsec_aso {
        u8 __aligned(64) ctx[MLX5_ST_SZ_BYTES(ipsec_aso)];
        dma_addr_t dma_addr;
        struct mlx5_aso *aso;
        /* Protect ASO WQ access, as it is global to whole IPsec */
        spinlock_t lock;
};

struct mlx5_replay_esn {
	u32 replay_window;
	u32 esn;
	u32 esn_msb;
	u8 overlap : 1;
	u8 trigger : 1;
};

struct mlx5_accel_esp_xfrm_attrs {
	u32   spi;
	struct aes_gcm_keymat aes_gcm;

	union {
		__be32 a4;
		__be32 a6[4];
	} saddr;

	union {
		__be32 a4;
		__be32 a6[4];
	} daddr;

	u8 dir : 2;
	u8 encap : 1;
	u8 drop : 1;
	u8 family;
	struct mlx5_replay_esn replay_esn;
	u32 authsize;
	u32 reqid;
	u16 sport;
	u16 dport;
};

enum mlx5_ipsec_cap {
	MLX5_IPSEC_CAP_CRYPTO		= 1 << 0,
	MLX5_IPSEC_CAP_ESN		= 1 << 1,
	MLX5_IPSEC_CAP_PACKET_OFFLOAD	= 1 << 2,
	MLX5_IPSEC_CAP_ROCE             = 1 << 3,
	MLX5_IPSEC_CAP_PRIO             = 1 << 4,
	MLX5_IPSEC_CAP_TUNNEL           = 1 << 5,
	MLX5_IPSEC_CAP_ESPINUDP         = 1 << 6,
};

struct mlx5e_ipsec {
	struct mlx5_core_dev *mdev;
	struct workqueue_struct *wq;
	struct mlx5e_ipsec_tx *tx;
	struct mlx5e_ipsec_rx *rx_ipv4;
	struct mlx5e_ipsec_rx *rx_ipv6;
	struct mlx5e_ipsec_rx_ip_type *rx_ip_type;
	struct mlx5e_ipsec_aso *aso;
	u32 pdn;
	u32 mkey;
};

struct mlx5e_ipsec_rule {
	struct mlx5_flow_handle *rule;
	struct mlx5_flow_handle *kspi_rule;
	struct mlx5_flow_handle *reqid_rule;
	struct mlx5_flow_handle *vid_zero_rule;
	struct mlx5_modify_hdr *modify_hdr;
	struct mlx5_pkt_reformat *pkt_reformat;
	struct mlx5_fc *fc;
};

struct mlx5e_ipsec_esn_state {
	u32 esn;
	u32 esn_msb;
	u8 overlap: 1;
};     

struct mlx5e_ipsec_sa_entry {
	struct secasvar *savp;
	if_t ifp;
	if_t ifpo;
	struct mlx5e_ipsec *ipsec;
	struct mlx5_accel_esp_xfrm_attrs attrs;
	struct mlx5e_ipsec_rule ipsec_rule;
	struct mlx5e_ipsec_dwork *dwork;
	struct mlx5e_ipsec_work *work;
	u32 ipsec_obj_id;
	u32 enc_key_id;
	u16 kspi; /* Stack allocated unique SA identifier */
	struct mlx5e_ipsec_esn_state esn_state;
	u16 vid;
};

struct upspec {
        u16 dport;
        u16 sport;
        u8 proto;
};

struct mlx5_accel_pol_xfrm_attrs {
        union {
                __be32 a4;
                __be32 a6[4];
        } saddr;

        union {
                __be32 a4;
                __be32 a6[4];
        } daddr;

	struct upspec upspec;

        u8 family;
        u8 action;
        u8 dir : 2;
        u32 reqid;
        u32 prio;
        u16 vid;
};

struct mlx5e_ipsec_pol_entry {
	struct secpolicy *sp;
	struct mlx5e_ipsec *ipsec;
	struct mlx5e_ipsec_rule ipsec_rule;
	struct mlx5_accel_pol_xfrm_attrs attrs;
};

/* This function doesn't really belong here, but let's put it here for now */
void mlx5_object_change_event(struct mlx5_core_dev *dev, struct mlx5_eqe *eqe);

int mlx5e_ipsec_init(struct mlx5e_priv *priv);
void mlx5e_ipsec_cleanup(struct mlx5e_priv *priv);

int mlx5e_ipsec_aso_init(struct mlx5e_ipsec *ipsec);
void mlx5e_ipsec_aso_cleanup(struct mlx5e_ipsec *ipsec);

int mlx5_ipsec_create_sa_ctx(struct mlx5e_ipsec_sa_entry *sa_entry);
void mlx5_ipsec_free_sa_ctx(struct mlx5e_ipsec_sa_entry *sa_entry);

u32 mlx5_ipsec_device_caps(struct mlx5_core_dev *mdev);

static inline struct mlx5_core_dev *
mlx5e_ipsec_sa2dev(struct mlx5e_ipsec_sa_entry *sa_entry)
{
	return sa_entry->ipsec->mdev;
}

static inline struct mlx5_core_dev *
mlx5e_ipsec_pol2dev(struct mlx5e_ipsec_pol_entry *pol_entry)
{
	return pol_entry->ipsec->mdev;
}

void mlx5e_ipsec_build_accel_xfrm_attrs(struct mlx5e_ipsec_sa_entry *sa_entry,
					struct mlx5_accel_esp_xfrm_attrs *attrs,
					u8 dir);

int mlx5e_accel_ipsec_fs_init(struct mlx5e_ipsec *ipsec);
void mlx5e_accel_ipsec_fs_cleanup(struct mlx5e_ipsec *ipsec);
int mlx5e_accel_ipsec_fs_add_rule(struct mlx5e_ipsec_sa_entry *sa_entry);
void mlx5e_accel_ipsec_fs_del_rule(struct mlx5e_ipsec_sa_entry *sa_entry);
void mlx5e_accel_ipsec_fs_modify(struct mlx5e_ipsec_sa_entry *sa_entry);
struct ipsec_accel_out_tag;
void mlx5e_accel_ipsec_handle_tx_wqe(struct mbuf *mb, struct mlx5e_tx_wqe *wqe,
    struct ipsec_accel_out_tag *tag);
int mlx5e_accel_ipsec_fs_add_pol(struct mlx5e_ipsec_pol_entry *pol_entry);
void mlx5e_accel_ipsec_fs_del_pol(struct mlx5e_ipsec_pol_entry *pol_entry);
static inline int mlx5e_accel_ipsec_get_metadata(unsigned int id)
{
	return MLX5_ETH_WQE_FT_META_IPSEC << 23 |  id;
}
static inline void
mlx5e_accel_ipsec_handle_tx(struct mbuf *mb, struct mlx5e_tx_wqe *wqe)
{
	struct ipsec_accel_out_tag *tag;

	tag = (struct ipsec_accel_out_tag *)m_tag_find(mb,
	    PACKET_TAG_IPSEC_ACCEL_OUT, NULL);
	if (tag != NULL)
		mlx5e_accel_ipsec_handle_tx_wqe(mb, wqe, tag);
}
void mlx5e_accel_ipsec_fs_rx_tables_destroy(struct mlx5e_priv *priv);
int mlx5e_accel_ipsec_fs_rx_tables_create(struct mlx5e_priv *priv);
void mlx5e_accel_ipsec_fs_rx_catchall_rules_destroy(struct mlx5e_priv *priv);
int mlx5e_accel_ipsec_fs_rx_catchall_rules(struct mlx5e_priv *priv);
int mlx5_accel_ipsec_rx_tag_add(if_t ifp, struct mlx5e_rq_mbuf *mr);
void mlx5e_accel_ipsec_handle_rx_cqe(if_t ifp, struct mbuf *mb,
    struct mlx5_cqe64 *cqe, struct mlx5e_rq_mbuf *mr);

static inline int mlx5e_accel_ipsec_flow(struct mlx5_cqe64 *cqe)
{
	return MLX5_IPSEC_METADATA_MARKER(be32_to_cpu(cqe->ft_metadata));
}

static inline void
mlx5e_accel_ipsec_handle_rx(if_t ifp, struct mbuf *mb, struct mlx5_cqe64 *cqe,
    struct mlx5e_rq_mbuf *mr)
{
	u32 ipsec_meta_data = be32_to_cpu(cqe->ft_metadata);

	if (MLX5_IPSEC_METADATA_MARKER(ipsec_meta_data))
		mlx5e_accel_ipsec_handle_rx_cqe(ifp, mb, cqe, mr);
}
#endif	/* __MLX5_ACCEL_IPSEC_H__ */
