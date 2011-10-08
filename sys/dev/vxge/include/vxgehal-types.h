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

#ifndef	VXGE_HAL_TYPES_H
#define	VXGE_HAL_TYPES_H

__EXTERN_BEGIN_DECLS

/*
 * enum vxge_hal_reopen_e - Open, close, or reopen option.
 * @VXGE_HAL_RESET_ONLY: Do not (de)allocate
 * @VXGE_HAL_OPEN_NORMAL: Do (de)allocate
 *
 * Enumerates options used with ring, fifo, sq, srq, cqrq, dmq and umq
 * open and close operations. The @VXGE_HAL_RESET_ONLY can be used when
 * resetting the device; in this case there is actually no need to free
 * and then again malloc the memory (including DMA-able memory).
 */
typedef enum vxge_hal_reopen_e {
	VXGE_HAL_RESET_ONLY	= 1,
	VXGE_HAL_OPEN_NORMAL	= 2
} vxge_hal_reopen_e;

/*
 * struct vxge_hal_version_t - HAL version info
 * @version_major: Major version
 * @version_minor: Minor version
 * @version_fix: version fix
 * @version_build: Version Build
 *
 * Structure to store version info
 */
typedef struct vxge_hal_version_t {
	u32	version_major;
	u32	version_minor;
	u32	version_fix;
	u32	version_build;
} vxge_hal_version_t;

/*
 * VXGE_HAL_ETH_ALEN
 */
#define	VXGE_HAL_ETH_ALEN				6

/*
 * typedef macaddr_t - Ethernet address type
 */
typedef u8 macaddr_t[VXGE_HAL_ETH_ALEN];

/*
 * struct vxge_hal_ipv4 - IP version 4 address type
 * @addr: IP address
 */
typedef struct vxge_hal_ipv4 {
	u32	addr;
} vxge_hal_ipv4;

/*
 * struct vxge_hal_ipv6 - IP version 6 address type
 * @addr: IP address
 */
typedef struct vxge_hal_ipv6 {
	u64	addr[2];
} vxge_hal_ipv6;

/*
 * union vxge_hal_ipaddr_t - IP address type
 * @ipv4: IP V4 address
 * @ipv6: IP V6 address
 */
typedef union vxge_hal_ipaddr_t {
	vxge_hal_ipv4 ipv4;
	vxge_hal_ipv6 ipv6;
} vxge_hal_ipaddr_t;

/*
 * typedef vxge_hal_obj_id_t - Object Id type used for Session,
 *		  SRQ, CQRQ, STAG, LRO, SPDM etc objects
 */
typedef u64 vxge_hal_obj_id_t;

/* basic handles */

/*
 * typedef vxge_hal_device_h - Handle to the adapter object
 */
typedef void *vxge_hal_device_h;

/*
 * typedef vxge_hal_vpath_h - Handle to the virtual path object returned to LL
 */
typedef void *vxge_hal_vpath_h;

/*
 * typedef vxge_hal_client_h - Handle passed by client for client's private data
 */
typedef void *vxge_hal_client_h;

/*
 * typedef vxge_hal_ring_h - Handle to the ring object used for non offload
 *		receive
 */
typedef void *vxge_hal_ring_h;

/*
 * typedef vxge_hal_fifo_h - Handle to the fifo object used for non offload send
 */
typedef void *vxge_hal_fifo_h;


/*
 * typedef vxge_hal_txdl_h - Handle to the transmit desriptor list object used
 *		for nonoffload send
 */
typedef void *vxge_hal_txdl_h;

/*
 * typedef vxge_hal_rxd_h - Handle to the receive desriptor object used for
 *		nonoffload receive
 */
typedef void *vxge_hal_rxd_h;

/*
 * typedef vxge_hal_up_msg_h - Handle to the up message queue
 */
typedef void *vxge_hal_up_msg_h;

/*
 * typedef vxge_hal_down_msg_h - Handle to the down message queue
 */
typedef void *vxge_hal_down_msg_h;


/*
 * typedef vxge_hal_callback_h - Handle to callback function
 */
typedef void *vxge_hal_callback_h;

/*
 * enum vxge_hal_message_type_e - Enumerated message types.
 *
 * @VXGE_HAL_MSG_TYPE_NCE_CREATE_REQ: The NCE Create Request
 *		 message is used by the host to create an NCE on the adapter.
 * @VXGE_HAL_MSG_TYPE_NCE_CREATE_RESP:The NCE Create Response
 *		 message is sent in response to the NCE Create Request
 *		 message from the host to indicate the status of the operation
 *		 and return the NCE ID.
 * @VXGE_HAL_MSG_TYPE_NCE_DELETE_REQ:The NCE Delete Request
 *		 messag is sent by the host to delete an NCE after it is no
 *		 longer required.
 * @VXGE_HAL_MSG_TYPE_NCE_DELETE_RESP:The NCE Delete Response
 *		 message is sent in response to the NCE Delete Request
 *		 message from the host to indicate the status of the operation.
 * @VXGE_HAL_MSG_TYPE_NCE_UPDATE_MAC_REQ:The NCE Update MAC Request
 *		  message is used by the host to modify the MAC address for
 *		 an NCE on the adapter.
 * @VXGE_HAL_MSG_TYPE_NCE_UPDATE_MAC_RESP:The NCE Update MAC Response
 *		  message is sent in response to the NCE Update MAC Request
 *		  message from the host to indicate the status of the
 *		 operation.
 * @VXGE_HAL_MSG_TYPE_NCE_UPDATE_RCH_TIME_REQ:The NCE Update Rch Time
 *		 Request  message is used by the host to update the
 *		 Reachability time for an NCE on the adapter.
 * @VXGE_HAL_MSG_TYPE_NCE_UPDATE_RCH_TIME_RESP:The NCE Update
 *		 Rch Time Response  message is sent in response to the NCE
 *		 Update Rch Time Request  message from the host to indicate
 *		 the status of updating the reachability time for the NCE.
 * @VXGE_HAL_MSG_TYPE_NCE_QUERY_REQ:The NCE Query Request  message
 *		 is used by the host to query an NCE on the adapter.
 * @VXGE_HAL_MSG_TYPE_NCE_QUERY_RESP:The NCE Query Response
 *		 message is sent in response to the NCE Query Request  message
 *		 from the host to indicate the status of the operation.
 * @VXGE_HAL_MSG_TYPE_NCE_RCH_TIME_EXCEEDED:This is an unsolicited message
 *		 sent to the host by the adapter when the NCE Reach Time has
 *		 been exceeded.
 * @VXGE_HAL_MSG_TYPE_CQRQ_CREATE_REQ:The CQRQ Create Request
 *		 message is used by the host to create a CQRQ on the adapter.
 * @VXGE_HAL_MSG_TYPE_CQRQ_CREATE_RESP:The CQRQ Create Response
 *		 message is sent in response to the CQRQ Create Request
 *		 message from the host to indicate the status of the operation.
 * @VXGE_HAL_MSG_TYPE_CQRQ_DELETE_REQ:The CQRQ Delete Request
 *		 message is used by the host to destroy a CQRQ on the adapter.
 * @VXGE_HAL_MSG_TYPE_CQRQ_DELETE_RESP:The CQRQ Delete Response
 *		 message is sent in response to the CQRQ Delete Request
 *		 message from the host to indicate the status of the operation.
 * @VXGE_HAL_MSG_TYPE_CQRQ_MODIFY_REQ:The CQRQ Modify Request
 *		 message is used by the host to modify fields for an
 *		 CQRQ on the adapter. The adapter will make the following
 *		 checks
 *		 - The CQRQ ID is valid
 *		 All other checks must be performed by the host software.
 * @VXGE_HAL_MSG_TYPE_CQRQ_MODIFY_RESP:The CQRQ Modify Response
 *		 message is sent in response to the CQRQ Modify Request
 *		 message from the host to indicate the status of the operation.
 * @VXGE_HAL_MSG_TYPE_CQRQ_QUERY_REQ:The CQRQ Query Request
 *		 message is used by the host to query the properties of a CQRQ
 *		 on the adapter
 * @VXGE_HAL_MSG_TYPE_CQRQ_QUERY_RESP:The CQRQ Query Response
 *		 message is sent in response to the CQRQ Query Request
 *		 message from the host to indicate the status of the operation
 *		 and return any CQRQ properties to the host.
 * @VXGE_HAL_MSG_TYPE_CQRQ_ARM_REQ:The CQRQ Arm Request  message
 *		 is used by the host to change the armed state of a CQRQ on the
 *		 adapter. The armed state determines how the adapter will
 *		 interrupt the host when RDMA messages arrive.
 * @VXGE_HAL_MSG_TYPE_CQRQ_ARM_RESP:The CQRQ Arm Response  message
 *		 is sent in response to the CQRQ Arm Request  message from the
 *		 host to indicate the status of arming the CQRQ
 * @VXGE_HAL_MSG_TYPE_CQRQ_EVENT_NOTIF:The CQRQ Event Notification
 *		  message is sent to host when the adapter encounters a
 *		 problem when DMAing CQEs from host memory. There are three
 *		 conditions, EOL, Low Threshold, Drained
 * @VXGE_HAL_MSG_TYPE_CQRQ_FIRST_CQE_BW_NOTIF_REQ:The CQRQ
 *		 First CQE BW Notification Request  message is used by the
 *		 host to notify the adapter after it has configured the first
 *		 CQE block wrapper(s). It is required to pass the host address
 *		 and number of bytes of the first CQE block wrapper in host
 *		 memory.
 * @VXGE_HAL_MSG_TYPE_CQRQ_FIRST_CQE_BW_NOTIF_RESP:The CQRQ
 *		 First CQE BW Notification Response  message is sent in
 *		 response to the CQRQ First CQE BW Notification Request
 *		 message from the host to acknowledge the notification from
 *		 host and return the status of updating the CQRQ record with
 *		 the address and bytes of the first CQE block wrapper.
 * @VXGE_HAL_MSG_TYPE_SRQ_CREATE_REQ:The SRQ Create Request
 *		 message is used by the host to create an SRQ on the adapter.
 * @VXGE_HAL_MSG_TYPE_SRQ_CREATE_RESP:The SRQ Create Response
 *		 message is sent in response to the SRQ Create Request
 *		 message from the host to indicate the status of the operation
 *		 and return the SRQ ID to the host.
 * @VXGE_HAL_MSG_TYPE_SRQ_DELETE_REQ:The SRQ Delete Request
 *		 message is used by the host to delete an SRQ on the adapter.
 * @VXGE_HAL_MSG_TYPE_SRQ_DELETE_RESP:The SRQ Delete Response
 *		 message is sent in response to the SRQ Delete Request
 *		 message from the host to indicate the status of the operation.
 * @VXGE_HAL_MSG_TYPE_SRQ_MODIFY_REQ:The SRQ Modify Request
 *		 message is used by the host to modify an SRQ on the adapter.
 *		 The host must send down all the fields to modify. To simplify
 *		 the adapter firmware there will be no mask to modify individual
 *		 fields.
 * @VXGE_HAL_MSG_TYPE_SRQ_MODIFY_RESP:The SRQ Modify Response
 *		 message is sent in response to the SRQ Modify Request
 *		 message from the host to indicate the status of the operation.
 * @VXGE_HAL_MSG_TYPE_SRQ_QUERY_REQ:The SRQ Query Request  message
 *		 is used by the host to query the properties of an SRQ on the
 *		 adapter.
 * @VXGE_HAL_MSG_TYPE_SRQ_QUERY_RESP:The SRQ Query Response
 *		 message is sent in response to the SRQ Query Request  message
 *		 from the host to indicate the status of the operation and
 *		 return any SRQ properties to the host
 * @VXGE_HAL_MSG_TYPE_SRQ_ARM_REQ:The SRQ Arm Request  message is
 *		 sent to the adapter to arm or re-arm the SRQ limit.
 * @VXGE_HAL_MSG_TYPE_SRQ_ARM_RESP:The SRQ Arm Response  is sent
 *		 to the host to acknowledge the SRQ Arm Request  and indicate
 *		 the status of arming or re-arming the SRQ limit.
 * @VXGE_HAL_MSG_TYPE_SRQ_EVENT_NOTIF:The SRQ Event Notification
 *		 iMSG is used to alert the host that the adapter has encountered
 *		 one of the following conditions when DMAing WQEs from host
 *		 memory - EOL (End of list of WQEs in host memory),Low Threshold
 *		 (The adapter is running low on available WQEs),Drained (Adapter
 *		 out of WQEs because of EOL condition or adapter use faster than
 *		 DMA), SRQ Limit (The number of available WQEs on adapter + host
 *		 less than SRQ limit and the SRQ limit is armed).
 * @VXGE_HAL_MSG_TYPE_SRQ_FIRST_WQE_BW_NOTIF_REQ:The SRQ First
 *		 WQE BW Notification Request  is used to alert the adapter of
 *		 the location of the first WQE block wrapper after initially
 *		 creating the SRQ. It is required because the host cannot
 *		 pre-post WQEs when creating the SRQ.
 * @VXGE_HAL_MSG_TYPE_SRQ_FIRST_WQE_BW_NOTIF_RESP:The SRQ First
 *		 WQE BW Notification Response  message is sent in response to
 *		 the SRQ First WQE BW Notification Request  message from the
 *		 host to indicate the status of the operation.
 * @VXGE_HAL_MSG_TYPE_SRQ_WQE_BLOCKS_ADDED_NOTIF_REQ:The SRQ
 *		 WQE Blocks Added Notification Request  is used to alert the
 *		 adapter that new WQEs have been posted in host memory. This is
 *		 required in order for the adapter to support the concept of SRQ
 *		 limit.
 * @VXGE_HAL_MSG_TYPE_SRQ_WQE_BLOCKS_ADDED_NOTIF_RESP:The SRQ
 *		 WQE Blocks Added Notification Response  is sent by the adapter
 *		 in response to the SRQ WQE Blocks Added Notification Request
 *		 to acknowledge the notification from the host and to return any
 *		 status in the event a problem occurred.
 * @VXGE_HAL_MSG_TYPE_SRQ_RETURN_UNUSED_WQES_REQ:The SRQ Return WQEs
 *		 Request  message may be sent by the host to reclaim unused
 *		 WQEs from the head of the WQE block wrapper list. Its purpose
 *		 is to reclaim over-provisioned WQEs for an SRQ. The host may
 *		 choose to reclaim WQEs from an SRQ at any time.
 * @VXGE_HAL_MSG_TYPE_SRQ_RETURN_UNUSED_WQES_RESP:The SRQ Return WQEs
 *		  is sent in reply to the SRQ Return WQEs Request  message
 *		 to reclaim unused WQEs from an over-provisioned SRQ.
 * @VXGE_HAL_MSG_TYPE_STAG_ALLOCATE_NSMR_REQ:This downward message
 *		 commands the adapter to create a new non-shared memory region
 *		 (NSMR) in the invalid state. This message is used to implement
 *		 the Allocate Non-Shared Memory Region memory management verb.
 * @VXGE_HAL_MSG_TYPE_STAG_ALLOCATE_NSMR_RESP:This upward message
 *		 communicates to the host the success or failure of the
 *		 corresponding STAG Allocate NSMR Request  message
 * @VXGE_HAL_MSG_TYPE_STAG_ALLOCATE_MW_REQ:This downward message
 *		 commands the adapter to allocate a new memory window (MW).
 *		 This message is used to implement the Allocate Memory Window
 *		 memory management verb.
 * @VXGE_HAL_MSG_TYPE_STAG_ALLOCATE_MW_RESP:This upward message
 *		 communicates to the host the success or failure of the
 *		 corresponding STAG Allocate MW Request  message.
 * @VXGE_HAL_MSG_TYPE_STAG_DE_ALLOCATE_REQ:This downward message
 *		 commands the adapter to deallocate the specified STag, freeing
 *		 up any on-adapter resources
 * @VXGE_HAL_MSG_TYPE_STAG_DE_ALLOCATE_RESP:This upward message
 *		 communicates to the host the success or failure of the
 *		 corresponding STAG De-allocate Request  message.
 * @VXGE_HAL_MSG_TYPE_STAG_REGISTER_NSMR_REQ:This downward message
 *		 commands the adapter to register a non-shared memory region.
 *		 This message is used to implement Register NSMR memory
 *		 management verb Fast registration cannot be performed with
 *		 this . It can only be done via the PostSQ TOWI.
 * @VXGE_HAL_MSG_TYPE_STAG_REGISTER_NSMR_RESP:This upward message
 *		 communicates to the host the success or failure of the
 *		 corresponding STAG Register NSMR Request  message.
 * @VXGE_HAL_MSG_TYPE_STAG_RE_REGISTER_NSMR_REQ:This downward
 *		 message commands the adapter to change the memory registration
 *		 of an existing NSMR to create a new NSMR in the valid state.
 *		 This message is used to implement the Reregister Non-Shared
 *		 Memory Region memory management verb.
 * @VXGE_HAL_MSG_TYPE_STAG_RE_REGISTER_NSMR_RESP:This upward message
 *		 communicates to the host the success or failure of the
 *		 corresponding STAG Re-register NSMR Request  message
 * @VXGE_HAL_MSG_TYPE_STAG_REGISTER_SMR_REQ:This downward message
 *		 commands the adapter to create a shared memory region (SMR)
 *		 based on an existing memory region, either shared(SMR) or
 *		 non-shared(NSMR). This message is used to implement the
 *		 Register Shared Memory Region verb.
 * @VXGE_HAL_MSG_TYPE_STAG_REGISTER_SMR_RESP:This upward message
 *		 communicates to the host the success or failure of the
 *		 corresponding STAG Re-register NSMR Request  message.
 * @VXGE_HAL_MSG_TYPE_STAG_QUERY_REQ:This downward message commands
 *		 the adapter to return the specifics of the specified STag.
 *		 This message is used to implement the Query Memory Region
 *		 memory management verb and the Query Memory Window memory
 *		 management verb. Memory region and memory window querying
 *		 could be handled entirely by the host software without any
 *		 adapter involvement. The STAG Query Request  and STAG
 *		 Query Response  messages allow the host to implement
 *		 adapter-based STag querying.
 * @VXGE_HAL_MSG_TYPE_STAG_QUERY_RESP:This upward message
 *		 communicates to the host the specifics of the queried STag.
 *		 The response message does not return the underlying the PBL.
 * @VXGE_HAL_MSG_TYPE_STAG_VALID_LOCAL_TAG_REQ:This message
 *		 commands the adapter to transition an invalid STag to the
 *		 valid state without changing any of its other attributes.
 *		 The Validate-STag-/Validate-STag-Response- messages
 *		 allow a Neterion-proprietary ability to revalidate an invalid
 *		 STag without changing any of its attributes or its PBL. This
 *		 is expected to be useful in situations where an STag is
 *		 invalidated and then revalidated with the same attributes
 *		 including PBL. Using this message, rather than the more
 *		 general Reregister NSMR, saves the overhead of transferring
 *		 the PBL to the adapter.
 * @VXGE_HAL_MSG_TYPE_STAG_VALID_LOCAL_TAG_RESP:This upward message
 *		 communicates to the host the success of failure of the
 *		 corresponding STAG Validate Local Tag Request  message.
 * @VXGE_HAL_MSG_TYPE_STAG_INVALID_LOCAL_TAG_REQ: The STAG
 *		 Invalidate Local Tag Request  message is used by the host to
 *		 invalidate a local STAG. This message provides an alternative
 *		 route for the normal TOWI based STAG Invalidation. It allows a
 *		 kernel mode process to invalidate an STAG without writing
 *		 a TOWI.
 * @VXGE_HAL_MSG_TYPE_STAG_INVALID_LOCAL_TAG_RESP: This upward
 *		 message communicates to the host the success or failure of the
 *		 corresponding STAG Invalidate Local Tag Request  message.
 * @VXGE_HAL_MSG_TYPE_STAG_BIND_MW_REQ: This downward message commands
 *		 the adapter to bind an existing (invalid) MW to an existing
 *		 (valid) MR.  This message provides an alternative to the TOWI
 *		 based implementation allowing the  path be used for MW binding
 * @VXGE_HAL_MSG_TYPE_STAG_BIND_MW_RESP: This upward message
 *		 communicates to the host the success or failure of the
 *		 corresponding STAG Bind MW Request  message.
 * @VXGE_HAL_MSG_TYPE_STAG_FAST_REGISTER_NSMR_REQ: The STAG Fast
 *		 Register NSMR Request  provides an alternative way to fast
 *		 register an NSMR instead of going to the TOWI path.
 * @VXGE_HAL_MSG_TYPE_STAG_FAST_REGISTER_NSMR_RESP:  The STag Fast
 *		 Register NSMR Response  message is sent to the host from
 *		 the adapter in response to the original  message. It
 *		 indicates the status of fast registering the NSMR.
 * @VXGE_HAL_MSG_TYPE_TCP_OPEN_REQ:The TCP Open Request  message
 *		 is sent by the host to open a TCP connection on the adapter.
 * @VXGE_HAL_MSG_TYPE_TCP_OPEN_RESP:The TCP Open Response  message
 *		 is sent in response to a TCP Open Request  message to indicate
 *		 that the TCP session has been opened
 * @VXGE_HAL_MSG_TYPE_TCP_PROMOTE_TO_IWARP_REQ:The TCP Promote to
 *		 iWARP Request  message is sent from the host to the adapter
 *		 in order to migrate an existing bytestream session to iWARP
 *		 mode.
 * @VXGE_HAL_MSG_TYPE_TCP_PROMOTE_TO_IWARP_RESP:The TCP Promote to
 *		 iWARP Response  message is sent to the host to indicate the
 *		 status of promoting an existing bytestream session to iWARP
 *		 mode.
 * @VXGE_HAL_MSG_TYPE_TCP_MODIFY_REQ:The TCP Modify Request  message
 *		 is sent by the host to modify the attributes associated with a
 *		 bytestream or iWARP session.
 * @VXGE_HAL_MSG_TYPE_TCP_MODIFY_RESP:The TCP Modify Response  message
 *		 is sent to the host in response to a TCP Modify Request message
 *		 to indicate the status of changing the attributes associated
 *		 with the bytestream or iWARP session.
 * @VXGE_HAL_MSG_TYPE_TCP_DELETE_REQ:The TCP Delete Request
 *		 message is sent by the host to delete a bytestream TCP session
 *		 on the adapter.
 * @VXGE_HAL_MSG_TYPE_TCP_DELETE_RESP:The TCP Delete Response
 *		 message is sent in response to a TCP Delete Request  message
 *		 to indicate that the TCP session has been deleted.
 * @VXGE_HAL_MSG_TYPE_TCP_ABORT_REQ: The TCP Abort Request  message
 *		 is used to abort a bytestream or iWARP session.
 * @VXGE_HAL_MSG_TYPE_TCP_ABORT_RESP: The TCP Abort Response
 *		 message is sent to the host from the adapter after aborting the
 *		 bytestream or iWARP session.
 * @VXGE_HAL_MSG_TYPE_TCP_ESTABLISHED: The TCP Established  message is
 *		 an un-solicited event sent from the adapter to the host when
 *		 the SYN+ACK segment arrives (active opener) or the ACK segment
 *		(passive opener) arrives at the adapter.
 * @VXGE_HAL_MSG_TYPE_TCP_FIN_RECEIVED: The TCP FIN Received  message
 *		 is an un-solicited event sent from the adapter to the host on
 *		 session teardown. It indicates that the FIN segment has been
 *		 received from the remote end and the session is now in TIME
 *		 WAIT state.
 * @VXGE_HAL_MSG_TYPE_TCP_TIME_WAIT_DONE: The TCP Time Wait Done  message
 *		 is sent from the adapter to the host to indicate when the TCP
 *		 session leaves the TIME WAIT state.
 * @VXGE_HAL_MSG_TYPE_TCP_UPDATE_RXWIN: This message is used for receive
 *		 window updates, both for rx window flow control updates(updates
 *		 to rcv_buf as data is consumed by the application on the host)
 *		 and for maximum receive window size updates (when the receive
 *		 buffer size changes on the host)
 * @VXGE_HAL_MSG_TYPE_TCP_UPDATE_MSS: This  is sent by the host to the
 *		 adapter to update the MSS for the session.
 * @VXGE_HAL_MSG_TYPE_TCP_UPDATE_IP_HEADER: The TCP Update IP Header
 *		 is used to update the IP TOS and IP flow label in the IP header
 * @VXGE_HAL_MSG_TYPE_TCP_UPDATE_KEEPALIVE: The TCP Update Keepalive
 *		 message is sent from the host to the adapter to update the
 *		 keep-alive timer for the session.
 * @VXGE_HAL_MSG_TYPE_TCP_UPDATE_FAILURE: The TCP Update Failure
 *		 message is sent to the host from the adapter in the event that
 *		 one of the TCP update messages failed for the session. Normally
 *		 these messages do not require a reply and therefore there is no
 *		 response from the adapter. The TCP Update messages include:
 *		 VXGE_HAL_MSG_TYPE_TCP_UPDATE_RXWIN
 *		 VXGE_HAL_MSG_TYPE_TCP_UPDATE_MSS
 *		 VXGE_HAL_MSG_TYPE_TCP_UPDATE_IP_HEADER
 *		 VXGE_HAL_MSG_TYPE_TCP_UPDATE_KEEPALIVE
 * @VXGE_HAL_MSG_TYPE_TCP_FIN_ACK_RECEIVED:The TCP FIN ACK Received
 *		 message is an unsolicited message sent to the host from the
 *		 adapter on received of the ACK segment acknowledging that the
 *		 remote end has received the FIN. It is required for Sun's KPI
 *		 interface.
 * @VXGE_HAL_MSG_TYPE_TCP_RELINK_TO_NCE_REQ:The TCP Relink to NCE
 *		 Request  would be used to change the NCE entry associated
 *		 with a particular bytestream or iWARP session. This message
 *		 could be used to change the NCE of a group of sessions if a
 *		 particular path went down and need to be replaced by a new path
 *		 The host is responsible for tracking the mapping of sessions to
 *		 NCEs so that when de-allocating an NCE it does not de-allocate
 *		 on that is still in use by a particular session.
 * @VXGE_HAL_MSG_TYPE_TCP_RELINK_TO_NCE_RESP:This message is sent in
 *		 response to the TCP Relink to NCE Request  to indicate the
 *		 status of re-linking the TCP session to a particular NCE.
 * @VXGE_HAL_MSG_TYPE_TCP_QP_LIMIT_EXCEEDED:The TCP QP Limit Exceeded
 *		 Notification  message is sent to the host when an iWARP
 *		 session has reached its QP Limit and the QP limit was armed.
 * @VXGE_HAL_MSG_TYPE_TCP_RDMA_TERMINATE_RECEIVED:The TCP RDMA Terminate
 *		 Received  message is an un-solicited event sent from the
 *		 adapter to the host when an RDMA terminate message has been
 *		 received from the remote end.
 * @VXGE_HAL_MSG_TYPE_LRO_OPEN_REQ:The LRO Open Request  message
 *		 is sent by the host to open an LRO connection on the adapter.
 *		 There is no PE context for an LRO session. The PE is involved
 *		 for timer purposes and transferring messages to the RPE but it
 *		 contains no session context.
 * @VXGE_HAL_MSG_TYPE_LRO_OPEN_RESP:The LRO Open Response  message
 *		 is sent in response to a LRO Open Request  message to
 *		 indicate that the LRO session has been opened.
 * @VXGE_HAL_MSG_TYPE_LRO_END_CLASSIF_REQ:The LRO End
 *		 Classification Request  is sent by the host before the LRO
 *		 Delete Request  to tell the adapter to stop steering Rx
 *		 frames from that session into the LRO path. The host would
 *		 later call LRO Delete Request . Separating these two calls
 *		 allows enough time to pass so that frames already in the FB can
 *		 be drained out, thereby avoiding the need for frame reversion.
 * @VXGE_HAL_MSG_TYPE_LRO_END_CLASSIF_RESP:The LRO End
 *		 Classification Response  message is sent in response to a
 *		 LRO End Classification Request  message to indicate that
 *		 classification has been stopped for the LRO session and the
 *		 host can proceed with deleting the LRO session.
 * @VXGE_HAL_MSG_TYPE_LRO_DELETE_REQ:The LRO Delete Request
 *		 message is sent by the host to delete a LRO session on the
 *		 adapter.It might be possible in the future to replace this
 *		 message and the TCP Delete Request  with a single common
 *		 message since there doesn't seem to be any difference between
 *		 the two anymore.
 * @VXGE_HAL_MSG_TYPE_LRO_DELETE_RESP:The LRO Delete Response
 *		 message is sent in response to a LRO Delete Request  message
 *		 to indicate that the LRO session has been deleted.
 * @VXGE_HAL_MSG_TYPE_LRO_SESSION_CANDIDATE_NOTIF:This msg
 *		 indicates to the host that the adapter's autoLRO feature has
 *		 identified a candidate LRO session. No response from the host
 *		 is required. (If the host did decide to act on this information
 *		 from the adapter, the host would use the usual LRO Open Request
 *		).
 * @VXGE_HAL_MSG_TYPE_SPDM_OPEN_REQ:The SPDM Open Request  message
 *		 is sent by the host to open an SPDM connection on the adapter.
 *		 There is no RPE or PE context for an SPDM session. The ONE is
 *		 not involved in this type of classification.
 * @VXGE_HAL_MSG_TYPE_SPDM_OPEN_RESP:The SPDM Open Response
 *		 message is sent in response to a SPDM Open Request  message
 *		 to indicate the status of creating the SPDM session.
 * @VXGE_HAL_MSG_TYPE_SPDM_DELETE_REQ:The SPDM Delete Request
 *		 message is sent by the host to delete an SPDM session on the
 *		 adapter. It might be possible in the future to replace this
 *		 message and the LRO/TCP Delete Request  with a single common
 *		 message since there doesn't seem to be any difference between
 *		 the two anymore.
 * @VXGE_HAL_MSG_TYPE_SPDM_DELETE_RESP:The SPDM Delete Response
 *		 message is sent in response to a SPDM Delete Request  message
 *		 to indicate that the SPDM session has been deleted.
 * @VXGE_HAL_MSG_TYPE_SESSION_EVENT_NOTIF:The Session Event
 *		 Notification  message is an unsolicited message from the
 *		 adapter used to inform the host about an unexpected condition
 *		 on a bytestream or iWARP session.
 * @VXGE_HAL_MSG_TYPE_SESSION_QUERY_REQ:The Session Query Request
 *		 message is sent by the host to query the attributes of an
 *		 existing offloaded session. This message may be used to query
 *		 the attributes of an SPDM, LRO, bytestream or iWARP session.
 *		 Initially this will be a single message used for all purposes.
 *		 In the future this may be split up into multiple messages
 *		 allowing the user to query the pecific context for an SPDM,
 *		 LRO, iWARP, or bytestream session.
 * @VXGE_HAL_MSG_TYPE_SESSION_QUERY_RESP:The Session Query Response
 *		  message is sent in response to a Session Query Request
 *		 message to return the attributes associated with the specified
 *		 session
 * @VXGE_HAL_MSG_TYPE_SESSION_RETURN_IN_PROG_WQES: This message is
 *		 generated by the adapter during deletion of a session to return
 *		 any WQEs that may be in the in-progress list for the session.If
 *		 a WQE is in the in-progress list it is owned by the session and
 *		 cannot be returned to the head of WQE list for an SRQ because
 *		 of ordering issues. Therefore, it must be returned to the host
 *		 at which point the host may choose to destroy the resource or
 *		 simply re-post the WQE for re-use.
 * @VXGE_HAL_MSG_TYPE_SESSION_FRAME_WRITE:The Frame Write  message is
 *		 generated by the adapter in order to send certain frames to the
 *		 host via the  path instead of the normal path. Frames will be
 *		 sent to the host under the following conditions:
 *		 1) mis-aligned frames that the adapter cannot place
 *		 2) during debugging to look at the contents of the frame
 *		 In addition to this,a RDMA terminate message will also be sent
 *		 via a  message but in this case it will be sent in a TCP RDMA
 *		 Terminate Received  message. Frames arriving in the  will
 *		 not have markers stripped. Instead the host will be responsible
 *		 for stripping markers and taking appropriate action on the
 *		 received frame.
 * @VXGE_HAL_MSG_TYPE_SQ_CREATE_REQ: This is HAL private message for.
 *		 SQ create. Never used.
 * @VXGE_HAL_MSG_TYPE_SQ_CREATE_RESP: This is HAL private message
 *		 for SQ create response. This is reported to clients by HAL.
 * @VXGE_HAL_MSG_TYPE_SQ_DELETE_REQ: This is HAL private message for.
 *		 SQ delete. Never used.
 * @VXGE_HAL_MSG_TYPE_SQ_DELETE_RESP:This is HAL private message
 *		 for SQ delete response. This is reported to clients by HAL.
 *
 * Message types supported by the adapter and HAL Private messages.
 */
typedef enum vxge_hal_message_type_e {
	VXGE_HAL_MSG_TYPE_NCE_CREATE_REQ			= 1,
	VXGE_HAL_MSG_TYPE_NCE_CREATE_RESP			= 2,
	VXGE_HAL_MSG_TYPE_NCE_DELETE_REQ			= 3,
	VXGE_HAL_MSG_TYPE_NCE_DELETE_RESP			= 4,
	VXGE_HAL_MSG_TYPE_NCE_UPDATE_MAC_REQ			= 5,
	VXGE_HAL_MSG_TYPE_NCE_UPDATE_MAC_RESP			= 6,
	VXGE_HAL_MSG_TYPE_NCE_UPDATE_RCH_TIME_REQ		= 7,
	VXGE_HAL_MSG_TYPE_NCE_UPDATE_RCH_TIME_RESP		= 8,
	VXGE_HAL_MSG_TYPE_NCE_QUERY_REQ				= 9,
	VXGE_HAL_MSG_TYPE_NCE_QUERY_RESP			= 10,
	VXGE_HAL_MSG_TYPE_NCE_RCH_TIME_EXCEEDED			= 86,
	VXGE_HAL_MSG_TYPE_CQRQ_CREATE_REQ			= 11,
	VXGE_HAL_MSG_TYPE_CQRQ_CREATE_RESP			= 12,
	VXGE_HAL_MSG_TYPE_CQRQ_DELETE_REQ			= 13,
	VXGE_HAL_MSG_TYPE_CQRQ_DELETE_RESP			= 14,
	VXGE_HAL_MSG_TYPE_CQRQ_MODIFY_REQ			= 16,
	VXGE_HAL_MSG_TYPE_CQRQ_MODIFY_RESP			= 17,
	VXGE_HAL_MSG_TYPE_CQRQ_QUERY_REQ			= 18,
	VXGE_HAL_MSG_TYPE_CQRQ_QUERY_RESP			= 19,
	VXGE_HAL_MSG_TYPE_CQRQ_ARM_REQ				= 20,
	VXGE_HAL_MSG_TYPE_CQRQ_ARM_RESP				= 21,
	VXGE_HAL_MSG_TYPE_CQRQ_EVENT_NOTIF			= 22,
	VXGE_HAL_MSG_TYPE_CQRQ_FIRST_CQE_BW_NOTIF_REQ		= 23,
	VXGE_HAL_MSG_TYPE_CQRQ_FIRST_CQE_BW_NOTIF_RESP		= 24,
	VXGE_HAL_MSG_TYPE_SRQ_CREATE_REQ			= 27,
	VXGE_HAL_MSG_TYPE_SRQ_CREATE_RESP			= 28,
	VXGE_HAL_MSG_TYPE_SRQ_DELETE_REQ			= 29,
	VXGE_HAL_MSG_TYPE_SRQ_DELETE_RESP			= 30,
	VXGE_HAL_MSG_TYPE_SRQ_MODIFY_REQ			= 31,
	VXGE_HAL_MSG_TYPE_SRQ_MODIFY_RESP			= 32,
	VXGE_HAL_MSG_TYPE_SRQ_QUERY_REQ				= 33,
	VXGE_HAL_MSG_TYPE_SRQ_QUERY_RESP			= 34,
	VXGE_HAL_MSG_TYPE_SRQ_ARM_REQ				= 35,
	VXGE_HAL_MSG_TYPE_SRQ_ARM_RESP				= 36,
	VXGE_HAL_MSG_TYPE_SRQ_EVENT_NOTIF			= 37,
	VXGE_HAL_MSG_TYPE_SRQ_FIRST_WQE_BW_NOTIF_REQ		= 38,
	VXGE_HAL_MSG_TYPE_SRQ_FIRST_WQE_BW_NOTIF_RESP		= 39,
	VXGE_HAL_MSG_TYPE_SRQ_WQE_BLOCKS_ADDED_NOTIF_REQ	= 40,
	VXGE_HAL_MSG_TYPE_SRQ_WQE_BLOCKS_ADDED_NOTIF_RESP	= 41,
	VXGE_HAL_MSG_TYPE_SRQ_RETURN_UNUSED_WQES_REQ		= 96,
	VXGE_HAL_MSG_TYPE_SRQ_RETURN_UNUSED_WQES_RESP		= 42,
	VXGE_HAL_MSG_TYPE_STAG_ALLOCATE_NSMR_REQ		= 43,
	VXGE_HAL_MSG_TYPE_STAG_ALLOCATE_NSMR_RESP		= 44,
	VXGE_HAL_MSG_TYPE_STAG_ALLOCATE_MW_REQ			= 45,
	VXGE_HAL_MSG_TYPE_STAG_ALLOCATE_MW_RESP			= 46,
	VXGE_HAL_MSG_TYPE_STAG_DE_ALLOCATE_REQ			= 47,
	VXGE_HAL_MSG_TYPE_STAG_DE_ALLOCATE_RESP			= 48,
	VXGE_HAL_MSG_TYPE_STAG_REGISTER_NSMR_REQ		= 49,
	VXGE_HAL_MSG_TYPE_STAG_REGISTER_NSMR_RESP		= 50,
	VXGE_HAL_MSG_TYPE_STAG_RE_REGISTER_NSMR_REQ		= 51,
	VXGE_HAL_MSG_TYPE_STAG_RE_REGISTER_NSMR_RESP		= 52,
	VXGE_HAL_MSG_TYPE_STAG_REGISTER_SMR_REQ			= 53,
	VXGE_HAL_MSG_TYPE_STAG_REGISTER_SMR_RESP		= 54,
	VXGE_HAL_MSG_TYPE_STAG_QUERY_REQ			= 55,
	VXGE_HAL_MSG_TYPE_STAG_QUERY_RESP			= 56,
	VXGE_HAL_MSG_TYPE_STAG_VALID_LOCAL_TAG_REQ		= 57,
	VXGE_HAL_MSG_TYPE_STAG_VALID_LOCAL_TAG_RESP		= 58,
	VXGE_HAL_MSG_TYPE_STAG_INVALID_LOCAL_TAG_REQ		= 87,
	VXGE_HAL_MSG_TYPE_STAG_INVALID_LOCAL_TAG_RESP		= 88,
	VXGE_HAL_MSG_TYPE_STAG_BIND_MW_REQ			= 89,
	VXGE_HAL_MSG_TYPE_STAG_BIND_MW_RESP			= 90,
	VXGE_HAL_MSG_TYPE_STAG_FAST_REGISTER_NSMR_REQ		= 91,
	VXGE_HAL_MSG_TYPE_STAG_FAST_REGISTER_NSMR_RESP		= 92,
	VXGE_HAL_MSG_TYPE_TCP_OPEN_REQ				= 59,
	VXGE_HAL_MSG_TYPE_TCP_OPEN_RESP				= 60,
	VXGE_HAL_MSG_TYPE_TCP_PROMOTE_TO_IWARP_REQ		= 61,
	VXGE_HAL_MSG_TYPE_TCP_PROMOTE_TO_IWARP_RESP		= 62,
	VXGE_HAL_MSG_TYPE_TCP_MODIFY_REQ			= 98,
	VXGE_HAL_MSG_TYPE_TCP_MODIFY_RESP			= 99,
	VXGE_HAL_MSG_TYPE_TCP_DELETE_REQ			= 63,
	VXGE_HAL_MSG_TYPE_TCP_DELETE_RESP			= 64,
	VXGE_HAL_MSG_TYPE_TCP_ABORT_REQ				= 65,
	VXGE_HAL_MSG_TYPE_TCP_ABORT_RESP			= 66,
	VXGE_HAL_MSG_TYPE_TCP_ESTABLISHED			= 78,
	VXGE_HAL_MSG_TYPE_TCP_FIN_RECEIVED			= 79,
	VXGE_HAL_MSG_TYPE_TCP_TIME_WAIT_DONE			= 80,
	VXGE_HAL_MSG_TYPE_TCP_UPDATE_RXWIN			= 81,
	VXGE_HAL_MSG_TYPE_TCP_UPDATE_MSS			= 82,
	VXGE_HAL_MSG_TYPE_TCP_UPDATE_IP_HEADER			= 83,
	VXGE_HAL_MSG_TYPE_TCP_UPDATE_KEEPALIVE			= 84,
	VXGE_HAL_MSG_TYPE_TCP_UPDATE_FAILURE			= 85,
	VXGE_HAL_MSG_TYPE_TCP_FIN_ACK_RECEIVED			= 87,
	VXGE_HAL_MSG_TYPE_TCP_RELINK_TO_NCE_REQ			= 88,
	VXGE_HAL_MSG_TYPE_TCP_RELINK_TO_NCE_RESP		= 89,
	VXGE_HAL_MSG_TYPE_TCP_QP_LIMIT_EXCEEDED			= 100,
	VXGE_HAL_MSG_TYPE_TCP_RDMA_TERMINATE_RECEIVED		= 101,
	VXGE_HAL_MSG_TYPE_LRO_OPEN_REQ				= 67,
	VXGE_HAL_MSG_TYPE_LRO_OPEN_RESP				= 68,
	VXGE_HAL_MSG_TYPE_LRO_END_CLASSIF_REQ			= 69,
	VXGE_HAL_MSG_TYPE_LRO_END_CLASSIF_RESP			= 70,
	VXGE_HAL_MSG_TYPE_LRO_DELETE_REQ			= 71,
	VXGE_HAL_MSG_TYPE_LRO_DELETE_RESP			= 72,
	VXGE_HAL_MSG_TYPE_LRO_SESSION_CANDIDATE_NOTIF		= 73,
	VXGE_HAL_MSG_TYPE_SPDM_OPEN_REQ				= 74,
	VXGE_HAL_MSG_TYPE_SPDM_OPEN_RESP			= 75,
	VXGE_HAL_MSG_TYPE_SPDM_DELETE_REQ			= 76,
	VXGE_HAL_MSG_TYPE_SPDM_DELETE_RESP			= 77,
	VXGE_HAL_MSG_TYPE_SESSION_EVENT_NOTIF			= 102,
	VXGE_HAL_MSG_TYPE_SESSION_QUERY_REQ			= 103,
	VXGE_HAL_MSG_TYPE_SESSION_QUERY_RESP			= 104,
	VXGE_HAL_MSG_TYPE_SESSION_RETURN_IN_PROG_WQES		= 97,
	VXGE_HAL_MSG_TYPE_SESSION_FRAME_WRITE			= 105,
	/* The following are private for HAL */
	VXGE_HAL_MSG_TYPE_SQ_CREATE_REQ				= 65537,
	VXGE_HAL_MSG_TYPE_SQ_CREATE_RESP			= 65538,
	VXGE_HAL_MSG_TYPE_SQ_DELETE_REQ				= 65539,
	VXGE_HAL_MSG_TYPE_SQ_DELETE_RESP			= 65540
} vxge_hal_message_type_e;


/*
 * struct vxge_hal_opaque_handle_t - Opaque handle used by the hal and clients
 *				  to save their contexts
 * @vpath_handle: Virtual path handle
 * @hal_priv: Private data which HAL assigns
 * @client_priv: Client assigned private data
 *
 * This structure is used to store the client and hal data and pass as
 * opaque handle in the messages.
 */
typedef struct vxge_hal_opaque_handle_t {
	vxge_hal_vpath_h		vpath_handle;
#define	VXGE_HAL_OPAQUE_HANDLE_GET_VPATH_HANDLE(op) ((op)->vpath_handle)
#define	VXGE_HAL_OPAQUE_HANDLE_VPATH_HANDLE(op, vrh) (op)->vpath_handle = vrh

	u64				hal_priv;
#define	VXGE_HAL_OPAQUE_HANDLE_GET_HAL_PRIV(op)	    ((op)->hal_priv)
#define	VXGE_HAL_OPAQUE_HANDLE_HAL_PRIV(op, priv)   (op)->hal_priv = (u64)priv

	u64				client_priv;
#define	VXGE_HAL_OPAQUE_HANDLE_GET_CLIENT_PRIV(op)  ((op)->client_priv)
#define	VXGE_HAL_OPAQUE_HANDLE_CLIENT_PRIV(op, priv)	\
					    (op)->client_priv = (u64)priv

} vxge_hal_opaque_handle_t;

/*
 * vxge_hal_vpath_callback_f - Callback to receive up messages.
 * @client_handle: handle passed by client in attach or open function
 * @msgh: Message handle.
 * @msg_type: Type of message
 * @obj_id: Object Id of object to which message belongs
 * @result: Result code
 * @opaque_handle: Opaque handle passed when the request was made.
 *
 * Callback function registered when opening vpath to receive the messages
 * This callback function passed to vxge_hal_vpath_open and
 * vxge_hal_vpath_attach routine to get replys to all asynchronous functions.
 * The format of the reply is a message along with the parameters that are
 * common fro all replys. The message handle passed to this callback is
 * opaque for the iWARP/RDMA module and the information from the message can
 * be got by calling appropriate get function depending on the message type
 * passed as one of the parameter to the callback. The message types that
 * are to be passed to the callback are the ones that are responses and
 * notifications
 */
typedef vxge_hal_status_e(*vxge_hal_vpath_callback_f) (
    vxge_hal_client_h client_handle,
    vxge_hal_up_msg_h msgh,
    vxge_hal_message_type_e msg_type,
    vxge_hal_obj_id_t obj_id,
    vxge_hal_status_e result,
    vxge_hal_opaque_handle_t *opaque_handle);


__EXTERN_END_DECLS

#endif	/* VXGE_HAL_TYPES_H */
