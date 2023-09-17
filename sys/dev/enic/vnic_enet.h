/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_ENIC_H_
#define _VNIC_ENIC_H_

/* Hardware intr coalesce timer is in units of 1.5us */
#define INTR_COALESCE_USEC_TO_HW(usec) ((usec) * 2 / 3)
#define INTR_COALESCE_HW_TO_USEC(usec) ((usec) * 3 / 2)

/* Device-specific region: enet configuration */
struct vnic_enet_config {
	u32 flags;
	u32 wq_desc_count;
	u32 rq_desc_count;
	u16 mtu;
	u16 intr_timer_deprecated;
	u8 intr_timer_type;
	u8 intr_mode;
	char devname[16];
	u32 intr_timer_usec;
	u16 loop_tag;
	u16 vf_rq_count;
	u16 num_arfs;
	u64 mem_paddr;
	u16 rdma_qp_id;
	u16 rdma_qp_count;
	u16 rdma_resgrp;
	u32 rdma_mr_id;
	u32 rdma_mr_count;
	u32 max_pkt_size;
};

#define VENETF_TSO		0x1	/* TSO enabled */
#define VENETF_LRO		0x2	/* LRO enabled */
#define VENETF_RXCSUM		0x4	/* RX csum enabled */
#define VENETF_TXCSUM		0x8	/* TX csum enabled */
#define VENETF_RSS		0x10	/* RSS enabled */
#define VENETF_RSSHASH_IPV4	0x20	/* Hash on IPv4 fields */
#define VENETF_RSSHASH_TCPIPV4	0x40	/* Hash on TCP + IPv4 fields */
#define VENETF_RSSHASH_IPV6	0x80	/* Hash on IPv6 fields */
#define VENETF_RSSHASH_TCPIPV6	0x100	/* Hash on TCP + IPv6 fields */
#define VENETF_RSSHASH_IPV6_EX	0x200	/* Hash on IPv6 extended fields */
#define VENETF_RSSHASH_TCPIPV6_EX 0x400	/* Hash on TCP + IPv6 ext. fields */
#define VENETF_LOOP		0x800	/* Loopback enabled */
#define VENETF_FAILOVER		0x1000	/* Fabric failover enabled */
#define VENETF_USPACE_NIC       0x2000	/* vHPC enabled */
#define VENETF_VMQ      0x4000 /* VMQ enabled */
#define VENETF_ARFS		0x8000  /* ARFS enabled */
#define VENETF_VXLAN    0x10000 /* VxLAN offload */
#define VENETF_NVGRE    0x20000 /* NVGRE offload */
#define VENETF_GRPINTR  0x40000 /* group interrupt */
#define VENETF_NICSWITCH        0x80000 /* NICSWITCH enabled */
#define VENETF_RSSHASH_UDPIPV4  0x100000 /* Hash on UDP + IPv4 fields */
#define VENETF_RSSHASH_UDPIPV6  0x200000 /* Hash on UDP + IPv6 fields */

#define VENET_INTR_TYPE_MIN	0	/* Timer specs min interrupt spacing */
#define VENET_INTR_TYPE_IDLE	1	/* Timer specs idle time before irq */

#define VENET_INTR_MODE_ANY	0	/* Try MSI-X, then MSI, then INTx */
#define VENET_INTR_MODE_MSI	1	/* Try MSI then INTx */
#define VENET_INTR_MODE_INTX	2	/* Try INTx only */

#endif /* _VNIC_ENIC_H_ */
