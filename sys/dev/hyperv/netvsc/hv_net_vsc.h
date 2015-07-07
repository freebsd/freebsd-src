/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2012 NetApp Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * HyperV vmbus (virtual machine bus) network VSC (virtual services client)
 * header file
 *
 * (Updated from unencumbered NvspProtocol.h)
 */

#ifndef __HV_NET_VSC_H__
#define __HV_NET_VSC_H__

#include <sys/types.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/sx.h>

#include <dev/hyperv/include/hyperv.h>

MALLOC_DECLARE(M_NETVSC);

#define NVSP_INVALID_PROTOCOL_VERSION           (0xFFFFFFFF)

#define NVSP_PROTOCOL_VERSION_1                 2
#define NVSP_PROTOCOL_VERSION_2                 0x30002
#define NVSP_PROTOCOL_VERSION_4                 0x40000
#define NVSP_PROTOCOL_VERSION_5                 0x50000
#define NVSP_MIN_PROTOCOL_VERSION               (NVSP_PROTOCOL_VERSION_1)
#define NVSP_MAX_PROTOCOL_VERSION               (NVSP_PROTOCOL_VERSION_2)

#define NVSP_PROTOCOL_VERSION_CURRENT           NVSP_PROTOCOL_VERSION_2

#define VERSION_4_OFFLOAD_SIZE                  22

#define NVSP_OPERATIONAL_STATUS_OK              (0x00000000)
#define NVSP_OPERATIONAL_STATUS_DEGRADED        (0x00000001)
#define NVSP_OPERATIONAL_STATUS_NONRECOVERABLE  (0x00000002)
#define NVSP_OPERATIONAL_STATUS_NO_CONTACT      (0x00000003)
#define NVSP_OPERATIONAL_STATUS_LOST_COMMUNICATION (0x00000004)

/*
 * Maximun number of transfer pages (packets) the VSP will use on a receive
 */
#define NVSP_MAX_PACKETS_PER_RECEIVE            375


typedef enum nvsp_msg_type_ {
	nvsp_msg_type_none                      = 0,

	/*
	 * Init Messages
	 */
	nvsp_msg_type_init                      = 1,
	nvsp_msg_type_init_complete             = 2,

	nvsp_version_msg_start                  = 100,

	/*
	 * Version 1 Messages
	 */
	nvsp_msg_1_type_send_ndis_vers          = nvsp_version_msg_start,

	nvsp_msg_1_type_send_rx_buf,
	nvsp_msg_1_type_send_rx_buf_complete,
	nvsp_msg_1_type_revoke_rx_buf,

	nvsp_msg_1_type_send_send_buf,
	nvsp_msg_1_type_send_send_buf_complete,
	nvsp_msg_1_type_revoke_send_buf,

	nvsp_msg_1_type_send_rndis_pkt,
	nvsp_msg_1_type_send_rndis_pkt_complete,
    
	/*
	 * Version 2 Messages
	 */
	nvsp_msg_2_type_send_chimney_delegated_buf,
	nvsp_msg_2_type_send_chimney_delegated_buf_complete,
	nvsp_msg_2_type_revoke_chimney_delegated_buf,

	nvsp_msg_2_type_resume_chimney_rx_indication,

	nvsp_msg_2_type_terminate_chimney,
	nvsp_msg_2_type_terminate_chimney_complete,

	nvsp_msg_2_type_indicate_chimney_event,

	nvsp_msg_2_type_send_chimney_packet,
	nvsp_msg_2_type_send_chimney_packet_complete,

	nvsp_msg_2_type_post_chimney_rx_request,
	nvsp_msg_2_type_post_chimney_rx_request_complete,

	nvsp_msg_2_type_alloc_rx_buf,
	nvsp_msg_2_type_alloc_rx_buf_complete,

	nvsp_msg_2_type_free_rx_buf,

	nvsp_msg_2_send_vmq_rndis_pkt,
	nvsp_msg_2_send_vmq_rndis_pkt_complete,

	nvsp_msg_2_type_send_ndis_config,

	nvsp_msg_2_type_alloc_chimney_handle,
	nvsp_msg_2_type_alloc_chimney_handle_complete,
} nvsp_msg_type;

typedef enum nvsp_status_ {
	nvsp_status_none = 0,
	nvsp_status_success,
	nvsp_status_failure,
	/* Deprecated */
	nvsp_status_prot_vers_range_too_new,
	/* Deprecated */
	nvsp_status_prot_vers_range_too_old,
	nvsp_status_invalid_rndis_pkt,
	nvsp_status_busy,
	nvsp_status_max,
} nvsp_status;

typedef struct nvsp_msg_hdr_ {
	uint32_t                                msg_type;
} __packed nvsp_msg_hdr;

/*
 * Init Messages
 */

/*
 * This message is used by the VSC to initialize the channel
 * after the channels has been opened. This message should 
 * never include anything other then versioning (i.e. this
 * message will be the same for ever).
 *
 * Forever is a long time.  The values have been redefined
 * in Win7 to indicate major and minor protocol version
 * number.
 */
typedef struct nvsp_msg_init_ {
	union {
		struct {
			uint16_t                minor_protocol_version;
			uint16_t                major_protocol_version;
		} s;
		/* Formerly min_protocol_version */
		uint32_t                        protocol_version;
	} p1;
	/* Formerly max_protocol_version */
	uint32_t                                protocol_version_2;
} __packed nvsp_msg_init;

/*
 * This message is used by the VSP to complete the initialization
 * of the channel. This message should never include anything other 
 * then versioning (i.e. this message will be the same forever).
 */
typedef struct nvsp_msg_init_complete_ {
	/* Deprecated */
	uint32_t                                negotiated_prot_vers;
	uint32_t                                max_mdl_chain_len;
	uint32_t                                status;
} __packed nvsp_msg_init_complete;

typedef union nvsp_msg_init_uber_ {
	nvsp_msg_init                           init;
	nvsp_msg_init_complete                  init_compl;
} __packed nvsp_msg_init_uber;

/*
 * Version 1 Messages
 */

/*
 * This message is used by the VSC to send the NDIS version
 * to the VSP.  The VSP can use this information when handling
 * OIDs sent by the VSC.
 */
typedef struct nvsp_1_msg_send_ndis_version_ {
	uint32_t                                ndis_major_vers;
	/* Deprecated */
	uint32_t                                ndis_minor_vers;
} __packed nvsp_1_msg_send_ndis_version;

/*
 * This message is used by the VSC to send a receive buffer
 * to the VSP. The VSP can then use the receive buffer to
 * send data to the VSC.
 */
typedef struct nvsp_1_msg_send_rx_buf_ {
	uint32_t                                gpadl_handle;
	uint16_t                                id;
} __packed nvsp_1_msg_send_rx_buf;

typedef struct nvsp_1_rx_buf_section_ {
	uint32_t                                offset;
	uint32_t                                sub_allocation_size;
	uint32_t                                num_sub_allocations;
	uint32_t                                end_offset;
} __packed nvsp_1_rx_buf_section;

/*
 * This message is used by the VSP to acknowledge a receive 
 * buffer send by the VSC.  This message must be sent by the 
 * VSP before the VSP uses the receive buffer.
 */
typedef struct nvsp_1_msg_send_rx_buf_complete_ {
	uint32_t                                status;
	uint32_t                                num_sections;

	/*
	 * The receive buffer is split into two parts, a large
	 * suballocation section and a small suballocation
	 * section. These sections are then suballocated by a 
	 * certain size.
	 *
	 * For example, the following break up of the receive
	 * buffer has 6 large suballocations and 10 small
	 * suballocations.
	 *
	 * |            Large Section          |  |   Small Section   |
	 * ------------------------------------------------------------
	 * |     |     |     |     |     |     |  | | | | | | | | | | |
	 * |                                      |  
	 * LargeOffset                            SmallOffset
	 */
	nvsp_1_rx_buf_section                   sections[1];

} __packed nvsp_1_msg_send_rx_buf_complete;

/*
 * This message is sent by the VSC to revoke the receive buffer.
 * After the VSP completes this transaction, the VSP should never
 * use the receive buffer again.
 */
typedef struct nvsp_1_msg_revoke_rx_buf_ {
	uint16_t                                id;
} __packed nvsp_1_msg_revoke_rx_buf;

/*
 * This message is used by the VSC to send a send buffer
 * to the VSP. The VSC can then use the send buffer to
 * send data to the VSP.
 */
typedef struct nvsp_1_msg_send_send_buf_ {
	uint32_t                                gpadl_handle;
	uint16_t                                id;
} __packed nvsp_1_msg_send_send_buf;

/*
 * This message is used by the VSP to acknowledge a send 
 * buffer sent by the VSC. This message must be sent by the 
 * VSP before the VSP uses the sent buffer.
 */
typedef struct nvsp_1_msg_send_send_buf_complete_ {
	uint32_t                                status;

	/*
	 * The VSC gets to choose the size of the send buffer and
	 * the VSP gets to choose the sections size of the buffer.
	 * This was done to enable dynamic reconfigurations when
	 * the cost of GPA-direct buffers decreases.
	 */
	uint32_t                                section_size;
} __packed nvsp_1_msg_send_send_buf_complete;

/*
 * This message is sent by the VSC to revoke the send buffer.
 * After the VSP completes this transaction, the vsp should never
 * use the send buffer again.
 */
typedef struct nvsp_1_msg_revoke_send_buf_ {
	uint16_t                                id;
} __packed nvsp_1_msg_revoke_send_buf;

/*
 * This message is used by both the VSP and the VSC to send
 * an RNDIS message to the opposite channel endpoint.
 */
typedef struct nvsp_1_msg_send_rndis_pkt_ {
	/*
	 * This field is specified by RNIDS.  They assume there's
	 * two different channels of communication. However, 
	 * the Network VSP only has one.  Therefore, the channel
	 * travels with the RNDIS packet.
	 */
	uint32_t                                chan_type;

	/*
	 * This field is used to send part or all of the data
	 * through a send buffer. This values specifies an 
	 * index into the send buffer.  If the index is 
	 * 0xFFFFFFFF, then the send buffer is not being used
	 * and all of the data was sent through other VMBus
	 * mechanisms.
	 */
	uint32_t                                send_buf_section_idx;
	uint32_t                                send_buf_section_size;
} __packed nvsp_1_msg_send_rndis_pkt;

/*
 * This message is used by both the VSP and the VSC to complete
 * a RNDIS message to the opposite channel endpoint.  At this
 * point, the initiator of this message cannot use any resources
 * associated with the original RNDIS packet.
 */
typedef struct nvsp_1_msg_send_rndis_pkt_complete_ {
	uint32_t                                status;
} __packed nvsp_1_msg_send_rndis_pkt_complete;


/*
 * Version 2 Messages
 */

/*
 * This message is used by the VSC to send the NDIS version
 * to the VSP.  The VSP can use this information when handling
 * OIDs sent by the VSC.
 */
typedef struct nvsp_2_netvsc_capabilities_ {
	union {
		uint64_t                        as_uint64;
		struct {
			uint64_t                vmq           : 1;
			uint64_t                chimney       : 1;
			uint64_t                sriov         : 1;
			uint64_t                ieee8021q     : 1;
			uint64_t                correlationid : 1;
			uint64_t                teaming       : 1;
		} u2;
	} u1;
} __packed nvsp_2_netvsc_capabilities;

typedef struct nvsp_2_msg_send_ndis_config_ {
	uint32_t                                mtu;
	uint32_t                                reserved;
	nvsp_2_netvsc_capabilities              capabilities;
} __packed nvsp_2_msg_send_ndis_config;

/*
 * NvspMessage2TypeSendChimneyDelegatedBuffer
 */
typedef struct nvsp_2_msg_send_chimney_buf_
{
	/*
	 * On WIN7 beta, delegated_obj_max_size is defined as a uint32_t
	 * Since WIN7 RC, it was split into two uint16_t.  To have the same
	 * struct layout, delegated_obj_max_size shall be the first field.
	 */
	uint16_t                                delegated_obj_max_size;

	/*
	 * The revision # of chimney protocol used between NVSC and NVSP.
	 *
	 * This revision is NOT related to the chimney revision between
	 * NDIS protocol and miniport drivers.
	 */
	uint16_t                                revision;

	uint32_t                                gpadl_handle;
} __packed nvsp_2_msg_send_chimney_buf;


/* Unsupported chimney revision 0 (only present in WIN7 beta) */
#define NVSP_CHIMNEY_REVISION_0                 0

/* WIN7 Beta Chimney QFE */
#define NVSP_CHIMNEY_REVISION_1                 1

/* The chimney revision since WIN7 RC */
#define NVSP_CHIMNEY_REVISION_2                 2


/*
 * NvspMessage2TypeSendChimneyDelegatedBufferComplete
 */
typedef struct nvsp_2_msg_send_chimney_buf_complete_ {
	uint32_t                                status;

	/*
	 * Maximum number outstanding sends and pre-posted receives.
	 *
	 * NVSC should not post more than SendQuota/ReceiveQuota packets.
	 * Otherwise, it can block the non-chimney path for an indefinite
	 * amount of time.
	 * (since chimney sends/receives are affected by the remote peer).
	 *
	 * Note: NVSP enforces the quota restrictions on a per-VMBCHANNEL
	 * basis.  It doesn't enforce the restriction separately for chimney
	 * send/receive.  If NVSC doesn't voluntarily enforce "SendQuota",
	 * it may kill its own network connectivity.
	 */
	uint32_t                                send_quota;
	uint32_t                                rx_quota;
} __packed nvsp_2_msg_send_chimney_buf_complete;

/*
 * NvspMessage2TypeRevokeChimneyDelegatedBuffer
 */
typedef struct nvsp_2_msg_revoke_chimney_buf_ {
	uint32_t                                gpadl_handle;
} __packed nvsp_2_msg_revoke_chimney_buf;


#define NVSP_CHIMNEY_OBJECT_TYPE_NEIGHBOR       0
#define NVSP_CHIMNEY_OBJECT_TYPE_PATH4          1
#define NVSP_CHIMNEY_OBJECT_TYPE_PATH6          2
#define NVSP_CHIMNEY_OBJECT_TYPE_TCP            3

/*
 * NvspMessage2TypeAllocateChimneyHandle
 */
typedef struct nvsp_2_msg_alloc_chimney_handle_ {
	uint64_t                                vsc_context;
	uint32_t                                object_type;
} __packed nvsp_2_msg_alloc_chimney_handle;

/*
 * NvspMessage2TypeAllocateChimneyHandleComplete
 */
typedef struct nvsp_2_msg_alloc_chimney_handle_complete_ {
	uint32_t                                vsp_handle;
} __packed nvsp_2_msg_alloc_chimney_handle_complete;


/*
 * NvspMessage2TypeResumeChimneyRXIndication
 */
typedef struct nvsp_2_msg_resume_chimney_rx_indication {
	/*
	 * Handle identifying the offloaded connection
	 */
	uint32_t                                vsp_tcp_handle;
} __packed nvsp_2_msg_resume_chimney_rx_indication;


#define NVSP_2_MSG_TERMINATE_CHIMNEY_FLAGS_FIRST_STAGE      (0x01u)
#define NVSP_2_MSG_TERMINATE_CHIMNEY_FLAGS_RESERVED         (~(0x01u))

/*
 * NvspMessage2TypeTerminateChimney
 */
typedef struct nvsp_2_msg_terminate_chimney_ {
	/*
	* Handle identifying the offloaded object
	*/
	uint32_t                                vsp_handle;

	/*
	 * Terminate Offload Flags
	 *     Bit 0:
	 *         When set to 0, terminate the offload at the destination NIC
	 *     Bit 1-31:  Reserved, shall be zero
	 */
	uint32_t                                flags;

	union {
		/*
		 * This field is valid only when bit 0 of flags is clear.
		 * It specifies the index into the premapped delegated
		 * object buffer.  The buffer was sent through the
		 * NvspMessage2TypeSendChimneyDelegatedBuffer
		 * message at initialization time.
		 *
		 * NVSP will write the delegated state into the delegated
		 * buffer upon upload completion.
		 */
		uint32_t                        index;

		/*
		 * This field is valid only when bit 0 of flags is set.
		 *
		 * The seqence number of the most recently accepted RX
		 * indication when VSC sets its TCP context into
		 * "terminating" state.
		 *
		 * This allows NVSP to determines if there are any in-flight
		 * RX indications for which the acceptance state is still
		 * undefined.
		 */
		uint64_t                        last_accepted_rx_seq_no;
	} f0;
} __packed nvsp_2_msg_terminate_chimney;


#define NVSP_TERMINATE_CHIMNEY_COMPLETE_FLAG_DATA_CORRUPTED     0x0000001u

/*
 * NvspMessage2TypeTerminateChimneyComplete
 */
typedef struct nvsp_2_msg_terminate_chimney_complete_ {
	uint64_t                                vsc_context;
	uint32_t                                flags;
} __packed nvsp_2_msg_terminate_chimney_complete;

/*
 * NvspMessage2TypeIndicateChimneyEvent
 */
typedef struct nvsp_2_msg_indicate_chimney_event_ {
	/*
	 * When VscTcpContext is 0, event_type is an NDIS_STATUS event code
	 * Otherwise, EventType is an TCP connection event (defined in
	 * NdisTcpOffloadEventHandler chimney DDK document).
	 */
	uint32_t                                event_type;

	/*
	 * When VscTcpContext is 0, EventType is an NDIS_STATUS event code
	 * Otherwise, EventType is an TCP connection event specific information
	 * (defined in NdisTcpOffloadEventHandler chimney DDK document).
	 */
	uint32_t                                event_specific_info;

	/*
	 * If not 0, the event is per-TCP connection event.  This field
	 * contains the VSC's TCP context.
	 * If 0, the event indication is global.
	 */
	uint64_t                                vsc_tcp_context;
} __packed nvsp_2_msg_indicate_chimney_event;


#define NVSP_1_CHIMNEY_SEND_INVALID_OOB_INDEX       0xffffu
#define NVSP_1_CHIMNEY_SEND_INVALID_SECTION_INDEX   0xffffffff

/*
 * NvspMessage2TypeSendChimneyPacket
 */
typedef struct nvsp_2_msg_send_chimney_pkt_ {
    /*
     * Identify the TCP connection for which this chimney send is
     */
    uint32_t                                    vsp_tcp_handle;

    /*
     * This field is used to send part or all of the data
     * through a send buffer. This values specifies an
     * index into the send buffer. If the index is
     * 0xFFFF, then the send buffer is not being used
     * and all of the data was sent through other VMBus
     * mechanisms.
     */
    uint16_t                                    send_buf_section_index;
    uint16_t                                    send_buf_section_size;

    /*
     * OOB Data Index
     * This an index to the OOB data buffer. If the index is 0xFFFFFFFF,
     * then there is no OOB data.
     *
     * This field shall be always 0xFFFFFFFF for now. It is reserved for
     * the future.
     */
    uint16_t                                    oob_data_index;

    /*
     * DisconnectFlags = 0
     *      Normal chimney send. See MiniportTcpOffloadSend for details.
     *
     * DisconnectFlags = TCP_DISCONNECT_GRACEFUL_CLOSE (0x01)
     *      Graceful disconnect. See MiniportTcpOffloadDisconnect for details.
     *
     * DisconnectFlags = TCP_DISCONNECT_ABORTIVE_CLOSE (0x02)
     *      Abortive disconnect. See MiniportTcpOffloadDisconnect for details.
     */
    uint16_t                                    disconnect_flags;

    uint32_t                                    seq_no;
} __packed nvsp_2_msg_send_chimney_pkt;

/*
 * NvspMessage2TypeSendChimneyPacketComplete
 */
typedef struct nvsp_2_msg_send_chimney_pkt_complete_ {
    /*
     * The NDIS_STATUS for the chimney send
     */
    uint32_t                                    status;

    /*
     * Number of bytes that have been sent to the peer (and ACKed by the peer).
     */
    uint32_t                                    bytes_transferred;
} __packed nvsp_2_msg_send_chimney_pkt_complete;


#define NVSP_1_CHIMNEY_RECV_FLAG_NO_PUSH        0x0001u
#define NVSP_1_CHIMNEY_RECV_INVALID_OOB_INDEX   0xffffu

/*
 * NvspMessage2TypePostChimneyRecvRequest
 */
typedef struct nvsp_2_msg_post_chimney_rx_request_ {
	/*
	 * Identify the TCP connection which this chimney receive request
	 * is for.
	 */
	uint32_t                                vsp_tcp_handle;

	/*
	 * OOB Data Index
	 * This an index to the OOB data buffer. If the index is 0xFFFFFFFF,
	 * then there is no OOB data.
	 *
	 * This field shall be always 0xFFFFFFFF for now. It is reserved for
	 * the future.
	 */
	uint32_t                                oob_data_index;

	/*
	 * Bit 0
	 *      When it is set, this is a "no-push" receive.
	 *      When it is clear, this is a "push" receive.
	 *
	 * Bit 1-15:  Reserved and shall be zero
	 */
	uint16_t                                flags;

	/*
	 * For debugging and diagnoses purpose.
	 * The SeqNo is per TCP connection and starts from 0.
	 */
	uint32_t                                seq_no;
} __packed nvsp_2_msg_post_chimney_rx_request;

/*
 * NvspMessage2TypePostChimneyRecvRequestComplete
 */
typedef struct nvsp_2_msg_post_chimney_rx_request_complete_ {
	/*
	 * The NDIS_STATUS for the chimney send
	 */
	uint32_t                                status;

	/*
	 * Number of bytes that have been sent to the peer (and ACKed by
	 * the peer).
	 */
	uint32_t                                bytes_xferred;
} __packed nvsp_2_msg_post_chimney_rx_request_complete;

/*
 * NvspMessage2TypeAllocateReceiveBuffer
 */
typedef struct nvsp_2_msg_alloc_rx_buf_ {
	/*
	 * Allocation ID to match the allocation request and response
	 */
	uint32_t                                allocation_id;

	/*
	 * Length of the VM shared memory receive buffer that needs to
	 * be allocated
	 */
	uint32_t                                length;
} __packed nvsp_2_msg_alloc_rx_buf;

/*
 * NvspMessage2TypeAllocateReceiveBufferComplete
 */
typedef struct nvsp_2_msg_alloc_rx_buf_complete_ {
	/*
	 * The NDIS_STATUS code for buffer allocation
	 */
	uint32_t                                status;

	/*
	 * Allocation ID from NVSP_2_MESSAGE_ALLOCATE_RECEIVE_BUFFER
	 */
	uint32_t                                allocation_id;

	/*
	 * GPADL handle for the allocated receive buffer
	 */
	uint32_t                                gpadl_handle;

	/*
	 * Receive buffer ID that is further used in
	 * NvspMessage2SendVmqRndisPacket
	 */
	uint64_t                                rx_buf_id;
} __packed nvsp_2_msg_alloc_rx_buf_complete;

/*
 * NvspMessage2TypeFreeReceiveBuffer
 */
typedef struct nvsp_2_msg_free_rx_buf_ {
	/*
	 * Receive buffer ID previous returned in
	 * NvspMessage2TypeAllocateReceiveBufferComplete message
	 */
	uint64_t                                rx_buf_id;
} __packed nvsp_2_msg_free_rx_buf;

/*
 * This structure is used in defining the buffers in
 * NVSP_2_MESSAGE_SEND_VMQ_RNDIS_PACKET structure
 */
typedef struct nvsp_xfer_page_range_ {
	/*
	 * Specifies the ID of the receive buffer that has the buffer. This
	 * ID can be the general receive buffer ID specified in
	 * NvspMessage1TypeSendReceiveBuffer or it can be the shared memory
	 * receive buffer ID allocated by the VSC and specified in
	 * NvspMessage2TypeAllocateReceiveBufferComplete message
	 */
	uint64_t                                xfer_page_set_id;

	/*
	 * Number of bytes
	 */
	uint32_t                                byte_count;

	/*
	 * Offset in bytes from the beginning of the buffer
	 */
	uint32_t                                byte_offset;
} __packed nvsp_xfer_page_range;

/*
 * NvspMessage2SendVmqRndisPacket
 */
typedef struct nvsp_2_msg_send_vmq_rndis_pkt_ {
	/*
	 * This field is specified by RNIDS. They assume there's
	 * two different channels of communication. However,
	 * the Network VSP only has one. Therefore, the channel
	 * travels with the RNDIS packet. It must be RMC_DATA
	 */
	uint32_t                                channel_type;

	/*
	 * Only the Range element corresponding to the RNDIS header of
	 * the first RNDIS message in the multiple RNDIS messages sent
	 * in one NVSP message.  Information about the data portions as well
	 * as the subsequent RNDIS messages in the same NVSP message are
	 * embedded in the RNDIS header itself
	 */
	nvsp_xfer_page_range                    range;
} __packed nvsp_2_msg_send_vmq_rndis_pkt;

/*
 * This message is used by the VSC to complete
 * a RNDIS VMQ message to the VSP.  At this point,
 * the initiator of this message can use any resources
 * associated with the original RNDIS VMQ packet.
 */
typedef struct nvsp_2_msg_send_vmq_rndis_pkt_complete_
{
	uint32_t                                status;
} __packed nvsp_2_msg_send_vmq_rndis_pkt_complete;


typedef union nvsp_1_msg_uber_ {
	nvsp_1_msg_send_ndis_version            send_ndis_vers;

	nvsp_1_msg_send_rx_buf                  send_rx_buf;
	nvsp_1_msg_send_rx_buf_complete         send_rx_buf_complete;
	nvsp_1_msg_revoke_rx_buf                revoke_rx_buf;

	nvsp_1_msg_send_send_buf                send_send_buf;
	nvsp_1_msg_send_send_buf_complete       send_send_buf_complete;
	nvsp_1_msg_revoke_send_buf              revoke_send_buf;

	nvsp_1_msg_send_rndis_pkt               send_rndis_pkt;
	nvsp_1_msg_send_rndis_pkt_complete      send_rndis_pkt_complete;
} __packed nvsp_1_msg_uber;


typedef union nvsp_2_msg_uber_ {
	nvsp_2_msg_send_ndis_config             send_ndis_config;

	nvsp_2_msg_send_chimney_buf             send_chimney_buf;
	nvsp_2_msg_send_chimney_buf_complete    send_chimney_buf_complete;
	nvsp_2_msg_revoke_chimney_buf           revoke_chimney_buf;

	nvsp_2_msg_resume_chimney_rx_indication resume_chimney_rx_indication;
	nvsp_2_msg_terminate_chimney            terminate_chimney;
	nvsp_2_msg_terminate_chimney_complete   terminate_chimney_complete;
	nvsp_2_msg_indicate_chimney_event       indicate_chimney_event;

	nvsp_2_msg_send_chimney_pkt             send_chimney_packet;
	nvsp_2_msg_send_chimney_pkt_complete    send_chimney_packet_complete;
	nvsp_2_msg_post_chimney_rx_request      post_chimney_rx_request;
	nvsp_2_msg_post_chimney_rx_request_complete
	                                       post_chimney_rx_request_complete;

	nvsp_2_msg_alloc_rx_buf                 alloc_rx_buffer;
	nvsp_2_msg_alloc_rx_buf_complete        alloc_rx_buffer_complete;
	nvsp_2_msg_free_rx_buf                  free_rx_buffer;

	nvsp_2_msg_send_vmq_rndis_pkt           send_vmq_rndis_pkt;
	nvsp_2_msg_send_vmq_rndis_pkt_complete  send_vmq_rndis_pkt_complete;
	nvsp_2_msg_alloc_chimney_handle         alloc_chimney_handle;
	nvsp_2_msg_alloc_chimney_handle_complete alloc_chimney_handle_complete;
} __packed nvsp_2_msg_uber;


typedef union nvsp_all_msgs_ {
	nvsp_msg_init_uber                      init_msgs;
	nvsp_1_msg_uber                         vers_1_msgs;
	nvsp_2_msg_uber                         vers_2_msgs;
} __packed nvsp_all_msgs;

/*
 * ALL Messages
 */
typedef struct nvsp_msg_ {
	nvsp_msg_hdr                            hdr; 
	nvsp_all_msgs                           msgs;
} __packed nvsp_msg;


/*
 * The following arguably belongs in a separate header file
 */

/*
 * Defines
 */

#define NETVSC_SEND_BUFFER_SIZE			(1024*1024*15)   /* 15M */
#define NETVSC_SEND_BUFFER_ID			0xface


#define NETVSC_RECEIVE_BUFFER_SIZE		(1024*1024*16) /* 16MB */

#define NETVSC_RECEIVE_BUFFER_ID		0xcafe

#define NETVSC_RECEIVE_SG_COUNT			1

/* Preallocated receive packets */
#define NETVSC_RECEIVE_PACKETLIST_COUNT		256

/*
 * Maximum MTU we permit to be configured for a netvsc interface.
 * When the code was developed, a max MTU of 12232 was tested and
 * proven to work.  9K is a reasonable maximum for an Ethernet.
 */
#define NETVSC_MAX_CONFIGURABLE_MTU		(9 * 1024)

#define NETVSC_PACKET_SIZE			PAGE_SIZE

/*
 * Data types
 */

/*
 * Per netvsc channel-specific
 */
typedef struct netvsc_dev_ {
	struct hv_device			*dev;
	int					num_outstanding_sends;

	/* Send buffer allocated by us but manages by NetVSP */
	void					*send_buf;
	uint32_t				send_buf_size;
	uint32_t				send_buf_gpadl_handle;
	uint32_t				send_section_size;
	uint32_t				send_section_count;
	unsigned long				bitsmap_words;
	unsigned long				*send_section_bitsmap;

	/* Receive buffer allocated by us but managed by NetVSP */
	void					*rx_buf;
	uint32_t				rx_buf_size;
	uint32_t				rx_buf_gpadl_handle;
	uint32_t				rx_section_count;
	nvsp_1_rx_buf_section			*rx_sections;

	/* Used for NetVSP initialization protocol */
	struct sema				channel_init_sema;
	nvsp_msg				channel_init_packet;

	nvsp_msg				revoke_packet;
	/*uint8_t				hw_mac_addr[HW_MACADDR_LEN];*/

	/* Holds rndis device info */
	void					*extension;

	hv_bool_uint8_t				destroy;
	/* Negotiated NVSP version */
	uint32_t				nvsp_version;
	
	uint8_t					callback_buf[NETVSC_PACKET_SIZE]; 
} netvsc_dev;


typedef void (*pfn_on_send_rx_completion)(void *);

#define NETVSC_DEVICE_RING_BUFFER_SIZE	(128 * PAGE_SIZE)
#define NETVSC_PACKET_MAXPAGE		32 


#define NETVSC_VLAN_PRIO_MASK		0xe000
#define NETVSC_VLAN_PRIO_SHIFT		13
#define NETVSC_VLAN_VID_MASK		0x0fff

#define TYPE_IPV4			2
#define TYPE_IPV6			4
#define TYPE_TCP			2
#define TYPE_UDP			4

#define TRANSPORT_TYPE_NOT_IP		0
#define TRANSPORT_TYPE_IPV4_TCP		((TYPE_IPV4 << 16) | TYPE_TCP)
#define TRANSPORT_TYPE_IPV4_UDP		((TYPE_IPV4 << 16) | TYPE_UDP)
#define TRANSPORT_TYPE_IPV6_TCP		((TYPE_IPV6 << 16) | TYPE_TCP)
#define TRANSPORT_TYPE_IPV6_UDP		((TYPE_IPV6 << 16) | TYPE_UDP)

#ifdef __LP64__
#define BITS_PER_LONG 64
#else
#define BITS_PER_LONG 32
#endif

typedef struct netvsc_packet_ {
	struct hv_device           *device;
	hv_bool_uint8_t            is_data_pkt;      /* One byte */
	uint16_t		   vlan_tci;
	uint32_t status;

	/* Completion */
	union {
		struct {
			uint64_t   rx_completion_tid;
			void	   *rx_completion_context;
			/* This is no longer used */
			pfn_on_send_rx_completion   on_rx_completion;
		} rx;
		struct {
			uint64_t    send_completion_tid;
			void	    *send_completion_context;
			/* Still used in netvsc and filter code */
			pfn_on_send_rx_completion   on_send_completion;
		} send;
	} compl;
	uint32_t	send_buf_section_idx;
	uint32_t	send_buf_section_size;

	void		*rndis_mesg;
	uint32_t	tot_data_buf_len;
	void		*data;
	uint32_t	page_buf_count;
	hv_vmbus_page_buffer	page_buffers[NETVSC_PACKET_MAXPAGE];
} netvsc_packet;

typedef struct {
	uint8_t		mac_addr[6];  /* Assumption unsigned long */
	hv_bool_uint8_t	link_state;
} netvsc_device_info;

/*
 * Device-specific softc structure
 */
typedef struct hn_softc {
	struct ifnet    *hn_ifp;
	struct arpcom   arpcom;
	device_t        hn_dev;
	uint8_t         hn_unit;
	int             hn_carrier;
	int             hn_if_flags;
	struct mtx      hn_lock;
	int             hn_initdone;
	/* See hv_netvsc_drv_freebsd.c for rules on how to use */
	int             temp_unusable;
	struct hv_device  *hn_dev_obj;
	netvsc_dev  	*net_dev;
} hn_softc_t;


/*
 * Externs
 */
extern int hv_promisc_mode;

void netvsc_linkstatus_callback(struct hv_device *device_obj, uint32_t status);
void netvsc_xmit_completion(void *context);
void hv_nv_on_receive_completion(struct hv_device *device,
    uint64_t tid, uint32_t status);
netvsc_dev *hv_nv_on_device_add(struct hv_device *device,
    void *additional_info);
int hv_nv_on_device_remove(struct hv_device *device,
    boolean_t destroy_channel);
int hv_nv_on_send(struct hv_device *device, netvsc_packet *pkt);
int hv_nv_get_next_send_section(netvsc_dev *net_dev);

#endif  /* __HV_NET_VSC_H__ */

