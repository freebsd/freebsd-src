/** @file
  This file defines the EFI DNSv6 (Domain Name Service version 6) Protocol. It is split
  into the following two main sections:
  DNSv6 Service Binding Protocol (DNSv6SB)
  DNSv6 Protocol (DNSv6)

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.5

**/

#ifndef __EFI_DNS6_PROTOCOL_H__
#define __EFI_DNS6_PROTOCOL_H__

#define EFI_DNS6_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0x7f1647c8, 0xb76e, 0x44b2, {0xa5, 0x65, 0xf7, 0xf, 0xf1, 0x9c, 0xd1, 0x9e } \
  }

#define EFI_DNS6_PROTOCOL_GUID \
  { \
    0xca37bc1f, 0xa327, 0x4ae9, {0x82, 0x8a, 0x8c, 0x40, 0xd8, 0x50, 0x6a, 0x17 } \
  }

typedef struct _EFI_DNS6_PROTOCOL EFI_DNS6_PROTOCOL;

///
/// EFI_DNS6_CONFIG_DATA
///
typedef struct {
  ///
  /// If TRUE, enable DNS cache function for this DNS instance. If FALSE, all DNS query
  /// will not lookup local DNS cache.
  ///
  BOOLEAN                       EnableDnsCache;
  ///
  /// Use the protocol number defined in
  /// http://www.iana.org/assignments/protocol-numbers. Beside TCP/UDP, Other protocol
  /// is invalid value. An implementation can choose to support UDP, or both TCP and UDP.
  ///
  UINT8                         Protocol;
  ///
  /// The local IP address to use. Set to zero to let the underlying IPv6
  /// driver choose a source address. If not zero it must be one of the
  /// configured IP addresses in the underlying IPv6 driver.
  ///
  EFI_IPv6_ADDRESS              StationIp;
  ///
  /// Local port number. Set to zero to use the automatically assigned port number.
  ///
  UINT16                        LocalPort;
  ///
  /// Count of the DNS servers. When used with GetModeData(),
  /// this field is the count of originally configured servers when
  /// Configure() was called for this instance. When used with
  /// Configure() this is the count of caller-supplied servers. If the
  /// DnsServerListCount is zero, the DNS server configuration
  /// will be retrieved from DHCP server automatically.
  ///
  UINT32                        DnsServerCount;
  ///
  /// Pointer to DNS server list containing DnsServerListCount
  /// entries or NULL if DnsServerListCount is 0. For Configure(),
  /// this will be NULL when there are no caller supplied server addresses
  /// and the DNS instance will retrieve DNS server from DHCP Server.
  /// The provided DNS server list is recommended to be filled up in the sequence
  /// of preference. When used with GetModeData(), the buffer containing the list
  /// will be allocated by the driver implementing this protocol and must be
  /// freed by the caller. When used with Configure(), the buffer
  /// containing the list will be allocated and released by the caller.
  ///
  EFI_IPv6_ADDRESS              *DnsServerList;
  ///
  /// Retry number if no response received after RetryInterval.
  ///
  UINT32                        RetryCount;
  ///
  /// Minimum interval of retry is 2 second. If the retry interval is less than 2
  /// seconds, then use the 2 seconds.
  UINT32                        RetryInterval;
} EFI_DNS6_CONFIG_DATA;

///
/// EFI_DNS6_CACHE_ENTRY
///
typedef struct {
  ///
  /// Host name. This should be interpreted as Unicode characters.
  ///
  CHAR16                        *HostName;
  ///
  /// IP address of this host.
  ///
  EFI_IPv6_ADDRESS              *IpAddress;
  ///
  /// Time in second unit that this entry will remain in DNS cache. A value of zero means
  /// that this entry is permanent. A nonzero value will override the existing one if
  /// this entry to be added is dynamic entry. Implementations may set its default
  /// timeout value for the dynamically created DNS cache entry after one DNS resolve
  /// succeeds.
  UINT32                        Timeout;
} EFI_DNS6_CACHE_ENTRY;

///
/// EFI_DNS6_MODE_DATA
///
typedef struct {
  ///
  /// The configuration data of this instance.
  ///
  EFI_DNS6_CONFIG_DATA          DnsConfigData;
  ///
  /// Number of configured DNS6 servers.
  ///
  UINT32                         DnsServerCount;
  ///
  /// Pointer to common list of addresses of all configured DNS server used by EFI_DNS6_PROTOCOL
  /// instances. List will include DNS servers configured by this or any other EFI_DNS6_PROTOCOL
  /// instance. The storage for this list is allocated by the driver publishing this protocol,
  /// and must be freed by the caller.
  ///
  EFI_IPv6_ADDRESS               *DnsServerList;
  ///
  /// Number of DNS Cache entries. The DNS Cache is shared among all DNS instances.
  ///
  UINT32                        DnsCacheCount;
  ///
  /// Pointer to a buffer containing DnsCacheCount DNS Cache
  /// entry structures. The storage for thislist is allocated by the driver
  /// publishing this protocol and must be freed by caller.
  ///
  EFI_DNS6_CACHE_ENTRY          *DnsCacheList;
} EFI_DNS6_MODE_DATA;

///
/// DNS6_HOST_TO_ADDR_DATA
///
typedef struct {
  ///
  /// Number of the returned IP address.
  ///
  UINT32                        IpCount;
  ///
  /// Pointer to the all the returned IP address.
  ///
  EFI_IPv6_ADDRESS              *IpList;
} DNS6_HOST_TO_ADDR_DATA;

///
/// DNS6_ADDR_TO_HOST_DATA
///
typedef struct {
  ///
  /// Pointer to the primary name for this host address. It's the caller's
  /// responsibility to free the response memory.
  ///
  CHAR16                        *HostName;
} DNS6_ADDR_TO_HOST_DATA;

///
/// DNS6_RESOURCE_RECORD
///
typedef struct {
  ///
  /// The Owner name.
  ///
  CHAR8                         *QName;
  ///
  /// The Type Code of this RR.
  ///
  UINT16                        QType;
  ///
  /// The CLASS code of this RR.
  ///
  UINT16                        QClass;
  ///
  /// 32 bit integer which specify the time interval that the resource record may be
  /// cached before the source of the information should again be consulted. Zero means
  /// this RR cannot be cached.
  ///
  UINT32                        TTL;
  ///
  /// 16 big integer which specify the length of RData.
  ///
  UINT16                        DataLength;
  ///
  /// A string of octets that describe the resource, the format of this information
  /// varies according to QType and QClass difference.
  ///
  CHAR8                         *RData;
} DNS6_RESOURCE_RECORD;

///
/// DNS6_GENERAL_LOOKUP_DATA
///
typedef struct {
  ///
  /// Number of returned matching RRs.
  ///
  UINTN                         RRCount;
  ///
  /// Pointer to the all the returned matching RRs. It's caller responsibility to free
  /// the allocated memory to hold the returned RRs.
  ///
  DNS6_RESOURCE_RECORD          *RRList;
} DNS6_GENERAL_LOOKUP_DATA;

///
/// EFI_DNS6_COMPLETION_TOKEN
///
typedef struct {
  ///
  /// This Event will be signaled after the Status field is updated by the EFI DNSv6
  /// protocol driver. The type of Event must be EFI_NOTIFY_SIGNAL.
  ///
  EFI_EVENT                               Event;
  ///
  /// Will be set to one of the following values:
  ///   EFI_SUCCESS:      The host name to address translation completed successfully.
  ///   EFI_NOT_FOUND:    No matching Resource Record (RR) is found.
  ///   EFI_TIMEOUT:      No DNS server reachable, or RetryCount was exhausted without
  ///                     response from all specified DNS servers.
  ///   EFI_DEVICE_ERROR: An unexpected system or network error occurred.
  ///   EFI_NO_MEDIA:     There was a media error.
  ///
  EFI_STATUS                              Status;
  ///
  /// The parameter configured through DNSv6.Configure() interface. Retry number if no
  /// response received after RetryInterval.
  ///
  UINT32                                  RetryCount;
  ///
  /// The parameter configured through DNSv6.Configure() interface. Minimum interval of
  /// retry is 2 seconds. If the retry interval is less than 2 seconds, then use the 2
  /// seconds.
  ///
  UINT32                                  RetryInterval;
  ///
  /// DNSv6 completion token data
  ///
  union {
    ///
    /// When the Token is used for host name to address translation, H2AData is a pointer
    /// to the DNS6_HOST_TO_ADDR_DATA.
    ///
    DNS6_HOST_TO_ADDR_DATA      *H2AData;
    ///
    /// When the Token is used for host address to host name translation, A2HData is a
    /// pointer to the DNS6_ADDR_TO_HOST_DATA.
    ///
    DNS6_ADDR_TO_HOST_DATA      *A2HData;
    ///
    /// When the Token is used for a general lookup function, GLookupDATA is a pointer to
    /// the DNS6_GENERAL_LOOKUP_DATA.
    ///
    DNS6_GENERAL_LOOKUP_DATA    *GLookupData;
  } RspData;
} EFI_DNS6_COMPLETION_TOKEN;

/**
  Retrieve mode data of this DNS instance.

  This function is used to retrieve DNS mode data for this DNS instance.

  @param[in]   This                Pointer to EFI_DNS6_PROTOCOL instance.
  @param[out]  DnsModeData         Pointer to the caller-allocated storage for the
                                   EFI_DNS6_MODE_DATA data.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_NOT_STARTED         When DnsConfigData is queried, no configuration data
                                  is available because this instance has not been
                                  configured.
  @retval EFI_INVALID_PARAMETER   This is NULL or DnsModeData is NULL.
  @retval EFI_OUT_OF_RESOURCE     Failed to allocate needed resources.
**/
typedef
EFI_STATUS
(EFIAPI * EFI_DNS6_GET_MODE_DATA)(
  IN  EFI_DNS6_PROTOCOL         *This,
  OUT EFI_DNS6_MODE_DATA        *DnsModeData
  );

/**
  Configure this DNS instance.

  The Configure() function is used to set and change the configuration data for this
  EFI DNSv6 Protocol driver instance. Reset the DNS instance if DnsConfigData is NULL.

  @param[in]  This                Pointer to EFI_DNS6_PROTOCOL instance.
  @param[in]  DnsConfigData       Pointer to the configuration data structure. All associated
                                  storage to be allocated and released by caller.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_INVALID_PARAMETER   This is NULL.
                                  The StationIp address provided in DnsConfigData is not zero and not a valid unicast.
                                  DnsServerList is NULL while DnsServerList Count is not ZERO.
                                  DnsServerList Count is ZERO while DnsServerList is not NULL.
  @retval EFI_OUT_OF_RESOURCES    The DNS instance data or required space could not be allocated.
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred. The
                                  EFI DNSv6 Protocol instance is not configured.
  @retval EFI_UNSUPPORTED         The designated protocol is not supported.
  @retval EFI_ALREADY_STARTED     Second call to Configure() with DnsConfigData. To
                                  reconfigure the instance the caller must call Configure() with
                                  NULL first to return driver to unconfigured state.
**/
typedef
EFI_STATUS
(EFIAPI * EFI_DNS6_CONFIGURE)(
  IN EFI_DNS6_PROTOCOL          *This,
  IN EFI_DNS6_CONFIG_DATA       *DnsConfigData
  );

/**
  Host name to host address translation.

  The HostNameToIp () function is used to translate the host name to host IP address. A
  type AAAA query is used to get the one or more IPv6 addresses for this host.

  @param[in]  This                Pointer to EFI_DNS6_PROTOCOL instance.
  @param[in]  HostName            Host name.
  @param[in]  Token               Point to the completion token to translate host name
                                  to host address.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  Token is NULL.
                                  Token.Event is NULL.
                                  HostName is NULL or buffer contained unsupported characters.
  @retval EFI_NO_MAPPING          There's no source address is available for use.
  @retval EFI_ALREADY_STARTED     This Token is being used in another DNS session.
  @retval EFI_NOT_STARTED         This instance has not been started.
  @retval EFI_OUT_OF_RESOURCES    Failed to allocate needed resources.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DNS6_HOST_NAME_TO_IP) (
  IN  EFI_DNS6_PROTOCOL         *This,
  IN  CHAR16                    *HostName,
  IN  EFI_DNS6_COMPLETION_TOKEN *Token
  );

/**
  Host address to host name translation.

  The IpToHostName () function is used to translate the host address to host name. A
  type PTR query is used to get the primary name of the host. Implementation can choose
  to support this function or not.

  @param[in]  This                Pointer to EFI_DNS6_PROTOCOL instance.
  @param[in]  IpAddress           Ip Address.
  @param[in]  Token               Point to the completion token to translate host
                                  address to host name.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_UNSUPPORTED         This function is not supported.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  Token is NULL.
                                  Token.Event is NULL.
                                  IpAddress is not valid IP address.
  @retval EFI_NO_MAPPING          There's no source address is available for use.
  @retval EFI_NOT_STARTED         This instance has not been started.
  @retval EFI_OUT_OF_RESOURCES    Failed to allocate needed resources.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DNS6_IP_TO_HOST_NAME) (
  IN  EFI_DNS6_PROTOCOL         *This,
  IN  EFI_IPv6_ADDRESS          IpAddress,
  IN  EFI_DNS6_COMPLETION_TOKEN *Token
  );

/**
  This function provides capability to retrieve arbitrary information from the DNS
  server.

  This GeneralLookup() function retrieves arbitrary information from the DNS. The caller
  supplies a QNAME, QTYPE, and QCLASS, and all of the matching RRs are returned. All
  RR content (e.g., TTL) was returned. The caller need parse the returned RR to get
  required information. The function is optional. Implementation can choose to support
  it or not.

  @param[in]  This                Pointer to EFI_DNS6_PROTOCOL instance.
  @param[in]  QName               Pointer to Query Name.
  @param[in]  QType               Query Type.
  @param[in]  QClass              Query Name.
  @param[in]  Token               Point to the completion token to retrieve arbitrary
                                  information.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_UNSUPPORTED         This function is not supported. Or the requested
                                  QType is not supported
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  Token is NULL.
                                  Token.Event is NULL.
                                  QName is NULL.
  @retval EFI_NO_MAPPING          There's no source address is available for use.
  @retval EFI_NOT_STARTED         This instance has not been started.
  @retval EFI_OUT_OF_RESOURCES    Failed to allocate needed resources.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DNS6_GENERAL_LOOKUP) (
  IN  EFI_DNS6_PROTOCOL         *This,
  IN  CHAR8                     *QName,
  IN  UINT16                    QType,
  IN  UINT16                    QClass,
  IN  EFI_DNS6_COMPLETION_TOKEN *Token
  );

/**
  This function is to update the DNS Cache.

  The UpdateDnsCache() function is used to add/delete/modify DNS cache entry. DNS cache
  can be normally dynamically updated after the DNS resolve succeeds. This function
  provided capability to manually add/delete/modify the DNS cache.

  @param[in]  This                Pointer to EFI_DNS6_PROTOCOL instance.
  @param[in]  DeleteFlag          If FALSE, this function is to add one entry to the
                                  DNS Cahce. If TRUE, this function will delete
                                  matching DNS Cache entry.
  @param[in]  Override            If TRUE, the maching DNS cache entry will be
                                  overwritten with the supplied parameter. If FALSE,
                                  EFI_ACCESS_DENIED will be returned if the entry to
                                  be added is already existed.
  @param[in]  DnsCacheEntry       Pointer to DNS Cache entry.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  DnsCacheEntry.HostName is NULL.
                                  DnsCacheEntry.IpAddress is NULL.
                                  DnsCacheEntry.Timeout is zero.
  @retval EFI_ACCESS_DENIED       The DNS cache entry already exists and Override is
                                  not TRUE.
  @retval EFI_OUT_OF_RESOURCE     Failed to allocate needed resources.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DNS6_UPDATE_DNS_CACHE) (
  IN EFI_DNS6_PROTOCOL          *This,
  IN BOOLEAN                    DeleteFlag,
  IN BOOLEAN                    Override,
  IN EFI_DNS6_CACHE_ENTRY       DnsCacheEntry
  );

/**
  Polls for incoming data packets and processes outgoing data packets.

  The Poll() function can be used by network drivers and applications to increase the
  rate that data packets are moved between the communications device and the transmit
  and receive queues.

  In some systems, the periodic timer event in the managed network driver may not poll
  the underlying communications device fast enough to transmit and/or receive all data
  packets without missing incoming packets or dropping outgoing packets. Drivers and
  applications that are experiencing packet loss should try calling the Poll()
  function more often.

  @param[in]  This                Pointer to EFI_DNS6_PROTOCOL instance.

  @retval EFI_SUCCESS             Incoming or outgoing data was processed.
  @retval EFI_NOT_STARTED         This EFI DNS Protocol instance has not been started.
  @retval EFI_INVALID_PARAMETER   This is NULL.
  @retval EFI_NO_MAPPING          There is no source address is available for use.
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred.
  @retval EFI_TIMEOUT             Data was dropped out of the transmit and/or receive
                                  queue. Consider increasing the polling rate.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DNS6_POLL) (
  IN  EFI_DNS6_PROTOCOL         *This
  );

/**
  Abort an asynchronous DNS operation, including translation between IP and Host, and
  general look up behavior.

  The Cancel() function is used to abort a pending resolution request. After calling
  this function, Token.Status will be set to EFI_ABORTED and then Token.Event will be
  signaled. If the token is not in one of the queues, which usually means that the
  asynchronous operation has completed, this function will not signal the token and
  EFI_NOT_FOUND is returned.

  @param[in]  This                Pointer to EFI_DNS6_PROTOCOL instance.
  @param[in]  Token               Pointer to a token that has been issued by
                                  EFI_DNS6_PROTOCOL.HostNameToIp (),
                                  EFI_DNS6_PROTOCOL.IpToHostName() or
                                  EFI_DNS6_PROTOCOL.GeneralLookup().
                                  If NULL, all pending tokens are aborted.

  @retval EFI_SUCCESS             Incoming or outgoing data was processed.
  @retval EFI_NOT_STARTED         This EFI DNS6 Protocol instance has not been started.
  @retval EFI_INVALID_PARAMETER   This is NULL.
  @retval EFI_NO_MAPPING          There's no source address is available for use.
  @retval EFI_NOT_FOUND           When Token is not NULL, and the asynchronous DNS
                                  operation was not found in the transmit queue. It
                                  was either completed or was not issued by
                                  HostNameToIp(), IpToHostName() or GeneralLookup().
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DNS6_CANCEL) (
  IN  EFI_DNS6_PROTOCOL         *This,
  IN  EFI_DNS6_COMPLETION_TOKEN *Token
  );

///
/// The EFI_DNS6_PROTOCOL provides the function to get the host name and address
/// mapping, also provide pass through interface to retrieve arbitrary information from
/// DNSv6.
///
struct _EFI_DNS6_PROTOCOL {
  EFI_DNS6_GET_MODE_DATA        GetModeData;
  EFI_DNS6_CONFIGURE            Configure;
  EFI_DNS6_HOST_NAME_TO_IP      HostNameToIp;
  EFI_DNS6_IP_TO_HOST_NAME      IpToHostName;
  EFI_DNS6_GENERAL_LOOKUP       GeneralLookUp;
  EFI_DNS6_UPDATE_DNS_CACHE     UpdateDnsCache;
  EFI_DNS6_POLL                 Poll;
  EFI_DNS6_CANCEL               Cancel;
};

extern EFI_GUID gEfiDns6ServiceBindingProtocolGuid;
extern EFI_GUID gEfiDns6ProtocolGuid;

#endif
