/** @file
  EFI TCPv4(Transmission Control Protocol version 4) Protocol Definition
  The EFI TCPv4 Service Binding Protocol is used to locate EFI TCPv4 Protocol drivers to create
  and destroy child of the driver to communicate with other host using TCP protocol.
  The EFI TCPv4 Protocol provides services to send and receive data stream.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.0.

**/

#ifndef __EFI_TCP4_PROTOCOL_H__
#define __EFI_TCP4_PROTOCOL_H__

#include <Protocol/Ip4.h>

#define EFI_TCP4_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0x00720665, 0x67EB, 0x4a99, {0xBA, 0xF7, 0xD3, 0xC3, 0x3A, 0x1C, 0x7C, 0xC9 } \
  }

#define EFI_TCP4_PROTOCOL_GUID \
  { \
    0x65530BC7, 0xA359, 0x410f, {0xB0, 0x10, 0x5A, 0xAD, 0xC7, 0xEC, 0x2B, 0x62 } \
  }

typedef struct _EFI_TCP4_PROTOCOL EFI_TCP4_PROTOCOL;

///
/// EFI_TCP4_SERVICE_POINT is deprecated in the UEFI 2.4B and should not be used any more.
/// The definition in here is only present to provide backwards compatability.
///
typedef struct {
  EFI_HANDLE              InstanceHandle;
  EFI_IPv4_ADDRESS        LocalAddress;
  UINT16                  LocalPort;
  EFI_IPv4_ADDRESS        RemoteAddress;
  UINT16                  RemotePort;
} EFI_TCP4_SERVICE_POINT;

///
/// EFI_TCP4_VARIABLE_DATA is deprecated in the UEFI 2.4B and should not be used any more.
/// The definition in here is only present to provide backwards compatability.
///
typedef struct {
  EFI_HANDLE              DriverHandle;
  UINT32                  ServiceCount;
  EFI_TCP4_SERVICE_POINT  Services[1];
} EFI_TCP4_VARIABLE_DATA;

typedef struct {
  BOOLEAN                 UseDefaultAddress;
  EFI_IPv4_ADDRESS        StationAddress;
  EFI_IPv4_ADDRESS        SubnetMask;
  UINT16                  StationPort;
  EFI_IPv4_ADDRESS        RemoteAddress;
  UINT16                  RemotePort;
  BOOLEAN                 ActiveFlag;
} EFI_TCP4_ACCESS_POINT;

typedef struct {
  UINT32                  ReceiveBufferSize;
  UINT32                  SendBufferSize;
  UINT32                  MaxSynBackLog;
  UINT32                  ConnectionTimeout;
  UINT32                  DataRetries;
  UINT32                  FinTimeout;
  UINT32                  TimeWaitTimeout;
  UINT32                  KeepAliveProbes;
  UINT32                  KeepAliveTime;
  UINT32                  KeepAliveInterval;
  BOOLEAN                 EnableNagle;
  BOOLEAN                 EnableTimeStamp;
  BOOLEAN                 EnableWindowScaling;
  BOOLEAN                 EnableSelectiveAck;
  BOOLEAN                 EnablePathMtuDiscovery;
} EFI_TCP4_OPTION;

typedef struct {
  //
  // I/O parameters
  //
  UINT8                   TypeOfService;
  UINT8                   TimeToLive;

  //
  // Access Point
  //
  EFI_TCP4_ACCESS_POINT   AccessPoint;

  //
  // TCP Control Options
  //
  EFI_TCP4_OPTION         *ControlOption;
} EFI_TCP4_CONFIG_DATA;

///
/// TCP4 connnection state
///
typedef enum {
  Tcp4StateClosed         = 0,
  Tcp4StateListen         = 1,
  Tcp4StateSynSent        = 2,
  Tcp4StateSynReceived    = 3,
  Tcp4StateEstablished    = 4,
  Tcp4StateFinWait1       = 5,
  Tcp4StateFinWait2       = 6,
  Tcp4StateClosing        = 7,
  Tcp4StateTimeWait       = 8,
  Tcp4StateCloseWait      = 9,
  Tcp4StateLastAck        = 10
} EFI_TCP4_CONNECTION_STATE;

typedef struct {
  EFI_EVENT   Event;
  EFI_STATUS  Status;
} EFI_TCP4_COMPLETION_TOKEN;

typedef struct {
  ///
  /// The Status in the CompletionToken will be set to one of
  /// the following values if the active open succeeds or an unexpected
  /// error happens:
  /// EFI_SUCCESS:              The active open succeeds and the instance's
  ///                           state is Tcp4StateEstablished.
  /// EFI_CONNECTION_RESET:     The connect fails because the connection is reset
  ///                           either by instance itself or the communication peer.
  /// EFI_CONNECTION_REFUSED:   The connect fails because this connection is initiated with
  ///                           an active open and the connection is refused.
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
  /// EFI_ICMP_ERROR:           The connection establishment timer timeout and some other ICMP
  ///                           error is received.
  /// EFI_DEVICE_ERROR:         An unexpected system or network error occurred.
  /// EFI_NO_MEDIA:             There was a media error.
  ///
  EFI_TCP4_COMPLETION_TOKEN CompletionToken;
} EFI_TCP4_CONNECTION_TOKEN;

typedef struct {
  EFI_TCP4_COMPLETION_TOKEN CompletionToken;
  EFI_HANDLE                NewChildHandle;
} EFI_TCP4_LISTEN_TOKEN;

typedef struct {
  UINT32 FragmentLength;
  VOID   *FragmentBuffer;
} EFI_TCP4_FRAGMENT_DATA;

typedef struct {
  BOOLEAN                   UrgentFlag;
  UINT32                    DataLength;
  UINT32                    FragmentCount;
  EFI_TCP4_FRAGMENT_DATA    FragmentTable[1];
} EFI_TCP4_RECEIVE_DATA;

typedef struct {
  BOOLEAN                   Push;
  BOOLEAN                   Urgent;
  UINT32                    DataLength;
  UINT32                    FragmentCount;
  EFI_TCP4_FRAGMENT_DATA    FragmentTable[1];
} EFI_TCP4_TRANSMIT_DATA;

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
  /// EFI_NO_MEDIA:             There was a media error.
  ///
  EFI_TCP4_COMPLETION_TOKEN CompletionToken;
  union {
    ///
    /// When this token is used for receiving, RxData is a pointer to EFI_TCP4_RECEIVE_DATA.
    ///
    EFI_TCP4_RECEIVE_DATA   *RxData;
    ///
    /// When this token is used for transmitting, TxData is a pointer to EFI_TCP4_TRANSMIT_DATA.
    ///
    EFI_TCP4_TRANSMIT_DATA  *TxData;
  } Packet;
} EFI_TCP4_IO_TOKEN;

typedef struct {
  EFI_TCP4_COMPLETION_TOKEN CompletionToken;
  BOOLEAN                   AbortOnClose;
} EFI_TCP4_CLOSE_TOKEN;

//
// Interface definition for TCP4 protocol
//

/**
  Get the current operational status.

  @param  This           The pointer to the EFI_TCP4_PROTOCOL instance.
  @param  Tcp4State      The pointer to the buffer to receive the current TCP state.
  @param  Tcp4ConfigData The pointer to the buffer to receive the current TCP configuration.
  @param  Ip4ModeData    The pointer to the buffer to receive the current IPv4 configuration
                         data used by the TCPv4 instance.
  @param  MnpConfigData  The pointer to the buffer to receive the current MNP configuration
                         data used indirectly by the TCPv4 instance.
  @param  SnpModeData    The pointer to the buffer to receive the current SNP configuration
                         data used indirectly by the TCPv4 instance.

  @retval EFI_SUCCESS           The mode data was read.
  @retval EFI_INVALID_PARAMETER This is NULL.
  @retval EFI_NOT_STARTED       No configuration data is available because this instance hasn't
                                 been started.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP4_GET_MODE_DATA)(
  IN   EFI_TCP4_PROTOCOL                  *This,
  OUT  EFI_TCP4_CONNECTION_STATE          *Tcp4State      OPTIONAL,
  OUT  EFI_TCP4_CONFIG_DATA               *Tcp4ConfigData OPTIONAL,
  OUT  EFI_IP4_MODE_DATA                  *Ip4ModeData    OPTIONAL,
  OUT  EFI_MANAGED_NETWORK_CONFIG_DATA    *MnpConfigData  OPTIONAL,
  OUT  EFI_SIMPLE_NETWORK_MODE            *SnpModeData    OPTIONAL
  );

/**
  Initialize or brutally reset the operational parameters for this EFI TCPv4 instance.

  @param  This           The pointer to the EFI_TCP4_PROTOCOL instance.
  @param  Tcp4ConfigData The pointer to the configure data to configure the instance.

  @retval EFI_SUCCESS           The operational settings are set, changed, or reset
                                successfully.
  @retval EFI_INVALID_PARAMETER Some parameter is invalid.
  @retval EFI_NO_MAPPING        When using a default address, configuration (through
                                DHCP, BOOTP, RARP, etc.) is not finished yet.
  @retval EFI_ACCESS_DENIED     Configuring TCP instance when it is configured without
                                calling Configure() with NULL to reset it.
  @retval EFI_DEVICE_ERROR      An unexpected network or system error occurred.
  @retval EFI_UNSUPPORTED       One or more of the control options are not supported in
                                the implementation.
  @retval EFI_OUT_OF_RESOURCES  Could not allocate enough system resources when
                                executing Configure().

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP4_CONFIGURE)(
  IN EFI_TCP4_PROTOCOL                   *This,
  IN EFI_TCP4_CONFIG_DATA                *TcpConfigData OPTIONAL
  );


/**
  Add or delete a route entry to the route table

  @param  This           The pointer to the EFI_TCP4_PROTOCOL instance.
  @param  DeleteRoute    Set it to TRUE to delete this route from the routing table. Set it to
                         FALSE to add this route to the routing table.
                         DestinationAddress and SubnetMask are used as the
                         keywords to search route entry.
  @param  SubnetAddress  The destination network.
  @param  SubnetMask     The subnet mask of the destination network.
  @param  GatewayAddress The gateway address for this route. It must be on the same
                         subnet with the station address unless a direct route is specified.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_NOT_STARTED       The EFI TCPv4 Protocol instance has not been configured.
  @retval EFI_NO_MAPPING        When using a default address, configuration (DHCP, BOOTP,
                                RARP, etc.) is not finished yet.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - This is NULL.
                                - SubnetAddress is NULL.
                                - SubnetMask is NULL.
                                - GatewayAddress is NULL.
                                - *SubnetAddress is not NULL a valid subnet address.
                                - *SubnetMask is not a valid subnet mask.
                                - *GatewayAddress is not a valid unicast IP address or it
                                is not in the same subnet.
  @retval EFI_OUT_OF_RESOURCES  Could not allocate enough resources to add the entry to the
                                routing table.
  @retval EFI_NOT_FOUND         This route is not in the routing table.
  @retval EFI_ACCESS_DENIED     The route is already defined in the routing table.
  @retval EFI_UNSUPPORTED       The TCP driver does not support this operation.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP4_ROUTES)(
  IN EFI_TCP4_PROTOCOL                   *This,
  IN BOOLEAN                             DeleteRoute,
  IN EFI_IPv4_ADDRESS                    *SubnetAddress,
  IN EFI_IPv4_ADDRESS                    *SubnetMask,
  IN EFI_IPv4_ADDRESS                    *GatewayAddress
  );

/**
  Initiate a nonblocking TCP connection request for an active TCP instance.

  @param  This                  The pointer to the EFI_TCP4_PROTOCOL instance.
  @param  ConnectionToken       The pointer to the connection token to return when the TCP three
                                way handshake finishes.

  @retval EFI_SUCCESS           The connection request is successfully initiated and the state
                                of this TCPv4 instance has been changed to Tcp4StateSynSent.
  @retval EFI_NOT_STARTED       This EFI TCPv4 Protocol instance has not been configured.
  @retval EFI_ACCESS_DENIED     One or more of the following conditions are TRUE:
                                - This instance is not configured as an active one.
                                - This instance is not in Tcp4StateClosed state.
  @retval EFI_INVALID_PARAMETER One or more of the following are TRUE:
                                - This is NULL.
                                - ConnectionToken is NULL.
                                - ConnectionToken->CompletionToken.Event is NULL.
  @retval EFI_OUT_OF_RESOURCES  The driver can't allocate enough resource to initiate the activ eopen.
  @retval EFI_DEVICE_ERROR      An unexpected system or network error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP4_CONNECT)(
  IN EFI_TCP4_PROTOCOL                   *This,
  IN EFI_TCP4_CONNECTION_TOKEN           *ConnectionToken
  );


/**
  Listen on the passive instance to accept an incoming connection request. This is a nonblocking operation.

  @param  This        The pointer to the EFI_TCP4_PROTOCOL instance.
  @param  ListenToken The pointer to the listen token to return when operation finishes.

  @retval EFI_SUCCESS           The listen token has been queued successfully.
  @retval EFI_NOT_STARTED       This EFI TCPv4 Protocol instance has not been configured.
  @retval EFI_ACCESS_DENIED     One or more of the following are TRUE:
                                - This instance is not a passive instance.
                                - This instance is not in Tcp4StateListen state.
                                - The same listen token has already existed in the listen
                                token queue of this TCP instance.
  @retval EFI_INVALID_PARAMETER One or more of the following are TRUE:
                                - This is NULL.
                                - ListenToken is NULL.
                                - ListentToken->CompletionToken.Event is NULL.
  @retval EFI_OUT_OF_RESOURCES  Could not allocate enough resource to finish the operation.
  @retval EFI_DEVICE_ERROR      Any unexpected and not belonged to above category error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP4_ACCEPT)(
  IN EFI_TCP4_PROTOCOL                   *This,
  IN EFI_TCP4_LISTEN_TOKEN               *ListenToken
  );

/**
  Queues outgoing data into the transmit queue.

  @param  This  The pointer to the EFI_TCP4_PROTOCOL instance.
  @param  Token The pointer to the completion token to queue to the transmit queue.

  @retval EFI_SUCCESS             The data has been queued for transmission.
  @retval EFI_NOT_STARTED         This EFI TCPv4 Protocol instance has not been configured.
  @retval EFI_NO_MAPPING          When using a default address, configuration (DHCP, BOOTP,
                                  RARP, etc.) is not finished yet.
  @retval EFI_INVALID_PARAMETER   One or more of the following are TRUE:
                                  - This is NULL.
                                  - Token is NULL.
                                  - Token->CompletionToken.Event is NULL.
                                  - Token->Packet.TxData is NULL L.
                                  - Token->Packet.FragmentCount is zero.
                                  - Token->Packet.DataLength is not equal to the sum of fragment lengths.
  @retval EFI_ACCESS_DENIED       One or more of the following conditions is TRUE:
                                  - A transmit completion token with the same Token->CompletionToken.Event
                                  was already in the transmission queue.
                                  - The current instance is in Tcp4StateClosed state.
                                  - The current instance is a passive one and it is in
                                  Tcp4StateListen state.
                                  - User has called Close() to disconnect this connection.
  @retval EFI_NOT_READY           The completion token could not be queued because the
                                  transmit queue is full.
  @retval EFI_OUT_OF_RESOURCES    Could not queue the transmit data because of resource
                                  shortage.
  @retval EFI_NETWORK_UNREACHABLE There is no route to the destination network or address.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP4_TRANSMIT)(
  IN EFI_TCP4_PROTOCOL                   *This,
  IN EFI_TCP4_IO_TOKEN                   *Token
  );


/**
  Places an asynchronous receive request into the receiving queue.

  @param  This  The pointer to the EFI_TCP4_PROTOCOL instance.
  @param  Token The pointer to a token that is associated with the receive data
                descriptor.

  @retval EFI_SUCCESS           The receive completion token was cached.
  @retval EFI_NOT_STARTED       This EFI TCPv4 Protocol instance has not been configured.
  @retval EFI_NO_MAPPING        When using a default address, configuration (DHCP, BOOTP, RARP,
                                etc.) is not finished yet.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - This is NULL.
                                - Token is NULL.
                                - Token->CompletionToken.Event is NULL.
                                - Token->Packet.RxData is NULL.
                                - Token->Packet.RxData->DataLength is 0.
                                - The Token->Packet.RxData->DataLength is not
                                the sum of all FragmentBuffer length in FragmentTable.
  @retval EFI_OUT_OF_RESOURCES The receive completion token could not be queued due to a lack of
                               system resources (usually memory).
  @retval EFI_DEVICE_ERROR     An unexpected system or network error occurred.
  @retval EFI_ACCESS_DENIED    One or more of the following conditions is TRUE:
                               - A receive completion token with the same Token-
                               >CompletionToken.Event was already in the receive
                               queue.
                               - The current instance is in Tcp4StateClosed state.
                               - The current instance is a passive one and it is in
                               Tcp4StateListen state.
                               - User has called Close() to disconnect this connection.
  @retval EFI_CONNECTION_FIN   The communication peer has closed the connection and there is
                               no any buffered data in the receive buffer of this instance.
  @retval EFI_NOT_READY        The receive request could not be queued because the receive queue is full.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP4_RECEIVE)(
  IN EFI_TCP4_PROTOCOL                   *This,
  IN EFI_TCP4_IO_TOKEN                   *Token
  );

/**
  Disconnecting a TCP connection gracefully or reset a TCP connection. This function is a
  nonblocking operation.

  @param  This       The pointer to the EFI_TCP4_PROTOCOL instance.
  @param  CloseToken The pointer to the close token to return when operation finishes.

  @retval EFI_SUCCESS           The Close() is called successfully.
  @retval EFI_NOT_STARTED       This EFI TCPv4 Protocol instance has not been configured.
  @retval EFI_ACCESS_DENIED     One or more of the following are TRUE:
                                - Configure() has been called with
                                TcpConfigData set to NULL and this function has
                                not returned.
                                - Previous Close() call on this instance has not
                                finished.
  @retval EFI_INVALID_PARAMETER One or more of the following are TRUE:
                                - This is NULL.
                                - CloseToken is NULL.
                                - CloseToken->CompletionToken.Event is NULL.
  @retval EFI_OUT_OF_RESOURCES  Could not allocate enough resource to finish the operation.
  @retval EFI_DEVICE_ERROR      Any unexpected and not belonged to above category error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP4_CLOSE)(
  IN EFI_TCP4_PROTOCOL                   *This,
  IN EFI_TCP4_CLOSE_TOKEN                *CloseToken
  );

/**
  Abort an asynchronous connection, listen, transmission or receive request.

  @param  This  The pointer to the EFI_TCP4_PROTOCOL instance.
  @param  Token The pointer to a token that has been issued by
                EFI_TCP4_PROTOCOL.Connect(),
                EFI_TCP4_PROTOCOL.Accept(),
                EFI_TCP4_PROTOCOL.Transmit() or
                EFI_TCP4_PROTOCOL.Receive(). If NULL, all pending
                tokens issued by above four functions will be aborted. Type
                EFI_TCP4_COMPLETION_TOKEN is defined in
                EFI_TCP4_PROTOCOL.Connect().

  @retval  EFI_SUCCESS             The asynchronous I/O request is aborted and Token->Event
                                   is signaled.
  @retval  EFI_INVALID_PARAMETER   This is NULL.
  @retval  EFI_NOT_STARTED         This instance hasn't been configured.
  @retval  EFI_NO_MAPPING          When using the default address, configuration
                                   (DHCP, BOOTP,RARP, etc.) hasn't finished yet.
  @retval  EFI_NOT_FOUND           The asynchronous I/O request isn't found in the
                                   transmission or receive queue. It has either
                                   completed or wasn't issued by Transmit() and Receive().
  @retval  EFI_UNSUPPORTED         The implementation does not support this function.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP4_CANCEL)(
  IN EFI_TCP4_PROTOCOL                   *This,
  IN EFI_TCP4_COMPLETION_TOKEN           *Token OPTIONAL
  );


/**
  Poll to receive incoming data and transmit outgoing segments.

  @param  This The pointer to the EFI_TCP4_PROTOCOL instance.

  @retval  EFI_SUCCESS           Incoming or outgoing data was processed.
  @retval  EFI_INVALID_PARAMETER This is NULL.
  @retval  EFI_DEVICE_ERROR      An unexpected system or network error occurred.
  @retval  EFI_NOT_READY         No incoming or outgoing data is processed.
  @retval  EFI_TIMEOUT           Data was dropped out of the transmission or receive queue.
                                 Consider increasing the polling rate.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TCP4_POLL)(
  IN EFI_TCP4_PROTOCOL                   *This
  );

///
/// The EFI_TCP4_PROTOCOL defines the EFI TCPv4 Protocol child to be used by
/// any network drivers or applications to send or receive data stream.
/// It can either listen on a specified port as a service or actively connected
/// to remote peer as a client. Each instance has its own independent settings,
/// such as the routing table.
///
struct _EFI_TCP4_PROTOCOL {
  EFI_TCP4_GET_MODE_DATA                 GetModeData;
  EFI_TCP4_CONFIGURE                     Configure;
  EFI_TCP4_ROUTES                        Routes;
  EFI_TCP4_CONNECT                       Connect;
  EFI_TCP4_ACCEPT                        Accept;
  EFI_TCP4_TRANSMIT                      Transmit;
  EFI_TCP4_RECEIVE                       Receive;
  EFI_TCP4_CLOSE                         Close;
  EFI_TCP4_CANCEL                        Cancel;
  EFI_TCP4_POLL                          Poll;
};

extern EFI_GUID gEfiTcp4ServiceBindingProtocolGuid;
extern EFI_GUID gEfiTcp4ProtocolGuid;

#endif
