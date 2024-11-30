/** @file
  This file defines the EFI Wireless MAC Connection II Protocol.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.6

**/

#ifndef __EFI_WIFI2_PROTOCOL_H__
#define __EFI_WIFI2_PROTOCOL_H__

#define EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL_GUID \
  { \
    0x1b0fb9bf, 0x699d, 0x4fdd, { 0xa7, 0xc3, 0x25, 0x46, 0x68, 0x1b, 0xf6, 0x3b } \
  }

typedef struct _EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL;

///
/// EFI_80211_BSS_TYPE
///
typedef enum {
  IeeeInfrastructureBSS,
  IeeeIndependentBSS,
  IeeeMeshBSS,
  IeeeAnyBss
} EFI_80211_BSS_TYPE;

///
/// EFI_80211_CONNECT_NETWORK_RESULT_CODE
///
typedef enum {
  //
  // The connection establishment operation finished successfully.
  //
  ConnectSuccess,
  //
  // The connection was refused by the Network.
  //
  ConnectRefused,
  //
  // The connection establishment operation failed (i.e, Network is not
  // detected).
  //
  ConnectFailed,
  //
  // The connection establishment operation was terminated on timeout.
  //
  ConnectFailureTimeout,
  //
  // The connection establishment operation failed on other reason.
  //
  ConnectFailedReasonUnspecified
} EFI_80211_CONNECT_NETWORK_RESULT_CODE;

///
/// EFI_80211_MAC_ADDRESS
///
typedef struct {
  UINT8    Addr[6];
} EFI_80211_MAC_ADDRESS;

#define EFI_MAX_SSID_LEN  32

///
/// EFI_80211_SSID
///
typedef struct {
  //
  // Length in bytes of the SSId. If zero, ignore SSId field.
  //
  UINT8    SSIdLen;
  //
  // Specifies the service set identifier.
  //
  UINT8    SSId[EFI_MAX_SSID_LEN];
} EFI_80211_SSID;

///
/// EFI_80211_GET_NETWORKS_DATA
///
typedef struct {
  //
  // The number of EFI_80211_SSID in SSIDList. If zero, SSIDList should be
  // ignored.
  //
  UINT32            NumOfSSID;
  //
  // The SSIDList is a pointer to an array of EFI_80211_SSID instances. The
  // number of entries is specified by NumOfSSID. The array should only include
  // SSIDs of hidden networks. It is suggested that the caller inputs less than
  // 10 elements in the SSIDList. It is the caller's responsibility to free
  // this buffer.
  //
  EFI_80211_SSID    SSIDList[1];
} EFI_80211_GET_NETWORKS_DATA;

///
/// EFI_80211_SUITE_SELECTOR
///
typedef struct {
  //
  // Organization Unique Identifier, as defined in IEEE 802.11 standard,
  // usually set to 00-0F-AC.
  //
  UINT8    Oui[3];
  //
  // Suites types, as defined in IEEE 802.11 standard.
  //
  UINT8    SuiteType;
} EFI_80211_SUITE_SELECTOR;

///
/// EFI_80211_AKM_SUITE_SELECTOR
///
typedef struct {
  //
  // Indicates the number of AKM suite selectors that are contained in
  // AKMSuiteList. If zero, the AKMSuiteList is ignored.
  //
  UINT16                      AKMSuiteCount;
  //
  // A variable-length array of AKM suites, as defined in IEEE 802.11 standard,
  // Table 8-101. The number of entries is specified by AKMSuiteCount.
  //
  EFI_80211_SUITE_SELECTOR    AKMSuiteList[1];
} EFI_80211_AKM_SUITE_SELECTOR;

///
/// EFI_80211_CIPHER_SUITE_SELECTOR
///
typedef struct {
  //
  // Indicates the number of cipher suites that are contained in
  // CipherSuiteList. If zero, the CipherSuiteList is ignored.
  //
  UINT16                      CipherSuiteCount;
  //
  // A variable-length array of cipher suites, as defined in IEEE 802.11
  // standard, Table 8-99. The number of entries is specified by
  // CipherSuiteCount.
  //
  EFI_80211_SUITE_SELECTOR    CipherSuiteList[1];
} EFI_80211_CIPHER_SUITE_SELECTOR;

///
/// EFI_80211_NETWORK
///
typedef struct {
  //
  // Specifies the type of the BSS.
  //
  EFI_80211_BSS_TYPE                 BSSType;
  //
  // Specifies the SSID of the BSS.
  //
  EFI_80211_SSID                     SSId;
  //
  // Pointer to the AKM suites supported in the wireless network.
  //
  EFI_80211_AKM_SUITE_SELECTOR       *AKMSuite;
  //
  // Pointer to the cipher suites supported in the wireless network.
  //
  EFI_80211_CIPHER_SUITE_SELECTOR    *CipherSuite;
} EFI_80211_NETWORK;

///
/// EFI_80211_NETWORK_DESCRIPTION
///
typedef struct {
  //
  // Specifies the found wireless network.
  //
  EFI_80211_NETWORK    Network;
  //
  // Indicates the network quality as a value between 0 to 100, where 100
  // indicates the highest network quality.
  //
  UINT8                NetworkQuality;
} EFI_80211_NETWORK_DESCRIPTION;

///
/// EFI_80211_GET_NETWORKS_RESULT
///
typedef struct {
  //
  // The number of EFI_80211_NETWORK_DESCRIPTION in NetworkDesc. If zero,
  // NetworkDesc should be ignored.
  //
  UINT8                            NumOfNetworkDesc;
  //
  // The NetworkDesc is a pointer to an array of EFI_80211_NETWORK_DESCRIPTION
  // instances. It is caller's responsibility to free this buffer.
  //
  EFI_80211_NETWORK_DESCRIPTION    NetworkDesc[1];
} EFI_80211_GET_NETWORKS_RESULT;

///
/// EFI_80211_GET_NETWORKS_TOKEN
///
typedef struct {
  //
  // If the status code returned by GetNetworks() is EFI_SUCCESS, then this
  // Event will be signaled after the Status field is updated by the EFI
  // Wireless MAC Connection Protocol II driver. The type of Event must be
  // EFI_NOTIFY_SIGNAL.
  //
  EFI_EVENT    Event;
  //
  // Will be set to one of the following values:
  // EFI_SUCCESS: The operation completed successfully.
  // EFI_NOT_FOUND: Failed to find available wireless networks.
  // EFI_DEVICE_ERROR: An unexpected network or system error occurred.
  // EFI_ACCESS_DENIED: The operation is not completed due to some underlying
  // hardware or software state.
  // EFI_NOT_READY: The operation is started but not yet completed.
  //
  EFI_STATUS                       Status;
  //
  // Pointer to the input data for getting networks.
  //
  EFI_80211_GET_NETWORKS_DATA      *Data;
  //
  // Indicates the scan result. It is caller's responsibility to free this
  // buffer.
  //
  EFI_80211_GET_NETWORKS_RESULT    *Result;
} EFI_80211_GET_NETWORKS_TOKEN;

///
/// EFI_80211_CONNECT_NETWORK_DATA
///
typedef struct {
  //
  // Specifies the wireless network to connect to.
  //
  EFI_80211_NETWORK    *Network;
  //
  // Specifies a time limit in seconds that is optionally present, after which
  // the connection establishment procedure is terminated by the UNDI driver.
  // This is an optional parameter and may be 0. Values of 5 seconds or higher
  // are recommended.
  //
  UINT32               FailureTimeout;
} EFI_80211_CONNECT_NETWORK_DATA;

///
/// EFI_80211_CONNECT_NETWORK_TOKEN
///
typedef struct {
  //
  // If the status code returned by ConnectNetwork() is EFI_SUCCESS, then this
  // Event will be signaled after the Status field is updated by the EFI
  // Wireless MAC Connection Protocol II driver. The type of Event must be
  // EFI_NOTIFY_SIGNAL.
  //
  EFI_EVENT    Event;
  //
  // Will be set to one of the following values:
  // EFI_SUCCESS: The operation completed successfully.
  // EFI_DEVICE_ERROR: An unexpected network or system error occurred.
  // EFI_ACCESS_DENIED: The operation is not completed due to some underlying
  // hardware or software state.
  // EFI_NOT_READY: The operation is started but not yet completed.
  //
  EFI_STATUS                               Status;
  //
  // Pointer to the connection data.
  //
  EFI_80211_CONNECT_NETWORK_DATA           *Data;
  //
  // Indicates the connection state.
  //
  EFI_80211_CONNECT_NETWORK_RESULT_CODE    ResultCode;
} EFI_80211_CONNECT_NETWORK_TOKEN;

///
/// EFI_80211_DISCONNECT_NETWORK_TOKEN
///
typedef struct {
  //
  // If the status code returned by DisconnectNetwork() is EFI_SUCCESS, then
  // this Event will be signaled after the Status field is updated by the EFI
  // Wireless MAC Connection Protocol II driver. The type of Event must be
  // EFI_NOTIFY_SIGNAL.
  //
  EFI_EVENT     Event;
  //
  // Will be set to one of the following values:
  // EFI_SUCCESS: The operation completed successfully
  // EFI_DEVICE_ERROR: An unexpected network or system error occurred.
  // EFI_ACCESS_DENIED: The operation is not completed due to some underlying
  // hardware or software state.
  //
  EFI_STATUS    Status;
} EFI_80211_DISCONNECT_NETWORK_TOKEN;

/**
  Request a survey of potential wireless networks that administrator can later
  elect to try to join.

  @param[in]  This                Pointer to the
                                  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL
                                  instance.
  @param[in]  Token               Pointer to the token for getting wireless
                                  network.

  @retval EFI_SUCCESS             The operation started, and an event will
                                  eventually be raised for the caller.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  This is NULL.
                                  Token is NULL.
  @retval EFI_UNSUPPORTED         One or more of the input parameters is not
                                  supported by this implementation.
  @retval EFI_ALREADY_STARTED     The operation of getting wireless network is
                                  already started.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be
                                  allocated.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_WIRELESS_MAC_CONNECTION_II_GET_NETWORKS)(
  IN EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL          *This,
  IN EFI_80211_GET_NETWORKS_TOKEN                     *Token
  );

/**
  Connect a wireless network specified by a particular SSID, BSS type and
  Security type.

  @param[in]  This                Pointer to the
                                  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL
                                  instance.
  @param[in]  Token               Pointer to the token for connecting wireless
                                  network.

  @retval EFI_SUCCESS             The operation started successfully. Results
                                  will be notified eventually.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  This is NULL.
                                  Token is NULL.
  @retval EFI_UNSUPPORTED         One or more of the input parameters are not
                                  supported by this implementation.
  @retval EFI_ALREADY_STARTED     The connection process is already started.
  @retval EFI_NOT_FOUND           The specified wireless network is not found.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be
                                  allocated.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_WIRELESS_MAC_CONNECTION_II_CONNECT_NETWORK)(
  IN EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL          *This,
  IN EFI_80211_CONNECT_NETWORK_TOKEN                  *Token
  );

/**
  Request a disconnection with current connected wireless network.

  @param[in]  This                Pointer to the
                                  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL
                                  instance.
  @param[in]  Token               Pointer to the token for disconnecting
                                  wireless network.

  @retval EFI_SUCCESS             The operation started successfully. Results
                                  will be notified eventually.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  This is NULL.
                                  Token is NULL.
  @retval EFI_UNSUPPORTED         One or more of the input parameters are not
                                  supported by this implementation.
  @retval EFI_NOT_FOUND           Not connected to a wireless network.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be
                                  allocated.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_WIRELESS_MAC_CONNECTION_II_DISCONNECT_NETWORK)(
  IN EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL          *This,
  IN EFI_80211_DISCONNECT_NETWORK_TOKEN               *Token
  );

///
/// The EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL provides network management
/// service interfaces for 802.11 network stack. It is used by network
/// applications (and drivers) to establish wireless connection with a wireless
/// network.
///
struct _EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL {
  EFI_WIRELESS_MAC_CONNECTION_II_GET_NETWORKS          GetNetworks;
  EFI_WIRELESS_MAC_CONNECTION_II_CONNECT_NETWORK       ConnectNetwork;
  EFI_WIRELESS_MAC_CONNECTION_II_DISCONNECT_NETWORK    DisconnectNetwork;
};

extern EFI_GUID  gEfiWiFi2ProtocolGuid;

#endif
