/** @file
  The EFI UDPv6 (User Datagram Protocol version 6) Protocol Definition, which is built upon
  the EFI IPv6 Protocol and provides simple packet-oriented services to transmit and receive
  UDP packets.

  Copyright (c) 2008 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.2

**/

#ifndef __EFI_UDP6_PROTOCOL_H__
#define __EFI_UDP6_PROTOCOL_H__

#include <Protocol/Ip6.h>

#define EFI_UDP6_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0x66ed4721, 0x3c98, 0x4d3e, {0x81, 0xe3, 0xd0, 0x3d, 0xd3, 0x9a, 0x72, 0x54 } \
  }

#define EFI_UDP6_PROTOCOL_GUID \
  { \
    0x4f948815, 0xb4b9, 0x43cb, {0x8a, 0x33, 0x90, 0xe0, 0x60, 0xb3, 0x49, 0x55 } \
  }

///
/// EFI_UDP6_SERVICE_POINT is deprecated in the UEFI 2.4B and should not be used any more.
/// The definition in here is only present to provide backwards compatability.
///
typedef struct {
  ///
  /// The EFI UDPv6 Protocol instance handle that is using this address/port pair.
  ///
  EFI_HANDLE          InstanceHandle;
  ///
  /// The IPv6 address to which this instance of the EFI UDPv6 Protocol is bound.
  /// Set to 0::/128, if this instance is used to listen all packets from any
  /// source address.
  ///
  EFI_IPv6_ADDRESS    LocalAddress;
  ///
  /// The port number in host byte order on which the service is listening.
  ///
  UINT16              LocalPort;
  ///
  /// The IPv6 address of the remote host. May be 0::/128 if it is not connected
  /// to any remote host or connected with more than one remote host.
  ///
  EFI_IPv6_ADDRESS    RemoteAddress;
  ///
  /// The port number in host byte order on which the remote host is
  /// listening. Maybe zero if it is not connected to any remote host.
  ///
  UINT16              RemotePort;
} EFI_UDP6_SERVICE_POINT;

///
/// EFI_UDP6_VARIABLE_DATA is deprecated in the UEFI 2.4B and should not be used any more.
/// The definition in here is only present to provide backwards compatability.
///
typedef struct {
  ///
  /// The handle of the driver that creates this entry.
  ///
  EFI_HANDLE                DriverHandle;
  ///
  /// The number of address/port pairs that follow this data structure.
  ///
  UINT32                    ServiceCount;
  ///
  /// List of address/port pairs that are currently in use.
  ///
  EFI_UDP6_SERVICE_POINT    Services[1];
} EFI_UDP6_VARIABLE_DATA;

typedef struct _EFI_UDP6_PROTOCOL EFI_UDP6_PROTOCOL;

///
/// EFI_UDP6_FRAGMENT_DATA allows multiple receive or transmit buffers to be specified.
/// The purpose of this structure is to avoid copying the same packet multiple times.
///
typedef struct {
  UINT32    FragmentLength;      ///< Length of the fragment data buffer.
  VOID      *FragmentBuffer;     ///< Pointer to the fragment data buffer.
} EFI_UDP6_FRAGMENT_DATA;

///
/// The EFI_UDP6_SESSION_DATA is used to retrieve the settings when receiving packets or
/// to override the existing settings (only DestinationAddress and DestinationPort can
/// be overridden) of this EFI UDPv6 Protocol instance when sending packets.
///
typedef struct {
  ///
  /// Address from which this packet is sent. This field should not be used when
  /// sending packets.
  ///
  EFI_IPv6_ADDRESS    SourceAddress;
  ///
  /// Port from which this packet is sent. It is in host byte order. This field should
  /// not be used when sending packets.
  ///
  UINT16              SourcePort;
  ///
  /// Address to which this packet is sent. When sending packet, it'll be ignored
  /// if it is zero.
  ///
  EFI_IPv6_ADDRESS    DestinationAddress;
  ///
  /// Port to which this packet is sent. When sending packet, it'll be
  /// ignored if it is zero.
  ///
  UINT16              DestinationPort;
} EFI_UDP6_SESSION_DATA;

typedef struct {
  ///
  /// Set to TRUE to accept UDP packets that are sent to any address.
  ///
  BOOLEAN    AcceptPromiscuous;
  ///
  /// Set to TRUE to accept UDP packets that are sent to any port.
  ///
  BOOLEAN    AcceptAnyPort;
  ///
  /// Set to TRUE to allow this EFI UDPv6 Protocol child instance to open a port number
  /// that is already being used by another EFI UDPv6 Protocol child instance.
  ///
  BOOLEAN    AllowDuplicatePort;
  ///
  /// TrafficClass field in transmitted IPv6 packets.
  ///
  UINT8      TrafficClass;
  ///
  /// HopLimit field in transmitted IPv6 packets.
  ///
  UINT8      HopLimit;
  ///
  /// The receive timeout value (number of microseconds) to be associated with each
  /// incoming packet. Zero means do not drop incoming packets.
  ///
  UINT32     ReceiveTimeout;
  ///
  /// The transmit timeout value (number of microseconds) to be associated with each
  /// outgoing packet. Zero means do not drop outgoing packets.
  ///
  UINT32     TransmitTimeout;
  ///
  /// The station IP address that will be assigned to this EFI UDPv6 Protocol instance.
  /// The EFI UDPv6 and EFI IPv6 Protocol drivers will only deliver incoming packets
  /// whose destination matches this IP address exactly. Address 0::/128 is also accepted
  /// as a special case. Under this situation, underlying IPv6 driver is responsible for
  /// binding a source address to this EFI IPv6 protocol instance according to source
  /// address selection algorithm. Only incoming packet from the selected source address
  /// is delivered. This field can be set and changed only when the EFI IPv6 driver is
  /// transitioning from the stopped to the started states. If no address is available
  /// for selecting, the EFI IPv6 Protocol driver will use EFI_IP6_CONFIG_PROTOCOL to
  /// retrieve the IPv6 address.
  EFI_IPv6_ADDRESS    StationAddress;
  ///
  /// The port number to which this EFI UDPv6 Protocol instance is bound. If a client
  /// of the EFI UDPv6 Protocol does not care about the port number, set StationPort
  /// to zero. The EFI UDPv6 Protocol driver will assign a random port number to transmitted
  /// UDP packets. Ignored it if AcceptAnyPort is TRUE.
  ///
  UINT16              StationPort;
  ///
  /// The IP address of remote host to which this EFI UDPv6 Protocol instance is connecting.
  /// If RemoteAddress is not 0::/128, this EFI UDPv6 Protocol instance will be connected to
  /// RemoteAddress; i.e., outgoing packets of this EFI UDPv6 Protocol instance will be sent
  /// to this address by default and only incoming packets from this address will be delivered
  /// to client. Ignored for incoming filtering if AcceptPromiscuous is TRUE.
  EFI_IPv6_ADDRESS    RemoteAddress;
  ///
  /// The port number of the remote host to which this EFI UDPv6 Protocol instance is connecting.
  /// If it is not zero, outgoing packets of this EFI UDPv6 Protocol instance will be sent to
  /// this port number by default and only incoming packets from this port will be delivered
  /// to client. Ignored if RemoteAddress is 0::/128 and ignored for incoming filtering if
  /// AcceptPromiscuous is TRUE.
  UINT16              RemotePort;
} EFI_UDP6_CONFIG_DATA;

///
/// The EFI UDPv6 Protocol client must fill this data structure before sending a packet.
/// The packet may contain multiple buffers that may be not in a continuous memory location.
///
typedef struct {
  ///
  /// If not NULL, the data that is used to override the transmitting settings.Only the two
  /// filed UdpSessionData.DestinationAddress and UdpSessionData.DestionPort can be used as
  /// the transmitting setting filed.
  ///
  EFI_UDP6_SESSION_DATA     *UdpSessionData;
  ///
  /// Sum of the fragment data length. Must not exceed the maximum UDP packet size.
  ///
  UINT32                    DataLength;
  ///
  /// Number of fragments.
  ///
  UINT32                    FragmentCount;
  ///
  /// Array of fragment descriptors.
  ///
  EFI_UDP6_FRAGMENT_DATA    FragmentTable[1];
} EFI_UDP6_TRANSMIT_DATA;

///
/// EFI_UDP6_RECEIVE_DATA is filled by the EFI UDPv6 Protocol driver when this EFI UDPv6
/// Protocol instance receives an incoming packet. If there is a waiting token for incoming
/// packets, the CompletionToken.Packet.RxData field is updated to this incoming packet and
/// the CompletionToken.Event is signaled. The EFI UDPv6 Protocol client must signal the
/// RecycleSignal after processing the packet.
/// FragmentTable could contain multiple buffers that are not in the continuous memory locations.
/// The EFI UDPv6 Protocol client might need to combine two or more buffers in FragmentTable to
/// form their own protocol header.
///
typedef struct {
  ///
  /// Time when the EFI UDPv6 Protocol accepted the packet.
  ///
  EFI_TIME                  TimeStamp;
  ///
  /// Indicates the event to signal when the received data has been processed.
  ///
  EFI_EVENT                 RecycleSignal;
  ///
  /// The UDP session data including SourceAddress, SourcePort, DestinationAddress,
  /// and DestinationPort.
  ///
  EFI_UDP6_SESSION_DATA     UdpSession;
  ///
  /// The sum of the fragment data length.
  ///
  UINT32                    DataLength;
  ///
  /// Number of fragments. Maybe zero.
  ///
  UINT32                    FragmentCount;
  ///
  /// Array of fragment descriptors. Maybe zero.
  ///
  EFI_UDP6_FRAGMENT_DATA    FragmentTable[1];
} EFI_UDP6_RECEIVE_DATA;

///
/// The EFI_UDP6_COMPLETION_TOKEN structures are used for both transmit and receive operations.
/// When used for transmitting, the Event and TxData fields must be filled in by the EFI UDPv6
/// Protocol client. After the transmit operation completes, the Status field is updated by the
/// EFI UDPv6 Protocol and the Event is signaled.
/// When used for receiving, only the Event field must be filled in by the EFI UDPv6 Protocol
/// client. After a packet is received, RxData and Status are filled in by the EFI UDPv6 Protocol
/// and the Event is signaled.
///
typedef struct {
  ///
  /// This Event will be signaled after the Status field is updated by the EFI UDPv6 Protocol
  /// driver. The type of Event must be EVT_NOTIFY_SIGNAL.
  ///
  EFI_EVENT    Event;
  ///
  /// Will be set to one of the following values:
  ///   - EFI_SUCCESS: The receive or transmit operation completed successfully.
  ///   - EFI_ABORTED: The receive or transmit was aborted.
  ///   - EFI_TIMEOUT: The transmit timeout expired.
  ///   - EFI_NETWORK_UNREACHABLE: The destination network is unreachable. RxData is set to
  ///     NULL in this situation.
  ///   - EFI_HOST_UNREACHABLE: The destination host is unreachable. RxData is set to NULL in
  ///     this situation.
  ///   - EFI_PROTOCOL_UNREACHABLE: The UDP protocol is unsupported in the remote system.
  ///     RxData is set to NULL in this situation.
  ///   - EFI_PORT_UNREACHABLE: No service is listening on the remote port. RxData is set to
  ///     NULL in this situation.
  ///   - EFI_ICMP_ERROR: Some other Internet Control Message Protocol (ICMP) error report was
  ///     received. For example, packets are being sent too fast for the destination to receive them
  ///     and the destination sent an ICMP source quench report. RxData is set to NULL in this situation.
  ///   - EFI_DEVICE_ERROR: An unexpected system or network error occurred.
  ///   - EFI_SECURITY_VIOLATION: The transmit or receive was failed because of IPsec policy check.
  ///   - EFI_NO_MEDIA: There was a media error.
  ///
  EFI_STATUS    Status;
  union {
    ///
    /// When this token is used for receiving, RxData is a pointer to EFI_UDP6_RECEIVE_DATA.
    ///
    EFI_UDP6_RECEIVE_DATA     *RxData;
    ///
    /// When this token is used for transmitting, TxData is a pointer to EFI_UDP6_TRANSMIT_DATA.
    ///
    EFI_UDP6_TRANSMIT_DATA    *TxData;
  } Packet;
} EFI_UDP6_COMPLETION_TOKEN;

/**
  Read the current operational settings.

  The GetModeData() function copies the current operational settings of this EFI UDPv6 Protocol
  instance into user-supplied buffers. This function is used optionally to retrieve the operational
  mode data of underlying networks or drivers.

  @param[in]   This             Pointer to the EFI_UDP6_PROTOCOL instance.
  @param[out]  Udp6ConfigData   The buffer in which the current UDP configuration data is returned.
  @param[out]  Ip6ModeData      The buffer in which the current EFI IPv6 Protocol mode data is returned.
  @param[out]  MnpConfigData    The buffer in which the current managed network configuration data is
                                returned.
  @param[out]  SnpModeData      The buffer in which the simple network mode data is returned.

  @retval EFI_SUCCESS           The mode data was read.
  @retval EFI_NOT_STARTED       When Udp6ConfigData is queried, no configuration data is available
                                because this instance has not been started.
  @retval EFI_INVALID_PARAMETER This is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_UDP6_GET_MODE_DATA)(
  IN EFI_UDP6_PROTOCOL                 *This,
  OUT EFI_UDP6_CONFIG_DATA             *Udp6ConfigData OPTIONAL,
  OUT EFI_IP6_MODE_DATA                *Ip6ModeData    OPTIONAL,
  OUT EFI_MANAGED_NETWORK_CONFIG_DATA  *MnpConfigData  OPTIONAL,
  OUT EFI_SIMPLE_NETWORK_MODE          *SnpModeData    OPTIONAL
  );

/**
  Initializes, changes, or resets the operational parameters for this instance of the EFI UDPv6
  Protocol.

  The Configure() function is used to do the following:
  - Initialize and start this instance of the EFI UDPv6 Protocol.
  - Change the filtering rules and operational parameters.
  - Reset this instance of the EFI UDPv6 Protocol.

  Until these parameters are initialized, no network traffic can be sent or received by this instance.
  This instance can be also reset by calling Configure() with UdpConfigData set to NULL.
  Once reset, the receiving queue and transmitting queue are flushed and no traffic is allowed through
  this instance.

  With different parameters in UdpConfigData, Configure() can be used to bind this instance to specified
  port.

  @param[in]   This             Pointer to the EFI_UDP6_PROTOCOL instance.
  @param[in]   UdpConfigData    Pointer to the buffer contained the configuration data.

  @retval EFI_SUCCESS           The configuration settings were set, changed, or reset successfully.
  @retval EFI_NO_MAPPING        The underlying IPv6 driver was responsible for choosing a source
                                address for this instance, but no source address was available for use.
  @retval EFI_INVALID_PARAMETER One or more following conditions are TRUE:
                                - This is NULL.
                                - UdpConfigData.StationAddress neither zero nor one of the configured IP
                                  addresses in the underlying IPv6 driver.
                                - UdpConfigData.RemoteAddress is not a valid unicast IPv6 address if it
                                  is not zero.
  @retval EFI_ALREADY_STARTED   The EFI UDPv6 Protocol instance is already started/configured and must be
                                stopped/reset before it can be reconfigured. Only TrafficClass, HopLimit,
                                ReceiveTimeout, and TransmitTimeout can be reconfigured without stopping
                                the current instance of the EFI UDPv6 Protocol.
  @retval EFI_ACCESS_DENIED     UdpConfigData.AllowDuplicatePort is FALSE and UdpConfigData.StationPort
                                is already used by other instance.
  @retval EFI_OUT_OF_RESOURCES  The EFI UDPv6 Protocol driver cannot allocate memory for this EFI UDPv6
                                Protocol instance.
  @retval EFI_DEVICE_ERROR      An unexpected network or system error occurred and this instance was not
                                opened.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_UDP6_CONFIGURE)(
  IN EFI_UDP6_PROTOCOL     *This,
  IN EFI_UDP6_CONFIG_DATA  *UdpConfigData OPTIONAL
  );

/**
  Joins and leaves multicast groups.

  The Groups() function is used to join or leave one or more multicast group.
  If the JoinFlag is FALSE and the MulticastAddress is NULL, then all currently joined groups are left.

  @param[in]   This             Pointer to the EFI_UDP6_PROTOCOL instance.
  @param[in]   JoinFlag         Set to TRUE to join a multicast group. Set to FALSE to leave one
                                or all multicast groups.
  @param[in]   MulticastAddress Pointer to multicast group address to join or leave.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_NOT_STARTED       The EFI UDPv6 Protocol instance has not been started.
  @retval EFI_OUT_OF_RESOURCES  Could not allocate resources to join the group.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - This is NULL.
                                - JoinFlag is TRUE and MulticastAddress is NULL.
                                - JoinFlag is TRUE and *MulticastAddress is not a valid multicast address.
  @retval EFI_ALREADY_STARTED   The group address is already in the group table (when JoinFlag is TRUE).
  @retval EFI_NOT_FOUND         The group address is not in the group table (when JoinFlag is FALSE).
  @retval EFI_DEVICE_ERROR      An unexpected system or network error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_UDP6_GROUPS)(
  IN EFI_UDP6_PROTOCOL  *This,
  IN BOOLEAN            JoinFlag,
  IN EFI_IPv6_ADDRESS   *MulticastAddress OPTIONAL
  );

/**
  Queues outgoing data packets into the transmit queue.

  The Transmit() function places a sending request to this instance of the EFI UDPv6 Protocol,
  alongside the transmit data that was filled by the user. Whenever the packet in the token is
  sent out or some errors occur, the Token.Event will be signaled and Token.Status is updated.
  Providing a proper notification function and context for the event will enable the user to
  receive the notification and transmitting status.

  @param[in]   This             Pointer to the EFI_UDP6_PROTOCOL instance.
  @param[in]   Token            Pointer to the completion token that will be placed into the
                                transmit queue.

  @retval EFI_SUCCESS           The data has been queued for transmission.
  @retval EFI_NOT_STARTED       This EFI UDPv6 Protocol instance has not been started.
  @retval EFI_NO_MAPPING        The underlying IPv6 driver was responsible for choosing a source
                                address for this instance, but no source address was available
                                for use.
  @retval EFI_INVALID_PARAMETER One or more of the following are TRUE:
                                - This is NULL.
                                - Token is NULL.
                                - Token.Event is NULL.
                                - Token.Packet.TxData is NULL.
                                - Token.Packet.TxData.FragmentCount is zero.
                                - Token.Packet.TxData.DataLength is not equal to the sum of fragment
                                  lengths.
                                - One or more of the Token.Packet.TxData.FragmentTable[].FragmentLength
                                  fields is zero.
                                - One or more of the Token.Packet.TxData.FragmentTable[].FragmentBuffer
                                  fields is NULL.
                                - Token.Packet.TxData.UdpSessionData.DestinationAddress is not zero
                                  and is not valid unicast Ipv6 address if UdpSessionData is not NULL.
                                - Token.Packet.TxData.UdpSessionData is NULL and this instance's
                                  UdpConfigData.RemoteAddress is unspecified.
                                - Token.Packet.TxData.UdpSessionData.DestinationAddress is non-zero
                                  when DestinationAddress is configured as non-zero when doing Configure()
                                  for this EFI Udp6 protocol instance.
                                - Token.Packet.TxData.UdpSesionData.DestinationAddress is zero when
                                  DestinationAddress is unspecified when doing Configure() for this
                                  EFI Udp6 protocol instance.
  @retval EFI_ACCESS_DENIED     The transmit completion token with the same Token.Event was already
                                in the transmit queue.
  @retval EFI_NOT_READY         The completion token could not be queued because the transmit queue
                                is full.
  @retval EFI_OUT_OF_RESOURCES  Could not queue the transmit data.
  @retval EFI_NOT_FOUND         There is no route to the destination network or address.
  @retval EFI_BAD_BUFFER_SIZE   The data length is greater than the maximum UDP packet size.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_UDP6_TRANSMIT)(
  IN EFI_UDP6_PROTOCOL          *This,
  IN EFI_UDP6_COMPLETION_TOKEN  *Token
  );

/**
  Places an asynchronous receive request into the receiving queue.

  The Receive() function places a completion token into the receive packet queue. This function is
  always asynchronous.
  The caller must fill in the Token.Event field in the completion token, and this field cannot be
  NULL. When the receive operation completes, the EFI UDPv6 Protocol driver updates the Token.Status
  and Token.Packet.RxData fields and the Token.Event is signaled.
  Providing a proper notification function and context for the event will enable the user to receive
  the notification and receiving status. That notification function is guaranteed to not be re-entered.

  @param[in]   This             Pointer to the EFI_UDP6_PROTOCOL instance.
  @param[in]   Token            Pointer to a token that is associated with the receive data descriptor.

  @retval EFI_SUCCESS           The receive completion token was cached.
  @retval EFI_NOT_STARTED       This EFI UDPv6 Protocol instance has not been started.
  @retval EFI_NO_MAPPING        The underlying IPv6 driver was responsible for choosing a source
                                address for this instance, but no source address was available
                                for use.
  @retval EFI_INVALID_PARAMETER One or more of the following is TRUE:
                                - This is NULL.
                                - Token is NULL.
                                - Token.Event is NULL.
  @retval EFI_OUT_OF_RESOURCES  The receive completion token could not be queued due to a lack of system
                                resources (usually memory).
  @retval EFI_DEVICE_ERROR      An unexpected system or network error occurred. The EFI UDPv6 Protocol
                                instance has been reset to startup defaults.
  @retval EFI_ACCESS_DENIED     A receive completion token with the same Token.Event was already in
                                the receive queue.
  @retval EFI_NOT_READY         The receive request could not be queued because the receive queue is full.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_UDP6_RECEIVE)(
  IN EFI_UDP6_PROTOCOL          *This,
  IN EFI_UDP6_COMPLETION_TOKEN  *Token
  );

/**
  Aborts an asynchronous transmit or receive request.

  The Cancel() function is used to abort a pending transmit or receive request. If the token is in the
  transmit or receive request queues, after calling this function, Token.Status will be set to
  EFI_ABORTED and then Token.Event will be signaled. If the token is not in one of the queues,
  which usually means that the asynchronous operation has completed, this function will not signal the
  token and EFI_NOT_FOUND is returned.

  @param[in]   This             Pointer to the EFI_UDP6_PROTOCOL instance.
  @param[in]   Token            Pointer to a token that has been issued by EFI_UDP6_PROTOCOL.Transmit()
                                or EFI_UDP6_PROTOCOL.Receive().If NULL, all pending tokens are aborted.

  @retval EFI_SUCCESS           The asynchronous I/O request was aborted and Token.Event was signaled.
                                When Token is NULL, all pending requests are aborted and their events
                                are signaled.
  @retval EFI_INVALID_PARAMETER This is NULL.
  @retval EFI_NOT_STARTED       This instance has not been started.
  @retval EFI_NOT_FOUND         When Token is not NULL, the asynchronous I/O request was not found in
                                the transmit or receive queue. It has either completed or was not issued
                                by Transmit() and Receive().

**/
typedef
EFI_STATUS
(EFIAPI *EFI_UDP6_CANCEL)(
  IN EFI_UDP6_PROTOCOL          *This,
  IN EFI_UDP6_COMPLETION_TOKEN  *Token OPTIONAL
  );

/**
  Polls for incoming data packets and processes outgoing data packets.

  The Poll() function can be used by network drivers and applications to increase the rate that data
  packets are moved between the communications device and the transmit and receive queues.
  In some systems, the periodic timer event in the managed network driver may not poll the underlying
  communications device fast enough to transmit and/or receive all data packets without missing incoming
  packets or dropping outgoing packets. Drivers and applications that are experiencing packet loss should
  try calling the Poll() function more often.

  @param[in]   This             Pointer to the EFI_UDP6_PROTOCOL instance.

  @retval EFI_SUCCESS           Incoming or outgoing data was processed.
  @retval EFI_INVALID_PARAMETER This is NULL.
  @retval EFI_DEVICE_ERROR      An unexpected system or network error occurred.
  @retval EFI_TIMEOUT           Data was dropped out of the transmit and/or receive queue.
                                Consider increasing the polling rate.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_UDP6_POLL)(
  IN EFI_UDP6_PROTOCOL  *This
  );

///
/// The EFI_UDP6_PROTOCOL defines an EFI UDPv6 Protocol session that can be used by any network drivers,
/// applications, or daemons to transmit or receive UDP packets. This protocol instance can either be
/// bound to a specified port as a service or connected to some remote peer as an active client.
/// Each instance has its own settings, such as group table, that are independent from each other.
///
struct _EFI_UDP6_PROTOCOL {
  EFI_UDP6_GET_MODE_DATA    GetModeData;
  EFI_UDP6_CONFIGURE        Configure;
  EFI_UDP6_GROUPS           Groups;
  EFI_UDP6_TRANSMIT         Transmit;
  EFI_UDP6_RECEIVE          Receive;
  EFI_UDP6_CANCEL           Cancel;
  EFI_UDP6_POLL             Poll;
};

extern EFI_GUID  gEfiUdp6ServiceBindingProtocolGuid;
extern EFI_GUID  gEfiUdp6ProtocolGuid;

#endif
