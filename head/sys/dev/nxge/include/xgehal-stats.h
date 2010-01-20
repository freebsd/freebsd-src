/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
 * All rights reserved.
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

#ifndef XGE_HAL_STATS_H
#define XGE_HAL_STATS_H

#include <dev/nxge/include/xge-os-pal.h>
#include <dev/nxge/include/xge-debug.h>
#include <dev/nxge/include/xgehal-types.h>
#include <dev/nxge/include/xgehal-config.h>

__EXTERN_BEGIN_DECLS

/**
 * struct xge_hal_stats_hw_info_t - Xframe hardware statistics.
 * Transmit MAC Statistics:
 *
 * @tmac_frms: Count of successfully transmitted MAC
 * frames Note that this statistic may be inaccurate. The correct statistic may
 * be derived by calcualating (tmac_ttl_octets - tmac_ttl_less_fb_octets) / 8
 *
 * @tmac_data_octets: Count of data and padding octets of successfully
 * transmitted frames.
 *
 * @tmac_drop_frms: Count of frames that could not be sent for no other reason
 * than internal MAC processing. Increments once whenever the
 * transmit buffer is flushed (due to an ECC error on a memory descriptor).
 *
 * @tmac_mcst_frms: Count of successfully transmitted frames to a multicast
 * address. Does not include frames sent to the broadcast address.
 *
 * @tmac_bcst_frms: Count of successfully transmitted frames to the broadcast
 * address.
 *
 * @tmac_pause_ctrl_frms: Count of MAC PAUSE control frames that are
 * transmitted. Since, the only control frames supported by this device
 * are PAUSE frames, this register is a count of all transmitted MAC control
 * frames.
 *
 * @tmac_ttl_octets: Count of total octets of transmitted frames, including
 * framing characters.
 *
 * @tmac_ucst_frms: Count of transmitted frames containing a unicast address.
 * @tmac_nucst_frms: Count of transmitted frames containing a non-unicast
 * (broadcast, multicast) address.
 *
 * @tmac_any_err_frms: Count of transmitted frames containing any error that
 * prevents them from being passed to the network. Increments if there is an ECC
 * while reading the frame out of the transmit buffer.
 *
 * @tmac_ttl_less_fb_octets: Count of total octets of transmitted
 * frames, not including framing characters (i.e. less framing bits)
 *
 * @tmac_vld_ip_octets: Count of total octets of transmitted IP datagrams that
 * were passed to the network. Frames that are padded by the host have
 * their padding counted as part of the IP datagram.
 *
 * @tmac_vld_ip: Count of transmitted IP datagrams that were passed to the
 * network.
 *
 * @tmac_drop_ip: Count of transmitted IP datagrams that could not be passed to
 * the network. Increments because of 1) an internal processing error (such as
 * an uncorrectable ECC error); 2) a frame parsing error during IP checksum
 * calculation.
 *
 * @tmac_icmp: Count of transmitted ICMP messages. Includes messages not sent
 * due to problems within ICMP.
 *
 * @tmac_rst_tcp: Count of transmitted TCP segments containing the RST flag.
 *
 * @tmac_tcp: Count of transmitted TCP segments. Note that Xena has
 * no knowledge of retransmission.
 *
 * @tmac_udp: Count of transmitted UDP datagrams.
 * @reserved_0: Reserved.
 *
 * Receive MAC Statistics:
 * @rmac_vld_frms: Count of successfully received MAC frames. Does not include
 * frames received with frame-too-long, FCS, or length errors.
 *
 * @rmac_data_octets: Count of data and padding octets of successfully received
 * frames. Does not include frames received with frame-too-long, FCS, or length
 * errors.
 *
 * @rmac_fcs_err_frms: Count of received MAC frames that do not pass FCS. Does
 * not include frames received with frame-too-long or frame-too-short error.
 *
 * @rmac_drop_frms: Count of received frames that could not be passed to the
 * host because of 1) Random Early Discard (RED); 2) Frame steering algorithm
 * found no available queue; 3) Receive ingress buffer overflow.
 *
 * @rmac_vld_mcst_frms: Count of successfully received MAC frames containing a
 * multicast address. Does not include frames received with frame-too-long, FCS,
 * or length errors.
 *
 * @rmac_vld_bcst_frms: Count of successfully received MAC frames containing a
 * broadcast address. Does not include frames received with frame-too-long, FCS,
 * or length errors.
 *
 * @rmac_in_rng_len_err_frms: Count of received frames with a length/type field
 * value between 46 (42 for VLANtagged frames) and 1500 (also 1500 for
 * VLAN-tagged frames), inclusive, that does not match the number of data octets
 * (including pad) received. Also contains a count of received frames with a
 * length/type field less than 46 (42 for VLAN-tagged frames) and the number of
 * data octets (including pad) received is greater than 46 (42 for VLAN-tagged
 * frames).
 *
 * @rmac_out_rng_len_err_frms: Count of received frames with length/type field
 * between 1501 and 1535 decimal, inclusive.
 *
 * @rmac_long_frms: Count of received frames that are longer than
 * rmac_max_pyld_len + 18 bytes (+22 bytes if VLAN-tagged).
 *
 * @rmac_pause_ctrl_frms: Count of received MAC PAUSE control frames.
 *
 * @rmac_unsup_ctrl_frms: Count of received MAC control frames
 * that do not contain the PAUSE opcode. The sum of MAC_PAUSE_CTRL_FRMS and this
 * register is a count of all received MAC control frames.
 *
 * @rmac_ttl_octets: Count of total octets of received frames, including framing
 * characters.
 *
 * @rmac_accepted_ucst_frms: Count of successfully received frames
 * containing a unicast address. Only includes frames that are passed to the
 * system.
 *
 * @rmac_accepted_nucst_frms: Count of successfully received frames
 * containing a non-unicast (broadcast or multicast) address. Only includes
 * frames that are passed to the system. Could include, for instance,
 * non-unicast frames that contain FCS errors if the MAC_ERROR_CFG register is
 * set to pass FCSerrored frames to the host.
 *
 * @rmac_discarded_frms: Count of received frames containing any error that
 * prevents them from being passed to the system. Includes, for example,
 * received pause frames that are discarded by the MAC and frames discarded
 * because of their destination address.
 *
 * @rmac_drop_events: Because the RMAC drops one frame at a time, this stat
 * matches rmac_drop_frms.
 *
 * @reserved_1: Reserved.
 * @rmac_ttl_less_fb_octets: Count of total octets of received frames,
 * not including framing characters (i.e. less framing bits).
 *
 * @rmac_ttl_frms: Count of all received MAC frames, including frames received
 * with frame-too-long, FCS, or length errors.
 *
 * @reserved_2: Reserved.
 * @reserved_3: Reserved.
 * @rmac_usized_frms: Count of received frames of length (including FCS, but not
 * framing bits) less than 64 octets, that are otherwise well-formed.
 *
 * @rmac_osized_frms: Count of received frames of length (including FCS, but not
 * framing bits) more than 1518 octets, that are otherwise well-formed.
 *
 * @rmac_frag_frms: Count of received frames of length (including FCS, but not
 * framing bits) less than 64 octets that had bad FCS. In other words, counts
 * fragments (i.e. runts).
 *
 * @rmac_jabber_frms: Count of received frames of length (including FCS, but not
 * framing bits) more than MTU octets that had bad FCS. In other words, counts
 * jabbers.
 *
 * @reserved_4: Reserved.
 * @rmac_ttl_64_frms: Count of all received MAC frames with length (including
 * FCS, but not framing bits) of exactly 64 octets. Includes frames received
 * with frame-too-long, FCS, or length errors.
 *
 * @rmac_ttl_65_127_frms: Count of all received MAC frames with length
 * (including FCS, but not framing bits) of between 65 and 127 octets
 * inclusive. Includes frames received with frame-too-long, FCS, or length
 * errors.
 * @reserved_5: Reserved.
 * @rmac_ttl_128_255_frms: Count of all received MAC frames with length
 * (including FCS, but not framing bits) of between 128 and 255 octets
 * inclusive. Includes frames received with frame-too-long, FCS, or length
 * errors.
 *
 * @rmac_ttl_256_511_frms: Count of all received MAC frames with length
 * (including FCS, but not framing bits) of between 256 and 511 octets
 * inclusive. Includes frames received with frame-too-long, FCS, or length
 * errors.
 *
 * @reserved_6: Reserved.
 * @rmac_ttl_512_1023_frms: Count of all received MAC frames with length
 * (including FCS, but not framing bits) of between 512 and 1023 octets
 * inclusive. Includes frames received with frame-too-long, FCS, or length
 * errors.
 *
 * @rmac_ttl_1024_1518_frms: Count of all received MAC frames with length
 * (including FCS, but not framing bits) of between 1024 and 1518 octets
 * inclusive. Includes frames received with frame-too-long, FCS, or length
 * errors.
 * @reserved_7: Reserved.
 * @rmac_ip: Count of received IP datagrams. Includes errored IP datagrams.
 *
 * @rmac_ip_octets: Count of number of octets in received IP datagrams. Includes
 * errored IP datagrams.
 *
 * @rmac_hdr_err_ip: Count of received IP datagrams that are discarded due to IP
 * header errors.
 *
 * @rmac_drop_ip: Count of received IP datagrams that could not be passed to the
 * host because of 1) Random Early Discard (RED); 2) Frame steering algorithm
 * found no available queue; 3) Receive ingress buffer overflow.
 * @rmac_icmp: Count of received ICMP messages. Includes errored ICMP messages
 * (due to ICMP checksum fail).
 *
 * @reserved_8: Reserved.
 * @rmac_tcp: Count of received TCP segments. Since Xena is unaware of
 * connection context, counts all received TCP segments, regardless of whether
 * or not they pertain to an established connection.
 *
 * @rmac_udp: Count of received UDP datagrams.
 * @rmac_err_drp_udp: Count of received UDP datagrams that were not delivered to
 * the system because of 1) Random Early Discard (RED); 2) Frame steering
 * algorithm found no available queue; 3) Receive ingress buffer overflow.
 *
 * @rmac_xgmii_err_sym: Count of the number of symbol errors in the received
 * XGMII data (i.e. PHY indicates "Receive Error" on the XGMII). Only includes
 * symbol errors that are observed between the XGMII Start Frame Delimiter
 * and End Frame Delimiter, inclusive. And only increments the count by one for
 * each frame.
 *
 * @rmac_frms_q0: Count of number of frames that pass through queue 0 of receive
 * buffer.
 * @rmac_frms_q1: Count of number of frames that pass through queue 1 of receive
 * buffer.
 * @rmac_frms_q2: Count of number of frames that pass through queue 2 of receive
 * buffer.
 * @rmac_frms_q3: Count of number of frames that pass through queue 3 of receive
 * buffer.
 * @rmac_frms_q4: Count of number of frames that pass through queue 4 of receive
 * buffer.
 * @rmac_frms_q5: Count of number of frames that pass through queue 5 of receive
 * buffer.
 * @rmac_frms_q6: Count of number of frames that pass through queue 6 of receive
 * buffer.
 * @rmac_frms_q7: Count of number of frames that pass through queue 7 of receive
 * buffer.
 * @rmac_full_q0: Count of number of times that receive buffer queue 0 has
 * filled up. If a queue is size 0, then this stat is incremented to a value of
 * 1 when MAC receives its first frame.
 *
 * @rmac_full_q1: Count of number of times that receive buffer queue 1 has
 * filled up. If a queue is size 0, then this stat is incremented to a value of
 * 1 when MAC receives its first frame.
 *
 * @rmac_full_q2: Count of number of times that receive buffer queue 2 has
 * filled up. If a queue is size 0, then this stat is incremented to a value of
 * 1 when MAC receives its first frame.
 *
 * @rmac_full_q3: Count of number of times that receive buffer queue 3 has
 * filled up. If a queue is size 0, then this stat is incremented to a value of
 * 1 when MAC receives its first frame.
 *
 * @rmac_full_q4: Count of number of times that receive buffer queue 4 has
 * filled up. If a queue is size 0, then this stat is incremented to a value of
 * 1 when MAC receives its first frame.
 *
 * @rmac_full_q5: Count of number of times that receive buffer queue 5 has
 * filled up. If a queue is size 0, then this stat is incremented to a value of
 * 1 when MAC receives its first frame.
 *
 * @rmac_full_q6: Count of number of times that receive buffer queue 6 has
 * filled up. If a queue is size 0, then this stat is incremented to a value of
 * 1 when MAC receives its first frame.
 *
 * @rmac_full_q7: Count of number of times that receive buffer queue 7 has
 * filled up. If a queue is size 0, then this stat is incremented to a value of
 * 1 when MAC receives its first frame.
 *
 * @rmac_pause_cnt: Count of number of pause quanta that the MAC has been in the
 * paused state. Recall, one pause quantum equates to 512 bit times.
 * @reserved_9: Reserved.
 * @rmac_xgmii_data_err_cnt: This counter is incremented when either 1) The
 * Reconcilliation Sublayer (RS) is expecting one control character and gets
 * another (i.e. expecting Start control character and gets another control
 * character); 2) Start control character is not in lane 0 or lane 4; 3) The RS
 * gets a Start control character, but the start frame delimiter is not found in
 * the correct location.
 * @rmac_xgmii_ctrl_err_cnt: Maintains a count of unexpected or
 * misplaced control characters occuring outside of normal data transmission
 * (i.e. not included in RMAC_XGMII_DATA_ERR_CNT).
 *
 * @rmac_accepted_ip: Count of received IP datagrams that were passed to the
 * system.
 *
 * @rmac_err_tcp: Count of received TCP segments containing errors. For example,
 * bad TCP checksum.
 *
 * PCI (bus) Statistics:
 * @rd_req_cnt: Counts the total number of read requests made by the device.
 * @new_rd_req_cnt: Counts the requests made for a new read sequence (request
 * made for the same sequence after a retry or disconnect response are not
 * counted).
 * @new_rd_req_rtry_cnt: Counts the Retry responses received on the start of
 * the new read sequences.
 * @rd_rtry_cnt: Counts the Retry responses received for read requests.
 * @wr_rtry_rd_ack_cnt: Increments whenever a read request is accepted by
 * the target after a write request was terminated with retry.
 * @wr_req_cnt: Counts the total number of Write requests made by the device.
 * @new_wr_req_cnt: Counts the requests made for a new write sequence (request
 * made for the same sequence after a retry or disconnect response are not
 * counted).
 * @new_wr_req_rtry_cnt: Counts the requests made for a new write sequence
 * (request made for the same sequence after a retry or disconnect response are
 * not counted).
 *
 * @wr_rtry_cnt: Counts the Retry responses received for write requests.
 * @wr_disc_cnt: Write Disconnect. Counts the target initiated disconnects
 * on write transactions.
 * @rd_rtry_wr_ack_cnt: Increments whenever a write request is accepted by the
 * target after a read request was terminated with retry.
 *
 * @txp_wr_cnt: Counts the host write transactions to the Tx Pointer
 * FIFOs.
 * @txd_rd_cnt: Count of the Transmit Descriptor (TxD) read requests.
 * @txd_wr_cnt: Count of the TxD write requests.
 * @rxd_rd_cnt: Count of the Receive Descriptor (RxD) read requests.
 * @rxd_wr_cnt: Count of the RxD write requests.
 * @txf_rd_cnt: Count of transmit frame read requests. This will not
 * equal the number of frames transmitted, as frame data is typically spread
 * across multiple PCI transactions.
 * @rxf_wr_cnt: Count of receive frame write requests.
 *
 * @tmac_frms_oflow: tbd
 * @tmac_data_octets_oflow: tbd
 * @tmac_mcst_frms_oflow: tbd
 * @tmac_bcst_frms_oflow: tbd
 * @tmac_ttl_octets_oflow: tbd
 * @tmac_ucst_frms_oflow: tbd
 * @tmac_nucst_frms_oflow: tbd
 * @tmac_any_err_frms_oflow: tbd
 * @tmac_vlan_frms: tbd
 * @tmac_vld_ip_oflow: tbd
 * @tmac_drop_ip_oflow: tbd
 * @tmac_icmp_oflow: tbd
 * @tmac_rst_tcp_oflow: tbd
 * @tmac_udp_oflow: tbd
 * @reserved_10: tbd
 * @tpa_unknown_protocol: tbd
 * @tpa_parse_failure: tbd
 * @rmac_vld_frms_oflow: tbd
 * @rmac_data_octets_oflow: tbd
 * @rmac_vld_mcst_frms_oflow: tbd
 * @rmac_vld_bcst_frms_oflow: tbd
 * @rmac_ttl_octets_oflow: tbd
 * @rmac_accepted_ucst_frms_oflow: tbd
 * @rmac_accepted_nucst_frms_oflow: tbd
 * @rmac_discarded_frms_oflow: tbd
 * @rmac_drop_events_oflow: tbd
 * @rmac_usized_frms_oflow: tbd
 * @rmac_osized_frms_oflow: tbd
 * @rmac_frag_frms_oflow: tbd
 * @rmac_jabber_frms_oflow: tbd
 * @rmac_ip_oflow: tbd
 * @rmac_drop_ip_oflow: tbd
 * @rmac_icmp_oflow: tbd
 * @rmac_udp_oflow: tbd
 * @reserved_11: tbd
 * @rmac_err_drp_udp_oflow: tbd
 * @rmac_pause_cnt_oflow: tbd
 * @rmac_ttl_1519_4095_frms: tbd
 * @rmac_ttl_4096_8191_frms: tbd
 * @rmac_ttl_8192_max_frms: tbd
 * @rmac_ttl_gt_max_frms: tbd
 * @rmac_osized_alt_frms: tbd
 * @rmac_jabber_alt_frms: tbd
 * @rmac_gt_max_alt_frms: tbd
 * @rmac_vlan_frms: tbd
 * @rmac_fcs_discard: tbd
 * @rmac_len_discard: tbd
 * @rmac_da_discard: tbd
 * @rmac_pf_discard: tbd
 * @rmac_rts_discard: tbd
 * @rmac_wol_discard: tbd
 * @rmac_red_discard: tbd
 * @rmac_ingm_full_discard: tbd
 * @rmac_accepted_ip_oflow: tbd
 * @reserved_12: tbd
 * @link_fault_cnt: TBD
 * @reserved_13: tbd
 * Xframe hardware statistics.
 */
typedef struct xge_hal_stats_hw_info_t {
#ifdef  XGE_OS_HOST_BIG_ENDIAN
/* Tx MAC statistics counters. */
	u32 tmac_frms;
	u32 tmac_data_octets;
	u64 tmac_drop_frms;
	u32 tmac_mcst_frms;
	u32 tmac_bcst_frms;
	u64 tmac_pause_ctrl_frms;
	u32 tmac_ttl_octets;
	u32 tmac_ucst_frms;
	u32 tmac_nucst_frms;
	u32 tmac_any_err_frms;
	u64 tmac_ttl_less_fb_octets;
	u64 tmac_vld_ip_octets;
	u32 tmac_vld_ip;
	u32 tmac_drop_ip;
	u32 tmac_icmp;
	u32 tmac_rst_tcp;
	u64 tmac_tcp;
	u32 tmac_udp;
	u32 reserved_0;

/* Rx MAC Statistics counters. */
	u32 rmac_vld_frms;
	u32 rmac_data_octets;
	u64 rmac_fcs_err_frms;
	u64 rmac_drop_frms;
	u32 rmac_vld_mcst_frms;
	u32 rmac_vld_bcst_frms;
	u32 rmac_in_rng_len_err_frms;
	u32 rmac_out_rng_len_err_frms;
	u64 rmac_long_frms;
	u64 rmac_pause_ctrl_frms;
	u64 rmac_unsup_ctrl_frms;
	u32 rmac_ttl_octets;
	u32 rmac_accepted_ucst_frms;
	u32 rmac_accepted_nucst_frms;
	u32 rmac_discarded_frms;
	u32 rmac_drop_events;
	u32 reserved_1;
	u64 rmac_ttl_less_fb_octets;
	u64 rmac_ttl_frms;
	u64 reserved_2;
	u32 reserved_3;
	u32 rmac_usized_frms;
	u32 rmac_osized_frms;
	u32 rmac_frag_frms;
	u32 rmac_jabber_frms;
	u32 reserved_4;
	u64 rmac_ttl_64_frms;
	u64 rmac_ttl_65_127_frms;
	u64 reserved_5;
	u64 rmac_ttl_128_255_frms;
	u64 rmac_ttl_256_511_frms;
	u64 reserved_6;
	u64 rmac_ttl_512_1023_frms;
	u64 rmac_ttl_1024_1518_frms;
	u32 reserved_7;
	u32 rmac_ip;
	u64 rmac_ip_octets;
	u32 rmac_hdr_err_ip;
	u32 rmac_drop_ip;
	u32 rmac_icmp;
	u32 reserved_8;
	u64 rmac_tcp;
	u32 rmac_udp;
	u32 rmac_err_drp_udp;
	u64 rmac_xgmii_err_sym;
	u64 rmac_frms_q0;
	u64 rmac_frms_q1;
	u64 rmac_frms_q2;
	u64 rmac_frms_q3;
	u64 rmac_frms_q4;
	u64 rmac_frms_q5;
	u64 rmac_frms_q6;
	u64 rmac_frms_q7;
	u16 rmac_full_q0;
	u16 rmac_full_q1;
	u16 rmac_full_q2;
	u16 rmac_full_q3;
	u16 rmac_full_q4;
	u16 rmac_full_q5;
	u16 rmac_full_q6;
	u16 rmac_full_q7;
	u32 rmac_pause_cnt;
	u32 reserved_9;
	u64 rmac_xgmii_data_err_cnt;
	u64 rmac_xgmii_ctrl_err_cnt;
	u32 rmac_accepted_ip;
	u32 rmac_err_tcp;

/* PCI/PCI-X Read transaction statistics. */
	u32 rd_req_cnt;
	u32 new_rd_req_cnt;
	u32 new_rd_req_rtry_cnt;
	u32 rd_rtry_cnt;
	u32 wr_rtry_rd_ack_cnt;

/* PCI/PCI-X write transaction statistics. */
	u32 wr_req_cnt;
	u32 new_wr_req_cnt;
	u32 new_wr_req_rtry_cnt;
	u32 wr_rtry_cnt;
	u32 wr_disc_cnt;
	u32 rd_rtry_wr_ack_cnt;

/*  DMA Transaction statistics. */
	u32 txp_wr_cnt;
	u32 txd_rd_cnt;
	u32 txd_wr_cnt;
	u32 rxd_rd_cnt;
	u32 rxd_wr_cnt;
	u32 txf_rd_cnt;
	u32 rxf_wr_cnt;

/* Enhanced Herc statistics */
	u32 tmac_frms_oflow;
	u32 tmac_data_octets_oflow;
	u32 tmac_mcst_frms_oflow;
	u32 tmac_bcst_frms_oflow;
	u32 tmac_ttl_octets_oflow;
	u32 tmac_ucst_frms_oflow;
	u32 tmac_nucst_frms_oflow;
	u32 tmac_any_err_frms_oflow;
	u64 tmac_vlan_frms;
	u32 tmac_vld_ip_oflow;
	u32 tmac_drop_ip_oflow;
	u32 tmac_icmp_oflow;
	u32 tmac_rst_tcp_oflow;
	u32 tmac_udp_oflow;
	u32 tpa_unknown_protocol;
	u32 tpa_parse_failure;
	u32 reserved_10;
	u32 rmac_vld_frms_oflow;
	u32 rmac_data_octets_oflow;
	u32 rmac_vld_mcst_frms_oflow;
	u32 rmac_vld_bcst_frms_oflow;
	u32 rmac_ttl_octets_oflow;
	u32 rmac_accepted_ucst_frms_oflow;
	u32 rmac_accepted_nucst_frms_oflow;
	u32 rmac_discarded_frms_oflow;
	u32 rmac_drop_events_oflow;
	u32 rmac_usized_frms_oflow;
	u32 rmac_osized_frms_oflow;
	u32 rmac_frag_frms_oflow;
	u32 rmac_jabber_frms_oflow;
	u32 rmac_ip_oflow;
	u32 rmac_drop_ip_oflow;
	u32 rmac_icmp_oflow;
	u32 rmac_udp_oflow;
	u32 rmac_err_drp_udp_oflow;
	u32 rmac_pause_cnt_oflow;
	u32 reserved_11;
	u64 rmac_ttl_1519_4095_frms;
	u64 rmac_ttl_4096_8191_frms;
	u64 rmac_ttl_8192_max_frms;
	u64 rmac_ttl_gt_max_frms;
	u64 rmac_osized_alt_frms;
	u64 rmac_jabber_alt_frms;
	u64 rmac_gt_max_alt_frms;
	u64 rmac_vlan_frms;
	u32 rmac_fcs_discard;
	u32 rmac_len_discard;
	u32 rmac_da_discard;
	u32 rmac_pf_discard;
	u32 rmac_rts_discard;
	u32 rmac_wol_discard;
	u32 rmac_red_discard;
	u32 rmac_ingm_full_discard;
	u32 rmac_accepted_ip_oflow;
	u32 reserved_12;
	u32 link_fault_cnt;
	u32 reserved_13;
#else
/* Tx MAC statistics counters. */
	u32 tmac_data_octets;
	u32 tmac_frms;
	u64 tmac_drop_frms;
	u32 tmac_bcst_frms;
	u32 tmac_mcst_frms;
	u64 tmac_pause_ctrl_frms;
	u32 tmac_ucst_frms;
	u32 tmac_ttl_octets;
	u32 tmac_any_err_frms;
	u32 tmac_nucst_frms;
	u64 tmac_ttl_less_fb_octets;
	u64 tmac_vld_ip_octets;
	u32 tmac_drop_ip;
	u32 tmac_vld_ip;
	u32 tmac_rst_tcp;
	u32 tmac_icmp;
	u64 tmac_tcp;
	u32 reserved_0;
	u32 tmac_udp;

/* Rx MAC Statistics counters. */
	u32 rmac_data_octets;
	u32 rmac_vld_frms;
	u64 rmac_fcs_err_frms;
	u64 rmac_drop_frms;
	u32 rmac_vld_bcst_frms;
	u32 rmac_vld_mcst_frms;
	u32 rmac_out_rng_len_err_frms;
	u32 rmac_in_rng_len_err_frms;
	u64 rmac_long_frms;
	u64 rmac_pause_ctrl_frms;
	u64 rmac_unsup_ctrl_frms;
	u32 rmac_accepted_ucst_frms;
	u32 rmac_ttl_octets;
	u32 rmac_discarded_frms;
	u32 rmac_accepted_nucst_frms;
	u32 reserved_1;
	u32 rmac_drop_events;
	u64 rmac_ttl_less_fb_octets;
	u64 rmac_ttl_frms;
	u64 reserved_2;
	u32 rmac_usized_frms;
	u32 reserved_3;
	u32 rmac_frag_frms;
	u32 rmac_osized_frms;
	u32 reserved_4;
	u32 rmac_jabber_frms;
	u64 rmac_ttl_64_frms;
	u64 rmac_ttl_65_127_frms;
	u64 reserved_5;
	u64 rmac_ttl_128_255_frms;
	u64 rmac_ttl_256_511_frms;
	u64 reserved_6;
	u64 rmac_ttl_512_1023_frms;
	u64 rmac_ttl_1024_1518_frms;
	u32 rmac_ip;
	u32 reserved_7;
	u64 rmac_ip_octets;
	u32 rmac_drop_ip;
	u32 rmac_hdr_err_ip;
	u32 reserved_8;
	u32 rmac_icmp;
	u64 rmac_tcp;
	u32 rmac_err_drp_udp;
	u32 rmac_udp;
	u64 rmac_xgmii_err_sym;
	u64 rmac_frms_q0;
	u64 rmac_frms_q1;
	u64 rmac_frms_q2;
	u64 rmac_frms_q3;
	u64 rmac_frms_q4;
	u64 rmac_frms_q5;
	u64 rmac_frms_q6;
	u64 rmac_frms_q7;
	u16 rmac_full_q3;
	u16 rmac_full_q2;
	u16 rmac_full_q1;
	u16 rmac_full_q0;
	u16 rmac_full_q7;
	u16 rmac_full_q6;
	u16 rmac_full_q5;
	u16 rmac_full_q4;
	u32 reserved_9;
	u32 rmac_pause_cnt;
	u64 rmac_xgmii_data_err_cnt;
	u64 rmac_xgmii_ctrl_err_cnt;
	u32 rmac_err_tcp;
	u32 rmac_accepted_ip;

/* PCI/PCI-X Read transaction statistics. */
	u32 new_rd_req_cnt;
	u32 rd_req_cnt;
	u32 rd_rtry_cnt;
	u32 new_rd_req_rtry_cnt;

/* PCI/PCI-X Write/Read transaction statistics. */
	u32 wr_req_cnt;
	u32 wr_rtry_rd_ack_cnt;
	u32 new_wr_req_rtry_cnt;
	u32 new_wr_req_cnt;
	u32 wr_disc_cnt;
	u32 wr_rtry_cnt;

/*  PCI/PCI-X Write / DMA Transaction statistics. */
	u32 txp_wr_cnt;
	u32 rd_rtry_wr_ack_cnt;
	u32 txd_wr_cnt;
	u32 txd_rd_cnt;
	u32 rxd_wr_cnt;
	u32 rxd_rd_cnt;
	u32 rxf_wr_cnt;
	u32 txf_rd_cnt;

/* Enhanced Herc statistics */
	u32 tmac_data_octets_oflow;
	u32 tmac_frms_oflow;
	u32 tmac_bcst_frms_oflow;
	u32 tmac_mcst_frms_oflow;
	u32 tmac_ucst_frms_oflow;
	u32 tmac_ttl_octets_oflow;
	u32 tmac_any_err_frms_oflow;
	u32 tmac_nucst_frms_oflow;
	u64 tmac_vlan_frms;
	u32 tmac_drop_ip_oflow;
	u32 tmac_vld_ip_oflow;
	u32 tmac_rst_tcp_oflow;
	u32 tmac_icmp_oflow;
	u32 tpa_unknown_protocol;
	u32 tmac_udp_oflow;
	u32 reserved_10;
	u32 tpa_parse_failure;
	u32 rmac_data_octets_oflow;
	u32 rmac_vld_frms_oflow;
	u32 rmac_vld_bcst_frms_oflow;
	u32 rmac_vld_mcst_frms_oflow;
	u32 rmac_accepted_ucst_frms_oflow;
	u32 rmac_ttl_octets_oflow;
	u32 rmac_discarded_frms_oflow;
	u32 rmac_accepted_nucst_frms_oflow;
	u32 rmac_usized_frms_oflow;
	u32 rmac_drop_events_oflow;
	u32 rmac_frag_frms_oflow;
	u32 rmac_osized_frms_oflow;
	u32 rmac_ip_oflow;
	u32 rmac_jabber_frms_oflow;
	u32 rmac_icmp_oflow;
	u32 rmac_drop_ip_oflow;
	u32 rmac_err_drp_udp_oflow;
	u32 rmac_udp_oflow;
	u32 reserved_11;
	u32 rmac_pause_cnt_oflow;
	u64 rmac_ttl_1519_4095_frms;
	u64 rmac_ttl_4096_8191_frms;
	u64 rmac_ttl_8192_max_frms;
	u64 rmac_ttl_gt_max_frms;
	u64 rmac_osized_alt_frms;
	u64 rmac_jabber_alt_frms;
	u64 rmac_gt_max_alt_frms;
	u64 rmac_vlan_frms;
	u32 rmac_len_discard;
	u32 rmac_fcs_discard;
	u32 rmac_pf_discard;
	u32 rmac_da_discard;
	u32 rmac_wol_discard;
	u32 rmac_rts_discard;
	u32 rmac_ingm_full_discard;
	u32 rmac_red_discard;
	u32 reserved_12;
	u32 rmac_accepted_ip_oflow;
	u32 reserved_13;
	u32 link_fault_cnt;
#endif
} xge_hal_stats_hw_info_t;

/**
 * struct xge_hal_stats_channel_into_t - HAL channel statistics.
 * @full_cnt: TBD
 * @usage_max: TBD
 * @reserve_free_swaps_cnt: Reserve/free swap counter. Internal usage.
 * @max_compl_per_intr_cnt: Maximum number of completions per interrupt.
 * @avg_compl_per_intr_cnt: Average number of completions per interrupt.
 *           Note that a total number of completed descriptors
 *           for the given channel can be calculated as
 *           (@traffic_intr_cnt * @avg_compl_per_intr_cnt).
 * @total_compl_cnt: Total completion count.
 *        @total_compl_cnt == (@traffic_intr_cnt * @avg_compl_per_intr_cnt).
 * @total_posts: Total number of descriptor postings on the channel.
 *        Counts the number of xge_hal_ring_dtr_post()
 *        or xge_hal_fifo_dtr_post() calls by ULD, for ring and fifo
 *        channel, respectively.
 * @total_posts_many: Total number of posts on the channel that involved
 *        more than one descriptor. Counts the number of
 *        xge_hal_fifo_dtr_post_many() calls performed by ULD.
 * @total_buffers: Total number of buffers posted on the channel.
 * @copied_frags: TBD
 * @copied_buffers: TBD
 * @avg_buffers_per_post: Average number of buffers transferred in a single
 *        post operation.
 *        Calculated as @total_buffers/@total_posts.
 * @avg_buffer_size: Average buffer size transferred by a single post
 *       operation on a fifo channel. The counter is not supported for a ring
 *       channel. Calculated as a total number of transmitted octets divided
 *       by @total_buffers.
 * @avg_post_size: Average amount of data transferred by a single post.
 *       Calculated as a total number of transmitted octets divided by
 *       @total_posts.
 * @ring_bump_cnt: Ring "bump" count. Number of times the hardware could
 *       not post receive data (and had to continue keeping it on-board)
 *       because of unavailable receive descriptor(s).
 * @total_posts_dtrs_many: Total number of posts on the channel that involving
 *       more than one descriptor.
 * @total_posts_frags_many: Total number of fragments posted on the channel
 *   during post requests of multiple descriptors.
 * @total_posts_dang_dtrs: Total number of posts on the channel involving
 *       dangling descriptors.
 * @total_posts_dang_frags: Total number of dangling fragments posted on the channel
 *   during post request containing multiple descriptors.
 *
 * HAL channel counters.
 * See also: xge_hal_stats_device_info_t{}.
 */
typedef struct xge_hal_stats_channel_info_t {
	u32 full_cnt;
	u32 usage_max;
	u32 reserve_free_swaps_cnt;
	u32 avg_compl_per_intr_cnt;
	u32 total_compl_cnt;
	u32 total_posts;
	u32 total_posts_many;
	u32 total_buffers;
	u32 copied_frags;
	u32 copied_buffers;
	u32 avg_buffers_per_post;
	u32 avg_buffer_size;
	u32 avg_post_size;
	u32 ring_bump_cnt;
	u32 total_posts_dtrs_many;
	u32 total_posts_frags_many;
	u32 total_posts_dang_dtrs;
	u32 total_posts_dang_frags;
} xge_hal_stats_channel_info_t;

/**
 * struct xge_hal_xpak_counter_t - HAL xpak error counters
 * @excess_temp: excess transceiver_temperature count
 * @excess_bias_current: excess laser_bias_current count
 * @excess_laser_output: excess laser_output_power count
 * @tick_period: tick count for each cycle
 */
typedef struct xge_hal_xpak_counter_t {
	    u32     excess_temp;
	    u32     excess_bias_current;
	    u32     excess_laser_output;
	    u32     tick_period;
} xge_hal_xpak_counter_t;

/**
 * struct xge_hal_stats_xpak_t - HAL xpak stats
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
typedef struct xge_hal_stats_xpak_t {
	    u16     alarm_transceiver_temp_high;
	    u16     alarm_transceiver_temp_low;
	    u16     alarm_laser_bias_current_high;
	    u16     alarm_laser_bias_current_low;
	    u16     alarm_laser_output_power_high;
	    u16     alarm_laser_output_power_low;
	    u16     warn_transceiver_temp_high;
	    u16     warn_transceiver_temp_low;
	    u16     warn_laser_bias_current_high;
	    u16     warn_laser_bias_current_low;
	    u16     warn_laser_output_power_high;
	    u16     warn_laser_output_power_low;
} xge_hal_stats_xpak_t;



/**
 * struct xge_hal_stats_sw_err_t - HAL device error statistics.
 * @sm_err_cnt: TBD
 * @single_ecc_err_cnt: TBD
 * @double_ecc_err_cnt: TBD
 * @ecc_err_cnt: ECC error count.
 * @parity_err_cnt: Parity error count.
 * @serr_cnt: Number of exceptions indicated to the host via PCI SERR#.
 * @rxd_t_code_err_cnt: Array of receive transfer codes. The position
 * (index) in this array reflects the transfer code type, for instance
 * 0x7 - for "invalid receive buffer size", or 0x8 - for ECC.
 * Value rxd_t_code_err_cnt[i] reflects the
 * number of times the corresponding transfer code was encountered.
 *
 * @txd_t_code_err_cnt: Array of transmit transfer codes. The position
 * (index) in this array reflects the transfer code type, for instance
 * 0xA - "loss of link".
 * Value txd_t_code_err_cnt[i] reflects the
 * number of times the corresponding transfer code was encountered.
 * @stats_xpak: TBD
 * @xpak_counter: TBD
 */
typedef struct xge_hal_stats_sw_err_t {
	u32     sm_err_cnt;
	u32     single_ecc_err_cnt;
	u32     double_ecc_err_cnt;
	u32     ecc_err_cnt;
	u32     parity_err_cnt;
	u32     serr_cnt;
	u32     rxd_t_code_err_cnt[16];
	u32     txd_t_code_err_cnt[16];
	xge_hal_stats_xpak_t    stats_xpak;
	xge_hal_xpak_counter_t  xpak_counter;
} xge_hal_stats_sw_err_t;

/**
 * struct xge_hal_stats_device_info_t - HAL own per-device statistics.
 *
 * @rx_traffic_intr_cnt: TBD
 * @tx_traffic_intr_cnt: TBD
 * @txpic_intr_cnt: TBD
 * @txdma_intr_cnt: TBD
 * @txmac_intr_cnt: TBD
 * @txxgxs_intr_cnt: TBD
 * @rxpic_intr_cnt: TBD
 * @rxdma_intr_cnt: TBD
 * @rxmac_intr_cnt: TBD
 * @rxxgxs_intr_cnt: TBD
 * @mc_intr_cnt: TBD
 * @not_traffic_intr_cnt: Number of times the host was interrupted
 *                        without new completions.
 *                        "Non-traffic interrupt counter".
 * @not_xge_intr_cnt: TBD
 * @traffic_intr_cnt: Number of traffic interrupts for the device.
 * @total_intr_cnt: Total number of traffic interrupts for the device.
 *                  @total_intr_cnt == @traffic_intr_cnt +
 *                              @not_traffic_intr_cnt
 * @soft_reset_cnt: Number of times soft reset is done on this device.
 * @rxufca_hi_adjust_cnt: TODO
 * @rxufca_lo_adjust_cnt: TODO
 * @bimodal_hi_adjust_cnt: TODO
 * @bimodal_lo_adjust_cnt: TODO
 *
 * @tot_frms_lroised: TBD
 * @tot_lro_sessions: TBD
 * @lro_frm_len_exceed_cnt: TBD
 * @lro_sg_exceed_cnt: TBD
 * @lro_out_of_seq_pkt_cnt: TBD
 * @lro_dup_pkt_cnt: TBD
 *
 * HAL per-device statistics.
 * See also: xge_hal_stats_channel_info_t{}.
 */
typedef struct xge_hal_stats_device_info_t {
	u32             rx_traffic_intr_cnt;
	u32             tx_traffic_intr_cnt;
	u32             txpic_intr_cnt;
	u32             txdma_intr_cnt;
	u32             pfc_err_cnt;
	u32             tda_err_cnt;
	u32             pcc_err_cnt;
	u32             tti_err_cnt;
	u32             lso_err_cnt;
	u32             tpa_err_cnt;
	u32             sm_err_cnt;
	u32             txmac_intr_cnt;
	u32             mac_tmac_err_cnt;
	u32             txxgxs_intr_cnt;
	u32             xgxs_txgxs_err_cnt;
	u32             rxpic_intr_cnt;
	u32             rxdma_intr_cnt;
	u32             rc_err_cnt;
	u32             rpa_err_cnt;
	u32             rda_err_cnt;
	u32             rti_err_cnt;
	u32             rxmac_intr_cnt;
	u32             mac_rmac_err_cnt;
	u32             rxxgxs_intr_cnt;
	u32             xgxs_rxgxs_err_cnt;
	u32             mc_intr_cnt;
	u32             not_traffic_intr_cnt;
	u32             not_xge_intr_cnt;
	u32             traffic_intr_cnt;
	u32             total_intr_cnt;
	u32             soft_reset_cnt;
	u32             rxufca_hi_adjust_cnt;
	u32             rxufca_lo_adjust_cnt;
	u32             bimodal_hi_adjust_cnt;
	u32             bimodal_lo_adjust_cnt;
#ifdef XGE_HAL_CONFIG_LRO
	u32             tot_frms_lroised;
	u32             tot_lro_sessions;
	u32             lro_frm_len_exceed_cnt;
	u32             lro_sg_exceed_cnt;
	u32             lro_out_of_seq_pkt_cnt;
	u32             lro_dup_pkt_cnt;
#endif
} xge_hal_stats_device_info_t;

/* ========================== XFRAME ER STATISTICS ======================== */
#define XGE_HAL_MAC_LINKS   3
#define XGE_HAL_MAC_AGGREGATORS 2
#define XGE_HAL_VPATHS      17
/**
 * struct xge_hal_stats_link_info_t - XGMAC statistics for a link
 *
 * @tx_frms: Count of transmitted MAC frames for mac the link.
 * @tx_ttl_eth_octets: Count of total octets of transmitted frames
 * for mac the link.
 * @tx_data_octets: Count of data and padding octets of transmitted
 * frames for mac the link.
 * @tx_mcst_frms: Count of multicast MAC frames for mac the link.
 * @tx_bcst_frms: Count of broadcast MAC frames for mac the link.
 * @tx_ucst_frms: Count of unicast MAC frames for mac the link.
 * @tx_tagged_frms: Count of transmitted frames containing a VLAN tag
 * for mac the link.
 * @tx_vld_ip: Count of transmitted IP datagrams for mac the link.
 * @tx_vld_ip_octets: Count of transmitted IP octets for mac the link.
 * @tx_icmp: Count of transmitted ICMP messages for mac the link.
 * @tx_tcp: Count of transmitted TCP segments for mac the link.
 * @tx_rst_tcp: Count of transmitted TCP segments containing the RST
 * flag mac the link.
 * @tx_udp: Count of transmitted UDP datagrams for mac the link.
 * @tx_unknown_protocol: Count of transmitted packets of unknown
 * protocol for mac the link.
 * @tx_parse_error: Count of transmitted packets with parsing errors
 * for mac the link.
 * @tx_pause_ctrl_frms: Count of MAC PAUSE control frames for mac
 * the link.
 * @tx_lacpdu_frms: Count of LACPDUs transmitted for mac the link.
 * @tx_marker_pdu_frms: Count of Marker PDUs transmitted for mac the
 * link.
 * @tx_marker_resp_pdu_frms: Count of Marker Response PDUs transmitted
 * for mac the link.
 * @tx_drop_ip: Count of dropped IP packets from the transmission path
 * for mac the link.
 * @tx_xgmii_char1_match: Count of the number of transmitted XGMII
 * characters that match first pattern, for mac the link.
 * @tx_xgmii_char2_match: Count of the number of transmitted XGMII
 * characters that match second pattern, for mac the link.
 * @tx_xgmii_column1_match: Count of the number of transmitted XGMII
 * columns that match first pattern, for mac the link.
 * @tx_xgmii_column2_match: Count of the number of transmitted XGMII
 * columns that match second pattern, for mac the link.
 * @tx_drop_frms: Count of frames dropped due to internal errors during
 * transmission for mac the link.
 * @tx_any_err_frms: Count of frames dropped due to any error during
 * transmission for mac the link.
 * @rx_ttl_frms: Count of all received MAC frames for mac the link.
 * @rx_vld_frms: Count of all successfully received MAC frames for mac
 * the link.
 * @rx_offld_frms: Count of all offloaded received MAC frames for mac
 * the link.
 * @rx_ttl_eth_octets: Count of total octets of received frames, not
 * including framing characters for mac the link.
 * @rx_data_octets: Count of data and padding octets of successfully
 * received frames for mac the link.
 * @rx_offld_octets: Count of total octets, not including framing
 * characters, of offloaded received frames for mac the link.
 * @rx_vld_mcst_frms: Count of successfully received multicast MAC
 * frames for mac the link.
 * @rx_vld_bcst_frms: Count of successfully received broadcast MAC
 * frames for mac the link.
 * @rx_accepted_ucst_frms: Count of successfully received unicast MAC
 * frames for mac the link.
 * @rx_accepted_nucst_frms: Count of successfully received non-unicast
 * MAC frames for mac the link.
 * @rx_tagged_frms: Count of received frames containing a VLAN tag for
 * mac the link.
 * @rx_long_frms: Count of received frames that are longer than
 * RX_MAX_PYLD_LEN + 18 bytes (+ 22 bytes if VLAN-tagged) for mac the link.
 * @rx_usized_frms: Count of received frames of length less than 64
 * octets, for mac the link.
 * @rx_osized_frms:  Count of received frames of length more than 1518
 * octets for mac the link.
 * @rx_frag_frms:  Count of received frames of length less than 64
 * octets that had bad FCS, for mac the link.
 * @rx_jabber_frms:  Count of received frames of length more than 1518
 * octets that had bad FCS, for mac the link.
 * @rx_ttl_64_frms: Count of all received MAC frames with length of
 * exactly 64 octets, for mac the link.
 * @rx_ttl_65_127_frms: Count of all received MAC frames with length
 * of between 65 and 127 octets inclusive, for mac the link.
 * @rx_ttl_128_255_frms: Count of all received MAC frames with length
 * of between 128 and 255 octets inclusive, for mac the link.
 * @rx_ttl_256_511_frms: Count of all received MAC frames with length
 * of between 246 and 511 octets inclusive, for mac the link.
 * @rx_ttl_512_1023_frms: Count of all received MAC frames with length
 * of between 512 and 1023 octets inclusive, for mac the link.
 * @rx_ttl_1024_1518_frms: Count of all received MAC frames with length
 * of between 1024 and 1518 octets inclusive, for mac the link.
 * @rx_ttl_1519_4095_frms: Count of all received MAC frames with length
 * of between 1519 and 4095 octets inclusive, for mac the link.
 * @rx_ttl_40956_8191_frms: Count of all received MAC frames with length
 * of between 4096 and 8191 octets inclusive, for mac the link.
 * @rx_ttl_8192_max_frms: Count of all received MAC frames with length
 * of between 8192 and RX_MAX_PYLD_LEN+18 octets inclusive, for mac the link.
 * @rx_ttl_gt_max_frms: Count of all received MAC frames with length
 * exceeding RX_MAX_PYLD_LEN+18 octets inclusive, for mac the link.
 * @rx_ip:  Count of received IP datagrams, for mac the link.
 * @rx_accepted_ip:  Count of received  and accepted IP datagrams,
 * for mac the link.
 * @rx_ip_octets: Count of number of octets in received IP datagrams,
 * for mac the link.
 * @rx_hdr_err_ip: Count of received IP datagrams that are discarded
 * due to IP header errors, for mac the link.
 * @rx_icmp: Count of received ICMP messages for mac the link.
 * @rx_tcp: Count of received TCP segments for mac the link.
 * @rx_udp: Count of received UDP datagrams for mac the link.
 * @rx_err_tcp: Count of received TCP segments containing errors for
 * mac the link.
 * @rx_pause_cnt: Count of number of pause quanta that the MAC has
 * been in the paused state, for mac the link.
 * @rx_pause_ctrl_frms: Count of received MAC PAUSE control frames for
 * mac the link.
 * @rx_unsup_ctrl_frms: Count of received MAC control frames that do
 * not contain the PAUSE opcode for mac the link.
 * @rx_fcs_err_frms: Count of received MAC frames that do not pass FCS
 * for mac the link.
 * @rx_in_rng_len_err_frms: Count of received frames with a length/type
 * field value between 46 and 1500 inclusive, that does not match the number
 * of data octets received, for mac the link.
 * @rx_out_rng_len_err_frms: Count of received frames with length/type
 * field between 1501 and 1535 decimal, inclusive. for mac the link.
 * @rx_drop_frms: Count of dropped frames from receive path for mac
 * the link.
 * @rx_discarded_frms: Count of discarded frames from receive path for
 * mac the link.
 * @rx_drop_ip: Count of droppen IP datagrams from receive path for
 * mac the link.
 * @rx_err_drp_udp: Count of droppen UDP datagrams from receive path
 * for mac the link.
 * @rx_lacpdu_frms: Count of valid LACPDUs received for mac the link.
 * @rx_marker_pdu_frms: Count of valid Marker PDUs received for mac
 * the link.
 * @rx_marker_resp_pdu_frms: Count of valid Marker Response PDUs
 * received for mac the link.
 * @rx_unknown_pdu_frms: Count of unknown PDUs received for mac the link.
 * @rx_illegal_pdu_frms: Count of illegal PDUs received for mac the link.
 * @rx_fcs_discard: Count of discarded PDUs received for mac the link.
 * @rx_len_discard: Count of received frames that were discarded
 * because of an invalid frame length, for mac the link.
 * @rx_len_discard: Count of received frames that were discarded
 * because of an invalid destination MAC address, for mac the link.
 * @rx_pf_discard: Count of received frames that were discarded for
 * mac the link.
 * @rx_trash_discard: Count of received frames that were steered to the
 * trash queue for mac the link.
 * @rx_rts_discard: Count of received frames that were discarded by RTS
 * logic for mac the link.
 * @rx_wol_discard: Count of received frames that were discarded by WOL
 * logic for mac the link.
 * @rx_red_discard: Count of received frames that were discarded by RED
 * logic for mac the link.
 * @rx_ingm_full_discard: Count of received frames that were discarded
 * because the internal ingress memory was full for mac the link.
 * @rx_xgmii_data_err_cnt: Count of unexpected control characters
 * during normal data transmission for mac the link.
 * @rx_xgmii_ctrl_err_cnt: Count of unexpected or misplaced control
 * characters occuring between times of normal data transmission for mac
 * the link.
 * @rx_xgmii_err_sym: Count of the number of symbol errors in the
 * received XGMII data for mac the link.
 * @rx_xgmii_char1_match: Count of the number of XGMII characters
 * that match first pattern defined in MAC_STATS_RX_XGMII_CHAR_LINK_N.
 * @rx_xgmii_char2_match: Count of the number of XGMII characters
 * that match second pattern defined in MAC_STATS_RX_XGMII_CHAR_LINK_N.
 * @rx_xgmii_column1_match: Count of the number of XGMII columns
 * that match a pattern defined in MAC_STATS_RX_XGMII_COLUMN1_LINK_N.
 * @rx_xgmii_column2_match: Count of the number of XGMII columns
 * that match a pattern defined in MAC_STATS_RX_XGMII_COLUMN1_LINK_N.
 * @rx_local_fault: Count of the number of local faults for mac the link.
 * @rx_remote_fault: Count of the number of remote faults for mac the
 * link.
 * @rx_queue_full: Count of the number of frame destined for a full
 * queue for mac the link.
 */
typedef struct xge_hal_stats_link_info_t {
	u64 tx_frms;
	u64 tx_ttl_eth_octets;
	u64 tx_data_octets;
	u64 tx_mcst_frms;
	u64 tx_bcst_frms;
	u64 tx_ucst_frms;
	u64 tx_tagged_frms;
	u64 tx_vld_ip;
	u64 tx_vld_ip_octets;
	u64 tx_icmp;
	u64 tx_tcp;
	u64 tx_rst_tcp;
	u64 tx_udp;
	u64 tx_unknown_protocol;
	u64 tx_parse_error;
	u64 tx_pause_ctrl_frms;
	u64 tx_lacpdu_frms;
	u64 tx_marker_pdu_frms;
	u64 tx_marker_resp_pdu_frms;
	u64 tx_drop_ip;
	u64 tx_xgmii_char1_match;
	u64 tx_xgmii_char2_match;
	u64 tx_xgmii_column1_match;
	u64 tx_xgmii_column2_match;
	u64 tx_drop_frms;
	u64 tx_any_err_frms;
	u64 rx_ttl_frms;
	u64 rx_vld_frms;
	u64 rx_offld_frms;
	u64 rx_ttl_eth_octets;
	u64 rx_data_octets;
	u64 rx_offld_octets;
	u64 rx_vld_mcst_frms;
	u64 rx_vld_bcst_frms;
	u64 rx_accepted_ucst_frms;
	u64 rx_accepted_nucst_frms;
	u64 rx_tagged_frms;
	u64 rx_long_frms;
	u64 rx_usized_frms;
	u64 rx_osized_frms;
	u64 rx_frag_frms;
	u64 rx_jabber_frms;
	u64 rx_ttl_64_frms;
	u64 rx_ttl_65_127_frms;
	u64 rx_ttl_128_255_frms;
	u64 rx_ttl_256_511_frms;
	u64 rx_ttl_512_1023_frms;
	u64 rx_ttl_1024_1518_frms;
	u64 rx_ttl_1519_4095_frms;
	u64 rx_ttl_40956_8191_frms;
	u64 rx_ttl_8192_max_frms;
	u64 rx_ttl_gt_max_frms;
	u64 rx_ip;
	u64 rx_ip_octets;
	u64 rx_hdr_err_ip;
	u64 rx_icmp;
	u64 rx_tcp;
	u64 rx_udp;
	u64 rx_err_tcp;
	u64 rx_pause_cnt;
	u64 rx_pause_ctrl_frms;
	u64 rx_unsup_ctrl_frms;
	u64 rx_in_rng_len_err_frms;
	u64 rx_out_rng_len_err_frms;
	u64 rx_drop_frms;
	u64 rx_discarded_frms;
	u64 rx_drop_ip;
	u64 rx_err_drp_udp;
	u64 rx_lacpdu_frms;
	u64 rx_marker_pdu_frms;
	u64 rx_marker_resp_pdu_frms;
	u64 rx_unknown_pdu_frms;
	u64 rx_illegal_pdu_frms;
	u64 rx_fcs_discard;
	u64 rx_len_discard;
	u64 rx_pf_discard;
	u64 rx_trash_discard;
	u64 rx_rts_discard;
	u64 rx_wol_discard;
	u64 rx_red_discard;
	u64 rx_ingm_full_discard;
	u64 rx_xgmii_data_err_cnt;
	u64 rx_xgmii_ctrl_err_cnt;
	u64 rx_xgmii_err_sym;
	u64 rx_xgmii_char1_match;
	u64 rx_xgmii_char2_match;
	u64 rx_xgmii_column1_match;
	u64 rx_xgmii_column2_match;
	u64 rx_local_fault;
	u64 rx_remote_fault;
	u64 rx_queue_full;
}xge_hal_stats_link_info_t;

/**
 * struct xge_hal_stats_aggr_info_t - XGMAC statistics for an aggregator
 *
 * @tx_frms: Count of data frames transmitted for the aggregator.
 * @tx_mcst_frms: Count of multicast data frames transmitted for
 * the aggregator.
 * @tx_bcst_frms: Count of broadcast data frames transmitted for
 * the aggregator.
 * @tx_discarded_frms: Count of discarded data frames transmitted for
 * the aggregator.
 * @tx_errored_frms: Count of errored data frames transmitted for
 * the aggregator.
 * @rx_frms: Count of received frames for aggregators
 * @rx_data_octets: Count of data and padding octets of frames received
 * the aggregator.
 * @rx_mcst_frms: Count of multicast frames received the aggregator.
 * @rx_bcst_frms: Count of broadast frames received the aggregator.
 * @rx_discarded_frms: Count of discarded frames received the aggregator.
 * @rx_errored_frms: Count of errored frames received the aggregator.
 * @rx_unknown_protocol_frms: Count of unknown protocol frames received
 * the aggregator.
*/
typedef struct xge_hal_stats_aggr_info_t {
	u64 tx_frms;
	u64 tx_mcst_frms;
	u64 tx_bcst_frms;
	u64 tx_discarded_frms;
	u64 tx_errored_frms;
	u64 rx_frms;
	u64 rx_data_octets;
	u64 rx_mcst_frms;
	u64 rx_bcst_frms;
	u64 rx_discarded_frms;
	u64 rx_errored_frms;
	u64 rx_unknown_protocol_frms;
}xge_hal_stats_aggr_info_t;

/**
 * struct xge_hal_stats_vpath_info_t - XGMAC statistics for a vpath.
 *
 * @tx_frms: Count of transmitted MAC frames for the vpath.
 * @tx_ttl_eth_octets: Count of total octets of transmitted frames
 * for the vpath.
 * @tx_data_octets: Count of data and padding octets of transmitted
 * frames for the vpath.
 * @tx_mcst_frms: Count of multicast MAC frames for the vpath.
 * @tx_bcst_frms: Count of broadcast MAC frames for the vpath.
 * @tx_ucst_frms: Count of unicast MAC frames for the vpath.
 * @tx_tagged_frms: Count of transmitted frames containing a VLAN
 * tag for the vpath.
 * @tx_vld_ip: Count of transmitted IP datagrams for the vpath.
 * @tx_vld_ip_octets: Count of transmitted IP octets for the vpath.
 * @tx_icmp: Count of transmitted ICMP messages for the vpath.
 * @tx_tcp: Count of transmitted TCP segments for the vpath.
 * @tx_rst_tcp: Count of transmitted TCP segments containing the RST
 * flag the vpath.
 * @tx_udp: Count of transmitted UDP datagrams for the vpath.
 * @tx_unknown_protocol: Count of transmitted packets of unknown
 * protocol for the vpath.
 * @tx_parse_error: Count of transmitted packets with parsing errors
 * for the vpath.
 * @rx_ttl_frms: Count of all received MAC frames for the vpath.
 * @rx_vld_frms: Count of all successfully received MAC frames for
 * the vpath.
 * @rx_offld_frms: Count of all offloaded received MAC frames for
 * the vpath.
 * @rx_ttl_eth_octets: Count of total octets of received frames, not
 * including framing characters for the vpath.
 * @rx_data_octets: Count of data and padding octets of successfully
 * received frames for the vpath.
 * @rx_offld_octets: Count of total octets, not including framing
 * characters, of offloaded received frames for the vpath.
 * @rx_vld_mcst_frms: Count of successfully received multicast MAC
 * frames for the vpath.
 * @rx_vld_bcst_frms: Count of successfully received broadcast MAC
 * frames for the vpath.
 * @rx_accepted_ucst_frms: Count of successfully received unicast
 * MAC frames for the vpath.
 * @rx_accepted_nucst_frms: Count of successfully received
 * non-unicast MAC frames for the vpath.
 * @rx_tagged_frms: Count of received frames containing a VLAN tag
 * for the vpath.
 * @rx_long_frms: Count of received frames that are longer than
 * RX_MAX_PYLD_LEN + 18 bytes (+ 22 bytes if VLAN-tagged) for the vpath.
 * @rx_usized_frms: Count of received frames of length less than 64
 * octets, for the vpath.
 * @rx_usized_frms: Count of received frames of length more than
 * 1518 octets, for the vpath.
 * @rx_osized_frms:  Count of received frames of length more than
 * 1518 octets for the vpath.
 * @rx_frag_frms:  Count of received frames of length less than 64
 * octets that had bad FCS, for the vpath.
 * @rx_jabber_frms:  Count of received frames of length more than
 * 1518 octets that had bad FCS, for the vpath.
 * @rx_ttl_64_frms: Count of all received MAC frames with length of
 * exactly 64 octets, for the vpath.
 * @rx_ttl_65_127_frms: Count of all received MAC frames with length
 * of between 65 and 127 octets inclusive, for the vpath.
 * @rx_ttl_128_255_frms: Count of all received MAC frames with
 * length of between 128 and 255 octets inclusive, for the vpath.
 * @rx_ttl_256_511_frms: Count of all received MAC frames with
 * length of between 246 and 511 octets inclusive, for the vpath.
 * @rx_ttl_512_1023_frms: Count of all received MAC frames with
 * length of between 512 and 1023 octets inclusive, for the vpath.
 * @rx_ttl_1024_1518_frms: Count of all received MAC frames with
 * length of between 1024 and 1518 octets inclusive, for the vpath.
 * @rx_ttl_1519_4095_frms: Count of all received MAC frames with
 * length of between 1519 and 4095 octets inclusive, for the vpath.
 * @rx_ttl_40956_8191_frms: Count of all received MAC frames with
 * of between 4096 and 8191 octets inclusive, for the vpath.
 * @rx_ttl_8192_max_frms: Count of all received MAC frames with
 * length of between 8192 and RX_MAX_PYLD_LEN+18 octets inclusive, for the
 * vpath.
 * @rx_ttl_gt_max_frms: Count of all received MAC frames with length
 * exceeding RX_MAX_PYLD_LEN+18 octets inclusive, for the vpath.
 * @rx_ip:  Count of received IP datagrams, for the vpath.
 * @rx_accepted_ip: Count of received  and accepted IP datagrams,
 * for the vpath.
 * @rx_ip_octets: Count of number of octets in received IP datagrams
 * for the vpath.
 * @rx_hdr_err_ip: Count of received IP datagrams that are discarded
 * due to IP header errors, for the vpath.
 * @rx_icmp: Count of received ICMP messages for the vpath.
 * @rx_tcp: Count of received TCP segments for the vpath.
 * @rx_udp: Count of received UDP datagrams for the vpath.
 * @rx_err_tcp: Count of received TCP segments containing errors for
 * the vpath.
 * @rx_mpa_ok_frms: Count of received frames that pass the MPA
 * checks for vptah.
 * @rx_mpa_crc_fail_frms: Count of received frames that fail the MPA
 * CRC check for the vpath.
 * @rx_mpa_mrk_fail_frms: Count of received frames that fail the
 * MPA marker check for the vpath.
 * @rx_mpa_len_fail_frms: Count of received frames that fail the MPA
 * length check for the vpath.
 * @rx_wol_frms: Count of received "magic packet" frames for
 * the vpath.
 */
typedef struct xge_hal_stats_vpath_info_t {
	u64 tx_frms;
	u64 tx_ttl_eth_octets;
	u64 tx_data_octets;
	u64 tx_mcst_frms;
	u64 tx_bcst_frms;
	u64 tx_ucst_frms;
	u64 tx_tagged_frms;
	u64 tx_vld_ip;
	u64 tx_vld_ip_octets;
	u64 tx_icmp;
	u64 tx_tcp;
	u64 tx_rst_tcp;
	u64 tx_udp;
	u64 tx_unknown_protocol;
	u64 tx_parse_error;
	u64 rx_ttl_frms;
	u64 rx_vld_frms;
	u64 rx_offld_frms;
	u64 rx_ttl_eth_octets;
	u64 rx_data_octets;
	u64 rx_offld_octets;
	u64 rx_vld_mcst_frms;
	u64 rx_vld_bcst_frms;
	u64 rx_accepted_ucst_frms;
	u64 rx_accepted_nucst_frms;
	u64 rx_tagged_frms;
	u64 rx_long_frms;
	u64 rx_usized_frms;
	u64 rx_osized_frms;
	u64 rx_frag_frms;
	u64 rx_jabber_frms;
	u64 rx_ttl_64_frms;
	u64 rx_ttl_65_127_frms;
	u64 rx_ttl_128_255_frms;
	u64 rx_ttl_256_511_frms;
	u64 rx_ttl_512_1023_frms;
	u64 rx_ttl_1024_1518_frms;
	u64 rx_ttl_1519_4095_frms;
	u64 rx_ttl_40956_8191_frms;
	u64 rx_ttl_8192_max_frms;
	u64 rx_ttl_gt_max_frms;
	u64 rx_ip;
	u64 rx_accepted_ip;
	u64 rx_ip_octets;
	u64 rx_hdr_err_ip;
	u64 rx_icmp;
	u64 rx_tcp;
	u64 rx_udp;
	u64 rx_err_tcp;
	u64 rx_mpa_ok_frms;
	u64 rx_mpa_crc_fail_frms;
	u64 rx_mpa_mrk_fail_frms;
	u64 rx_mpa_len_fail_frms;
	u64 rx_wol_frms;
}xge_hal_stats_vpath_info_t;

/**
 * struct xge_hal_stats_pcim_info_t - Contains PCIM statistics
 *
 * @link_info: PCIM links info for link 0, 1, and 2.
 * @aggr_info: PCIM aggregators info for aggregator 0 and 1.
 * See also: xge_hal_stats_link_info_t{}, xge_hal_stats_aggr_info_t{}.
 */
typedef struct xge_hal_stats_pcim_info_t {
	xge_hal_stats_link_info_t   link_info[XGE_HAL_MAC_LINKS];
	xge_hal_stats_aggr_info_t   aggr_info[XGE_HAL_MAC_AGGREGATORS];
}xge_hal_stats_pcim_info_t;

/**
 * struct xge_hal_stats_t - Contains HAL per-device statistics,
 * including hw.
 * @devh: HAL device handle.
 * @dma_addr: DMA addres of the %hw_info. Given to device to fill-in the stats.
 * @hw_info_dmah: DMA handle used to map hw statistics onto the device memory
 *                space.
 * @hw_info_dma_acch: One more DMA handle used subsequently to free the
 *                    DMA object. Note that this and the previous handle have
 *                    physical meaning for Solaris; on Windows and Linux the
 *                    corresponding value will be simply pointer to PCI device.
 *
 * @hw_info: Xframe statistics maintained by the hardware.
 * @hw_info_saved: TBD
 * @hw_info_latest: TBD
 * @pcim_info: Xframe PCIM statistics maintained by the hardware.
 * @pcim_info_saved: TBD
 * @pcim_info_latest: TBD
 * @sw_dev_info_stats: HAL's "soft" device informational statistics, e.g. number
 *                     of completions per interrupt.
 * @sw_dev_err_stats: HAL's "soft" device error statistics.
 *
 * @is_initialized: True, if all the subordinate structures are allocated and
 *                  initialized.
 * @is_enabled: True, if device stats collection is enabled.
 *
 * Structure-container of HAL per-device statistics. Note that per-channel
 * statistics are kept in separate structures under HAL's fifo and ring
 * channels.
 * See also: xge_hal_stats_hw_info_t{}, xge_hal_stats_sw_err_t{},
 * xge_hal_stats_device_info_t{}.
 * See also: xge_hal_stats_channel_info_t{}.
 */
typedef struct xge_hal_stats_t {
	    /* handles */
	xge_hal_device_h        devh;
	dma_addr_t          dma_addr;
	pci_dma_h           hw_info_dmah;
	pci_dma_acc_h           hw_info_dma_acch;

	    /* HAL device hardware statistics */
	xge_hal_stats_hw_info_t     *hw_info;
	xge_hal_stats_hw_info_t     hw_info_saved;
	xge_hal_stats_hw_info_t     hw_info_latest;

	/* HAL device hardware statistics for XFRAME ER */
	xge_hal_stats_pcim_info_t   *pcim_info;
	xge_hal_stats_pcim_info_t   *pcim_info_saved;
	xge_hal_stats_pcim_info_t   *pcim_info_latest;

	    /* HAL device "soft" stats */
	xge_hal_stats_sw_err_t          sw_dev_err_stats;
	xge_hal_stats_device_info_t     sw_dev_info_stats;

	    /* flags */
	int             is_initialized;
	int             is_enabled;
} xge_hal_stats_t;

/* ========================== STATS PRIVATE API ========================= */

xge_hal_status_e __hal_stats_initialize(xge_hal_stats_t *stats,
	        xge_hal_device_h devh);

void __hal_stats_terminate(xge_hal_stats_t *stats);

void __hal_stats_enable(xge_hal_stats_t *stats);

void __hal_stats_disable(xge_hal_stats_t *stats);

void __hal_stats_soft_reset(xge_hal_device_h devh, int reset_all);

/* ========================== STATS PUBLIC API ========================= */

xge_hal_status_e xge_hal_stats_hw(xge_hal_device_h devh,
	        xge_hal_stats_hw_info_t **hw_info);

xge_hal_status_e xge_hal_stats_pcim(xge_hal_device_h devh,
	        xge_hal_stats_pcim_info_t   **pcim_info);

xge_hal_status_e xge_hal_stats_device(xge_hal_device_h devh,
	        xge_hal_stats_device_info_t **device_info);

xge_hal_status_e xge_hal_stats_channel(xge_hal_channel_h channelh,
	        xge_hal_stats_channel_info_t **channel_info);

xge_hal_status_e xge_hal_stats_reset(xge_hal_device_h devh);


__EXTERN_END_DECLS

#endif /* XGE_HAL_STATS_H */
