/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_HAL_STATS_H
#define	VXGE_HAL_STATS_H

__EXTERN_BEGIN_DECLS

#define	VXGE_HAL_STATS_OP_READ						0
#define	VXGE_HAL_STATS_OP_CLEAR_STAT					1
#define	VXGE_HAL_STATS_OP_CLEAR_ALL_VPATH_STATS				2
#define	VXGE_HAL_STATS_OP_CLEAR_ALL_STATS_OF_LOC			2
#define	VXGE_HAL_STATS_OP_CLEAR_ALL_STATS				3

#define	VXGE_HAL_STATS_LOC_VPATH(n)					n
#define	VXGE_HAL_STATS_LOC_AGGR						17
#define	VXGE_HAL_STATS_LOC_PORT						17

#define	VXGE_HAL_STATS_AGGRn_TX_FRMS_OFFSET(n)		    ((0x720+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_TX_FRMS(bits)		    bits

#define	VXGE_HAL_STATS_AGGRn_TX_DATA_OCTETS_OFFSET(n)	    ((0x728+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_TX_DATA_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_AGGRn_TX_MCAST_FRMS_OFFSET(n)	    ((0x730+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_TX_MCAST_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_AGGRn_TX_BCAST_FRMS_OFFSET(n)	    ((0x738+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_TX_BCAST_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_AGGRn_TX_DISCARDED_FRMS_OFFSET(n)    ((0x740+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_TX_DISCARDED_FRMS(bits)    bits

#define	VXGE_HAL_STATS_AGGRn_TX_ERRORED_FRMS_OFFSET(n)	    ((0x748+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_TX_ERRORED_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_AGGRn_RX_FRMS_OFFSET(n)		    ((0x750+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_RX_FRMS(bits)		    bits

#define	VXGE_HAL_STATS_AGGRn_RX_DATA_OCTETS_OFFSET(n)	    ((0x758+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_RX_DATA_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_AGGRn_RX_MCAST_FRMS_OFFSET(n)	    ((0x760+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_RX_MCAST_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_AGGRn_RX_BCAST_FRMS_OFFSET(n)	    ((0x768+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_RX_BCAST_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_AGGRn_RX_DISCARDED_FRMS_OFFSET(n)    ((0x770+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_RX_DISCARDED_FRMS(bits)    bits

#define	VXGE_HAL_STATS_AGGRn_RX_ERRORED_FRMS_OFFSET(n)	    ((0x778+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_RX_ERRORED_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_AGGRn_RX_U_SLOW_PROTO_FRMS_OFFSET(n) ((0x780+(104*n))>>3)
#define	VXGE_HAL_STATS_GET_AGGRn_RX_U_SLOW_PROTO_FRMS(bits) bits

#define	VXGE_HAL_STATS_GLOBAL_PROG_EVENT_GNUM0_OFFSET	    (0x7f0>>3)
#define	VXGE_HAL_STATS_GET_GLOBAL_PROG_EVENT_GNUM0(bits)    bits

#define	VXGE_HAL_STATS_GLOBAL_PROG_EVENT_GNUM1_OFFSET	    (0x7f8>>3)
#define	VXGE_HAL_STATS_GET_GLOBAL_PROG_EVENT_GNUM1(bits)    bits

#define	VXGE_HAL_STATS_PORTn_TX_TTL_FRMS_OFFSET(n)	    ((0x000+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_TTL_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_TTL_OCTETS_OFFSET(n)	    ((0x008+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_TTL_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_DATA_OCTETS_OFFSET(n)	    ((0x010+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_DATA_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_MCAST_FRMS_OFFSET(n)	    ((0x018+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_MCAST_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_BCAST_FRMS_OFFSET(n)	    ((0x020+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_BCAST_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_UCAST_FRMS_OFFSET(n)	    ((0x028+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_UCAST_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_TAGGED_FRMS_OFFSET(n)	    ((0x030+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_TAGGED_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_VLD_IP_OFFSET(n)	    ((0x038+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_VLD_IP(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_VLD_IP_OCTETS_OFFSET(n)	    ((0x040+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_VLD_IP_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_ICMP_OFFSET(n)		    ((0x048+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_ICMP(bits)		    bits

#define	VXGE_HAL_STATS_PORTn_TX_TCP_OFFSET(n)		    ((0x050+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_TCP(bits)		    bits

#define	VXGE_HAL_STATS_PORTn_TX_RST_TCP_OFFSET(n)	    ((0x058+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_RST_TCP(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_UDP_OFFSET(n)		    ((0x060+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_UDP(bits)		    bits

#define	VXGE_HAL_STATS_PORTn_TX_UNKNOWN_PROTOCOL_OFFSET(n)  ((0x068+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_UNKNOWN_PROTOCOL(bits)  bits

#define	VXGE_HAL_STATS_PORTn_TX_PARSE_ERROR_OFFSET(n)	    ((0x068+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_PARSE_ERROR(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_PAUSE_CTRL_FRMS_OFFSET(n)   ((0x070+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_PAUSE_CTRL_FRMS(bits)   bits

#define	VXGE_HAL_STATS_PORTn_TX_LACPDU_FRMS_OFFSET(n)	    ((0x078+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_LACPDU_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_TX_MRKR_PDU_FRMS_OFFSET(n)	   ((0x078+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_MRKR_PDU_FRMS(bits)	   bits

#define	VXGE_HAL_STATS_PORTn_TX_MRKR_RESP_PDU_FRMS_OFFSET(n)\
							    ((0x080+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_MRKR_RESP_PDU_FRMS(bits) bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_TX_DROP_IP_OFFSET(n)	    ((0x080+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_DROP_IP(bits)	    bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_TX_XGMII_CHAR1_MATCH_OFFSET(n) ((0x088+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_XGMII_CHAR1_MATCH(bits) bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_TX_XGMII_CHAR2_MATCH_OFFSET(n) ((0x088+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_XGMII_CHAR2_MATCH(bits) bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_TX_XGMII_COL1_MATCH_OFFSET(n)  ((0x090+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_XGMII_COL1_MATCH(bits)  bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_TX_XGMII_COL2_MATCH_OFFSET(n)  ((0x090+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_XGMII_COL2_MATCH(bits)  bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_TX_DROP_FRMS_OFFSET(n)	    ((0x098+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_DROP_FRMS(bits)	    bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_TX_ANY_ERR_FRMS_OFFSET(n)	    ((0x098+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_TX_ANY_ERR_FRMS(bits)	    bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_TTL_FRMS_OFFSET(n)	    ((0x0a0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_VLD_FRMS_OFFSET(n)	    ((0x0a8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_VLD_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_OFFLOAD_FRMS_OFFSET(n)	    ((0x0b0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_OFFLOAD_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_TTL_OCTETS_OFFSET(n)	    ((0x0b8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_DATA_OCTETS_OFFSET(n)	    ((0x0c0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_DATA_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_OFFLOAD_OCTETS_OFFSET(n)    ((0x0c8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_OFFLOAD_OCTETS(bits)    bits

#define	VXGE_HAL_STATS_PORTn_RX_VLD_MCAST_FRMS_OFFSET(n)    ((0x0d0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_VLD_MCAST_FRMS(bits)    bits

#define	VXGE_HAL_STATS_PORTn_RX_VLD_BCAST_FRMS_OFFSET(n)    ((0x0d8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_VLD_BCAST_FRMS(bits)    bits

#define	VXGE_HAL_STATS_PORTn_RX_ACC_UCAST_FRMS_OFFSET(n)    ((0x0e0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_ACC_UCAST_FRMS(bits)    bits

#define	VXGE_HAL_STATS_PORTn_RX_ACC_NUCAST_FRMS_OFFSET(n)   ((0x0e8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_ACC_NUCAST_FRMS(bits)   bits

#define	VXGE_HAL_STATS_PORTn_RX_TAGGED_FRMS_OFFSET(n)	    ((0x0f0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TAGGED_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_LONG_FRMS_OFFSET(n)	    ((0x0f8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_LONG_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_USIZED_FRMS_OFFSET(n)	    ((0x100+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_USIZED_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_OSIZED_FRMS_OFFSET(n)	    ((0x108+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_OSIZED_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_FRAG_FRMS_OFFSET(n)	    ((0x110+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_FRAG_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_JABBER_FRMS_OFFSET(n)	    ((0x118+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_JABBER_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_TTL_64_FRMS_OFFSET(n)	    ((0x120+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_64_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_TTL_65_127_FRMS_OFFSET(n)   ((0x128+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_65_127_FRMS(bits)   bits

#define	VXGE_HAL_STATS_PORTn_RX_TTL_128_255_FRMS_OFFSET(n)  ((0x130+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_128_255_FRMS(bits)  bits

#define	VXGE_HAL_STATS_PORTn_RX_TTL_256_511_FRMS_OFFSET(n)  ((0x138+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_256_511_FRMS(bits)  bits

#define	VXGE_HAL_STATS_PORTn_RX_TTL_512_1023_FRMS_OFFSET(n) ((0x140+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_512_1023_FRMS(bits) bits

#define	VXGE_HAL_STATS_PORTn_RX_TTL_1024_1518_FRMS_OFFSET(n)\
							    ((0x148+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_1024_1518_FRMS(bits) bits

#define	VXGE_HAL_STATS_PORTn_RX_TTL_1519_4095_FRMS_OFFSET(n)\
							    ((0x150+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_1519_4095_FRMS(bits) bits

#define	VXGE_HAL_STATS_PORTn_RX_TTL_4096_81915_FRMS_OFFSET(n)\
							    ((0x158+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_4096_8191_FRMS(bits) bits

#define	VXGE_HAL_STATS_PORTn_RX_TTL_8192_MAX_FRMS_OFFSET(n) ((0x160+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_8192_MAX_FRMS(bits) bits

#define	VXGE_HAL_STATS_PORTn_RX_TTL_GT_MAX_FRMS_OFFSET(n)   ((0x168+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TTL_GT_MAX_FRMS(bits)   bits

#define	VXGE_HAL_STATS_PORTn_RX_IP_OFFSET(n)		    ((0x170+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_IP(bits)		    bits

#define	VXGE_HAL_STATS_PORTn_RX_ACC_IP_OFFSET(n)	    ((0x178+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_ACC_IP(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_IP_OCTETS_OFFSET(n)	    ((0x180+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_IP_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_ERR_IP_OFFSET(n)	    ((0x188+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_ERR_IP(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_ICMP_OFFSET(n)		    ((0x190+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_ICMP(bits)		    bits

#define	VXGE_HAL_STATS_PORTn_RX_TCP_OFFSET(n)		    ((0x198+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TCP(bits)		    bits

#define	VXGE_HAL_STATS_PORTn_RX_UDP_OFFSET(n)		    ((0x1a0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_UDP(bits)		    bits

#define	VXGE_HAL_STATS_PORTn_RX_ERR_TCP_OFFSET(n)	    ((0x1a8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_ERR_TCP(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_PAUSE_CNT_OFFSET(n)	    ((0x1b0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_PAUSE_CNT(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_PAUSE_CTRL_FRMS_OFFSET(n)   ((0x1b8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_PAUSE_CTRL_FRMS(bits)   bits

#define	VXGE_HAL_STATS_PORTn_RX_UNSUP_CTRL_FRMS_OFFSET(n)   ((0x1c0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_UNSUP_CTRL_FRMS(bits)   bits

#define	VXGE_HAL_STATS_PORTn_RX_FCS_ERR_FRMS_OFFSET(n)	    ((0x1c8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_FCS_ERR_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_IN_RNG_LEN_ERR_FRMS_OFFSET(n)\
							    ((0x1d0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_IN_RNG_LEN_ERR_FRMS(bits) bits

#define	VXGE_HAL_STATS_PORTn_RX_OUT_RNG_LEN_ERR_FRMS_OFFSET(n)\
							    ((0x1d8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_OUT_RNG_LEN_ERR_FRMS(bits) bits

#define	VXGE_HAL_STATS_PORTn_RX_DROP_FRMS_OFFSET(n)	    ((0x1e0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_DROP_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_DISCARDED_FRMS_OFFSET(n)    ((0x1e8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_DISCARDED_FRMS(bits)    bits

#define	VXGE_HAL_STATS_PORTn_RX_DROP_IP_OFFSET(n)	    ((0x1f0+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_DROP_IP(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_DRP_UDP_OFFSET(n)	    ((0x1f8+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_DRP_UDP(bits)	    bits

#define	VXGE_HAL_STATS_PORTn_RX_LACPDU_FRMS_OFFSET(n)	    ((0x200+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_LACPDU_FRMS(bits)	    bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_MRKR_PDU_FRMS_OFFSET(n)	    ((0x200+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_MRKR_PDU_FRMS(bits)	    bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_MRKR_RESP_PDU_FRMS_OFFSET(n)\
							    ((0x208+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_MRKR_RESP_PDU_FRMS(bits) bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_UNKNOWN_PDU_FRMS_OFFSET(n)  ((0x208+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_UNKNOWN_PDU_FRMS(bits)  bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_ILLEGAL_PDU_FRMS_OFFSET(n)  ((0x210+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_ILLEGAL_PDU_FRMS(bits)  bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_FCS_DISCARD_OFFSET(n)	    ((0x210+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_FCS_DISCARD(bits)	    bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_LEN_DISCARD_OFFSET(n)	    ((0x218+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_LEN_DISCARD(bits)	    bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_SWITCH_DISCARD_OFFSET(n)    ((0x218+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_SWITCH_DISCARD(bits)    bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_L2_MGMT_DISCARD_OFFSET(n)   ((0x220+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_L2_MGMT_DISCARD(bits)   bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_RPA_DISCARD_OFFSET(n)	    ((0x220+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_RPA_DISCARD(bits)	    bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_TRASH_DISCARD_OFFSET(n)	    ((0x228+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_TRASH_DISCARD(bits)	    bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_RTS_DISCARD_OFFSET(n)	    ((0x228+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_RTS_DISCARD(bits)	    bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_RED_DISCARD_OFFSET(n)	    ((0x230+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_RED_DISCARD(bits)	    bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_BUFF_FULL_DISCARD_OFFSET(n) ((0x230+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_BUFF_FULL_DISCARD(bits) bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_XGMII_DATA_ERR_CNT_OFFSET(n)\
							    ((0x238+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_XGMII_DATA_ERR_CNT(bits) bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_XGMII_CTRL_ERR_CNT_OFFSET(n)\
							    ((0x238+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_XGMII_CTRL_ERR_CNT(bits) bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_XGMII_ERR_SYM_OFFSET(n)	    ((0x240+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_XGMII_ERR_SYM(bits)	    bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_XGMII_CHAR1_MATCH_OFFSET(n) ((0x240+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_XGMII_CHAR1_MATCH(bits) bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_XGMII_CHAR2_MATCH_OFFSET(n) ((0x248+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_XGMII_CHAR2_MATCH(bits) bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_XGMII_COL1_MATCH_OFFSET(n)  ((0x248+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_XGMII_COL1_MATCH(bits)  bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_XGMII_COL2_MATCH_OFFSET(n)  ((0x250+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_XGMII_COL2_MATCH(bits)  bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_LOCAL_FAULT_OFFSET(n)	    ((0x250+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_LOCAL_FAULT(bits)	    bVAL32(bits, 32)

#define	VXGE_HAL_STATS_PORTn_RX_REMOTE_FAULT_OFFSET(n)	    ((0x258+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_REMOTE_FAULT(bits)	    bVAL32(bits, 0)

#define	VXGE_HAL_STATS_PORTn_RX_JETTISON_OFFSET(n)	    ((0x258+(608*n))>>3)
#define	VXGE_HAL_STATS_GET_PORTn_RX_JETTISON(bits)	    bVAL32(bits, 32)

#define	VXGE_HAL_STATS_VPATH_TX_TTL_ETH_FRMS_OFFSET	    (0x000>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_TTL_ETH_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_TTL_ETH_OCTETS_OFFSET	    (0x008>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_TTL_ETH_OCTETS(bits)    bits

#define	VXGE_HAL_STATS_VPATH_TX_DATA_OCTETS_OFFSET	    (0x010>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_DATA_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_MCAST_FRMS_OFFSET	    (0x018>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_MCAST_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_BCAST_FRMS_OFFSET	    (0x020>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_BCAST_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_UCAST_FRMS_OFFSET	    (0x028>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_UCAST_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_TAGGED_FRMS_OFFSET	    (0x030>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_TAGGED_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_VLD_IP_OFFSET		    (0x038>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_VLD_IP(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_VLD_IP_OCTETS_OFFSET	    (0x040>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_VLD_IP_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_ICMP_OFFSET		    (0x048>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_ICMP(bits)		    bits

#define	VXGE_HAL_STATS_VPATH_TX_TCP_OFFSET		    (0x050>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_TCP(bits)		    bits

#define	VXGE_HAL_STATS_VPATH_TX_RST_TCP_OFFSET		    (0x058>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_RST_TCP(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_UDP_OFFSET		    (0x060>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_UDP(bits)		    bits

#define	VXGE_HAL_STATS_VPATH_TX_LOST_IP_OFFSET		    (0x068>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_LOST_IP(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_UNKNOWN_PROTOCOL_OFFSET	    (0x068>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_UNKNOWN_PROTOCOL(bits)  bits

#define	VXGE_HAL_STATS_VPATH_TX_PARSE_ERROR_OFFSET	    (0x070>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_PARSE_ERROR(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_TCP_OFFLOAD_OFFSET	    (0x078>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_TCP_OFFLOAD(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_TX_RETX_TCP_OFFLOAD_OFFSET	    (0x080>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_RETX_TCP_OFFLOAD(bits)  bits

#define	VXGE_HAL_STATS_VPATH_TX_LOST_IP_OFFLOAD_OFFSET	    (0x088>>3)
#define	VXGE_HAL_STATS_GET_VPATH_TX_LOST_IP_OFFLOAD(bits)   bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_ETH_FRMS_OFFSET	    (0x090>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_ETH_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_VLD_FRMS_OFFSET		    (0x098>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_VLD_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_OFFLOAD_FRMS_OFFSET	    (0x0a0>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_OFFLOAD_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_ETH_OCTETS_OFFSET	    (0x0a8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_ETH_OCTETS(bits)    bits

#define	VXGE_HAL_STATS_VPATH_RX_DATA_OCTETS_OFFSET	    (0x0b0>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_DATA_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_OFFLOAD_OCTETS_OFFSET	    (0x0b8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_OFFLOAD_OCTETS(bits)    bits

#define	VXGE_HAL_STATS_VPATH_RX_VLD_MCAST_FRMS_OFFSET	    (0x0c0>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_VLD_MCAST_FRMS(bits)    bits

#define	VXGE_HAL_STATS_VPATH_RX_VLD_BCAST_FRMS_OFFSET	    (0x0c8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_VLD_BCAST_FRMS(bits)    bits

#define	VXGE_HAL_STATS_VPATH_RX_ACC_UCAST_FRMS_OFFSET	    (0x0d0>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_ACC_UCAST_FRMS(bits)    bits

#define	VXGE_HAL_STATS_VPATH_RX_ACC_NUCAST_FRMS_OFFSET	    (0x0d8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_ACC_NUCAST_FRMS(bits)   bits

#define	VXGE_HAL_STATS_VPATH_RX_TAGGED_FRMS_OFFSET	    (0x0e0>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TAGGED_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_LONG_FRMS_OFFSET	    (0x0e8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_LONG_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_USIZED_FRMS_OFFSET	    (0x0f0>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_USIZED_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_OSIZED_FRMS_OFFSET	    (0x0f8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_OSIZED_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_FRAG_FRMS_OFFSET	    (0x100>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_FRAG_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_JABBER_FRMS_OFFSET	    (0x108>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_JABBER_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_64_FRMS_OFFSET	    (0x110>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_64_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_65_127_FRMS_OFFSET	    (0x118>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_65_127_FRMS(bits)   bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_128_255_FRMS_OFFSET	    (0x120>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_128_255_FRMS(bits)   bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_256_511_FRMS_OFFSET	    (0x128>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_256_511_FRMS(bits)   bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_512_1023_FRMS_OFFSET    (0x130>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_512_1023_FRMS(bits) bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_1024_1518_FRMS_OFFSET   (0x138>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_1024_1518_FRMS(bits) bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_1519_4095_FRMS_OFFSET   (0x140>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_1519_4095_FRMS(bits) bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_4096_8191_FRMS_OFFSET   (0x148>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_4096_8191_FRMS(bits) bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_8192_MAX_FRMS_OFFSET    (0x150>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_8192_MAX_FRMS(bits) bits

#define	VXGE_HAL_STATS_VPATH_RX_TTL_GT_MAX_FRMS_OFFSET	    (0x158>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TTL_GT_MAX_FRMS(bits)   bits

#define	VXGE_HAL_STATS_VPATH_RX_IP_OFFSET		    (0x160>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_IP(bits)		    bits

#define	VXGE_HAL_STATS_VPATH_RX_ACC_IP_OFFSET		    (0x168>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_ACC_IP(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_IP_OCTETS_OFFSET	    (0x170>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_IP_OCTETS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_ERR_IP_OFFSET		    (0x178>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_ERR_IP(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_ICMP_OFFSET		    (0x180>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_ICMP(bits)		    bits

#define	VXGE_HAL_STATS_VPATH_RX_TCP_OFFSET		    (0x188>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_TCP(bits)		    bits

#define	VXGE_HAL_STATS_VPATH_RX_UDP_OFFSET		    (0x190>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_UDP(bits)		    bits

#define	VXGE_HAL_STATS_VPATH_RX_ERR_TCP_OFFSET		    (0x198>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_ERR_TCP(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_LOST_FRMS_OFFSET	    (0x1a0>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_LOST_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_LOST_IP_OFFSET		    (0x1a8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_LOST_IP(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_RX_LOST_IP_OFFLOAD_OFFSET	    (0x1b0>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_LOST_IP_OFFLOAD(bits)   bits

#define	VXGE_HAL_STATS_VPATH_RX_QUEUE_FULL_DISCARD_OFFSET   (0x1b8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_QUEUE_FULL_DISCARD(bits) bVAL16(bits, 8)

#define	VXGE_HAL_STATS_VPATH_RX_RED_DISCARD_OFFSET	    (0x1b8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_RED_DISCARD(bits)	    bVAL16(bits, 24)

#define	VXGE_HAL_STATS_VPATH_RX_SLEEP_DISCARD_OFFSET	    (0x1b8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_SLEEP_DISCARD(bits)	    bVAL16(bits, 42)

#define	VXGE_HAL_STATS_VPATH_RX_MPA_OK_FRMS_OFFSET	    (0x1c0>>3)
#define	VXGE_HAL_STATS_GET_VPATH_RX_MPA_OK_FRMS(bits)	    bits

#define	VXGE_HAL_STATS_VPATH_PROG_EVENT_VNUM0_OFFSET	    (0x1d0>>3)
#define	VXGE_HAL_STATS_GET_VPATH_PROG_EVENT_VNUM0(bits)	    bVAL32(bits, 0)

#define	VXGE_HAL_STATS_VPATH_PROG_EVENT_VNUM1_OFFSET	    (0x1d0>>3)
#define	VXGE_HAL_STATS_GET_VPATH_PROG_EVENT_VNUM1(bits)	    bVAL32(bits, 32)

#define	VXGE_HAL_STATS_VPATH_PROG_EVENT_VNUM2_OFFSET	    (0x1d8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_PROG_EVENT_VNUM2(bits)	    bVAL32(bits, 0)

#define	VXGE_HAL_STATS_VPATH_PROG_EVENT_VNUM3_OFFSET	    (0x1d8>>3)
#define	VXGE_HAL_STATS_GET_VPATH_PROG_EVENT_VNUM3(bits)	    bVAL32(bits, 32)

/*
 * struct vxge_hal_xmac_aggr_stats_t - Per-Aggregator XMAC Statistics
 *
 * @tx_frms: Count of data frames transmitted on this Aggregator on all
 *		its Aggregation ports. Does not include LACPDUs or Marker PDUs.
 *		However, does include frames discarded by the Distribution
 *		function.
 * @tx_data_octets: Count of data and padding octets of frames transmitted
 *		on this Aggregator on all its Aggregation ports.Does not include
 *		octets of LACPDUs or Marker PDUs. However,does include octets of
 *		frames discarded by the Distribution function.
 * @tx_mcast_frms: Count of data frames transmitted (to a group destination
 *		address other than the broadcast address) on this Aggregator on
 *		all its Aggregation ports. Does not include LACPDUs or Marker
 *		PDUs. However, does include frames discarded by the Distribution
 *		function.
 * @tx_bcast_frms: Count of broadcast data frames transmitted on this Aggregator
 *		on all its Aggregation ports. Does not include LACPDUs or Marker
 *		PDUs. However, does include frames discarded by the Distribution
 *		function.
 * @tx_discarded_frms: Count of data frames to be transmitted on this Aggregator
 *		that are discarded by the Distribution function.This occurs when
 *		conversation are allocated to different ports and have to be
 *		flushed on old ports
 * @tx_errored_frms: Count of data frames transmitted on this Aggregator that
 *		experience transmission errors on its Aggregation ports.
 * @rx_frms: Count of data frames received on this Aggregator on all its
 *		Aggregation ports. Does not include LACPDUs or Marker PDUs.
 *		Also, does not include frames discarded by the Collection
 *		function.
 * @rx_data_octets: Count of data and padding octets of frames received on this
 *		Aggregator on all its Aggregation ports. Does not include octets
 *		of LACPDUs or Marker PDUs.Also,does not include octets of frames
 *		discarded by the Collection function.
 * @rx_mcast_frms: Count of data frames received (from a group destination
 *		address other than the broadcast address) on this Aggregator on
 *		all its Aggregation ports. Does not include LACPDUs or Marker
 *		PDUs. Also, does not include frames discarded by the Collection
 *		function.
 * @rx_bcast_frms: Count of broadcast data frames received on this Aggregator on
 *		all its Aggregation ports. Does not include LACPDUs or Marker
 *		PDUs. Also, does not include frames discarded by the Collection
 *		function.
 * @rx_discarded_frms: Count of data frames received on this Aggregator that are
 *		discarded by the Collection function because the Collection
 *		function was disabled on the port which the frames are received.
 * @rx_errored_frms: Count of data frames received on this Aggregator that are
 *		discarded by its Aggregation ports, or are discarded by the
 *		Collection function of the Aggregator, or that are discarded by
 *		the Aggregator due to detection of an illegal Slow Protocols PDU
 * @rx_unknown_slow_proto_frms: Count of data frames received on this Aggregator
 *		that are discarded by its Aggregation ports due to detection of
 *		an unknown Slow Protocols PDU.
 *
 * Per aggregator XMAC RX statistics.
 */
#pragma pack(1)
typedef struct vxge_hal_xmac_aggr_stats_t {
/* 0x000 */		u64	tx_frms;
/* 0x008 */		u64	tx_data_octets;
/* 0x010 */		u64	tx_mcast_frms;
/* 0x018 */		u64	tx_bcast_frms;
/* 0x020 */		u64	tx_discarded_frms;
/* 0x028 */		u64	tx_errored_frms;
/* 0x030 */		u64	rx_frms;
/* 0x038 */		u64	rx_data_octets;
/* 0x040 */		u64	rx_mcast_frms;
/* 0x048 */		u64	rx_bcast_frms;
/* 0x050 */		u64	rx_discarded_frms;
/* 0x058 */		u64	rx_errored_frms;
/* 0x060 */		u64	rx_unknown_slow_proto_frms;
} vxge_hal_xmac_aggr_stats_t;

#pragma pack()

/*
 * struct vxge_hal_xmac_port_stats_t - XMAC Port Statistics
 *
 * @tx_ttl_frms: Count of successfully transmitted MAC frames
 * @tx_ttl_octets: Count of total octets of transmitted frames, not including
 *	   framing characters (i.e. less framing bits). To determine the
 *	   total octets of transmitted frames, including framing characters,
 *	   multiply PORTn_TX_TTL_FRMS by 8 and add it to this stat (unless
 *	   otherwise configured, this stat only counts frames that have
 *	   8 bytes of preamble for each frame). This stat can be configured
 *	   (see XMAC_STATS_GLOBAL_CFG.TTL_FRMS_HANDLING) to count everything
 *	   including the preamble octets.
 * @tx_data_octets: Count of data and padding octets of successfully transmitted
 *	   frames.
 * @tx_mcast_frms: Count of successfully transmitted frames to a group address
 *	   other than the broadcast address.
 * @tx_bcast_frms: Count of successfully transmitted frames to the broadcast
 *	   group address.
 * @tx_ucast_frms: Count of transmitted frames containing a unicast address.
 *	   Includes discarded frames that are not sent to the network.
 * @tx_tagged_frms: Count of transmitted frames containing a VLAN tag.
 * @tx_vld_ip: Count of transmitted IP datagrams that are passed to the network.
 * @tx_vld_ip_octets: Count of total octets of transmitted IP datagrams that
 *	   are passed to the network.
 * @tx_icmp: Count of transmitted ICMP messages. Includes messages not sent
 *	   due to problems within ICMP.
 * @tx_tcp: Count of transmitted TCP segments. Does not include segments
 *	   containing retransmitted octets.
 * @tx_rst_tcp: Count of transmitted TCP segments containing the RST flag.
 * @tx_udp: Count of transmitted UDP datagrams.
 * @tx_parse_error: Increments when the TPA is unable to parse a packet. This
 *	   generally occurs when a packet is corrupt somehow, including
 *	   packets that have IP version mismatches, invalid Layer 2 control
 *	   fields, etc. L3/L4 checksums are not offloaded, but the packet
 *	   is still be transmitted.
 * @tx_unknown_protocol: Increments when the TPA encounters an unknown
 *	   protocol, such as a new IPv6 extension header, or an unsupported
 *	   Routing Type. The packet still has a checksum calculated but it
 *	   may be incorrect.
 * @tx_pause_ctrl_frms: Count of MAC PAUSE control frames that are transmitted.
 *	   Since, the only control frames supported by this device are
 *	   PAUSE frames, this register is a count of all transmitted MAC
 *	   control frames.
 * @tx_marker_pdu_frms: Count of Marker PDUs transmitted on this Aggr port
 * @tx_lacpdu_frms: Count of LACPDUs transmitted on this Aggregation port.
 * @tx_drop_ip: Count of transmitted IP datagrams that could not be passed to
 *	   the network. Increments because of: 1) An internal processing error
 *	   (such as an uncorrectable ECC error). 2) A frame parsing error
 *	   during IP checksum calculation.
 * @tx_marker_resp_pdu_frms: Count of Marker Response PDUs transmitted on this
 *	   Aggregation port.
 * @tx_xgmii_char2_match: Maintains a count of the number of transmitted XGMII
 *	   characters that match a pattern that is programmable through
 *	   register XMAC_STATS_TX_XGMII_CHAR_PORTn. By default, the pattern
 *	   is set to /T/ (i.e. the terminate character), thus the statistic
 *	   tracks the number of transmitted Terminate characters.
 * @tx_xgmii_char1_match: Maintains a count of the number of transmitted XGMII
 *	   characters that match a pattern that is programmable through
 *	   register XMAC_STATS_TX_XGMII_CHAR_PORTn. By default, the pattern
 *	   is set to /S/ (i.e. the start character), thus the statistic tracks
 *	   the number of transmitted Start characters.
 * @tx_xgmii_column2_match: Maintains a count of the number of transmitted XGMII
 *	   columns that match a pattern that is programmable through register
 *	   XMAC_STATS_TX_XGMII_COLUMN2_PORTn. By default, the pattern is set
 *	   to 4 x /E/ (i.e. a column containing all error characters), thus
 *	   the statistic tracks the number of Error columns transmitted at
 *	   any time. If XMAC_STATS_TX_XGMII_BEHAV_COLUMN2_PORTn.NEAR_COL1 is
 *	   set to 1, then this stat increments when COLUMN2 is found within
 *	   'n' clocks after COLUMN1. Here, 'n' is defined by
 *	   XMAC_STATS_TX_XGMII_BEHAV_COLUMN2_PORTn.NUM_COL (if 'n' is set
 *	   to 0, then it means to search anywhere for COLUMN2).
 * @tx_xgmii_column1_match: Maintains a count of the number of transmitted XGMII
 *	   columns that match a pattern that is programmable through register
 *	   XMAC_STATS_TX_XGMII_COLUMN1_PORTn. By default, the pattern is set
 *	   to 4 x /I/ (i.e. a column containing all idle characters), thus the
 *	   statistic tracks the number of transmitted Idle columns.
 * @unused1: Reserved
 * @tx_any_err_frms: Count of transmitted frames containing any error that
 *	   prevents them from being passed to the network. Increments if
 *	   there is an ECC while reading the frame out of the transmit
 *	   buffer. Also increments if the transmit protocol assist (TPA)
 *	   block determines that the frame should not be sent.
 * @tx_drop_frms: Count of frames that could not be sent for no other reason
 *	   than internal MAC processing. Increments once whenever the
 *	   transmit buffer is flushed (due to an ECC error on a memory
 *	   descriptor).
 * @rx_ttl_frms: Count of total received MAC frames, including frames received
 *	   with frame-too-long, FCS, or length errors. This stat can be
 *	   configured (see XMAC_STATS_GLOBAL_CFG.TTL_FRMS_HANDLING) to count
 *	   everything, even "frames" as small one byte of preamble.
 * @rx_vld_frms: Count of successfully received MAC frames. Does not include
 *	   frames received with frame-too-long, FCS, or length errors.
 * @rx_offload_frms: Count of offloaded received frames that are passed to
 *	   the host.
 * @rx_ttl_octets: Count of total octets of received frames, not including
 *	   framing characters (i.e. less framing bits). To determine the
 *	   total octets of received frames, including framing characters,
 *	   multiply PORTn_RX_TTL_FRMS by 8 and add it to this stat (unless
 *	   otherwise configured, this stat only counts frames that have 8
 *	   bytes of preamble for each frame). This stat can be configured
 *	   (see XMAC_STATS_GLOBAL_CFG.TTL_FRMS_HANDLING) to count everything,
 *	   even the preamble octets of "frames" as small one byte of preamble.
 * @rx_data_octets: Count of data and padding octets of successfully received
 *	   frames. Does not include frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_offload_octets: Count of total octets, not including framing
 *	   characters, of offloaded received frames that are passed
 *	   to the host.
 * @rx_vld_mcast_frms: Count of successfully received MAC frames containing a
 *		 nonbroadcast group address. Does not include frames received
 *	   with frame-too-long, FCS, or length errors.
 * @rx_vld_bcast_frms: Count of successfully received MAC frames containing
 *	   the broadcast group address. Does not include frames received
 *	   with frame-too-long, FCS, or length errors.
 * @rx_accepted_ucast_frms: Count of successfully received frames containing
 *	   a unicast address. Only includes frames that are passed to
 *	   the system.
 * @rx_accepted_nucast_frms: Count of successfully received frames containing
 *	   a non-unicast (broadcast or multicast) address. Only includes
 *	   frames that are passed to the system. Could include, for instance,
 *	   non-unicast frames that contain FCS errors if the MAC_ERROR_CFG
 *	   register is set to pass FCS-errored frames to the host.
 * @rx_tagged_frms: Count of received frames containing a VLAN tag.
 * @rx_long_frms: Count of received frames that are longer than RX_MAX_PYLD_LEN
 *	   + 18 bytes (+ 22 bytes if VLAN-tagged).
 * @rx_usized_frms: Count of received frames of length (including FCS, but not
 *	   framing bits) less than 64 octets, that are otherwise well-formed.
 *	   In other words, counts runts.
 * @rx_osized_frms: Count of received frames of length (including FCS, but not
 *	   framing bits) more than 1518 octets, that are otherwise
 *	   well-formed. Note: If register XMAC_STATS_GLOBAL_CFG.VLAN_HANDLING
 *	   is set to 1, then "more than 1518 octets" becomes "more than 1518
 *	   (1522 if VLAN-tagged) octets".
 * @rx_frag_frms: Count of received frames of length (including FCS, but not
 *	   framing bits) less than 64 octets that had bad FCS. In other
 *	   words, counts fragments.
 * @rx_jabber_frms: Count of received frames of length (including FCS, but not
 *	   framing bits) more than 1518 octets that had bad FCS. In other
 *	   words, counts jabbers. Note: If register
 *	   XMAC_STATS_GLOBAL_CFG.VLAN_HANDLING is set to 1, then "more than
 *	   1518 octets" becomes "more than 1518 (1522 if VLAN-tagged) octets".
 * @rx_ttl_64_frms: Count of total received MAC frames with length (including
 *	   FCS, but not framing bits) of exactly 64 octets. Includes frames
 *	   received with frame-too-long, FCS, or length errors.
 * @rx_ttl_65_127_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 65 and 127
 *	   octets inclusive. Includes frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_ttl_128_255_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 128 and 255
 *	   octets inclusive. Includes frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_ttl_256_511_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 256 and 511
 *	   octets inclusive. Includes frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_ttl_512_1023_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 512 and 1023
 *	   octets inclusive. Includes frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_ttl_1024_1518_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 1024 and 1518
 *	   octets inclusive. Includes frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_ttl_1519_4095_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 1519 and 4095
 *	   octets inclusive. Includes frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_ttl_4096_8191_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 4096 and 8191
 *	   octets inclusive. Includes frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_ttl_8192_max_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 8192 and
 *	   RX_MAX_PYLD_LEN+18 octets inclusive. Includes frames received
 *	   with frame-too-long, FCS, or length errors.
 * @rx_ttl_gt_max_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) exceeding
 *	   RX_MAX_PYLD_LEN+18 (+22 bytes if VLAN-tagged) octets inclusive.
 *	   Includes frames received with frame-too-long, FCS, or length errors.
 * @rx_ip: Count of received IP datagrams. Includes errored IP datagrams.
 * @rx_accepted_ip: Count of received IP datagrams that are passed to the system
 * @rx_ip_octets: Count of number of octets in received IP datagrams. Includes
 *	   errored IP datagrams.
 * @rx_err_ip:	Count of received IP datagrams containing errors. For example,
 *	   bad IP checksum.
 * @rx_icmp: Count of received ICMP messages. Includes errored ICMP messages.
 * @rx_tcp: Count of received TCP segments. Includes errored TCP segments.
 *	   Note: This stat contains a count of all received TCP segments,
 *	   regardless of whether or not they pertain to an established
 *	   connection.
 * @rx_udp: Count of received UDP datagrams.
 * @rx_err_tcp: Count of received TCP segments containing errors. For example,
 *	   bad TCP checksum.
 * @rx_pause_count: Count of number of pause quanta that the MAC has been in
 *	   the paused state. Recall, one pause quantum equates to 512
 *	   bit times.
 * @rx_pause_ctrl_frms: Count of received MAC PAUSE control frames.
 * @rx_unsup_ctrl_frms: Count of received MAC control frames that do not
 *	   contain the PAUSE opcode. The sum of RX_PAUSE_CTRL_FRMS and
 *	   this register is a count of all received MAC control frames.
 *	   Note: This stat may be configured to count all layer 2 errors
 *	   (i.e. length errors and FCS errors).
 * @rx_fcs_err_frms: Count of received MAC frames that do not pass FCS. Does
 *	   not include frames received with frame-too-long or
 *	   frame-too-short error.
 * @rx_in_rng_len_err_frms: Count of received frames with a length/type field
 *	   value between 46 (42 for VLAN-tagged frames) and 1500 (also 1500
 *	   for VLAN-tagged frames), inclusive, that does not match the
 *	   number of data octets (including pad) received. Also contains
 *	   a count of received frames with a length/type field less than
 *	   46 (42 for VLAN-tagged frames) and the number of data octets
 *	   (including pad) received is greater than 46 (42 for VLAN-tagged
 *	   frames).
 * @rx_out_rng_len_err_frms:  Count of received frames with length/type field
 *	   between 1501 and 1535 decimal, inclusive.
 * @rx_drop_frms: Count of received frames that could not be passed to the host.
 *	   See PORTn_RX_L2_MGMT_DISCARD, PORTn_RX_RPA_DISCARD,
 *	   PORTn_RX_TRASH_DISCARD, PORTn_RX_RTS_DISCARD, PORTn_RX_RED_DISCARD
 *	   for a list of reasons. Because the RMAC drops one frame at a time,
 *	   this stat also indicates the number of drop events.
 * @rx_discarded_frms: Count of received frames containing error that prevents
 *	   them from being passed to the system. See PORTn_RX_FCS_DISCARD,
 *	   PORTn_RX_LEN_DISCARD, and PORTn_RX_SWITCH_DISCARD for a list of
 *	   reasons.
 * @rx_drop_ip: Count of received IP datagrams that could not be passed to the
 *	   host. See PORTn_RX_DROP_FRMS for a list of reasons.
 * @rx_drop_udp: Count of received UDP datagrams that are not delivered to the
 *	   host. See PORTn_RX_DROP_FRMS for a list of reasons.
 * @rx_marker_pdu_frms: Count of valid Marker PDUs received on this Aggregation
 *	   port.
 * @rx_lacpdu_frms: Count of valid LACPDUs received on this Aggregation port.
 * @rx_unknown_pdu_frms: Count of received frames (on this Aggregation port)
 *	   that carry the Slow Protocols EtherType, but contain an unknown
 *	   PDU. Or frames that contain the Slow Protocols group MAC address,
 *	   but do not carry the Slow Protocols EtherType.
 * @rx_marker_resp_pdu_frms: Count of valid Marker Response PDUs received on
 *	   this Aggregation port.
 * @rx_fcs_discard: Count of received frames that are discarded because the
 *	   FCS check failed.
 * @rx_illegal_pdu_frms: Count of received frames (on this Aggregation port)
 *	   that carry the Slow Protocols EtherType, but contain a badly
 *	   formed PDU. Or frames that carry the Slow Protocols EtherType,
 *	   but contain an illegal value of Protocol Subtype.
 * @rx_switch_discard: Count of received frames that are discarded by the
 *	   internal switch because they did not have an entry in the
 *	   Filtering Database. This includes frames that had an invalid
 *	   destination MAC address or VLAN ID. It also includes frames are
 *	   discarded because they did not satisfy the length requirements
 *	   of the target VPATH.
 * @rx_len_discard: Count of received frames that are discarded because of an
 *	   invalid frame length (includes fragments, oversized frames and
 *	   mismatch between frame length and length/type field). This stat
 *	   can be configured (see XMAC_STATS_GLOBAL_CFG.LEN_DISCARD_HANDLING).
 * @rx_rpa_discard: Count of received frames that were discarded because the
 *	   receive protocol assist (RPA) discovered and error in the frame
 *	   or was unable to parse the frame.
 * @rx_l2_mgmt_discard: Count of Layer 2 management frames (eg. pause frames,
 *	   Link Aggregation Control Protocol (LACP) frames, etc.) that are
 *	   discarded.
 * @rx_rts_discard: Count of received frames that are discarded by the receive
 *	   traffic steering (RTS) logic. Includes those frame discarded
 *	   because the SSC response contradicted the switch table, because
 *	   the SSC timed out, or because the target queue could not fit the
 *	   frame.
 * @rx_trash_discard: Count of received frames that are discarded because
 *	   receive traffic steering (RTS) steered the frame to the trash
 *	   queue.
 * @rx_buff_full_discard: Count of received frames that are discarded because
 *	   internal buffers are full. Includes frames discarded because the
 *	   RTS logic is waiting for an SSC lookup that has no timeout bound.
 *	   Also, includes frames that are dropped because the MAC2FAU buffer
 *	   is nearly full -- this can happen if the external receive buffer
 *	   is full and the receive path is backing up.
 * @rx_red_discard: Count of received frames that are discarded because of RED
 *	   (Random Early Discard).
 * @rx_xgmii_ctrl_err_cnt: Maintains a count of unexpected or misplaced control
 *	   characters occuring between times of normal data transmission
 *	   (i.e. not included in RX_XGMII_DATA_ERR_CNT). This counter is
 *	   incremented when either -
 *	   1) The Reconciliation Sublayer (RS) is expecting one control
 *		  character and gets another (i.e. is expecting a Start
 *		  character, but gets another control character).
 *	   2) Start control character is not in lane 0
 *	   Only increments the count by one for each XGMII column.
 * @rx_xgmii_data_err_cnt: Maintains a count of unexpected control characters
 *	   during normal data transmission. If the Reconciliation Sublayer
 *	   (RS) receives a control character, other than a terminate control
 *	   character, during receipt of data octets then this register is
 *	   incremented. Also increments if the start frame delimiter is not
 *	   found in the correct location. Only increments the count by one
 *	   for each XGMII column.
 * @rx_xgmii_char1_match: Maintains a count of the number of XGMII characters
 *	   that match a pattern that is programmable through register
 *	   XMAC_STATS_RX_XGMII_CHAR_PORTn. By default, the pattern is set
 *	   to /E/ (i.e. the error character), thus the statistic tracks the
 *	   number of Error characters received at any time.
 * @rx_xgmii_err_sym: Count of the number of symbol errors in the received
 *	   XGMII data (i.e. PHY indicates "Receive Error" on the XGMII).
 *	   Only includes symbol errors that are observed between the XGMII
 *	   Start Frame Delimiter and End Frame Delimiter, inclusive. And
 *	   only increments the count by one for each frame.
 * @rx_xgmii_column1_match: Maintains a count of the number of XGMII columns
 *	   that match a pattern that is programmable through register
 *	   XMAC_STATS_RX_XGMII_COLUMN1_PORTn. By default, the pattern is set
 *	   to 4 x /E/ (i.e. a column containing all error characters), thus
 *	   the statistic tracks the number of Error columns received at any
 *	   time.
 * @rx_xgmii_char2_match: Maintains a count of the number of XGMII characters
 *	   that match a pattern that is programmable through register
 *	   XMAC_STATS_RX_XGMII_CHAR_PORTn. By default, the pattern is set
 *	   to /E/ (i.e. the error character), thus the statistic tracks the
 *	   number of Error characters received at any time.
 * @rx_local_fault: Maintains a count of the number of times that link
 *	   transitioned from "up" to "down" due to a local fault.
 * @rx_xgmii_column2_match: Maintains a count of the number of XGMII columns
 *	   that match a pattern that is programmable through register
 *	   XMAC_STATS_RX_XGMII_COLUMN2_PORTn. By default, the pattern is set
 *	   to 4 x /E/ (i.e. a column containing all error characters), thus
 *	   the statistic tracks the number of Error columns received at any
 *	   time. If XMAC_STATS_RX_XGMII_BEHAV_COLUMN2_PORTn.NEAR_COL1 is set
 *	   to 1, then this stat increments when COLUMN2 is found within 'n'
 *	   clocks after COLUMN1. Here, 'n' is defined by
 *	   XMAC_STATS_RX_XGMII_BEHAV_COLUMN2_PORTn.NUM_COL (if 'n' is set to
 *	   0, then it means to search anywhere for COLUMN2).
 * @rx_jettison: Count of received frames that are jettisoned because internal
 *	   buffers are full.
 * @rx_remote_fault: Maintains a count of the number of times that link
 *	   transitioned from "up" to "down" due to a remote fault.
 *
 * XMAC Port Statistics.
 */
#pragma pack(1)
typedef struct vxge_hal_xmac_port_stats_t {
/* 0x000 */		u64	tx_ttl_frms;
/* 0x008 */		u64	tx_ttl_octets;
/* 0x010 */		u64	tx_data_octets;
/* 0x018 */		u64	tx_mcast_frms;
/* 0x020 */		u64	tx_bcast_frms;
/* 0x028 */		u64	tx_ucast_frms;
/* 0x030 */		u64	tx_tagged_frms;
/* 0x038 */		u64	tx_vld_ip;
/* 0x040 */		u64	tx_vld_ip_octets;
/* 0x048 */		u64	tx_icmp;
/* 0x050 */		u64	tx_tcp;
/* 0x058 */		u64	tx_rst_tcp;
/* 0x060 */		u64	tx_udp;
/* 0x068 */		u32	tx_parse_error;
/* 0x06c */		u32	tx_unknown_protocol;
/* 0x070 */		u64	tx_pause_ctrl_frms;
/* 0x078 */		u32	tx_marker_pdu_frms;
/* 0x07c */		u32	tx_lacpdu_frms;
/* 0x080 */		u32	tx_drop_ip;
/* 0x084 */		u32	tx_marker_resp_pdu_frms;
/* 0x088 */		u32	tx_xgmii_char2_match;
/* 0x08c */		u32	tx_xgmii_char1_match;
/* 0x090 */		u32	tx_xgmii_column2_match;
/* 0x094 */		u32	tx_xgmii_column1_match;
/* 0x098 */		u32	unused1;
/* 0x09c */		u16	tx_any_err_frms;
/* 0x09e */		u16	tx_drop_frms;
/* 0x0a0 */		u64	rx_ttl_frms;
/* 0x0a8 */		u64	rx_vld_frms;
/* 0x0b0 */		u64	rx_offload_frms;
/* 0x0b8 */		u64	rx_ttl_octets;
/* 0x0c0 */		u64	rx_data_octets;
/* 0x0c8 */		u64	rx_offload_octets;
/* 0x0d0 */		u64	rx_vld_mcast_frms;
/* 0x0d8 */		u64	rx_vld_bcast_frms;
/* 0x0e0 */		u64	rx_accepted_ucast_frms;
/* 0x0e8 */		u64	rx_accepted_nucast_frms;
/* 0x0f0 */		u64	rx_tagged_frms;
/* 0x0f8 */		u64	rx_long_frms;
/* 0x100 */		u64	rx_usized_frms;
/* 0x108 */		u64	rx_osized_frms;
/* 0x110 */		u64	rx_frag_frms;
/* 0x118 */		u64	rx_jabber_frms;
/* 0x120 */		u64	rx_ttl_64_frms;
/* 0x128 */		u64	rx_ttl_65_127_frms;
/* 0x130 */		u64	rx_ttl_128_255_frms;
/* 0x138 */		u64	rx_ttl_256_511_frms;
/* 0x140 */		u64	rx_ttl_512_1023_frms;
/* 0x148 */		u64	rx_ttl_1024_1518_frms;
/* 0x150 */		u64	rx_ttl_1519_4095_frms;
/* 0x158 */		u64	rx_ttl_4096_8191_frms;
/* 0x160 */		u64	rx_ttl_8192_max_frms;
/* 0x168 */		u64	rx_ttl_gt_max_frms;
/* 0x170 */		u64	rx_ip;
/* 0x178 */		u64	rx_accepted_ip;
/* 0x180 */		u64	rx_ip_octets;
/* 0x188 */		u64	rx_err_ip;
/* 0x190 */		u64	rx_icmp;
/* 0x198 */		u64	rx_tcp;
/* 0x1a0 */		u64	rx_udp;
/* 0x1a8 */		u64	rx_err_tcp;
/* 0x1b0 */		u64	rx_pause_count;
/* 0x1b8 */		u64	rx_pause_ctrl_frms;
/* 0x1c0 */		u64	rx_unsup_ctrl_frms;
/* 0x1c8 */		u64	rx_fcs_err_frms;
/* 0x1d0 */		u64	rx_in_rng_len_err_frms;
/* 0x1d8 */		u64	rx_out_rng_len_err_frms;
/* 0x1e0 */		u64	rx_drop_frms;
/* 0x1e8 */		u64	rx_discarded_frms;
/* 0x1f0 */		u64	rx_drop_ip;
/* 0x1f8 */		u64	rx_drop_udp;
/* 0x200 */		u32	rx_marker_pdu_frms;
/* 0x204 */		u32	rx_lacpdu_frms;
/* 0x208 */		u32	rx_unknown_pdu_frms;
/* 0x20c */		u32	rx_marker_resp_pdu_frms;
/* 0x210 */		u32	rx_fcs_discard;
/* 0x214 */		u32	rx_illegal_pdu_frms;
/* 0x218 */		u32	rx_switch_discard;
/* 0x21c */		u32	rx_len_discard;
/* 0x220 */		u32	rx_rpa_discard;
/* 0x224 */		u32	rx_l2_mgmt_discard;
/* 0x228 */		u32	rx_rts_discard;
/* 0x22c */		u32	rx_trash_discard;
/* 0x230 */		u32	rx_buff_full_discard;
/* 0x234 */		u32	rx_red_discard;
/* 0x238 */		u32	rx_xgmii_ctrl_err_cnt;
/* 0x23c */		u32	rx_xgmii_data_err_cnt;
/* 0x240 */		u32	rx_xgmii_char1_match;
/* 0x244 */		u32	rx_xgmii_err_sym;
/* 0x248 */		u32	rx_xgmii_column1_match;
/* 0x24c */		u32	rx_xgmii_char2_match;
/* 0x250 */		u32	rx_local_fault;
/* 0x254 */		u32	rx_xgmii_column2_match;
/* 0x258 */		u32	rx_jettison;
/* 0x25c */		u32	rx_remote_fault;
} vxge_hal_xmac_port_stats_t;

#pragma pack()

/*
 * struct vxge_hal_mrpcim_xmac_stats_t - XMAC Statistics
 *
 * @aggr_stats: Statistics on aggregate port(port 0, port 1)
 * @port_stats: Staticstics on ports(wire 0, wire 1, lag)
 *
 * XMAC Statistics.
 */
typedef struct vxge_hal_mrpcim_xmac_stats_t {
	vxge_hal_xmac_aggr_stats_t	aggr_stats[VXGE_HAL_MAC_MAX_AGGR_PORTS];
	vxge_hal_xmac_port_stats_t	port_stats[VXGE_HAL_MAC_MAX_PORTS];
} vxge_hal_mrpcim_xmac_stats_t;

/*
 * struct vxge_hal_xmac_vpath_tx_stats_t - XMAC Vpath Tx Statistics
 *
 * @tx_ttl_eth_frms: Count of successfully transmitted MAC frames.
 * @tx_ttl_eth_octets: Count of total octets of transmitted frames,
 *		not including framing characters (i.e. less framing bits).
 *		To determine the total octets of transmitted frames, including
 *		framing characters, multiply TX_TTL_ETH_FRMS by 8 and add it to
 *		this stat (the device always prepends 8 bytes of preamble for
 *		each frame)
 * @tx_data_octets: Count of data and padding octets of successfully transmitted
 *		frames.
 * @tx_mcast_frms: Count of successfully transmitted frames to a group address
 *		other than the broadcast address.
 * @tx_bcast_frms: Count of successfully transmitted frames to the broadcast
 *		group address.
 * @tx_ucast_frms: Count of transmitted frames containing a unicast address.
 *		Includes discarded frames that are not sent to the network.
 * @tx_tagged_frms: Count of transmitted frames containing a VLAN tag.
 * @tx_vld_ip: Count of transmitted IP datagrams that are passed to the network.
 * @tx_vld_ip_octets: Count of total octets of transmitted IP datagrams that
 *	   are passed to the network.
 * @tx_icmp: Count of transmitted ICMP messages. Includes messages not sent due
 *	   to problems within ICMP.
 * @tx_tcp: Count of transmitted TCP segments. Does not include segments
 *	   containing retransmitted octets.
 * @tx_rst_tcp: Count of transmitted TCP segments containing the RST flag.
 * @tx_udp: Count of transmitted UDP datagrams.
 * @tx_unknown_protocol: Increments when the TPA encounters an unknown protocol,
 *	   such as a new IPv6 extension header, or an unsupported Routing
 *	   Type. The packet still has a checksum calculated but it may be
 *	   incorrect.
 * @tx_lost_ip: Count of transmitted IP datagrams that could not be passed
 *	   to the network. Increments because of: 1) An internal processing
 *	   error (such as an uncorrectable ECC error). 2) A frame parsing
 *	   error during IP checksum calculation.
 * @unused1: Reserved.
 * @tx_parse_error: Increments when the TPA is unable to parse a packet. This
 *	   generally occurs when a packet is corrupt somehow, including
 *	   packets that have IP version mismatches, invalid Layer 2 control
 *	   fields, etc. L3/L4 checksums are not offloaded, but the packet
 *	   is still be transmitted.
 * @tx_tcp_offload: For frames belonging to offloaded sessions only, a count
 *	   of transmitted TCP segments. Does not include segments containing
 *	   retransmitted octets.
 * @tx_retx_tcp_offload: For frames belonging to offloaded sessions only, the
 *	   total number of segments retransmitted. Retransmitted segments
 *	   that are sourced by the host are counted by the host.
 * @tx_lost_ip_offload: For frames belonging to offloaded sessions only, a count
 *	   of transmitted IP datagrams that could not be passed to the
 *	   network.
 *
 * XMAC Vpath TX Statistics.
 */
#pragma pack(1)
typedef struct vxge_hal_xmac_vpath_tx_stats_t {
	u64	tx_ttl_eth_frms;
	u64	tx_ttl_eth_octets;
	u64	tx_data_octets;
	u64	tx_mcast_frms;
	u64	tx_bcast_frms;
	u64	tx_ucast_frms;
	u64	tx_tagged_frms;
	u64	tx_vld_ip;
	u64	tx_vld_ip_octets;
	u64	tx_icmp;
	u64	tx_tcp;
	u64	tx_rst_tcp;
	u64	tx_udp;
	u32	tx_unknown_protocol;
	u32	tx_lost_ip;
	u32	unused1;
	u32	tx_parse_error;
	u64	tx_tcp_offload;
	u64	tx_retx_tcp_offload;
	u64	tx_lost_ip_offload;
} vxge_hal_xmac_vpath_tx_stats_t;

#pragma pack()

/*
 * struct vxge_hal_xmac_vpath_rx_stats_t - XMAC Vpath RX Statistics
 *
 * @rx_ttl_eth_frms: Count of successfully received MAC frames.
 * @rx_vld_frms: Count of successfully received MAC frames. Does not include
 *	   frames received with frame-too-long, FCS, or length errors.
 * @rx_offload_frms: Count of offloaded received frames that are passed to
 *	   the host.
 * @rx_ttl_eth_octets: Count of total octets of received frames, not including
 *	   framing characters (i.e. less framing bits). Only counts octets
 *	   of frames that are at least 14 bytes (18 bytes for VLAN-tagged)
 *	   before FCS. To determine the total octets of received frames,
 *	   including framing characters, multiply RX_TTL_ETH_FRMS by 8 and
 *	   add it to this stat (the stat RX_TTL_ETH_FRMS only counts frames
 *	   that have the required 8 bytes of preamble).
 * @rx_data_octets: Count of data and padding octets of successfully received
 *	   frames. Does not include frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_offload_octets: Count of total octets, not including framing characters,
 *	   of offloaded received frames that are passed to the host.
 * @rx_vld_mcast_frms: Count of successfully received MAC frames containing a
 *	   nonbroadcast group address. Does not include frames received with
 *	   frame-too-long, FCS, or length errors.
 * @rx_vld_bcast_frms: Count of successfully received MAC frames containing the
 *	   broadcast group address. Does not include frames received with
 *	   frame-too-long, FCS, or length errors.
 * @rx_accepted_ucast_frms: Count of successfully received frames containing
 *	   a unicast address. Only includes frames that are passed to the
 *	   system.
 * @rx_accepted_nucast_frms: Count of successfully received frames containing
 *	   a non-unicast (broadcast or multicast) address. Only includes
 *	   frames that are passed to the system. Could include, for instance,
 *	   non-unicast frames that contain FCS errors if the MAC_ERROR_CFG
 *	   register is set to pass FCS-errored frames to the host.
 * @rx_tagged_frms: Count of received frames containing a VLAN tag.
 * @rx_long_frms: Count of received frames that are longer than RX_MAX_PYLD_LEN
 *	   + 18 bytes (+ 22 bytes if VLAN-tagged).
 * @rx_usized_frms: Count of received frames of length (including FCS, but not
 *	   framing bits) less than 64 octets, that are otherwise well-formed.
 *	   In other words, counts runts.
 * @rx_osized_frms: Count of received frames of length (including FCS, but not
 *	   framing bits) more than 1518 octets, that are otherwise
 *	   well-formed.
 * @rx_frag_frms: Count of received frames of length (including FCS, but not
 *	   framing bits) less than 64 octets that had bad FCS. In other words,
 *	   counts fragments.
 * @rx_jabber_frms: Count of received frames of length (including FCS, but not
 *	   framing bits) more than 1518 octets that had bad FCS. In other
 *	   words, counts jabbers.
 * @rx_ttl_64_frms: Count of total received MAC frames with length (including
 *	   FCS, but not framing bits) of exactly 64 octets. Includes frames
 *	   received with frame-too-long, FCS, or length errors.
 * @rx_ttl_65_127_frms: Count of total received MAC frames with length(including
 *	   FCS, but not framing bits) of between 65 and 127 octets inclusive.
 *	   Includes frames received with frame-too-long, FCS, or length errors.
 * @rx_ttl_128_255_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 128 and 255 octets
 *	   inclusive. Includes frames received with frame-too-long, FCS,
 *	   or length errors.
 * @rx_ttl_256_511_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 256 and 511 octets
 *	   inclusive. Includes frames received with frame-too-long, FCS, or
 *	   length errors.
 * @rx_ttl_512_1023_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 512 and 1023
 *	   octets inclusive. Includes frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_ttl_1024_1518_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 1024 and 1518
 *	   octets inclusive. Includes frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_ttl_1519_4095_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 1519 and 4095
 *	   octets inclusive. Includes frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_ttl_4096_8191_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 4096 and 8191
 *	   octets inclusive. Includes frames received with frame-too-long,
 *	   FCS, or length errors.
 * @rx_ttl_8192_max_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) of between 8192 and
 *	   RX_MAX_PYLD_LEN+18 octets inclusive. Includes frames received
 *	   with frame-too-long, FCS, or length errors.
 * @rx_ttl_gt_max_frms: Count of total received MAC frames with length
 *	   (including FCS, but not framing bits) exceeding RX_MAX_PYLD_LEN+18
 *	   (+22 bytes if VLAN-tagged) octets inclusive. Includes frames
 *	   received with frame-too-long, FCS, or length errors.
 * @rx_ip: Count of received IP datagrams. Includes errored IP datagrams.
 * @rx_accepted_ip: Count of received IP datagrams that are passed to the system
 * @rx_ip_octets: Count of number of octets in received IP datagrams.
 *	   Includes errored IP datagrams.
 * @rx_err_ip: Count of received IP datagrams containing errors. For example,
 *	   bad IP checksum.
 * @rx_icmp: Count of received ICMP messages. Includes errored ICMP messages.
 * @rx_tcp: Count of received TCP segments. Includes errored TCP segments.
 *		Note: This stat contains a count of all received TCP segments,
 *		regardless of whether or not they pertain to an established
 *		connection.
 * @rx_udp: Count of received UDP datagrams.
 * @rx_err_tcp: Count of received TCP segments containing errors. For example,
 *		bad TCP checksum.
 * @rx_lost_frms: Count of received frames that could not be passed to the host.
 *		See RX_QUEUE_FULL_DISCARD and RX_RED_DISCARD for list of reasons
 * @rx_lost_ip: Count of received IP datagrams that could not be passed to
 *		the host. See RX_LOST_FRMS for a list of reasons.
 * @rx_lost_ip_offload: For frames belonging to offloaded sessions only, a count
 *		of received IP datagrams that could not be passed to the host.
 *		See RX_LOST_FRMS for a list of reasons.
 * @rx_various_discard: Count of received frames that are discarded because
 *		the target receive queue is full.
 * @rx_sleep_discard: Count of received frames that are discarded because the
 *	   target VPATH is asleep (a Wake-on-LAN magic packet can be used
 *	   to awaken the VPATH).
 * @rx_red_discard: Count of received frames that are discarded because of RED
 *	   (Random Early Discard).
 * @rx_queue_full_discard: Count of received frames that are discarded because
 *		the target receive queue is full.
 * @rx_mpa_ok_frms: Count of received frames that pass the MPA checks.
 *
 * XMAC Vpath RX Statistics.
 */
#pragma pack(1)
typedef struct vxge_hal_xmac_vpath_rx_stats_t {
	u64	rx_ttl_eth_frms;
	u64	rx_vld_frms;
	u64	rx_offload_frms;
	u64	rx_ttl_eth_octets;
	u64	rx_data_octets;
	u64	rx_offload_octets;
	u64	rx_vld_mcast_frms;
	u64	rx_vld_bcast_frms;
	u64	rx_accepted_ucast_frms;
	u64	rx_accepted_nucast_frms;
	u64	rx_tagged_frms;
	u64	rx_long_frms;
	u64	rx_usized_frms;
	u64	rx_osized_frms;
	u64	rx_frag_frms;
	u64	rx_jabber_frms;
	u64	rx_ttl_64_frms;
	u64	rx_ttl_65_127_frms;
	u64	rx_ttl_128_255_frms;
	u64	rx_ttl_256_511_frms;
	u64	rx_ttl_512_1023_frms;
	u64	rx_ttl_1024_1518_frms;
	u64	rx_ttl_1519_4095_frms;
	u64	rx_ttl_4096_8191_frms;
	u64	rx_ttl_8192_max_frms;
	u64	rx_ttl_gt_max_frms;
	u64	rx_ip;
	u64	rx_accepted_ip;
	u64	rx_ip_octets;
	u64	rx_err_ip;
	u64	rx_icmp;
	u64	rx_tcp;
	u64	rx_udp;
	u64	rx_err_tcp;
	u64	rx_lost_frms;
	u64	rx_lost_ip;
	u64	rx_lost_ip_offload;
	u16	rx_various_discard;
	u16	rx_sleep_discard;
	u16	rx_red_discard;
	u16	rx_queue_full_discard;
	u64	rx_mpa_ok_frms;
} vxge_hal_xmac_vpath_rx_stats_t;

#pragma pack()

/*
 * struct vxge_hal_device_xmac_stats_t - XMAC Statistics
 *
 * @vpath_tx_stats: Per vpath XMAC TX stats
 * @vpath_rx_stats: Per vpath XMAC RX stats
 *
 * XMAC Statistics.
 */
typedef struct vxge_hal_device_xmac_stats_t {
	vxge_hal_xmac_vpath_tx_stats_t vpath_tx_stats[VXGE_HAL_MAX_VIRTUAL_PATHS];
	vxge_hal_xmac_vpath_rx_stats_t vpath_rx_stats[VXGE_HAL_MAX_VIRTUAL_PATHS];
} vxge_hal_device_xmac_stats_t;

/*
 * struct vxge_hal_vpath_stats_hw_info_t - X3100 vpath hardware statistics.
 * @ini_num_mwr_sent: The number of PCI memory writes initiated by the PIC block
 *		for the given VPATH
 * @unused1: Reserved
 * @ini_num_mrd_sent: The number of PCI memory reads initiated by the PIC block
 * @unused2: Reserved
 * @ini_num_cpl_rcvd: The number of PCI read completions received by the
 *		PIC block
 * @unused3: Reserved
 * @ini_num_mwr_byte_sent: The number of PCI memory write bytes sent by the PIC
 *		block to the host
 * @ini_num_cpl_byte_rcvd: The number of PCI read completion bytes received by
 *		the PIC block
 * @wrcrdtarb_xoff: TBD
 * @unused4: Reserved
 * @rdcrdtarb_xoff: TBD
 * @unused5: Reserved
 * @vpath_genstats_count0: Configurable statistic #1. Refer to the GENSTATS0_CFG
 *		for information on configuring this statistic
 * @vpath_genstats_count1: Configurable statistic #2. Refer to the GENSTATS1_CFG
 *		for information on configuring this statistic
 * @vpath_genstats_count2: Configurable statistic #3. Refer to the GENSTATS2_CFG
 *		for information on configuring this statistic
 * @vpath_genstats_count3: Configurable statistic #4. Refer to the GENSTATS3_CFG
 *		for information on configuring this statistic
 * @vpath_genstats_count4: Configurable statistic #5. Refer to the GENSTATS4_CFG
 *		for information on configuring this statistic
 * @unused6: Reserved
 * @vpath_gennstats_count5: Configurable statistic #6. Refer to the
 *		GENSTATS5_CFG for information on configuring this statistic
 * @unused7: Reserved
 * @tx_stats: Transmit stats
 * @rx_stats: Receive stats
 * @unused9: Reserved
 * @prog_event_vnum1: Programmable statistic. Increments when internal logic
 *		detects a certain event. See register
 *		XMAC_STATS_CFG.EVENT_VNUM1_CFG for more information.
 * @prog_event_vnum0: Programmable statistic. Increments when internal logic
 *		detects a certain event. See register
 *		XMAC_STATS_CFG.EVENT_VNUM0_CFG for more information.
 * @prog_event_vnum3: Programmable statistic. Increments when internal logic
 *		detects a certain event. See register
 *		XMAC_STATS_CFG.EVENT_VNUM3_CFG for more information.
 * @prog_event_vnum2: Programmable statistic. Increments when internal logic
 *		detects a certain event. See register
 *		XMAC_STATS_CFG.EVENT_VNUM2_CFG for more information.
 * @rx_multi_cast_frame_discard: TBD
 * @unused10: Reserved
 * @rx_frm_transferred: TBD
 * @unused11: Reserved
 * @rxd_returned: TBD
 * @unused12: Reserved
 * @rx_mpa_len_fail_frms: Count of received frames failed the MPA length check
 * @rx_mpa_mrk_fail_frms: Count of received frames failed the MPA marker check
 * @rx_mpa_crc_fail_frms: Count of received frames failed the MPA CRC check
 * @rx_permitted_frms: Count of frames that pass through the FAU and on to the
 *		frame buffer (and subsequently to the host).
 * @rx_vp_reset_discarded_frms: Count of receive frames that are discarded
 *		because the VPATH is in reset
 * @rx_wol_frms: Count of received "magic packet" frames. Stat increments
 *		whenever the received frame matches the VPATH's Wake-on-LAN
 *		signature(s) CRC.
 * @tx_vp_reset_discarded_frms: Count of transmit frames that are discarded
 *		because the VPATH is in reset.Includes frames that are discarded
 *		because the current VPIN does not match that VPIN of the frame
 *
 * X3100 vpath hardware statistics.
 */
#pragma pack(1)
typedef struct vxge_hal_vpath_stats_hw_info_t {
/* 0x000 */	u32 ini_num_mwr_sent;
/* 0x004 */	u32 unused1;
/* 0x008 */	u32 ini_num_mrd_sent;
/* 0x00c */	u32 unused2;
/* 0x010 */	u32 ini_num_cpl_rcvd;
/* 0x014 */	u32 unused3;
/* 0x018 */	u64 ini_num_mwr_byte_sent;
/* 0x020 */	u64 ini_num_cpl_byte_rcvd;
/* 0x028 */	u32 wrcrdtarb_xoff;
/* 0x02c */	u32 unused4;
/* 0x030 */	u32 rdcrdtarb_xoff;
/* 0x034 */	u32 unused5;
/* 0x038 */	u32 vpath_genstats_count0;
/* 0x03c */	u32 vpath_genstats_count1;
/* 0x040 */	u32 vpath_genstats_count2;
/* 0x044 */	u32 vpath_genstats_count3;
/* 0x048 */	u32 vpath_genstats_count4;
/* 0x04c */	u32 unused6;
/* 0x050 */	u32 vpath_genstats_count5;
/* 0x054 */	u32 unused7;
/* 0x058 */	vxge_hal_xmac_vpath_tx_stats_t tx_stats;
/* 0x0e8 */	vxge_hal_xmac_vpath_rx_stats_t rx_stats;
/* 0x220 */	u64 unused9;
/* 0x228 */	u32 prog_event_vnum1;
/* 0x22c */	u32 prog_event_vnum0;
/* 0x230 */	u32 prog_event_vnum3;
/* 0x234 */	u32 prog_event_vnum2;
/* 0x238 */	u16 rx_multi_cast_frame_discard;
/* 0x23a */	u8 unused10[6];
/* 0x240 */	u32 rx_frm_transferred;
/* 0x244 */	u32 unused11;
/* 0x248 */	u16 rxd_returned;
/* 0x24a */	u8 unused12[6];
/* 0x252 */	u16 rx_mpa_len_fail_frms;
/* 0x254 */	u16 rx_mpa_mrk_fail_frms;
/* 0x256 */	u16 rx_mpa_crc_fail_frms;
/* 0x258 */	u16 rx_permitted_frms;
/* 0x25c */	u64 rx_vp_reset_discarded_frms;
/* 0x25e */	u64 rx_wol_frms;
/* 0x260 */	u64 tx_vp_reset_discarded_frms;
} vxge_hal_vpath_stats_hw_info_t;

#pragma pack()

/*
 * struct vxge_hal_device_stats_mrpcim_info_t - X3100 mrpcim hardware
 *					    statistics.
 * @pic_ini_rd_drop: Number of DMA reads initiated by the adapter that were
 *		discarded because the VPATH is out of service
 * @pic_ini_wr_drop: Number of DMA writes initiated by the adapter that were
 *		discared because the VPATH is out of service
 * @pic_wrcrdtarb_ph_crdt_depleted: Number of times the posted header credits
 *		for upstream PCI writes were depleted
 * @unused1: Reserved
 * @pic_wrcrdtarb_ph_crdt_depleted_vplane: Array of structures containing above
 *		two fields.
 * @pic_wrcrdtarb_pd_crdt_depleted: Number of times the posted data credits for
 *		upstream PCI writes were depleted
 * @unused2: Reserved
 * @pic_wrcrdtarb_pd_crdt_depleted_vplane: Array of structures containing above
 *		two fields.
 * @pic_rdcrdtarb_nph_crdt_depleted: Number of times the non-posted header
 *		credits for upstream PCI reads were depleted
 * @unused3: Reserved
 * @pic_rdcrdtarb_nph_crdt_depleted_vplane: Array of structures containing above
 *		two fields.
 * @pic_ini_rd_vpin_drop: Number of DMA reads initiated by the adapter that were
 *		discarded because the VPATH instance number does not match
 * @pic_ini_wr_vpin_drop: Number of DMA writes initiated by the adapter that
 *		were discarded because the VPATH instance number does not match
 * @pic_genstats_count0: Configurable statistic #1. Refer to the GENSTATS0_CFG
 *		for information on configuring this statistic
 * @pic_genstats_count1: Configurable statistic #2. Refer to the GENSTATS1_CFG
 *		for information on configuring this statistic
 * @pic_genstats_count2: Configurable statistic #3. Refer to the GENSTATS2_CFG
 *		for information on configuring this statistic
 * @pic_genstats_count3: Configurable statistic #4. Refer to the GENSTATS3_CFG
 *		for information on configuring this statistic
 * @pic_genstats_count4: Configurable statistic #5. Refer to the GENSTATS4_CFG
 *		for information on configuring this statistic
 * @unused4: Reserved
 * @pic_genstats_count5: Configurable statistic #6. Refer to the GENSTATS5_CFG
 *		for information on configuring this statistic
 * @unused5: Reserved
 * @pci_rstdrop_cpl: TBD
 * @pci_rstdrop_msg: TBD
 * @pci_rstdrop_client1: TBD
 * @pci_rstdrop_client0: TBD
 * @pci_rstdrop_client2: TBD
 * @unused6: Reserved
 * @unused7: Reserved
 * @pci_depl_cplh: Number of times completion header credits were depleted
 * @pci_depl_nph: Number of times non posted header credits were depleted
 * @pci_depl_ph: Number of times the posted header credits were depleted
 * @pci_depl_h_vplane: Array of structures containing above four fields.
 * @unused8: Reserved
 * @pci_depl_cpld: Number of times completion data credits were depleted
 * @pci_depl_npd: Number of times non posted data credits were depleted
 * @pci_depl_pd: Number of times the posted data credits were depleted
 * @pci_depl_d_vplane: Array of structures containing above four fields.
 * @xgmac_port: Array of xmac port stats
 * @xgmac_aggr: Array of aggr port stats
 * @xgmac_global_prog_event_gnum0: Programmable statistic. Increments when
 *		internal logic detects a certain event. See register
 *		XMAC_STATS_GLOBAL_CFG.EVENT_GNUM0_CFG for more information.
 * @xgmac_global_prog_event_gnum1: Programmable statistic. Increments when
 *		internal logic detects a certain event. See register
 *		XMAC_STATS_GLOBAL_CFG.EVENT_GNUM1_CFG for more information.
 * @unused9: Reserved
 * @xgmac.orp_lro_events: TBD
 * @xgmac.orp_bs_events: TBD
 * @xgmac.orp_iwarp_events: TBD
 * @unused10: Reserved
 * @xgmac.tx_permitted_frms: TBD
 * @unused11: Reserved
 * @unused12: Reserved
 * @xgmac.port2_tx_any_frms: TBD
 * @xgmac.port1_tx_any_frms: TBD
 * @xgmac.port0_tx_any_frms: TBD
 * @unused13: Reserved
 * @unused14: Reserved
 * @xgmac.port2_rx_any_frms: TBD
 * @xgmac.port1_rx_any_frms: TBD
 * @xgmac.port0_rx_any_frms: TBD
 *
 * X3100 mrpcim hardware statistics.
 */
#pragma pack(1)
typedef struct vxge_hal_mrpcim_stats_hw_info_t {
/* 0x0000 */	u32	pic_ini_rd_drop;
/* 0x0004 */	u32	pic_ini_wr_drop;
/* 0x0008 */	struct {
	/* 0x0000 */	u32	pic_wrcrdtarb_ph_crdt_depleted;
	/* 0x0004 */	u32	unused1;
		} pic_wrcrdtarb_ph_crdt_depleted_vplane[17];
/* 0x0090 */	struct {
	/* 0x0000 */	u32	pic_wrcrdtarb_pd_crdt_depleted;
	/* 0x0004 */	u32	unused2;
		} pic_wrcrdtarb_pd_crdt_depleted_vplane[17];
/* 0x0118 */	struct {
	/* 0x0000 */	u32	pic_rdcrdtarb_nph_crdt_depleted;
	/* 0x0004 */	u32	unused3;
		} pic_rdcrdtarb_nph_crdt_depleted_vplane[17];
/* 0x01a0 */	u32	pic_ini_rd_vpin_drop;
/* 0x01a4 */	u32	pic_ini_wr_vpin_drop;
/* 0x01a8 */	u32	pic_genstats_count0;
/* 0x01ac */	u32	pic_genstats_count1;
/* 0x01b0 */	u32	pic_genstats_count2;
/* 0x01b4 */	u32	pic_genstats_count3;
/* 0x01b8 */	u32	pic_genstats_count4;
/* 0x01bc */	u32	unused4;
/* 0x01c0 */	u32	pic_genstats_count5;
/* 0x01c4 */	u32	unused5;
/* 0x01c8 */	u32	pci_rstdrop_cpl;
/* 0x01cc */	u32	pci_rstdrop_msg;
/* 0x01d0 */	u32	pci_rstdrop_client1;
/* 0x01d4 */	u32	pci_rstdrop_client0;
/* 0x01d8 */	u32	pci_rstdrop_client2;
/* 0x01dc */	u32	unused6;
/* 0x01e0 */	struct {
	/* 0x0000 */	u16	unused7;
	/* 0x0002 */	u16	pci_depl_cplh;
	/* 0x0004 */	u16	pci_depl_nph;
	/* 0x0006 */	u16	pci_depl_ph;
		} pci_depl_h_vplane[17];
/* 0x0268 */	struct {
	/* 0x0000 */	u16	unused8;
	/* 0x0002 */	u16	pci_depl_cpld;
	/* 0x0004 */	u16	pci_depl_npd;
	/* 0x0006 */	u16	pci_depl_pd;
		} pci_depl_d_vplane[17];
/* 0x02f0 */	vxge_hal_xmac_port_stats_t xgmac_port[3];
/* 0x0a10 */	vxge_hal_xmac_aggr_stats_t xgmac_aggr[2];
/* 0x0ae0 */	u64	xgmac_global_prog_event_gnum0;
/* 0x0ae8 */	u64	xgmac_global_prog_event_gnum1;
/* 0x0af0 */	u64	unused9;
/* 0x0af8 */	u64	xgmac_orp_lro_events;
/* 0x0b00 */	u64	xgmac_orp_bs_events;
/* 0x0b08 */	u64	xgmac_orp_iwarp_events;
/* 0x0b10 */	u32	unused10;
/* 0x0b14 */	u32	xgmac_tx_permitted_frms;
/* 0x0b18 */	u32	unused11;
/* 0x0b1c */	u8	unused12;
/* 0x0b1d */	u8	xgmac_port2_tx_any_frms;
/* 0x0b1e */	u8	xgmac_port1_tx_any_frms;
/* 0x0b1f */	u8	xgmac_port0_tx_any_frms;
/* 0x0b20 */	u32	unused13;
/* 0x0b24 */	u8	unused14;
/* 0x0b25 */	u8	xgmac_port2_rx_any_frms;
/* 0x0b26 */	u8	xgmac_port1_rx_any_frms;
/* 0x0b27 */	u8	xgmac_port0_rx_any_frms;
} vxge_hal_mrpcim_stats_hw_info_t;

#pragma pack()

/*
 * struct vxge_hal_mrpcim_xpak_stats_t - HAL xpak stats
 * @excess_temp: excess transceiver_temperature count
 * @excess_bias_current: excess laser_bias_current count
 * @excess_laser_output: excess laser_output_power count
 * @alarm_transceiver_temp_high: alarm_transceiver_temp_high count value
 * @alarm_transceiver_temp_low : alarm_transceiver_temp_low count value
 * @alarm_laser_bias_current_high: alarm_laser_bias_current_high count value
 * @alarm_laser_bias_current_low: alarm_laser_bias_current_low count value
 * @alarm_laser_output_power_high: alarm_laser_output_power_high count value
 * @alarm_laser_output_power_low: alarm_laser_output_power_low count value
 * @warn_transceiver_temp_high: warn_transceiver_temp_high count value
 * @warn_transceiver_temp_low: warn_transceiver_temp_low count value
 * @warn_laser_bias_current_high: warn_laser_bias_current_high count value
 * @warn_laser_bias_current_low: warn_laser_bias_current_low count value
 * @warn_laser_output_power_high: warn_laser_output_power_high count value
 * @warn_laser_output_power_low: warn_laser_output_power_low count value
 */
typedef struct vxge_hal_mrpcim_xpak_stats_t {
	u32	 excess_temp;
	u32	 excess_bias_current;
	u32	 excess_laser_output;
	u16	 alarm_transceiver_temp_high;
	u16	 alarm_transceiver_temp_low;
	u16	 alarm_laser_bias_current_high;
	u16	 alarm_laser_bias_current_low;
	u16	 alarm_laser_output_power_high;
	u16	 alarm_laser_output_power_low;
	u16	 warn_transceiver_temp_high;
	u16	 warn_transceiver_temp_low;
	u16	 warn_laser_bias_current_high;
	u16	 warn_laser_bias_current_low;
	u16	 warn_laser_output_power_high;
	u16	 warn_laser_output_power_low;
} vxge_hal_mrpcim_xpak_stats_t;

/*
 * struct vxge_hal_device_stats_hw_info_t - X3100 hardware statistics.
 * @vpath_info: VPath statistics
 * @vpath_info_sav: Vpath statistics saved
 *
 * X3100 hardware statistics.
 */
typedef struct vxge_hal_device_stats_hw_info_t {
	vxge_hal_vpath_stats_hw_info_t *vpath_info[VXGE_HAL_MAX_VIRTUAL_PATHS];
	vxge_hal_vpath_stats_hw_info_t vpath_info_sav[VXGE_HAL_MAX_VIRTUAL_PATHS];
} vxge_hal_device_stats_hw_info_t;

/*
 * struct vxge_hal_vpath_stats_sw_common_info_t - HAL common stats for queues.
 * @full_cnt: Number of times the queue was full
 * @usage_cnt: usage count.
 * @usage_max: Maximum usage
 * @avg_compl_per_intr_cnt: Average number of completions per interrupt.
 *	   Note that a total number of completed descriptors
 *	   for the given channel can be calculated as
 *	   (@traffic_intr_cnt * @avg_compl_per_intr_cnt).
 * @total_compl_cnt: Total completion count.
 *	    @total_compl_cnt == (@traffic_intr_cnt * @avg_compl_per_intr_cnt).
 *
 * HAL common counters for queues
 * See also: vxge_hal_vpath_stats_sw_fifo_info_t {},
 *	  vxge_hal_vpath_stats_sw_ring_info_t {},
 *	  vxge_hal_vpath_stats_sw_dmq_info_t {},
 *	  vxge_hal_vpath_stats_sw_umq_info_t {},
 *	  vxge_hal_vpath_stats_sw_srq_info_t {},
 *	  vxge_hal_vpath_stats_sw_cqrq_info_t {}.
 */
typedef struct vxge_hal_vpath_stats_sw_common_info_t {
	u32	full_cnt;
	u32	usage_cnt;
	u32	usage_max;
	u32	avg_compl_per_intr_cnt;
	u32	total_compl_cnt;
} vxge_hal_vpath_stats_sw_common_info_t;

/*
 * struct vxge_hal_vpath_stats_sw_fifo_info_t - HAL fifo statistics
 * @common_stats: Common counters for all queues
 * @total_posts: Total number of postings on the queue.
 * @total_buffers: Total number of buffers posted.
 * @avg_buffers_per_post: Average number of buffers transferred in a single
 *	post operation. Calculated as @total_buffers/@total_posts.
 * @copied_buffers: Number of buffers copied
 * @avg_buffer_size: Average buffer size transferred by a single post
 *		operation. Calculated as a total number of transmitted octets
 *		divided by @total_buffers.
 * @avg_post_size: Average amount of data transferred by a single post.
 *		Calculated as a total number of transmitted octets divided by
 *		@total_posts.
 * @total_frags: Total number of fragments
 * @copied_frags: Number of fragments copied
 * @total_posts_dang_dtrs: Total number of posts involving dangling descriptors.
 * @total_posts_dang_frags: Total number of dangling fragments posted during
 *		 post request containing multiple descriptors.
 * @txd_t_code_err_cnt: Array of transmit transfer codes. The position
 * (index) in this array reflects the transfer code type, for instance
 * 0xA - "loss of link".
 * Value txd_t_code_err_cnt[i] reflects the
 * number of times the corresponding transfer code was encountered.
 *
 * HAL fifo counters
 * See also: vxge_hal_vpath_stats_sw_common_info_t {},
 *	     vxge_hal_vpath_stats_sw_ring_info_t {},
 *	     vxge_hal_vpath_stats_sw_dmq_info_t {},
 *	     vxge_hal_vpath_stats_sw_umq_info_t {},
 *	     vxge_hal_vpath_stats_sw_sq_info_t {},
 *	     vxge_hal_vpath_stats_sw_srq_info_t {},
 *	     vxge_hal_vpath_stats_sw_cqrq_info_t {}.
 */
typedef struct vxge_hal_vpath_stats_sw_fifo_info_t {
	vxge_hal_vpath_stats_sw_common_info_t common_stats;
	u32	total_posts;
	u32	total_buffers;
	u32	avg_buffers_per_post;
	u32	copied_buffers;
	u32	avg_buffer_size;
	u32	avg_post_size;
	u32	total_frags;
	u32	copied_frags;
	u32	total_posts_dang_dtrs;
	u32	total_posts_dang_frags;
	u32	txd_t_code_err_cnt[16];
} vxge_hal_vpath_stats_sw_fifo_info_t;

/*
 * struct vxge_hal_vpath_stats_sw_ring_info_t - HAL ring statistics
 * @common_stats: Common counters for all queues
 * @rxd_t_code_err_cnt: Array of receive transfer codes. The position
 *		(index) in this array reflects the transfer code type,
 *		for instance
 *		0x7 - for "invalid receive buffer size", or 0x8 - for ECC.
 *		Value rxd_t_code_err_cnt[i] reflects the
 *		number of times the corresponding transfer code was encountered.
 * @lro_clubbed_frms_cnt: Total no of Aggregated packets
 * @lro_sending_both: Number of times the aggregation of packets broken
 * @lro_outof_sequence_pkts: Number of out of order packets
 * @lro_flush_max_pkts: Number of times we reached upper packet limit for
 *		aggregation per session
 * @lro_sum_avg_pkts_aggregated: Total number of packets considered for
 *		aggregation
 * @lro_num_aggregations: Number of packets sent to the stack
 * @lro_max_pkts_aggr: Max number of aggr packet  per ring
 * @lro_avg_agr_pkts: Average Aggregate packet
 *
 * HAL ring counters
 * See also: vxge_hal_vpath_stats_sw_common_info_t {},
 *	     vxge_hal_vpath_stats_sw_fifo_info_t {},
 *	     vxge_hal_vpath_stats_sw_dmq_info_t {},
 *	     vxge_hal_vpath_stats_sw_umq_info_t {},
 *	     vxge_hal_vpath_stats_sw_sq_info_t {},
 *	     vxge_hal_vpath_stats_sw_srq_info_t {},
 *	     vxge_hal_vpath_stats_sw_cqrq_info_t {}.
 */
typedef struct vxge_hal_vpath_stats_sw_ring_info_t {
	vxge_hal_vpath_stats_sw_common_info_t common_stats;
	u32	rxd_t_code_err_cnt[16];
} vxge_hal_vpath_stats_sw_ring_info_t;

/*
 * struct vxge_hal_vpath_stats_sw_dmq_info_t - HAL dmq statistics
 * @common_stats: Common counters for all queues
 *
 * HAL dmq counters
 * See also: vxge_hal_vpath_stats_sw_common_info_t {},
 *	     vxge_hal_vpath_stats_sw_fifo_info_t {},
 *	     vxge_hal_vpath_stats_sw_ring_info_t {},
 *	     vxge_hal_vpath_stats_sw_umq_info_t {},
 *	     vxge_hal_vpath_stats_sw_sq_info_t {},
 *	     vxge_hal_vpath_stats_sw_srq_info_t {},
 *	     vxge_hal_vpath_stats_sw_cqrq_info_t {}.
 */
typedef struct vxge_hal_vpath_stats_sw_dmq_info_t {
	vxge_hal_vpath_stats_sw_common_info_t common_stats;
} vxge_hal_vpath_stats_sw_dmq_info_t;

/*
 * struct vxge_hal_vpath_stats_sw_umq_info_t - HAL umq statistics
 * @common_stats: Common counters for all queues
 *
 * HAL dmq counters
 * See also: vxge_hal_vpath_stats_sw_common_info_t {},
 *	     vxge_hal_vpath_stats_sw_fifo_info_t {},
 *	     vxge_hal_vpath_stats_sw_ring_info_t {},
 *	     vxge_hal_vpath_stats_sw_dmq_info_t {},
 *	     vxge_hal_vpath_stats_sw_sq_info_t {},
 *	     vxge_hal_vpath_stats_sw_srq_info_t {},
 *	     vxge_hal_vpath_stats_sw_cqrq_info_t {}.
 */
typedef struct vxge_hal_vpath_stats_sw_umq_info_t {
	vxge_hal_vpath_stats_sw_common_info_t common_stats;
} vxge_hal_vpath_stats_sw_umq_info_t;

/*
 * struct vxge_hal_vpath_stats_sw_sq_info_t - HAL sq statistics
 * @common_stats: Common counters for all queues
 *
 * HAL srq counters
 * See also: vxge_hal_vpath_stats_sw_common_info_t {},
 *	     vxge_hal_vpath_stats_sw_fifo_info_t {},
 *	     vxge_hal_vpath_stats_sw_ring_info_t {},
 *	     vxge_hal_vpath_stats_sw_dmq_info_t {},
 *	     vxge_hal_vpath_stats_sw_umq_info_t {},
 *	     vxge_hal_vpath_stats_sw_srq_info_t {},
 *	     vxge_hal_vpath_stats_sw_cqrq_info_t {}.
 */
typedef struct vxge_hal_vpath_stats_sw_sq_info_t {
	vxge_hal_vpath_stats_sw_common_info_t common_stats;
} vxge_hal_vpath_stats_sw_sq_info_t;

/*
 * struct vxge_hal_vpath_stats_sw_srq_info_t - HAL srq statistics
 * @common_stats: Common counters for all queues
 *
 * HAL srq counters
 * See also: vxge_hal_vpath_stats_sw_common_info_t {},
 *	     vxge_hal_vpath_stats_sw_fifo_info_t {},
 *	     vxge_hal_vpath_stats_sw_ring_info_t {},
 *	     vxge_hal_vpath_stats_sw_dmq_info_t {},
 *	     vxge_hal_vpath_stats_sw_umq_info_t {},
 *	     vxge_hal_vpath_stats_sw_sq_info_t {},
 *	     vxge_hal_vpath_stats_sw_cqrq_info_t {}.
 */
typedef struct vxge_hal_vpath_stats_sw_srq_info_t {
	vxge_hal_vpath_stats_sw_common_info_t common_stats;
} vxge_hal_vpath_stats_sw_srq_info_t;

/*
 * struct vxge_hal_vpath_stats_sw_cqrq_info_t - HAL cqrq statistics
 * @common_stats: Common counters for all queues
 *
 * HAL cqrq counters
 * See also: vxge_hal_vpath_stats_sw_common_info_t {},
 *	     vxge_hal_vpath_stats_sw_fifo_info_t {},
 *	     vxge_hal_vpath_stats_sw_ring_info_t {},
 *	     vxge_hal_vpath_stats_sw_dmq_info_t {},
 *	     vxge_hal_vpath_stats_sw_umq_info_t {},
 *	     vxge_hal_vpath_stats_sw_sq_info_t {},
 *	     vxge_hal_vpath_stats_sw_srq_info_t {}.
 */
typedef struct vxge_hal_vpath_stats_sw_cqrq_info_t {
	vxge_hal_vpath_stats_sw_common_info_t common_stats;
} vxge_hal_vpath_stats_sw_cqrq_info_t;

/*
 * struct vxge_hal_vpath_sw_obj_count_t - Usage count of obj ids in virtual path
 *
 * @no_nces: Number of NCEs on Adapter in this VP
 * @no_sqs: Number of SQs on Adapter in this VP
 * @no_srqs: Number of SRQs on Adapter in this VP
 * @no_cqrqs: Number of CQRQs on Adapter in this VP
 * @no_sessions: Number of sessions on Adapter in this VP
 *
 * This structure contains fields to keep the usage count of objects in
 * a virtual path
 */
typedef struct vxge_hal_vpath_sw_obj_count_t {
	u32	no_nces;
	u32	no_sqs;
	u32	no_srqs;
	u32	no_cqrqs;
	u32	no_sessions;
} vxge_hal_vpath_sw_obj_count_t;

/*
 * struct vxge_hal_vpath_stats_sw_err_t - HAL vpath error statistics
 * @unknown_alarms: Unknown Alarm count
 * @network_sustained_fault: Network sustained fault count
 * @network_sustained_ok: Network sustained ok count
 * @kdfcctl_fifo0_overwrite: Fifo 0 overwrite count
 * @kdfcctl_fifo0_poison: Fifo 0 poison count
 * @kdfcctl_fifo0_dma_error: Fifo 0 dma error count
 * @kdfcctl_fifo1_overwrite: Fifo 1 overwrite count
 * @kdfcctl_fifo1_poison: Fifo 1 poison count
 * @kdfcctl_fifo1_dma_error: Fifo 1 dma error count
 * @kdfcctl_fifo2_overwrite: Fifo 2 overwrite count
 * @kdfcctl_fifo2_poison: Fifo 2 overwrite count
 * @kdfcctl_fifo2_dma_error: Fifo 2 dma error count
 * @dblgen_fifo0_overflow: Dblgen Fifo 0 overflow count
 * @dblgen_fifo1_overflow: Dblgen Fifo 1 overflow count
 * @dblgen_fifo2_overflow: Dblgen Fifo 2 overflow count
 * @statsb_pif_chain_error: Statsb pif chain error count
 * @statsb_drop_timeout: Statsb drop timeout count
 * @target_illegal_access: Target illegal access count
 * @ini_serr_det: Serious error detected count
 * @pci_config_status_err: PCI config status error count
 * @pci_config_uncor_err: PCI config uncorrectable error count
 * @pci_config_cor_err: PCI config correctable error count
 * @mrpcim_to_vpath_alarms: MRPCIM to vpath alarm count
 * @srpcim_to_vpath_alarms: SRPCIM to vpath alarm count
 * @srpcim_msg_to_vpath: SRPCIM to vpath message count
 * @prc_ring_bumps: Ring controller ring bumps count
 * @prc_rxdcm_sc_err: Ring controller rxdsm sc error count
 * @prc_rxdcm_sc_abort: Ring controller rxdsm sc abort count
 * @prc_quanta_size_err: Ring controller quanta size count
 *
 * HAL vpath error statistics
 */
typedef struct vxge_hal_vpath_stats_sw_err_t {
	u32	unknown_alarms;
	u32	network_sustained_fault;
	u32	network_sustained_ok;
	u32	kdfcctl_fifo0_overwrite;
	u32	kdfcctl_fifo0_poison;
	u32	kdfcctl_fifo0_dma_error;
	u32	kdfcctl_fifo1_overwrite;
	u32	kdfcctl_fifo1_poison;
	u32	kdfcctl_fifo1_dma_error;
	u32	kdfcctl_fifo2_overwrite;
	u32	kdfcctl_fifo2_poison;
	u32	kdfcctl_fifo2_dma_error;
	u32	dblgen_fifo0_overflow;
	u32	dblgen_fifo1_overflow;
	u32	dblgen_fifo2_overflow;
	u32	statsb_pif_chain_error;
	u32	statsb_drop_timeout;
	u32	target_illegal_access;
	u32	ini_serr_det;
	u32	pci_config_status_err;
	u32	pci_config_uncor_err;
	u32	pci_config_cor_err;
	u32	mrpcim_to_vpath_alarms;
	u32	srpcim_to_vpath_alarms;
	u32	srpcim_msg_to_vpath;
	u32	prc_ring_bumps;
	u32	prc_rxdcm_sc_err;
	u32	prc_rxdcm_sc_abort;
	u32	prc_quanta_size_err;
} vxge_hal_vpath_stats_sw_err_t;

/*
 * struct vxge_hal_vpath_stats_sw_info_t - HAL vpath sw statistics
 * @soft_reset_cnt: Number of times soft reset is done on this vpath.
 * @obj_counts: Statistics for the VP
 * @error_stats: error counters for the vpath
 * @ring_stats: counters for ring belonging to the vpath
 * @fifo_stats: counters for fifo belonging to the vpath
 * @dmq_stats: counters for dmq belonging to the vpath
 * @umq_stats: counters for umq belonging to the vpath
 *
 * HAL vpath sw statistics
 * See also: vxge_hal_device_info_t {}}.
 */
typedef struct vxge_hal_vpath_stats_sw_info_t {
	u32					soft_reset_cnt;
	vxge_hal_vpath_sw_obj_count_t		obj_counts;
	vxge_hal_vpath_stats_sw_err_t		error_stats;
	vxge_hal_vpath_stats_sw_ring_info_t	ring_stats;
	vxge_hal_vpath_stats_sw_fifo_info_t	fifo_stats;
	vxge_hal_vpath_stats_sw_dmq_info_t	dmq_stats;
	vxge_hal_vpath_stats_sw_umq_info_t	umq_stats;
} vxge_hal_vpath_stats_sw_info_t;

/*
 * struct vxge_hal_device_stats_sw_info_t - HAL own per-device statistics.
 *
 * @soft_reset_cnt: Number of times soft reset is done on this device.
 * @vpath_info: please see vxge_hal_vpath_stats_sw_info_t {}
 * HAL per-device statistics.
 */
typedef struct vxge_hal_device_stats_sw_info_t {
	u32	soft_reset_cnt;
	vxge_hal_vpath_stats_sw_info_t vpath_info[VXGE_HAL_MAX_VIRTUAL_PATHS];
} vxge_hal_device_stats_sw_info_t;

/*
 * struct vxge_hal_device_stats_sw_err_t - HAL device error statistics.
 * @mrpcim_alarms: Number of mrpcim alarms
 * @srpcim_alarms: Number of srpcim alarms
 * @vpath_alarms: Number of vpath alarms
 *
 * HAL Device error stats
 */
typedef struct vxge_hal_device_stats_sw_err_t {
	u32	mrpcim_alarms;
	u32	srpcim_alarms;
	u32	vpath_alarms;
} vxge_hal_device_stats_sw_err_t;

/*
 * struct vxge_hal_device_stats_t - Contains HAL per-device statistics,
 * including hw.
 * @devh: HAL device handle.
 *
 * @hw_dev_info_stats: X3100 statistics maintained by the hardware.
 * @sw_dev_err_stats: HAL's "soft" device error statistics.
 * @sw_dev_info_stats: HAL's "soft" device informational statistics, e.g. number
 *			of completions per interrupt.
 *
 * @is_enabled: True, if device stats collection is enabled.
 *
 * Structure-container of HAL per-device statistics. Note that per-channel
 * statistics are kept in separate structures under HAL's fifo and ring
 * channels.
 */
typedef struct vxge_hal_device_stats_t {
	/* handles */
	vxge_hal_device_h		devh;

	/* HAL device hardware statistics */
	vxge_hal_device_stats_hw_info_t	hw_dev_info_stats;

	/* HAL device "soft" stats */
	vxge_hal_device_stats_sw_err_t  sw_dev_err_stats;
	vxge_hal_device_stats_sw_info_t sw_dev_info_stats;

	/* flags */
	int				is_enabled;
} vxge_hal_device_stats_t;

/*
 * vxge_hal_vpath_hw_stats_enable - Enable vpath h/w statistics.
 * @vpath_handle: Virtual Path handle.
 *
 * Enable the DMA vpath statistics. The function is to be called to re-enable
 * the adapter to update stats into the host memory
 *
 * See also: vxge_hal_vpath_hw_stats_disable()
 */
vxge_hal_status_e
vxge_hal_vpath_hw_stats_enable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_hw_stats_disable - Disable vpath h/w statistics.
 * @vpath_handle: Virtual Path handle.
 *
 * Enable the DMA vpath statistics. The function is to be called to disable
 * the adapter to update stats into the host memory. This function is not
 * needed to be called, normally.
 *
 * See also: vxge_hal_vpath_hw_stats_enable()
 */
vxge_hal_status_e
vxge_hal_vpath_hw_stats_disable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_hw_stats_get - Get the vpath hw statistics.
 * @vpath_handle: Virtual Path handle.
 * @hw_stats: Hardware stats
 *
 * Returns the vpath h/w stats.
 *
 * See also: vxge_hal_vpath_hw_stats_enable(), vxge_hal_vpath_hw_stats_disable()
 */
vxge_hal_status_e
vxge_hal_vpath_hw_stats_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_vpath_stats_hw_info_t *hw_stats);

/*
 * vxge_hal_vpath_sw_stats_get - Get the vpath sw statistics.
 * @vpath_handle: Virtual Path handle.
 * @sw_stats: Software stats
 *
 * Returns the vpath s/w stats.
 *
 * See also: vxge_hal_vpath_hw_stats_get()
 */
vxge_hal_status_e
vxge_hal_vpath_sw_stats_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_vpath_stats_sw_info_t *sw_stats);

/*
 * vxge_hal_vpath_stats_access - Get the statistics from the given location
 *			  and offset and perform an operation
 * @vpath_handle: Virtual path handle.
 * @operation: Operation to be performed
 * @offset: Offset with in the location
 * @stat: Pointer to a buffer to return the value
 *
 * Get the statistics from the given location and offset.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_stats_access(
    vxge_hal_vpath_h vpath_handle,
    u32 operation,
    u32 offset,
    u64 *stat);

/*
 * vxge_hal_vpath_xmac_tx_stats_get - Get the TX Statistics of a vpath
 * @virtual_path: vpath handle.
 * @vpath_tx_stats: Buffer to return TX Statistics of vpath.
 *
 * Get the TX Statistics of a vpath
 *
 */
vxge_hal_status_e
vxge_hal_vpath_xmac_tx_stats_get(vxge_hal_vpath_h virtual_path,
    vxge_hal_xmac_vpath_tx_stats_t *vpath_tx_stats);

/*
 * vxge_hal_vpath_xmac_rx_stats_get - Get the RX Statistics of a vpath
 * @virtual_path: vpath handle.
 * @vpath_rx_stats: Buffer to return RX Statistics of vpath.
 *
 * Get the RX Statistics of a vpath
 *
 */
vxge_hal_status_e
vxge_hal_vpath_xmac_rx_stats_get(vxge_hal_vpath_h virtual_path,
    vxge_hal_xmac_vpath_rx_stats_t *vpath_rx_stats);

/*
 * vxge_hal_vpath_stats_clear - Clear all the statistics of vpath
 * @vpath_handle: Virtual path handle.
 *
 * Clear the statistics of the given vpath.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_stats_clear(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_device_hw_stats_enable - Enable device h/w statistics.
 * @devh: HAL Device.
 *
 * Enable the DMA vpath statistics for the device. The function is to be called
 * to re-enable the adapter to update stats into the host memory
 *
 * See also: vxge_hal_device_hw_stats_disable()
 */
vxge_hal_status_e
vxge_hal_device_hw_stats_enable(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_hw_stats_disable - Disable device h/w statistics.
 * @devh: HAL Device.
 *
 * Enable the DMA vpath statistics for the device. The function is to be called
 * to disable the adapter to update stats into the host memory. This function
 * is not needed to be called, normally.
 *
 * See also: vxge_hal_device_hw_stats_enable()
 */
vxge_hal_status_e
vxge_hal_device_hw_stats_disable(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_hw_stats_get - Get the device hw statistics.
 * @devh: HAL Device.
 * @hw_stats: Hardware stats
 *
 * Returns the vpath h/w stats for the device.
 *
 * See also: vxge_hal_device_hw_stats_enable(),
 *	     vxge_hal_device_hw_stats_disable(),
 *	     vxge_hal_device_sw_stats_get(),
 *	     vxge_hal_device_stats_get()
 */
vxge_hal_status_e
vxge_hal_device_hw_stats_get(
    vxge_hal_device_h devh,
    vxge_hal_device_stats_hw_info_t *hw_stats);

/*
 * vxge_hal_device_sw_stats_get - Get the device sw statistics.
 * @devh: HAL Device.
 * @sw_stats: Software stats
 *
 * Returns the device s/w stats for the device.
 *
 * See also: vxge_hal_device_hw_stats_get(), vxge_hal_device_stats_get()
 */
vxge_hal_status_e
vxge_hal_device_sw_stats_get(
    vxge_hal_device_h devh,
    vxge_hal_device_stats_sw_info_t *sw_stats);

/*
 * vxge_hal_device_stats_get - Get the device statistics.
 * @devh: HAL Device.
 * @stats: Device stats
 *
 * Returns the device stats for the device.
 *
 * See also: vxge_hal_device_hw_stats_get(), vxge_hal_device_sw_stats_get()
 */
vxge_hal_status_e
vxge_hal_device_stats_get(
    vxge_hal_device_h devh,
    vxge_hal_device_stats_t *stats);

/*
 * vxge_hal_device_xmac_stats_get - Get the XMAC Statistics
 * @devh: HAL device handle.
 * @xmac_stats: Buffer to return XMAC Statistics.
 *
 * Get the XMAC Statistics
 *
 */
vxge_hal_status_e
vxge_hal_device_xmac_stats_get(vxge_hal_device_h devh,
    vxge_hal_device_xmac_stats_t *xmac_stats);

/*
 * vxge_hal_mrpcim_stats_enable - Enable mrpcim statistics.
 * @devh: HAL Device.
 *
 * Enable the DMA mrpcim statistics for the device. The function is to be called
 * to re-enable the adapter to update stats into the host memory
 *
 * See also: vxge_hal_mrpcim_stats_disable()
 */
vxge_hal_status_e
vxge_hal_mrpcim_stats_enable(
    vxge_hal_device_h devh);

/*
 * vxge_hal_mrpcim_stats_disable - Disable mrpcim statistics.
 * @devh: HAL Device.
 *
 * Enable the DMA mrpcim statistics for the device. The function is to be called
 * to disable the adapter to update stats into the host memory. This function
 * is not needed to be called, normally.
 *
 * See also: vxge_hal_mrpcim_stats_enable()
 */
vxge_hal_status_e
vxge_hal_mrpcim_stats_disable(
    vxge_hal_device_h devh);

/*
 * vxge_hal_mrpcim_stats_get - Get the mrpcim statistics.
 * @devh: HAL Device.
 * @stats: mrpcim stats
 *
 * Returns the device mrpcim stats for the device.
 *
 * See also: vxge_hal_device_stats_get()
 */
vxge_hal_status_e
vxge_hal_mrpcim_stats_get(
    vxge_hal_device_h devh,
    vxge_hal_mrpcim_stats_hw_info_t *stats);

/*
 * vxge_hal_mrpcim_stats_access - Access the statistics from the given location
 *			  and offset and perform an operation
 * @devh: HAL Device handle.
 * @operation: Operation to be performed
 * @location: Location (one of vpath id, aggregate or port)
 * @offset: Offset with in the location
 * @stat: Pointer to a buffer to return the value
 *
 * Get the statistics from the given location and offset.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_stats_access(
    vxge_hal_device_h devh,
    u32 operation,
    u32 location,
    u32 offset,
    u64 *stat);

/*
 * vxge_hal_mrpcim_xmac_aggr_stats_get - Get the Statistics on aggregate port
 * @devh: HAL device handle.
 * @port: Number of the port (0 or 1)
 * @aggr_stats: Buffer to return Statistics on aggregate port.
 *
 * Get the Statistics on aggregate port
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_xmac_aggr_stats_get(vxge_hal_device_h devh,
    u32 port,
    vxge_hal_xmac_aggr_stats_t *aggr_stats);

/*
 * vxge_hal_mrpcim_xmac_port_stats_get - Get the Statistics on a port
 * @devh: HAL device handle.
 * @port: Number of the port (wire 0, wire 1 or LAG)
 * @port_stats: Buffer to return Statistics on a port.
 *
 * Get the Statistics on port
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_xmac_port_stats_get(vxge_hal_device_h devh,
    u32 port,
    vxge_hal_xmac_port_stats_t *port_stats);

/*
 * vxge_hal_mrpcim_xmac_stats_get - Get the XMAC Statistics
 * @devh: HAL device handle.
 * @xmac_stats: Buffer to return XMAC Statistics.
 *
 * Get the XMAC Statistics
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_xmac_stats_get(vxge_hal_device_h devh,
    vxge_hal_mrpcim_xmac_stats_t *xmac_stats);

/*
 * vxge_hal_mrpcim_stats_clear - Clear the statistics of the device
 * @devh: HAL Device handle.
 *
 * Clear the statistics of the given Device.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_stats_clear(
    vxge_hal_device_h devh);

/*
 * vxge_hal_mrpcim_xpak_stats_poll -  Poll and update the Xpak error count.
 * @devh: HAL device handle
 * @port: Port number
 *
 * It is used to update the xpak stats value. Called by ULD periodically
 */
vxge_hal_status_e
vxge_hal_mrpcim_xpak_stats_poll(
    vxge_hal_device_h devh, u32 port);

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_STATS_H */
