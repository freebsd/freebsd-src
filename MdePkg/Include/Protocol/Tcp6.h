/** @file
  EFI TCPv6(Transmission Control Protocol version 6) Protocol Definition
  The EFI TCPv6 Service Binding Protocol is used to locate EFI TCPv6 Protocol drivers to create
  and destroy child of the driver to communicate with other host using TCP protocol.
  The EFI TCPv6 Protocol provides services to send and receive data stream.

  Copyright (c) 2008 - 2014, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.2

**/

#ifndef __EFI_TCP6_PROTOCOL_H__
#define __EFI_TCP6_PROTOCOL_H__

#include <Protocol/ManagedNetwork.h>
#include <Protocol/Ip6.h>

#define EFI_TCP6_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0xec20eb79, 0x6c1a, 0x4664, {0x9a, 0x0d, 0xd2, 0xe4, 0xcc, 0x16, 0xd6, 0x64 } \
  }

#define EFI_TCP6_PROTOCOL_GUID \
  { \
    0x46e44855, 0xbd60, 0x4ab7, {0xab, 0x0d, 0xa6, 0x79, 0xb9, 0x44, 0x7d, 0x77 } \
  }


typedef struct _EFI_TCP6_PROTOCOL EFI_TCP6_PROTOCOL;

///
/// EFI_TCP6_SERVICE_POINT is deprecated in the UEFI 2.4B and should not be used any more.
/// The definition in here is only present to provide backwards compatability.
///
typedef struct {
  ///
  /// The EFI TCPv6 Protocol instance handle that is using this
  /// address/port pair.
  ///
  EFI_HANDLE        InstanceHandle;
  ///
  /// The local IPv6 address to which this TCP instance is bound. Set
  /// to 0::/128, if this TCP instance is configured to listen on all
  /// available source addresses.
  ///
  EFI_IPv6_ADDRESS  LocalAddress;
  ///
  /// The local port number in host byte order.
  ///
  UINT16            LocalPort;
  ///
  /// The remote IPv6 address. It may be 0::/128 if this TCP instance is
  /// not connected to any remote host.
  ///
  EFI_IPv6_ADDRESS  RemoteAddress;
  ///
  /// The remote port number in host byte order. It may be zero if this
  /// TCP instance is not connected to any remote host.
  ///
  UINT16            RemotePort;
} EFI_TCP6_SERVICE_POINT;

///
/// EFI_TCP6_VARIABLE_DATA is deprecated in the UEFI 2.4B and should not be used any more.
/// The definition in here is only present to provide backwards compatability.
///
typedef struct {
  EFI_HANDLE             DriverHandle; ///< The handle of the driver that creates this entry.
  UINT32                 ServiceCount; ///< The number of address/port pairs following this data structure.
  EFI_TCP6_SERVICE_POINT Services[1];  ///< List of address/port pairs that are currently in use.
} EFI_TCP6_VARIABLE_DATA;

///
/// EFI_TCP6_ACCESS_POINT
///
typedef struct {
  ///
  /// The local IP address assigned to this TCP instance. The EFI
  /// TCPv6 driver will only deliver incoming packets whose
  /// destination addresses exactly match the IP address. Set to zero to
  /// let the underlying IPv6 driver choose a source address. If not zero
  /// it must be one of the configured IP addresses in the underlying
  /// IPv6 driver.
  ///
  EFI_IPv6_ADDRESS  StationAddress;
  ///
  /// The local port number to which this EFI TCPv6 Protocol instance
  /// is bound. If the instance doesn't care the local port number, set
  /// StationPort to zero to use an ephemeral port.
  ///
  UINT16            StationPort;
  ///
  /// The remote IP address to which this EFI TCPv6 Protocol instance
  /// is connected. If ActiveFlag is FALSE (i.e. a passive TCPv6
  /// instance), the instance only accepts connections from the
  /// RemoteAddress. If ActiveFlag is TRUE the instance will
  /// connect to the RemoteAddress, i.e., outgoing segments will be
  /// sent to this address and only segments from this address will be
  /// delivered to the application. When ActiveFlag is FALSE, it
  /// can be set to zero and means that incoming connection requests
  /// from any address will be accepted.
  ///
  EFI_IPv6_ADDRESS  RemoteAddress;
  ///
  /// The remote port to which this EFI TCPv6 Protocol instance
  /// connects or from which connection request will be accepted by
  /// this EFI TCPv6 Protocol instance. If ActiveFlag is FALSE it
  /// can be zero and means that incoming connection request from
  /// any port will be accepted. Its value can not be zero when
  /// ActiveFlag is TRUE.
  ///
  UINT16            RemotePort;
  ///
  /// Set it to TRUE to initiate an active open. Set it to FALSE to
  /// initiate a passive open to act as a server.
  ///
  BOOLEAN           ActiveFlag;
} EFI_TCP6_ACCESS_POINT;

///
/// EFI_TCP6_OPTION
///
typedef struct {
  ///
  /// The size of the TCP receive buffer.
  ///
  UINT32   ReceiveBufferSize;
  ///
  /// The size of the TCP send buffer.
  ///
  UINT32   SendBufferSize;
  ///
  /// The length of incoming connect request queue for a passive
  /// instance. When set to zero, the value is implementation specific.
  ///
  UINT32   MaxSynBackLog;
  ///
  /// The maximum seconds a TCP instance will wait for before a TCP
  /// connection established. When set to zero, the value is
  /// implementation specific.
  ///
  UINT32   ConnectionTimeout;
  ///
  ///The number of times TCP will attempt to retransmit a packet on
  ///an established connection. When set to zero, the value is
  ///implementation specific.
  ///
  UINT32   DataRetries;
  ///
  /// How many seconds to wait in the FIN_WAIT_2 states for a final
  /// FIN flag before the TCP instance is closed. This timeout is in
  /// effective only if the application has called Close() to
  /// disconnect the connection completely. It is also called
  /// FIN_WAIT_2 timer in other implementations. When set to zero,
  /// it should be disabled because the FIN_WAIT_2 timer itself is
  /// against the standard. The default value is 60.
  ///
  UINT32   FinTimeout;
  ///
  /// How many seconds to wait in TIME_WAIT state before the TCP
  /// instance is closed. The timer is disabled completely to provide a
  /// method to close the TCP connection quickly if it is set to zero. It
  /// is against the related RFC documents.
  ///
  UINT32   TimeWaitTimeout;
  ///
  /// The maximum number of TCP keep-alive probes to send before
  /// giving up and resetting the connection if no response from the
  /// other end. Set to zero to disable keep-alive probe.
  ///
  UINT32   KeepAliveProbes;
  ///
  /// The number of seconds a connection needs to be idle before TCP
  /// sends out periodical keep-alive probes. When set to zero, the
  /// value is implementation specific. It should be ignored if keep-
  /// alive probe is disabled.
  ///
  UINT32   KeepAliveTime;
  ///
  /// The number of seconds between TCP keep-alive probes after the
  /// periodical keep-alive probe if no response. When set to zero, the
  /// value is implementation specific. It should be ignored if keep-
  /// alive probe is disabled.
  ///
  UINT32   KeepAliveInterval;
  ///
  /// Set it to TRUE to enable the Nagle algorithm as defined in
  /// RFC896. Set it to FALSE to disable it.
  ///
  BOOLEAN  EnableNagle;
  ///
  /// Set it to TRUE to enable TCP timestamps option as defined in
  /// RFC1323. Set to FALSE to disable it.
  ///
  BOOLEAN  EnableTimeStamp;
  ///
  /// Set it to TRUE to enable TCP window scale option as defined in
  /// RFC1323. Set it to FALSE to disable it.
  ///
  BOOLEAN  EnableWindowScaling;
  ///
  /// Set it to TRUE to enable selective acknowledge mechanism
  /// described in RFC 2018. Set it to FALSE to disable it.
  /// Implementation that supports SACK can optionally support
  /// DSAK as defined in RFC 2883.
  ///
  BOOLEAN  EnableSelectiveAck;
  ///
  /// Set it to TRUE to enable path MTU discovery as defined in
  /// RFC 1191. Set to FALSE to disable it.
  ///
  BOOLEAN  EnablePathMtuDiscovery;
} EFI_TCP6_OPTION;

///
/// EFI_TCP6_CONFIG_DATA
///
typedef struct {
  ///
  /// TrafficClass field in transmitted IPv6 packets.
  ///
  UINT8                 TrafficClass;
  ///
  /// HopLimit field in transmitted IPv6 packets.
  ///
  UINT8                 HopLimit;
  ///
  /// Used to specify TCP communication end settings for a TCP instance.
  ///
  EFI_TCP6_ACCESS_POINT AccessPoint;
  ///
  /// Used to configure the advance TCP option for a connection. If set
  /// to NULL, implementation specific options for TCP connection will be used.
  ///
  EFI_TCP6_OPTION       *ControlOption;
} EFI_TCP6_CONFIG_DATA;

///
/// EFI_TCP6_CONNECTION_STATE
///
typedef enum {
  Tcp6StateClosed      = 0,
  Tcp6StateListen      = 1,
  Tcp6StateSynSent     = 2,
  Tcp6StateSynReceived = 3,
  Tcp6StateEstablished = 4,
  Tcp6StateFinWait1    = 5,
  Tcp6StateFinWait2    = 6,
  Tcp6StateClosing     = 7,
  Tcp6StateTimeWait    = 8,
  Tcp6StateCloseWait   = 9,
  Tcp6StateLastAck     = 10
} EFI_TCP6_CONNECTION_STATE;

///
/// EFI_TCP6_COMPLETION_TOKEN
/// is used as a common header for various asynchronous tokens.
///
typedef struct {
  ///
  /// The Event to signal after request is finished and Status field is
  /// updated by the EFI TCPv6 Protocol driver.
  ///
  EFI_EVENT   Event;
  ///
  /// The result of the completed operation.
  ///
  EFI_STATUS  Status;
} EFI_TCP6_COMPLETION_TOKEN;

///
/// EFI_TCP6_CONNECTION_TOKEN
/// will be set if the active open succeeds or an unexpected
/// error happens.
///
typedef struct {
  ///
  /// The Status in the CompletionToken will be set to one of
  /// the following values if the active open succeeds or an unexpected
  /// error happens:
  /// EFI_SUCCESS:              The active open succeeds and the instance's
  ///                           state is Tcp6StateEstablished.
  /// EFI_CONNECTION_RESET:     The connect fails because the connection is reset
  ///                           either by instance itself or the communication peer.
  /// EFI_CONNECTION_REFUSED:   The receiving or transmission operation fails because this
  ///                           connection is refused.
  /// EFI_ABORTED:              The active open is aborted.
  /// EFI_TIMEOUT:              The connection establishment timer expires and
  ///                           no more specific information is available.
  /// EFI_NETWORK_UNREACHABLE:  The active open fails because
  ///                           an ICMP network unreachable error is received.
  /// EFI_HOST_UNREACHABLE:     The active open fails because an
  ///                           ICMP host unreachable error is received.
  /// EFI_PROTOCOL_UNREACHABLE: The active open fails
  ///                           because an ICMP protocol unreachable error is received.
  /// EFI_PORT_UNREACHABLE:     The connection establishment
  ///                           timer times out and an ICMP port unreachable error is received.
  /// EFI_ICMP_ERROR:           The connection establishment timer times
  ///                           out and some other ICMP error is received.
  /// EFI_DEVICE_ERROR:         An unexpected system or network error occurred.
  /// EFI_SECURITY_VIOLATION:   The active open was failed because of IPSec policy check.
  /// EFI_NO_MEDIA:             There was a media error.
  ///
  EFI_TCP6_COMPLETION_TOKEN CompletionToken;
} EFI_TCP6_CONNECTION_TOKEN;

///
/// EFI_TCP6_LISTEN_TOKEN
/// returns when list operation finishes.
///
typedef struct {
  ///
  /// The Status in CompletionToken will be set to the
  /// following value if accept finishes:
  /// EFI_SUCCESS:            A remote peer has successfully established a
  ///                         connection to this instance. A new TCP instance has also been
  ///                         created for the connection.
  /// EFI_CONNECTION_RESET:   The accept fails because the connection is reset either
  ///                         by instance itself or communication peer.
  /// EFI_ABORTED:            The accept request has been aborted.
  /// EFI_SECURITY_VIOLATION: The accept operation was failed because of IPSec policy check.
  ///
  EFI_TCP6_COMPLETION_TOKEN CompletionToken;
  EFI_HANDLE                NewChildHandle;
} EFI_TCP6_LISTEN_TOKEN;

///
/// EFI_TCP6_FRAGMENT_DATA
/// allows multiple receive or transmit buffers to be specified. The
/// purpose of this structure is to provide scattered read and write.
///
typedef struct {
  UINT32 FragmentLength;   ///< Length of data buffer in the fragment.
  VOID   *FragmentBuffer;  ///< Pointer to the data buffer in the fragment.
} EFI_TCP6_FRAGMENT_DATA;

///
/// EFI_TCP6_RECEIVE_DATA
/// When TCPv6 driver wants to deliver received data to the application,
/// it will pick up the first queued receiving token, update its
/// Token->Packet.RxData then signal the Token->CompletionToken.Event.
///
typedef struct {
  ///
  /// Whether the data is urgent. When this flag is set, the instance is in
  /// urgent mode.
  ///
  BOOLEAN                 UrgentFlag;
  ///
  /// When calling Receive() function, it is the byte counts of all
  /// Fragmentbuffer in FragmentTable allocated by user.
  /// When the token is signaled by TCPv6 driver it is the length of
  /// received data in the fragments.
  ///
  UINT32                  DataLength;
  ///
  /// Number of fragments.
  ///
  UINT32                  FragmentCount;
  ///
  /// An array of fragment descriptors.
  ///
  EFI_TCP6_FRAGMENT_DATA  FragmentTable[1];
} EFI_TCP6_RECEIVE_DATA;

///
/// EFI_TCP6_TRANSMIT_DATA
/// The EFI TCPv6 Protocol user must fill this data structure before sending a packet.
/// The packet may contain multiple buffers in non-continuous memory locations.
///
typedef struct {
  ///
  /// Push If TRUE, data must be transmitted promptly, and the PUSH bit in
  /// the last TCP segment created will be set. If FALSE, data
  /// transmission may be delayed to combine with data from
  /// subsequent Transmit()s for efficiency.
  ///
  BOOLEAN                 Push;
  ///
  /// The data in the fragment table are urgent and urgent point is in
  /// effect if TRUE. Otherwise those data are NOT considered urgent.
  ///
  BOOLEAN                 Urgent;
  ///
  /// Length of the data in the fragments.
  ///
  UINT32                  DataLength;
  ///
  /// Number of fragments.
  ///
  UINT32                  FragmentCount;
  ///
  /// An array of fragment descriptors.
  ///
  EFI_TCP6_FRAGMENT_DATA  FragmentTable[1];
} EFI_TCP6_TRANSMIT_DATA;

///
/// EFI_TCP6_IO_TOKEN
/// returns When transmission finishes or meets any unexpected error.
///
typedef struct {
  ///
  /// When transmission finishes or meets any unexpected error it will
  /// be set to one of the following values:
  /// EFI_SUCCESS:              The receiving or transmission operation
  ///                           completes successfully.
  /// EFI_CONNECTION_FIN:       The receiving operation fails because the communication peer
  ///                           has closed the connection and there is no more data in the
  ///                           receive buffer of the instance.
  /// EFI_CONNECTION_RESET:     The receiving or transmission operation fails
  ///                           because this connection is reset either by instance
  ///                           itself or the communication peer.
  /// EFI_ABORTED:              The receiving or transmission is aborted.
  /// EFI_TIMEOUT:              The transmission timer expires and no more
  ///                           specific information is available.
  /// EFI_NETWORK_UNREACHABLE:  The transmission fails
  ///                           because an ICMP network unreachable error is received.
  /// EFI_HOST_UNREACHABLE:     The transmission fails because an
  ///                           ICMP host unreachable error is received.
  /// EFI_PROTOCOL_UNREACHABLE: The transmission fails
  ///                           because an ICMP protocol unreachable error is received.
  /// EFI_PORT_UNREACHABLE:     The transmission fails and an
  ///                           ICMP port unreachable error is received.
  /// EFI_ICMP_ERROR:           The transmission fails and some other
  ///                           ICMP error is received.
  /// EFI_DEVICE_ERROR:         An unexpected system or network error occurs.
  /// EFI_SECURITY_VIOLATION:   The receiving or transmission
  ///                           operation was failed because of IPSec policy check
  /// EFI_NO_MEDIA:             There was a media error.
  ///
  EFI_TCP6_COMPLETION_TOKEN CompletionToken;
  union {
    ///
    /// When this token is used for receiving, RxData is a pointer to
    /// EFI_TCP6_RECEIVE_DATA.
    ///
    EFI_TCP6_RECEIVE_DATA   *RxData;
    ///
    /// When this token is used for transmitting, TxData is a pointer to
    /// EFI_TCP6_TRANSMIT_DATA.
    ///
    EFI_TCP6_TRANSMIT_DATA  *TxData;
  } Packet;
} EFI_TCP6_IO_TOKEN;

///
/// EFI_TCP6_CLOSE_TOKEN
/// returns when close operation finishes.
///
typedef struct {
  ///
  /// When close finishes or meets any unexpected error it will be set
  /// to one of the following values:
  /// EFI_SUCCESS:            The close operation completes successfully.
  /// EFI_ABORTED:            User called configure with NULL without close stopping.
  /// EFI_SECURITY_VIOLATION: The close operation was failed because of IPSec policy check.
  ///
  EFI_TCP6_COMPLETION_TOKEN CompletionToken;
  ///
  /// Abort the TCP connection on close instead of the standard TCP
  /// close process when it is set to TRUE. This option can be used to
  /// satisfy a fast disconnect.
  ///
  BOOLEAN                   AbortOnClose;
} EFI_TCP6_CLOSE_TOKEN;

/**
  Get the current operational status.

  The GetModeData() function copies the current operational settings of this EFI TCPv6
  Protocol instance into user-supplied buffers. This function can also be used to retrieve
  the operational setting of underlying drivers such as IPv6, MNP, or SNP.

  @param[in]  This              Pointer to the EFI_TCP6_PROTOCOL instance.
  @param[out] Tcp6State         The buffer in which the current TCP state is returned.
  @param[out] Tcp6ConfigData    The buffer in which the current TCP configuration is returned.
  @param[out] Ip6ModeData       The buffer in which the current IPv6 configuration data used by
                                the TCP instance is returned.
  @param[out] MnpConfigData     The buffer in which the current MNP configuration data used
                                indirectly by the TCP instance is returned.
  @param[out] SnpModeData       The buffer in which the current SNP mode data used indirectly by
                                the TCP instance is returned.

  @retval EFI_SUCCESS           The mode data was read.
  @retval EFI_NOT_STARTED       No configuration data is available because this instance hasn't
                                been started.
  @retval EFI_INVALID_PARAMETER This is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP6_GET_MODE_DATA)(
  IN  EFI_TCP6_PROTOCOL                  *This,
  OUT EFI_TCP6_CONNECTION_STATE          *Tcp6State OPTIONAL,
  OUT EFI_TCP6_CONFIG_DATA               *Tcp6ConfigData OPTIONAL,
  OUT EFI_IP6_MODE_DATA                  *Ip6ModeData OPTIONAL,
  OUT EFI_MANAGED_NETWORK_CONFIG_DATA    *MnpConfigData OPTIONAL,
  OUT EFI_SIMPLE_NETWORK_MODE            *SnpModeData OPTIONAL
  );

/**
  Initialize or brutally reset the operational parameters for this EFI TCPv6 instance.

  The Configure() function does the following:
  - Initialize this TCP instance, i.e., initialize the communication end settings and
    specify active open or passive open for an instance.
  - Reset this TCP instance brutally, i.e., cancel all pending asynchronous tokens, flush
    transmission and receiving buffer directly without informing the communication peer.

  No other TCPv6 Protocol operation except Poll() can be executed by this instance until
  it is configured properly. For an active TCP instance, after a proper configuration it
  may call Connect() to initiates the three-way handshake. For a passive TCP instance,
  its state will transit to Tcp6StateListen after configuration, and Accept() may be
  called to listen the incoming TCP connection requests. If Tcp6ConfigData is set to NULL,
  the instance is reset. Resetting process will be done brutally, the state machine will
  be set to Tcp6StateClosed directly, the receive queue and transmit queue will be flushed,
  and no traffic is allowed through this instance.

  @param[in] This               Pointer to the EFI_TCP6_PROTOCOL instance.
  @param[in] Tcp6ConfigData     Pointer to the configure data to configure the instance.
                                If Tcp6ConfigData is set to NULL, the instance is reset.

  @retval EFI_SUCCESS           The operational settings are set, changed, or reset
                                successfully.
  @retval EFI_NO_MAPPING        The underlying IPv6 driver was responsible for choosing a source
                                address for this instance, but no source address was available for
                                use.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions are TRUE:
                                - This is NULL.
                                - Tcp6ConfigData->AccessPoint.StationAddress is neither zero nor
                                  one of the configured IP addresses in the underlying IPv6 driver.
                                - Tcp6ConfigData->AccessPoint.RemoteAddress isn't a valid unicast
                                  IPv6 address.
                                - Tcp6ConfigData->AccessPoint.RemoteAddress is zero or
                                  Tcp6ConfigData->AccessPoint.RemotePort is zero when
                                  Tcp6ConfigData->AccessPoint.ActiveFlag is TRUE.
                                - A same access point has been configured in other TCP
                                  instance properly.
  @retval EFI_ACCESS_DENIED     Configuring TCP instance when it is configured without
                                calling Configure() with NULL to reset it.
  @retval EFI_UNSUPPORTED       One or more of the control options are not supported in
                                the implementation.
  @retval EFI_OUT_OF_RESOURCES  Could not allocate enough system resources when
                                executing Configure().
  @retval EFI_DEVICE_ERROR      An unexpected network or system error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP6_CONFIGURE)(
  IN EFI_TCP6_PROTOCOL        *This,
  IN EFI_TCP6_CONFIG_DATA     *Tcp6ConfigData OPTIONAL
  );

/**
  Initiate a nonblocking TCP connection request for an active TCP instance.

  The Connect() function will initiate an active open to the remote peer configured
  in current TCP instance if it is configured active. If the connection succeeds or
  fails due to any error, the ConnectionToken->CompletionToken.Event will be signaled
  and ConnectionToken->CompletionToken.Status will be updated accordingly. This
  function can only be called for the TCP instance in Tcp6StateClosed state. The
  instance will transfer into Tcp6StateSynSent if the function returns EFI_SUCCESS.
  If TCP three-way handshake succeeds, its state will become Tcp6StateEstablished,
  otherwise, the state will return to Tcp6StateClosed.

  @param[in] This                Pointer to the EFI_TCP6_PROTOCOL instance.
  @param[in] ConnectionToken     Pointer to the connection token to return when the TCP three
                                 way handshake finishes.

  @retval EFI_SUCCESS            The connection request is successfully initiated and the state of
                                 this TCP instance has been changed to Tcp6StateSynSent.
  @retval EFI_NOT_STARTED        This EFI TCPv6 Protocol instance has not been configured.
  @retval EFI_ACCESS_DENIED      One or more of the following conditions are TRUE:
                                 - This instance is not configured as an active one.
                                 - This instance is not in Tcp6StateClosed state.
  @retval EFI_INVALID_PARAMETER  One or more of the following are TRUE:
                                 - This is NULL.
                                 - ConnectionToken is NULL.
                                 - ConnectionToken->CompletionToken.Event is NULL.
  @retval EFI_OUT_OF_RESOURCES   The driver can't allocate enough resource to initiate the active open.
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP6_CONNECT)(
  IN EFI_TCP6_PROTOCOL           *This,
  IN EFI_TCP6_CONNECTION_TOKEN   *ConnectionToken
  );

/**
  Listen on the passive instance to accept an incoming connection request. This is a
  nonblocking operation.

  The Accept() function initiates an asynchronous accept request to wait for an incoming
  connection on the passive TCP instance. If a remote peer successfully establishes a
  connection with this instance, a new TCP instance will be created and its handle will
  be returned in ListenToken->NewChildHandle. The newly created instance is configured
  by inheriting the passive instance's configuration and is ready for use upon return.
  The new instance is in the Tcp6StateEstablished state.

  The ListenToken->CompletionToken.Event will be signaled when a new connection is
  accepted, user aborts the listen or connection is reset.

  This function only can be called when current TCP instance is in Tcp6StateListen state.

  @param[in] This                Pointer to the EFI_TCP6_PROTOCOL instance.
  @param[in] ListenToken         Pointer to the listen token to return when operation finishes.


  @retval EFI_SUCCESS            The listen token has been queued successfully.
  @retval EFI_NOT_STARTED        This EFI TCPv6 Protocol instance has not been configured.
  @retval EFI_ACCESS_DENIED      One or more of the following are TRUE:
                                 - This instance is not a passive instance.
                                 - This instance is not in Tcp6StateListen state.
                                 - The same listen token has already existed in the listen
                                   token queue of this TCP instance.
  @retval EFI_INVALID_PARAMETER  One or more of the following are TRUE:
                                 - This is NULL.
                                 - ListenToken is NULL.
                                 - ListentToken->CompletionToken.Event is NULL.
  @retval EFI_OUT_OF_RESOURCES   Could not allocate enough resource to finish the operation.
  @retval EFI_DEVICE_ERROR       Any unexpected and not belonged to above category error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP6_ACCEPT)(
  IN EFI_TCP6_PROTOCOL             *This,
  IN EFI_TCP6_LISTEN_TOKEN         *ListenToken
  );

/**
  Queues outgoing data into the transmit queue.

  The Transmit() function queues a sending request to this TCP instance along with the
  user data. The status of the token is updated and the event in the token will be
  signaled once the data is sent out or some error occurs.

  @param[in] This                 Pointer to the EFI_TCP6_PROTOCOL instance.
  @param[in] Token                Pointer to the completion token to queue to the transmit queue.

  @retval EFI_SUCCESS             The data has been queued for transmission.
  @retval EFI_NOT_STARTED         This EFI TCPv6 Protocol instance has not been configured.
  @retval EFI_NO_MAPPING          The underlying IPv6 driver was responsible for choosing a
                                  source address for this instance, but no source address was
                                  available for use.
  @retval EFI_INVALID_PARAMETER   One or more of the following are TRUE:
                                  - This is NULL.
                                  - Token is NULL.
                                  - Token->CompletionToken.Event is NULL.
                                  - Token->Packet.TxData is NULL.
                                  - Token->Packet.FragmentCount is zero.
                                  - Token->Packet.DataLength is not equal to the sum of fragment lengths.
  @retval EFI_ACCESS_DENIED       One or more of the following conditions are TRUE:
                                  - A transmit completion token with the same Token->
                                    CompletionToken.Event was already in the
                                    transmission queue.
                                  - The current instance is in Tcp6StateClosed state.
                                  - The current instance is a passive one and it is in
                                    Tcp6StateListen state.
                                  - User has called Close() to disconnect this connection.
  @retval EFI_NOT_READY           The completion token could not be queued because the
                                  transmit queue is full.
  @retval EFI_OUT_OF_RESOURCES    Could not queue the transmit data because of resource
                                  shortage.
  @retval EFI_NETWORK_UNREACHABLE There is no route to the destination network or address.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP6_TRANSMIT)(
  IN EFI_TCP6_PROTOCOL            *This,
  IN EFI_TCP6_IO_TOKEN            *Token
  );

/**
  Places an asynchronous receive request into the receiving queue.

  The Receive() function places a completion token into the receive packet queue. This
  function is always asynchronous. The caller must allocate the Token->CompletionToken.Event
  and the FragmentBuffer used to receive data. The caller also must fill the DataLength which
  represents the whole length of all FragmentBuffer. When the receive operation completes, the
  EFI TCPv6 Protocol driver updates the Token->CompletionToken.Status and Token->Packet.RxData
  fields and the Token->CompletionToken.Event is signaled. If got data the data and its length
  will be copied into the FragmentTable, at the same time the full length of received data will
  be recorded in the DataLength fields. Providing a proper notification function and context
  for the event will enable the user to receive the notification and receiving status. That
  notification function is guaranteed to not be re-entered.

  @param[in] This               Pointer to the EFI_TCP6_PROTOCOL instance.
  @param[in] Token              Pointer to a token that is associated with the receive data
                                descriptor.

  @retval EFI_SUCCESS            The receive completion token was cached.
  @retval EFI_NOT_STARTED        This EFI TCPv6 Protocol instance has not been configured.
  @retval EFI_NO_MAPPING         The underlying IPv6 driver was responsible for choosing a source
                                 address for this instance, but no source address was available for use.
  @retval EFI_INVALID_PARAMETER  One or more of the following conditions is TRUE:
                                 - This is NULL.
                                 - Token is NULL.
                                 - Token->CompletionToken.Event is NULL.
                                 - Token->Packet.RxData is NULL.
                                 - Token->Packet.RxData->DataLength is 0.
                                 - The Token->Packet.RxData->DataLength is not the
                                   sum of all FragmentBuffer length in FragmentTable.
  @retval EFI_OUT_OF_RESOURCES   The receive completion token could not be queued due to a lack of
                                 system resources (usually memory).
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred.
                                 The EFI TCPv6 Protocol instance has been reset to startup defaults.
  @retval EFI_ACCESS_DENIED      One or more of the following conditions is TRUE:
                                 - A receive completion token with the same Token->CompletionToken.Event
                                   was already in the receive queue.
                                 - The current instance is in Tcp6StateClosed state.
                                 - The current instance is a passive one and it is in
                                   Tcp6StateListen state.
                                 - User has called Close() to disconnect this connection.
  @retval EFI_CONNECTION_FIN     The communication peer has closed the connection and there is no
                                 any buffered data in the receive buffer of this instance
  @retval EFI_NOT_READY          The receive request could not be queued because the receive queue is full.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP6_RECEIVE)(
  IN EFI_TCP6_PROTOCOL           *This,
  IN EFI_TCP6_IO_TOKEN           *Token
  );

/**
  Disconnecting a TCP connection gracefully or reset a TCP connection. This function is a
  nonblocking operation.

  Initiate an asynchronous close token to TCP driver. After Close() is called, any buffered
  transmission data will be sent by TCP driver and the current instance will have a graceful close
  working flow described as RFC 793 if AbortOnClose is set to FALSE, otherwise, a rest packet
  will be sent by TCP driver to fast disconnect this connection. When the close operation completes
  successfully the TCP instance is in Tcp6StateClosed state, all pending asynchronous
  operations are signaled and any buffers used for TCP network traffic are flushed.

  @param[in] This                Pointer to the EFI_TCP6_PROTOCOL instance.
  @param[in] CloseToken          Pointer to the close token to return when operation finishes.

  @retval EFI_SUCCESS            The Close() is called successfully.
  @retval EFI_NOT_STARTED        This EFI TCPv6 Protocol instance has not been configured.
  @retval EFI_ACCESS_DENIED      One or more of the following are TRUE:
                                 - CloseToken or CloseToken->CompletionToken.Event is already in use.
                                 - Previous Close() call on this instance has not finished.
  @retval EFI_INVALID_PARAMETER  One or more of the following are TRUE:
                                 - This is NULL.
                                 - CloseToken is NULL.
                                 - CloseToken->CompletionToken.Event is NULL.
  @retval EFI_OUT_OF_RESOURCES   Could not allocate enough resource to finish the operation.
  @retval EFI_DEVICE_ERROR       Any unexpected and not belonged to above category error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP6_CLOSE)(
  IN EFI_TCP6_PROTOCOL           *This,
  IN EFI_TCP6_CLOSE_TOKEN        *CloseToken
  );

/**
  Abort an asynchronous connection, listen, transmission or receive request.

  The Cancel() function aborts a pending connection, listen, transmit or
  receive request.

  If Token is not NULL and the token is in the connection, listen, transmission
  or receive queue when it is being cancelled, its Token->Status will be set
  to EFI_ABORTED and then Token->Event will be signaled.

  If the token is not in one of the queues, which usually means that the
  asynchronous operation has completed, EFI_NOT_FOUND is returned.

  If Token is NULL all asynchronous token issued by Connect(), Accept(),
  Transmit() and Receive() will be aborted.

  @param[in] This                Pointer to the EFI_TCP6_PROTOCOL instance.
  @param[in] Token               Pointer to a token that has been issued by
                                 EFI_TCP6_PROTOCOL.Connect(),
                                 EFI_TCP6_PROTOCOL.Accept(),
                                 EFI_TCP6_PROTOCOL.Transmit() or
                                 EFI_TCP6_PROTOCOL.Receive(). If NULL, all pending
                                 tokens issued by above four functions will be aborted. Type
                                 EFI_TCP6_COMPLETION_TOKEN is defined in
                                 EFI_TCP_PROTOCOL.Connect().

  @retval EFI_SUCCESS            The asynchronous I/O request is aborted and Token->Event
                                 is signaled.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_NOT_STARTED        This instance hasn't been configured.
  @retval EFI_NOT_FOUND          The asynchronous I/O request isn't found in the transmission or
                                 receive queue. It has either completed or wasn't issued by
                                 Transmit() and Receive().
  @retval EFI_UNSUPPORTED        The implementation does not support this function.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP6_CANCEL)(
  IN EFI_TCP6_PROTOCOL           *This,
  IN EFI_TCP6_COMPLETION_TOKEN   *Token OPTIONAL
  );

/**
  Poll to receive incoming data and transmit outgoing segments.

  The Poll() function increases the rate that data is moved between the network
  and application and can be called when the TCP instance is created successfully.
  Its use is optional.

  @param[in] This                Pointer to the EFI_TCP6_PROTOCOL instance.

  @retval EFI_SUCCESS            Incoming or outgoing data was processed.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_DEVICE_ERROR       An unexpected system or network error occurred.
  @retval EFI_NOT_READY          No incoming or outgoing data is processed.
  @retval EFI_TIMEOUT            Data was dropped out of the transmission or receive queue.
                                 Consider increasing the polling rate.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP6_POLL)(
  IN EFI_TCP6_PROTOCOL        *This
  );

///
/// EFI_TCP6_PROTOCOL
/// defines the EFI TCPv6 Protocol child to be used by any network drivers or
/// applications to send or receive data stream. It can either listen on a
/// specified port as a service or actively connect to remote peer as a client.
/// Each instance has its own independent settings.
///
struct _EFI_TCP6_PROTOCOL {
  EFI_TCP6_GET_MODE_DATA  GetModeData;
  EFI_TCP6_CONFIGURE      Configure;
  EFI_TCP6_CONNECT        Connect;
  EFI_TCP6_ACCEPT         Accept;
  EFI_TCP6_TRANSMIT       Transmit;
  EFI_TCP6_RECEIVE        Receive;
  EFI_TCP6_CLOSE          Close;
  EFI_TCP6_CANCEL         Cancel;
  EFI_TCP6_POLL           Poll;
};

extern EFI_GUID gEfiTcp6ServiceBindingProtocolGuid;
extern EFI_GUID gEfiTcp6ProtocolGuid;

#endif

