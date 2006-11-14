/*-
 * Copyright (c) 2003 Harti Brandt.
 * Copyright (c) 2003 Vincent Jardin.
 * 	All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

/*
 * Supported AALs
 */
enum vatmpif_aal {
	VATMPIF_AAL_0 = 0,	/* Cell Service */
	VATMPIF_AAL_4 = 4,	/* AAL 3/4 */
	VATMPIF_AAL_5 = 5,	/* AAL 5 */
};
typedef enum vatmpif_aal Vatmpif_aal;

/*
 * Supported traffic type
 */
enum vatmpif_traffic_type {
	VATMPIF_TRAF_CBR = 0x01,	/* Constant bit rate */
	VATMPIF_TRAF_VBR = 0x02,	/* Variable bit rate */
	VATMPIF_TRAF_ABR = 0x03,	/* Available Bit Rate */
	VATMPIF_TRAF_UBR = 0x04,	/* Unspecified bit rate */
};
typedef enum vatmpif_traffic_type Vatmpif_traffic_type;

typedef struct t_atm_traffic Vatmpif_traffic;

/*
 * Host protocol control blocks
 * 
 */
/*
 * Device VCC Entry
 * 
 * Contains the common (vv_cmn) and specific information for each VCC
 * which is opened through a ATM PIF node.
 * It is a virtual VCC. From the Netgraph poit of view it is a
 * per-node's hook private data.
 *
 * It is a polymorph object with the instances of Cmn_vcc.
 */
struct vatmpif_vcc {
	Cmn_vcc			vv_cmn;		/* Common VCC stuff */
	Vatmpif_aal		vv_aal;		/* AAL */
	Vatmpif_traffic		vv_traffic;	/* forward and backward ATM traffic */
	Vatmpif_traffic_type	vv_traffic_type;/* CBR, VBR, UBR, ... */
};
typedef struct vatmpif_vcc	Vatmpif_vcc;

#define vv_next		vv_cmn.cv_next
#define vv_toku		vv_cmn.cv_toku
#define vv_upper	vv_cmn.cv_upper
#define vv_connvc	vv_cmn.cv_connvc
#define vv_state	vv_cmn.cv_state

/*
 * The hook structure describes a virtual link
 */
struct ng_vatmpif_hook {
	hook_p		hook;		/* netgraph hook */
	Vatmpif_stats	stats;		/* link stats */
	uint32_t	InSeq;		/* last received sequence number + 1 */
	uint32_t	OutSeq;		/* last sent sequence number */
	uint32_t	cur_pcr;	/* slot's reserved PCR */
};

/*
 * Device Virtual Unit Structure
 * 
 * Contains all the information for a single device (adapter).
 * It is a virtual device. From the Netgraph point of view it is
 * a per-node private data.
 *
 * It is a polymorph object with the instances of Cmn_unit.
 */
struct vatmpif_unit {
	Cmn_unit			vu_cmn;	/* Common unit stuff */
	node_p				node;	/* netgraph node */
	struct ng_vatmpif_hook*		link;	/* virtual link hoook */
	struct ng_vatmpif_config	conf;	/* node configuration */
};
typedef struct vatmpif_unit		Vatmpif_unit;

#define ng_vatmpif_private vatmpif_unit
typedef struct ng_vatmpif_private *priv_p;

#define vu_pif			vu_cmn.cu_pif
#define vu_unit			vu_cmn.cu_unit
#define vu_flags		vu_cmn.cu_flags
#define vu_mtu			vu_cmn.cu_mtu
#define vu_open_vcc		vu_cmn.cu_open_vcc
#define vu_vcc			vu_cmn.cu_vcc
#define vu_vcc_zone		vu_cmn.cu_vcc_zone
#define vu_nif_zone		vu_cmn.cu_nif_zone
#define vu_ioctl		vu_cmn.cu_ioctl
#define vu_instvcc		vu_cmn.cu_instvcc
#define vu_openvcc		vu_cmn.cu_openvcc
#define vu_closevcc		vu_cmn.cu_closevcc
#define vu_output		vu_cmn.cu_output
#define vu_config		vu_cmn.cu_config
#define vu_softc		vu_cmn.cu_softc

#define vu_stats		link->stats
#define vu_cur_pcr		link->cur_pcr

/*
 * Netgraph to HARP API
 */
int vatmpif_harp_attach(node_p node);
int vatmpif_harp_detach(node_p node);

int vatmpif_harp_recv_drain(Vatmpif_unit *vup, KBuffer *m,
  uint8_t vpi, uint16_t vci, uint8_t pt, uint8_t clp, Vatmpif_aal aal);

/*
 * HARP to Netgraph API
 */
int ng_atmpif_transmit(const priv_p priv, struct mbuf *m,
    uint8_t vpi, uint16_t vci, uint8_t pt, uint8_t clp, Vatmpif_aal aal);

extern uma_zone_t vatmpif_nif_zone;
extern uma_zone_t vatmpif_vcc_zone;
