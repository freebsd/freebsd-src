/*-
 * Copyright (c) 2015-2021 Mellanox Technologies. All rights reserved.
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

#include "opt_kern_tls.h"
#include "opt_rss.h"
#include "opt_ratelimit.h"

#include "en.h"

#include <sys/eventhandler.h>
#include <sys/sockio.h>
#include <machine/atomic.h>

#include <net/debugnet.h>

static int mlx5e_get_wqe_sz(struct mlx5e_priv *priv, u32 *wqe_sz, u32 *nsegs);
static if_snd_tag_query_t mlx5e_ul_snd_tag_query;
static if_snd_tag_free_t mlx5e_ul_snd_tag_free;

struct mlx5e_channel_param {
	struct mlx5e_rq_param rq;
	struct mlx5e_sq_param sq;
	struct mlx5e_cq_param rx_cq;
	struct mlx5e_cq_param tx_cq;
};

struct media {
	u32	subtype;
	u64	baudrate;
};

static const struct media mlx5e_mode_table[MLX5E_LINK_SPEEDS_NUMBER] =
{
	[MLX5E_1000BASE_CX_SGMII] = {
		.subtype = IFM_1000_CX_SGMII,
		.baudrate = IF_Mbps(1000ULL),
	},
	[MLX5E_1000BASE_KX] = {
		.subtype = IFM_1000_KX,
		.baudrate = IF_Mbps(1000ULL),
	},
	[MLX5E_10GBASE_CX4] = {
		.subtype = IFM_10G_CX4,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_10GBASE_KX4] = {
		.subtype = IFM_10G_KX4,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_10GBASE_KR] = {
		.subtype = IFM_10G_KR,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_20GBASE_KR2] = {
		.subtype = IFM_20G_KR2,
		.baudrate = IF_Gbps(20ULL),
	},
	[MLX5E_40GBASE_CR4] = {
		.subtype = IFM_40G_CR4,
		.baudrate = IF_Gbps(40ULL),
	},
	[MLX5E_40GBASE_KR4] = {
		.subtype = IFM_40G_KR4,
		.baudrate = IF_Gbps(40ULL),
	},
	[MLX5E_56GBASE_R4] = {
		.subtype = IFM_56G_R4,
		.baudrate = IF_Gbps(56ULL),
	},
	[MLX5E_10GBASE_CR] = {
		.subtype = IFM_10G_CR1,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_10GBASE_SR] = {
		.subtype = IFM_10G_SR,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_10GBASE_ER_LR] = {
		.subtype = IFM_10G_ER,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_40GBASE_SR4] = {
		.subtype = IFM_40G_SR4,
		.baudrate = IF_Gbps(40ULL),
	},
	[MLX5E_40GBASE_LR4_ER4] = {
		.subtype = IFM_40G_LR4,
		.baudrate = IF_Gbps(40ULL),
	},
	[MLX5E_100GBASE_CR4] = {
		.subtype = IFM_100G_CR4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100GBASE_SR4] = {
		.subtype = IFM_100G_SR4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100GBASE_KR4] = {
		.subtype = IFM_100G_KR4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100GBASE_LR4] = {
		.subtype = IFM_100G_LR4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100BASE_TX] = {
		.subtype = IFM_100_TX,
		.baudrate = IF_Mbps(100ULL),
	},
	[MLX5E_1000BASE_T] = {
		.subtype = IFM_1000_T,
		.baudrate = IF_Mbps(1000ULL),
	},
	[MLX5E_10GBASE_T] = {
		.subtype = IFM_10G_T,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_25GBASE_CR] = {
		.subtype = IFM_25G_CR,
		.baudrate = IF_Gbps(25ULL),
	},
	[MLX5E_25GBASE_KR] = {
		.subtype = IFM_25G_KR,
		.baudrate = IF_Gbps(25ULL),
	},
	[MLX5E_25GBASE_SR] = {
		.subtype = IFM_25G_SR,
		.baudrate = IF_Gbps(25ULL),
	},
	[MLX5E_50GBASE_CR2] = {
		.subtype = IFM_50G_CR2,
		.baudrate = IF_Gbps(50ULL),
	},
	[MLX5E_50GBASE_KR2] = {
		.subtype = IFM_50G_KR2,
		.baudrate = IF_Gbps(50ULL),
	},
	[MLX5E_50GBASE_KR4] = {
		.subtype = IFM_50G_KR4,
		.baudrate = IF_Gbps(50ULL),
	},
};

static const struct media mlx5e_ext_mode_table[MLX5E_EXT_LINK_SPEEDS_NUMBER][MLX5E_CABLE_TYPE_NUMBER] =
{
	/**/
	[MLX5E_SGMII_100M][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_100_SGMII,
		.baudrate = IF_Mbps(100),
	},

	/**/
	[MLX5E_1000BASE_X_SGMII][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_1000_CX,
		.baudrate = IF_Mbps(1000),
	},
	[MLX5E_1000BASE_X_SGMII][MLX5E_CABLE_TYPE_OPTICAL_MODULE] = {
		.subtype = IFM_1000_SX,
		.baudrate = IF_Mbps(1000),
	},

	/**/
	[MLX5E_5GBASE_R][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_5000_KR,
		.baudrate = IF_Mbps(5000),
	},
	[MLX5E_5GBASE_R][MLX5E_CABLE_TYPE_TWISTED_PAIR] = {
		.subtype = IFM_5000_T,
		.baudrate = IF_Mbps(5000),
	},

	/**/
	[MLX5E_10GBASE_XFI_XAUI_1][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_10G_KR,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_10GBASE_XFI_XAUI_1][MLX5E_CABLE_TYPE_PASSIVE_COPPER] = {
		.subtype = IFM_10G_CR1,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_10GBASE_XFI_XAUI_1][MLX5E_CABLE_TYPE_OPTICAL_MODULE] = {
		.subtype = IFM_10G_SR,
		.baudrate = IF_Gbps(10ULL),
	},

	/**/
	[MLX5E_40GBASE_XLAUI_4_XLPPI_4][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_40G_KR4,
		.baudrate = IF_Gbps(40ULL),
	},
	[MLX5E_40GBASE_XLAUI_4_XLPPI_4][MLX5E_CABLE_TYPE_PASSIVE_COPPER] = {
		.subtype = IFM_40G_CR4,
		.baudrate = IF_Gbps(40ULL),
	},
	[MLX5E_40GBASE_XLAUI_4_XLPPI_4][MLX5E_CABLE_TYPE_OPTICAL_MODULE] = {
		.subtype = IFM_40G_SR4,
		.baudrate = IF_Gbps(40ULL),
	},

	/**/
	[MLX5E_25GAUI_1_25GBASE_CR_KR][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_25G_KR,
		.baudrate = IF_Gbps(25ULL),
	},
	[MLX5E_25GAUI_1_25GBASE_CR_KR][MLX5E_CABLE_TYPE_PASSIVE_COPPER] = {
		.subtype = IFM_25G_CR,
		.baudrate = IF_Gbps(25ULL),
	},
	[MLX5E_25GAUI_1_25GBASE_CR_KR][MLX5E_CABLE_TYPE_OPTICAL_MODULE] = {
		.subtype = IFM_25G_SR,
		.baudrate = IF_Gbps(25ULL),
	},
	[MLX5E_25GAUI_1_25GBASE_CR_KR][MLX5E_CABLE_TYPE_TWISTED_PAIR] = {
		.subtype = IFM_25G_T,
		.baudrate = IF_Gbps(25ULL),
	},

	/**/
	[MLX5E_50GAUI_2_LAUI_2_50GBASE_CR2_KR2][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_50G_KR2,
		.baudrate = IF_Gbps(50ULL),
	},
	[MLX5E_50GAUI_2_LAUI_2_50GBASE_CR2_KR2][MLX5E_CABLE_TYPE_PASSIVE_COPPER] = {
		.subtype = IFM_50G_CR2,
		.baudrate = IF_Gbps(50ULL),
	},
	[MLX5E_50GAUI_2_LAUI_2_50GBASE_CR2_KR2][MLX5E_CABLE_TYPE_OPTICAL_MODULE] = {
		.subtype = IFM_50G_SR2,
		.baudrate = IF_Gbps(50ULL),
	},

	/**/
	[MLX5E_50GAUI_1_LAUI_1_50GBASE_CR_KR][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_50G_KR_PAM4,
		.baudrate = IF_Gbps(50ULL),
	},
	[MLX5E_50GAUI_1_LAUI_1_50GBASE_CR_KR][MLX5E_CABLE_TYPE_PASSIVE_COPPER] = {
		.subtype = IFM_50G_CP,
		.baudrate = IF_Gbps(50ULL),
	},
	[MLX5E_50GAUI_1_LAUI_1_50GBASE_CR_KR][MLX5E_CABLE_TYPE_OPTICAL_MODULE] = {
		.subtype = IFM_50G_SR,
		.baudrate = IF_Gbps(50ULL),
	},

	/**/
	[MLX5E_CAUI_4_100GBASE_CR4_KR4][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_100G_KR4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_CAUI_4_100GBASE_CR4_KR4][MLX5E_CABLE_TYPE_PASSIVE_COPPER] = {
		.subtype = IFM_100G_CR4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_CAUI_4_100GBASE_CR4_KR4][MLX5E_CABLE_TYPE_OPTICAL_MODULE] = {
		.subtype = IFM_100G_SR4,
		.baudrate = IF_Gbps(100ULL),
	},

	/**/
	[MLX5E_100GAUI_1_100GBASE_CR_KR][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_100G_KR_PAM4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100GAUI_1_100GBASE_CR_KR][MLX5E_CABLE_TYPE_PASSIVE_COPPER] = {
		.subtype = IFM_100G_CR_PAM4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100GAUI_1_100GBASE_CR_KR][MLX5E_CABLE_TYPE_OPTICAL_MODULE] = {
		.subtype = IFM_100G_SR2,	/* XXX */
		.baudrate = IF_Gbps(100ULL),
	},

	/**/
	[MLX5E_100GAUI_2_100GBASE_CR2_KR2][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_100G_KR4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100GAUI_2_100GBASE_CR2_KR2][MLX5E_CABLE_TYPE_PASSIVE_COPPER] = {
		.subtype = IFM_100G_CP2,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100GAUI_2_100GBASE_CR2_KR2][MLX5E_CABLE_TYPE_OPTICAL_MODULE] = {
		.subtype = IFM_100G_SR2,
		.baudrate = IF_Gbps(100ULL),
	},

	/**/
	[MLX5E_200GAUI_2_200GBASE_CR2_KR2][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_200G_KR4_PAM4,	/* XXX */
		.baudrate = IF_Gbps(200ULL),
	},
	[MLX5E_200GAUI_2_200GBASE_CR2_KR2][MLX5E_CABLE_TYPE_PASSIVE_COPPER] = {
		.subtype = IFM_200G_CR4_PAM4,	/* XXX */
		.baudrate = IF_Gbps(200ULL),
	},
	[MLX5E_200GAUI_2_200GBASE_CR2_KR2][MLX5E_CABLE_TYPE_OPTICAL_MODULE] = {
		.subtype = IFM_200G_SR4,	/* XXX */
		.baudrate = IF_Gbps(200ULL),
	},

	/**/
	[MLX5E_200GAUI_4_200GBASE_CR4_KR4][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_200G_KR4_PAM4,
		.baudrate = IF_Gbps(200ULL),
	},
	[MLX5E_200GAUI_4_200GBASE_CR4_KR4][MLX5E_CABLE_TYPE_PASSIVE_COPPER] = {
		.subtype = IFM_200G_CR4_PAM4,
		.baudrate = IF_Gbps(200ULL),
	},
	[MLX5E_200GAUI_4_200GBASE_CR4_KR4][MLX5E_CABLE_TYPE_OPTICAL_MODULE] = {
		.subtype = IFM_200G_SR4,
		.baudrate = IF_Gbps(200ULL),
	},

	/**/
	[MLX5E_400GAUI_8][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_400G_LR8,	/* XXX */
		.baudrate = IF_Gbps(400ULL),
	},

	/**/
	[MLX5E_400GAUI_4_400GBASE_CR4_KR4][MLX5E_CABLE_TYPE_UNKNOWN] = {
		.subtype = IFM_400G_LR8,	/* XXX */
		.baudrate = IF_Gbps(400ULL),
	},
};

static const struct if_snd_tag_sw mlx5e_ul_snd_tag_sw = {
	.snd_tag_query = mlx5e_ul_snd_tag_query,
	.snd_tag_free = mlx5e_ul_snd_tag_free,
	.type = IF_SND_TAG_TYPE_UNLIMITED
};

DEBUGNET_DEFINE(mlx5_en);

MALLOC_DEFINE(M_MLX5EN, "MLX5EN", "MLX5 Ethernet");

static void
mlx5e_update_carrier(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 out[MLX5_ST_SZ_DW(ptys_reg)];
	u32 eth_proto_oper;
	int error;
	u8 i;
	u8 cable_type;
	u8 port_state;
	u8 is_er_type;
	bool ext;
	struct media media_entry = {};

	port_state = mlx5_query_vport_state(mdev,
	    MLX5_QUERY_VPORT_STATE_IN_OP_MOD_VNIC_VPORT, 0);

	if (port_state == VPORT_STATE_UP) {
		priv->media_status_last |= IFM_ACTIVE;
	} else {
		priv->media_status_last &= ~IFM_ACTIVE;
		priv->media_active_last = IFM_ETHER;
		if_link_state_change(priv->ifp, LINK_STATE_DOWN);
		return;
	}

	error = mlx5_query_port_ptys(mdev, out, sizeof(out),
	    MLX5_PTYS_EN, 1);
	if (error) {
		priv->media_active_last = IFM_ETHER;
		priv->ifp->if_baudrate = 1;
		mlx5_en_err(priv->ifp, "query port ptys failed: 0x%x\n",
		    error);
		return;
	}

	ext = MLX5_CAP_PCAM_FEATURE(mdev, ptys_extended_ethernet);
	eth_proto_oper = MLX5_GET_ETH_PROTO(ptys_reg, out, ext,
	    eth_proto_oper);

	i = ilog2(eth_proto_oper);

	if (ext) {
		error = mlx5_query_pddr_cable_type(mdev, 1, &cable_type);
		if (error != 0) {
			/* use fallback entry */
			media_entry = mlx5e_ext_mode_table[i][MLX5E_CABLE_TYPE_UNKNOWN];

			mlx5_en_err(priv->ifp,
			    "query port pddr failed: %d\n", error);
		} else {
			media_entry = mlx5e_ext_mode_table[i][cable_type];

			/* check if we should use fallback entry */
			if (media_entry.subtype == 0)
				media_entry = mlx5e_ext_mode_table[i][MLX5E_CABLE_TYPE_UNKNOWN];
		}
	} else {
		media_entry = mlx5e_mode_table[i];
	}

	if (media_entry.subtype == 0) {
		mlx5_en_err(priv->ifp,
		    "Could not find operational media subtype\n");
		return;
	}

	switch (media_entry.subtype) {
	case IFM_10G_ER:
		error = mlx5_query_pddr_range_info(mdev, 1, &is_er_type);
		if (error != 0) {
			mlx5_en_err(priv->ifp,
			    "query port pddr failed: %d\n", error);
		}
		if (error != 0 || is_er_type == 0)
			media_entry.subtype = IFM_10G_LR;
		break;
	case IFM_40G_LR4:
		error = mlx5_query_pddr_range_info(mdev, 1, &is_er_type);
		if (error != 0) {
			mlx5_en_err(priv->ifp,
			    "query port pddr failed: %d\n", error);
		}
		if (error == 0 && is_er_type != 0)
			media_entry.subtype = IFM_40G_ER4;
		break;
	}
	priv->media_active_last = media_entry.subtype | IFM_ETHER | IFM_FDX;
	priv->ifp->if_baudrate = media_entry.baudrate;

	if_link_state_change(priv->ifp, LINK_STATE_UP);
}

static void
mlx5e_media_status(struct ifnet *dev, struct ifmediareq *ifmr)
{
	struct mlx5e_priv *priv = dev->if_softc;

	ifmr->ifm_status = priv->media_status_last;
	ifmr->ifm_current = ifmr->ifm_active = priv->media_active_last |
	    (priv->params.rx_pauseframe_control ? IFM_ETH_RXPAUSE : 0) |
	    (priv->params.tx_pauseframe_control ? IFM_ETH_TXPAUSE : 0);

}

static u32
mlx5e_find_link_mode(u32 subtype, bool ext)
{
	u32 link_mode = 0;

	switch (subtype) {
	case 0:
		goto done;
	case IFM_10G_LR:
		subtype = IFM_10G_ER;
		break;
	case IFM_40G_ER4:
		subtype = IFM_40G_LR4;
		break;
	default:
		break;
	}

	if (ext) {
		for (unsigned i = 0; i != MLX5E_EXT_LINK_SPEEDS_NUMBER; i++) {
			for (unsigned j = 0; j != MLX5E_CABLE_TYPE_NUMBER; j++) {
				if (mlx5e_ext_mode_table[i][j].subtype == subtype)
					link_mode |= MLX5E_PROT_MASK(i);
			}
		}
	} else {
		for (unsigned i = 0; i != MLX5E_LINK_SPEEDS_NUMBER; i++) {
			if (mlx5e_mode_table[i].subtype == subtype)
				link_mode |= MLX5E_PROT_MASK(i);
		}
	}
done:
	return (link_mode);
}

static int
mlx5e_set_port_pause_and_pfc(struct mlx5e_priv *priv)
{
	return (mlx5_set_port_pause_and_pfc(priv->mdev, 1,
	    priv->params.rx_pauseframe_control,
	    priv->params.tx_pauseframe_control,
	    priv->params.rx_priority_flow_control,
	    priv->params.tx_priority_flow_control));
}

static int
mlx5e_set_port_pfc(struct mlx5e_priv *priv)
{
	int error;

	if (priv->gone != 0) {
		error = -ENXIO;
	} else if (priv->params.rx_pauseframe_control ||
	    priv->params.tx_pauseframe_control) {
		mlx5_en_err(priv->ifp,
		    "Global pauseframes must be disabled before enabling PFC.\n");
		error = -EINVAL;
	} else {
		error = mlx5e_set_port_pause_and_pfc(priv);
	}
	return (error);
}

static int
mlx5e_media_change(struct ifnet *dev)
{
	struct mlx5e_priv *priv = dev->if_softc;
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 eth_proto_cap;
	u32 link_mode;
	u32 out[MLX5_ST_SZ_DW(ptys_reg)];
	int was_opened;
	int locked;
	int error;
	bool ext;

	locked = PRIV_LOCKED(priv);
	if (!locked)
		PRIV_LOCK(priv);

	if (IFM_TYPE(priv->media.ifm_media) != IFM_ETHER) {
		error = EINVAL;
		goto done;
	}

	error = mlx5_query_port_ptys(mdev, out, sizeof(out),
	    MLX5_PTYS_EN, 1);
	if (error != 0) {
		mlx5_en_err(dev, "Query port media capability failed\n");
		goto done;
	}

	ext = MLX5_CAP_PCAM_FEATURE(mdev, ptys_extended_ethernet);
	link_mode = mlx5e_find_link_mode(IFM_SUBTYPE(priv->media.ifm_media), ext);

	/* query supported capabilities */
	eth_proto_cap = MLX5_GET_ETH_PROTO(ptys_reg, out, ext,
	    eth_proto_capability);

	/* check for autoselect */
	if (IFM_SUBTYPE(priv->media.ifm_media) == IFM_AUTO) {
		link_mode = eth_proto_cap;
		if (link_mode == 0) {
			mlx5_en_err(dev, "Port media capability is zero\n");
			error = EINVAL;
			goto done;
		}
	} else {
		link_mode = link_mode & eth_proto_cap;
		if (link_mode == 0) {
			mlx5_en_err(dev, "Not supported link mode requested\n");
			error = EINVAL;
			goto done;
		}
	}
	if (priv->media.ifm_media & (IFM_ETH_RXPAUSE | IFM_ETH_TXPAUSE)) {
		/* check if PFC is enabled */
		if (priv->params.rx_priority_flow_control ||
		    priv->params.tx_priority_flow_control) {
			mlx5_en_err(dev, "PFC must be disabled before enabling global pauseframes.\n");
			error = EINVAL;
			goto done;
		}
	}
	/* update pauseframe control bits */
	priv->params.rx_pauseframe_control =
	    (priv->media.ifm_media & IFM_ETH_RXPAUSE) ? 1 : 0;
	priv->params.tx_pauseframe_control =
	    (priv->media.ifm_media & IFM_ETH_TXPAUSE) ? 1 : 0;

	/* check if device is opened */
	was_opened = test_bit(MLX5E_STATE_OPENED, &priv->state);

	/* reconfigure the hardware */
	mlx5_set_port_status(mdev, MLX5_PORT_DOWN);
	mlx5_set_port_proto(mdev, link_mode, MLX5_PTYS_EN, ext);
	error = -mlx5e_set_port_pause_and_pfc(priv);
	if (was_opened)
		mlx5_set_port_status(mdev, MLX5_PORT_UP);

done:
	if (!locked)
		PRIV_UNLOCK(priv);
	return (error);
}

static void
mlx5e_update_carrier_work(struct work_struct *work)
{
	struct mlx5e_priv *priv = container_of(work, struct mlx5e_priv,
	    update_carrier_work);

	PRIV_LOCK(priv);
	if (test_bit(MLX5E_STATE_OPENED, &priv->state))
		mlx5e_update_carrier(priv);
	PRIV_UNLOCK(priv);
}

#define	MLX5E_PCIE_PERF_GET_64(a,b,c,d,e,f)    \
	s_debug->c = MLX5_GET64(mpcnt_reg, out, counter_set.f.c);

#define	MLX5E_PCIE_PERF_GET_32(a,b,c,d,e,f)    \
	s_debug->c = MLX5_GET(mpcnt_reg, out, counter_set.f.c);

static void
mlx5e_update_pcie_counters(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5e_port_stats_debug *s_debug = &priv->stats.port_stats_debug;
	const unsigned sz = MLX5_ST_SZ_BYTES(mpcnt_reg);
	void *out;
	void *in;
	int err;

	/* allocate firmware request structures */
	in = mlx5_vzalloc(sz);
	out = mlx5_vzalloc(sz);
	if (in == NULL || out == NULL)
		goto free_out;

	MLX5_SET(mpcnt_reg, in, grp, MLX5_PCIE_PERFORMANCE_COUNTERS_GROUP);
	err = mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_MPCNT, 0, 0);
	if (err != 0)
		goto free_out;

	MLX5E_PCIE_PERFORMANCE_COUNTERS_64(MLX5E_PCIE_PERF_GET_64)
	MLX5E_PCIE_PERFORMANCE_COUNTERS_32(MLX5E_PCIE_PERF_GET_32)

	MLX5_SET(mpcnt_reg, in, grp, MLX5_PCIE_TIMERS_AND_STATES_COUNTERS_GROUP);
	err = mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_MPCNT, 0, 0);
	if (err != 0)
		goto free_out;

	MLX5E_PCIE_TIMERS_AND_STATES_COUNTERS_32(MLX5E_PCIE_PERF_GET_32)

	MLX5_SET(mpcnt_reg, in, grp, MLX5_PCIE_LANE_COUNTERS_GROUP);
	err = mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_MPCNT, 0, 0);
	if (err != 0)
		goto free_out;

	MLX5E_PCIE_LANE_COUNTERS_32(MLX5E_PCIE_PERF_GET_32)

free_out:
	/* free firmware request structures */
	kvfree(in);
	kvfree(out);
}

/*
 * This function reads the physical port counters from the firmware
 * using a pre-defined layout defined by various MLX5E_PPORT_XXX()
 * macros. The output is converted from big-endian 64-bit values into
 * host endian ones and stored in the "priv->stats.pport" structure.
 */
static void
mlx5e_update_pport_counters(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5e_pport_stats *s = &priv->stats.pport;
	struct mlx5e_port_stats_debug *s_debug = &priv->stats.port_stats_debug;
	u32 *in;
	u32 *out;
	const u64 *ptr;
	unsigned sz = MLX5_ST_SZ_BYTES(ppcnt_reg);
	unsigned x;
	unsigned y;
	unsigned z;

	/* allocate firmware request structures */
	in = mlx5_vzalloc(sz);
	out = mlx5_vzalloc(sz);
	if (in == NULL || out == NULL)
		goto free_out;

	/*
	 * Get pointer to the 64-bit counter set which is located at a
	 * fixed offset in the output firmware request structure:
	 */
	ptr = (const uint64_t *)MLX5_ADDR_OF(ppcnt_reg, out, counter_set);

	MLX5_SET(ppcnt_reg, in, local_port, 1);

	/* read IEEE802_3 counter group using predefined counter layout */
	MLX5_SET(ppcnt_reg, in, grp, MLX5_IEEE_802_3_COUNTERS_GROUP);
	mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPCNT, 0, 0);
	for (x = 0, y = MLX5E_PPORT_PER_PRIO_STATS_NUM;
	     x != MLX5E_PPORT_IEEE802_3_STATS_NUM; x++, y++)
		s->arg[y] = be64toh(ptr[x]);

	/* read RFC2819 counter group using predefined counter layout */
	MLX5_SET(ppcnt_reg, in, grp, MLX5_RFC_2819_COUNTERS_GROUP);
	mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPCNT, 0, 0);
	for (x = 0; x != MLX5E_PPORT_RFC2819_STATS_NUM; x++, y++)
		s->arg[y] = be64toh(ptr[x]);

	for (y = 0; x != MLX5E_PPORT_RFC2819_STATS_NUM +
	    MLX5E_PPORT_RFC2819_STATS_DEBUG_NUM; x++, y++)
		s_debug->arg[y] = be64toh(ptr[x]);

	/* read RFC2863 counter group using predefined counter layout */
	MLX5_SET(ppcnt_reg, in, grp, MLX5_RFC_2863_COUNTERS_GROUP);
	mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPCNT, 0, 0);
	for (x = 0; x != MLX5E_PPORT_RFC2863_STATS_DEBUG_NUM; x++, y++)
		s_debug->arg[y] = be64toh(ptr[x]);

	/* read physical layer stats counter group using predefined counter layout */
	MLX5_SET(ppcnt_reg, in, grp, MLX5_PHYSICAL_LAYER_COUNTERS_GROUP);
	mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPCNT, 0, 0);
	for (x = 0; x != MLX5E_PPORT_PHYSICAL_LAYER_STATS_DEBUG_NUM; x++, y++)
		s_debug->arg[y] = be64toh(ptr[x]);

	/* read Extended Ethernet counter group using predefined counter layout */
	MLX5_SET(ppcnt_reg, in, grp, MLX5_ETHERNET_EXTENDED_COUNTERS_GROUP);
	mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPCNT, 0, 0);
	for (x = 0; x != MLX5E_PPORT_ETHERNET_EXTENDED_STATS_DEBUG_NUM; x++, y++)
		s_debug->arg[y] = be64toh(ptr[x]);

	/* read Extended Statistical Group */
	if (MLX5_CAP_GEN(mdev, pcam_reg) &&
	    MLX5_CAP_PCAM_FEATURE(mdev, ppcnt_statistical_group) &&
	    MLX5_CAP_PCAM_FEATURE(mdev, per_lane_error_counters)) {
		/* read Extended Statistical counter group using predefined counter layout */
		MLX5_SET(ppcnt_reg, in, grp, MLX5_PHYSICAL_LAYER_STATISTICAL_GROUP);
		mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPCNT, 0, 0);

		for (x = 0; x != MLX5E_PPORT_STATISTICAL_DEBUG_NUM; x++, y++)
			s_debug->arg[y] = be64toh(ptr[x]);
	}

	/* read PCIE counters */
	mlx5e_update_pcie_counters(priv);

	/* read per-priority counters */
	MLX5_SET(ppcnt_reg, in, grp, MLX5_PER_PRIORITY_COUNTERS_GROUP);

	/* iterate all the priorities */
	for (y = z = 0; z != MLX5E_PPORT_PER_PRIO_STATS_NUM_PRIO; z++) {
		MLX5_SET(ppcnt_reg, in, prio_tc, z);
		mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPCNT, 0, 0);

		/* read per priority stats counter group using predefined counter layout */
		for (x = 0; x != (MLX5E_PPORT_PER_PRIO_STATS_NUM /
		    MLX5E_PPORT_PER_PRIO_STATS_NUM_PRIO); x++, y++)
			s->arg[y] = be64toh(ptr[x]);
	}

free_out:
	/* free firmware request structures */
	kvfree(in);
	kvfree(out);
}

static void
mlx5e_grp_vnic_env_update_stats(struct mlx5e_priv *priv)
{
	u32 out[MLX5_ST_SZ_DW(query_vnic_env_out)] = {};
	u32 in[MLX5_ST_SZ_DW(query_vnic_env_in)] = {};

	if (!MLX5_CAP_GEN(priv->mdev, nic_receive_steering_discard))
		return;

	MLX5_SET(query_vnic_env_in, in, opcode,
	    MLX5_CMD_OP_QUERY_VNIC_ENV);
	MLX5_SET(query_vnic_env_in, in, op_mod, 0);
	MLX5_SET(query_vnic_env_in, in, other_vport, 0);

	if (mlx5_cmd_exec(priv->mdev, in, sizeof(in), out, sizeof(out)) != 0)
		return;

	priv->stats.vport.rx_steer_missed_packets =
	    MLX5_GET64(query_vnic_env_out, out,
	    vport_env.nic_receive_steering_discard);
}

/*
 * This function is called regularly to collect all statistics
 * counters from the firmware. The values can be viewed through the
 * sysctl interface. Execution is serialized using the priv's global
 * configuration lock.
 */
static void
mlx5e_update_stats_locked(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5e_vport_stats *s = &priv->stats.vport;
	struct mlx5e_sq_stats *sq_stats;
#if (__FreeBSD_version < 1100000)
	struct ifnet *ifp = priv->ifp;
#endif

	u32 in[MLX5_ST_SZ_DW(query_vport_counter_in)];
	u32 *out;
	int outlen = MLX5_ST_SZ_BYTES(query_vport_counter_out);
	u64 tso_packets = 0;
	u64 tso_bytes = 0;
	u64 tx_queue_dropped = 0;
	u64 tx_defragged = 0;
	u64 tx_offload_none = 0;
	u64 lro_packets = 0;
	u64 lro_bytes = 0;
	u64 sw_lro_queued = 0;
	u64 sw_lro_flushed = 0;
	u64 rx_csum_none = 0;
	u64 rx_wqe_err = 0;
	u64 rx_packets = 0;
	u64 rx_bytes = 0;
	u32 rx_out_of_buffer = 0;
	int error;
	int i;
	int j;

	out = mlx5_vzalloc(outlen);
	if (out == NULL)
		goto free_out;

	/* Collect firts the SW counters and then HW for consistency */
	for (i = 0; i < priv->params.num_channels; i++) {
		struct mlx5e_channel *pch = priv->channel + i;
		struct mlx5e_rq *rq = &pch->rq;
		struct mlx5e_rq_stats *rq_stats = &pch->rq.stats;

		/* collect stats from LRO */
		rq_stats->sw_lro_queued = rq->lro.lro_queued;
		rq_stats->sw_lro_flushed = rq->lro.lro_flushed;
		sw_lro_queued += rq_stats->sw_lro_queued;
		sw_lro_flushed += rq_stats->sw_lro_flushed;
		lro_packets += rq_stats->lro_packets;
		lro_bytes += rq_stats->lro_bytes;
		rx_csum_none += rq_stats->csum_none;
		rx_wqe_err += rq_stats->wqe_err;
		rx_packets += rq_stats->packets;
		rx_bytes += rq_stats->bytes;

		for (j = 0; j < priv->num_tc; j++) {
			sq_stats = &pch->sq[j].stats;

			tso_packets += sq_stats->tso_packets;
			tso_bytes += sq_stats->tso_bytes;
			tx_queue_dropped += sq_stats->dropped;
			tx_queue_dropped += sq_stats->enobuf;
			tx_defragged += sq_stats->defragged;
			tx_offload_none += sq_stats->csum_offload_none;
		}
	}

#ifdef RATELIMIT
	/* Collect statistics from all rate-limit queues */
	for (j = 0; j < priv->rl.param.tx_worker_threads_def; j++) {
		struct mlx5e_rl_worker *rlw = priv->rl.workers + j;

		for (i = 0; i < priv->rl.param.tx_channels_per_worker_def; i++) {
			struct mlx5e_rl_channel *channel = rlw->channels + i;
			struct mlx5e_sq *sq = channel->sq;

			if (sq == NULL)
				continue;

			sq_stats = &sq->stats;

			tso_packets += sq_stats->tso_packets;
			tso_bytes += sq_stats->tso_bytes;
			tx_queue_dropped += sq_stats->dropped;
			tx_queue_dropped += sq_stats->enobuf;
			tx_defragged += sq_stats->defragged;
			tx_offload_none += sq_stats->csum_offload_none;
		}
	}
#endif

	/* update counters */
	s->tso_packets = tso_packets;
	s->tso_bytes = tso_bytes;
	s->tx_queue_dropped = tx_queue_dropped;
	s->tx_defragged = tx_defragged;
	s->lro_packets = lro_packets;
	s->lro_bytes = lro_bytes;
	s->sw_lro_queued = sw_lro_queued;
	s->sw_lro_flushed = sw_lro_flushed;
	s->rx_csum_none = rx_csum_none;
	s->rx_wqe_err = rx_wqe_err;
	s->rx_packets = rx_packets;
	s->rx_bytes = rx_bytes;

	mlx5e_grp_vnic_env_update_stats(priv);

	/* HW counters */
	memset(in, 0, sizeof(in));

	MLX5_SET(query_vport_counter_in, in, opcode,
	    MLX5_CMD_OP_QUERY_VPORT_COUNTER);
	MLX5_SET(query_vport_counter_in, in, op_mod, 0);
	MLX5_SET(query_vport_counter_in, in, other_vport, 0);

	memset(out, 0, outlen);

	/* get number of out-of-buffer drops first */
	if (test_bit(MLX5E_STATE_OPENED, &priv->state) != 0 &&
	    mlx5_vport_query_out_of_rx_buffer(mdev, priv->counter_set_id,
	    &rx_out_of_buffer) == 0) {
		s->rx_out_of_buffer = rx_out_of_buffer;
	}

	/* get port statistics */
	if (mlx5_cmd_exec(mdev, in, sizeof(in), out, outlen) == 0) {
#define	MLX5_GET_CTR(out, x) \
	MLX5_GET64(query_vport_counter_out, out, x)

		s->rx_error_packets =
		    MLX5_GET_CTR(out, received_errors.packets);
		s->rx_error_bytes =
		    MLX5_GET_CTR(out, received_errors.octets);
		s->tx_error_packets =
		    MLX5_GET_CTR(out, transmit_errors.packets);
		s->tx_error_bytes =
		    MLX5_GET_CTR(out, transmit_errors.octets);

		s->rx_unicast_packets =
		    MLX5_GET_CTR(out, received_eth_unicast.packets);
		s->rx_unicast_bytes =
		    MLX5_GET_CTR(out, received_eth_unicast.octets);
		s->tx_unicast_packets =
		    MLX5_GET_CTR(out, transmitted_eth_unicast.packets);
		s->tx_unicast_bytes =
		    MLX5_GET_CTR(out, transmitted_eth_unicast.octets);

		s->rx_multicast_packets =
		    MLX5_GET_CTR(out, received_eth_multicast.packets);
		s->rx_multicast_bytes =
		    MLX5_GET_CTR(out, received_eth_multicast.octets);
		s->tx_multicast_packets =
		    MLX5_GET_CTR(out, transmitted_eth_multicast.packets);
		s->tx_multicast_bytes =
		    MLX5_GET_CTR(out, transmitted_eth_multicast.octets);

		s->rx_broadcast_packets =
		    MLX5_GET_CTR(out, received_eth_broadcast.packets);
		s->rx_broadcast_bytes =
		    MLX5_GET_CTR(out, received_eth_broadcast.octets);
		s->tx_broadcast_packets =
		    MLX5_GET_CTR(out, transmitted_eth_broadcast.packets);
		s->tx_broadcast_bytes =
		    MLX5_GET_CTR(out, transmitted_eth_broadcast.octets);

		s->tx_packets = s->tx_unicast_packets +
		    s->tx_multicast_packets + s->tx_broadcast_packets;
		s->tx_bytes = s->tx_unicast_bytes + s->tx_multicast_bytes +
		    s->tx_broadcast_bytes;

		/* Update calculated offload counters */
		s->tx_csum_offload = s->tx_packets - tx_offload_none;
		s->rx_csum_good = s->rx_packets - s->rx_csum_none;
	}

	/* Get physical port counters */
	mlx5e_update_pport_counters(priv);

	s->tx_jumbo_packets =
	    priv->stats.port_stats_debug.tx_stat_p1519to2047octets +
	    priv->stats.port_stats_debug.tx_stat_p2048to4095octets +
	    priv->stats.port_stats_debug.tx_stat_p4096to8191octets +
	    priv->stats.port_stats_debug.tx_stat_p8192to10239octets;

#if (__FreeBSD_version < 1100000)
	/* no get_counters interface in fbsd 10 */
	ifp->if_ipackets = s->rx_packets;
	ifp->if_ierrors = priv->stats.pport.in_range_len_errors +
	    priv->stats.pport.out_of_range_len +
	    priv->stats.pport.too_long_errors +
	    priv->stats.pport.check_seq_err +
	    priv->stats.pport.alignment_err;
	ifp->if_iqdrops = s->rx_out_of_buffer;
	ifp->if_opackets = s->tx_packets;
	ifp->if_oerrors = priv->stats.port_stats_debug.out_discards;
	ifp->if_snd.ifq_drops = s->tx_queue_dropped;
	ifp->if_ibytes = s->rx_bytes;
	ifp->if_obytes = s->tx_bytes;
	ifp->if_collisions =
	    priv->stats.pport.collisions;
#endif

free_out:
	kvfree(out);

	/* Update diagnostics, if any */
	if (priv->params_ethtool.diag_pci_enable ||
	    priv->params_ethtool.diag_general_enable) {
		error = mlx5_core_get_diagnostics_full(mdev,
		    priv->params_ethtool.diag_pci_enable ? &priv->params_pci : NULL,
		    priv->params_ethtool.diag_general_enable ? &priv->params_general : NULL);
		if (error != 0)
			mlx5_en_err(priv->ifp,
			    "Failed reading diagnostics: %d\n", error);
	}

	/* Update FEC, if any */
	error = mlx5e_fec_update(priv);
	if (error != 0 && error != EOPNOTSUPP) {
		mlx5_en_err(priv->ifp,
		    "Updating FEC failed: %d\n", error);
	}

	/* Update temperature, if any */
	if (priv->params_ethtool.hw_num_temp != 0) {
		error = mlx5e_hw_temperature_update(priv);
		if (error != 0 && error != EOPNOTSUPP) {
			mlx5_en_err(priv->ifp,
			    "Updating temperature failed: %d\n", error);
		}
	}
}

static void
mlx5e_update_stats_work(struct work_struct *work)
{
	struct mlx5e_priv *priv;

	priv = container_of(work, struct mlx5e_priv, update_stats_work);
	PRIV_LOCK(priv);
	if (test_bit(MLX5E_STATE_OPENED, &priv->state) != 0 &&
	    !test_bit(MLX5_INTERFACE_STATE_TEARDOWN, &priv->mdev->intf_state))
		mlx5e_update_stats_locked(priv);
	PRIV_UNLOCK(priv);
}

static void
mlx5e_update_stats(void *arg)
{
	struct mlx5e_priv *priv = arg;

	queue_work(priv->wq, &priv->update_stats_work);

	callout_reset(&priv->watchdog, hz / 4, &mlx5e_update_stats, priv);
}

static void
mlx5e_async_event_sub(struct mlx5e_priv *priv,
    enum mlx5_dev_event event)
{
	switch (event) {
	case MLX5_DEV_EVENT_PORT_UP:
	case MLX5_DEV_EVENT_PORT_DOWN:
		queue_work(priv->wq, &priv->update_carrier_work);
		break;

	default:
		break;
	}
}

static void
mlx5e_async_event(struct mlx5_core_dev *mdev, void *vpriv,
    enum mlx5_dev_event event, unsigned long param)
{
	struct mlx5e_priv *priv = vpriv;

	mtx_lock(&priv->async_events_mtx);
	if (test_bit(MLX5E_STATE_ASYNC_EVENTS_ENABLE, &priv->state))
		mlx5e_async_event_sub(priv, event);
	mtx_unlock(&priv->async_events_mtx);
}

static void
mlx5e_enable_async_events(struct mlx5e_priv *priv)
{
	set_bit(MLX5E_STATE_ASYNC_EVENTS_ENABLE, &priv->state);
}

static void
mlx5e_disable_async_events(struct mlx5e_priv *priv)
{
	mtx_lock(&priv->async_events_mtx);
	clear_bit(MLX5E_STATE_ASYNC_EVENTS_ENABLE, &priv->state);
	mtx_unlock(&priv->async_events_mtx);
}

static void mlx5e_calibration_callout(void *arg);
static int mlx5e_calibration_duration = 20;
static int mlx5e_fast_calibration = 1;
static int mlx5e_normal_calibration = 30;

static SYSCTL_NODE(_hw_mlx5, OID_AUTO, calibr, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "MLX5 timestamp calibration parameters");

SYSCTL_INT(_hw_mlx5_calibr, OID_AUTO, duration, CTLFLAG_RWTUN,
    &mlx5e_calibration_duration, 0,
    "Duration of initial calibration");
SYSCTL_INT(_hw_mlx5_calibr, OID_AUTO, fast, CTLFLAG_RWTUN,
    &mlx5e_fast_calibration, 0,
    "Recalibration interval during initial calibration");
SYSCTL_INT(_hw_mlx5_calibr, OID_AUTO, normal, CTLFLAG_RWTUN,
    &mlx5e_normal_calibration, 0,
    "Recalibration interval during normal operations");

/*
 * Ignites the calibration process.
 */
static void
mlx5e_reset_calibration_callout(struct mlx5e_priv *priv)
{

	if (priv->clbr_done == 0)
		mlx5e_calibration_callout(priv);
	else
		callout_reset_sbt_curcpu(&priv->tstmp_clbr, (priv->clbr_done <
		    mlx5e_calibration_duration ? mlx5e_fast_calibration :
		    mlx5e_normal_calibration) * SBT_1S, 0,
		    mlx5e_calibration_callout, priv, C_DIRECT_EXEC);
}

static uint64_t
mlx5e_timespec2usec(const struct timespec *ts)
{

	return ((uint64_t)ts->tv_sec * 1000000000 + ts->tv_nsec);
}

static uint64_t
mlx5e_hw_clock(struct mlx5e_priv *priv)
{
	struct mlx5_init_seg *iseg;
	uint32_t hw_h, hw_h1, hw_l;

	iseg = priv->mdev->iseg;
	do {
		hw_h = ioread32be(&iseg->internal_timer_h);
		hw_l = ioread32be(&iseg->internal_timer_l);
		hw_h1 = ioread32be(&iseg->internal_timer_h);
	} while (hw_h1 != hw_h);
	return (((uint64_t)hw_h << 32) | hw_l);
}

/*
 * The calibration callout, it runs either in the context of the
 * thread which enables calibration, or in callout.  It takes the
 * snapshot of system and adapter clocks, then advances the pointers to
 * the calibration point to allow rx path to read the consistent data
 * lockless.
 */
static void
mlx5e_calibration_callout(void *arg)
{
	struct mlx5e_priv *priv;
	struct mlx5e_clbr_point *next, *curr;
	struct timespec ts;
	int clbr_curr_next;

	priv = arg;
	curr = &priv->clbr_points[priv->clbr_curr];
	clbr_curr_next = priv->clbr_curr + 1;
	if (clbr_curr_next >= nitems(priv->clbr_points))
		clbr_curr_next = 0;
	next = &priv->clbr_points[clbr_curr_next];

	next->base_prev = curr->base_curr;
	next->clbr_hw_prev = curr->clbr_hw_curr;

	next->clbr_hw_curr = mlx5e_hw_clock(priv);
	if (((next->clbr_hw_curr - curr->clbr_hw_curr) >> MLX5E_TSTMP_PREC) ==
	    0) {
		if (priv->clbr_done != 0) {
			mlx5_en_err(priv->ifp,
			    "HW failed tstmp frozen %#jx %#jx, disabling\n",
			     next->clbr_hw_curr, curr->clbr_hw_prev);
			priv->clbr_done = 0;
		}
		atomic_store_rel_int(&curr->clbr_gen, 0);
		return;
	}

	nanouptime(&ts);
	next->base_curr = mlx5e_timespec2usec(&ts);

	curr->clbr_gen = 0;
	atomic_thread_fence_rel();
	priv->clbr_curr = clbr_curr_next;
	atomic_store_rel_int(&next->clbr_gen, ++(priv->clbr_gen));

	if (priv->clbr_done < mlx5e_calibration_duration)
		priv->clbr_done++;
	mlx5e_reset_calibration_callout(priv);
}

static const char *mlx5e_rq_stats_desc[] = {
	MLX5E_RQ_STATS(MLX5E_STATS_DESC)
};

static int
mlx5e_create_rq(struct mlx5e_channel *c,
    struct mlx5e_rq_param *param,
    struct mlx5e_rq *rq)
{
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;
	char buffer[16];
	void *rqc = param->rqc;
	void *rqc_wq = MLX5_ADDR_OF(rqc, rqc, wq);
	int wq_sz;
	int err;
	int i;
	u32 nsegs, wqe_sz;

	err = mlx5e_get_wqe_sz(priv, &wqe_sz, &nsegs);
	if (err != 0)
		goto done;

	/* Create DMA descriptor TAG */
	if ((err = -bus_dma_tag_create(
	    bus_get_dma_tag(mdev->pdev->dev.bsddev),
	    1,				/* any alignment */
	    0,				/* no boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    nsegs * MLX5E_MAX_RX_BYTES,	/* maxsize */
	    nsegs,			/* nsegments */
	    nsegs * MLX5E_MAX_RX_BYTES,	/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &rq->dma_tag)))
		goto done;

	err = mlx5_wq_ll_create(mdev, &param->wq, rqc_wq, &rq->wq,
	    &rq->wq_ctrl);
	if (err)
		goto err_free_dma_tag;

	rq->wq.db = &rq->wq.db[MLX5_RCV_DBR];

	err = mlx5e_get_wqe_sz(priv, &rq->wqe_sz, &rq->nsegs);
	if (err != 0)
		goto err_rq_wq_destroy;

	wq_sz = mlx5_wq_ll_get_size(&rq->wq);

	err = -tcp_lro_init_args(&rq->lro, priv->ifp, TCP_LRO_ENTRIES, wq_sz);
	if (err)
		goto err_rq_wq_destroy;

	rq->mbuf = malloc_domainset(wq_sz * sizeof(rq->mbuf[0]), M_MLX5EN,
	    mlx5_dev_domainset(mdev), M_WAITOK | M_ZERO);
	for (i = 0; i != wq_sz; i++) {
		struct mlx5e_rx_wqe *wqe = mlx5_wq_ll_get_wqe(&rq->wq, i);
		int j;

		err = -bus_dmamap_create(rq->dma_tag, 0, &rq->mbuf[i].dma_map);
		if (err != 0) {
			while (i--)
				bus_dmamap_destroy(rq->dma_tag, rq->mbuf[i].dma_map);
			goto err_rq_mbuf_free;
		}

		/* set value for constant fields */
		for (j = 0; j < rq->nsegs; j++)
			wqe->data[j].lkey = cpu_to_be32(priv->mr.key);
	}

	INIT_WORK(&rq->dim.work, mlx5e_dim_work);
	if (priv->params.rx_cq_moderation_mode < 2) {
		rq->dim.mode = NET_DIM_CQ_PERIOD_MODE_DISABLED;
	} else {
		void *cqc = container_of(param,
		    struct mlx5e_channel_param, rq)->rx_cq.cqc;

		switch (MLX5_GET(cqc, cqc, cq_period_mode)) {
		case MLX5_CQ_PERIOD_MODE_START_FROM_EQE:
			rq->dim.mode = NET_DIM_CQ_PERIOD_MODE_START_FROM_EQE;
			break;
		case MLX5_CQ_PERIOD_MODE_START_FROM_CQE:
			rq->dim.mode = NET_DIM_CQ_PERIOD_MODE_START_FROM_CQE;
			break;
		default:
			rq->dim.mode = NET_DIM_CQ_PERIOD_MODE_DISABLED;
			break;
		}
	}

	rq->ifp = priv->ifp;
	rq->channel = c;
	rq->ix = c->ix;

	snprintf(buffer, sizeof(buffer), "rxstat%d", c->ix);
	mlx5e_create_stats(&rq->stats.ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    buffer, mlx5e_rq_stats_desc, MLX5E_RQ_STATS_NUM,
	    rq->stats.arg);
	return (0);

err_rq_mbuf_free:
	free(rq->mbuf, M_MLX5EN);
	tcp_lro_free(&rq->lro);
err_rq_wq_destroy:
	mlx5_wq_destroy(&rq->wq_ctrl);
err_free_dma_tag:
	bus_dma_tag_destroy(rq->dma_tag);
done:
	return (err);
}

static void
mlx5e_destroy_rq(struct mlx5e_rq *rq)
{
	int wq_sz;
	int i;

	/* destroy all sysctl nodes */
	sysctl_ctx_free(&rq->stats.ctx);

	/* free leftover LRO packets, if any */
	tcp_lro_free(&rq->lro);

	wq_sz = mlx5_wq_ll_get_size(&rq->wq);
	for (i = 0; i != wq_sz; i++) {
		if (rq->mbuf[i].mbuf != NULL) {
			bus_dmamap_unload(rq->dma_tag, rq->mbuf[i].dma_map);
			m_freem(rq->mbuf[i].mbuf);
		}
		bus_dmamap_destroy(rq->dma_tag, rq->mbuf[i].dma_map);
	}
	free(rq->mbuf, M_MLX5EN);
	mlx5_wq_destroy(&rq->wq_ctrl);
	bus_dma_tag_destroy(rq->dma_tag);
}

static int
mlx5e_enable_rq(struct mlx5e_rq *rq, struct mlx5e_rq_param *param)
{
	struct mlx5e_channel *c = rq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;
	void *in;
	void *rqc;
	void *wq;
	int inlen;
	int err;
	u8 ts_format;

	inlen = MLX5_ST_SZ_BYTES(create_rq_in) +
	    sizeof(u64) * rq->wq_ctrl.buf.npages;
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	ts_format = mlx5_get_rq_default_ts(mdev);
	rqc = MLX5_ADDR_OF(create_rq_in, in, ctx);
	wq = MLX5_ADDR_OF(rqc, rqc, wq);

	memcpy(rqc, param->rqc, sizeof(param->rqc));

	MLX5_SET(rqc, rqc, cqn, c->rq.cq.mcq.cqn);
	MLX5_SET(rqc, rqc, state, MLX5_RQC_STATE_RST);
	MLX5_SET(rqc, rqc, ts_format, ts_format);
	MLX5_SET(rqc, rqc, flush_in_error_en, 1);
	if (priv->counter_set_id >= 0)
		MLX5_SET(rqc, rqc, counter_set_id, priv->counter_set_id);
	MLX5_SET(wq, wq, log_wq_pg_sz, rq->wq_ctrl.buf.page_shift -
	    PAGE_SHIFT);
	MLX5_SET64(wq, wq, dbr_addr, rq->wq_ctrl.db.dma);

	mlx5_fill_page_array(&rq->wq_ctrl.buf,
	    (__be64 *) MLX5_ADDR_OF(wq, wq, pas));

	err = mlx5_core_create_rq(mdev, in, inlen, &rq->rqn);

	kvfree(in);

	return (err);
}

static int
mlx5e_modify_rq(struct mlx5e_rq *rq, int curr_state, int next_state)
{
	struct mlx5e_channel *c = rq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;

	void *in;
	void *rqc;
	int inlen;
	int err;

	inlen = MLX5_ST_SZ_BYTES(modify_rq_in);
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	rqc = MLX5_ADDR_OF(modify_rq_in, in, ctx);

	MLX5_SET(modify_rq_in, in, rqn, rq->rqn);
	MLX5_SET(modify_rq_in, in, rq_state, curr_state);
	MLX5_SET(rqc, rqc, state, next_state);

	err = mlx5_core_modify_rq(mdev, in, inlen);

	kvfree(in);

	return (err);
}

static void
mlx5e_disable_rq(struct mlx5e_rq *rq)
{
	struct mlx5e_channel *c = rq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;

	mlx5_core_destroy_rq(mdev, rq->rqn);
}

static int
mlx5e_wait_for_min_rx_wqes(struct mlx5e_rq *rq)
{
	struct mlx5e_channel *c = rq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_wq_ll *wq = &rq->wq;
	int i;

	for (i = 0; i < 1000; i++) {
		if (wq->cur_sz >= priv->params.min_rx_wqes)
			return (0);

		msleep(4);
	}
	return (-ETIMEDOUT);
}

static int
mlx5e_open_rq(struct mlx5e_channel *c,
    struct mlx5e_rq_param *param,
    struct mlx5e_rq *rq)
{
	int err;

	err = mlx5e_create_rq(c, param, rq);
	if (err)
		return (err);

	err = mlx5e_enable_rq(rq, param);
	if (err)
		goto err_destroy_rq;

	err = mlx5e_modify_rq(rq, MLX5_RQC_STATE_RST, MLX5_RQC_STATE_RDY);
	if (err)
		goto err_disable_rq;

	c->rq.enabled = 1;

	return (0);

err_disable_rq:
	mlx5e_disable_rq(rq);
err_destroy_rq:
	mlx5e_destroy_rq(rq);

	return (err);
}

static void
mlx5e_close_rq(struct mlx5e_rq *rq)
{
	mtx_lock(&rq->mtx);
	rq->enabled = 0;
	callout_stop(&rq->watchdog);
	mtx_unlock(&rq->mtx);

	mlx5e_modify_rq(rq, MLX5_RQC_STATE_RDY, MLX5_RQC_STATE_ERR);
}

static void
mlx5e_close_rq_wait(struct mlx5e_rq *rq)
{

	mlx5e_disable_rq(rq);
	mlx5e_close_cq(&rq->cq);
	cancel_work_sync(&rq->dim.work);
	mlx5e_destroy_rq(rq);
}

void
mlx5e_free_sq_db(struct mlx5e_sq *sq)
{
	int wq_sz = mlx5_wq_cyc_get_size(&sq->wq);
	int x;

	for (x = 0; x != wq_sz; x++) {
		if (unlikely(sq->mbuf[x].p_refcount != NULL)) {
			atomic_add_int(sq->mbuf[x].p_refcount, -1);
			sq->mbuf[x].p_refcount = NULL;
		}
		if (sq->mbuf[x].mbuf != NULL) {
			bus_dmamap_unload(sq->dma_tag, sq->mbuf[x].dma_map);
			m_freem(sq->mbuf[x].mbuf);
		}
		bus_dmamap_destroy(sq->dma_tag, sq->mbuf[x].dma_map);
	}
	free(sq->mbuf, M_MLX5EN);
}

int
mlx5e_alloc_sq_db(struct mlx5e_sq *sq)
{
	int wq_sz = mlx5_wq_cyc_get_size(&sq->wq);
	int err;
	int x;

	sq->mbuf = malloc_domainset(wq_sz * sizeof(sq->mbuf[0]), M_MLX5EN,
	    mlx5_dev_domainset(sq->priv->mdev), M_WAITOK | M_ZERO);

	/* Create DMA descriptor MAPs */
	for (x = 0; x != wq_sz; x++) {
		err = -bus_dmamap_create(sq->dma_tag, 0, &sq->mbuf[x].dma_map);
		if (err != 0) {
			while (x--)
				bus_dmamap_destroy(sq->dma_tag, sq->mbuf[x].dma_map);
			free(sq->mbuf, M_MLX5EN);
			return (err);
		}
	}
	return (0);
}

static const char *mlx5e_sq_stats_desc[] = {
	MLX5E_SQ_STATS(MLX5E_STATS_DESC)
};

void
mlx5e_update_sq_inline(struct mlx5e_sq *sq)
{
	sq->max_inline = sq->priv->params.tx_max_inline;
	sq->min_inline_mode = sq->priv->params.tx_min_inline_mode;

	/*
	 * Check if trust state is DSCP or if inline mode is NONE which
	 * indicates CX-5 or newer hardware.
	 */
	if (sq->priv->params_ethtool.trust_state != MLX5_QPTS_TRUST_PCP ||
	    sq->min_inline_mode == MLX5_INLINE_MODE_NONE) {
		if (MLX5_CAP_ETH(sq->priv->mdev, wqe_vlan_insert))
			sq->min_insert_caps = MLX5E_INSERT_VLAN | MLX5E_INSERT_NON_VLAN;
		else
			sq->min_insert_caps = MLX5E_INSERT_NON_VLAN;
	} else {
		sq->min_insert_caps = 0;
	}
}

static void
mlx5e_refresh_sq_inline_sub(struct mlx5e_priv *priv, struct mlx5e_channel *c)
{
	int i;

	for (i = 0; i != priv->num_tc; i++) {
		mtx_lock(&c->sq[i].lock);
		mlx5e_update_sq_inline(&c->sq[i]);
		mtx_unlock(&c->sq[i].lock);
	}
}

void
mlx5e_refresh_sq_inline(struct mlx5e_priv *priv)
{
	int i;

	/* check if channels are closed */
	if (test_bit(MLX5E_STATE_OPENED, &priv->state) == 0)
		return;

	for (i = 0; i < priv->params.num_channels; i++)
		mlx5e_refresh_sq_inline_sub(priv, &priv->channel[i]);
}

static int
mlx5e_create_sq(struct mlx5e_channel *c,
    int tc,
    struct mlx5e_sq_param *param,
    struct mlx5e_sq *sq)
{
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;
	char buffer[16];
	void *sqc = param->sqc;
	void *sqc_wq = MLX5_ADDR_OF(sqc, sqc, wq);
	int err;

	/* Create DMA descriptor TAG */
	if ((err = -bus_dma_tag_create(
	    bus_get_dma_tag(mdev->pdev->dev.bsddev),
	    1,				/* any alignment */
	    0,				/* no boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MLX5E_MAX_TX_PAYLOAD_SIZE,	/* maxsize */
	    MLX5E_MAX_TX_MBUF_FRAGS,	/* nsegments */
	    MLX5E_MAX_TX_MBUF_SIZE,	/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &sq->dma_tag)))
		goto done;

	sq->mkey_be = cpu_to_be32(priv->mr.key);
	sq->ifp = priv->ifp;
	sq->priv = priv;
	sq->tc = tc;

	err = mlx5_wq_cyc_create(mdev, &param->wq, sqc_wq, &sq->wq,
	    &sq->wq_ctrl);
	if (err)
		goto err_free_dma_tag;

	sq->wq.db = &sq->wq.db[MLX5_SND_DBR];

	err = mlx5e_alloc_sq_db(sq);
	if (err)
		goto err_sq_wq_destroy;

	mlx5e_update_sq_inline(sq);

	snprintf(buffer, sizeof(buffer), "txstat%dtc%d", c->ix, tc);
	mlx5e_create_stats(&sq->stats.ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    buffer, mlx5e_sq_stats_desc, MLX5E_SQ_STATS_NUM,
	    sq->stats.arg);

	return (0);

err_sq_wq_destroy:
	mlx5_wq_destroy(&sq->wq_ctrl);

err_free_dma_tag:
	bus_dma_tag_destroy(sq->dma_tag);
done:
	return (err);
}

static void
mlx5e_destroy_sq(struct mlx5e_sq *sq)
{
	/* destroy all sysctl nodes */
	sysctl_ctx_free(&sq->stats.ctx);

	mlx5e_free_sq_db(sq);
	mlx5_wq_destroy(&sq->wq_ctrl);
	bus_dma_tag_destroy(sq->dma_tag);
}

int
mlx5e_enable_sq(struct mlx5e_sq *sq, struct mlx5e_sq_param *param,
    const struct mlx5_sq_bfreg *bfreg, int tis_num)
{
	void *in;
	void *sqc;
	void *wq;
	int inlen;
	int err;
	u8 ts_format;

	inlen = MLX5_ST_SZ_BYTES(create_sq_in) +
	    sizeof(u64) * sq->wq_ctrl.buf.npages;
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	sq->uar_map = bfreg->map;

	ts_format = mlx5_get_sq_default_ts(sq->priv->mdev);
	sqc = MLX5_ADDR_OF(create_sq_in, in, ctx);
	wq = MLX5_ADDR_OF(sqc, sqc, wq);

	memcpy(sqc, param->sqc, sizeof(param->sqc));

	MLX5_SET(sqc, sqc, tis_num_0, tis_num);
	MLX5_SET(sqc, sqc, cqn, sq->cq.mcq.cqn);
	MLX5_SET(sqc, sqc, state, MLX5_SQC_STATE_RST);
	MLX5_SET(sqc, sqc, ts_format, ts_format);
	MLX5_SET(sqc, sqc, tis_lst_sz, 1);
	MLX5_SET(sqc, sqc, flush_in_error_en, 1);
	MLX5_SET(sqc, sqc, allow_swp, 1);

	MLX5_SET(wq, wq, wq_type, MLX5_WQ_TYPE_CYCLIC);
	MLX5_SET(wq, wq, uar_page, bfreg->index);
	MLX5_SET(wq, wq, log_wq_pg_sz, sq->wq_ctrl.buf.page_shift -
	    PAGE_SHIFT);
	MLX5_SET64(wq, wq, dbr_addr, sq->wq_ctrl.db.dma);

	mlx5_fill_page_array(&sq->wq_ctrl.buf,
	    (__be64 *) MLX5_ADDR_OF(wq, wq, pas));

	err = mlx5_core_create_sq(sq->priv->mdev, in, inlen, &sq->sqn);

	kvfree(in);

	return (err);
}

int
mlx5e_modify_sq(struct mlx5e_sq *sq, int curr_state, int next_state)
{
	void *in;
	void *sqc;
	int inlen;
	int err;

	inlen = MLX5_ST_SZ_BYTES(modify_sq_in);
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	sqc = MLX5_ADDR_OF(modify_sq_in, in, ctx);

	MLX5_SET(modify_sq_in, in, sqn, sq->sqn);
	MLX5_SET(modify_sq_in, in, sq_state, curr_state);
	MLX5_SET(sqc, sqc, state, next_state);

	err = mlx5_core_modify_sq(sq->priv->mdev, in, inlen);

	kvfree(in);

	return (err);
}

void
mlx5e_disable_sq(struct mlx5e_sq *sq)
{

	mlx5_core_destroy_sq(sq->priv->mdev, sq->sqn);
}

static int
mlx5e_open_sq(struct mlx5e_channel *c,
    int tc,
    struct mlx5e_sq_param *param,
    struct mlx5e_sq *sq)
{
	int err;

	sq->cev_factor = c->priv->params_ethtool.tx_completion_fact;

	/* ensure the TX completion event factor is not zero */
	if (sq->cev_factor == 0)
		sq->cev_factor = 1;

	err = mlx5e_create_sq(c, tc, param, sq);
	if (err)
		return (err);

	err = mlx5e_enable_sq(sq, param, &c->bfreg, c->priv->tisn[tc]);
	if (err)
		goto err_destroy_sq;

	err = mlx5e_modify_sq(sq, MLX5_SQC_STATE_RST, MLX5_SQC_STATE_RDY);
	if (err)
		goto err_disable_sq;

	WRITE_ONCE(sq->running, 1);

	return (0);

err_disable_sq:
	mlx5e_disable_sq(sq);
err_destroy_sq:
	mlx5e_destroy_sq(sq);

	return (err);
}

static void
mlx5e_sq_send_nops_locked(struct mlx5e_sq *sq, int can_sleep)
{
	/* fill up remainder with NOPs */
	while (sq->cev_counter != 0) {
		while (!mlx5e_sq_has_room_for(sq, 1)) {
			if (can_sleep != 0) {
				mtx_unlock(&sq->lock);
				msleep(4);
				mtx_lock(&sq->lock);
			} else {
				goto done;
			}
		}
		/* send a single NOP */
		mlx5e_send_nop(sq, 1);
		atomic_thread_fence_rel();
	}
done:
	/* Check if we need to write the doorbell */
	if (likely(sq->doorbell.d64 != 0)) {
		mlx5e_tx_notify_hw(sq, sq->doorbell.d32);
		sq->doorbell.d64 = 0;
	}
}

void
mlx5e_sq_cev_timeout(void *arg)
{
	struct mlx5e_sq *sq = arg;

	mtx_assert(&sq->lock, MA_OWNED);

	/* check next state */
	switch (sq->cev_next_state) {
	case MLX5E_CEV_STATE_SEND_NOPS:
		/* fill TX ring with NOPs, if any */
		mlx5e_sq_send_nops_locked(sq, 0);

		/* check if completed */
		if (sq->cev_counter == 0) {
			sq->cev_next_state = MLX5E_CEV_STATE_INITIAL;
			return;
		}
		break;
	default:
		/* send NOPs on next timeout */
		sq->cev_next_state = MLX5E_CEV_STATE_SEND_NOPS;
		break;
	}

	/* restart timer */
	callout_reset_curcpu(&sq->cev_callout, hz, mlx5e_sq_cev_timeout, sq);
}

void
mlx5e_drain_sq(struct mlx5e_sq *sq)
{
	int error;
	struct mlx5_core_dev *mdev= sq->priv->mdev;

	/*
	 * Check if already stopped.
	 *
	 * NOTE: Serialization of this function is managed by the
	 * caller ensuring the priv's state lock is locked or in case
	 * of rate limit support, a single thread manages drain and
	 * resume of SQs. The "running" variable can therefore safely
	 * be read without any locks.
	 */
	if (READ_ONCE(sq->running) == 0)
		return;

	/* don't put more packets into the SQ */
	WRITE_ONCE(sq->running, 0);

	/* serialize access to DMA rings */
	mtx_lock(&sq->lock);

	/* teardown event factor timer, if any */
	sq->cev_next_state = MLX5E_CEV_STATE_HOLD_NOPS;
	callout_stop(&sq->cev_callout);

	/* send dummy NOPs in order to flush the transmit ring */
	mlx5e_sq_send_nops_locked(sq, 1);
	mtx_unlock(&sq->lock);

	/* wait till SQ is empty or link is down */
	mtx_lock(&sq->lock);
	while (sq->cc != sq->pc &&
	    (sq->priv->media_status_last & IFM_ACTIVE) != 0 &&
	    mdev->state != MLX5_DEVICE_STATE_INTERNAL_ERROR &&
	    pci_channel_offline(mdev->pdev) == 0) {
		mtx_unlock(&sq->lock);
		msleep(1);
		sq->cq.mcq.comp(&sq->cq.mcq, NULL);
		mtx_lock(&sq->lock);
	}
	mtx_unlock(&sq->lock);

	/* error out remaining requests */
	error = mlx5e_modify_sq(sq, MLX5_SQC_STATE_RDY, MLX5_SQC_STATE_ERR);
	if (error != 0) {
		mlx5_en_err(sq->ifp,
		    "mlx5e_modify_sq() from RDY to ERR failed: %d\n", error);
	}

	/* wait till SQ is empty */
	mtx_lock(&sq->lock);
	while (sq->cc != sq->pc &&
	       mdev->state != MLX5_DEVICE_STATE_INTERNAL_ERROR &&
	       pci_channel_offline(mdev->pdev) == 0) {
		mtx_unlock(&sq->lock);
		msleep(1);
		sq->cq.mcq.comp(&sq->cq.mcq, NULL);
		mtx_lock(&sq->lock);
	}
	mtx_unlock(&sq->lock);
}

static void
mlx5e_close_sq_wait(struct mlx5e_sq *sq)
{

	mlx5e_drain_sq(sq);
	mlx5e_disable_sq(sq);
	mlx5e_destroy_sq(sq);
}

static int
mlx5e_create_cq(struct mlx5e_priv *priv,
    struct mlx5e_cq_param *param,
    struct mlx5e_cq *cq,
    mlx5e_cq_comp_t *comp,
    int eq_ix)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5_core_cq *mcq = &cq->mcq;
	int eqn_not_used;
	int irqn;
	int err;
	u32 i;

	err = mlx5_vector2eqn(mdev, eq_ix, &eqn_not_used, &irqn);
	if (err)
		return (err);

	err = mlx5_cqwq_create(mdev, &param->wq, param->cqc, &cq->wq,
	    &cq->wq_ctrl);
	if (err)
		return (err);

	mcq->cqe_sz = 64;
	mcq->set_ci_db = cq->wq_ctrl.db.db;
	mcq->arm_db = cq->wq_ctrl.db.db + 1;
	*mcq->set_ci_db = 0;
	*mcq->arm_db = 0;
	mcq->vector = eq_ix;
	mcq->comp = comp;
	mcq->event = mlx5e_cq_error_event;
	mcq->irqn = irqn;

	for (i = 0; i < mlx5_cqwq_get_size(&cq->wq); i++) {
		struct mlx5_cqe64 *cqe = mlx5_cqwq_get_wqe(&cq->wq, i);

		cqe->op_own = 0xf1;
	}

	cq->priv = priv;

	return (0);
}

static void
mlx5e_destroy_cq(struct mlx5e_cq *cq)
{
	mlx5_wq_destroy(&cq->wq_ctrl);
}

static int
mlx5e_enable_cq(struct mlx5e_cq *cq, struct mlx5e_cq_param *param, int eq_ix)
{
	struct mlx5_core_cq *mcq = &cq->mcq;
	u32 out[MLX5_ST_SZ_DW(create_cq_out)];
	void *in;
	void *cqc;
	int inlen;
	int irqn_not_used;
	int eqn;
	int err;

	inlen = MLX5_ST_SZ_BYTES(create_cq_in) +
	    sizeof(u64) * cq->wq_ctrl.buf.npages;
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	cqc = MLX5_ADDR_OF(create_cq_in, in, cq_context);

	memcpy(cqc, param->cqc, sizeof(param->cqc));

	mlx5_fill_page_array(&cq->wq_ctrl.buf,
	    (__be64 *) MLX5_ADDR_OF(create_cq_in, in, pas));

	mlx5_vector2eqn(cq->priv->mdev, eq_ix, &eqn, &irqn_not_used);

	MLX5_SET(cqc, cqc, c_eqn, eqn);
	MLX5_SET(cqc, cqc, log_page_size, cq->wq_ctrl.buf.page_shift -
	    PAGE_SHIFT);
	MLX5_SET64(cqc, cqc, dbr_addr, cq->wq_ctrl.db.dma);

	err = mlx5_core_create_cq(cq->priv->mdev, mcq, in, inlen, out, sizeof(out));

	kvfree(in);

	if (err)
		return (err);

	mlx5e_cq_arm(cq, MLX5_GET_DOORBELL_LOCK(&cq->priv->doorbell_lock));

	return (0);
}

static void
mlx5e_disable_cq(struct mlx5e_cq *cq)
{

	mlx5_core_destroy_cq(cq->priv->mdev, &cq->mcq);
}

int
mlx5e_open_cq(struct mlx5e_priv *priv,
    struct mlx5e_cq_param *param,
    struct mlx5e_cq *cq,
    mlx5e_cq_comp_t *comp,
    int eq_ix)
{
	int err;

	err = mlx5e_create_cq(priv, param, cq, comp, eq_ix);
	if (err)
		return (err);

	err = mlx5e_enable_cq(cq, param, eq_ix);
	if (err)
		goto err_destroy_cq;

	return (0);

err_destroy_cq:
	mlx5e_destroy_cq(cq);

	return (err);
}

void
mlx5e_close_cq(struct mlx5e_cq *cq)
{
	mlx5e_disable_cq(cq);
	mlx5e_destroy_cq(cq);
}

static int
mlx5e_open_tx_cqs(struct mlx5e_channel *c,
    struct mlx5e_channel_param *cparam)
{
	int err;
	int tc;

	for (tc = 0; tc < c->priv->num_tc; tc++) {
		/* open completion queue */
		err = mlx5e_open_cq(c->priv, &cparam->tx_cq, &c->sq[tc].cq,
		    &mlx5e_tx_cq_comp, c->ix);
		if (err)
			goto err_close_tx_cqs;
	}
	return (0);

err_close_tx_cqs:
	for (tc--; tc >= 0; tc--)
		mlx5e_close_cq(&c->sq[tc].cq);

	return (err);
}

static void
mlx5e_close_tx_cqs(struct mlx5e_channel *c)
{
	int tc;

	for (tc = 0; tc < c->priv->num_tc; tc++)
		mlx5e_close_cq(&c->sq[tc].cq);
}

static int
mlx5e_open_sqs(struct mlx5e_channel *c,
    struct mlx5e_channel_param *cparam)
{
	int err;
	int tc;

	for (tc = 0; tc < c->priv->num_tc; tc++) {
		err = mlx5e_open_sq(c, tc, &cparam->sq, &c->sq[tc]);
		if (err)
			goto err_close_sqs;
	}

	return (0);

err_close_sqs:
	for (tc--; tc >= 0; tc--)
		mlx5e_close_sq_wait(&c->sq[tc]);

	return (err);
}

static void
mlx5e_close_sqs_wait(struct mlx5e_channel *c)
{
	int tc;

	for (tc = 0; tc < c->priv->num_tc; tc++)
		mlx5e_close_sq_wait(&c->sq[tc]);
}

static void
mlx5e_chan_static_init(struct mlx5e_priv *priv, struct mlx5e_channel *c, int ix)
{
	int tc;

	/* setup priv and channel number */
	c->priv = priv;
	c->ix = ix;

	/* setup send tag */
	m_snd_tag_init(&c->tag, c->priv->ifp, &mlx5e_ul_snd_tag_sw);

	init_completion(&c->completion);

	mtx_init(&c->rq.mtx, "mlx5rx", MTX_NETWORK_LOCK, MTX_DEF);

	callout_init_mtx(&c->rq.watchdog, &c->rq.mtx, 0);

	for (tc = 0; tc != MLX5E_MAX_TX_NUM_TC; tc++) {
		struct mlx5e_sq *sq = c->sq + tc;

		mtx_init(&sq->lock, "mlx5tx",
		    MTX_NETWORK_LOCK " TX", MTX_DEF);
		mtx_init(&sq->comp_lock, "mlx5comp",
		    MTX_NETWORK_LOCK " TX", MTX_DEF);

		callout_init_mtx(&sq->cev_callout, &sq->lock, 0);
	}
}

static void
mlx5e_chan_wait_for_completion(struct mlx5e_channel *c)
{

	m_snd_tag_rele(&c->tag);
	wait_for_completion(&c->completion);
}

static void
mlx5e_priv_wait_for_completion(struct mlx5e_priv *priv, const uint32_t channels)
{
	uint32_t x;

	for (x = 0; x != channels; x++)
		mlx5e_chan_wait_for_completion(&priv->channel[x]);
}

static void
mlx5e_chan_static_destroy(struct mlx5e_channel *c)
{
	int tc;

	callout_drain(&c->rq.watchdog);

	mtx_destroy(&c->rq.mtx);

	for (tc = 0; tc != MLX5E_MAX_TX_NUM_TC; tc++) {
		callout_drain(&c->sq[tc].cev_callout);
		mtx_destroy(&c->sq[tc].lock);
		mtx_destroy(&c->sq[tc].comp_lock);
	}
}

static int
mlx5e_open_channel(struct mlx5e_priv *priv,
    struct mlx5e_channel_param *cparam,
    struct mlx5e_channel *c)
{
	struct epoch_tracker et;
	int i, err;

	/* zero non-persistant data */
	MLX5E_ZERO(&c->rq, mlx5e_rq_zero_start);
	for (i = 0; i != priv->num_tc; i++)
		MLX5E_ZERO(&c->sq[i], mlx5e_sq_zero_start);

	/* open transmit completion queue */
	err = mlx5e_open_tx_cqs(c, cparam);
	if (err)
		goto err_free;

	/* open receive completion queue */
	err = mlx5e_open_cq(c->priv, &cparam->rx_cq, &c->rq.cq,
	    &mlx5e_rx_cq_comp, c->ix);
	if (err)
		goto err_close_tx_cqs;

	err = mlx5e_open_sqs(c, cparam);
	if (err)
		goto err_close_rx_cq;

	err = mlx5e_open_rq(c, &cparam->rq, &c->rq);
	if (err)
		goto err_close_sqs;

	/* poll receive queue initially */
	NET_EPOCH_ENTER(et);
	c->rq.cq.mcq.comp(&c->rq.cq.mcq, NULL);
	NET_EPOCH_EXIT(et);

	return (0);

err_close_sqs:
	mlx5e_close_sqs_wait(c);

err_close_rx_cq:
	mlx5e_close_cq(&c->rq.cq);

err_close_tx_cqs:
	mlx5e_close_tx_cqs(c);

err_free:
	return (err);
}

static void
mlx5e_close_channel(struct mlx5e_channel *c)
{
	mlx5e_close_rq(&c->rq);
}

static void
mlx5e_close_channel_wait(struct mlx5e_channel *c)
{
	mlx5e_close_rq_wait(&c->rq);
	mlx5e_close_sqs_wait(c);
	mlx5e_close_tx_cqs(c);
}

static int
mlx5e_get_wqe_sz(struct mlx5e_priv *priv, u32 *wqe_sz, u32 *nsegs)
{
	u32 r, n;

	r = priv->params.hw_lro_en ? priv->params.lro_wqe_sz :
	    MLX5E_SW2MB_MTU(priv->ifp->if_mtu);
	if (r > MJUM16BYTES)
		return (-ENOMEM);

	if (r > MJUM9BYTES)
		r = MJUM16BYTES;
	else if (r > MJUMPAGESIZE)
		r = MJUM9BYTES;
	else if (r > MCLBYTES)
		r = MJUMPAGESIZE;
	else
		r = MCLBYTES;

	/*
	 * n + 1 must be a power of two, because stride size must be.
	 * Stride size is 16 * (n + 1), as the first segment is
	 * control.
	 */
	for (n = howmany(r, MLX5E_MAX_RX_BYTES); !powerof2(n + 1); n++)
		;

	if (n > MLX5E_MAX_BUSDMA_RX_SEGS)
		return (-ENOMEM);

	*wqe_sz = r;
	*nsegs = n;
	return (0);
}

static void
mlx5e_build_rq_param(struct mlx5e_priv *priv,
    struct mlx5e_rq_param *param)
{
	void *rqc = param->rqc;
	void *wq = MLX5_ADDR_OF(rqc, rqc, wq);
	u32 wqe_sz, nsegs;

	mlx5e_get_wqe_sz(priv, &wqe_sz, &nsegs);
	MLX5_SET(wq, wq, wq_type, MLX5_WQ_TYPE_LINKED_LIST);
	MLX5_SET(wq, wq, end_padding_mode, MLX5_WQ_END_PAD_MODE_ALIGN);
	MLX5_SET(wq, wq, log_wq_stride, ilog2(sizeof(struct mlx5e_rx_wqe) +
	    nsegs * sizeof(struct mlx5_wqe_data_seg)));
	MLX5_SET(wq, wq, log_wq_sz, priv->params.log_rq_size);
	MLX5_SET(wq, wq, pd, priv->pdn);

	param->wq.linear = 1;
}

static void
mlx5e_build_sq_param(struct mlx5e_priv *priv,
    struct mlx5e_sq_param *param)
{
	void *sqc = param->sqc;
	void *wq = MLX5_ADDR_OF(sqc, sqc, wq);

	MLX5_SET(wq, wq, log_wq_sz, priv->params.log_sq_size);
	MLX5_SET(wq, wq, log_wq_stride, ilog2(MLX5_SEND_WQE_BB));
	MLX5_SET(wq, wq, pd, priv->pdn);

	param->wq.linear = 1;
}

static void
mlx5e_build_common_cq_param(struct mlx5e_priv *priv,
    struct mlx5e_cq_param *param)
{
	void *cqc = param->cqc;

	MLX5_SET(cqc, cqc, uar_page, priv->mdev->priv.uar->index);
}

static void
mlx5e_get_default_profile(struct mlx5e_priv *priv, int mode, struct net_dim_cq_moder *ptr)
{

	*ptr = net_dim_get_profile(mode, MLX5E_DIM_DEFAULT_PROFILE);

	/* apply LRO restrictions */
	if (priv->params.hw_lro_en &&
	    ptr->pkts > MLX5E_DIM_MAX_RX_CQ_MODERATION_PKTS_WITH_LRO) {
		ptr->pkts = MLX5E_DIM_MAX_RX_CQ_MODERATION_PKTS_WITH_LRO;
	}
}

static void
mlx5e_build_rx_cq_param(struct mlx5e_priv *priv,
    struct mlx5e_cq_param *param)
{
	struct net_dim_cq_moder curr;
	void *cqc = param->cqc;

	/*
	 * We use MLX5_CQE_FORMAT_HASH because the RX hash mini CQE
	 * format is more beneficial for FreeBSD use case.
	 *
	 * Adding support for MLX5_CQE_FORMAT_CSUM will require changes
	 * in mlx5e_decompress_cqe.
	 */
	if (priv->params.cqe_zipping_en) {
		MLX5_SET(cqc, cqc, mini_cqe_res_format, MLX5_CQE_FORMAT_HASH);
		MLX5_SET(cqc, cqc, cqe_compression_en, 1);
	}

	MLX5_SET(cqc, cqc, log_cq_size, priv->params.log_rq_size);

	switch (priv->params.rx_cq_moderation_mode) {
	case 0:
		MLX5_SET(cqc, cqc, cq_period, priv->params.rx_cq_moderation_usec);
		MLX5_SET(cqc, cqc, cq_max_count, priv->params.rx_cq_moderation_pkts);
		MLX5_SET(cqc, cqc, cq_period_mode, MLX5_CQ_PERIOD_MODE_START_FROM_EQE);
		break;
	case 1:
		MLX5_SET(cqc, cqc, cq_period, priv->params.rx_cq_moderation_usec);
		MLX5_SET(cqc, cqc, cq_max_count, priv->params.rx_cq_moderation_pkts);
		if (MLX5_CAP_GEN(priv->mdev, cq_period_start_from_cqe))
			MLX5_SET(cqc, cqc, cq_period_mode, MLX5_CQ_PERIOD_MODE_START_FROM_CQE);
		else
			MLX5_SET(cqc, cqc, cq_period_mode, MLX5_CQ_PERIOD_MODE_START_FROM_EQE);
		break;
	case 2:
		mlx5e_get_default_profile(priv, NET_DIM_CQ_PERIOD_MODE_START_FROM_EQE, &curr);
		MLX5_SET(cqc, cqc, cq_period, curr.usec);
		MLX5_SET(cqc, cqc, cq_max_count, curr.pkts);
		MLX5_SET(cqc, cqc, cq_period_mode, MLX5_CQ_PERIOD_MODE_START_FROM_EQE);
		break;
	case 3:
		mlx5e_get_default_profile(priv, NET_DIM_CQ_PERIOD_MODE_START_FROM_CQE, &curr);
		MLX5_SET(cqc, cqc, cq_period, curr.usec);
		MLX5_SET(cqc, cqc, cq_max_count, curr.pkts);
		if (MLX5_CAP_GEN(priv->mdev, cq_period_start_from_cqe))
			MLX5_SET(cqc, cqc, cq_period_mode, MLX5_CQ_PERIOD_MODE_START_FROM_CQE);
		else
			MLX5_SET(cqc, cqc, cq_period_mode, MLX5_CQ_PERIOD_MODE_START_FROM_EQE);
		break;
	default:
		break;
	}

	mlx5e_dim_build_cq_param(priv, param);

	mlx5e_build_common_cq_param(priv, param);
}

static void
mlx5e_build_tx_cq_param(struct mlx5e_priv *priv,
    struct mlx5e_cq_param *param)
{
	void *cqc = param->cqc;

	MLX5_SET(cqc, cqc, log_cq_size, priv->params.log_sq_size);
	MLX5_SET(cqc, cqc, cq_period, priv->params.tx_cq_moderation_usec);
	MLX5_SET(cqc, cqc, cq_max_count, priv->params.tx_cq_moderation_pkts);

	switch (priv->params.tx_cq_moderation_mode) {
	case 0:
		MLX5_SET(cqc, cqc, cq_period_mode, MLX5_CQ_PERIOD_MODE_START_FROM_EQE);
		break;
	default:
		if (MLX5_CAP_GEN(priv->mdev, cq_period_start_from_cqe))
			MLX5_SET(cqc, cqc, cq_period_mode, MLX5_CQ_PERIOD_MODE_START_FROM_CQE);
		else
			MLX5_SET(cqc, cqc, cq_period_mode, MLX5_CQ_PERIOD_MODE_START_FROM_EQE);
		break;
	}

	mlx5e_build_common_cq_param(priv, param);
}

static void
mlx5e_build_channel_param(struct mlx5e_priv *priv,
    struct mlx5e_channel_param *cparam)
{
	memset(cparam, 0, sizeof(*cparam));

	mlx5e_build_rq_param(priv, &cparam->rq);
	mlx5e_build_sq_param(priv, &cparam->sq);
	mlx5e_build_rx_cq_param(priv, &cparam->rx_cq);
	mlx5e_build_tx_cq_param(priv, &cparam->tx_cq);
}

static int
mlx5e_open_channels(struct mlx5e_priv *priv)
{
	struct mlx5e_channel_param *cparam;
	int err;
	int i;
	int j;

	cparam = malloc(sizeof(*cparam), M_MLX5EN, M_WAITOK);

	mlx5e_build_channel_param(priv, cparam);
	for (i = 0; i < priv->params.num_channels; i++) {
		err = mlx5e_open_channel(priv, cparam, &priv->channel[i]);
		if (err)
			goto err_close_channels;

		/* Bind interrupt vectors, if any. */
		if (priv->params_ethtool.irq_cpu_base > -1) {
			cpuset_t cpuset;
			int cpu;
			int irq;
			int eqn;
			int nirq;

			err = mlx5_vector2eqn(priv->mdev, i,
			    &eqn, &nirq);

			/* error here is non-fatal */
			if (err != 0)
				continue;

			irq = priv->mdev->priv.msix_arr[nirq].vector;
			cpu = (unsigned)(priv->params_ethtool.irq_cpu_base +
			    i * priv->params_ethtool.irq_cpu_stride) % (unsigned)mp_ncpus;

			CPU_ZERO(&cpuset);
			CPU_SET(cpu, &cpuset);
			intr_setaffinity(irq, CPU_WHICH_INTRHANDLER, &cpuset);
		}
	}

	for (j = 0; j < priv->params.num_channels; j++) {
		err = mlx5e_wait_for_min_rx_wqes(&priv->channel[j].rq);
		if (err)
			goto err_close_channels;
	}
	free(cparam, M_MLX5EN);
	return (0);

err_close_channels:
	while (i--) {
		mlx5e_close_channel(&priv->channel[i]);
		mlx5e_close_channel_wait(&priv->channel[i]);
	}
	free(cparam, M_MLX5EN);
	return (err);
}

static void
mlx5e_close_channels(struct mlx5e_priv *priv)
{
	int i;

	for (i = 0; i < priv->params.num_channels; i++)
		mlx5e_close_channel(&priv->channel[i]);
	for (i = 0; i < priv->params.num_channels; i++)
		mlx5e_close_channel_wait(&priv->channel[i]);
}

static int
mlx5e_refresh_sq_params(struct mlx5e_priv *priv, struct mlx5e_sq *sq)
{

	if (MLX5_CAP_GEN(priv->mdev, cq_period_mode_modify)) {
		uint8_t cq_mode;

		switch (priv->params.tx_cq_moderation_mode) {
		case 0:
		case 2:
			cq_mode = MLX5_CQ_PERIOD_MODE_START_FROM_EQE;
			break;
		default:
			cq_mode = MLX5_CQ_PERIOD_MODE_START_FROM_CQE;
			break;
		}

		return (mlx5_core_modify_cq_moderation_mode(priv->mdev, &sq->cq.mcq,
		    priv->params.tx_cq_moderation_usec,
		    priv->params.tx_cq_moderation_pkts,
		    cq_mode));
	}

	return (mlx5_core_modify_cq_moderation(priv->mdev, &sq->cq.mcq,
	    priv->params.tx_cq_moderation_usec,
	    priv->params.tx_cq_moderation_pkts));
}

static int
mlx5e_refresh_rq_params(struct mlx5e_priv *priv, struct mlx5e_rq *rq)
{

	if (MLX5_CAP_GEN(priv->mdev, cq_period_mode_modify)) {
		uint8_t cq_mode;
		uint8_t dim_mode;
		int retval;

		switch (priv->params.rx_cq_moderation_mode) {
		case 0:
		case 2:
			cq_mode = MLX5_CQ_PERIOD_MODE_START_FROM_EQE;
			dim_mode = NET_DIM_CQ_PERIOD_MODE_START_FROM_EQE;
			break;
		default:
			cq_mode = MLX5_CQ_PERIOD_MODE_START_FROM_CQE;
			dim_mode = NET_DIM_CQ_PERIOD_MODE_START_FROM_CQE;
			break;
		}

		/* tear down dynamic interrupt moderation */
		mtx_lock(&rq->mtx);
		rq->dim.mode = NET_DIM_CQ_PERIOD_MODE_DISABLED;
		mtx_unlock(&rq->mtx);

		/* wait for dynamic interrupt moderation work task, if any */
		cancel_work_sync(&rq->dim.work);

		if (priv->params.rx_cq_moderation_mode >= 2) {
			struct net_dim_cq_moder curr;

			mlx5e_get_default_profile(priv, dim_mode, &curr);

			retval = mlx5_core_modify_cq_moderation_mode(priv->mdev, &rq->cq.mcq,
			    curr.usec, curr.pkts, cq_mode);

			/* set dynamic interrupt moderation mode and zero defaults */
			mtx_lock(&rq->mtx);
			rq->dim.mode = dim_mode;
			rq->dim.state = 0;
			rq->dim.profile_ix = MLX5E_DIM_DEFAULT_PROFILE;
			mtx_unlock(&rq->mtx);
		} else {
			retval = mlx5_core_modify_cq_moderation_mode(priv->mdev, &rq->cq.mcq,
			    priv->params.rx_cq_moderation_usec,
			    priv->params.rx_cq_moderation_pkts,
			    cq_mode);
		}
		return (retval);
	}

	return (mlx5_core_modify_cq_moderation(priv->mdev, &rq->cq.mcq,
	    priv->params.rx_cq_moderation_usec,
	    priv->params.rx_cq_moderation_pkts));
}

static int
mlx5e_refresh_channel_params_sub(struct mlx5e_priv *priv, struct mlx5e_channel *c)
{
	int err;
	int i;

	err = mlx5e_refresh_rq_params(priv, &c->rq);
	if (err)
		goto done;

	for (i = 0; i != priv->num_tc; i++) {
		err = mlx5e_refresh_sq_params(priv, &c->sq[i]);
		if (err)
			goto done;
	}
done:
	return (err);
}

int
mlx5e_refresh_channel_params(struct mlx5e_priv *priv)
{
	int i;

	/* check if channels are closed */
	if (test_bit(MLX5E_STATE_OPENED, &priv->state) == 0)
		return (EINVAL);

	for (i = 0; i < priv->params.num_channels; i++) {
		int err;

		err = mlx5e_refresh_channel_params_sub(priv, &priv->channel[i]);
		if (err)
			return (err);
	}
	return (0);
}

static int
mlx5e_open_tis(struct mlx5e_priv *priv, int tc)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 in[MLX5_ST_SZ_DW(create_tis_in)];
	void *tisc = MLX5_ADDR_OF(create_tis_in, in, ctx);

	memset(in, 0, sizeof(in));

	MLX5_SET(tisc, tisc, prio, tc);
	MLX5_SET(tisc, tisc, transport_domain, priv->tdn);

	return (mlx5_core_create_tis(mdev, in, sizeof(in), &priv->tisn[tc]));
}

static void
mlx5e_close_tis(struct mlx5e_priv *priv, int tc)
{
	mlx5_core_destroy_tis(priv->mdev, priv->tisn[tc], 0);
}

static int
mlx5e_open_tises(struct mlx5e_priv *priv)
{
	int num_tc = priv->num_tc;
	int err;
	int tc;

	for (tc = 0; tc < num_tc; tc++) {
		err = mlx5e_open_tis(priv, tc);
		if (err)
			goto err_close_tises;
	}

	return (0);

err_close_tises:
	for (tc--; tc >= 0; tc--)
		mlx5e_close_tis(priv, tc);

	return (err);
}

static void
mlx5e_close_tises(struct mlx5e_priv *priv)
{
	int num_tc = priv->num_tc;
	int tc;

	for (tc = 0; tc < num_tc; tc++)
		mlx5e_close_tis(priv, tc);
}

static int
mlx5e_open_rqt(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 *in;
	u32 out[MLX5_ST_SZ_DW(create_rqt_out)] = {0};
	void *rqtc;
	int inlen;
	int err;
	int sz;
	int i;

	sz = 1 << priv->params.rx_hash_log_tbl_sz;

	inlen = MLX5_ST_SZ_BYTES(create_rqt_in) + sizeof(u32) * sz;
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);
	rqtc = MLX5_ADDR_OF(create_rqt_in, in, rqt_context);

	MLX5_SET(rqtc, rqtc, rqt_actual_size, sz);
	MLX5_SET(rqtc, rqtc, rqt_max_size, sz);

	for (i = 0; i < sz; i++) {
		int ix = i;
#ifdef RSS
		ix = rss_get_indirection_to_bucket(ix);
#endif
		/* ensure we don't overflow */
		ix %= priv->params.num_channels;

		/* apply receive side scaling stride, if any */
		ix -= ix % (int)priv->params.channels_rsss;

		MLX5_SET(rqtc, rqtc, rq_num[i], priv->channel[ix].rq.rqn);
	}

	MLX5_SET(create_rqt_in, in, opcode, MLX5_CMD_OP_CREATE_RQT);

	err = mlx5_cmd_exec(mdev, in, inlen, out, sizeof(out));
	if (!err)
		priv->rqtn = MLX5_GET(create_rqt_out, out, rqtn);

	kvfree(in);

	return (err);
}

static void
mlx5e_close_rqt(struct mlx5e_priv *priv)
{
	u32 in[MLX5_ST_SZ_DW(destroy_rqt_in)] = {0};
	u32 out[MLX5_ST_SZ_DW(destroy_rqt_out)] = {0};

	MLX5_SET(destroy_rqt_in, in, opcode, MLX5_CMD_OP_DESTROY_RQT);
	MLX5_SET(destroy_rqt_in, in, rqtn, priv->rqtn);

	mlx5_cmd_exec(priv->mdev, in, sizeof(in), out, sizeof(out));
}

#define	MLX5E_RSS_KEY_SIZE (10 * 4)	/* bytes */

static void
mlx5e_get_rss_key(void *key_ptr)
{
#ifdef RSS
	rss_getkey(key_ptr);
#else
	static const u32 rsskey[] = {
	    cpu_to_be32(0xD181C62C),
	    cpu_to_be32(0xF7F4DB5B),
	    cpu_to_be32(0x1983A2FC),
	    cpu_to_be32(0x943E1ADB),
	    cpu_to_be32(0xD9389E6B),
	    cpu_to_be32(0xD1039C2C),
	    cpu_to_be32(0xA74499AD),
	    cpu_to_be32(0x593D56D9),
	    cpu_to_be32(0xF3253C06),
	    cpu_to_be32(0x2ADC1FFC),
	};
	CTASSERT(sizeof(rsskey) == MLX5E_RSS_KEY_SIZE);
	memcpy(key_ptr, rsskey, MLX5E_RSS_KEY_SIZE);
#endif
}

static void
mlx5e_build_tir_ctx(struct mlx5e_priv *priv, u32 * tirc, int tt, bool inner_vxlan)
{
	void *hfso = MLX5_ADDR_OF(tirc, tirc, rx_hash_field_selector_outer);
	void *hfsi = MLX5_ADDR_OF(tirc, tirc, rx_hash_field_selector_inner);
	void *hfs = inner_vxlan ? hfsi : hfso;
	__be32 *hkey;

	MLX5_SET(tirc, tirc, transport_domain, priv->tdn);

#define	ROUGH_MAX_L2_L3_HDR_SZ 256

#define	MLX5_HASH_IP     (MLX5_HASH_FIELD_SEL_SRC_IP   |\
			  MLX5_HASH_FIELD_SEL_DST_IP)

#define	MLX5_HASH_ALL    (MLX5_HASH_FIELD_SEL_SRC_IP   |\
			  MLX5_HASH_FIELD_SEL_DST_IP   |\
			  MLX5_HASH_FIELD_SEL_L4_SPORT |\
			  MLX5_HASH_FIELD_SEL_L4_DPORT)

#define	MLX5_HASH_IP_IPSEC_SPI	(MLX5_HASH_FIELD_SEL_SRC_IP   |\
				 MLX5_HASH_FIELD_SEL_DST_IP   |\
				 MLX5_HASH_FIELD_SEL_IPSEC_SPI)

	if (priv->params.hw_lro_en) {
		MLX5_SET(tirc, tirc, lro_enable_mask,
		    MLX5_TIRC_LRO_ENABLE_MASK_IPV4_LRO |
		    MLX5_TIRC_LRO_ENABLE_MASK_IPV6_LRO);
		MLX5_SET(tirc, tirc, lro_max_msg_sz,
		    (priv->params.lro_wqe_sz -
		    ROUGH_MAX_L2_L3_HDR_SZ) >> 8);
		/* TODO: add the option to choose timer value dynamically */
		MLX5_SET(tirc, tirc, lro_timeout_period_usecs,
		    MLX5_CAP_ETH(priv->mdev,
		    lro_timer_supported_periods[2]));
	}

	if (inner_vxlan)
		MLX5_SET(tirc, tirc, tunneled_offload_en, 1);

	/* setup parameters for hashing TIR type, if any */
	switch (tt) {
	case MLX5E_TT_ANY:
		MLX5_SET(tirc, tirc, disp_type,
		    MLX5_TIRC_DISP_TYPE_DIRECT);
		MLX5_SET(tirc, tirc, inline_rqn,
		    priv->channel[0].rq.rqn);
		break;
	default:
		MLX5_SET(tirc, tirc, disp_type,
		    MLX5_TIRC_DISP_TYPE_INDIRECT);
		MLX5_SET(tirc, tirc, indirect_table,
		    priv->rqtn);
		MLX5_SET(tirc, tirc, rx_hash_fn,
		    MLX5_TIRC_RX_HASH_FN_HASH_TOEPLITZ);
		hkey = (__be32 *) MLX5_ADDR_OF(tirc, tirc, rx_hash_toeplitz_key);

		CTASSERT(MLX5_FLD_SZ_BYTES(tirc, rx_hash_toeplitz_key) >=
		    MLX5E_RSS_KEY_SIZE);
#ifdef RSS
		/*
		 * The FreeBSD RSS implementation does currently not
		 * support symmetric Toeplitz hashes:
		 */
		MLX5_SET(tirc, tirc, rx_hash_symmetric, 0);
#else
		MLX5_SET(tirc, tirc, rx_hash_symmetric, 1);
#endif
		mlx5e_get_rss_key(hkey);
		break;
	}

	switch (tt) {
	case MLX5E_TT_IPV4_TCP:
		MLX5_SET(rx_hash_field_select, hfs, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV4);
		MLX5_SET(rx_hash_field_select, hfs, l4_prot_type,
		    MLX5_L4_PROT_TYPE_TCP);
#ifdef RSS
		if (!(rss_gethashconfig() & RSS_HASHTYPE_RSS_TCP_IPV4)) {
			MLX5_SET(rx_hash_field_select, hfs, selected_fields,
			    MLX5_HASH_IP);
		} else
#endif
		MLX5_SET(rx_hash_field_select, hfs, selected_fields,
		    MLX5_HASH_ALL);
		break;

	case MLX5E_TT_IPV6_TCP:
		MLX5_SET(rx_hash_field_select, hfs, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV6);
		MLX5_SET(rx_hash_field_select, hfs, l4_prot_type,
		    MLX5_L4_PROT_TYPE_TCP);
#ifdef RSS
		if (!(rss_gethashconfig() & RSS_HASHTYPE_RSS_TCP_IPV6)) {
			MLX5_SET(rx_hash_field_select, hfs, selected_fields,
			    MLX5_HASH_IP);
		} else
#endif
		MLX5_SET(rx_hash_field_select, hfs, selected_fields,
		    MLX5_HASH_ALL);
		break;

	case MLX5E_TT_IPV4_UDP:
		MLX5_SET(rx_hash_field_select, hfs, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV4);
		MLX5_SET(rx_hash_field_select, hfs, l4_prot_type,
		    MLX5_L4_PROT_TYPE_UDP);
#ifdef RSS
		if (!(rss_gethashconfig() & RSS_HASHTYPE_RSS_UDP_IPV4)) {
			MLX5_SET(rx_hash_field_select, hfs, selected_fields,
			    MLX5_HASH_IP);
		} else
#endif
		MLX5_SET(rx_hash_field_select, hfs, selected_fields,
		    MLX5_HASH_ALL);
		break;

	case MLX5E_TT_IPV6_UDP:
		MLX5_SET(rx_hash_field_select, hfs, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV6);
		MLX5_SET(rx_hash_field_select, hfs, l4_prot_type,
		    MLX5_L4_PROT_TYPE_UDP);
#ifdef RSS
		if (!(rss_gethashconfig() & RSS_HASHTYPE_RSS_UDP_IPV6)) {
			MLX5_SET(rx_hash_field_select, hfs, selected_fields,
			    MLX5_HASH_IP);
		} else
#endif
		MLX5_SET(rx_hash_field_select, hfs, selected_fields,
		    MLX5_HASH_ALL);
		break;

	case MLX5E_TT_IPV4_IPSEC_AH:
		MLX5_SET(rx_hash_field_select, hfs, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV4);
		MLX5_SET(rx_hash_field_select, hfs, selected_fields,
		    MLX5_HASH_IP_IPSEC_SPI);
		break;

	case MLX5E_TT_IPV6_IPSEC_AH:
		MLX5_SET(rx_hash_field_select, hfs, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV6);
		MLX5_SET(rx_hash_field_select, hfs, selected_fields,
		    MLX5_HASH_IP_IPSEC_SPI);
		break;

	case MLX5E_TT_IPV4_IPSEC_ESP:
		MLX5_SET(rx_hash_field_select, hfs, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV4);
		MLX5_SET(rx_hash_field_select, hfs, selected_fields,
		    MLX5_HASH_IP_IPSEC_SPI);
		break;

	case MLX5E_TT_IPV6_IPSEC_ESP:
		MLX5_SET(rx_hash_field_select, hfs, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV6);
		MLX5_SET(rx_hash_field_select, hfs, selected_fields,
		    MLX5_HASH_IP_IPSEC_SPI);
		break;

	case MLX5E_TT_IPV4:
		MLX5_SET(rx_hash_field_select, hfs, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV4);
		MLX5_SET(rx_hash_field_select, hfs, selected_fields,
		    MLX5_HASH_IP);
		break;

	case MLX5E_TT_IPV6:
		MLX5_SET(rx_hash_field_select, hfs, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV6);
		MLX5_SET(rx_hash_field_select, hfs, selected_fields,
		    MLX5_HASH_IP);
		break;

	default:
		break;
	}
}

static int
mlx5e_open_tir(struct mlx5e_priv *priv, int tt, bool inner_vxlan)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 *in;
	void *tirc;
	int inlen;
	int err;

	inlen = MLX5_ST_SZ_BYTES(create_tir_in);
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);
	tirc = MLX5_ADDR_OF(create_tir_in, in, tir_context);

	mlx5e_build_tir_ctx(priv, tirc, tt, inner_vxlan);

	err = mlx5_core_create_tir(mdev, in, inlen, inner_vxlan ?
	    &priv->tirn_inner_vxlan[tt] : &priv->tirn[tt]);

	kvfree(in);

	return (err);
}

static void
mlx5e_close_tir(struct mlx5e_priv *priv, int tt, bool inner_vxlan)
{
	mlx5_core_destroy_tir(priv->mdev, inner_vxlan ?
	    priv->tirn_inner_vxlan[tt] : priv->tirn[tt], 0);
}

static int
mlx5e_open_tirs(struct mlx5e_priv *priv, bool inner_vxlan)
{
	int err;
	int i;

	for (i = 0; i < MLX5E_NUM_TT; i++) {
		err = mlx5e_open_tir(priv, i, inner_vxlan);
		if (err)
			goto err_close_tirs;
	}

	return (0);

err_close_tirs:
	for (i--; i >= 0; i--)
		mlx5e_close_tir(priv, i, inner_vxlan);

	return (err);
}

static void
mlx5e_close_tirs(struct mlx5e_priv *priv, bool inner_vxlan)
{
	int i;

	for (i = 0; i < MLX5E_NUM_TT; i++)
		mlx5e_close_tir(priv, i, inner_vxlan);
}

/*
 * SW MTU does not include headers,
 * HW MTU includes all headers and checksums.
 */
static int
mlx5e_set_dev_port_mtu(struct ifnet *ifp, int sw_mtu)
{
	struct mlx5e_priv *priv = ifp->if_softc;
	struct mlx5_core_dev *mdev = priv->mdev;
	int hw_mtu;
	int err;

	hw_mtu = MLX5E_SW2HW_MTU(sw_mtu);

	err = mlx5_set_port_mtu(mdev, hw_mtu);
	if (err) {
		mlx5_en_err(ifp, "mlx5_set_port_mtu failed setting %d, err=%d\n",
		    sw_mtu, err);
		return (err);
	}

	/* Update vport context MTU */
	err = mlx5_set_vport_mtu(mdev, hw_mtu);
	if (err) {
		mlx5_en_err(ifp,
		    "Failed updating vport context with MTU size, err=%d\n",
		    err);
	}

	ifp->if_mtu = sw_mtu;

	err = mlx5_query_vport_mtu(mdev, &hw_mtu);
	if (err || !hw_mtu) {
		/* fallback to port oper mtu */
		err = mlx5_query_port_oper_mtu(mdev, &hw_mtu);
	}
	if (err) {
		mlx5_en_err(ifp,
		    "Query port MTU, after setting new MTU value, failed\n");
		return (err);
	} else if (MLX5E_HW2SW_MTU(hw_mtu) < sw_mtu) {
		err = -E2BIG,
		mlx5_en_err(ifp,
		    "Port MTU %d is smaller than ifp mtu %d\n",
		    hw_mtu, sw_mtu);
	} else if (MLX5E_HW2SW_MTU(hw_mtu) > sw_mtu) {
		err = -EINVAL;
                mlx5_en_err(ifp,
		    "Port MTU %d is bigger than ifp mtu %d\n",
		    hw_mtu, sw_mtu);
	}
	priv->params_ethtool.hw_mtu = hw_mtu;

	/* compute MSB */
	while (hw_mtu & (hw_mtu - 1))
		hw_mtu &= (hw_mtu - 1);
	priv->params_ethtool.hw_mtu_msb = hw_mtu;

	return (err);
}

int
mlx5e_open_locked(struct ifnet *ifp)
{
	struct mlx5e_priv *priv = ifp->if_softc;
	int err;
	u16 set_id;

	/* check if already opened */
	if (test_bit(MLX5E_STATE_OPENED, &priv->state) != 0)
		return (0);

#ifdef RSS
	if (rss_getnumbuckets() > priv->params.num_channels) {
		mlx5_en_info(ifp,
		    "NOTE: There are more RSS buckets(%u) than channels(%u) available\n",
		    rss_getnumbuckets(), priv->params.num_channels);
	}
#endif
	err = mlx5e_open_tises(priv);
	if (err) {
		mlx5_en_err(ifp, "mlx5e_open_tises failed, %d\n", err);
		return (err);
	}
	err = mlx5_vport_alloc_q_counter(priv->mdev,
	    MLX5_INTERFACE_PROTOCOL_ETH, &set_id);
	if (err) {
		mlx5_en_err(priv->ifp,
		    "mlx5_vport_alloc_q_counter failed: %d\n", err);
		goto err_close_tises;
	}
	/* store counter set ID */
	priv->counter_set_id = set_id;

	err = mlx5e_open_channels(priv);
	if (err) {
		mlx5_en_err(ifp,
		    "mlx5e_open_channels failed, %d\n", err);
		goto err_dalloc_q_counter;
	}
	err = mlx5e_open_rqt(priv);
	if (err) {
		mlx5_en_err(ifp, "mlx5e_open_rqt failed, %d\n", err);
		goto err_close_channels;
	}
	err = mlx5e_open_tirs(priv, false);
	if (err) {
		mlx5_en_err(ifp, "mlx5e_open_tir(main) failed, %d\n", err);
		goto err_close_rqls;
	}
	if ((ifp->if_capenable & IFCAP_VXLAN_HWCSUM) != 0) {
		err = mlx5e_open_tirs(priv, true);
		if (err) {
			mlx5_en_err(ifp, "mlx5e_open_tir(inner) failed, %d\n",
			    err);
			goto err_close_tirs;
		}
	}
	err = mlx5e_open_flow_table(priv);
	if (err) {
		mlx5_en_err(ifp,
		    "mlx5e_open_flow_table failed, %d\n", err);
		goto err_close_tirs_inner;
	}
	err = mlx5e_add_all_vlan_rules(priv);
	if (err) {
		mlx5_en_err(ifp,
		    "mlx5e_add_all_vlan_rules failed, %d\n", err);
		goto err_close_flow_table;
	}
	if ((ifp->if_capenable & IFCAP_VXLAN_HWCSUM) != 0) {
		err = mlx5e_add_all_vxlan_rules(priv);
		if (err) {
			mlx5_en_err(ifp,
			    "mlx5e_add_all_vxlan_rules failed, %d\n", err);
			goto err_del_vlan_rules;
		}
	}
	set_bit(MLX5E_STATE_OPENED, &priv->state);

	mlx5e_update_carrier(priv);
	mlx5e_set_rx_mode_core(priv);

	return (0);

err_del_vlan_rules:
	mlx5e_del_all_vlan_rules(priv);

err_close_flow_table:
	mlx5e_close_flow_table(priv);

err_close_tirs_inner:
	if ((ifp->if_capenable & IFCAP_VXLAN_HWCSUM) != 0)
		mlx5e_close_tirs(priv, true);

err_close_tirs:
	mlx5e_close_tirs(priv, false);

err_close_rqls:
	mlx5e_close_rqt(priv);

err_close_channels:
	mlx5e_close_channels(priv);

err_dalloc_q_counter:
	mlx5_vport_dealloc_q_counter(priv->mdev,
	    MLX5_INTERFACE_PROTOCOL_ETH, priv->counter_set_id);

err_close_tises:
	mlx5e_close_tises(priv);

	return (err);
}

static void
mlx5e_open(void *arg)
{
	struct mlx5e_priv *priv = arg;

	PRIV_LOCK(priv);
	if (mlx5_set_port_status(priv->mdev, MLX5_PORT_UP))
		mlx5_en_err(priv->ifp,
		    "Setting port status to up failed\n");

	mlx5e_open_locked(priv->ifp);
	priv->ifp->if_drv_flags |= IFF_DRV_RUNNING;
	PRIV_UNLOCK(priv);
}

int
mlx5e_close_locked(struct ifnet *ifp)
{
	struct mlx5e_priv *priv = ifp->if_softc;

	/* check if already closed */
	if (test_bit(MLX5E_STATE_OPENED, &priv->state) == 0)
		return (0);

	clear_bit(MLX5E_STATE_OPENED, &priv->state);

	mlx5e_set_rx_mode_core(priv);
	mlx5e_del_all_vlan_rules(priv);
	if ((ifp->if_capenable & IFCAP_VXLAN_HWCSUM) != 0)
		mlx5e_del_all_vxlan_rules(priv);
	if_link_state_change(priv->ifp, LINK_STATE_DOWN);
	mlx5e_close_flow_table(priv);
	if ((ifp->if_capenable & IFCAP_VXLAN_HWCSUM) != 0)
		mlx5e_close_tirs(priv, true);
	mlx5e_close_tirs(priv, false);
	mlx5e_close_rqt(priv);
	mlx5e_close_channels(priv);
	mlx5_vport_dealloc_q_counter(priv->mdev,
	    MLX5_INTERFACE_PROTOCOL_ETH, priv->counter_set_id);
	mlx5e_close_tises(priv);

	return (0);
}

#if (__FreeBSD_version >= 1100000)
static uint64_t
mlx5e_get_counter(struct ifnet *ifp, ift_counter cnt)
{
	struct mlx5e_priv *priv = ifp->if_softc;
	u64 retval;

	/* PRIV_LOCK(priv); XXX not allowed */
	switch (cnt) {
	case IFCOUNTER_IPACKETS:
		retval = priv->stats.vport.rx_packets;
		break;
	case IFCOUNTER_IERRORS:
		retval = priv->stats.pport.in_range_len_errors +
		    priv->stats.pport.out_of_range_len +
		    priv->stats.pport.too_long_errors +
		    priv->stats.pport.check_seq_err +
		    priv->stats.pport.alignment_err;
		break;
	case IFCOUNTER_IQDROPS:
		retval = priv->stats.vport.rx_out_of_buffer;
		break;
	case IFCOUNTER_OPACKETS:
		retval = priv->stats.vport.tx_packets;
		break;
	case IFCOUNTER_OERRORS:
		retval = priv->stats.port_stats_debug.out_discards;
		break;
	case IFCOUNTER_IBYTES:
		retval = priv->stats.vport.rx_bytes;
		break;
	case IFCOUNTER_OBYTES:
		retval = priv->stats.vport.tx_bytes;
		break;
	case IFCOUNTER_IMCASTS:
		retval = priv->stats.vport.rx_multicast_packets;
		break;
	case IFCOUNTER_OMCASTS:
		retval = priv->stats.vport.tx_multicast_packets;
		break;
	case IFCOUNTER_OQDROPS:
		retval = priv->stats.vport.tx_queue_dropped;
		break;
	case IFCOUNTER_COLLISIONS:
		retval = priv->stats.pport.collisions;
		break;
	default:
		retval = if_get_counter_default(ifp, cnt);
		break;
	}
	/* PRIV_UNLOCK(priv); XXX not allowed */
	return (retval);
}
#endif

static void
mlx5e_set_rx_mode(struct ifnet *ifp)
{
	struct mlx5e_priv *priv = ifp->if_softc;

	queue_work(priv->wq, &priv->set_rx_mode_work);
}

static int
mlx5e_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct mlx5e_priv *priv;
	struct ifreq *ifr;
	struct ifdownreason *ifdr;
	struct ifi2creq i2c;
	struct ifrsskey *ifrk;
	struct ifrsshash *ifrh;
	int error = 0;
	int mask = 0;
	int size_read = 0;
	int module_status;
	int module_num;
	int max_mtu;
	uint8_t read_addr;

	priv = ifp->if_softc;

	/* check if detaching */
	if (priv == NULL || priv->gone != 0)
		return (ENXIO);

	switch (command) {
	case SIOCSIFMTU:
		ifr = (struct ifreq *)data;

		PRIV_LOCK(priv);
		mlx5_query_port_max_mtu(priv->mdev, &max_mtu);

		if (ifr->ifr_mtu >= MLX5E_MTU_MIN &&
		    ifr->ifr_mtu <= MIN(MLX5E_MTU_MAX, max_mtu)) {
			int was_opened;

			was_opened = test_bit(MLX5E_STATE_OPENED, &priv->state);
			if (was_opened)
				mlx5e_close_locked(ifp);

			/* set new MTU */
			mlx5e_set_dev_port_mtu(ifp, ifr->ifr_mtu);

			if (was_opened)
				mlx5e_open_locked(ifp);
		} else {
			error = EINVAL;
			mlx5_en_err(ifp,
			    "Invalid MTU value. Min val: %d, Max val: %d\n",
			    MLX5E_MTU_MIN, MIN(MLX5E_MTU_MAX, max_mtu));
		}
		PRIV_UNLOCK(priv);
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			mlx5e_set_rx_mode(ifp);
			break;
		}
		PRIV_LOCK(priv);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
				if (test_bit(MLX5E_STATE_OPENED, &priv->state) == 0)
					mlx5e_open_locked(ifp);
				ifp->if_drv_flags |= IFF_DRV_RUNNING;
				mlx5_set_port_status(priv->mdev, MLX5_PORT_UP);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				mlx5_set_port_status(priv->mdev,
				    MLX5_PORT_DOWN);
				if (test_bit(MLX5E_STATE_OPENED, &priv->state) != 0)
					mlx5e_close_locked(ifp);
				mlx5e_update_carrier(priv);
				ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			}
		}
		PRIV_UNLOCK(priv);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		mlx5e_set_rx_mode(ifp);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
	case SIOCGIFXMEDIA:
		ifr = (struct ifreq *)data;
		error = ifmedia_ioctl(ifp, ifr, &priv->media, command);
		break;
	case SIOCSIFCAP:
		ifr = (struct ifreq *)data;
		PRIV_LOCK(priv);
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;

		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			ifp->if_hwassist ^= (CSUM_TCP | CSUM_UDP | CSUM_IP);

			if (IFCAP_TSO4 & ifp->if_capenable &&
			    !(IFCAP_TXCSUM & ifp->if_capenable)) {
				mask &= ~IFCAP_TSO4;
				ifp->if_capenable &= ~IFCAP_TSO4;
				ifp->if_hwassist &= ~CSUM_IP_TSO;
				mlx5_en_err(ifp,
				    "tso4 disabled due to -txcsum.\n");
			}
		}
		if (mask & IFCAP_TXCSUM_IPV6) {
			ifp->if_capenable ^= IFCAP_TXCSUM_IPV6;
			ifp->if_hwassist ^= (CSUM_UDP_IPV6 | CSUM_TCP_IPV6);

			if (IFCAP_TSO6 & ifp->if_capenable &&
			    !(IFCAP_TXCSUM_IPV6 & ifp->if_capenable)) {
				mask &= ~IFCAP_TSO6;
				ifp->if_capenable &= ~IFCAP_TSO6;
				ifp->if_hwassist &= ~CSUM_IP6_TSO;
				mlx5_en_err(ifp,
				    "tso6 disabled due to -txcsum6.\n");
			}
		}
		if (mask & IFCAP_MEXTPG)
			ifp->if_capenable ^= IFCAP_MEXTPG;
		if (mask & IFCAP_TXTLS4)
			ifp->if_capenable ^= IFCAP_TXTLS4;
		if (mask & IFCAP_TXTLS6)
			ifp->if_capenable ^= IFCAP_TXTLS6;
#ifdef RATELIMIT
		if (mask & IFCAP_TXTLS_RTLMT)
			ifp->if_capenable ^= IFCAP_TXTLS_RTLMT;
#endif
		if (mask & IFCAP_RXCSUM)
			ifp->if_capenable ^= IFCAP_RXCSUM;
		if (mask & IFCAP_RXCSUM_IPV6)
			ifp->if_capenable ^= IFCAP_RXCSUM_IPV6;
		if (mask & IFCAP_TSO4) {
			if (!(IFCAP_TSO4 & ifp->if_capenable) &&
			    !(IFCAP_TXCSUM & ifp->if_capenable)) {
				mlx5_en_err(ifp, "enable txcsum first.\n");
				error = EAGAIN;
				goto out;
			}
			ifp->if_capenable ^= IFCAP_TSO4;
			ifp->if_hwassist ^= CSUM_IP_TSO;
		}
		if (mask & IFCAP_TSO6) {
			if (!(IFCAP_TSO6 & ifp->if_capenable) &&
			    !(IFCAP_TXCSUM_IPV6 & ifp->if_capenable)) {
				mlx5_en_err(ifp, "enable txcsum6 first.\n");
				error = EAGAIN;
				goto out;
			}
			ifp->if_capenable ^= IFCAP_TSO6;
			ifp->if_hwassist ^= CSUM_IP6_TSO;
		}
		if (mask & IFCAP_VLAN_HWTSO)
			ifp->if_capenable ^= IFCAP_VLAN_HWTSO;
		if (mask & IFCAP_VLAN_HWFILTER) {
			if (ifp->if_capenable & IFCAP_VLAN_HWFILTER)
				mlx5e_disable_vlan_filter(priv);
			else
				mlx5e_enable_vlan_filter(priv);

			ifp->if_capenable ^= IFCAP_VLAN_HWFILTER;
		}
		if (mask & IFCAP_VLAN_HWTAGGING)
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
		if (mask & IFCAP_WOL_MAGIC)
			ifp->if_capenable ^= IFCAP_WOL_MAGIC;
		if (mask & IFCAP_VXLAN_HWCSUM) {
			int was_opened = test_bit(MLX5E_STATE_OPENED,
			    &priv->state);
			if (was_opened)
				mlx5e_close_locked(ifp);
			ifp->if_capenable ^= IFCAP_VXLAN_HWCSUM;
			ifp->if_hwassist ^= CSUM_INNER_IP | CSUM_INNER_IP_UDP |
			    CSUM_INNER_IP_TCP | CSUM_INNER_IP6_UDP |
			    CSUM_INNER_IP6_TCP;
			if (was_opened)
				mlx5e_open_locked(ifp);
		}
		if (mask & IFCAP_VXLAN_HWTSO) {
			ifp->if_capenable ^= IFCAP_VXLAN_HWTSO;
			ifp->if_hwassist ^= CSUM_INNER_IP_TSO |
			    CSUM_INNER_IP6_TSO;
		}

		VLAN_CAPABILITIES(ifp);
		/* turn off LRO means also turn of HW LRO - if it's on */
		if (mask & IFCAP_LRO) {
			int was_opened = test_bit(MLX5E_STATE_OPENED, &priv->state);
			bool need_restart = false;

			ifp->if_capenable ^= IFCAP_LRO;

			/* figure out if updating HW LRO is needed */
			if (!(ifp->if_capenable & IFCAP_LRO)) {
				if (priv->params.hw_lro_en) {
					priv->params.hw_lro_en = false;
					need_restart = true;
				}
			} else {
				if (priv->params.hw_lro_en == false &&
				    priv->params_ethtool.hw_lro != 0) {
					priv->params.hw_lro_en = true;
					need_restart = true;
				}
			}
			if (was_opened && need_restart) {
				mlx5e_close_locked(ifp);
				mlx5e_open_locked(ifp);
			}
		}
		if (mask & IFCAP_HWRXTSTMP) {
			ifp->if_capenable ^= IFCAP_HWRXTSTMP;
			if (ifp->if_capenable & IFCAP_HWRXTSTMP) {
				if (priv->clbr_done == 0)
					mlx5e_reset_calibration_callout(priv);
			} else {
				callout_drain(&priv->tstmp_clbr);
				priv->clbr_done = 0;
			}
		}
out:
		PRIV_UNLOCK(priv);
		break;

	case SIOCGI2C:
		ifr = (struct ifreq *)data;

		/*
		 * Copy from the user-space address ifr_data to the
		 * kernel-space address i2c
		 */
		error = copyin(ifr_data_get_ptr(ifr), &i2c, sizeof(i2c));
		if (error)
			break;

		if (i2c.len > sizeof(i2c.data)) {
			error = EINVAL;
			break;
		}

		PRIV_LOCK(priv);
		/* Get module_num which is required for the query_eeprom */
		error = mlx5_query_module_num(priv->mdev, &module_num);
		if (error) {
			mlx5_en_err(ifp,
			    "Query module num failed, eeprom reading is not supported\n");
			error = EINVAL;
			goto err_i2c;
		}
		/* Check if module is present before doing an access */
		module_status = mlx5_query_module_status(priv->mdev, module_num);
		if (module_status != MLX5_MODULE_STATUS_PLUGGED_ENABLED) {
			error = EINVAL;
			goto err_i2c;
		}
		/*
		 * Currently 0XA0 and 0xA2 are the only addresses permitted.
		 * The internal conversion is as follows:
		 */
		if (i2c.dev_addr == 0xA0)
			read_addr = MLX5_I2C_ADDR_LOW;
		else if (i2c.dev_addr == 0xA2)
			read_addr = MLX5_I2C_ADDR_HIGH;
		else {
			mlx5_en_err(ifp,
			    "Query eeprom failed, Invalid Address: %X\n",
			    i2c.dev_addr);
			error = EINVAL;
			goto err_i2c;
		}
		error = mlx5_query_eeprom(priv->mdev,
		    read_addr, MLX5_EEPROM_LOW_PAGE,
		    (uint32_t)i2c.offset, (uint32_t)i2c.len, module_num,
		    (uint32_t *)i2c.data, &size_read);
		if (error) {
			mlx5_en_err(ifp,
			    "Query eeprom failed, eeprom reading is not supported\n");
			error = EINVAL;
			goto err_i2c;
		}

		if (i2c.len > MLX5_EEPROM_MAX_BYTES) {
			error = mlx5_query_eeprom(priv->mdev,
			    read_addr, MLX5_EEPROM_LOW_PAGE,
			    (uint32_t)(i2c.offset + size_read),
			    (uint32_t)(i2c.len - size_read), module_num,
			    (uint32_t *)(i2c.data + size_read), &size_read);
		}
		if (error) {
			mlx5_en_err(ifp,
			    "Query eeprom failed, eeprom reading is not supported\n");
			error = EINVAL;
			goto err_i2c;
		}

		error = copyout(&i2c, ifr_data_get_ptr(ifr), sizeof(i2c));
err_i2c:
		PRIV_UNLOCK(priv);
		break;
	case SIOCGIFDOWNREASON:
		ifdr = (struct ifdownreason *)data;
		bzero(ifdr->ifdr_msg, sizeof(ifdr->ifdr_msg));
		PRIV_LOCK(priv);
		error = -mlx5_query_pddr_troubleshooting_info(priv->mdev, NULL,
		    ifdr->ifdr_msg, sizeof(ifdr->ifdr_msg));
		PRIV_UNLOCK(priv);
		if (error == 0)
			ifdr->ifdr_reason = IFDR_REASON_MSG;
		break;

	case SIOCGIFRSSKEY:
		ifrk = (struct ifrsskey *)data;
		ifrk->ifrk_func = RSS_FUNC_TOEPLITZ;
		ifrk->ifrk_keylen = MLX5E_RSS_KEY_SIZE;
		CTASSERT(sizeof(ifrk->ifrk_key) >= MLX5E_RSS_KEY_SIZE);
		mlx5e_get_rss_key(ifrk->ifrk_key);
		break;

	case SIOCGIFRSSHASH:
		ifrh = (struct ifrsshash *)data;
		ifrh->ifrh_func = RSS_FUNC_TOEPLITZ;
		ifrh->ifrh_types =
		    RSS_TYPE_IPV4 |
		    RSS_TYPE_TCP_IPV4 |
		    RSS_TYPE_UDP_IPV4 |
		    RSS_TYPE_IPV6 |
		    RSS_TYPE_TCP_IPV6 |
		    RSS_TYPE_UDP_IPV6;
		break;

	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}
	return (error);
}

static int
mlx5e_check_required_hca_cap(struct mlx5_core_dev *mdev)
{
	/*
	 * TODO: uncoment once FW really sets all these bits if
	 * (!mdev->caps.eth.rss_ind_tbl_cap || !mdev->caps.eth.csum_cap ||
	 * !mdev->caps.eth.max_lso_cap || !mdev->caps.eth.vlan_cap ||
	 * !(mdev->caps.gen.flags & MLX5_DEV_CAP_FLAG_SCQE_BRK_MOD)) return
	 * -ENOTSUPP;
	 */

	/* TODO: add more must-to-have features */

	if (MLX5_CAP_GEN(mdev, port_type) != MLX5_CAP_PORT_TYPE_ETH)
		return (-ENODEV);

	return (0);
}

static u16
mlx5e_get_max_inline_cap(struct mlx5_core_dev *mdev)
{
	const int min_size = ETHER_VLAN_ENCAP_LEN + ETHER_HDR_LEN;
	const int max_size = MLX5E_MAX_TX_INLINE;
	const int bf_buf_size =
	    ((1U << MLX5_CAP_GEN(mdev, log_bf_reg_size)) / 2U) -
	    (sizeof(struct mlx5e_tx_wqe) - 2);

	/* verify against driver limits */
	if (bf_buf_size > max_size)
		return (max_size);
	else if (bf_buf_size < min_size)
		return (min_size);
	else
		return (bf_buf_size);
}

static int
mlx5e_build_ifp_priv(struct mlx5_core_dev *mdev,
    struct mlx5e_priv *priv,
    int num_comp_vectors)
{
	int err;

	/*
	 * TODO: Consider link speed for setting "log_sq_size",
	 * "log_rq_size" and "cq_moderation_xxx":
	 */
	priv->params.log_sq_size =
	    MLX5E_PARAMS_DEFAULT_LOG_SQ_SIZE;
	priv->params.log_rq_size =
	    MLX5E_PARAMS_DEFAULT_LOG_RQ_SIZE;
	priv->params.rx_cq_moderation_usec =
	    MLX5_CAP_GEN(mdev, cq_period_start_from_cqe) ?
	    MLX5E_PARAMS_DEFAULT_RX_CQ_MODERATION_USEC_FROM_CQE :
	    MLX5E_PARAMS_DEFAULT_RX_CQ_MODERATION_USEC;
	priv->params.rx_cq_moderation_mode =
	    MLX5_CAP_GEN(mdev, cq_period_start_from_cqe) ? 1 : 0;
	priv->params.rx_cq_moderation_pkts =
	    MLX5E_PARAMS_DEFAULT_RX_CQ_MODERATION_PKTS;
	priv->params.tx_cq_moderation_usec =
	    MLX5E_PARAMS_DEFAULT_TX_CQ_MODERATION_USEC;
	priv->params.tx_cq_moderation_pkts =
	    MLX5E_PARAMS_DEFAULT_TX_CQ_MODERATION_PKTS;
	priv->params.min_rx_wqes =
	    MLX5E_PARAMS_DEFAULT_MIN_RX_WQES;
	priv->params.rx_hash_log_tbl_sz =
	    (order_base_2(num_comp_vectors) >
	    MLX5E_PARAMS_DEFAULT_RX_HASH_LOG_TBL_SZ) ?
	    order_base_2(num_comp_vectors) :
	    MLX5E_PARAMS_DEFAULT_RX_HASH_LOG_TBL_SZ;
	priv->params.num_tc = 1;
	priv->params.default_vlan_prio = 0;
	priv->counter_set_id = -1;
	priv->params.tx_max_inline = mlx5e_get_max_inline_cap(mdev);

	err = mlx5_query_min_inline(mdev, &priv->params.tx_min_inline_mode);
	if (err)
		return (err);

	/*
	 * hw lro is currently defaulted to off. when it won't anymore we
	 * will consider the HW capability: "!!MLX5_CAP_ETH(mdev, lro_cap)"
	 */
	priv->params.hw_lro_en = false;
	priv->params.lro_wqe_sz = MLX5E_PARAMS_DEFAULT_LRO_WQE_SZ;

	/*
	 * CQE zipping is currently defaulted to off. when it won't
	 * anymore we will consider the HW capability:
	 * "!!MLX5_CAP_GEN(mdev, cqe_compression)"
	 */
	priv->params.cqe_zipping_en = false;

	priv->mdev = mdev;
	priv->params.num_channels = num_comp_vectors;
	priv->params.channels_rsss = 1;
	priv->order_base_2_num_channels = order_base_2(num_comp_vectors);
	priv->queue_mapping_channel_mask =
	    roundup_pow_of_two(num_comp_vectors) - 1;
	priv->num_tc = priv->params.num_tc;
	priv->default_vlan_prio = priv->params.default_vlan_prio;

	INIT_WORK(&priv->update_stats_work, mlx5e_update_stats_work);
	INIT_WORK(&priv->update_carrier_work, mlx5e_update_carrier_work);
	INIT_WORK(&priv->set_rx_mode_work, mlx5e_set_rx_mode_work);

	return (0);
}

static void
mlx5e_mkey_set_relaxed_ordering(struct mlx5_core_dev *mdev, void *mkc)
{
	bool ro_pci_enable =
	    pci_get_relaxed_ordering_enabled(mdev->pdev->dev.bsddev);
	bool ro_write = MLX5_CAP_GEN(mdev, relaxed_ordering_write);
	bool ro_read = MLX5_CAP_GEN(mdev, relaxed_ordering_read);

	MLX5_SET(mkc, mkc, relaxed_ordering_read, ro_pci_enable && ro_read);
	MLX5_SET(mkc, mkc, relaxed_ordering_write, ro_pci_enable && ro_write);
}

static int
mlx5e_create_mkey(struct mlx5e_priv *priv, u32 pdn,
		  struct mlx5_core_mkey *mkey)
{
	struct ifnet *ifp = priv->ifp;
	struct mlx5_core_dev *mdev = priv->mdev;
	int inlen = MLX5_ST_SZ_BYTES(create_mkey_in);
	void *mkc;
	u32 *in;
	int err;

	in = mlx5_vzalloc(inlen);
	if (in == NULL) {
		mlx5_en_err(ifp, "failed to allocate inbox\n");
		return (-ENOMEM);
	}

	mkc = MLX5_ADDR_OF(create_mkey_in, in, memory_key_mkey_entry);
	MLX5_SET(mkc, mkc, access_mode, MLX5_ACCESS_MODE_PA);
	MLX5_SET(mkc, mkc, umr_en, 1);	/* used by HW TLS */
	MLX5_SET(mkc, mkc, lw, 1);
	MLX5_SET(mkc, mkc, lr, 1);
	mlx5e_mkey_set_relaxed_ordering(mdev, mkc);
	MLX5_SET(mkc, mkc, pd, pdn);
	MLX5_SET(mkc, mkc, length64, 1);
	MLX5_SET(mkc, mkc, qpn, 0xffffff);

	err = mlx5_core_create_mkey(mdev, mkey, in, inlen);
	if (err)
		mlx5_en_err(ifp, "mlx5_core_create_mkey failed, %d\n",
		    err);

	kvfree(in);
	return (err);
}

static const char *mlx5e_vport_stats_desc[] = {
	MLX5E_VPORT_STATS(MLX5E_STATS_DESC)
};

static const char *mlx5e_pport_stats_desc[] = {
	MLX5E_PPORT_STATS(MLX5E_STATS_DESC)
};

static int
mlx5e_priv_static_init(struct mlx5e_priv *priv, struct mlx5_core_dev *mdev,
    const uint32_t channels)
{
	uint32_t x;
	int err;

	mtx_init(&priv->async_events_mtx, "mlx5async", MTX_NETWORK_LOCK, MTX_DEF);
	sx_init(&priv->state_lock, "mlx5state");
	callout_init_mtx(&priv->watchdog, &priv->async_events_mtx, 0);
	MLX5_INIT_DOORBELL_LOCK(&priv->doorbell_lock);
	for (x = 0; x != channels; x++)
		mlx5e_chan_static_init(priv, &priv->channel[x], x);

	for (x = 0; x != channels; x++) {
		err = mlx5_alloc_bfreg(mdev, &priv->channel[x].bfreg, false, false);
		if (err)
			goto err_alloc_bfreg;
	}
	return (0);

err_alloc_bfreg:
	while (x--)
		mlx5_free_bfreg(mdev, &priv->channel[x].bfreg);

	for (x = 0; x != channels; x++)
		mlx5e_chan_static_destroy(&priv->channel[x]);
	callout_drain(&priv->watchdog);
	mtx_destroy(&priv->async_events_mtx);
	sx_destroy(&priv->state_lock);
	return (err);
}

static void
mlx5e_priv_static_destroy(struct mlx5e_priv *priv, struct mlx5_core_dev *mdev,
    const uint32_t channels)
{
	uint32_t x;

	for (x = 0; x != channels; x++)
		mlx5_free_bfreg(mdev, &priv->channel[x].bfreg);
	for (x = 0; x != channels; x++)
		mlx5e_chan_static_destroy(&priv->channel[x]);
	callout_drain(&priv->watchdog);
	mtx_destroy(&priv->async_events_mtx);
	sx_destroy(&priv->state_lock);
}

static int
sysctl_firmware(SYSCTL_HANDLER_ARGS)
{
	/*
	 * %d.%d%.d the string format.
	 * fw_rev_{maj,min,sub} return u16, 2^16 = 65536.
	 * We need at most 5 chars to store that.
	 * It also has: two "." and NULL at the end, which means we need 18
	 * (5*3 + 3) chars at most.
	 */
	char fw[18];
	struct mlx5e_priv *priv = arg1;
	int error;

	snprintf(fw, sizeof(fw), "%d.%d.%d", fw_rev_maj(priv->mdev), fw_rev_min(priv->mdev),
	    fw_rev_sub(priv->mdev));
	error = sysctl_handle_string(oidp, fw, sizeof(fw), req);
	return (error);
}

static void
mlx5e_disable_tx_dma(struct mlx5e_channel *ch)
{
	int i;

	for (i = 0; i < ch->priv->num_tc; i++)
		mlx5e_drain_sq(&ch->sq[i]);
}

static void
mlx5e_reset_sq_doorbell_record(struct mlx5e_sq *sq)
{

	sq->doorbell.d32[0] = cpu_to_be32(MLX5_OPCODE_NOP);
	sq->doorbell.d32[1] = cpu_to_be32(sq->sqn << 8);
	mlx5e_tx_notify_hw(sq, sq->doorbell.d32);
	sq->doorbell.d64 = 0;
}

void
mlx5e_resume_sq(struct mlx5e_sq *sq)
{
	int err;

	/* check if already enabled */
	if (READ_ONCE(sq->running) != 0)
		return;

	err = mlx5e_modify_sq(sq, MLX5_SQC_STATE_ERR,
	    MLX5_SQC_STATE_RST);
	if (err != 0) {
		mlx5_en_err(sq->ifp,
		    "mlx5e_modify_sq() from ERR to RST failed: %d\n", err);
	}

	sq->cc = 0;
	sq->pc = 0;

	/* reset doorbell prior to moving from RST to RDY */
	mlx5e_reset_sq_doorbell_record(sq);

	err = mlx5e_modify_sq(sq, MLX5_SQC_STATE_RST,
	    MLX5_SQC_STATE_RDY);
	if (err != 0) {
		mlx5_en_err(sq->ifp,
		    "mlx5e_modify_sq() from RST to RDY failed: %d\n", err);
	}

	sq->cev_next_state = MLX5E_CEV_STATE_INITIAL;
	WRITE_ONCE(sq->running, 1);
}

static void
mlx5e_enable_tx_dma(struct mlx5e_channel *ch)
{
        int i;

	for (i = 0; i < ch->priv->num_tc; i++)
		mlx5e_resume_sq(&ch->sq[i]);
}

static void
mlx5e_disable_rx_dma(struct mlx5e_channel *ch)
{
	struct mlx5e_rq *rq = &ch->rq;
	struct epoch_tracker et;
	int err;

	mtx_lock(&rq->mtx);
	rq->enabled = 0;
	callout_stop(&rq->watchdog);
	mtx_unlock(&rq->mtx);

	err = mlx5e_modify_rq(rq, MLX5_RQC_STATE_RDY, MLX5_RQC_STATE_ERR);
	if (err != 0) {
		mlx5_en_err(rq->ifp,
		    "mlx5e_modify_rq() from RDY to RST failed: %d\n", err);
	}

	while (!mlx5_wq_ll_is_empty(&rq->wq)) {
		msleep(1);
		NET_EPOCH_ENTER(et);
		rq->cq.mcq.comp(&rq->cq.mcq, NULL);
		NET_EPOCH_EXIT(et);
	}

	/*
	 * Transitioning into RST state will allow the FW to track less ERR state queues,
	 * thus reducing the recv queue flushing time
	 */
	err = mlx5e_modify_rq(rq, MLX5_RQC_STATE_ERR, MLX5_RQC_STATE_RST);
	if (err != 0) {
		mlx5_en_err(rq->ifp,
		    "mlx5e_modify_rq() from ERR to RST failed: %d\n", err);
	}
}

static void
mlx5e_enable_rx_dma(struct mlx5e_channel *ch)
{
	struct mlx5e_rq *rq = &ch->rq;
	struct epoch_tracker et;
	int err;

	rq->wq.wqe_ctr = 0;
	mlx5_wq_ll_update_db_record(&rq->wq);
	err = mlx5e_modify_rq(rq, MLX5_RQC_STATE_RST, MLX5_RQC_STATE_RDY);
	if (err != 0) {
		mlx5_en_err(rq->ifp,
		    "mlx5e_modify_rq() from RST to RDY failed: %d\n", err);
        }

	rq->enabled = 1;

	NET_EPOCH_ENTER(et);
	rq->cq.mcq.comp(&rq->cq.mcq, NULL);
	NET_EPOCH_EXIT(et);
}

void
mlx5e_modify_tx_dma(struct mlx5e_priv *priv, uint8_t value)
{
	int i;

	if (test_bit(MLX5E_STATE_OPENED, &priv->state) == 0)
		return;

	for (i = 0; i < priv->params.num_channels; i++) {
		if (value)
			mlx5e_disable_tx_dma(&priv->channel[i]);
		else
			mlx5e_enable_tx_dma(&priv->channel[i]);
	}
}

void
mlx5e_modify_rx_dma(struct mlx5e_priv *priv, uint8_t value)
{
	int i;

	if (test_bit(MLX5E_STATE_OPENED, &priv->state) == 0)
		return;

	for (i = 0; i < priv->params.num_channels; i++) {
		if (value)
			mlx5e_disable_rx_dma(&priv->channel[i]);
		else
			mlx5e_enable_rx_dma(&priv->channel[i]);
	}
}

static void
mlx5e_add_hw_stats(struct mlx5e_priv *priv)
{
	SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_hw),
	    OID_AUTO, "fw_version", CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    priv, 0, sysctl_firmware, "A", "HCA firmware version");

	SYSCTL_ADD_STRING(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_hw),
	    OID_AUTO, "board_id", CTLFLAG_RD, priv->mdev->board_id, 0,
	    "Board ID");
}

static int
mlx5e_sysctl_tx_priority_flow_control(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	uint8_t temp[MLX5E_MAX_PRIORITY];
	uint32_t tx_pfc;
	int err;
	int i;

	PRIV_LOCK(priv);

	tx_pfc = priv->params.tx_priority_flow_control;

	for (i = 0; i != MLX5E_MAX_PRIORITY; i++)
		temp[i] = (tx_pfc >> i) & 1;

	err = SYSCTL_OUT(req, temp, MLX5E_MAX_PRIORITY);
	if (err || !req->newptr)
		goto done;
	err = SYSCTL_IN(req, temp, MLX5E_MAX_PRIORITY);
	if (err)
		goto done;

	priv->params.tx_priority_flow_control = 0;

	/* range check input value */
	for (i = 0; i != MLX5E_MAX_PRIORITY; i++) {
		if (temp[i] > 1) {
			err = ERANGE;
			goto done;
		}
		priv->params.tx_priority_flow_control |= (temp[i] << i);
	}

	/* check if update is required */
	if (tx_pfc != priv->params.tx_priority_flow_control)
		err = -mlx5e_set_port_pfc(priv);
done:
	if (err != 0)
		priv->params.tx_priority_flow_control= tx_pfc;
	PRIV_UNLOCK(priv);

	return (err);
}

static int
mlx5e_sysctl_rx_priority_flow_control(SYSCTL_HANDLER_ARGS)
{
	struct mlx5e_priv *priv = arg1;
	uint8_t temp[MLX5E_MAX_PRIORITY];
	uint32_t rx_pfc;
	int err;
	int i;

	PRIV_LOCK(priv);

	rx_pfc = priv->params.rx_priority_flow_control;

	for (i = 0; i != MLX5E_MAX_PRIORITY; i++)
		temp[i] = (rx_pfc >> i) & 1;

	err = SYSCTL_OUT(req, temp, MLX5E_MAX_PRIORITY);
	if (err || !req->newptr)
		goto done;
	err = SYSCTL_IN(req, temp, MLX5E_MAX_PRIORITY);
	if (err)
		goto done;

	priv->params.rx_priority_flow_control = 0;

	/* range check input value */
	for (i = 0; i != MLX5E_MAX_PRIORITY; i++) {
		if (temp[i] > 1) {
			err = ERANGE;
			goto done;
		}
		priv->params.rx_priority_flow_control |= (temp[i] << i);
	}

	/* check if update is required */
	if (rx_pfc != priv->params.rx_priority_flow_control) {
		err = -mlx5e_set_port_pfc(priv);
		if (err == 0 && priv->sw_is_port_buf_owner)
			err = mlx5e_update_buf_lossy(priv);
	}
done:
	if (err != 0)
		priv->params.rx_priority_flow_control= rx_pfc;
	PRIV_UNLOCK(priv);

	return (err);
}

static void
mlx5e_setup_pauseframes(struct mlx5e_priv *priv)
{
#if (__FreeBSD_version < 1100000)
	char path[96];
#endif
	int error;

	/* enable pauseframes by default */
	priv->params.tx_pauseframe_control = 1;
	priv->params.rx_pauseframe_control = 1;

	/* disable ports flow control, PFC, by default */
	priv->params.tx_priority_flow_control = 0;
	priv->params.rx_priority_flow_control = 0;

#if (__FreeBSD_version < 1100000)
	/* compute path for sysctl */
	snprintf(path, sizeof(path), "dev.mce.%d.tx_pauseframe_control",
	    device_get_unit(priv->mdev->pdev->dev.bsddev));

	/* try to fetch tunable, if any */
	TUNABLE_INT_FETCH(path, &priv->params.tx_pauseframe_control);

	/* compute path for sysctl */
	snprintf(path, sizeof(path), "dev.mce.%d.rx_pauseframe_control",
	    device_get_unit(priv->mdev->pdev->dev.bsddev));

	/* try to fetch tunable, if any */
	TUNABLE_INT_FETCH(path, &priv->params.rx_pauseframe_control);
#endif

	/* register pauseframe SYSCTLs */
	SYSCTL_ADD_INT(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    OID_AUTO, "tx_pauseframe_control", CTLFLAG_RDTUN,
	    &priv->params.tx_pauseframe_control, 0,
	    "Set to enable TX pause frames. Clear to disable.");

	SYSCTL_ADD_INT(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    OID_AUTO, "rx_pauseframe_control", CTLFLAG_RDTUN,
	    &priv->params.rx_pauseframe_control, 0,
	    "Set to enable RX pause frames. Clear to disable.");

	/* register priority flow control, PFC, SYSCTLs */
	SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    OID_AUTO, "tx_priority_flow_control", CTLTYPE_U8 | CTLFLAG_RWTUN |
	    CTLFLAG_MPSAFE, priv, 0, &mlx5e_sysctl_tx_priority_flow_control, "CU",
	    "Set to enable TX ports flow control frames for priorities 0..7. Clear to disable.");

	SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    OID_AUTO, "rx_priority_flow_control", CTLTYPE_U8 | CTLFLAG_RWTUN |
	    CTLFLAG_MPSAFE, priv, 0, &mlx5e_sysctl_rx_priority_flow_control, "CU",
	    "Set to enable RX ports flow control frames for priorities 0..7. Clear to disable.");

	PRIV_LOCK(priv);

	/* range check */
	priv->params.tx_pauseframe_control =
	    priv->params.tx_pauseframe_control ? 1 : 0;
	priv->params.rx_pauseframe_control =
	    priv->params.rx_pauseframe_control ? 1 : 0;

	/* update firmware */
	error = mlx5e_set_port_pause_and_pfc(priv);
	if (error == -EINVAL) {
		mlx5_en_err(priv->ifp,
		    "Global pauseframes must be disabled before enabling PFC.\n");
		priv->params.rx_priority_flow_control = 0;
		priv->params.tx_priority_flow_control = 0;

		/* update firmware */
		(void) mlx5e_set_port_pause_and_pfc(priv);
	}
	PRIV_UNLOCK(priv);
}

static int
mlx5e_ul_snd_tag_alloc(struct ifnet *ifp,
    union if_snd_tag_alloc_params *params,
    struct m_snd_tag **ppmt)
{
	struct mlx5e_priv *priv;
	struct mlx5e_channel *pch;

	priv = ifp->if_softc;

	if (unlikely(priv->gone || params->hdr.flowtype == M_HASHTYPE_NONE)) {
		return (EOPNOTSUPP);
	} else {
		/* keep this code synced with mlx5e_select_queue() */
		u32 ch = priv->params.num_channels;
#ifdef RSS
		u32 temp;

		if (rss_hash2bucket(params->hdr.flowid,
		    params->hdr.flowtype, &temp) == 0)
			ch = temp % ch;
		else
#endif
			ch = (params->hdr.flowid % 128) % ch;

		/*
		 * NOTE: The channels array is only freed at detach
		 * and it safe to return a pointer to the send tag
		 * inside the channels structure as long as we
		 * reference the priv.
		 */
		pch = priv->channel + ch;

		/* check if send queue is not running */
		if (unlikely(pch->sq[0].running == 0))
			return (ENXIO);
		m_snd_tag_ref(&pch->tag);
		*ppmt = &pch->tag;
		return (0);
	}
}

static int
mlx5e_ul_snd_tag_query(struct m_snd_tag *pmt, union if_snd_tag_query_params *params)
{
	struct mlx5e_channel *pch =
	    container_of(pmt, struct mlx5e_channel, tag);

	params->unlimited.max_rate = -1ULL;
	params->unlimited.queue_level = mlx5e_sq_queue_level(&pch->sq[0]);
	return (0);
}

static void
mlx5e_ul_snd_tag_free(struct m_snd_tag *pmt)
{
	struct mlx5e_channel *pch =
	    container_of(pmt, struct mlx5e_channel, tag);

	complete(&pch->completion);
}

static int
mlx5e_snd_tag_alloc(struct ifnet *ifp,
    union if_snd_tag_alloc_params *params,
    struct m_snd_tag **ppmt)
{

	switch (params->hdr.type) {
#ifdef RATELIMIT
	case IF_SND_TAG_TYPE_RATE_LIMIT:
		return (mlx5e_rl_snd_tag_alloc(ifp, params, ppmt));
#ifdef KERN_TLS
	case IF_SND_TAG_TYPE_TLS_RATE_LIMIT:
		return (mlx5e_tls_snd_tag_alloc(ifp, params, ppmt));
#endif
#endif
	case IF_SND_TAG_TYPE_UNLIMITED:
		return (mlx5e_ul_snd_tag_alloc(ifp, params, ppmt));
#ifdef KERN_TLS
	case IF_SND_TAG_TYPE_TLS:
		return (mlx5e_tls_snd_tag_alloc(ifp, params, ppmt));
#endif
	default:
		return (EOPNOTSUPP);
	}
}

#ifdef RATELIMIT
#define NUM_HDWR_RATES_MLX 13
static const uint64_t adapter_rates_mlx[NUM_HDWR_RATES_MLX] = {
	135375,			/* 1,083,000 */
	180500,			/* 1,444,000 */
	270750,			/* 2,166,000 */
	361000,			/* 2,888,000 */
	541500,			/* 4,332,000 */
	721875,			/* 5,775,000 */
	1082875,		/* 8,663,000 */
	1443875,		/* 11,551,000 */
	2165750,		/* 17,326,000 */
	2887750,		/* 23,102,000 */
	4331625,		/* 34,653,000 */
	5775500,		/* 46,204,000 */
	8663125			/* 69,305,000 */
};

static void
mlx5e_ratelimit_query(struct ifnet *ifp __unused, struct if_ratelimit_query_results *q)
{
	/*
	 * This function needs updating by the driver maintainer!
	 * For the MLX card there are currently (ConectX-4?) 13 
	 * pre-set rates and others i.e. ConnectX-5, 6, 7??
	 *
	 * This will change based on later adapters
	 * and this code should be updated to look at ifp
	 * and figure out the specific adapter type
	 * settings i.e. how many rates as well
	 * as if they are fixed (as is shown here) or
	 * if they are dynamic (example chelsio t4). Also if there
	 * is a maximum number of flows that the adapter
	 * can handle that too needs to be updated in
	 * the max_flows field.
	 */
	q->rate_table = adapter_rates_mlx;
	q->flags = RT_IS_FIXED_TABLE;
	q->max_flows = 0;	/* mlx has no limit */
	q->number_of_rates = NUM_HDWR_RATES_MLX;
	q->min_segment_burst = 1;
}
#endif

static void
mlx5e_ifm_add(struct mlx5e_priv *priv, int type)
{
	ifmedia_add(&priv->media, type | IFM_ETHER, 0, NULL);
	ifmedia_add(&priv->media, type | IFM_ETHER |
	    IFM_ETH_RXPAUSE | IFM_ETH_TXPAUSE, 0, NULL);
	ifmedia_add(&priv->media, type | IFM_ETHER | IFM_ETH_RXPAUSE, 0, NULL);
	ifmedia_add(&priv->media, type | IFM_ETHER | IFM_ETH_TXPAUSE, 0, NULL);
	ifmedia_add(&priv->media, type | IFM_ETHER | IFM_FDX, 0, NULL);
	ifmedia_add(&priv->media, type | IFM_ETHER | IFM_FDX |
	    IFM_ETH_RXPAUSE, 0, NULL);
	ifmedia_add(&priv->media, type | IFM_ETHER | IFM_FDX |
	    IFM_ETH_TXPAUSE, 0, NULL);
	ifmedia_add(&priv->media, type | IFM_ETHER | IFM_FDX |
	    IFM_ETH_RXPAUSE | IFM_ETH_TXPAUSE, 0, NULL);
}

static void *
mlx5e_create_ifp(struct mlx5_core_dev *mdev)
{
	struct ifnet *ifp;
	struct mlx5e_priv *priv;
	u8 dev_addr[ETHER_ADDR_LEN] __aligned(4);
	struct sysctl_oid_list *child;
	int ncv = mdev->priv.eq_table.num_comp_vectors;
	char unit[16];
	struct pfil_head_args pa;
	int err;
	u32 eth_proto_cap;
	u32 out[MLX5_ST_SZ_DW(ptys_reg)];
	bool ext;
	struct media media_entry = {};

	if (mlx5e_check_required_hca_cap(mdev)) {
		mlx5_core_dbg(mdev, "mlx5e_check_required_hca_cap() failed\n");
		return (NULL);
	}

	/*
	 * Try to allocate the priv and make room for worst-case
	 * number of channel structures:
	 */
	priv = malloc_domainset(sizeof(*priv) +
	    (sizeof(priv->channel[0]) * mdev->priv.eq_table.num_comp_vectors),
	    M_MLX5EN, mlx5_dev_domainset(mdev), M_WAITOK | M_ZERO);

	ifp = priv->ifp = if_alloc_dev(IFT_ETHER, mdev->pdev->dev.bsddev);
	if (ifp == NULL) {
		mlx5_core_err(mdev, "if_alloc() failed\n");
		goto err_free_priv;
	}
	/* setup all static fields */
	if (mlx5e_priv_static_init(priv, mdev, mdev->priv.eq_table.num_comp_vectors)) {
		mlx5_core_err(mdev, "mlx5e_priv_static_init() failed\n");
		goto err_free_ifp;
	}

	ifp->if_softc = priv;
	if_initname(ifp, "mce", device_get_unit(mdev->pdev->dev.bsddev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_init = mlx5e_open;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_KNOWSEPOCH;
	ifp->if_ioctl = mlx5e_ioctl;
	ifp->if_transmit = mlx5e_xmit;
	ifp->if_qflush = if_qflush;
#if (__FreeBSD_version >= 1100000)
	ifp->if_get_counter = mlx5e_get_counter;
#endif
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	/*
         * Set driver features
         */
	ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6;
	ifp->if_capabilities |= IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING;
	ifp->if_capabilities |= IFCAP_VLAN_HWCSUM | IFCAP_VLAN_HWFILTER;
	ifp->if_capabilities |= IFCAP_LINKSTATE | IFCAP_JUMBO_MTU;
	ifp->if_capabilities |= IFCAP_LRO;
	ifp->if_capabilities |= IFCAP_TSO | IFCAP_VLAN_HWTSO;
	ifp->if_capabilities |= IFCAP_HWSTATS | IFCAP_HWRXTSTMP;
	ifp->if_capabilities |= IFCAP_MEXTPG;
	ifp->if_capabilities |= IFCAP_TXTLS4 | IFCAP_TXTLS6;
#ifdef RATELIMIT
	ifp->if_capabilities |= IFCAP_TXRTLMT | IFCAP_TXTLS_RTLMT;
#endif
	ifp->if_capabilities |= IFCAP_VXLAN_HWCSUM | IFCAP_VXLAN_HWTSO;
	ifp->if_snd_tag_alloc = mlx5e_snd_tag_alloc;
#ifdef RATELIMIT
	ifp->if_ratelimit_query = mlx5e_ratelimit_query;
#endif
	/* set TSO limits so that we don't have to drop TX packets */
	ifp->if_hw_tsomax = MLX5E_MAX_TX_PAYLOAD_SIZE - (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN);
	ifp->if_hw_tsomaxsegcount = MLX5E_MAX_TX_MBUF_FRAGS - 1 /* hdr */;
	ifp->if_hw_tsomaxsegsize = MLX5E_MAX_TX_MBUF_SIZE;

	ifp->if_capenable = ifp->if_capabilities;
	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TSO)
		ifp->if_hwassist |= CSUM_TSO;
	if (ifp->if_capenable & IFCAP_TXCSUM)
		ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP | CSUM_IP);
	if (ifp->if_capenable & IFCAP_TXCSUM_IPV6)
		ifp->if_hwassist |= (CSUM_UDP_IPV6 | CSUM_TCP_IPV6);
	if (ifp->if_capabilities & IFCAP_VXLAN_HWCSUM)
		ifp->if_hwassist |= CSUM_INNER_IP6_UDP | CSUM_INNER_IP6_TCP |
		    CSUM_INNER_IP | CSUM_INNER_IP_UDP | CSUM_INNER_IP_TCP |
		    CSUM_ENCAP_VXLAN;
	if (ifp->if_capabilities  & IFCAP_VXLAN_HWTSO)
		ifp->if_hwassist |= CSUM_INNER_IP6_TSO | CSUM_INNER_IP_TSO;

	/* ifnet sysctl tree */
	sysctl_ctx_init(&priv->sysctl_ctx);
	priv->sysctl_ifnet = SYSCTL_ADD_NODE(&priv->sysctl_ctx, SYSCTL_STATIC_CHILDREN(_dev),
	    OID_AUTO, ifp->if_dname, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
	    "MLX5 ethernet - interface name");
	if (priv->sysctl_ifnet == NULL) {
		mlx5_core_err(mdev, "SYSCTL_ADD_NODE() failed\n");
		goto err_free_sysctl;
	}
	snprintf(unit, sizeof(unit), "%d", ifp->if_dunit);
	priv->sysctl_ifnet = SYSCTL_ADD_NODE(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    OID_AUTO, unit, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
	    "MLX5 ethernet - interface unit");
	if (priv->sysctl_ifnet == NULL) {
		mlx5_core_err(mdev, "SYSCTL_ADD_NODE() failed\n");
		goto err_free_sysctl;
	}

	/* HW sysctl tree */
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(mdev->pdev->dev.bsddev));
	priv->sysctl_hw = SYSCTL_ADD_NODE(&priv->sysctl_ctx, child,
	    OID_AUTO, "hw", CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
	    "MLX5 ethernet dev hw");
	if (priv->sysctl_hw == NULL) {
		mlx5_core_err(mdev, "SYSCTL_ADD_NODE() failed\n");
		goto err_free_sysctl;
	}

	err = mlx5e_build_ifp_priv(mdev, priv, ncv);
	if (err) {
		mlx5_core_err(mdev, "mlx5e_build_ifp_priv() failed (%d)\n", err);
		goto err_free_sysctl;
	}

	/* reuse mlx5core's watchdog workqueue */
	priv->wq = mdev->priv.health.wq_watchdog;

	err = mlx5_core_alloc_pd(mdev, &priv->pdn, 0);
	if (err) {
		mlx5_en_err(ifp, "mlx5_core_alloc_pd failed, %d\n", err);
		goto err_free_wq;
	}
	err = mlx5_alloc_transport_domain(mdev, &priv->tdn, 0);
	if (err) {
		mlx5_en_err(ifp,
		    "mlx5_alloc_transport_domain failed, %d\n", err);
		goto err_dealloc_pd;
	}
	err = mlx5e_create_mkey(priv, priv->pdn, &priv->mr);
	if (err) {
		mlx5_en_err(ifp, "mlx5e_create_mkey failed, %d\n", err);
		goto err_dealloc_transport_domain;
	}
	mlx5_query_nic_vport_mac_address(priv->mdev, 0, dev_addr);

	/* check if we should generate a random MAC address */
	if (MLX5_CAP_GEN(priv->mdev, vport_group_manager) == 0 &&
	    is_zero_ether_addr(dev_addr)) {
		random_ether_addr(dev_addr);
		mlx5_en_err(ifp, "Assigned random MAC address\n");
	}

	err = mlx5e_rl_init(priv);
	if (err) {
		mlx5_en_err(ifp, "mlx5e_rl_init failed, %d\n", err);
		goto err_create_mkey;
	}

	err = mlx5e_tls_init(priv);
	if (err) {
		if_printf(ifp, "%s: mlx5e_tls_init failed\n", __func__);
		goto err_rl_init;
	}

	/* set default MTU */
	mlx5e_set_dev_port_mtu(ifp, ifp->if_mtu);

	/* Set default media status */
	priv->media_status_last = IFM_AVALID;
	priv->media_active_last = IFM_ETHER | IFM_AUTO | IFM_FDX;

	/* setup default pauseframes configuration */
	mlx5e_setup_pauseframes(priv);

	/* Setup supported medias */
	if (!mlx5_query_port_ptys(mdev, out, sizeof(out), MLX5_PTYS_EN, 1)) {
		ext = MLX5_CAP_PCAM_FEATURE(mdev,
		    ptys_extended_ethernet);
		eth_proto_cap = MLX5_GET_ETH_PROTO(ptys_reg, out, ext,
		    eth_proto_capability);
	} else {
		ext = false;
		eth_proto_cap = 0;
		mlx5_en_err(ifp, "Query port media capability failed, %d\n", err);
	}

	ifmedia_init(&priv->media, IFM_IMASK,
	    mlx5e_media_change, mlx5e_media_status);

	if (ext) {
		for (unsigned i = 0; i != MLX5E_EXT_LINK_SPEEDS_NUMBER; i++) {
			/* check if hardware has the right capability */
			if (MLX5E_PROT_MASK(i) & ~eth_proto_cap)
				continue;
			for (unsigned j = 0; j != MLX5E_CABLE_TYPE_NUMBER; j++) {
				media_entry = mlx5e_ext_mode_table[i][j];
				if (media_entry.subtype == 0)
					continue;
				/* check if this subtype was already added */
				for (unsigned k = 0; k != i; k++) {
					/* check if hardware has the right capability */
					if (MLX5E_PROT_MASK(k) & ~eth_proto_cap)
						continue;
					for (unsigned m = 0; m != MLX5E_CABLE_TYPE_NUMBER; m++) {
						if (media_entry.subtype == mlx5e_ext_mode_table[k][m].subtype)
							goto skip_ext_media;
					}
				}
				mlx5e_ifm_add(priv, media_entry.subtype);
			skip_ext_media:;
			}
		}
	} else {
		for (unsigned i = 0; i != MLX5E_LINK_SPEEDS_NUMBER; i++) {
			media_entry = mlx5e_mode_table[i];
			if (media_entry.subtype == 0)
				continue;
			if (MLX5E_PROT_MASK(i) & ~eth_proto_cap)
				continue;
			/* check if this subtype was already added */
			for (unsigned k = 0; k != i; k++) {
				if (media_entry.subtype == mlx5e_mode_table[k].subtype)
					goto skip_media;
			}
			mlx5e_ifm_add(priv, media_entry.subtype);

			/* NOTE: 10G ER and LR shares the same entry */
			if (media_entry.subtype == IFM_10G_ER)
				mlx5e_ifm_add(priv, IFM_10G_LR);
		skip_media:;
		}
	}

	mlx5e_ifm_add(priv, IFM_AUTO);

	/* Set autoselect by default */
	ifmedia_set(&priv->media, IFM_ETHER | IFM_AUTO | IFM_FDX |
	    IFM_ETH_RXPAUSE | IFM_ETH_TXPAUSE);

	DEBUGNET_SET(ifp, mlx5_en);

	ether_ifattach(ifp, dev_addr);

	/* Register for VLAN events */
	priv->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    mlx5e_vlan_rx_add_vid, priv, EVENTHANDLER_PRI_FIRST);
	priv->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    mlx5e_vlan_rx_kill_vid, priv, EVENTHANDLER_PRI_FIRST);

	/* Register for VxLAN events */
	priv->vxlan_start = EVENTHANDLER_REGISTER(vxlan_start,
	    mlx5e_vxlan_start, priv, EVENTHANDLER_PRI_ANY);
	priv->vxlan_stop = EVENTHANDLER_REGISTER(vxlan_stop,
	    mlx5e_vxlan_stop, priv, EVENTHANDLER_PRI_ANY);

	/* Link is down by default */
	if_link_state_change(ifp, LINK_STATE_DOWN);

	mlx5e_enable_async_events(priv);

	mlx5e_add_hw_stats(priv);

	mlx5e_create_stats(&priv->stats.vport.ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    "vstats", mlx5e_vport_stats_desc, MLX5E_VPORT_STATS_NUM,
	    priv->stats.vport.arg);

	mlx5e_create_stats(&priv->stats.pport.ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    "pstats", mlx5e_pport_stats_desc, MLX5E_PPORT_STATS_NUM,
	    priv->stats.pport.arg);

	mlx5e_create_ethtool(priv);

	mtx_lock(&priv->async_events_mtx);
	mlx5e_update_stats(priv);
	mtx_unlock(&priv->async_events_mtx);

	SYSCTL_ADD_INT(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    OID_AUTO, "rx_clbr_done", CTLFLAG_RD,
	    &priv->clbr_done, 0,
	    "RX timestamps calibration state");
	callout_init(&priv->tstmp_clbr, 1);
	mlx5e_reset_calibration_callout(priv);

	pa.pa_version = PFIL_VERSION;
	pa.pa_flags = PFIL_IN;
	pa.pa_type = PFIL_TYPE_ETHERNET;
	pa.pa_headname = ifp->if_xname;
	priv->pfil = pfil_head_register(&pa);

	return (priv);

err_rl_init:
	mlx5e_rl_cleanup(priv);

err_create_mkey:
	mlx5_core_destroy_mkey(priv->mdev, &priv->mr);

err_dealloc_transport_domain:
	mlx5_dealloc_transport_domain(mdev, priv->tdn, 0);

err_dealloc_pd:
	mlx5_core_dealloc_pd(mdev, priv->pdn, 0);

err_free_wq:
	flush_workqueue(priv->wq);

err_free_sysctl:
	sysctl_ctx_free(&priv->sysctl_ctx);
	if (priv->sysctl_debug)
		sysctl_ctx_free(&priv->stats.port_stats_debug.ctx);
	mlx5e_priv_static_destroy(priv, mdev, mdev->priv.eq_table.num_comp_vectors);

err_free_ifp:
	if_free(ifp);

err_free_priv:
	free(priv, M_MLX5EN);
	return (NULL);
}

static void
mlx5e_destroy_ifp(struct mlx5_core_dev *mdev, void *vpriv)
{
	struct mlx5e_priv *priv = vpriv;
	struct ifnet *ifp = priv->ifp;

	/* don't allow more IOCTLs */
	priv->gone = 1;

	/* XXX wait a bit to allow IOCTL handlers to complete */
	pause("W", hz);

#ifdef RATELIMIT
	/*
	 * The kernel can have reference(s) via the m_snd_tag's into
	 * the ratelimit channels, and these must go away before
	 * detaching:
	 */
	while (READ_ONCE(priv->rl.stats.tx_active_connections) != 0) {
		mlx5_en_err(priv->ifp,
		    "Waiting for all ratelimit connections to terminate\n");
		pause("W", hz);
	}
#endif

#ifdef KERN_TLS
	/* wait for all TLS tags to get freed */
	while (priv->tls.init != 0 &&
	    uma_zone_get_cur(priv->tls.zone) != 0)  {
		mlx5_en_err(priv->ifp,
		    "Waiting for all TLS connections to terminate\n");
		pause("W", hz);
	}
#endif
	/* wait for all unlimited send tags to complete */
	mlx5e_priv_wait_for_completion(priv, mdev->priv.eq_table.num_comp_vectors);

	/* stop watchdog timer */
	callout_drain(&priv->watchdog);

	callout_drain(&priv->tstmp_clbr);

	if (priv->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, priv->vlan_attach);
	if (priv->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, priv->vlan_detach);
	if (priv->vxlan_start != NULL)
		EVENTHANDLER_DEREGISTER(vxlan_start, priv->vxlan_start);
	if (priv->vxlan_stop != NULL)
		EVENTHANDLER_DEREGISTER(vxlan_stop, priv->vxlan_stop);

	/* make sure device gets closed */
	PRIV_LOCK(priv);
	mlx5e_close_locked(ifp);
	PRIV_UNLOCK(priv);

	/* deregister pfil */
	if (priv->pfil != NULL) {
		pfil_head_unregister(priv->pfil);
		priv->pfil = NULL;
	}

	/* unregister device */
	ifmedia_removeall(&priv->media);
	ether_ifdetach(ifp);

	mlx5e_tls_cleanup(priv);
	mlx5e_rl_cleanup(priv);

	/* destroy all remaining sysctl nodes */
	sysctl_ctx_free(&priv->stats.vport.ctx);
	sysctl_ctx_free(&priv->stats.pport.ctx);
	if (priv->sysctl_debug)
		sysctl_ctx_free(&priv->stats.port_stats_debug.ctx);
	sysctl_ctx_free(&priv->sysctl_ctx);

	mlx5_core_destroy_mkey(priv->mdev, &priv->mr);
	mlx5_dealloc_transport_domain(priv->mdev, priv->tdn, 0);
	mlx5_core_dealloc_pd(priv->mdev, priv->pdn, 0);
	mlx5e_disable_async_events(priv);
	flush_workqueue(priv->wq);
	mlx5e_priv_static_destroy(priv, mdev, mdev->priv.eq_table.num_comp_vectors);
	if_free(ifp);
	free(priv, M_MLX5EN);
}

#ifdef DEBUGNET
static void
mlx5_en_debugnet_init(struct ifnet *dev, int *nrxr, int *ncl, int *clsize)
{
	struct mlx5e_priv *priv = if_getsoftc(dev);

	PRIV_LOCK(priv);
	*nrxr = priv->params.num_channels;
	*ncl = DEBUGNET_MAX_IN_FLIGHT;
	*clsize = MLX5E_MAX_RX_BYTES;
	PRIV_UNLOCK(priv);
}

static void
mlx5_en_debugnet_event(struct ifnet *dev, enum debugnet_ev event)
{
}

static int
mlx5_en_debugnet_transmit(struct ifnet *dev, struct mbuf *m)
{
	struct mlx5e_priv *priv = if_getsoftc(dev);
	struct mlx5e_sq *sq;
	int err;

	if ((if_getdrvflags(dev) & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING || (priv->media_status_last & IFM_ACTIVE) == 0)
		return (ENOENT);

	sq = &priv->channel[0].sq[0];

	if (sq->running == 0) {
		m_freem(m);
		return (ENOENT);
	}

	if (mlx5e_sq_xmit(sq, &m) != 0) {
		m_freem(m);
		err = ENOBUFS;
	} else {
		err = 0;
	}

	if (likely(sq->doorbell.d64 != 0)) {
		mlx5e_tx_notify_hw(sq, sq->doorbell.d32);
		sq->doorbell.d64 = 0;
	}
	return (err);
}

static int
mlx5_en_debugnet_poll(struct ifnet *dev, int count)
{
	struct mlx5e_priv *priv = if_getsoftc(dev);

	if ((if_getdrvflags(dev) & IFF_DRV_RUNNING) == 0 ||
	    (priv->media_status_last & IFM_ACTIVE) == 0)
		return (ENOENT);

	mlx5_poll_interrupts(priv->mdev);

	return (0);
}
#endif /* DEBUGNET */

static void *
mlx5e_get_ifp(void *vpriv)
{
	struct mlx5e_priv *priv = vpriv;

	return (priv->ifp);
}

static struct mlx5_interface mlx5e_interface = {
	.add = mlx5e_create_ifp,
	.remove = mlx5e_destroy_ifp,
	.event = mlx5e_async_event,
	.protocol = MLX5_INTERFACE_PROTOCOL_ETH,
	.get_dev = mlx5e_get_ifp,
};

void
mlx5e_init(void)
{
	mlx5_register_interface(&mlx5e_interface);
}

void
mlx5e_cleanup(void)
{
	mlx5_unregister_interface(&mlx5e_interface);
}

module_init_order(mlx5e_init, SI_ORDER_SIXTH);
module_exit_order(mlx5e_cleanup, SI_ORDER_SIXTH);

#if (__FreeBSD_version >= 1100000)
MODULE_DEPEND(mlx5en, linuxkpi, 1, 1, 1);
#endif
MODULE_DEPEND(mlx5en, mlx5, 1, 1, 1);
MODULE_VERSION(mlx5en, 1);
