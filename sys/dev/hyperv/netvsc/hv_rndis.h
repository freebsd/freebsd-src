/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
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

#ifndef __HV_RNDIS_H__
#define __HV_RNDIS_H__


/*
 * NDIS protocol version numbers
 */
#define NDIS_VERSION_5_0                        0x00050000
#define NDIS_VERSION_5_1                        0x00050001
#define NDIS_VERSION_6_0                        0x00060000
#define NDIS_VERSION_6_1                        0x00060001
#define NDIS_VERSION_6_30                       0x0006001e

#define NDIS_VERSION                            (NDIS_VERSION_5_1)

/*
 * Status codes
 */

#define STATUS_SUCCESS                          (0x00000000L)
#define STATUS_UNSUCCESSFUL                     (0xC0000001L)
#define STATUS_PENDING                          (0x00000103L)
#define STATUS_INSUFFICIENT_RESOURCES           (0xC000009AL)
#define STATUS_BUFFER_OVERFLOW                  (0x80000005L)
#define STATUS_NOT_SUPPORTED                    (0xC00000BBL)

#define RNDIS_STATUS_SUCCESS                    (STATUS_SUCCESS)
#define RNDIS_STATUS_PENDING                    (STATUS_PENDING)
#define RNDIS_STATUS_NOT_RECOGNIZED             (0x00010001L)
#define RNDIS_STATUS_NOT_COPIED                 (0x00010002L)
#define RNDIS_STATUS_NOT_ACCEPTED               (0x00010003L)
#define RNDIS_STATUS_CALL_ACTIVE                (0x00010007L)

#define RNDIS_STATUS_ONLINE                     (0x40010003L)
#define RNDIS_STATUS_RESET_START                (0x40010004L)
#define RNDIS_STATUS_RESET_END                  (0x40010005L)
#define RNDIS_STATUS_RING_STATUS                (0x40010006L)
#define RNDIS_STATUS_CLOSED                     (0x40010007L)
#define RNDIS_STATUS_WAN_LINE_UP                (0x40010008L)
#define RNDIS_STATUS_WAN_LINE_DOWN              (0x40010009L)
#define RNDIS_STATUS_WAN_FRAGMENT               (0x4001000AL)
#define RNDIS_STATUS_MEDIA_CONNECT              (0x4001000BL)
#define RNDIS_STATUS_MEDIA_DISCONNECT           (0x4001000CL)
#define RNDIS_STATUS_HARDWARE_LINE_UP           (0x4001000DL)
#define RNDIS_STATUS_HARDWARE_LINE_DOWN         (0x4001000EL)
#define RNDIS_STATUS_INTERFACE_UP               (0x4001000FL)
#define RNDIS_STATUS_INTERFACE_DOWN             (0x40010010L)
#define RNDIS_STATUS_MEDIA_BUSY                 (0x40010011L)
#define RNDIS_STATUS_MEDIA_SPECIFIC_INDICATION  (0x40010012L)
#define RNDIS_STATUS_WW_INDICATION        RNDIS_STATUS_MEDIA_SPECIFIC_INDICATION
#define RNDIS_STATUS_LINK_SPEED_CHANGE          (0x40010013L)

#define RNDIS_STATUS_NOT_RESETTABLE             (0x80010001L)
#define RNDIS_STATUS_SOFT_ERRORS                (0x80010003L)
#define RNDIS_STATUS_HARD_ERRORS                (0x80010004L)
#define RNDIS_STATUS_BUFFER_OVERFLOW            (STATUS_BUFFER_OVERFLOW)

#define RNDIS_STATUS_FAILURE                    (STATUS_UNSUCCESSFUL)
#define RNDIS_STATUS_RESOURCES                  (STATUS_INSUFFICIENT_RESOURCES)
#define RNDIS_STATUS_CLOSING                    (0xC0010002L)
#define RNDIS_STATUS_BAD_VERSION                (0xC0010004L)
#define RNDIS_STATUS_BAD_CHARACTERISTICS        (0xC0010005L)
#define RNDIS_STATUS_ADAPTER_NOT_FOUND          (0xC0010006L)
#define RNDIS_STATUS_OPEN_FAILED                (0xC0010007L)
#define RNDIS_STATUS_DEVICE_FAILED              (0xC0010008L)
#define RNDIS_STATUS_MULTICAST_FULL             (0xC0010009L)
#define RNDIS_STATUS_MULTICAST_EXISTS           (0xC001000AL)
#define RNDIS_STATUS_MULTICAST_NOT_FOUND        (0xC001000BL)
#define RNDIS_STATUS_REQUEST_ABORTED            (0xC001000CL)
#define RNDIS_STATUS_RESET_IN_PROGRESS          (0xC001000DL)
#define RNDIS_STATUS_CLOSING_INDICATING         (0xC001000EL)
#define RNDIS_STATUS_NOT_SUPPORTED              (STATUS_NOT_SUPPORTED)
#define RNDIS_STATUS_INVALID_PACKET             (0xC001000FL)
#define RNDIS_STATUS_OPEN_LIST_FULL             (0xC0010010L)
#define RNDIS_STATUS_ADAPTER_NOT_READY          (0xC0010011L)
#define RNDIS_STATUS_ADAPTER_NOT_OPEN           (0xC0010012L)
#define RNDIS_STATUS_NOT_INDICATING             (0xC0010013L)
#define RNDIS_STATUS_INVALID_LENGTH             (0xC0010014L)
#define RNDIS_STATUS_INVALID_DATA               (0xC0010015L)
#define RNDIS_STATUS_BUFFER_TOO_SHORT           (0xC0010016L)
#define RNDIS_STATUS_INVALID_OID                (0xC0010017L)
#define RNDIS_STATUS_ADAPTER_REMOVED            (0xC0010018L)
#define RNDIS_STATUS_UNSUPPORTED_MEDIA          (0xC0010019L)
#define RNDIS_STATUS_GROUP_ADDRESS_IN_USE       (0xC001001AL)
#define RNDIS_STATUS_FILE_NOT_FOUND             (0xC001001BL)
#define RNDIS_STATUS_ERROR_READING_FILE         (0xC001001CL)
#define RNDIS_STATUS_ALREADY_MAPPED             (0xC001001DL)
#define RNDIS_STATUS_RESOURCE_CONFLICT          (0xC001001EL)
#define RNDIS_STATUS_NO_CABLE                   (0xC001001FL)

#define RNDIS_STATUS_INVALID_SAP                (0xC0010020L)
#define RNDIS_STATUS_SAP_IN_USE                 (0xC0010021L)
#define RNDIS_STATUS_INVALID_ADDRESS            (0xC0010022L)
#define RNDIS_STATUS_VC_NOT_ACTIVATED           (0xC0010023L)
#define RNDIS_STATUS_DEST_OUT_OF_ORDER          (0xC0010024L)
#define RNDIS_STATUS_VC_NOT_AVAILABLE           (0xC0010025L)
#define RNDIS_STATUS_CELLRATE_NOT_AVAILABLE     (0xC0010026L)
#define RNDIS_STATUS_INCOMPATABLE_QOS           (0xC0010027L)
#define RNDIS_STATUS_AAL_PARAMS_UNSUPPORTED     (0xC0010028L)
#define RNDIS_STATUS_NO_ROUTE_TO_DESTINATION    (0xC0010029L)

#define RNDIS_STATUS_TOKEN_RING_OPEN_ERROR      (0xC0011000L)


/*
 * Object Identifiers used by NdisRequest Query/Set Information
 */

/*
 * General Objects
 */

#define RNDIS_OID_GEN_SUPPORTED_LIST                    0x00010101
#define RNDIS_OID_GEN_HARDWARE_STATUS                   0x00010102
#define RNDIS_OID_GEN_MEDIA_SUPPORTED                   0x00010103
#define RNDIS_OID_GEN_MEDIA_IN_USE                      0x00010104
#define RNDIS_OID_GEN_MAXIMUM_LOOKAHEAD                 0x00010105
#define RNDIS_OID_GEN_MAXIMUM_FRAME_SIZE                0x00010106
#define RNDIS_OID_GEN_LINK_SPEED                        0x00010107
#define RNDIS_OID_GEN_TRANSMIT_BUFFER_SPACE             0x00010108
#define RNDIS_OID_GEN_RECEIVE_BUFFER_SPACE              0x00010109
#define RNDIS_OID_GEN_TRANSMIT_BLOCK_SIZE               0x0001010A
#define RNDIS_OID_GEN_RECEIVE_BLOCK_SIZE                0x0001010B
#define RNDIS_OID_GEN_VENDOR_ID                         0x0001010C
#define RNDIS_OID_GEN_VENDOR_DESCRIPTION                0x0001010D
#define RNDIS_OID_GEN_CURRENT_PACKET_FILTER             0x0001010E
#define RNDIS_OID_GEN_CURRENT_LOOKAHEAD                 0x0001010F
#define RNDIS_OID_GEN_DRIVER_VERSION                    0x00010110
#define RNDIS_OID_GEN_MAXIMUM_TOTAL_SIZE                0x00010111
#define RNDIS_OID_GEN_PROTOCOL_OPTIONS                  0x00010112
#define RNDIS_OID_GEN_MAC_OPTIONS                       0x00010113
#define RNDIS_OID_GEN_MEDIA_CONNECT_STATUS              0x00010114
#define RNDIS_OID_GEN_MAXIMUM_SEND_PACKETS              0x00010115
#define RNDIS_OID_GEN_VENDOR_DRIVER_VERSION             0x00010116
#define RNDIS_OID_GEN_NETWORK_LAYER_ADDRESSES           0x00010118
#define RNDIS_OID_GEN_TRANSPORT_HEADER_OFFSET           0x00010119
#define RNDIS_OID_GEN_MACHINE_NAME                      0x0001021A
#define RNDIS_OID_GEN_RNDIS_CONFIG_PARAMETER            0x0001021B

/*
 * For receive side scale
 */
/* Query only */
#define RNDIS_OID_GEN_RSS_CAPABILITIES			0x00010203
/* Query and set */
#define RNDIS_OID_GEN_RSS_PARAMETERS			0x00010204

#define RNDIS_OID_GEN_XMIT_OK                           0x00020101
#define RNDIS_OID_GEN_RCV_OK                            0x00020102
#define RNDIS_OID_GEN_XMIT_ERROR                        0x00020103
#define RNDIS_OID_GEN_RCV_ERROR                         0x00020104
#define RNDIS_OID_GEN_RCV_NO_BUFFER                     0x00020105

#define RNDIS_OID_GEN_DIRECTED_BYTES_XMIT               0x00020201
#define RNDIS_OID_GEN_DIRECTED_FRAMES_XMIT              0x00020202
#define RNDIS_OID_GEN_MULTICAST_BYTES_XMIT              0x00020203
#define RNDIS_OID_GEN_MULTICAST_FRAMES_XMIT             0x00020204
#define RNDIS_OID_GEN_BROADCAST_BYTES_XMIT              0x00020205
#define RNDIS_OID_GEN_BROADCAST_FRAMES_XMIT             0x00020206
#define RNDIS_OID_GEN_DIRECTED_BYTES_RCV                0x00020207
#define RNDIS_OID_GEN_DIRECTED_FRAMES_RCV               0x00020208
#define RNDIS_OID_GEN_MULTICAST_BYTES_RCV               0x00020209
#define RNDIS_OID_GEN_MULTICAST_FRAMES_RCV              0x0002020A
#define RNDIS_OID_GEN_BROADCAST_BYTES_RCV               0x0002020B
#define RNDIS_OID_GEN_BROADCAST_FRAMES_RCV              0x0002020C

#define RNDIS_OID_GEN_RCV_CRC_ERROR                     0x0002020D
#define RNDIS_OID_GEN_TRANSMIT_QUEUE_LENGTH             0x0002020E

#define RNDIS_OID_GEN_GET_TIME_CAPS                     0x0002020F
#define RNDIS_OID_GEN_GET_NETCARD_TIME                  0x00020210

/*
 * These are connection-oriented general OIDs.
 * These replace the above OIDs for connection-oriented media.
 */
#define RNDIS_OID_GEN_CO_SUPPORTED_LIST                 0x00010101
#define RNDIS_OID_GEN_CO_HARDWARE_STATUS                0x00010102
#define RNDIS_OID_GEN_CO_MEDIA_SUPPORTED                0x00010103
#define RNDIS_OID_GEN_CO_MEDIA_IN_USE                   0x00010104
#define RNDIS_OID_GEN_CO_LINK_SPEED                     0x00010105
#define RNDIS_OID_GEN_CO_VENDOR_ID                      0x00010106
#define RNDIS_OID_GEN_CO_VENDOR_DESCRIPTION             0x00010107
#define RNDIS_OID_GEN_CO_DRIVER_VERSION                 0x00010108
#define RNDIS_OID_GEN_CO_PROTOCOL_OPTIONS               0x00010109
#define RNDIS_OID_GEN_CO_MAC_OPTIONS                    0x0001010A
#define RNDIS_OID_GEN_CO_MEDIA_CONNECT_STATUS           0x0001010B
#define RNDIS_OID_GEN_CO_VENDOR_DRIVER_VERSION          0x0001010C
#define RNDIS_OID_GEN_CO_MINIMUM_LINK_SPEED             0x0001010D

#define RNDIS_OID_GEN_CO_GET_TIME_CAPS                  0x00010201
#define RNDIS_OID_GEN_CO_GET_NETCARD_TIME               0x00010202

/*
 * These are connection-oriented statistics OIDs.
 */
#define RNDIS_OID_GEN_CO_XMIT_PDUS_OK                   0x00020101
#define RNDIS_OID_GEN_CO_RCV_PDUS_OK                    0x00020102
#define RNDIS_OID_GEN_CO_XMIT_PDUS_ERROR                0x00020103
#define RNDIS_OID_GEN_CO_RCV_PDUS_ERROR                 0x00020104
#define RNDIS_OID_GEN_CO_RCV_PDUS_NO_BUFFER             0x00020105


#define RNDIS_OID_GEN_CO_RCV_CRC_ERROR                  0x00020201
#define RNDIS_OID_GEN_CO_TRANSMIT_QUEUE_LENGTH          0x00020202
#define RNDIS_OID_GEN_CO_BYTES_XMIT                     0x00020203
#define RNDIS_OID_GEN_CO_BYTES_RCV                      0x00020204
#define RNDIS_OID_GEN_CO_BYTES_XMIT_OUTSTANDING         0x00020205
#define RNDIS_OID_GEN_CO_NETCARD_LOAD                   0x00020206

/*
 * These are objects for Connection-oriented media call-managers.
 */
#define RNDIS_OID_CO_ADD_PVC                            0xFF000001
#define RNDIS_OID_CO_DELETE_PVC                         0xFF000002
#define RNDIS_OID_CO_GET_CALL_INFORMATION               0xFF000003
#define RNDIS_OID_CO_ADD_ADDRESS                        0xFF000004
#define RNDIS_OID_CO_DELETE_ADDRESS                     0xFF000005
#define RNDIS_OID_CO_GET_ADDRESSES                      0xFF000006
#define RNDIS_OID_CO_ADDRESS_CHANGE                     0xFF000007
#define RNDIS_OID_CO_SIGNALING_ENABLED                  0xFF000008
#define RNDIS_OID_CO_SIGNALING_DISABLED                 0xFF000009


/*
 * 802.3 Objects (Ethernet)
 */

#define RNDIS_OID_802_3_PERMANENT_ADDRESS               0x01010101
#define RNDIS_OID_802_3_CURRENT_ADDRESS                 0x01010102
#define RNDIS_OID_802_3_MULTICAST_LIST                  0x01010103
#define RNDIS_OID_802_3_MAXIMUM_LIST_SIZE               0x01010104
#define RNDIS_OID_802_3_MAC_OPTIONS                     0x01010105

/*
 *
 */
#define NDIS_802_3_MAC_OPTION_PRIORITY                  0x00000001

#define RNDIS_OID_802_3_RCV_ERROR_ALIGNMENT             0x01020101
#define RNDIS_OID_802_3_XMIT_ONE_COLLISION              0x01020102
#define RNDIS_OID_802_3_XMIT_MORE_COLLISIONS            0x01020103

#define RNDIS_OID_802_3_XMIT_DEFERRED                   0x01020201
#define RNDIS_OID_802_3_XMIT_MAX_COLLISIONS             0x01020202
#define RNDIS_OID_802_3_RCV_OVERRUN                     0x01020203
#define RNDIS_OID_802_3_XMIT_UNDERRUN                   0x01020204
#define RNDIS_OID_802_3_XMIT_HEARTBEAT_FAILURE          0x01020205
#define RNDIS_OID_802_3_XMIT_TIMES_CRS_LOST             0x01020206
#define RNDIS_OID_802_3_XMIT_LATE_COLLISIONS            0x01020207


/*
 * RNDIS MP custom OID for test
 */
#define OID_RNDISMP_GET_RECEIVE_BUFFERS                 0xFFA0C90D // Query only


/*
 * Remote NDIS message types
 */
#define REMOTE_NDIS_PACKET_MSG                          0x00000001
#define REMOTE_NDIS_INITIALIZE_MSG                      0x00000002
#define REMOTE_NDIS_HALT_MSG                            0x00000003
#define REMOTE_NDIS_QUERY_MSG                           0x00000004
#define REMOTE_NDIS_SET_MSG                             0x00000005
#define REMOTE_NDIS_RESET_MSG                           0x00000006
#define REMOTE_NDIS_INDICATE_STATUS_MSG                 0x00000007
#define REMOTE_NDIS_KEEPALIVE_MSG                       0x00000008

#define REMOTE_CONDIS_MP_CREATE_VC_MSG                  0x00008001
#define REMOTE_CONDIS_MP_DELETE_VC_MSG                  0x00008002
#define REMOTE_CONDIS_MP_ACTIVATE_VC_MSG                0x00008005
#define REMOTE_CONDIS_MP_DEACTIVATE_VC_MSG              0x00008006
#define REMOTE_CONDIS_INDICATE_STATUS_MSG               0x00008007

/*
 * Remote NDIS message completion types
 */
#define REMOTE_NDIS_INITIALIZE_CMPLT                    0x80000002
#define REMOTE_NDIS_QUERY_CMPLT                         0x80000004
#define REMOTE_NDIS_SET_CMPLT                           0x80000005
#define REMOTE_NDIS_RESET_CMPLT                         0x80000006
#define REMOTE_NDIS_KEEPALIVE_CMPLT                     0x80000008

#define REMOTE_CONDIS_MP_CREATE_VC_CMPLT                0x80008001
#define REMOTE_CONDIS_MP_DELETE_VC_CMPLT                0x80008002
#define REMOTE_CONDIS_MP_ACTIVATE_VC_CMPLT              0x80008005
#define REMOTE_CONDIS_MP_DEACTIVATE_VC_CMPLT            0x80008006

/*
 * Reserved message type for private communication between lower-layer
 * host driver and remote device, if necessary.
 */
#define REMOTE_NDIS_BUS_MSG                             0xff000001

/*
 * Defines for DeviceFlags in rndis_initialize_complete
 */
#define RNDIS_DF_CONNECTIONLESS                         0x00000001
#define RNDIS_DF_CONNECTION_ORIENTED                    0x00000002
#define RNDIS_DF_RAW_DATA                               0x00000004

/*
 * Remote NDIS medium types.
 */
#define RNDIS_MEDIUM_802_3                              0x00000000
#define RNDIS_MEDIUM_802_5                              0x00000001
#define RNDIS_MEDIUM_FDDI                               0x00000002
#define RNDIS_MEDIUM_WAN                                0x00000003
#define RNDIS_MEDIUM_LOCAL_TALK                         0x00000004
#define RNDIS_MEDIUM_ARCNET_RAW                         0x00000006
#define RNDIS_MEDIUM_ARCNET_878_2                       0x00000007
#define RNDIS_MEDIUM_ATM                                0x00000008
#define RNDIS_MEDIUM_WIRELESS_WAN                       0x00000009
#define RNDIS_MEDIUM_IRDA                               0x0000000a
#define RNDIS_MEDIUM_CO_WAN                             0x0000000b
/* Not a real medium, defined as an upper bound */
#define RNDIS_MEDIUM_MAX                                0x0000000d

/*
 * Remote NDIS medium connection states.
 */
#define RNDIS_MEDIA_STATE_CONNECTED                     0x00000000
#define RNDIS_MEDIA_STATE_DISCONNECTED                  0x00000001

/*
 * Remote NDIS version numbers
 */
#define RNDIS_MAJOR_VERSION                             0x00000001
#define RNDIS_MINOR_VERSION                             0x00000000


/*
 * Remote NDIS offload parameters
 */
#define RNDIS_OBJECT_TYPE_DEFAULT			0x80
 
#define RNDIS_OFFLOAD_PARAMETERS_REVISION_3		3
#define RNDIS_OFFLOAD_PARAMETERS_NO_CHANGE		0
#define RNDIS_OFFLOAD_PARAMETERS_LSOV2_DISABLED		1
#define RNDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED 		2
#define RNDIS_OFFLOAD_PARAMETERS_LSOV1_ENABLED		2
#define RNDIS_OFFLOAD_PARAMETERS_RSC_DISABLED		1
#define RNDIS_OFFLOAD_PARAMETERS_RSC_ENABLED		2
#define RNDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED		1
#define RNDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED	2
#define RNDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED	3
#define RNDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED		4

#define RNDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE		1
#define RNDIS_TCP_LARGE_SEND_OFFLOAD_IPV4		0
#define RNDIS_TCP_LARGE_SEND_OFFLOAD_IPV6		1


#define RNDIS_OID_TCP_OFFLOAD_CURRENT_CONFIG		0xFC01020B /* query only */
#define RNDIS_OID_TCP_OFFLOAD_PARAMETERS		0xFC01020C /* set only */
#define RNDIS_OID_TCP_OFFLOAD_HARDWARE_CAPABILITIES	0xFC01020D/* query only */
#define RNDIS_OID_TCP_CONNECTION_OFFLOAD_CURRENT_CONFIG	0xFC01020E /* query only */
#define RNDIS_OID_TCP_CONNECTION_OFFLOAD_HARDWARE_CAPABILITIES	0xFC01020F /* query */
#define RNDIS_OID_OFFLOAD_ENCAPSULATION			0x0101010A /* set/query */

/*
 * NdisInitialize message
 */
typedef struct rndis_initialize_request_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    uint32_t                                major_version;
    uint32_t                                minor_version;
    uint32_t                                max_xfer_size;
} rndis_initialize_request;

/*
 * Response to NdisInitialize
 */
typedef struct rndis_initialize_complete_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS status */
    uint32_t                                status;
    uint32_t                                major_version;
    uint32_t                                minor_version;
    uint32_t                                device_flags;
    /* RNDIS medium */
    uint32_t                                medium;
    uint32_t                                max_pkts_per_msg;
    uint32_t                                max_xfer_size;
    uint32_t                                pkt_align_factor;
    uint32_t                                af_list_offset;
    uint32_t                                af_list_size;
} rndis_initialize_complete;

/*
 * Call manager devices only: Information about an address family
 * supported by the device is appended to the response to NdisInitialize.
 */
typedef struct rndis_co_address_family_ {
    /* RNDIS AF */
    uint32_t                                address_family;
    uint32_t                                major_version;
    uint32_t                                minor_version;
} rndis_co_address_family;

/*
 * NdisHalt message
 */
typedef struct rndis_halt_request_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
} rndis_halt_request;

/*
 * NdisQueryRequest message
 */
typedef struct rndis_query_request_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS OID */
    uint32_t                                oid;
    uint32_t                                info_buffer_length;
    uint32_t                                info_buffer_offset;
    /* RNDIS handle */
    uint32_t                                device_vc_handle;
} rndis_query_request;

/*
 * Response to NdisQueryRequest
 */
typedef struct rndis_query_complete_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS status */
    uint32_t                                status;
    uint32_t                                info_buffer_length;
    uint32_t                                info_buffer_offset;
} rndis_query_complete;

/*
 * NdisSetRequest message
 */
typedef struct rndis_set_request_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS OID */
    uint32_t                                oid;
    uint32_t                                info_buffer_length;
    uint32_t                                info_buffer_offset;
    /* RNDIS handle */
    uint32_t                                device_vc_handle;
} rndis_set_request;

/*
 * Response to NdisSetRequest
 */
typedef struct rndis_set_complete_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS status */
    uint32_t                                status;
} rndis_set_complete;

/*
 * NdisReset message
 */
typedef struct rndis_reset_request_ {
    uint32_t                                reserved;
} rndis_reset_request;

/*
 * Response to NdisReset
 */
typedef struct rndis_reset_complete_ {
    /* RNDIS status */
    uint32_t                                status;
    uint32_t                                addressing_reset;
} rndis_reset_complete;

/*
 * NdisMIndicateStatus message
 */
typedef struct rndis_indicate_status_ {
    /* RNDIS status */
    uint32_t                                status;
    uint32_t                                status_buf_length;
    uint32_t                                status_buf_offset;
} rndis_indicate_status;

/*
 * Diagnostic information passed as the status buffer in
 * rndis_indicate_status messages signifying error conditions.
 */
typedef struct rndis_diagnostic_info_ {
    /* RNDIS status */
    uint32_t                                diag_status;
    uint32_t                                error_offset;
} rndis_diagnostic_info;

/*
 * NdisKeepAlive message
 */
typedef struct rndis_keepalive_request_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
} rndis_keepalive_request;

/*
 * Response to NdisKeepAlive
 */  
typedef struct rndis_keepalive_complete_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS status */
    uint32_t                                status;
} rndis_keepalive_complete;

/*
 * Data message. All offset fields contain byte offsets from the beginning
 * of the rndis_packet structure. All length fields are in bytes.
 * VcHandle is set to 0 for connectionless data, otherwise it
 * contains the VC handle.
 */
typedef struct rndis_packet_ {
    uint32_t                                data_offset;
    uint32_t                                data_length;
    uint32_t                                oob_data_offset;
    uint32_t                                oob_data_length;
    uint32_t                                num_oob_data_elements;
    uint32_t                                per_pkt_info_offset;
    uint32_t                                per_pkt_info_length;
    /* RNDIS handle */
    uint32_t                                vc_handle;
    uint32_t                                reserved;
} rndis_packet;

typedef struct rndis_packet_ex_ {
    uint32_t                                data_offset;
    uint32_t                                data_length;
    uint32_t                                oob_data_offset;
    uint32_t                                oob_data_length;
    uint32_t                                num_oob_data_elements;
    uint32_t                                per_pkt_info_offset;
    uint32_t                                per_pkt_info_length;
    /* RNDIS handle */
    uint32_t                                vc_handle;
    uint32_t                                reserved;
    uint64_t                                data_buf_id;
    uint32_t                                data_buf_offset;
    uint64_t                                next_header_buf_id;
    uint32_t                                next_header_byte_offset;
    uint32_t                                next_header_byte_count;
} rndis_packet_ex;

/*
 * Optional Out of Band data associated with a Data message.
 */
typedef struct rndis_oobd_ {
    uint32_t                                size;
    /* RNDIS class ID */
    uint32_t                                type;
    uint32_t                                class_info_offset;
} rndis_oobd;

/*
 * Packet extension field contents associated with a Data message.
 */
typedef struct rndis_per_packet_info_ {
    uint32_t                                size;
    uint32_t                                type;
    uint32_t                                per_packet_info_offset;
} rndis_per_packet_info;

typedef enum ndis_per_pkt_infotype_ {
	tcpip_chksum_info,
	ipsec_info,
	tcp_large_send_info,
	classification_handle_info,
	ndis_reserved,
	sgl_info,
	ieee_8021q_info,
	original_pkt_info,
	pkt_cancel_id,
	original_netbuf_list,
	cached_netbuf_list,
	short_pkt_padding_info,
	max_perpkt_info
} ndis_per_pkt_infotype;

#define nbl_hash_value	pkt_cancel_id
#define nbl_hash_info	original_netbuf_list

typedef struct ndis_8021q_info_ {
	union {
		struct {
			uint32_t   user_pri : 3;  /* User Priority */
			uint32_t   cfi      : 1;  /* Canonical Format ID */
			uint32_t   vlan_id  : 12;
			uint32_t   reserved : 16;
		} s1;
		uint32_t    value;
	} u1;
} ndis_8021q_info;

struct rndis_object_header {
	uint8_t type;
	uint8_t revision;
	uint16_t size;
};

typedef struct rndis_offload_params_ {
	struct rndis_object_header header;
	uint8_t ipv4_csum;
	uint8_t tcp_ipv4_csum;
	uint8_t udp_ipv4_csum;
	uint8_t tcp_ipv6_csum;
	uint8_t udp_ipv6_csum;
	uint8_t lso_v1;
	uint8_t ip_sec_v1;
	uint8_t lso_v2_ipv4;
	uint8_t lso_v2_ipv6;
	uint8_t tcp_connection_ipv4;
	uint8_t tcp_connection_ipv6;
	uint32_t flags;
	uint8_t ip_sec_v2;
	uint8_t ip_sec_v2_ipv4;
	struct {
		uint8_t rsc_ipv4;
		uint8_t rsc_ipv6;
	};
	struct {
		uint8_t encapsulated_packet_task_offload;
		uint8_t encapsulation_types;
	};

} rndis_offload_params;


typedef struct rndis_tcp_ip_csum_info_ {
	union {
		struct {
			uint32_t is_ipv4:1;
			uint32_t is_ipv6:1;
			uint32_t tcp_csum:1;
			uint32_t udp_csum:1;
			uint32_t ip_header_csum:1;
			uint32_t reserved:11;
			uint32_t tcp_header_offset:10;
		} xmit;
		struct {
			uint32_t tcp_csum_failed:1;
			uint32_t udp_csum_failed:1;
			uint32_t ip_csum_failed:1;
			uint32_t tcp_csum_succeeded:1;
			uint32_t udp_csum_succeeded:1;
			uint32_t ip_csum_succeeded:1;
			uint32_t loopback:1;
			uint32_t tcp_csum_value_invalid:1;
			uint32_t ip_csum_value_invalid:1;
		} receive;
		uint32_t  value;
	};
} rndis_tcp_ip_csum_info;

struct rndis_hash_value {
	uint32_t	hash_value;
} __packed;

struct rndis_hash_info {
	uint32_t	hash_info;
} __packed;

#define NDIS_HASH_FUNCTION_MASK		0x000000FF	/* see hash function */
#define NDIS_HASH_TYPE_MASK		0x00FFFF00	/* see hash type */

/* hash function */
#define NDIS_HASH_FUNCTION_TOEPLITZ	0x00000001

/* hash type */
#define NDIS_HASH_IPV4			0x00000100
#define NDIS_HASH_TCP_IPV4		0x00000200
#define NDIS_HASH_IPV6			0x00000400
#define NDIS_HASH_IPV6_EX		0x00000800
#define NDIS_HASH_TCP_IPV6		0x00001000
#define NDIS_HASH_TCP_IPV6_EX		0x00002000

typedef struct rndis_tcp_tso_info_ {
	union {
		struct {
			uint32_t unused:30;
			uint32_t type:1;
			uint32_t reserved2:1;
		} xmit;
		struct {
			uint32_t mss:20;
			uint32_t tcp_header_offset:10;
			uint32_t type:1;
			uint32_t reserved2:1;
		} lso_v1_xmit;
		struct {
			uint32_t tcp_payload:30;
			uint32_t type:1;
			uint32_t reserved2:1;
		} lso_v1_xmit_complete;
		struct {
			uint32_t mss:20;
			uint32_t tcp_header_offset:10;
			uint32_t type:1;
			uint32_t ip_version:1;
		} lso_v2_xmit;
		struct {
			uint32_t reserved:30;
			uint32_t type:1;
			uint32_t reserved2:1;
		} lso_v2_xmit_complete;
		uint32_t  value;
	};
} rndis_tcp_tso_info;

#define RNDIS_HASHVAL_PPI_SIZE	(sizeof(rndis_per_packet_info) + \
				sizeof(struct rndis_hash_value))

#define RNDIS_VLAN_PPI_SIZE	(sizeof(rndis_per_packet_info) + \
				sizeof(ndis_8021q_info))

#define RNDIS_CSUM_PPI_SIZE	(sizeof(rndis_per_packet_info) + \
				sizeof(rndis_tcp_ip_csum_info))

#define RNDIS_TSO_PPI_SIZE	(sizeof(rndis_per_packet_info) + \
				sizeof(rndis_tcp_tso_info))

/*
 * Format of Information buffer passed in a SetRequest for the OID
 * OID_GEN_RNDIS_CONFIG_PARAMETER.
 */
typedef struct rndis_config_parameter_info_ {
    uint32_t                                parameter_name_offset;
    uint32_t                                parameter_name_length;
    uint32_t                                parameter_type;
    uint32_t                                parameter_value_offset;
    uint32_t                                parameter_value_length;
} rndis_config_parameter_info;

/*
 * Values for ParameterType in rndis_config_parameter_info
 */
#define RNDIS_CONFIG_PARAM_TYPE_INTEGER     0
#define RNDIS_CONFIG_PARAM_TYPE_STRING      2


/*
 * CONDIS Miniport messages for connection oriented devices
 * that do not implement a call manager.
 */

/*
 * CoNdisMiniportCreateVc message
 */
typedef struct rcondis_mp_create_vc_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS handle */
    uint32_t                                ndis_vc_handle;
} rcondis_mp_create_vc;

/*
 * Response to CoNdisMiniportCreateVc
 */
typedef struct rcondis_mp_create_vc_complete_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS handle */
    uint32_t                                device_vc_handle;
    /* RNDIS status */
    uint32_t                                status;
} rcondis_mp_create_vc_complete;

/*
 * CoNdisMiniportDeleteVc message
 */
typedef struct rcondis_mp_delete_vc_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS handle */
    uint32_t                                device_vc_handle;
} rcondis_mp_delete_vc;

/*
 * Response to CoNdisMiniportDeleteVc
 */
typedef struct rcondis_mp_delete_vc_complete_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS status */
    uint32_t                                status;
} rcondis_mp_delete_vc_complete;

/*
 * CoNdisMiniportQueryRequest message
 */
typedef struct rcondis_mp_query_request_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS request type */
    uint32_t                                request_type;
    /* RNDIS OID */
    uint32_t                                oid;
    /* RNDIS handle */
    uint32_t                                device_vc_handle;
    uint32_t                                info_buf_length;
    uint32_t                                info_buf_offset;
} rcondis_mp_query_request;

/*
 * CoNdisMiniportSetRequest message
 */
typedef struct rcondis_mp_set_request_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS request type */
    uint32_t                                request_type;
    /* RNDIS OID */
    uint32_t                                oid;
    /* RNDIS handle */
    uint32_t                                device_vc_handle;
    uint32_t                                info_buf_length;
    uint32_t                                info_buf_offset;
} rcondis_mp_set_request;

/*
 * CoNdisIndicateStatus message
 */
typedef struct rcondis_indicate_status_ {
    /* RNDIS handle */
    uint32_t                                ndis_vc_handle;
    /* RNDIS status */
    uint32_t                                status;
    uint32_t                                status_buf_length;
    uint32_t                                status_buf_offset;
} rcondis_indicate_status;

/*
 * CONDIS Call/VC parameters
 */

typedef struct rcondis_specific_parameters_ {
    uint32_t                                parameter_type;
    uint32_t                                parameter_length;
    uint32_t                                parameter_offset;
} rcondis_specific_parameters;

typedef struct rcondis_media_parameters_ {
    uint32_t                                flags;
    uint32_t                                reserved1;
    uint32_t                                reserved2;
    rcondis_specific_parameters             media_specific;
} rcondis_media_parameters;

typedef struct rndis_flowspec_ {
    uint32_t                                token_rate;
    uint32_t                                token_bucket_size;
    uint32_t                                peak_bandwidth;
    uint32_t                                latency;
    uint32_t                                delay_variation;
    uint32_t                                service_type;
    uint32_t                                max_sdu_size;
    uint32_t                                minimum_policed_size;
} rndis_flowspec;

typedef struct rcondis_call_manager_parameters_ {
    rndis_flowspec                          transmit;
    rndis_flowspec                          receive;
    rcondis_specific_parameters             call_mgr_specific;
} rcondis_call_manager_parameters;

/*
 * CoNdisMiniportActivateVc message
 */
typedef struct rcondis_mp_activate_vc_request_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    uint32_t                                flags;
    /* RNDIS handle */
    uint32_t                                device_vc_handle;
    uint32_t                                media_params_offset;
    uint32_t                                media_params_length;
    uint32_t                                call_mgr_params_offset;
    uint32_t                                call_mgr_params_length;
} rcondis_mp_activate_vc_request;

/*
 * Response to CoNdisMiniportActivateVc
 */
typedef struct rcondis_mp_activate_vc_complete_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS status */
    uint32_t                                status;
} rcondis_mp_activate_vc_complete;

/*
 * CoNdisMiniportDeactivateVc message
 */
typedef struct rcondis_mp_deactivate_vc_request_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    uint32_t                                flags;
    /* RNDIS handle */
    uint32_t                                device_vc_handle;
} rcondis_mp_deactivate_vc_request;

/*
 * Response to CoNdisMiniportDeactivateVc
 */
typedef struct rcondis_mp_deactivate_vc_complete_ {
    /* RNDIS request ID */
    uint32_t                                request_id;
    /* RNDIS status */
    uint32_t                                status;
} rcondis_mp_deactivate_vc_complete;

/*
 * union with all of the RNDIS messages
 */
typedef union rndis_msg_container_ {
    rndis_packet                            packet;
    rndis_initialize_request                init_request;
    rndis_halt_request                      halt_request;
    rndis_query_request                     query_request;
    rndis_set_request                       set_request;
    rndis_reset_request                     reset_request;
    rndis_keepalive_request                 keepalive_request;
    rndis_indicate_status                   indicate_status;
    rndis_initialize_complete               init_complete;
    rndis_query_complete                    query_complete;
    rndis_set_complete                      set_complete;
    rndis_reset_complete                    reset_complete;
    rndis_keepalive_complete                keepalive_complete;
    rcondis_mp_create_vc                    co_miniport_create_vc;
    rcondis_mp_delete_vc                    co_miniport_delete_vc;
    rcondis_indicate_status                 co_miniport_status;
    rcondis_mp_activate_vc_request          co_miniport_activate_vc;
    rcondis_mp_deactivate_vc_request        co_miniport_deactivate_vc;
    rcondis_mp_create_vc_complete           co_miniport_create_vc_complete;
    rcondis_mp_delete_vc_complete           co_miniport_delete_vc_complete;
    rcondis_mp_activate_vc_complete         co_miniport_activate_vc_complete;
    rcondis_mp_deactivate_vc_complete       co_miniport_deactivate_vc_complete;
    rndis_packet_ex                         packet_ex;
} rndis_msg_container;

/*
 * Remote NDIS message format
 */
typedef struct rndis_msg_ {
    uint32_t                                ndis_msg_type;

    /*
     * Total length of this message, from the beginning
     * of the rndis_msg struct, in bytes.
     */
    uint32_t                                msg_len;

    /* Actual message */
    rndis_msg_container                     msg;
} rndis_msg;


/*
 * Handy macros
 */

/*
 * get the size of an RNDIS message. Pass in the message type, 
 * rndis_set_request, rndis_packet for example
 */
#define RNDIS_MESSAGE_SIZE(message)                             \
    (sizeof(message) + (sizeof(rndis_msg) - sizeof(rndis_msg_container)))

/*
 * get pointer to info buffer with message pointer
 */
#define MESSAGE_TO_INFO_BUFFER(message)                         \
    (((PUCHAR)(message)) + message->InformationBufferOffset)

/*
 * get pointer to status buffer with message pointer
 */
#define MESSAGE_TO_STATUS_BUFFER(message)                       \
    (((PUCHAR)(message)) + message->StatusBufferOffset)

/*
 * get pointer to OOBD buffer with message pointer
 */
#define MESSAGE_TO_OOBD_BUFFER(message)                         \
    (((PUCHAR)(message)) + message->OOBDataOffset)

/*
 * get pointer to data buffer with message pointer
 */
#define MESSAGE_TO_DATA_BUFFER(message)                         \
    (((PUCHAR)(message)) + message->PerPacketInfoOffset)

/*
 * get pointer to contained message from NDIS_MESSAGE pointer
 */
#define RNDIS_MESSAGE_PTR_TO_MESSAGE_PTR(rndis_message)         \
    ((void *) &rndis_message->Message)

/*
 * get pointer to contained message from NDIS_MESSAGE pointer
 */
#define RNDIS_MESSAGE_RAW_PTR_TO_MESSAGE_PTR(rndis_message)     \
    ((void *) rndis_message)



/*
 * Structures used in OID_RNDISMP_GET_RECEIVE_BUFFERS
 */

#define RNDISMP_RECEIVE_BUFFER_ELEM_FLAG_VMQ_RECEIVE_BUFFER 0x00000001

typedef struct rndismp_rx_buf_elem_ {
    uint32_t                            flags;
    uint32_t                            length;
    uint64_t                            rx_buf_id;
    uint32_t                            gpadl_handle;
    void                                *rx_buf;
} rndismp_rx_buf_elem;

typedef struct rndismp_rx_bufs_info_ {
    uint32_t                            num_rx_bufs;
    rndismp_rx_buf_elem                 rx_buf_elems[1];
} rndismp_rx_bufs_info;



#define RNDIS_HEADER_SIZE (sizeof(rndis_msg) - sizeof(rndis_msg_container))

#define NDIS_PACKET_TYPE_DIRECTED	0x00000001
#define NDIS_PACKET_TYPE_MULTICAST	0x00000002
#define NDIS_PACKET_TYPE_ALL_MULTICAST	0x00000004
#define NDIS_PACKET_TYPE_BROADCAST	0x00000008
#define NDIS_PACKET_TYPE_SOURCE_ROUTING	0x00000010
#define NDIS_PACKET_TYPE_PROMISCUOUS	0x00000020
#define NDIS_PACKET_TYPE_SMT		0x00000040
#define NDIS_PACKET_TYPE_ALL_LOCAL	0x00000080
#define NDIS_PACKET_TYPE_GROUP		0x00000100
#define NDIS_PACKET_TYPE_ALL_FUNCTIONAL	0x00000200
#define NDIS_PACKET_TYPE_FUNCTIONAL	0x00000400
#define NDIS_PACKET_TYPE_MAC_FRAME	0x00000800

/*
 * Externs
 */
struct hv_vmbus_channel;

int netvsc_recv(struct hv_vmbus_channel *chan,
    netvsc_packet *packet, const rndis_tcp_ip_csum_info *csum_info,
    const struct rndis_hash_info *hash_info,
    const struct rndis_hash_value *hash_value);
void netvsc_channel_rollup(struct hv_vmbus_channel *chan);

void* hv_set_rppi_data(rndis_msg *rndis_mesg,
    uint32_t rppi_size,
    int pkt_type);

void* hv_get_ppi_data(rndis_packet *rpkt, uint32_t type);

#endif  /* __HV_RNDIS_H__ */

