/** @file
  This file defines the EFI IPv6 (Internet Protocol version 6)
  Protocol interface. It is split into the following three main
  sections:
  - EFI IPv6 Service Binding Protocol
  - EFI IPv6 Variable (deprecated in UEFI 2.4B)
  - EFI IPv6 Protocol
  The EFI IPv6 Protocol provides basic network IPv6 packet I/O services,
  which includes support for Neighbor Discovery Protocol (ND), Multicast
  Listener Discovery Protocol (MLD), and a subset of the Internet Control
  Message Protocol (ICMPv6).

  Copyright (c) 2008 - 2014, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.2

**/

#ifndef __EFI_IP6_PROTOCOL_H__
#define __EFI_IP6_PROTOCOL_H__

#include <Protocol/ManagedNetwork.h>


#define EFI_IP6_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0xec835dd3, 0xfe0f, 0x617b, {0xa6, 0x21, 0xb3, 0x50, 0xc3, 0xe1, 0x33, 0x88 } \
  }

#define EFI_IP6_PROTOCOL_GUID \
  { \
    0x2c8759d5, 0x5c2d, 0x66ef, {0x92, 0x5f, 0xb6, 0x6c, 0x10, 0x19, 0x57, 0xe2 } \
  }

typedef struct _EFI_IP6_PROTOCOL EFI_IP6_PROTOCOL;

///
/// EFI_IP6_ADDRESS_PAIR is deprecated in the UEFI 2.4B and should not be used any more.
/// The definition in here is only present to provide backwards compatability.
///
typedef struct{
  ///
  /// The EFI IPv6 Protocol instance handle that is using this address/prefix pair.
  ///
  EFI_HANDLE          InstanceHandle;
  ///
  /// IPv6 address in network byte order.
  ///
  EFI_IPv6_ADDRESS    Ip6Address;
  ///
  /// The length of the prefix associated with the Ip6Address.
  ///
  UINT8               PrefixLength;
} EFI_IP6_ADDRESS_PAIR;

///
/// EFI_IP6_VARIABLE_DATA is deprecated in the UEFI 2.4B and should not be used any more.
/// The definition in here is only present to provide backwards compatability.
///
typedef struct {
  ///
  /// The handle of the driver that creates this entry.
  ///
  EFI_HANDLE              DriverHandle;
  ///
  /// The number of IPv6 address pairs that follow this data structure.
  ///
  UINT32                  AddressCount;
  ///
  /// List of IPv6 address pairs that are currently in use.
  ///
  EFI_IP6_ADDRESS_PAIR    AddressPairs[1];
} EFI_IP6_VARIABLE_DATA;

///
/// ICMPv6 type definitions for error messages
///
///@{
#define ICMP_V6_DEST_UNREACHABLE                 0x1
#define ICMP_V6_PACKET_TOO_BIG                   0x2
#define ICMP_V6_TIME_EXCEEDED                    0x3
#define ICMP_V6_PARAMETER_PROBLEM                0x4
///@}

///
/// ICMPv6 type definition for informational messages
///
///@{
#define ICMP_V6_ECHO_REQUEST                     0x80
#define ICMP_V6_ECHO_REPLY                       0x81
#define ICMP_V6_LISTENER_QUERY                   0x82
#define ICMP_V6_LISTENER_REPORT                  0x83
#define ICMP_V6_LISTENER_DONE                    0x84
#define ICMP_V6_ROUTER_SOLICIT                   0x85
#define ICMP_V6_ROUTER_ADVERTISE                 0x86
#define ICMP_V6_NEIGHBOR_SOLICIT                 0x87
#define ICMP_V6_NEIGHBOR_ADVERTISE               0x88
#define ICMP_V6_REDIRECT                         0x89
#define ICMP_V6_LISTENER_REPORT_2                0x8F
///@}

///
/// ICMPv6 code definitions for ICMP_V6_DEST_UNREACHABLE
///
///@{
#define ICMP_V6_NO_ROUTE_TO_DEST                 0x0
#define ICMP_V6_COMM_PROHIBITED                  0x1
#define ICMP_V6_BEYOND_SCOPE                     0x2
#define ICMP_V6_ADDR_UNREACHABLE                 0x3
#define ICMP_V6_PORT_UNREACHABLE                 0x4
#define ICMP_V6_SOURCE_ADDR_FAILED               0x5
#define ICMP_V6_ROUTE_REJECTED                   0x6
///@}

///
/// ICMPv6 code definitions for ICMP_V6_TIME_EXCEEDED
///
///@{
#define ICMP_V6_TIMEOUT_HOP_LIMIT                0x0
#define ICMP_V6_TIMEOUT_REASSEMBLE               0x1
///@}

///
/// ICMPv6 code definitions for ICMP_V6_PARAMETER_PROBLEM
///
///@{
#define ICMP_V6_ERRONEOUS_HEADER                 0x0
#define ICMP_V6_UNRECOGNIZE_NEXT_HDR             0x1
#define ICMP_V6_UNRECOGNIZE_OPTION               0x2
///@}

///
/// EFI_IP6_CONFIG_DATA
/// is used to report and change IPv6 session parameters.
///
typedef struct {
  ///
  /// For the IPv6 packet to send and receive, this is the default value
  /// of the 'Next Header' field in the last IPv6 extension header or in
  /// the IPv6 header if there are no extension headers. Ignored when
  /// AcceptPromiscuous is TRUE.
  ///
  UINT8                   DefaultProtocol;
  ///
  /// Set to TRUE to receive all IPv6 packets that get through the
  /// receive filters.
  /// Set to FALSE to receive only the DefaultProtocol IPv6
  /// packets that get through the receive filters. Ignored when
  /// AcceptPromiscuous is TRUE.
  ///
  BOOLEAN                 AcceptAnyProtocol;
  ///
  /// Set to TRUE to receive ICMP error report packets. Ignored when
  /// AcceptPromiscuous or AcceptAnyProtocol is TRUE.
  ///
  BOOLEAN                 AcceptIcmpErrors;
  ///
  /// Set to TRUE to receive all IPv6 packets that are sent to any
  /// hardware address or any protocol address. Set to FALSE to stop
  /// receiving all promiscuous IPv6 packets.
  ///
  BOOLEAN                 AcceptPromiscuous;
  ///
  /// The destination address of the packets that will be transmitted.
  /// Ignored if it is unspecified.
  ///
  EFI_IPv6_ADDRESS        DestinationAddress;
  ///
  /// The station IPv6 address that will be assigned to this EFI IPv6
  /// Protocol instance. This field can be set and changed only when
  /// the EFI IPv6 driver is transitioning from the stopped to the started
  /// states. If the StationAddress is specified, the EFI IPv6 Protocol
  /// driver will deliver only incoming IPv6 packets whose destination
  /// matches this IPv6 address exactly. The StationAddress is required
  /// to be one of currently configured IPv6 addresses. An address
  /// containing all zeroes is also accepted as a special case. Under this
  /// situation, the IPv6 driver is responsible for binding a source
  /// address to this EFI IPv6 protocol instance according to the source
  /// address selection algorithm. Only incoming packets destined to
  /// the selected address will be delivered to the user.  And the
  /// selected station address can be retrieved through later
  /// GetModeData() call. If no address is available for selecting,
  /// EFI_NO_MAPPING will be returned, and the station address will
  /// only be successfully bound to this EFI IPv6 protocol instance
  /// after IP6ModeData.IsConfigured changed to TRUE.
  ///
  EFI_IPv6_ADDRESS        StationAddress;
  ///
  /// TrafficClass field in transmitted IPv6 packets. Default value
  /// is zero.
  ///
  UINT8                   TrafficClass;
  ///
  /// HopLimit field in transmitted IPv6 packets.
  ///
  UINT8                   HopLimit;
  ///
  /// FlowLabel field in transmitted IPv6 packets. Default value is
  /// zero.
  ///
  UINT32                  FlowLabel;
  ///
  /// The timer timeout value (number of microseconds) for the
  /// receive timeout event to be associated with each assembled
  /// packet. Zero means do not drop assembled packets.
  ///
  UINT32                  ReceiveTimeout;
  ///
  /// The timer timeout value (number of microseconds) for the
  /// transmit timeout event to be associated with each outgoing
  /// packet. Zero means do not drop outgoing packets.
  ///
  UINT32                  TransmitTimeout;
} EFI_IP6_CONFIG_DATA;

///
/// EFI_IP6_ADDRESS_INFO
///
typedef struct {
  EFI_IPv6_ADDRESS        Address;       ///< The IPv6 address.
  UINT8                   PrefixLength;  ///< The length of the prefix associated with the Address.
} EFI_IP6_ADDRESS_INFO;

///
/// EFI_IP6_ROUTE_TABLE
/// is the entry structure that is used in routing tables
///
typedef struct {
  ///
  /// The IPv6 address of the gateway to be used as the next hop for
  /// packets to this prefix. If the IPv6 address is all zeros, then the
  /// prefix is on-link.
  ///
  EFI_IPv6_ADDRESS        Gateway;
  ///
  /// The destination prefix to be routed.
  ///
  EFI_IPv6_ADDRESS        Destination;
  ///
  /// The length of the prefix associated with the Destination.
  ///
  UINT8                   PrefixLength;
} EFI_IP6_ROUTE_TABLE;

///
/// EFI_IP6_NEIGHBOR_STATE
///
typedef enum {
  ///
  /// Address resolution is being performed on this entry. Specially,
  /// Neighbor Solicitation has been sent to the solicited-node
  /// multicast address of the target, but corresponding Neighbor
  /// Advertisement has not been received.
  ///
  EfiNeighborInComplete,
  ///
  /// Positive confirmation was received that the forward path to the
  /// neighbor was functioning properly.
  ///
  EfiNeighborReachable,
  ///
  ///Reachable Time has elapsed since the last positive confirmation
  ///was received. In this state, the forward path to the neighbor was
  ///functioning properly.
  ///
  EfiNeighborStale,
  ///
  /// This state is an optimization that gives upper-layer protocols
  /// additional time to provide reachability confirmation.
  ///
  EfiNeighborDelay,
  ///
  /// A reachability confirmation is actively sought by retransmitting
  /// Neighbor Solicitations every RetransTimer milliseconds until a
  /// reachability confirmation is received.
  ///
  EfiNeighborProbe
} EFI_IP6_NEIGHBOR_STATE;

///
/// EFI_IP6_NEIGHBOR_CACHE
/// is the entry structure that is used in neighbor cache. It records a set
/// of entries about individual neighbors to which traffic has been sent recently.
///
typedef struct {
  EFI_IPv6_ADDRESS        Neighbor;    ///< The on-link unicast/anycast IP address of the neighbor.
  EFI_MAC_ADDRESS         LinkAddress; ///< Link-layer address of the neighbor.
  EFI_IP6_NEIGHBOR_STATE  State;       ///< State of this neighbor cache entry.
} EFI_IP6_NEIGHBOR_CACHE;

///
/// EFI_IP6_ICMP_TYPE
/// is used to describe those ICMP messages that are supported by this EFI
/// IPv6 Protocol driver.
///
typedef struct {
  UINT8                   Type;   ///< The type of ICMP message.
  UINT8                   Code;   ///< The code of the ICMP message.
} EFI_IP6_ICMP_TYPE;

///
/// EFI_IP6_MODE_DATA
///
typedef struct {
  ///
  /// Set to TRUE after this EFI IPv6 Protocol instance is started.
  /// All other fields in this structure are undefined until this field is TRUE.
  /// Set to FALSE when the EFI IPv6 Protocol instance is stopped.
  ///
  BOOLEAN                 IsStarted;
  ///
  /// The maximum packet size, in bytes, of the packet which the upper layer driver could feed.
  ///
  UINT32                  MaxPacketSize;
  ///
  /// Current configuration settings. Undefined until IsStarted is TRUE.
  ///
  EFI_IP6_CONFIG_DATA     ConfigData;
  ///
  /// Set to TRUE when the EFI IPv6 Protocol instance is configured.
  /// The instance is configured when it has a station address and
  /// corresponding prefix length.
  /// Set to FALSE when the EFI IPv6 Protocol instance is not configured.
  ///
  BOOLEAN                 IsConfigured;
  ///
  /// Number of configured IPv6 addresses on this interface.
  ///
  UINT32                  AddressCount;
  ///
  /// List of currently configured IPv6 addresses and corresponding
  /// prefix lengths assigned to this interface. It is caller's
  /// responsibility to free this buffer.
  ///
  EFI_IP6_ADDRESS_INFO    *AddressList;
  ///
  /// Number of joined multicast groups. Undefined until
  /// IsConfigured is TRUE.
  ///
  UINT32                  GroupCount;
  ///
  /// List of joined multicast group addresses. It is caller's
  /// responsibility to free this buffer. Undefined until
  /// IsConfigured is TRUE.
  ///
  EFI_IPv6_ADDRESS        *GroupTable;
  ///
  /// Number of entries in the routing table. Undefined until
  /// IsConfigured is TRUE.
  ///
  UINT32                  RouteCount;
  ///
  /// Routing table entries. It is caller's responsibility to free this buffer.
  ///
  EFI_IP6_ROUTE_TABLE     *RouteTable;
  ///
  /// Number of entries in the neighbor cache. Undefined until
  /// IsConfigured is TRUE.
  ///
  UINT32                  NeighborCount;
  ///
  /// Neighbor cache entries. It is caller's responsibility to free this
  /// buffer. Undefined until IsConfigured is TRUE.
  ///
  EFI_IP6_NEIGHBOR_CACHE  *NeighborCache;
  ///
  /// Number of entries in the prefix table. Undefined until
  /// IsConfigured is TRUE.
  ///
  UINT32                  PrefixCount;
  ///
  /// On-link Prefix table entries. It is caller's responsibility to free this
  /// buffer. Undefined until IsConfigured is TRUE.
  ///
  EFI_IP6_ADDRESS_INFO    *PrefixTable;
  ///
  /// Number of entries in the supported ICMP types list.
  ///
  UINT32                  IcmpTypeCount;
  ///
  /// Array of ICMP types and codes that are supported by this EFI
  /// IPv6 Protocol driver. It is caller's responsibility to free this
  /// buffer.
  ///
  EFI_IP6_ICMP_TYPE       *IcmpTypeList;
} EFI_IP6_MODE_DATA;

///
/// EFI_IP6_HEADER
/// The fields in the IPv6 header structure are defined in the Internet
/// Protocol version6 specification.
///
#pragma pack(1)
typedef struct _EFI_IP6_HEADER {
  UINT8                   TrafficClassH:4;
  UINT8                   Version:4;
  UINT8                   FlowLabelH:4;
  UINT8                   TrafficClassL:4;
  UINT16                  FlowLabelL;
  UINT16                  PayloadLength;
  UINT8                   NextHeader;
  UINT8                   HopLimit;
  EFI_IPv6_ADDRESS        SourceAddress;
  EFI_IPv6_ADDRESS        DestinationAddress;
} EFI_IP6_HEADER;
#pragma pack()

///
/// EFI_IP6_FRAGMENT_DATA
/// describes the location and length of the IPv6 packet
/// fragment to transmit or that has been received.
///
typedef struct _EFI_IP6_FRAGMENT_DATA {
  UINT32                  FragmentLength;  ///< Length of fragment data. This field may not be set to zero.
  VOID                    *FragmentBuffer; ///< Pointer to fragment data. This field may not be set to NULL.
} EFI_IP6_FRAGMENT_DATA;

///
/// EFI_IP6_RECEIVE_DATA
///
typedef struct _EFI_IP6_RECEIVE_DATA {
  ///
  /// Time when the EFI IPv6 Protocol driver accepted the packet.
  /// Ignored if it is zero.
  ///
  EFI_TIME                TimeStamp;
  ///
  /// After this event is signaled, the receive data structure is released
  /// and must not be referenced.
  ///
  EFI_EVENT               RecycleSignal;
  ///
  ///Length of the IPv6 packet headers, including both the IPv6
  ///header and any extension headers.
  ///
  UINT32                  HeaderLength;
  ///
  /// Pointer to the IPv6 packet header. If the IPv6 packet was
  /// fragmented, this argument is a pointer to the header in the first
  /// fragment.
  ///
  EFI_IP6_HEADER          *Header;
  ///
  /// Sum of the lengths of IPv6 packet buffers in FragmentTable. May
  /// be zero.
  ///
  UINT32                  DataLength;
  ///
  /// Number of IPv6 payload fragments. May be zero.
  ///
  UINT32                  FragmentCount;
  ///
  /// Array of payload fragment lengths and buffer pointers.
  ///
  EFI_IP6_FRAGMENT_DATA   FragmentTable[1];
} EFI_IP6_RECEIVE_DATA;

///
/// EFI_IP6_OVERRIDE_DATA
/// The information and flags in the override data structure will override
/// default parameters or settings for one Transmit() function call.
///
typedef struct _EFI_IP6_OVERRIDE_DATA {
  UINT8                   Protocol;   ///< Protocol type override.
  UINT8                   HopLimit;   ///< Hop-Limit override.
  UINT32                  FlowLabel;  ///< Flow-Label override.
} EFI_IP6_OVERRIDE_DATA;

///
/// EFI_IP6_TRANSMIT_DATA
///
typedef struct _EFI_IP6_TRANSMIT_DATA {
  ///
  /// The destination IPv6 address.  If it is unspecified,
  /// ConfigData.DestinationAddress will be used instead.
  ///
  EFI_IPv6_ADDRESS        DestinationAddress;
  ///
  /// If not NULL, the IPv6 transmission control override data.
  ///
  EFI_IP6_OVERRIDE_DATA   *OverrideData;
  ///
  /// Total length in byte of the IPv6 extension headers specified in
  /// ExtHdrs.
  ///
  UINT32                  ExtHdrsLength;
  ///
  /// Pointer to the IPv6 extension headers. The IP layer will append
  /// the required extension headers if they are not specified by
  /// ExtHdrs. Ignored if ExtHdrsLength is zero.
  ///
  VOID                    *ExtHdrs;
  ///
  /// The protocol of first extension header in ExtHdrs. Ignored if
  /// ExtHdrsLength is zero.
  ///
  UINT8                   NextHeader;
  ///
  /// Total length in bytes of the FragmentTable data to transmit.
  ///
  UINT32                  DataLength;
  ///
  /// Number of entries in the fragment data table.
  ///
  UINT32                  FragmentCount;
  ///
  /// Start of the fragment data table.
  ///
  EFI_IP6_FRAGMENT_DATA   FragmentTable[1];
} EFI_IP6_TRANSMIT_DATA;

///
/// EFI_IP6_COMPLETION_TOKEN
/// structures are used for both transmit and receive operations.
///
typedef struct {
  ///
  /// This Event will be signaled after the Status field is updated by
  /// the EFI IPv6 Protocol driver. The type of Event must be EFI_NOTIFY_SIGNAL.
  ///
  EFI_EVENT               Event;
  ///
  /// Will be set to one of the following values:
  /// - EFI_SUCCESS:  The receive or transmit completed
  ///   successfully.
  /// - EFI_ABORTED:  The receive or transmit was aborted
  /// - EFI_TIMEOUT:  The transmit timeout expired.
  /// - EFI_ICMP_ERROR:  An ICMP error packet was received.
  /// - EFI_DEVICE_ERROR:  An unexpected system or network
  ///   error occurred.
  /// - EFI_SECURITY_VIOLATION: The transmit or receive was
  ///   failed because of an IPsec policy check.
  /// - EFI_NO_MEDIA: There was a media error.
  ///
  EFI_STATUS              Status;
  union {
    ///
    /// When the Token is used for receiving, RxData is a pointer to the EFI_IP6_RECEIVE_DATA.
    ///
    EFI_IP6_RECEIVE_DATA  *RxData;
    ///
    /// When the Token is used for transmitting, TxData is a pointer to the EFI_IP6_TRANSMIT_DATA.
    ///
    EFI_IP6_TRANSMIT_DATA *TxData;
  } Packet;
} EFI_IP6_COMPLETION_TOKEN;

/**
  Gets the current operational settings for this instance of the EFI IPv6 Protocol driver.

  The GetModeData() function returns the current operational mode data for this driver instance.
  The data fields in EFI_IP6_MODE_DATA are read only. This function is used optionally to
  retrieve the operational mode data of underlying networks or drivers..

  @param[in]  This               Pointer to the EFI_IP6_PROTOCOL instance.
  @param[out] Ip6ModeData        Pointer to the EFI IPv6 Protocol mode data structure.
  @param[out] MnpConfigData      Pointer to the managed network configuration data structure.
  @param[out] SnpModeData        Pointer to the simple network mode data structure.

  @retval EFI_SUCCESS            The operation completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_OUT_OF_RESOURCES   The required mode data could not be allocated.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP6_GET_MODE_DATA)(
  IN EFI_IP6_PROTOCOL                 *This,
  OUT EFI_IP6_MODE_DATA               *Ip6ModeData     OPTIONAL,
  OUT EFI_MANAGED_NETWORK_CONFIG_DATA *MnpConfigData   OPTIONAL,
  OUT EFI_SIMPLE_NETWORK_MODE         *SnpModeData     OPTIONAL
  );

/**
  Assigns an IPv6 address and subnet mask to this EFI IPv6 Protocol driver instance.

  The Configure() function is used to set, change, or reset the operational parameters and filter
  settings for this EFI IPv6 Protocol instance. Until these parameters have been set, no network traffic
  can be sent or received by this instance. Once the parameters have been reset (by calling this
  function with Ip6ConfigData set to NULL), no more traffic can be sent or received until these
  parameters have been set again. Each EFI IPv6 Protocol instance can be started and stopped
  independently of each other by enabling or disabling their receive filter settings with the
  Configure() function.

  If Ip6ConfigData.StationAddress is a valid non-zero IPv6 unicast address, it is required
  to be one of the currently configured IPv6 addresses list in the EFI IPv6 drivers, or else
  EFI_INVALID_PARAMETER will be returned. If Ip6ConfigData.StationAddress is
  unspecified, the IPv6 driver will bind a source address according to the source address selection
  algorithm. Clients could frequently call GetModeData() to check get currently configured IPv6
  address list in the EFI IPv6 driver. If both Ip6ConfigData.StationAddress and
  Ip6ConfigData.Destination are unspecified, when transmitting the packet afterwards, the
  source address filled in each outgoing IPv6 packet is decided based on the destination of this packet. .

  If operational parameters are reset or changed, any pending transmit and receive requests will be
  cancelled. Their completion token status will be set to EFI_ABORTED and their events will be
  signaled.

  @param[in]  This               Pointer to the EFI_IP6_PROTOCOL instance.
  @param[in]  Ip6ConfigData      Pointer to the EFI IPv6 Protocol configuration data structure.

  @retval EFI_SUCCESS            The driver instance was successfully opened.
  @retval EFI_INVALID_PARAMETER  One or more of the following conditions is TRUE:
                                 - This is NULL.
                                 - Ip6ConfigData.StationAddress is neither zero nor
                                   a unicast IPv6 address.
                                 - Ip6ConfigData.StationAddress is neither zero nor
                                   one of the configured IP addresses in the EFI IPv6 driver.
                                 - Ip6ConfigData.DefaultProtocol is illegal.
  @retval EFI_OUT_OF_RESOURCES   The EFI IPv6 Protocol driver instance data could not be allocated.
  @retval EFI_NO_MAPPING         The IPv6 driver was responsible for choosing a source address for
                                 this instance, but no source address was available for use.
  @retval EFI_ALREADY_STARTED    The interface is already open and must be stopped before the IPv6
                                 address or prefix length can be changed.
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred. The EFI IPv6
                                 Protocol driver instance is not opened.
  @retval EFI_UNSUPPORTED        Default protocol specified through
                                 Ip6ConfigData.DefaulProtocol isn't supported.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP6_CONFIGURE)(
  IN EFI_IP6_PROTOCOL            *This,
  IN EFI_IP6_CONFIG_DATA         *Ip6ConfigData OPTIONAL
  );

/**
  Joins and leaves multicast groups.

  The Groups() function is used to join and leave multicast group sessions. Joining a group will
  enable reception of matching multicast packets. Leaving a group will disable reception of matching
  multicast packets. Source-Specific Multicast isn't required to be supported.

  If JoinFlag is FALSE and GroupAddress is NULL, all joined groups will be left.

  @param[in]  This               Pointer to the EFI_IP6_PROTOCOL instance.
  @param[in]  JoinFlag           Set to TRUE to join the multicast group session and FALSE to leave.
  @param[in]  GroupAddress       Pointer to the IPv6 multicast address.

  @retval EFI_SUCCESS            The operation completed successfully.
  @retval EFI_INVALID_PARAMETER  One or more of the following is TRUE:
                                 - This is NULL.
                                 - JoinFlag is TRUE and GroupAddress is NULL.
                                 - GroupAddress is not NULL and *GroupAddress is
                                   not a multicast IPv6 address.
                                 - GroupAddress is not NULL and *GroupAddress is in the
                                   range of SSM destination address.
  @retval EFI_NOT_STARTED        This instance has not been started.
  @retval EFI_OUT_OF_RESOURCES   System resources could not be allocated.
  @retval EFI_UNSUPPORTED        This EFI IPv6 Protocol implementation does not support multicast groups.
  @retval EFI_ALREADY_STARTED    The group address is already in the group table (when
                                 JoinFlag is TRUE).
  @retval EFI_NOT_FOUND          The group address is not in the group table (when JoinFlag is FALSE).
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP6_GROUPS)(
  IN EFI_IP6_PROTOCOL            *This,
  IN BOOLEAN                     JoinFlag,
  IN EFI_IPv6_ADDRESS            *GroupAddress  OPTIONAL
  );

/**
  Adds and deletes routing table entries.

  The Routes() function adds a route to or deletes a route from the routing table.

  Routes are determined by comparing the leftmost PrefixLength bits of Destination with
  the destination IPv6 address arithmetically. The gateway address must be on the same subnet as the
  configured station address.

  The default route is added with Destination and PrefixLegth both set to all zeros. The
  default route matches all destination IPv6 addresses that do not match any other routes.

  All EFI IPv6 Protocol instances share a routing table.

  @param[in]  This               Pointer to the EFI_IP6_PROTOCOL instance.
  @param[in]  DeleteRoute        Set to TRUE to delete this route from the routing table. Set to
                                 FALSE to add this route to the routing table. Destination,
                                 PrefixLength and Gateway are used as the key to each
                                 route entry.
  @param[in]  Destination        The address prefix of the subnet that needs to be routed.
  @param[in]  PrefixLength       The prefix length of Destination. Ignored if Destination
                                 is NULL.
  @param[in]  GatewayAddress     The unicast gateway IPv6 address for this route.

  @retval EFI_SUCCESS            The operation completed successfully.
  @retval EFI_NOT_STARTED        The driver instance has not been started.
  @retval EFI_INVALID_PARAMETER  One or more of the following conditions is TRUE:
                                 - This is NULL.
                                 - When DeleteRoute is TRUE, both Destination and
                                   GatewayAddress are NULL.
                                 - When DeleteRoute is FALSE, either Destination or
                                   GatewayAddress is NULL.
                                 - *GatewayAddress is not a valid unicast IPv6 address.
                                 - *GatewayAddress is one of the local configured IPv6
                                   addresses.
  @retval EFI_OUT_OF_RESOURCES   Could not add the entry to the routing table.
  @retval EFI_NOT_FOUND          This route is not in the routing table (when DeleteRoute is TRUE).
  @retval EFI_ACCESS_DENIED      The route is already defined in the routing table (when
                                 DeleteRoute is FALSE).

**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP6_ROUTES)(
  IN EFI_IP6_PROTOCOL            *This,
  IN BOOLEAN                     DeleteRoute,
  IN EFI_IPv6_ADDRESS            *Destination OPTIONAL,
  IN UINT8                       PrefixLength,
  IN EFI_IPv6_ADDRESS            *GatewayAddress OPTIONAL
  );

/**
  Add or delete Neighbor cache entries.

  The Neighbors() function is used to add, update, or delete an entry from neighbor cache.
  IPv6 neighbor cache entries are typically inserted and updated by the network protocol driver as
  network traffic is processed. Most neighbor cache entries will time out and be deleted if the network
  traffic stops. Neighbor cache entries that were inserted by Neighbors() may be static (will not
  timeout) or dynamic (will time out).

  The implementation should follow the neighbor cache timeout mechanism which is defined in
  RFC4861. The default neighbor cache timeout value should be tuned for the expected network
  environment

  @param[in]  This               Pointer to the EFI_IP6_PROTOCOL instance.
  @param[in]  DeleteFlag         Set to TRUE to delete the specified cache entry, set to FALSE to
                                 add (or update, if it already exists and Override is TRUE) the
                                 specified cache entry. TargetIp6Address is used as the key
                                 to find the requested cache entry.
  @param[in]  TargetIp6Address   Pointer to Target IPv6 address.
  @param[in]  TargetLinkAddress  Pointer to link-layer address of the target. Ignored if NULL.
  @param[in]  Timeout            Time in 100-ns units that this entry will remain in the neighbor
                                 cache, it will be deleted after Timeout. A value of zero means that
                                 the entry is permanent. A non-zero value means that the entry is
                                 dynamic.
  @param[in]  Override           If TRUE, the cached link-layer address of the matching entry will
                                 be overridden and updated; if FALSE, EFI_ACCESS_DENIED
                                 will be returned if a corresponding cache entry already existed.

  @retval  EFI_SUCCESS           The data has been queued for transmission.
  @retval  EFI_NOT_STARTED       This instance has not been started.
  @retval  EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                 - This is NULL.
                                 - TargetIpAddress is NULL.
                                 - *TargetLinkAddress is invalid when not NULL.
                                 - *TargetIpAddress is not a valid unicast IPv6 address.
                                 - *TargetIpAddress is one of the local configured IPv6
                                   addresses.
  @retval  EFI_OUT_OF_RESOURCES  Could not add the entry to the neighbor cache.
  @retval  EFI_NOT_FOUND         This entry is not in the neighbor cache (when DeleteFlag  is
                                 TRUE or when DeleteFlag  is FALSE while
                                 TargetLinkAddress is NULL.).
  @retval  EFI_ACCESS_DENIED     The to-be-added entry is already defined in the neighbor cache,
                                 and that entry is tagged as un-overridden (when DeleteFlag
                                 is FALSE).

**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP6_NEIGHBORS)(
  IN EFI_IP6_PROTOCOL            *This,
  IN BOOLEAN                     DeleteFlag,
  IN EFI_IPv6_ADDRESS            *TargetIp6Address,
  IN EFI_MAC_ADDRESS             *TargetLinkAddress,
  IN UINT32                      Timeout,
  IN BOOLEAN                     Override
  );

/**
  Places outgoing data packets into the transmit queue.

  The Transmit() function places a sending request in the transmit queue of this
  EFI IPv6 Protocol instance. Whenever the packet in the token is sent out or some
  errors occur, the event in the token will be signaled and the status is updated.

  @param[in]  This               Pointer to the EFI_IP6_PROTOCOL instance.
  @param[in]  Token              Pointer to the transmit token.

  @retval  EFI_SUCCESS           The data has been queued for transmission.
  @retval  EFI_NOT_STARTED       This instance has not been started.
  @retval  EFI_NO_MAPPING        The IPv6 driver was responsible for choosing a source address for
                                 this transmission, but no source address was available for use.
  @retval  EFI_INVALID_PARAMETER One or more of the following is TRUE:
                                 - This is NULL.
                                 - Token is NULL.
                                 - Token.Event is NULL.
                                 - Token.Packet.TxData is NULL.
                                 - Token.Packet.ExtHdrsLength is not zero and Token.Packet.ExtHdrs is NULL.
                                 - Token.Packet.FragmentCount is zero.
                                 - One or more of the Token.Packet.TxData.FragmentTable[].FragmentLength fields is zero.
                                 - One or more of the Token.Packet.TxData.FragmentTable[].FragmentBuffer fields is NULL.
                                 - Token.Packet.TxData.DataLength is zero or not equal to the sum of fragment lengths.
                                 - Token.Packet.TxData.DestinationAddress is non-zero when DestinationAddress is configured as
                                   non-zero when doing Configure() for this EFI IPv6 protocol instance.
                                 - Token.Packet.TxData.DestinationAddress is unspecified when DestinationAddress is unspecified
                                   when doing Configure() for this EFI IPv6 protocol instance.
  @retval  EFI_ACCESS_DENIED     The transmit completion token with the same Token.Event
                                 was already in the transmit queue.
  @retval  EFI_NOT_READY         The completion token could not be queued because the transmit
                                 queue is full.
  @retval  EFI_NOT_FOUND         Not route is found to destination address.
  @retval  EFI_OUT_OF_RESOURCES  Could not queue the transmit data.
  @retval  EFI_BUFFER_TOO_SMALL  Token.Packet.TxData.TotalDataLength is too
                                 short to transmit.
  @retval  EFI_BAD_BUFFER_SIZE   If Token.Packet.TxData.DataLength is beyond the
                                 maximum that which can be described through the Fragment Offset
                                 field in Fragment header when performing fragmentation.
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP6_TRANSMIT)(
  IN EFI_IP6_PROTOCOL            *This,
  IN EFI_IP6_COMPLETION_TOKEN    *Token
  );

/**
  Places a receiving request into the receiving queue.

  The Receive() function places a completion token into the receive packet queue.
  This function is always asynchronous.

  The Token.Event field in the completion token must be filled in by the caller
  and cannot be NULL. When the receive operation completes, the EFI IPv6 Protocol
  driver updates the Token.Status and Token.Packet.RxData fields and the Token.Event
  is signaled.

  @param[in]  This               Pointer to the EFI_IP6_PROTOCOL instance.
  @param[in]  Token              Pointer to a token that is associated with the receive data descriptor.

  @retval EFI_SUCCESS            The receive completion token was cached.
  @retval EFI_NOT_STARTED        This EFI IPv6 Protocol instance has not been started.
  @retval EFI_NO_MAPPING         When IP6 driver responsible for binding source address to this instance,
                                 while no source address is available for use.
  @retval EFI_INVALID_PARAMETER  One or more of the following conditions is TRUE:
                                 - This is NULL.
                                 - Token is NULL.
                                 - Token.Event is NULL.
  @retval EFI_OUT_OF_RESOURCES   The receive completion token could not be queued due to a lack of system
                                 resources (usually memory).
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred.
                                 The EFI IPv6 Protocol instance has been reset to startup defaults.
  @retval EFI_ACCESS_DENIED      The receive completion token with the same Token.Event was already
                                 in the receive queue.
  @retval EFI_NOT_READY          The receive request could not be queued because the receive queue is full.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP6_RECEIVE)(
  IN EFI_IP6_PROTOCOL            *This,
  IN EFI_IP6_COMPLETION_TOKEN    *Token
  );

/**
  Abort an asynchronous transmit or receive request.

  The Cancel() function is used to abort a pending transmit or receive request.
  If the token is in the transmit or receive request queues, after calling this
  function, Token->Status will be set to EFI_ABORTED and then Token->Event will
  be signaled. If the token is not in one of the queues, which usually means the
  asynchronous operation has completed, this function will not signal the token
  and EFI_NOT_FOUND is returned.

  @param[in]  This               Pointer to the EFI_IP6_PROTOCOL instance.
  @param[in]  Token              Pointer to a token that has been issued by
                                 EFI_IP6_PROTOCOL.Transmit() or
                                 EFI_IP6_PROTOCOL.Receive(). If NULL, all pending
                                 tokens are aborted. Type EFI_IP6_COMPLETION_TOKEN is
                                 defined in EFI_IP6_PROTOCOL.Transmit().

  @retval EFI_SUCCESS            The asynchronous I/O request was aborted and
                                 Token->Event was signaled. When Token is NULL, all
                                 pending requests were aborted and their events were signaled.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_NOT_STARTED        This instance has not been started.
  @retval EFI_NOT_FOUND          When Token is not NULL, the asynchronous I/O request was
                                 not found in the transmit or receive queue. It has either completed
                                 or was not issued by Transmit() and Receive().
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP6_CANCEL)(
  IN EFI_IP6_PROTOCOL            *This,
  IN EFI_IP6_COMPLETION_TOKEN    *Token    OPTIONAL
  );

/**
  Polls for incoming data packets and processes outgoing data packets.

  The Poll() function polls for incoming data packets and processes outgoing data
  packets. Network drivers and applications can call the EFI_IP6_PROTOCOL.Poll()
  function to increase the rate that data packets are moved between the communications
  device and the transmit and receive queues.

  In some systems the periodic timer event may not poll the underlying communications
  device fast enough to transmit and/or receive all data packets without missing
  incoming packets or dropping outgoing packets. Drivers and applications that are
  experiencing packet loss should try calling the EFI_IP6_PROTOCOL.Poll() function
  more often.

  @param[in]  This               Pointer to the EFI_IP6_PROTOCOL instance.

  @retval  EFI_SUCCESS           Incoming or outgoing data was processed.
  @retval  EFI_NOT_STARTED       This EFI IPv6 Protocol instance has not been started.
  @retval  EFI_INVALID_PARAMETER This is NULL.
  @retval  EFI_DEVICE_ERROR      An unexpected system or network error occurred.
  @retval  EFI_NOT_READY         No incoming or outgoing data is processed.
  @retval  EFI_TIMEOUT           Data was dropped out of the transmit and/or receive queue.
                                 Consider increasing the polling rate.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP6_POLL)(
  IN EFI_IP6_PROTOCOL            *This
  );

///
/// The EFI IPv6 Protocol implements a simple packet-oriented interface that can be
/// used by drivers, daemons, and applications to transmit and receive network packets.
///
struct _EFI_IP6_PROTOCOL {
  EFI_IP6_GET_MODE_DATA   GetModeData;
  EFI_IP6_CONFIGURE       Configure;
  EFI_IP6_GROUPS          Groups;
  EFI_IP6_ROUTES          Routes;
  EFI_IP6_NEIGHBORS       Neighbors;
  EFI_IP6_TRANSMIT        Transmit;
  EFI_IP6_RECEIVE         Receive;
  EFI_IP6_CANCEL          Cancel;
  EFI_IP6_POLL            Poll;
};

extern EFI_GUID gEfiIp6ServiceBindingProtocolGuid;
extern EFI_GUID gEfiIp6ProtocolGuid;

#endif
