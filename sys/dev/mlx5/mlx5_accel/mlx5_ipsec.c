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

#include "opt_ipsec.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/pfkeyv2.h>
#include <netipsec/key.h>
#include <netipsec/key_var.h>
#include <netipsec/keydb.h>
#include <netipsec/ipsec.h>
#include <netipsec/xform.h>
#include <netipsec/ipsec_offload.h>
#include <dev/mlx5/fs.h>
#include <dev/mlx5/mlx5_en/en.h>
#include <dev/mlx5/mlx5_accel/ipsec.h>

#define MLX5_IPSEC_RESCHED msecs_to_jiffies(1000)

static void mlx5e_if_sa_deinstall_onekey(struct ifnet *ifp, u_int dev_spi,
    void *priv);
static int mlx5e_if_sa_deinstall(struct ifnet *ifp, u_int dev_spi, void *priv);

static struct mlx5e_ipsec_sa_entry *to_ipsec_sa_entry(void *x)
{
	return (struct mlx5e_ipsec_sa_entry *)x;
}

static struct mlx5e_ipsec_pol_entry *to_ipsec_pol_entry(void *x)
{
	return (struct mlx5e_ipsec_pol_entry *)x;
}

static void
mlx5e_ipsec_handle_counters_onedir(struct mlx5e_ipsec_sa_entry *sa_entry,
    u64 *packets, u64 *bytes)
{
	struct mlx5e_ipsec_rule *ipsec_rule = &sa_entry->ipsec_rule;
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);

	mlx5_fc_query(mdev, ipsec_rule->fc, packets, bytes);
}

static struct mlx5e_ipsec_sa_entry *
mlx5e_ipsec_other_sa_entry(struct mlx5e_ipsec_priv_bothdir *pb,
    struct mlx5e_ipsec_sa_entry *sa_entry)
{
	return (pb->priv_in == sa_entry ? pb->priv_out : pb->priv_in);
}

static void
mlx5e_ipsec_handle_counters(struct work_struct *_work)
{
	struct mlx5e_ipsec_dwork *dwork =
	    container_of(_work, struct mlx5e_ipsec_dwork, dwork.work);
	struct mlx5e_ipsec_sa_entry *sa_entry = dwork->sa_entry;
	struct mlx5e_ipsec_sa_entry *other_sa_entry;
	u64 bytes, bytes1, packets1, packets;

	if (sa_entry->attrs.drop)
		return;
	other_sa_entry = mlx5e_ipsec_other_sa_entry(dwork->pb, sa_entry);
	if (other_sa_entry == NULL || other_sa_entry->attrs.drop)
		return;

	mlx5e_ipsec_handle_counters_onedir(sa_entry, &packets, &bytes);
	mlx5e_ipsec_handle_counters_onedir(other_sa_entry, &packets1, &bytes1);
	packets += packets1;
	bytes += bytes1;
	
#ifdef IPSEC_OFFLOAD
	ipsec_accel_drv_sa_lifetime_update(
	    sa_entry->savp, sa_entry->ifpo, sa_entry->kspi, bytes, packets);
#endif

	queue_delayed_work(sa_entry->ipsec->wq, &dwork->dwork,
	    MLX5_IPSEC_RESCHED);
}

static int
mlx5e_ipsec_create_dwork(struct mlx5e_ipsec_sa_entry *sa_entry,
    struct mlx5e_ipsec_priv_bothdir *pb)
{
        struct mlx5e_ipsec_dwork *dwork;

        dwork = kzalloc(sizeof(*dwork), GFP_KERNEL);
        if (!dwork)
		return (ENOMEM);

        dwork->sa_entry = sa_entry;
	dwork->pb = pb;
        INIT_DELAYED_WORK(&dwork->dwork, mlx5e_ipsec_handle_counters);
        sa_entry->dwork = dwork;
        return 0;
}

static int mlx5_xform_ah_authsize(const struct auth_hash *esph)
{
        int alen;

        if (esph == NULL)
                return 0;

        switch (esph->type) {
        case CRYPTO_SHA2_256_HMAC:
        case CRYPTO_SHA2_384_HMAC:
        case CRYPTO_SHA2_512_HMAC:
                alen = esph->hashsize / 2;      /* RFC4868 2.3 */
                break;

        case CRYPTO_POLY1305:
        case CRYPTO_AES_NIST_GMAC:
                alen = esph->hashsize;
                break;

        default:
                alen = AH_HMAC_HASHLEN;
                break;
        }

        return alen;
}

void mlx5e_ipsec_build_accel_xfrm_attrs(struct mlx5e_ipsec_sa_entry *sa_entry,
					struct mlx5_accel_esp_xfrm_attrs *attrs,
					u8 dir)
{
	struct secasvar *savp = sa_entry->savp;
	const struct auth_hash *esph = savp->tdb_authalgxform;
	struct aes_gcm_keymat *aes_gcm = &attrs->aes_gcm;
	struct secasindex *saidx = &savp->sah->saidx;
	struct seckey *key_encap = savp->key_enc;
	int key_len;

	memset(attrs, 0, sizeof(*attrs));

	/* subtract off the salt, RFC4106, 8.1 and RFC3686, 5.1 */
	key_len = _KEYLEN(key_encap) - SAV_ISCTRORGCM(savp) * 4 - SAV_ISCHACHA(savp) * 4;

	memcpy(aes_gcm->aes_key, key_encap->key_data, key_len);
	aes_gcm->key_len = key_len;

	/* salt and seq_iv */
	aes_gcm->seq_iv = 0;
	memcpy(&aes_gcm->salt, key_encap->key_data + key_len,
	       sizeof(aes_gcm->salt));

	switch (savp->alg_enc) {
	case SADB_X_EALG_AESGCM8:
		attrs->authsize = 8 / 4; /* in dwords */
		break;
	case SADB_X_EALG_AESGCM12:
		attrs->authsize = 12 / 4; /* in dwords */
		break;
	case SADB_X_EALG_AESGCM16:
		attrs->authsize = 16 / 4; /* in dwords */
		break;
	default: break;
	}

	/* iv len */
	aes_gcm->icv_len = mlx5_xform_ah_authsize(esph); //TBD: check if value make sense

	attrs->dir = dir;
	/* spi - host order */
	attrs->spi = ntohl(savp->spi);
	attrs->family = saidx->dst.sa.sa_family;
	attrs->reqid = saidx->reqid;

	if (saidx->src.sa.sa_family == AF_INET) {
		attrs->saddr.a4 = saidx->src.sin.sin_addr.s_addr;
		attrs->daddr.a4 = saidx->dst.sin.sin_addr.s_addr;
	} else {
		memcpy(&attrs->saddr.a6, &saidx->src.sin6.sin6_addr, 16);
		memcpy(&attrs->daddr.a6, &saidx->dst.sin6.sin6_addr, 16);
	}

	if (savp->natt) {
		attrs->encap = true;
		attrs->sport = savp->natt->sport;
		attrs->dport = savp->natt->dport;
	}

	if (savp->flags & SADB_X_SAFLAGS_ESN) {
		/* We support replay window with ESN only */
		attrs->replay_esn.trigger = true;
		if (sa_entry->esn_state.esn_msb)
			attrs->replay_esn.esn = sa_entry->esn_state.esn;
		else
			/* According to RFC4303, section "3.3.3. Sequence Number Generation",
			 * the first packet sent using a given SA will contain a sequence
			 * number of 1.
			 */
			attrs->replay_esn.esn = max_t(u32, sa_entry->esn_state.esn, 1);
		attrs->replay_esn.esn_msb = sa_entry->esn_state.esn_msb;
		attrs->replay_esn.overlap = sa_entry->esn_state.overlap;

	        if (savp->replay) {
			switch (savp->replay->wsize) {
			case 4:
	                     attrs->replay_esn.replay_window = MLX5_IPSEC_ASO_REPLAY_WIN_32BIT;
			     break;
			case 8:
	                     attrs->replay_esn.replay_window = MLX5_IPSEC_ASO_REPLAY_WIN_64BIT;
			     break;
			case 16:
	                     attrs->replay_esn.replay_window = MLX5_IPSEC_ASO_REPLAY_WIN_128BIT;
			     break;
			case 32:
	                     attrs->replay_esn.replay_window = MLX5_IPSEC_ASO_REPLAY_WIN_256BIT;
			     break;
			default:
			     /* Do nothing */
			     break;
	                }
		}
        }
}

static int mlx5e_xfrm_validate_state(struct mlx5_core_dev *mdev,
				     struct secasvar *savp)
{
	struct secasindex *saidx = &savp->sah->saidx;
	struct seckey *key_encp = savp->key_enc;
	int keylen;

	if (!(mlx5_ipsec_device_caps(mdev) &
				MLX5_IPSEC_CAP_PACKET_OFFLOAD)) {
		mlx5_core_err(mdev, "FULL offload is not supported\n");
		return (EOPNOTSUPP);
	}
	if (savp->state == SADB_SASTATE_DEAD)
		return (EOPNOTSUPP);
	if (savp->alg_enc == SADB_EALG_NONE) {
		mlx5_core_err(mdev, "Cannot offload authenticated xfrm states\n");
		return (EOPNOTSUPP);
	}
	if (savp->alg_enc != SADB_X_EALG_AESGCM16) {
		mlx5_core_err(mdev, "Only IPSec aes-gcm-16 encryption protocol may be offloaded\n");
		return (EOPNOTSUPP);
	}
	if (savp->tdb_compalgxform) {
		mlx5_core_err(mdev, "Cannot offload compressed xfrm states\n");
		return (EOPNOTSUPP);
	}
	if (savp->alg_auth != SADB_X_AALG_AES128GMAC && savp->alg_auth != SADB_X_AALG_AES256GMAC) {
		mlx5_core_err(mdev, "Cannot offload xfrm states with AEAD key length other than 128/256 bits\n");
		return (EOPNOTSUPP);
	}
	if ((saidx->dst.sa.sa_family != AF_INET && saidx->dst.sa.sa_family != AF_INET6) ||
	    (saidx->src.sa.sa_family != AF_INET && saidx->src.sa.sa_family != AF_INET6)) {
		mlx5_core_err(mdev, "Only IPv4/6 xfrm states may be offloaded\n");
		return (EOPNOTSUPP);
	}
	if (saidx->proto != IPPROTO_ESP) {
		mlx5_core_err(mdev, "Only ESP xfrm state may be offloaded\n");
		return (EOPNOTSUPP);
	}
	/* subtract off the salt, RFC4106, 8.1 and RFC3686, 5.1 */
	keylen = _KEYLEN(key_encp) - SAV_ISCTRORGCM(savp) * 4 - SAV_ISCHACHA(savp) * 4;
	if (keylen != 128/8 && keylen != 256 / 8) {
		mlx5_core_err(mdev, "Cannot offload xfrm states with AEAD key length other than 128/256 bit\n");
		return (EOPNOTSUPP);
	}

	if (saidx->mode != IPSEC_MODE_TRANSPORT) {
		mlx5_core_err(mdev, "Only transport xfrm states may be offloaded in full offload mode\n");
		return (EOPNOTSUPP);
	}

	if (savp->natt) {
		if (!(mlx5_ipsec_device_caps(mdev) & MLX5_IPSEC_CAP_ESPINUDP)) {
			mlx5_core_err(mdev, "Encapsulation is not supported\n");
			return (EOPNOTSUPP);
		}
        }

        if (savp->replay && savp->replay->wsize != 0 && savp->replay->wsize != 4 &&
	    savp->replay->wsize != 8 && savp->replay->wsize != 16 && savp->replay->wsize != 32) {
		mlx5_core_err(mdev, "Unsupported replay window size %d\n", savp->replay->wsize);
		return (EOPNOTSUPP);
	}

	if ((savp->flags & SADB_X_SAFLAGS_ESN) != 0) {
		if ((mlx5_ipsec_device_caps(mdev) & MLX5_IPSEC_CAP_ESN) == 0) {
			mlx5_core_err(mdev, "ESN is not supported\n");
			return (EOPNOTSUPP);
		}
	} else if (savp->replay != NULL && savp->replay->wsize != 0) {
		mlx5_core_warn(mdev,
		    "non-ESN but replay-protect SA offload is not supported\n");
		return (EOPNOTSUPP);
	}
        return 0;
}

static int
mlx5e_if_sa_newkey_onedir(struct ifnet *ifp, void *sav, int dir, u_int drv_spi,
    struct mlx5e_ipsec_sa_entry **privp, struct mlx5e_ipsec_priv_bothdir *pb,
    struct ifnet *ifpo)
{
#ifdef IPSEC_OFFLOAD
	struct rm_priotracker tracker;
#endif
	struct mlx5e_ipsec_sa_entry *sa_entry = NULL;
	struct mlx5e_priv *priv = if_getsoftc(ifp);
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5e_ipsec *ipsec = priv->ipsec;
	u16 vid = VLAN_NONE;
	int err;

	if (priv->gone != 0 || ipsec == NULL)
		return (EOPNOTSUPP);

	if (if_gettype(ifpo) == IFT_L2VLAN)
		VLAN_TAG(ifpo, &vid);

#ifdef IPSEC_OFFLOAD
	ipsec_sahtree_rlock(&tracker);
#endif
	err = mlx5e_xfrm_validate_state(mdev, sav);
#ifdef IPSEC_OFFLOAD
	ipsec_sahtree_runlock(&tracker);
#endif
	if (err)
		return err;

	sa_entry = kzalloc(sizeof(*sa_entry), GFP_KERNEL);
	if (sa_entry == NULL)
		return (ENOMEM);

	sa_entry->kspi = drv_spi;
	sa_entry->savp = sav;
	sa_entry->ifp = ifp;
	sa_entry->ifpo = ifpo;
	sa_entry->ipsec = ipsec;
	sa_entry->vid = vid;

#ifdef IPSEC_OFFLOAD
	ipsec_sahtree_rlock(&tracker);
#endif
	err = mlx5e_xfrm_validate_state(mdev, sav);
	if (err != 0) {
#ifdef IPSEC_OFFLOAD
		ipsec_sahtree_runlock(&tracker);
#endif
		goto err_xfrm;
	}
	mlx5e_ipsec_build_accel_xfrm_attrs(sa_entry, &sa_entry->attrs, dir);
#ifdef IPSEC_OFFLOAD
	ipsec_sahtree_runlock(&tracker);
#endif

	err = mlx5e_ipsec_create_dwork(sa_entry, pb);
	if (err)
		goto err_xfrm;

	/* create hw context */
	err = mlx5_ipsec_create_sa_ctx(sa_entry);
	if (err)
		goto err_sa_ctx;

	err = mlx5e_accel_ipsec_fs_add_rule(sa_entry);
	if (err)
		goto err_fs;

	*privp = sa_entry;
	if (sa_entry->dwork)
		queue_delayed_work(ipsec->wq, &sa_entry->dwork->dwork, MLX5_IPSEC_RESCHED);

	err = xa_insert(&mdev->ipsec_sadb, sa_entry->ipsec_obj_id, sa_entry, GFP_KERNEL);
	if (err)
		goto err_xa;

	return 0;

err_xa:
	if (sa_entry->dwork)
		cancel_delayed_work_sync(&sa_entry->dwork->dwork);
	mlx5e_accel_ipsec_fs_del_rule(sa_entry);
err_fs:
	mlx5_ipsec_free_sa_ctx(sa_entry);
err_sa_ctx:
	kfree(sa_entry->dwork);
	sa_entry->dwork = NULL;
err_xfrm:
	kfree(sa_entry);
	mlx5_en_err(ifp, "Device failed to offload this state");
	return err;
}

#define GET_TRUNK_IF(vifp, ifp, ept)          \
	if (if_gettype(vifp) == IFT_L2VLAN) { \
		NET_EPOCH_ENTER(ept);         \
		ifp = VLAN_TRUNKDEV(vifp);    \
		NET_EPOCH_EXIT(ept);          \
	} else {                              \
		ifp = vifp;                   \
	}

static int
mlx5e_if_sa_newkey(struct ifnet *ifpo, void *sav, u_int dev_spi, void **privp)
{
	struct mlx5e_ipsec_priv_bothdir *pb;
	struct epoch_tracker et;
	struct ifnet *ifp;
	int error;

	GET_TRUNK_IF(ifpo, ifp, et);

	pb = malloc(sizeof(struct mlx5e_ipsec_priv_bothdir), M_DEVBUF,
	    M_WAITOK | M_ZERO);
	error = mlx5e_if_sa_newkey_onedir(
	    ifp, sav, IPSEC_DIR_INBOUND, dev_spi, &pb->priv_in, pb, ifpo);
	if (error != 0) {
		free(pb, M_DEVBUF);
		return (error);
	}
	error = mlx5e_if_sa_newkey_onedir(
	    ifp, sav, IPSEC_DIR_OUTBOUND, dev_spi, &pb->priv_out, pb, ifpo);
	if (error == 0) {
		*privp = pb;
	} else {
		if (pb->priv_in->dwork != NULL)
			cancel_delayed_work_sync(&pb->priv_in->dwork->dwork);
		mlx5e_if_sa_deinstall_onekey(ifp, dev_spi, pb->priv_in);
		free(pb, M_DEVBUF);
	}
	return (error);
}

static void
mlx5e_if_sa_deinstall_onekey(struct ifnet *ifp, u_int dev_spi, void *priv)
{
	struct mlx5e_ipsec_sa_entry *sa_entry = to_ipsec_sa_entry(priv);
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);
	struct mlx5e_ipsec_sa_entry *old;

	old = xa_erase(&mdev->ipsec_sadb, sa_entry->ipsec_obj_id);
	WARN_ON(old != sa_entry);

	mlx5e_accel_ipsec_fs_del_rule(sa_entry);
	mlx5_ipsec_free_sa_ctx(sa_entry);
	kfree(sa_entry->dwork);
	kfree(sa_entry);
}

static int
mlx5e_if_sa_deinstall(struct ifnet *ifpo, u_int dev_spi, void *priv)
{
	struct mlx5e_ipsec_priv_bothdir pb, *pbp;
	struct epoch_tracker et;
	struct ifnet *ifp;

	GET_TRUNK_IF(ifpo, ifp, et);

	pbp = priv;
	pb = *(struct mlx5e_ipsec_priv_bothdir *)priv;
	pbp->priv_in = pbp->priv_out = NULL;

	if (pb.priv_in->dwork != NULL)
		cancel_delayed_work_sync(&pb.priv_in->dwork->dwork);
	if (pb.priv_out->dwork != NULL)
		cancel_delayed_work_sync(&pb.priv_out->dwork->dwork);

	mlx5e_if_sa_deinstall_onekey(ifp, dev_spi, pb.priv_in);
	mlx5e_if_sa_deinstall_onekey(ifp, dev_spi, pb.priv_out);
	free(pbp, M_DEVBUF);
	return (0);
}

static void
mlx5e_if_sa_cnt_one(struct ifnet *ifp, void *sa, uint32_t drv_spi,
    void *priv, u64 *bytes, u64 *packets)
{
	struct mlx5e_ipsec_sa_entry *sa_entry = to_ipsec_sa_entry(priv);
	struct mlx5e_ipsec_rule *ipsec_rule = &sa_entry->ipsec_rule;
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);

	mlx5_fc_query(mdev, ipsec_rule->fc, packets, bytes);
}

static int
mlx5e_if_sa_cnt(struct ifnet *ifpo, void *sa, uint32_t drv_spi, void *priv,
    struct seclifetime *lt)
{
	struct mlx5e_ipsec_priv_bothdir *pb;
	u64 packets_in, packets_out;
	u64 bytes_in, bytes_out;
	struct epoch_tracker et;
	struct ifnet *ifp;

	GET_TRUNK_IF(ifpo, ifp, et);

	pb = priv;
	mlx5e_if_sa_cnt_one(ifp, sa, drv_spi, pb->priv_in,
	    &bytes_in, &packets_in);
	mlx5e_if_sa_cnt_one(ifp, sa, drv_spi, pb->priv_out,
	    &bytes_out, &packets_out);
	/* TODO: remove this casting once Kostia changes allocation type to be u64 */
	lt->bytes = bytes_in + bytes_out;
	lt->allocations = (uint32_t)(packets_in + packets_out);
	return (0);
}

static int mlx5e_xfrm_validate_policy(struct mlx5_core_dev *mdev,
                                      struct secpolicy *sp, struct inpcb *inp)
{
	struct secpolicyindex *spidx = &sp->spidx;

	if (!(mlx5_ipsec_device_caps(mdev) &
				MLX5_IPSEC_CAP_PACKET_OFFLOAD)) {
		mlx5_core_err(mdev, "FULL offload is not supported\n");
		return (EINVAL);
	}

        if (sp->tcount > 1) {
		mlx5_core_err(mdev, "Can offload exactly one template, "
		    "not %d\n", sp->tcount);
                return (EINVAL);
        }

        if (sp->policy == IPSEC_POLICY_BYPASS &&
            !(mlx5_ipsec_device_caps(mdev) & MLX5_IPSEC_CAP_PRIO)) {
		mlx5_core_err(mdev, "Device does not support policy priority\n");
		return (EINVAL);
	}

	if (sp->tcount > 0 && inp != NULL) {
		mlx5_core_err(mdev, "Not valid input data\n");
		return (EINVAL);
	}

	if (spidx->dir != IPSEC_DIR_INBOUND && spidx->dir != IPSEC_DIR_OUTBOUND) {
		mlx5_core_err(mdev, "Wrong policy direction\n");
		return (EINVAL);
	}

	if (sp->tcount > 0 && sp->req[0]->saidx.mode != IPSEC_MODE_TRANSPORT) {
		mlx5_core_err(mdev, "Device supports transport mode only");
		return (EINVAL);
	}

        if (sp->policy != IPSEC_POLICY_DISCARD &&
            sp->policy != IPSEC_POLICY_IPSEC && sp->policy != IPSEC_POLICY_BYPASS) {
                mlx5_core_err(mdev, "Offloaded policy must be specific on its action\n");
		return (EINVAL);
        }

	if (sp->policy == IPSEC_POLICY_BYPASS && !inp) {
		mlx5_core_err(mdev, "Missing port information for IKE bypass\n");
		return (EINVAL);
	}

	if (inp != NULL) {
		INP_RLOCK(inp);
		if (inp->inp_socket == NULL || inp->inp_socket->so_proto->
		    pr_protocol != IPPROTO_UDP) {
			mlx5_core_err(mdev, "Unsupported IKE bypass protocol %d\n",
			    inp->inp_socket == NULL ? -1 :
			    inp->inp_socket->so_proto->pr_protocol);
			INP_RUNLOCK(inp);
			return (EINVAL);
		}
		INP_RUNLOCK(inp);
	}

        /* TODO fill relevant bits */
	return 0;
}

static void
mlx5e_ipsec_build_accel_pol_attrs(struct mlx5e_ipsec_pol_entry *pol_entry,
    struct mlx5_accel_pol_xfrm_attrs *attrs, struct inpcb *inp, u16 vid)
{
	struct secpolicy *sp = pol_entry->sp;
	struct secpolicyindex *spidx = &sp->spidx;

	memset(attrs, 0, sizeof(*attrs));

	if (!inp) {
		if (spidx->src.sa.sa_family == AF_INET) {
			attrs->saddr.a4 = spidx->src.sin.sin_addr.s_addr;
			attrs->daddr.a4 = spidx->dst.sin.sin_addr.s_addr;
		} else if (spidx->src.sa.sa_family == AF_INET6) {
			memcpy(&attrs->saddr.a6, &spidx->src.sin6.sin6_addr, 16);
			memcpy(&attrs->daddr.a6, &spidx->dst.sin6.sin6_addr, 16);
		} else {
			KASSERT(0, ("unsupported family %d", spidx->src.sa.sa_family));
		}
		attrs->family = spidx->src.sa.sa_family;
		attrs->prio = 0;
		attrs->action = sp->policy;
		attrs->reqid = sp->req[0]->saidx.reqid;
	} else {
		INP_RLOCK(inp);
		if ((inp->inp_vflag & INP_IPV4) != 0) {
			attrs->saddr.a4 = inp->inp_laddr.s_addr;
			attrs->daddr.a4 = inp->inp_faddr.s_addr;
			attrs->family = AF_INET;
		} else if ((inp->inp_vflag & INP_IPV6) != 0) {
			memcpy(&attrs->saddr.a6, &inp->in6p_laddr, 16);
			memcpy(&attrs->daddr.a6, &inp->in6p_laddr, 16);
			attrs->family = AF_INET6;
		} else {
			KASSERT(0, ("unsupported family %d", inp->inp_vflag));
		}
		attrs->upspec.dport = inp->inp_fport;
		attrs->upspec.sport = inp->inp_lport;
		attrs->upspec.proto = inp->inp_ip_p;
		INP_RUNLOCK(inp);

		/* Give highest priority for PCB policies */
		attrs->prio = 1;
		attrs->action = IPSEC_POLICY_IPSEC;
	}
	attrs->dir = spidx->dir;
	attrs->vid = vid;
}

static int
mlx5e_if_spd_install(struct ifnet *ifpo, void *sp, void *inp1, void **ifdatap)
{
	struct mlx5e_ipsec_pol_entry *pol_entry;
	struct mlx5e_priv *priv;
	struct epoch_tracker et;
	u16 vid = VLAN_NONE;
	struct ifnet *ifp;
	int err;

	GET_TRUNK_IF(ifpo, ifp, et);
	if (if_gettype(ifpo) == IFT_L2VLAN)
		VLAN_TAG(ifpo, &vid);
	priv = if_getsoftc(ifp);
	if (priv->gone || !priv->ipsec)
		return (EOPNOTSUPP);

	err = mlx5e_xfrm_validate_policy(priv->mdev, sp, inp1);
	if (err)
		return err;

	pol_entry = kzalloc(sizeof(*pol_entry), GFP_KERNEL);
	if (!pol_entry)
		return (ENOMEM);

	pol_entry->sp = sp;
	pol_entry->ipsec = priv->ipsec;

	mlx5e_ipsec_build_accel_pol_attrs(pol_entry, &pol_entry->attrs,
	    inp1, vid);
	err = mlx5e_accel_ipsec_fs_add_pol(pol_entry);
	if (err)
		goto err_pol;
	*ifdatap = pol_entry;

	return 0;

err_pol:
	kfree(pol_entry);
	mlx5_en_err(ifp, "Device failed to offload this policy");
	return err;
}

static int
mlx5e_if_spd_deinstall(struct ifnet *ifpo, void *sp, void *ifdata)
{
	struct mlx5e_ipsec_pol_entry *pol_entry;

	pol_entry = to_ipsec_pol_entry(ifdata);
	mlx5e_accel_ipsec_fs_del_pol(pol_entry);
	kfree(pol_entry);
	return 0;
}

void mlx5e_ipsec_cleanup(struct mlx5e_priv *priv)
{
	struct mlx5e_ipsec *pipsec = priv->ipsec;
	if (!pipsec)
		return;

	mlx5e_accel_ipsec_fs_cleanup(pipsec);
	destroy_workqueue(pipsec->wq);
	mlx5e_ipsec_aso_cleanup(pipsec);
	kfree(pipsec);
	priv->ipsec = NULL;
}

static int
mlx5e_if_ipsec_hwassist(if_t ifneto, void *sav __unused,
    uint32_t drv_spi __unused, void *priv __unused)
{
	if_t ifnet;

	if (if_gettype(ifneto) == IFT_L2VLAN) {
		ifnet = VLAN_TRUNKDEV(ifneto);
	} else {
		ifnet = ifneto;
	}

	return (if_gethwassist(ifnet) & (CSUM_TSO | CSUM_TCP | CSUM_UDP |
	    CSUM_IP | CSUM_IP6_TSO | CSUM_IP6_TCP | CSUM_IP6_UDP));
}

static const struct if_ipsec_accel_methods  mlx5e_ipsec_funcs = {
	.if_sa_newkey = mlx5e_if_sa_newkey,
	.if_sa_deinstall = mlx5e_if_sa_deinstall,
	.if_spdadd = mlx5e_if_spd_install,
	.if_spddel = mlx5e_if_spd_deinstall,
	.if_sa_cnt = mlx5e_if_sa_cnt,
	.if_hwassist = mlx5e_if_ipsec_hwassist,
};

int mlx5e_ipsec_init(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5e_ipsec *pipsec;
	if_t ifp = priv->ifp;
	int ret;

	mlx5_core_info(mdev, "ipsec "
	    "offload %d log_max_dek %d gen_obj_types %d "
	    "ipsec_encrypt %d ipsec_decrypt %d "
	    "esp_aes_gcm_128_encrypt %d esp_aes_gcm_128_decrypt %d "
	    "ipsec_full_offload %d "
	    "reformat_add_esp_trasport %d reformat_del_esp_trasport %d "
	    "decap %d "
	    "ignore_flow_level_tx %d ignore_flow_level_rx %d "
	    "reformat_natt_tx %d reformat_natt_rx %d "
	    "ipsec_esn %d\n",
	    MLX5_CAP_GEN(mdev, ipsec_offload) != 0,
	    MLX5_CAP_GEN(mdev, log_max_dek) != 0,
	    (MLX5_CAP_GEN_64(mdev, general_obj_types) &
		MLX5_HCA_CAP_GENERAL_OBJECT_TYPES_IPSEC) != 0,
	    MLX5_CAP_FLOWTABLE_NIC_TX(mdev, ipsec_encrypt) != 0,
	    MLX5_CAP_FLOWTABLE_NIC_RX(mdev, ipsec_decrypt) != 0,
	    MLX5_CAP_IPSEC(mdev, ipsec_crypto_esp_aes_gcm_128_encrypt) != 0,
	    MLX5_CAP_IPSEC(mdev, ipsec_crypto_esp_aes_gcm_128_decrypt) != 0,
	    MLX5_CAP_IPSEC(mdev, ipsec_full_offload) != 0,
            MLX5_CAP_FLOWTABLE_NIC_TX(mdev, reformat_add_esp_trasport) != 0,
            MLX5_CAP_FLOWTABLE_NIC_RX(mdev, reformat_del_esp_trasport) != 0,
            MLX5_CAP_FLOWTABLE_NIC_RX(mdev, decap) != 0,
	    MLX5_CAP_FLOWTABLE_NIC_TX(mdev, ignore_flow_level) != 0,
	    MLX5_CAP_FLOWTABLE_NIC_RX(mdev, ignore_flow_level) != 0,
	    MLX5_CAP_FLOWTABLE_NIC_TX(mdev,
	        reformat_add_esp_transport_over_udp) != 0,
	    MLX5_CAP_FLOWTABLE_NIC_RX(mdev,
		reformat_del_esp_transport_over_udp) != 0,
	    MLX5_CAP_IPSEC(mdev, ipsec_esn) != 0);

	if (!(mlx5_ipsec_device_caps(mdev) & MLX5_IPSEC_CAP_PACKET_OFFLOAD)) {
		mlx5_core_dbg(mdev, "Not an IPSec offload device\n");
		return 0;
	}

	xa_init_flags(&mdev->ipsec_sadb, XA_FLAGS_ALLOC);

	pipsec = kzalloc(sizeof(*pipsec), GFP_KERNEL);
	if (pipsec == NULL)
		return (ENOMEM);

	pipsec->mdev = mdev;
	pipsec->pdn = priv->pdn;
	pipsec->mkey = priv->mr.key;

	ret = mlx5e_ipsec_aso_init(pipsec);
	if (ret)
		goto err_ipsec_aso;

	pipsec->wq = alloc_workqueue("mlx5e_ipsec", WQ_UNBOUND, 0);
	if (pipsec->wq == NULL) {
		ret = ENOMEM;
		goto err_ipsec_wq;
	}

	ret = mlx5e_accel_ipsec_fs_init(pipsec);
	if (ret)
		goto err_ipsec_alloc;

	if_setipsec_accel_methods(ifp, &mlx5e_ipsec_funcs);
	priv->ipsec = pipsec;
	mlx5_core_dbg(mdev, "IPSec attached to netdevice\n");
	return 0;

err_ipsec_alloc:
	destroy_workqueue(pipsec->wq);
err_ipsec_wq:
	mlx5e_ipsec_aso_cleanup(pipsec);
err_ipsec_aso:
	kfree(pipsec);
	mlx5_core_err(priv->mdev, "IPSec initialization failed, %d\n", ret);
	return ret;
}
