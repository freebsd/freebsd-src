/*
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NDIS_VAR_H_
#define _NDIS_VAR_H_

/* Forward declarations */
struct ndis_miniport_block;
struct ndis_mdriver_block;
typedef struct ndis_miniport_block ndis_miniport_block;
typedef struct ndis_mdriver_block ndis_mdriver_block;

/* Base types */
typedef uint32_t ndis_status;
typedef void *ndis_handle;
typedef uint32_t ndis_oid;
typedef uint32_t ndis_error_code;
typedef uint32_t ndis_kspin_lock;
typedef uint8_t ndis_kirql;

/*
 * NDIS status codes (there are lots of them). The ones that
 * don't seem to fit the pattern are actually mapped to generic
 * NT status codes.
 */

#define NDIS_STATUS_SUCCESS		0
#define NDIS_STATUS_PENDING		0x00000103
#define NDIS_STATUS_NOT_RECOGNIZED	0x00010001
#define NDIS_STATUS_NOT_COPIED		0x00010002
#define NDIS_STATUS_NOT_ACCEPTED	0x00010003
#define NDIS_STATUS_CALL_ACTIVE		0x00010007
#define NDIS_STATUS_ONLINE		0x40010003
#define NDIS_STATUS_RESET_START		0x40010004
#define NDIS_STATUS_RESET_END		0x40010005
#define NDIS_STATUS_RING_STATUS		0x40010006
#define NDIS_STATUS_CLOSED		0x40010007
#define NDIS_STATUS_WAN_LINE_UP		0x40010008
#define NDIS_STATUS_WAN_LINE_DOWN	0x40010009
#define NDIS_STATUS_WAN_FRAGMENT	0x4001000A
#define NDIS_STATUS_MEDIA_CONNECT	0x4001000B
#define NDIS_STATUS_MEDIA_DISCONNECT	0x4001000C
#define NDIS_STATUS_HARDWARE_LINE_UP	0x4001000D
#define NDIS_STATUS_HARDWARE_LINE_DOWN	0x4001000E
#define NDIS_STATUS_INTERFACE_UP	0x4001000F
#define NDIS_STATUS_INTERFACE_DOWN	0x40010010
#define NDIS_STATUS_MEDIA_BUSY		0x40010011
#define NDIS_STATUS_MEDIA_SPECIFIC_INDICATION	0x40010012
#define NDIS_STATUS_WW_INDICATION NDIS_STATUS_MEDIA_SPECIFIC_INDICATION
#define NDIS_STATUS_LINK_SPEED_CHANGE	0x40010013
#define NDIS_STATUS_WAN_GET_STATS	0x40010014
#define NDIS_STATUS_WAN_CO_FRAGMENT	0x40010015
#define NDIS_STATUS_WAN_CO_LINKPARAMS	0x40010016
#define NDIS_STATUS_NOT_RESETTABLE	0x80010001
#define NDIS_STATUS_SOFT_ERRORS		0x80010003
#define NDIS_STATUS_HARD_ERRORS		0x80010004
#define NDIS_STATUS_BUFFER_OVERFLOW	0x80000005
#define NDIS_STATUS_FAILURE		0xC0000001
#define NDIS_STATUS_RESOURCES		0xC000009A
#define NDIS_STATUS_CLOSING		0xC0010002
#define NDIS_STATUS_BAD_VERSION		0xC0010004
#define NDIS_STATUS_BAD_CHARACTERISTICS	0xC0010005
#define NDIS_STATUS_ADAPTER_NOT_FOUND	0xC0010006
#define NDIS_STATUS_OPEN_FAILED		0xC0010007
#define NDIS_STATUS_DEVICE_FAILED	0xC0010008
#define NDIS_STATUS_MULTICAST_FULL	0xC0010009
#define NDIS_STATUS_MULTICAST_EXISTS	0xC001000A
#define NDIS_STATUS_MULTICAST_NOT_FOUND	0xC001000B
#define NDIS_STATUS_REQUEST_ABORTED	0xC001000C
#define NDIS_STATUS_RESET_IN_PROGRESS	0xC001000D
#define NDIS_STATUS_CLOSING_INDICATING	0xC001000E
#define NDIS_STATUS_BAD_VERSION		0xC0010004
#define NDIS_STATUS_BAD_CHARACTERISTICS	0xC0010005
#define NDIS_STATUS_ADAPTER_NOT_FOUND	0xC0010006
#define NDIS_STATUS_OPEN_FAILED		0xC0010007
#define NDIS_STATUS_DEVICE_FAILED	0xC0010008
#define NDIS_STATUS_MULTICAST_FULL	0xC0010009
#define NDIS_STATUS_MULTICAST_EXISTS	0xC001000A
#define NDIS_STATUS_MULTICAST_NOT_FOUND	0xC001000B
#define NDIS_STATUS_REQUEST_ABORTED	0xC001000C
#define NDIS_STATUS_RESET_IN_PROGRESS	0xC001000D
#define NDIS_STATUS_CLOSING_INDICATING	0xC001000E
#define NDIS_STATUS_NOT_SUPPORTED	0xC00000BB
#define NDIS_STATUS_INVALID_PACKET	0xC001000F
#define NDIS_STATUS_OPEN_LIST_FULL	0xC0010010
#define NDIS_STATUS_ADAPTER_NOT_READY	0xC0010011
#define NDIS_STATUS_ADAPTER_NOT_OPEN	0xC0010012
#define NDIS_STATUS_NOT_INDICATING	0xC0010013
#define NDIS_STATUS_INVALID_LENGTH	0xC0010014
#define NDIS_STATUS_INVALID_DATA	0xC0010015
#define NDIS_STATUS_BUFFER_TOO_SHORT	0xC0010016
#define NDIS_STATUS_INVALID_OID		0xC0010017
#define NDIS_STATUS_ADAPTER_REMOVED	0xC0010018
#define NDIS_STATUS_UNSUPPORTED_MEDIA	0xC0010019
#define NDIS_STATUS_GROUP_ADDRESS_IN_USE	0xC001001A
#define NDIS_STATUS_FILE_NOT_FOUND	0xC001001B
#define NDIS_STATUS_ERROR_READING_FILE	0xC001001C
#define NDIS_STATUS_ALREADY_MAPPED	0xC001001D
#define NDIS_STATUS_RESOURCE_CONFLICT	0xC001001E
#define NDIS_STATUS_NO_CABLE		0xC001001F
#define NDIS_STATUS_INVALID_SAP		0xC0010020
#define NDIS_STATUS_SAP_IN_USE		0xC0010021
#define NDIS_STATUS_INVALID_ADDRESS	0xC0010022
#define NDIS_STATUS_VC_NOT_ACTIVATED	0xC0010023
#define NDIS_STATUS_DEST_OUT_OF_ORDER	0xC0010024
#define NDIS_STATUS_VC_NOT_AVAILABLE	0xC0010025
#define NDIS_STATUS_CELLRATE_NOT_AVAILABLE	0xC0010026
#define NDIS_STATUS_INCOMPATABLE_QOS	0xC0010027
#define NDIS_STATUS_AAL_PARAMS_UNSUPPORTED	0xC0010028
#define NDIS_STATUS_NO_ROUTE_TO_DESTINATION	0xC0010029
#define NDIS_STATUS_TOKEN_RING_OPEN_ERROR	0xC0011000
#define NDIS_STATUS_INVALID_DEVICE_REQUEST	0xC0000010
#define NDIS_STATUS_NETWORK_UNREACHABLE         0xC000023C

/*
 * NDIS event codes. They are usually reported to NdisWriteErrorLogEntry().
 */

#define EVENT_NDIS_RESOURCE_CONFLICT	0xC0001388
#define EVENT_NDIS_OUT_OF_RESOURCE	0xC0001389
#define EVENT_NDIS_HARDWARE_FAILURE	0xC000138A
#define EVENT_NDIS_ADAPTER_NOT_FOUND	0xC000138B
#define EVENT_NDIS_INTERRUPT_CONNECT	0xC000138C
#define EVENT_NDIS_DRIVER_FAILURE	0xC000138D
#define EVENT_NDIS_BAD_VERSION		0xC000138E
#define EVENT_NDIS_TIMEOUT		0x8000138F
#define EVENT_NDIS_NETWORK_ADDRESS	0xC0001390
#define EVENT_NDIS_UNSUPPORTED_CONFIGURATION	0xC0001391
#define EVENT_NDIS_INVALID_VALUE_FROM_ADAPTER	0xC0001392
#define EVENT_NDIS_MISSING_CONFIGURATION_PARAMETER	0xC0001393
#define EVENT_NDIS_BAD_IO_BASE_ADDRESS	0xC0001394
#define EVENT_NDIS_RECEIVE_SPACE_SMALL	0x40001395
#define EVENT_NDIS_ADAPTER_DISABLED	0x80001396
#define EVENT_NDIS_IO_PORT_CONFLICT	0x80001397
#define EVENT_NDIS_PORT_OR_DMA_CONFLICT	0x80001398
#define EVENT_NDIS_MEMORY_CONFLICT	0x80001399
#define EVENT_NDIS_INTERRUPT_CONFLICT	0x8000139A
#define EVENT_NDIS_DMA_CONFLICT		0x8000139B
#define EVENT_NDIS_INVALID_DOWNLOAD_FILE_ERROR	0xC000139C
#define EVENT_NDIS_MAXRECEIVES_ERROR	0x8000139D
#define EVENT_NDIS_MAXTRANSMITS_ERROR	0x8000139E
#define EVENT_NDIS_MAXFRAMESIZE_ERROR	0x8000139F
#define EVENT_NDIS_MAXINTERNALBUFS_ERROR	0x800013A0
#define EVENT_NDIS_MAXMULTICAST_ERROR	0x800013A1
#define EVENT_NDIS_PRODUCTID_ERROR	0x800013A2
#define EVENT_NDIS_LOBE_FAILUE_ERROR	0x800013A3
#define EVENT_NDIS_SIGNAL_LOSS_ERROR	0x800013A4
#define EVENT_NDIS_REMOVE_RECEIVED_ERROR	0x800013A5
#define EVENT_NDIS_TOKEN_RING_CORRECTION	0x400013A6
#define EVENT_NDIS_ADAPTER_CHECK_ERROR	0xC00013A7
#define EVENT_NDIS_RESET_FAILURE_ERROR	0x800013A8
#define EVENT_NDIS_CABLE_DISCONNECTED_ERROR	0x800013A9
#define EVENT_NDIS_RESET_FAILURE_CORRECTION	0x800013AA

/*
 * NDIS OIDs used by the queryinfo/setinfo routines.
 * Some are required by all NDIS drivers, some are specific to
 * a particular type of device, and some are purely optional.
 * Unfortunately, one of the purely optional OIDs is the one
 * that lets us set the MAC address of the device.
 */

/* Required OIDs */
#define OID_GEN_SUPPORTED_LIST                  0x00010101
#define OID_GEN_HARDWARE_STATUS                 0x00010102
#define OID_GEN_MEDIA_SUPPORTED                 0x00010103
#define OID_GEN_MEDIA_IN_USE                    0x00010104
#define OID_GEN_MAXIMUM_LOOKAHEAD               0x00010105
#define OID_GEN_MAXIMUM_FRAME_SIZE              0x00010106
#define OID_GEN_LINK_SPEED                      0x00010107
#define OID_GEN_TRANSMIT_BUFFER_SPACE           0x00010108
#define OID_GEN_RECEIVE_BUFFER_SPACE            0x00010109
#define OID_GEN_TRANSMIT_BLOCK_SIZE             0x0001010A
#define OID_GEN_RECEIVE_BLOCK_SIZE              0x0001010B
#define OID_GEN_VENDOR_ID                       0x0001010C
#define OID_GEN_VENDOR_DESCRIPTION              0x0001010D
#define OID_GEN_CURRENT_PACKET_FILTER           0x0001010E
#define OID_GEN_CURRENT_LOOKAHEAD               0x0001010F
#define OID_GEN_DRIVER_VERSION                  0x00010110
#define OID_GEN_MAXIMUM_TOTAL_SIZE              0x00010111
#define OID_GEN_PROTOCOL_OPTIONS                0x00010112
#define OID_GEN_MAC_OPTIONS                     0x00010113
#define OID_GEN_MEDIA_CONNECT_STATUS            0x00010114
#define OID_GEN_MAXIMUM_SEND_PACKETS            0x00010115
#define OID_GEN_VENDOR_DRIVER_VERSION           0x00010116
#define OID_GEN_SUPPORTED_GUIDS                 0x00010117
#define OID_GEN_NETWORK_LAYER_ADDRESSES         0x00010118  /* Set only */
#define OID_GEN_TRANSPORT_HEADER_OFFSET         0x00010119  /* Set only */
#define OID_GEN_MACHINE_NAME                    0x0001021A
#define OID_GEN_RNDIS_CONFIG_PARAMETER          0x0001021B  /* Set only */
#define OID_GEN_VLAN_ID                         0x0001021C

/* Optional OIDs. */
#define OID_GEN_MEDIA_CAPABILITIES              0x00010201
#define OID_GEN_PHYSICAL_MEDIUM                 0x00010202

/* Required statistics OIDs. */
#define OID_GEN_XMIT_OK                         0x00020101
#define OID_GEN_RCV_OK                          0x00020102
#define OID_GEN_XMIT_ERROR                      0x00020103
#define OID_GEN_RCV_ERROR                       0x00020104
#define OID_GEN_RCV_NO_BUFFER                   0x00020105

/* Optional OID statistics */
#define OID_GEN_DIRECTED_BYTES_XMIT             0x00020201
#define OID_GEN_DIRECTED_FRAMES_XMIT            0x00020202
#define OID_GEN_MULTICAST_BYTES_XMIT            0x00020203
#define OID_GEN_MULTICAST_FRAMES_XMIT           0x00020204
#define OID_GEN_BROADCAST_BYTES_XMIT            0x00020205
#define OID_GEN_BROADCAST_FRAMES_XMIT           0x00020206
#define OID_GEN_DIRECTED_BYTES_RCV              0x00020207
#define OID_GEN_DIRECTED_FRAMES_RCV             0x00020208
#define OID_GEN_MULTICAST_BYTES_RCV             0x00020209
#define OID_GEN_MULTICAST_FRAMES_RCV            0x0002020A
#define OID_GEN_BROADCAST_BYTES_RCV             0x0002020B
#define OID_GEN_BROADCAST_FRAMES_RCV            0x0002020C
#define OID_GEN_RCV_CRC_ERROR                   0x0002020D
#define OID_GEN_TRANSMIT_QUEUE_LENGTH           0x0002020E
#define OID_GEN_GET_TIME_CAPS                   0x0002020F
#define OID_GEN_GET_NETCARD_TIME                0x00020210
#define OID_GEN_NETCARD_LOAD                    0x00020211
#define OID_GEN_DEVICE_PROFILE                  0x00020212

/* 802.3 (ethernet) OIDs */
#define OID_802_3_PERMANENT_ADDRESS             0x01010101
#define OID_802_3_CURRENT_ADDRESS               0x01010102
#define OID_802_3_MULTICAST_LIST                0x01010103
#define OID_802_3_MAXIMUM_LIST_SIZE             0x01010104
#define OID_802_3_MAC_OPTIONS                   0x01010105
#define NDIS_802_3_MAC_OPTION_PRIORITY          0x00000001
#define OID_802_3_RCV_ERROR_ALIGNMENT           0x01020101
#define OID_802_3_XMIT_ONE_COLLISION            0x01020102
#define OID_802_3_XMIT_MORE_COLLISIONS          0x01020103
#define OID_802_3_XMIT_DEFERRED                 0x01020201
#define OID_802_3_XMIT_MAX_COLLISIONS           0x01020202
#define OID_802_3_RCV_OVERRUN                   0x01020203
#define OID_802_3_XMIT_UNDERRUN                 0x01020204
#define OID_802_3_XMIT_HEARTBEAT_FAILURE        0x01020205
#define OID_802_3_XMIT_TIMES_CRS_LOST           0x01020206
#define OID_802_3_XMIT_LATE_COLLISIONS          0x01020207

/* PnP and power management OIDs */
#define OID_PNP_CAPABILITIES                    0xFD010100
#define OID_PNP_SET_POWER                       0xFD010101
#define OID_PNP_QUERY_POWER                     0xFD010102
#define OID_PNP_ADD_WAKE_UP_PATTERN             0xFD010103
#define OID_PNP_REMOVE_WAKE_UP_PATTERN          0xFD010104
#define OID_PNP_WAKE_UP_PATTERN_LIST            0xFD010105
#define OID_PNP_ENABLE_WAKE_UP                  0xFD010106

/* PnP/PM Statistics (Optional). */
#define OID_PNP_WAKE_UP_OK                      0xFD020200
#define OID_PNP_WAKE_UP_ERROR                   0xFD020201

/* The following bits are defined for OID_PNP_ENABLE_WAKE_UP */
#define NDIS_PNP_WAKE_UP_MAGIC_PACKET           0x00000001
#define NDIS_PNP_WAKE_UP_PATTERN_MATCH          0x00000002
#define NDIS_PNP_WAKE_UP_LINK_CHANGE            0x00000004

/* 802.11 OIDs */
#define OID_802_11_BSSID                        0x0D010101
#define OID_802_11_SSID                         0x0D010102
#define OID_802_11_NETWORK_TYPES_SUPPORTED      0x0D010203
#define OID_802_11_NETWORK_TYPE_IN_USE          0x0D010204
#define OID_802_11_TX_POWER_LEVEL               0x0D010205
#define OID_802_11_RSSI                         0x0D010206
#define OID_802_11_RSSI_TRIGGER                 0x0D010207
#define OID_802_11_INFRASTRUCTURE_MODE          0x0D010108
#define OID_802_11_FRAGMENTATION_THRESHOLD      0x0D010209
#define OID_802_11_RTS_THRESHOLD                0x0D01020A
#define OID_802_11_NUMBER_OF_ANTENNAS           0x0D01020B
#define OID_802_11_RX_ANTENNA_SELECTED          0x0D01020C
#define OID_802_11_TX_ANTENNA_SELECTED          0x0D01020D
#define OID_802_11_SUPPORTED_RATES              0x0D01020E
#define OID_802_11_DESIRED_RATES                0x0D010210
#define OID_802_11_CONFIGURATION                0x0D010211
#define OID_802_11_STATISTICS                   0x0D020212
#define OID_802_11_ADD_WEP                      0x0D010113
#define OID_802_11_REMOVE_WEP                   0x0D010114
#define OID_802_11_DISASSOCIATE                 0x0D010115
#define OID_802_11_POWER_MODE                   0x0D010216
#define OID_802_11_BSSID_LIST                   0x0D010217
#define OID_802_11_AUTHENTICATION_MODE          0x0D010118
#define OID_802_11_PRIVACY_FILTER               0x0D010119
#define OID_802_11_BSSID_LIST_SCAN              0x0D01011A
#define OID_802_11_WEP_STATUS                   0x0D01011B
#define OID_802_11_RELOAD_DEFAULTS              0x0D01011C

/* structures/definitions for 802.11 */
#define NDIS_80211_NETTYPE_11FH		0x00000000
#define NDIS_80211_NETTYPE_11DS		0x00000001

struct ndis_80211_nettype_list {
	uint32_t		ntl_items;
	uint32_t		ntl_type[1];
};

#define NDIS_80211_POWERMODE_CAM	0x00000000
#define NDIS_80211_POWERMODE_MAX_PSP	0x00000001
#define NDIS_80211_POWERMODE_FAST_PSP	0x00000002

typedef uint32_t ndis_80211_power;	/* Power in milliwatts */
typedef uint32_t ndis_80211_rssi;	/* Signal strength in dBm */

struct ndis_80211_config_fh {
	uint32_t		ncf_length;
	uint32_t		ncf_hoppatterh;
	uint32_t		ncf_hopset;
	uint32_t		ncf_dwelltime;
};

typedef struct ndis_80211_config_fh ndis_80211_config_fh;

struct ndis_80211_config {
	uint32_t		nc_length;
	uint32_t		nc_beaconperiod;
	uint32_t		nc_atimwin;
	uint32_t		nc_dsconfig;
	ndis_80211_config_fh	nc_fhconfig;
};

typedef struct ndis_80211_config ndis_80211_config;

struct ndis_80211_stats {
	uint32_t		ns_length;
	uint64_t		ns_txfragcnt;
	uint64_t		ns_txmcastcnt;
	uint64_t		ns_failedcnt;
	uint64_t		ns_retrycnt;
	uint64_t		ns_multiretrycnt;
	uint64_t		ns_rtssuccesscnt;
	uint64_t		ns_rtsfailcnt;
	uint64_t		ns_ackfailcnt;
	uint64_t		ns_dupeframecnt;
	uint64_t		ns_rxfragcnt;
	uint64_t		ns_rxmcastcnt;
	uint64_t		ns_fcserrcnt;
};

typedef struct ndis_80211_stats ndis_80211_stats;

typedef uint32_t ndis_80211_key_idx;

struct ndis_80211_wep {
	uint32_t		nw_length;
	uint32_t		nw_keyidx;
	uint32_t		nw_keylen;
	uint32_t		nw_keydata[1];
};

typedef struct ndis_80211_wep ndis_80211_wep;

#define NDIS_80211_NET_INFRA_IBSS	0x00000000
#define NDIS_80211_NET_INFRA_BSS	0x00000001
#define NDIS_80211_NET_INFRA_AUTO	0x00000002

#define NDIS_80211_AUTHMODE_OPEN	0x00000000
#define NDIS_80211_AUTHMODE_SHARED	0x00000001
#define NDIS_80211_AUTHMODE_AUTO	0x00000002

typedef uint8_t ndis_80211_rates[8];
typedef uint8_t ndis_80211_macaddr[6];

struct ndis_80211_ssid {
	uint32_t		ns_ssidlen;
	uint8_t			ns_ssid[32];
};

typedef struct ndis_80211_ssid ndis_80211_ssid;

struct ndis_wlan_bssid {
	uint32_t		nwb_length;
	ndis_80211_macaddr	nwb_macaddr;
	uint8_t			nwb_rsvd[2];
	ndis_80211_ssid		nwb_ssid;
	uint32_t		nwb_privacy;
	ndis_80211_rssi		nwb_rssi;
	uint32_t		nwb_nettype;
	ndis_80211_config	nwb_config;
	uint32_t		nwb_netinfra;
	ndis_80211_rates	nwb_supportedrates;
};

typedef struct ndis_wlan_bssid ndis_wlan_bssid;

struct ndis_80211_bssid_list {
	uint32_t		nbl_items;
	ndis_wlan_bssid		nbl_bssid[1];
};

typedef struct ndis_80211_bssid_list ndis_80211_bssid_list;

typedef uint32_t ndis_80211_fragthresh;
typedef uint32_t ndis_80211_rtsthresh;
typedef uint32_t ndis_80211_antenna;

#define NDIS_80211_PRIVFILT_ACCEPTALL	0x00000000
#define NDIS_80211_PRIVFILT_8021XWEP	0x00000001

#define NDIS_80211_WEPSTAT_ENABLED	0x00000000
#define NDIS_80211_WEPSTAT_DISABLED	0x00000001
#define NDIS_80211_WEPSTAT_KEYABSENT	0x00000002
#define NDIS_80211_WEPSTAT_NOTSUPPORTED	0x00000003

#define NDIS_80211_RELOADDEFAULT_WEP	0x00000000

/*
 * Attribures of NDIS drivers. Not all drivers support
 * all attributes.
 */

#define NDIS_ATTRIBUTE_IGNORE_REQUEST_TIMEOUT		0x00000002
#define NDIS_ATTRIBUTE_IGNORE_TOKEN_RING_ERRORS		0x00000004
#define NDIS_ATTRIBUTE_BUS_MASTER			0x00000008
#define NDIS_ATTRIBUTE_INTERMEDIATE_DRIVER		0x00000010
#define NDIS_ATTRIBUTE_DESERIALIZE			0x00000020
#define NDIS_ATTRIBUTE_NO_HALT_ON_SUSPEND		0x00000040
#define NDIS_ATTRIBUTE_SURPRISE_REMOVE_OK		0x00000080
#define NDIS_ATTRIBUTE_NOT_CO_NDIS			0x00000100
#define NDIS_ATTRIBUTE_USES_SAFE_BUFFER_APIS		0x00000200

enum ndis_media_state {
	nmc_connected,
	nmc_disconnected
};

typedef enum ndis_media_state ndis_media_state;

/* Ndis Packet Filter Bits (OID_GEN_CURRENT_PACKET_FILTER). */

#define NDIS_PACKET_TYPE_DIRECTED               0x00000001
#define NDIS_PACKET_TYPE_MULTICAST              0x00000002
#define NDIS_PACKET_TYPE_ALL_MULTICAST          0x00000004
#define NDIS_PACKET_TYPE_BROADCAST              0x00000008
#define NDIS_PACKET_TYPE_SOURCE_ROUTING         0x00000010
#define NDIS_PACKET_TYPE_PROMISCUOUS            0x00000020
#define NDIS_PACKET_TYPE_SMT                    0x00000040
#define NDIS_PACKET_TYPE_ALL_LOCAL              0x00000080
#define NDIS_PACKET_TYPE_GROUP                  0x00001000
#define NDIS_PACKET_TYPE_ALL_FUNCTIONAL         0x00002000
#define NDIS_PACKET_TYPE_FUNCTIONAL             0x00004000
#define NDIS_PACKET_TYPE_MAC_FRAME              0x00008000


/* Ndis MAC option bits (OID_GEN_MAC_OPTIONS). */

#define NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA             0x00000001
#define NDIS_MAC_OPTION_RECEIVE_SERIALIZED              0x00000002
#define NDIS_MAC_OPTION_TRANSFERS_NOT_PEND              0x00000004
#define NDIS_MAC_OPTION_NO_LOOPBACK                     0x00000008
#define NDIS_MAC_OPTION_FULL_DUPLEX                     0x00000010
#define NDIS_MAC_OPTION_EOTX_INDICATION                 0x00000020
#define NDIS_MAC_OPTION_8021P_PRIORITY                  0x00000040
#define NDIS_MAC_OPTION_SUPPORTS_MAC_ADDRESS_OVERWRITE  0x00000080
#define NDIS_MAC_OPTION_RECEIVE_AT_DPC                  0x00000100
#define NDIS_MAC_OPTION_8021Q_VLAN                      0x00000200
#define NDIS_MAC_OPTION_RESERVED                        0x80000000

#define NDIS_DMA_24BITS		0x00
#define NDIS_DMA_32BITS		0x01
#define NDIS_DMA_64BITS		0x02

struct ndis_physaddr {
	uint64_t		np_quad;
#ifdef notdef
	uint32_t		np_low;
	uint32_t		np_high;
#endif
};

typedef struct ndis_physaddr ndis_physaddr;

struct ndis_ansi_string {
	uint16_t		nas_len;
	uint16_t		nas_maxlen;
	char			*nas_buf;
};

typedef struct ndis_ansi_string ndis_ansi_string;

/*
 * nus_buf is really a wchar_t *, but it's inconvenient to include
 * all the necessary header goop needed to define it, and it's a
 * pointer anyway, so for now, just make it a uint16_t *.
 */
struct ndis_unicode_string {
	uint16_t		nus_len;
	uint16_t		nus_maxlen;
	uint16_t		*nus_buf;
};

typedef struct ndis_unicode_string ndis_unicode_string;


enum ndis_parm_type {
	ndis_parm_int,
	ndis_parm_hexint,
	ndis_parm_string,
	ndis_parm_multistring,
	ndis_parm_binary
};

typedef enum ndis_parm_type ndis_parm_type;

struct ndis_binary_data {
	uint16_t		nbd_len;
	void			*nbd_buf;
};

typedef struct ndis_binary_data ndis_binary_data;

struct ndis_config_parm {
	ndis_parm_type		ncp_type;
	union {
		uint32_t		ncp_intdata;
		ndis_unicode_string	ncp_stringdata;
		ndis_binary_data	ncp_binarydata;
	} ncp_parmdata;
};

typedef struct ndis_config_parm ndis_config_parm;

struct ndis_list_entry {
	struct ndis_list_entry *nle_flink;
	struct ndis_list_entry *nle_blink;
};

typedef struct ndis_list_entry ndis_list_entry;

struct ndis_bind_paths {
	uint32_t		nbp_number;
	ndis_unicode_string	nbp_paths[1];
};

typedef struct ndis_bind_paths ndis_bind_paths;

struct dispatch_header {
	uint8_t			dh_type;
	uint8_t			dh_abs;
	uint8_t			dh_size;
	uint8_t			dh_inserted;
	uint32_t		dh_sigstate;
	ndis_list_entry		dh_waitlisthead;
};

struct ndis_ktimer {
	struct dispatch_header	nk_header;
	uint64_t		nk_duetime;
	ndis_list_entry		nk_timerlistentry;
	void			*nk_dpc;
	uint32_t		nk_period;
};

struct ndis_kevent {
	struct dispatch_header	nk_header;
};

struct ndis_event {
	struct ndis_kevent	ne_event;
};

typedef struct ndis_event ndis_event;

/* Kernel defered procedure call (i.e. timer callback) */

struct ndis_kdpc;
typedef void (*ndis_kdpc_func)(struct ndis_kdpc *, void *, void *, void *);

struct ndis_kdpc {
	uint16_t		nk_type;
	uint8_t			nk_num;
	uint8_t			nk_importance;
	ndis_list_entry		nk_dpclistentry;
	ndis_kdpc_func		nk_deferedfunc;
	void			*nk_deferredctx;
	void			*nk_sysarg1;
	void			*nk_sysarg2;
	uint32_t		*nk_lock;
};

struct ndis_timer {
	struct ndis_ktimer	nt_timer;
	struct ndis_kdpc	nt_dpc;
};

typedef struct ndis_timer ndis_timer;

typedef void (*ndis_timer_function)(void *, void *, void *, void *);

struct ndis_miniport_timer {
	struct ndis_ktimer	nmt_ktimer;
	struct ndis_kdpc	nmt_dpc;
	ndis_timer_function	nmt_timerfunc;
	void			*nmt_timerctx;
	struct ndis_miniport_timer	*nmt_nexttimer;
};

typedef struct ndis_miniport_timer ndis_miniport_timer;

struct ndis_spin_lock {
	ndis_kspin_lock		nsl_spinlock;
	ndis_kirql		nsl_kirql;
};

typedef struct ndis_spin_lock ndis_spin_lock;

struct ndis_request {
	uint8_t			nr_macreserved[4*sizeof(void *)];
	uint32_t		nr_requesttype;
	union _ndis_data {
		struct _ndis_query_information {
			ndis_oid	nr_oid;
			void		*nr_infobuf;
			uint32_t	nr_infobuflen;
			uint32_t	nr_byteswritten;
			uint32_t	nr_bytesneeded;
		} ndis_query_information;
		struct _ndis_set_information {
			ndis_oid	nr_oid;
			void		*nr_infobuf;
			uint32_t	nr_infobuflen;
			uint32_t	nr_byteswritten;
			uint32_t	nr_bytesneeded;
		} ndis_set_information;
	} ndis_data;
	/* NDIS 5.0 extentions */
	uint8_t			nr_ndis_rsvd[9 * sizeof(void *)];
	union {
		uint8_t		nr_callmgr_rsvd[2 * sizeof(void *)];
		uint8_t		nr_protocol_rsvd[2 * sizeof(void *)];
	} u;
	uint8_t			nr_miniport_rsvd[2 * sizeof(void *)];
};

typedef struct ndis_request ndis_request;

/*
 * Filler, not used.
 */
struct ndis_miniport_interrupt {
	void			*ni_introbj;
	ndis_spin_lock		ni_dpccountlock;
	void			*ni_rsvd;
	void			*ni_isrfunc;
	void			*ni_dpcfunc;
	struct ndis_kdpc	ni_dpc;
	ndis_miniport_block	*ni_block;
	uint8_t			ni_dpccnt;
	uint8_t			ni_filler1;
	struct ndis_kevent	ni_dpcsdoneevent;
	uint8_t			ni_shared;
	uint8_t			ni_isrreq;
};

typedef struct ndis_miniport_interrupt ndis_miniport_interrupt;

enum ndis_interrupt_mode {
	nim_level,
	nim_latched
};

typedef enum ndis_interrupt_mode ndis_interrupt_mode;


struct ndis_buffer {
	struct ndis_buffer	*nb_next;
	uint16_t		nb_size;
	uint16_t		nb_flags;
	void			*nb_process;
	void			*nb_mappedsystemva;
	void			*nb_startva;
	uint32_t		nb_bytecount;
	uint32_t		nb_byteoffset;
};

typedef struct ndis_buffer ndis_buffer;

struct ndis_sc_element {
	ndis_physaddr		nse_addr;
	uint32_t		nse_len;
	uint32_t		*nse_rsvd;
};

typedef struct ndis_sc_element ndis_sc_element;

#define NDIS_MAXSEG		32
struct ndis_sc_list {
	uint32_t		nsl_frags;
	uint32_t		*nsl_rsvd;
	ndis_sc_element		nsl_elements[NDIS_MAXSEG];
};

typedef struct ndis_sc_list ndis_sc_list;

enum ndis_perpkt_info {
	ndis_tcpipcsum_info,
	ndis_ipsec_info,
	ndis_largesend_info,
	ndis_classhandle_info,
	ndis_rsvd,
	ndis_sclist_info,
	ndis_ieee8021q_info,
	ndis_originalpkt_info,
        ndis_packetcancelid,
	ndis_maxpkt_info
};

typedef enum ndis_perpkt_info ndis_perpkt_info;

struct ndis_packet_extension {
	void			*npe_info[ndis_maxpkt_info];
};

typedef struct ndis_packet_extension ndis_packet_extension;

struct ndis_packet_private {
	uint32_t		npp_physcnt;
	uint32_t		npp_totlen;
	ndis_buffer		*npp_head;
	ndis_buffer		*npp_tail;

	void			*npp_pool;
	uint32_t		npp_count;
	uint32_t		npp_flags;
	uint8_t			npp_validcounts;
	uint8_t			npp_ndispktflags;
	uint16_t		npp_packetooboffset;
};

#define NDIS_FLAGS_PROTOCOL_ID_MASK             0x0000000F
#define NDIS_FLAGS_MULTICAST_PACKET             0x00000010
#define NDIS_FLAGS_RESERVED2                    0x00000020
#define NDIS_FLAGS_RESERVED3                    0x00000040
#define NDIS_FLAGS_DONT_LOOPBACK                0x00000080
#define NDIS_FLAGS_IS_LOOPBACK_PACKET           0x00000100
#define NDIS_FLAGS_LOOPBACK_ONLY                0x00000200
#define NDIS_FLAGS_RESERVED4                    0x00000400
#define NDIS_FLAGS_DOUBLE_BUFFERED              0x00000800
#define NDIS_FLAGS_SENT_AT_DPC                  0x00001000
#define NDIS_FLAGS_USES_SG_BUFFER_LIST          0x00002000

#define NDIS_PROTOCOL_ID_DEFAULT        0x00
#define NDIS_PROTOCOL_ID_TCP_IP         0x02
#define NDIS_PROTOCOL_ID_IPX            0x06
#define NDIS_PROTOCOL_ID_NBF            0x07
#define NDIS_PROTOCOL_ID_MAX            0x0F
#define NDIS_PROTOCOL_ID_MASK           0x0F

typedef struct ndis_packet_private ndis_packet_private;

enum ndis_classid {
	ndis_class_802_3prio,
	ndis_class_wirelesswan_mbx,
	ndis_class_irda_packetinfo,
	ndis_class_atm_aainfo
};

typedef enum ndis_classid ndis_classid;

struct ndis_mediaspecific_info {
	uint32_t		nmi_nextentoffset;
	ndis_classid		nmi_classid;
	uint32_t		nmi_size;
	uint8_t			nmi_classinfo[1];
};

typedef struct ndis_mediaspecific_info ndis_mediaspecific_info;

struct ndis_packet_oob {
	union {
		uint64_t		npo_timetotx;
		uint64_t		npo_timetxed;
	} u;
	uint64_t			npo_timerxed;
	uint32_t			npo_hdrlen;
	uint32_t			npo_mediaspecific_len;
	void				*npo_mediaspecific;
	ndis_status			npo_status;
};

typedef struct ndis_packet_oob ndis_packet_oob;

struct ndis_packet {
	ndis_packet_private	np_private;
	union {
		/* For connectionless miniports. */
		struct {
			uint8_t		np_miniport_rsvd[2 * sizeof(void *)];
			uint8_t		np_wrapper_rsvd[2 * sizeof(void *)];
		} np_clrsvd;
		/* For de-serialized miniports */
		struct {
			uint8_t		np_miniport_rsvdex[3 * sizeof(void *)];
			uint8_t		np_wrapper_rsvdex[sizeof(void *)];
		} np_dsrsvd;
		struct {
			uint8_t		np_mac_rsvd[4 * sizeof(void *)];
		} np_macrsvd;
	} u;
	uint32_t		*np_rsvd[2];
	uint8_t			np_proto_rsvd[1];

	/*
	 * This next part is probably wrong, but we need some place
	 * to put the out of band data structure...
	 */
	ndis_packet_oob		np_oob;
	ndis_packet_extension	np_ext;
	ndis_sc_list		np_sclist;
};

typedef struct ndis_packet ndis_packet;

/* mbuf ext type for NDIS */
#define EXT_NDIS		0x999

struct ndis_filterdbs {
	union {
		void			*nf_ethdb;
		void			*nf_nulldb;
	} u;
	void			*nf_trdb;
	void			*nf_fddidb;
	void			*nf_arcdb;
};

typedef struct ndis_filterdbs ndis_filterdbs;

enum ndis_medium {
    NdisMedium802_3,
    NdisMedium802_5,
    NdisMediumFddi,
    NdisMediumWan,
    NdisMediumLocalTalk,
    NdisMediumDix,              /* defined for convenience, not a real medium */
    NdisMediumArcnetRaw,
    NdisMediumArcnet878_2,
    NdisMediumAtm,
    NdisMediumWirelessWan,
    NdisMediumIrda,
    NdisMediumBpc,
    NdisMediumCoWan,
    NdisMedium1394,
    NdisMediumMax
};

typedef enum ndis_medium ndis_medium;
/*
enum interface_type {
	InterfaceTypeUndefined = -1,
	Internal,
	Isa,
	Eisa,
	MicroChannel,
	TurboChannel,
	PCIBus,
	VMEBus,
	NuBus,
	PCMCIABus,
	CBus,
	MPIBus,
	MPSABus,
	ProcessorInternal,
	InternalPowerBus,
	PNPISABus,
	PNPBus,
	MaximumInterfaceType
};
*/
enum ndis_interface_type {
	NdisInterfaceInternal = Internal,
	NdisInterfaceIsa = Isa,
	NdisInterfaceEisa = Eisa,
	NdisInterfaceMca = MicroChannel,
	NdisInterfaceTurboChannel = TurboChannel,
	NdisInterfacePci = PCIBus,
	NdisInterfacePcMcia = PCMCIABus
};

typedef enum ndis_interface_type ndis_interface_type;

struct ndis_paddr_unit {
	ndis_physaddr		npu_physaddr;
	uint32_t		npu_len;
};

typedef struct ndis_paddr_unit ndis_paddr_unit;

struct ndis_map_arg {
	ndis_paddr_unit		*nma_fraglist;
	int			nma_cnt;
	int			nma_max;
};

/*
 * Miniport characteristics were originally defined in the NDIS 3.0
 * spec and then extended twice, in NDIS 4.0 and 5.0.
 */

struct ndis_miniport_characteristics {

	/* NDIS 3.0 */

	uint8_t			nmc_version_major;
	uint8_t			nmc_version_minor;
	uint16_t		nmc_pad;
	uint32_t		nmc_rsvd;
	void *			nmc_checkhang_func;
	void *			nmc_disable_interrupts_func;
	void *			nmc_enable_interrupts_func;
	void *			nmc_halt_func;
	void *			nmc_interrupt_func;
	void *			nmc_init_func;
	void *			nmc_isr_func;
	void *			nmc_queryinfo_func;
	void *			nmc_reconfig_func;
	void *			nmc_reset_func;
	void *			nmc_sendsingle_func;
	void *			nmc_setinfo_func;
	void *			nmc_transferdata_func;

	/* NDIS 4.0 extentions */

	void *			nmc_return_packet_func;
	void *			nmc_sendmulti_func;
	void *			nmc_allocate_complete_func;

	/* NDIS 5.0 extensions */

	void *			nmc_cocreatevc_func;
	void *			nmc_codeletevc_func;
	void *			nmc_coactivatevc_func;
	void *			nmc_codeactivatevc_func;
	void *			nmc_comultisend_func;
	void *			nmc_corequest_func;

	/* NDIS 5.1 extentions */

	void *			nmc_canceltxpkts_handler;
	void *			nmc_pnpevent_handler;
	void *			nmc_shutdown_handler;
	void *			nmc_rsvd0;
	void *			nmc_rsvd1;
	void *			nmc_rsvd2;
	void *			nmc_rsvd3;
};

typedef struct ndis_miniport_characteristics ndis_miniport_characteristics;

struct ndis_driver_object {
	char			*ndo_ifname;
	void			*ndo_softc;
	ndis_miniport_characteristics ndo_chars;
};

typedef struct ndis_driver_object ndis_driver_object;

struct ndis_reference {
	ndis_kspin_lock		nr_spinlock;
	uint16_t		nr_refcnt;
	uint8_t			nr_closing;
};

typedef struct ndis_reference ndis_reference;

struct ndis_timer_entry {
	struct callout_handle	nte_ch;
	ndis_miniport_timer	*nte_timer;
	TAILQ_ENTRY(ndis_timer_entry)	link;
};

TAILQ_HEAD(nte_head, ndis_timer_entry);

/*
 * The miniport block is basically the internal NDIS handle. We need
 * to define this because, unfortunately, it is not entirely opaque
 * to NDIS drivers. For one thing, it contains the function pointer
 * to the NDIS packet receive handler, which is invoked out of the
 * NDIS block via a macro rather than a function pointer. (The
 * NdisMIndicateReceivePacket() routine is a macro rather than
 * a function.) For another, the driver maintains a pointer to the
 * miniport block and passes it as a handle to various NDIS functions.
 * (The driver never really knows this because it's hidden behind
 * an ndis_handle though.)
 *
 * The miniport block has two parts: the first part contains fields
 * that must never change, since they are referenced by driver
 * binaries through macros. The second part is ignored by the driver,
 * but contains various things used internaly by NDIS.SYS. In our
 * case, we define the first 'immutable' part exactly as it appears
 * in Windows, but don't bother duplicating the Windows definitions
 * for the second part. Instead, we replace them with a few BSD-specific
 * things.
 */

struct ndis_miniport_block {
	/*
	 * Windows-specific portion -- DO NOT MODIFY OR NDIS
	 * DRIVERS WILL NOT WORK.
	 */ 
	void			*nmb_signature;	/* magic number */
	ndis_miniport_block	*nmb_nextminiport;
	ndis_mdriver_block	*nmb_driverhandle;
	ndis_handle		nmb_miniportadapterctx;
	ndis_unicode_string	nmb_name;
	ndis_bind_paths		*nmb_bindpaths;
	ndis_handle		nmb_openqueue;
	ndis_reference		nmb_ref;
	ndis_handle		nmb_devicectx;
	uint8_t			nmb_padding;
	uint8_t			nmb_lockacquired;
	uint8_t			nmb_pmodeopens;
	uint8_t			nmb_assignedcpu;
	ndis_kspin_lock		nmb_lock;
	ndis_request		*nmb_mediarequest;
	ndis_miniport_interrupt	*nmb_interrupt;
	uint32_t		nmb_flags;
	uint32_t		nmb_pnpflags;
	ndis_list_entry		nmb_packetlist;
	ndis_packet		*nmb_firstpendingtxpacket;
	ndis_packet		*nmb_returnpacketqueue;
	uint32_t		nmb_requestbuffer;
	void			*nmb_setmcastbuf;
	ndis_miniport_block	*nmb_primaryminiport;
	void			*nmb_wrapperctx;
	void			*nmb_busdatactx;
	uint32_t		nmb_pnpcaps;
	cm_resource_list	*nmb_resources;
	ndis_timer		nmb_wkupdpctimer;
	ndis_unicode_string	nmb_basename;
	ndis_unicode_string	nmb_symlinkname;
	uint32_t		nmb_checkforhangsecs;
	uint16_t		nmb_cfhticks;
	uint16_t		nmb_cfhcurrticks;
	ndis_status		nmb_resetstatus;
	ndis_handle		nmb_resetopen;
	ndis_filterdbs		nmb_filterdbs;
	void			*nmb_pktind_func;
	void			*nmb_senddone_func;
	void			*nmb_sendrsrc_func;
	void			*nmb_resetdone_func;
	ndis_medium		nmb_medium;
	uint32_t		nmb_busnum;
	uint32_t		nmb_bustye;
	uint32_t		nmb_adaptertype;
	void			*nmb_deviceobj;
	void			*nmb_physdeviceobj;
	void			*nmb_nextdeviceobj;
	void			*nmb_mapreg;
	void			*nmb_callmgraflist;
	void			*nmb_miniportthread;
	void			*nmb_setinfobuf;
	uint16_t		nmb_setinfobuflen;
	uint16_t		nmb_maxsendpkts;
	ndis_status		nmb_fakestatus;
	void			*nmb_lockhandler;
	ndis_unicode_string	*nmb_adapterinstancename;
	void			*nmb_timerqueue;
	uint32_t		nmb_mactoptions;
	ndis_request		*nmb_pendingreq;
	uint32_t		nmb_maxlongaddrs;
	uint32_t		nmb_maxshortaddrs;
	uint32_t		nmb_currlookahead;
	uint32_t		nmb_maxlookahead;
	void			*nmb_interrupt_func;
	void			*nmb_disableintr_func;
	void			*nmb_enableintr_func;
	void			*nmb_sendpkts_func;
	void			*nmb_deferredsend_func;
	void			*nmb_ethrxindicate_func;
	void			*nmb_txrxindicate_func;
	void			*nmb_fddirxindicate_func;
	void			*nmb_ethrxdone_func;
	void			*nmb_txrxdone_func;
	void			*nmb_fddirxcond_func;
	void			*nmb_status_func;
	void			*nmb_statusdone_func;
	void			*nmb_tdcond_func;
	void			*nmb_querydone_func;
	void			*nmb_setdone_func;
	void			*nmb_wantxdone_func;
	void			*nmb_wanrx_func;
	void			*nmb_wanrxdone_func;
	/*
	 * End of windows-specific portion of miniport block. Everything
	 * below is BSD-specific.
	 */
	struct ifnet		*nmb_ifp;
	uint8_t			nmb_dummybuf[128];
	ndis_config_parm	nmb_replyparm;
	int			nmb_pciidx;
	device_t		nmb_dev;
	ndis_resource_list	*nmb_rlist;
	ndis_status		nmb_getstat;
	ndis_status		nmb_setstat;
	struct nte_head		nmb_timerlist;
};

typedef ndis_status (*ndis_init_handler)(ndis_status *, uint32_t *,
		ndis_medium *, uint32_t, ndis_handle, ndis_handle);
typedef ndis_status (*ndis_queryinfo_handler)(ndis_handle, ndis_oid,
		void *, uint32_t, uint32_t *, uint32_t *);
typedef ndis_status (*ndis_setinfo_handler)(ndis_handle, ndis_oid,
		void *, uint32_t, uint32_t *, uint32_t *);
typedef ndis_status (*ndis_sendsingle_handler)(ndis_handle,
		ndis_packet *, uint32_t);
typedef ndis_status (*ndis_sendmulti_handler)(ndis_handle,
		ndis_packet **, uint32_t);
typedef void (*ndis_isr_handler)(uint8_t *, uint8_t *, ndis_handle);
typedef void (*ndis_interrupt_handler)(ndis_handle);
typedef void (*ndis_reset_handler)(uint8_t *, ndis_handle);
typedef void (*ndis_halt_handler)(ndis_handle);
typedef void (*ndis_return_handler)(ndis_handle, ndis_packet *);
typedef void (*ndis_enable_interrupts_handler)(ndis_handle);
typedef void (*ndis_disable_interrupts_handler)(ndis_handle);
typedef void (*ndis_shutdown_handler)(void *);
typedef void (*ndis_allocdone_handler)(ndis_handle, void *,
		ndis_physaddr *, uint32_t, void *);
typedef uint8_t (*ndis_checkforhang_handler)(ndis_handle);

typedef ndis_status (*driver_entry)(void *, ndis_unicode_string *);

extern image_patch_table ndis_functbl[];

__BEGIN_DECLS
extern int ndis_libinit(void);
extern int ndis_libfini(void);
extern int ndis_ascii_to_unicode(char *, uint16_t **);
extern int ndis_unicode_to_ascii(uint16_t *, int, char **);
extern int ndis_load_driver(vm_offset_t, void *);
extern int ndis_unload_driver(void *);
extern int ndis_mtop(struct mbuf *, ndis_packet **);
extern int ndis_ptom(struct mbuf **, ndis_packet *);
extern int ndis_get_info(void *, ndis_oid, void *, int *);
extern int ndis_set_info(void *, ndis_oid, void *, int *);
extern int ndis_get_supported_oids(void *, ndis_oid **, int *);
extern int ndis_send_packets(void *, ndis_packet **, int);
extern int ndis_send_packet(void *, ndis_packet *);
extern int ndis_convert_res(void *);
extern int ndis_alloc_amem(void *);
extern void ndis_free_packet(ndis_packet *);
extern void ndis_free_bufs(ndis_buffer *);
extern int ndis_reset_nic(void *);
extern int ndis_halt_nic(void *);
extern int ndis_shutdown_nic(void *);
extern int ndis_init_nic(void *);
extern int ndis_isr(void *, int *, int *);
extern int ndis_intrhand(void *);
extern void ndis_return_packet(void *, void *);
extern void ndis_enable_intr(void *);
extern void ndis_disable_intr(void *);
extern int ndis_init_dma(void *);
extern int ndis_destroy_dma(void *);
extern int ndis_create_sysctls(void *);
extern int ndis_add_sysctl(void *, char *, char *, char *, int);
extern int ndis_flush_sysctls(void *);
__END_DECLS

#endif /* _NDIS_VAR_H_ */
