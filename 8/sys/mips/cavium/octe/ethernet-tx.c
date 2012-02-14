/*************************************************************************
Copyright (c) 2003-2007  Cavium Networks (support@cavium.com). All rights
reserved.


Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Cavium Networks nor the names of
      its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

This Software, including technical data, may be subject to U.S. export  control laws, including the U.S. Export Administration Act and its  associated regulations, and may be subject to export or import  regulations in other countries.

TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.

*************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>

#include "wrapper-cvmx-includes.h"
#include "ethernet-headers.h"

/* You can define GET_MBUF_QOS() to override how the mbuf output function
   determines which output queue is used. The default implementation
   always uses the base queue for the port. If, for example, you wanted
   to use the m->priority fieid, define GET_MBUF_QOS as:
   #define GET_MBUF_QOS(m) ((m)->priority) */
#ifndef GET_MBUF_QOS
    #define GET_MBUF_QOS(m) 0
#endif

extern int pow_send_group;


/**
 * Packet transmit
 *
 * @param m    Packet to send
 * @param dev    Device info structure
 * @return Always returns zero
 */
int cvm_oct_xmit(struct mbuf *m, struct ifnet *ifp)
{
	cvmx_pko_command_word0_t    pko_command;
	cvmx_buf_ptr_t              hw_buffer;
	uint64_t                    old_scratch;
	uint64_t                    old_scratch2;
	int                         dropped;
	int                         qos;
	cvm_oct_private_t          *priv = (cvm_oct_private_t *)ifp->if_softc;
	int32_t in_use;
	int32_t buffers_to_free;
	cvmx_wqe_t *work;

	/* Prefetch the private data structure.
	   It is larger that one cache line */
	CVMX_PREFETCH(priv, 0);

	/* Start off assuming no drop */
	dropped = 0;

	/* The check on CVMX_PKO_QUEUES_PER_PORT_* is designed to completely
	   remove "qos" in the event neither interface supports multiple queues
	   per port */
	if ((CVMX_PKO_QUEUES_PER_PORT_INTERFACE0 > 1) ||
	    (CVMX_PKO_QUEUES_PER_PORT_INTERFACE1 > 1)) {
		qos = GET_MBUF_QOS(m);
		if (qos <= 0)
			qos = 0;
		else if (qos >= cvmx_pko_get_num_queues(priv->port))
			qos = 0;
	} else
		qos = 0;

	if (USE_ASYNC_IOBDMA) {
		/* Save scratch in case userspace is using it */
		CVMX_SYNCIOBDMA;
		old_scratch = cvmx_scratch_read64(CVMX_SCR_SCRATCH);
		old_scratch2 = cvmx_scratch_read64(CVMX_SCR_SCRATCH+8);

		/* Assume we're going to be able t osend this packet. Fetch and increment
		   the number of pending packets for output */
		cvmx_fau_async_fetch_and_add32(CVMX_SCR_SCRATCH+8, FAU_NUM_PACKET_BUFFERS_TO_FREE, 0);
		cvmx_fau_async_fetch_and_add32(CVMX_SCR_SCRATCH, priv->fau+qos*4, 1);
	}

	/* The CN3XXX series of parts has an errata (GMX-401) which causes the
	   GMX block to hang if a collision occurs towards the end of a
	   <68 byte packet. As a workaround for this, we pad packets to be
	   68 bytes whenever we are in half duplex mode. We don't handle
	   the case of having a small packet but no room to add the padding.
	   The kernel should always give us at least a cache line */
	if (__predict_false(m->m_pkthdr.len < 64) && OCTEON_IS_MODEL(OCTEON_CN3XXX)) {
		cvmx_gmxx_prtx_cfg_t gmx_prt_cfg;
		int interface = INTERFACE(priv->port);
		int index = INDEX(priv->port);

		if (interface < 2) {
			/* We only need to pad packet in half duplex mode */
			gmx_prt_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
			if (gmx_prt_cfg.s.duplex == 0) {
				static uint8_t pad[64];

				if (!m_append(m, sizeof pad - m->m_pkthdr.len, pad))
					printf("%s: unable to padd small packet.", __func__);
			}
		}
	}

	/*
	 * If the packet is not fragmented.
	 */
	if (m->m_pkthdr.len == m->m_len) {
		/* Build the PKO buffer pointer */
		hw_buffer.u64 = 0;
		hw_buffer.s.addr = cvmx_ptr_to_phys(m->m_data);
		hw_buffer.s.pool = 0;
		hw_buffer.s.size = m->m_len;

		/* Build the PKO command */
		pko_command.u64 = 0;
		pko_command.s.segs = 1;

		work = NULL;
	} else {
		struct mbuf *n;
		unsigned segs;
		uint64_t *gp;

		/*
		 * The packet is fragmented, we need to send a list of segments
		 * in memory we borrow from the WQE pool.
		 */
		work = cvmx_fpa_alloc(CVMX_FPA_WQE_POOL);
		gp = (uint64_t *)work;

		segs = 0;
		for (n = m; n != NULL; n = n->m_next) {
			if (segs == CVMX_FPA_WQE_POOL_SIZE / sizeof (uint64_t))
				panic("%s: too many segments in packet; call m_collapse().", __func__);

			/* Build the PKO buffer pointer */
			hw_buffer.u64 = 0;
			hw_buffer.s.addr = cvmx_ptr_to_phys(n->m_data);
			hw_buffer.s.pool = 0;
			hw_buffer.s.size = n->m_len;

			*gp++ = hw_buffer.u64;
			segs++;
		}

		/* Build the PKO buffer gather list pointer */
		hw_buffer.u64 = 0;
		hw_buffer.s.addr = cvmx_ptr_to_phys(work);
		hw_buffer.s.pool = CVMX_FPA_WQE_POOL;
		hw_buffer.s.size = segs;

		/* Build the PKO command */
		pko_command.u64 = 0;
		pko_command.s.segs = segs;
		pko_command.s.gather = 1;
	}

	/* Finish building the PKO command */
	pko_command.s.n2 = 1; /* Don't pollute L2 with the outgoing packet */
	pko_command.s.dontfree = 1;
	pko_command.s.reg0 = priv->fau+qos*4;
	pko_command.s.reg0 = priv->fau+qos*4;
	pko_command.s.total_bytes = m->m_pkthdr.len;
	pko_command.s.size0 = CVMX_FAU_OP_SIZE_32;
	pko_command.s.subone0 = 1;

	/* Check if we can use the hardware checksumming */
	if (USE_HW_TCPUDP_CHECKSUM &&
	    (m->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP)) != 0) {
		/* Use hardware checksum calc */
		pko_command.s.ipoffp1 = ETHER_HDR_LEN + 1;
	}

	IF_LOCK(&priv->tx_free_queue[qos]);
	if (USE_ASYNC_IOBDMA) {
		/* Get the number of mbufs in use by the hardware */
		CVMX_SYNCIOBDMA;
		in_use = cvmx_scratch_read64(CVMX_SCR_SCRATCH);
		buffers_to_free = cvmx_scratch_read64(CVMX_SCR_SCRATCH+8);
	} else {
		/* Get the number of mbufs in use by the hardware */
		in_use = cvmx_fau_fetch_and_add32(priv->fau+qos*4, 1);
		buffers_to_free = cvmx_fau_fetch_and_add32(FAU_NUM_PACKET_BUFFERS_TO_FREE, 0);
	}

	cvmx_pko_send_packet_prepare(priv->port, priv->queue + qos, CVMX_PKO_LOCK_CMD_QUEUE);

	/* Drop this packet if we have too many already queued to the HW */
	if (_IF_QFULL(&priv->tx_free_queue[qos])) {
		dropped = 1;
	}
	/* Send the packet to the output queue */
	else
	if (__predict_false(cvmx_pko_send_packet_finish(priv->port, priv->queue + qos, pko_command, hw_buffer, CVMX_PKO_LOCK_CMD_QUEUE))) {
		DEBUGPRINT("%s: Failed to send the packet\n", if_name(ifp));
		dropped = 1;
	}

	if (USE_ASYNC_IOBDMA) {
		/* Restore the scratch area */
		cvmx_scratch_write64(CVMX_SCR_SCRATCH, old_scratch);
		cvmx_scratch_write64(CVMX_SCR_SCRATCH+8, old_scratch2);
	}

	if (__predict_false(dropped)) {
		m_freem(m);
		cvmx_fau_atomic_add32(priv->fau+qos*4, -1);
		ifp->if_oerrors++;
	} else {
		/* Put this packet on the queue to be freed later */
		_IF_ENQUEUE(&priv->tx_free_queue[qos], m);

		/* Pass it to any BPF listeners.  */
		ETHER_BPF_MTAP(ifp, m);
	}
	if (work != NULL)
		cvmx_fpa_free(work, CVMX_FPA_WQE_POOL, DONT_WRITEBACK(1));

	/* Free mbufs not in use by the hardware */
	if (_IF_QLEN(&priv->tx_free_queue[qos]) > in_use) {
		while (_IF_QLEN(&priv->tx_free_queue[qos]) > in_use) {
			_IF_DEQUEUE(&priv->tx_free_queue[qos], m);
			m_freem(m);
		}
	}
	IF_UNLOCK(&priv->tx_free_queue[qos]);

	return dropped;
}


/**
 * Packet transmit to the POW
 *
 * @param m    Packet to send
 * @param dev    Device info structure
 * @return Always returns zero
 */
int cvm_oct_xmit_pow(struct mbuf *m, struct ifnet *ifp)
{
	cvm_oct_private_t  *priv = (cvm_oct_private_t *)ifp->if_softc;
	char               *packet_buffer;
	char               *copy_location;

	/* Get a work queue entry */
	cvmx_wqe_t *work = cvmx_fpa_alloc(CVMX_FPA_WQE_POOL);
	if (__predict_false(work == NULL)) {
		DEBUGPRINT("%s: Failed to allocate a work queue entry\n", if_name(ifp));
		ifp->if_oerrors++;
		m_freem(m);
		return 0;
	}

	/* Get a packet buffer */
	packet_buffer = cvmx_fpa_alloc(CVMX_FPA_PACKET_POOL);
	if (__predict_false(packet_buffer == NULL)) {
		DEBUGPRINT("%s: Failed to allocate a packet buffer\n",
			   if_name(ifp));
		cvmx_fpa_free(work, CVMX_FPA_WQE_POOL, DONT_WRITEBACK(1));
		ifp->if_oerrors++;
		m_freem(m);
		return 0;
	}

	/* Calculate where we need to copy the data to. We need to leave 8 bytes
	   for a next pointer (unused). We also need to include any configure
	   skip. Then we need to align the IP packet src and dest into the same
	   64bit word. The below calculation may add a little extra, but that
	   doesn't hurt */
	copy_location = packet_buffer + sizeof(uint64_t);
	copy_location += ((CVMX_HELPER_FIRST_MBUFF_SKIP+7)&0xfff8) + 6;

	/* We have to copy the packet since whoever processes this packet
	   will free it to a hardware pool. We can't use the trick of
	   counting outstanding packets like in cvm_oct_xmit */
	m_copydata(m, 0, m->m_pkthdr.len, copy_location);

	/* Fill in some of the work queue fields. We may need to add more
	   if the software at the other end needs them */
#if 0
	work->hw_chksum     = m->csum;
#endif
	work->len           = m->m_pkthdr.len;
	work->ipprt         = priv->port;
	work->qos           = priv->port & 0x7;
	work->grp           = pow_send_group;
	work->tag_type      = CVMX_HELPER_INPUT_TAG_TYPE;
	work->tag           = pow_send_group; /* FIXME */
	work->word2.u64     = 0;    /* Default to zero. Sets of zero later are commented out */
	work->word2.s.bufs  = 1;
	work->packet_ptr.u64 = 0;
	work->packet_ptr.s.addr = cvmx_ptr_to_phys(copy_location);
	work->packet_ptr.s.pool = CVMX_FPA_PACKET_POOL;
	work->packet_ptr.s.size = CVMX_FPA_PACKET_POOL_SIZE;
	work->packet_ptr.s.back = (copy_location - packet_buffer)>>7;

	panic("%s: POW transmit not quite implemented yet.", __func__);
#if 0
	if (m->protocol == htons(ETH_P_IP)) {
		work->word2.s.ip_offset     = 14;
		#if 0
		work->word2.s.vlan_valid  = 0; /* FIXME */
		work->word2.s.vlan_cfi    = 0; /* FIXME */
		work->word2.s.vlan_id     = 0; /* FIXME */
		work->word2.s.dec_ipcomp  = 0; /* FIXME */
		#endif
		work->word2.s.tcp_or_udp    = (ip_hdr(m)->protocol == IP_PROTOCOL_TCP) || (ip_hdr(m)->protocol == IP_PROTOCOL_UDP);
		#if 0
		work->word2.s.dec_ipsec   = 0; /* FIXME */
		work->word2.s.is_v6       = 0; /* We only support IPv4 right now */
		work->word2.s.software    = 0; /* Hardware would set to zero */
		work->word2.s.L4_error    = 0; /* No error, packet is internal */
		#endif
		work->word2.s.is_frag       = !((ip_hdr(m)->frag_off == 0) || (ip_hdr(m)->frag_off == 1<<14));
		#if 0
		work->word2.s.IP_exc      = 0;  /* Assume Linux is sending a good packet */
		#endif
		work->word2.s.is_bcast      = (m->pkt_type == PACKET_BROADCAST);
		work->word2.s.is_mcast      = (m->pkt_type == PACKET_MULTICAST);
		#if 0
		work->word2.s.not_IP      = 0; /* This is an IP packet */
		work->word2.s.rcv_error   = 0; /* No error, packet is internal */
		work->word2.s.err_code    = 0; /* No error, packet is internal */
		#endif

		/* When copying the data, include 4 bytes of the ethernet header to
		   align the same way hardware does */
		memcpy(work->packet_data, m->data + 10, sizeof(work->packet_data));
	} else {
		#if 0
		work->word2.snoip.vlan_valid  = 0; /* FIXME */
		work->word2.snoip.vlan_cfi    = 0; /* FIXME */
		work->word2.snoip.vlan_id     = 0; /* FIXME */
		work->word2.snoip.software    = 0; /* Hardware would set to zero */
		#endif
		work->word2.snoip.is_rarp       = m->protocol == htons(ETH_P_RARP);
		work->word2.snoip.is_arp        = m->protocol == htons(ETH_P_ARP);
		work->word2.snoip.is_bcast      = (m->pkt_type == PACKET_BROADCAST);
		work->word2.snoip.is_mcast      = (m->pkt_type == PACKET_MULTICAST);
		work->word2.snoip.not_IP        = 1; /* IP was done up above */
		#if 0
		work->word2.snoip.rcv_error   = 0; /* No error, packet is internal */
		work->word2.snoip.err_code    = 0; /* No error, packet is internal */
		#endif
		memcpy(work->packet_data, m->data, sizeof(work->packet_data));
	}
#endif

	/* Submit the packet to the POW */
	cvmx_pow_work_submit(work, work->tag, work->tag_type, work->qos, work->grp);
	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
	m_freem(m);
	return 0;
}


/**
 * This function frees all mbufs that are currenty queued for TX.
 *
 * @param dev    Device being shutdown
 */
void cvm_oct_tx_shutdown(struct ifnet *ifp)
{
	cvm_oct_private_t *priv = (cvm_oct_private_t *)ifp->if_softc;
	int qos;

	for (qos = 0; qos < 16; qos++) {
		IF_DRAIN(&priv->tx_free_queue[qos]);
	}
}
