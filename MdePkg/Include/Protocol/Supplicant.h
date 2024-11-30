/** @file
  This file defines the EFI Supplicant Protocol.

  Copyright (c) 2016 - 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.6

**/

#ifndef __EFI_SUPPLICANT_PROTOCOL_H__
#define __EFI_SUPPLICANT_PROTOCOL_H__

#include <Protocol/WiFi2.h>

///
/// The EFI Supplicant Service Binding Protocol is used to locate EFI
/// Supplicant Protocol drivers to create and destroy child of the driver to
/// communicate with other host using Supplicant protocol.
///
#define EFI_SUPPLICANT_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0x45bcd98e, 0x59ad, 0x4174, { 0x95, 0x46, 0x34, 0x4a, 0x7, 0x48, 0x58, 0x98 } \
  }

///
/// The EFI Supplicant protocol provides services to process authentication and
/// data encryption/decryption for security management.
///
#define EFI_SUPPLICANT_PROTOCOL_GUID \
  { \
    0x54fcc43e, 0xaa89, 0x4333, { 0x9a, 0x85, 0xcd, 0xea, 0x24, 0x5, 0x1e, 0x9e } \
  }

typedef struct _EFI_SUPPLICANT_PROTOCOL EFI_SUPPLICANT_PROTOCOL;

///
/// EFI_SUPPLICANT_CRYPT_MODE
///
typedef enum {
  //
  // Encrypt data provided in the fragment buffers.
  //
  EfiSupplicantEncrypt,
  //
  // Decrypt data provided in the fragment buffers.
  //
  EfiSupplicantDecrypt,
} EFI_SUPPLICANT_CRYPT_MODE;

///
/// EFI_SUPPLICANT_DATA_TYPE
///
typedef enum {
  //
  // Session Configuration
  //

  //
  // Current authentication type in use. The corresponding Data is of type
  // EFI_80211_AKM_SUITE_SELECTOR.
  //
  EfiSupplicant80211AKMSuite,
  //
  // Group data encryption type in use. The corresponding Data is of type
  // EFI_SUPPLICANT_CIPHER_SUITE_SELECTOR.
  //
  EfiSupplicant80211GroupDataCipherSuite,
  //
  // Pairwise encryption type in use. The corresponding Data is of type
  // EFI_80211_CIPHER_SUITE_SELECTOR.
  //
  EfiSupplicant80211PairwiseCipherSuite,
  //
  // PSK password. The corresponding Data is a NULL-terminated ASCII string.
  //
  EfiSupplicant80211PskPassword,
  //
  // Target SSID name. The corresponding Data is of type EFI_80211_SSID.
  //
  EfiSupplicant80211TargetSSIDName,
  //
  // Station MAC address. The corresponding Data is of type
  // EFI_80211_MAC_ADDRESS.
  //
  EfiSupplicant80211StationMac,
  //
  // Target SSID MAC address. The corresponding Data is 6 bytes MAC address.
  //
  EfiSupplicant80211TargetSSIDMac,

  //
  // Session Information
  //

  //
  // 802.11 PTK. The corresponding Data is of type EFI_SUPPLICANT_KEY.
  //
  EfiSupplicant80211PTK,
  //
  // 802.11 GTK. The corresponding Data is of type EFI_SUPPLICANT_GTK_LIST.
  //
  EfiSupplicant80211GTK,
  //
  // Supplicant state. The corresponding Data is
  // EFI_EAPOL_SUPPLICANT_PAE_STATE.
  //
  EfiSupplicantState,
  //
  // 802.11 link state. The corresponding Data is EFI_80211_LINK_STATE.
  //
  EfiSupplicant80211LinkState,
  //
  // Flag indicates key is refreshed. The corresponding Data is
  // EFI_SUPPLICANT_KEY_REFRESH.
  //
  EfiSupplicantKeyRefresh,

  //
  // Session Configuration
  //

  //
  // Supported authentication types. The corresponding Data is of type
  // EFI_80211_AKM_SUITE_SELECTOR.
  //
  EfiSupplicant80211SupportedAKMSuites,
  //
  // Supported software encryption types provided by supplicant driver. The
  // corresponding Data is of type EFI_80211_CIPHER_SUITE_SELECTOR.
  //
  EfiSupplicant80211SupportedSoftwareCipherSuites,
  //
  // Supported hardware encryption types provided by wireless UNDI driver. The
  // corresponding Data is of type EFI_80211_CIPHER_SUITE_SELECTOR.
  //
  EfiSupplicant80211SupportedHardwareCipherSuites,

  //
  // Session Information
  //

  //
  // 802.11 Integrity GTK. The corresponding Data is of type
  // EFI_SUPPLICANT_GTK_LIST.
  //
  EfiSupplicant80211IGTK,
  //
  // 802.11 PMK. The corresponding Data is 32 bytes pairwise master key.
  //
  EfiSupplicant80211PMK,
  EfiSupplicantDataTypeMaximum
} EFI_SUPPLICANT_DATA_TYPE;

///
/// EFI_80211_LINK_STATE
///
typedef enum {
  //
  // Indicates initial start state, unauthenticated, unassociated.
  //
  Ieee80211UnauthenticatedUnassociated,
  //
  // Indicates authenticated, unassociated.
  //
  Ieee80211AuthenticatedUnassociated,
  //
  // Indicates authenticated and associated, but pending RSN authentication.
  //
  Ieee80211PendingRSNAuthentication,
  //
  // Indicates authenticated and associated.
  //
  Ieee80211AuthenticatedAssociated
} EFI_80211_LINK_STATE;

///
/// EFI_SUPPLICANT_KEY_TYPE (IEEE Std 802.11 Section 6.3.19.1.2)
///
typedef enum {
  Group,
  Pairwise,
  PeerKey,
  IGTK
} EFI_SUPPLICANT_KEY_TYPE;

///
/// EFI_SUPPLICANT_KEY_DIRECTION (IEEE Std 802.11 Section 6.3.19.1.2)
///
typedef enum {
  //
  // Indicates that the keys are being installed for the receive direction.
  //
  Receive,
  //
  // Indicates that the keys are being installed for the transmit direction.
  //
  Transmit,
  //
  // Indicates that the keys are being installed for both the receive and
  // transmit directions.
  //
  Both
} EFI_SUPPLICANT_KEY_DIRECTION;

///
/// EFI_SUPPLICANT_KEY_REFRESH
///
typedef struct {
  //
  // If TRUE, indicates GTK is just refreshed after a successful call to
  // EFI_SUPPLICANT_PROTOCOL.BuildResponsePacket().
  //
  BOOLEAN    GTKRefresh;
} EFI_SUPPLICANT_KEY_REFRESH;

#define EFI_MAX_KEY_LEN  64

///
/// EFI_SUPPLICANT_KEY
///
typedef struct {
  //
  // The key value.
  //
  UINT8                           Key[EFI_MAX_KEY_LEN];
  //
  // Length in bytes of the Key. Should be up to EFI_MAX_KEY_LEN.
  //
  UINT8                           KeyLen;
  //
  // The key identifier.
  //
  UINT8                           KeyId;
  //
  // Defines whether this key is a group key, pairwise key, PeerKey, or
  // Integrity Group.
  //
  EFI_SUPPLICANT_KEY_TYPE         KeyType;
  //
  // The value is set according to the KeyType.
  //
  EFI_80211_MAC_ADDRESS           Addr;
  //
  // The Receive Sequence Count value.
  //
  UINT8                           Rsc[8];
  //
  // Length in bytes of the Rsc. Should be up to 8.
  //
  UINT8                           RscLen;
  //
  // Indicates whether the key is configured by the Authenticator or
  // Supplicant. The value true indicates Authenticator.
  //
  BOOLEAN                         IsAuthenticator;
  //
  // The cipher suite required for this association.
  //
  EFI_80211_SUITE_SELECTOR        CipherSuite;
  //
  // Indicates the direction for which the keys are to be installed.
  //
  EFI_SUPPLICANT_KEY_DIRECTION    Direction;
} EFI_SUPPLICANT_KEY;

///
/// EFI_SUPPLICANT_GTK_LIST
///
typedef struct {
  //
  // Indicates the number of GTKs that are contained in GTKList.
  //
  UINT8                 GTKCount;
  //
  // A variable-length array of GTKs of type EFI_SUPPLICANT_KEY. The number of
  // entries is specified by GTKCount.
  //
  EFI_SUPPLICANT_KEY    GTKList[1];
} EFI_SUPPLICANT_GTK_LIST;

///
/// EFI_SUPPLICANT_FRAGMENT_DATA
///
typedef struct {
  //
  // Length of data buffer in the fragment.
  //
  UINT32    FragmentLength;
  //
  // Pointer to the data buffer in the fragment.
  //
  VOID      *FragmentBuffer;
} EFI_SUPPLICANT_FRAGMENT_DATA;

/**
  BuildResponsePacket() is called during STA and AP authentication is in
  progress. Supplicant derives the PTK or session keys depend on type of
  authentication is being employed.

  @param[in]       This           Pointer to the EFI_SUPPLICANT_PROTOCOL
                                  instance.
  @param[in]       RequestBuffer  Pointer to the most recently received EAPOL
                                  packet. NULL means the supplicant need
                                  initiate the EAP authentication session and
                                  send EAPOL-Start message.
  @param[in]       RequestBufferSize
                                  Packet size in bytes for the most recently
                                  received EAPOL packet. 0 is only valid when
                                  RequestBuffer is NULL.
  @param[out]      Buffer         Pointer to the buffer to hold the built
                                  packet.
  @param[in, out]  BufferSize     Pointer to the buffer size in bytes. On
                                  input, it is the buffer size provided by the
                                  caller. On output, it is the buffer size in
                                  fact needed to contain the packet.

  @retval EFI_SUCCESS             The required EAPOL packet is built
                                  successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  RequestBuffer is NULL, but RequestSize is
                                  NOT 0.
                                  RequestBufferSize is 0.
                                  Buffer is NULL, but RequestBuffer is NOT 0.
                                  BufferSize is NULL.
  @retval EFI_BUFFER_TOO_SMALL    BufferSize is too small to hold the response
                                  packet.
  @retval EFI_NOT_READY           Current EAPOL session state is NOT ready to
                                  build ResponsePacket.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SUPPLICANT_BUILD_RESPONSE_PACKET)(
  IN     EFI_SUPPLICANT_PROTOCOL          *This,
  IN     UINT8                            *RequestBuffer      OPTIONAL,
  IN     UINTN                            RequestBufferSize   OPTIONAL,
  OUT UINT8                            *Buffer,
  IN OUT UINTN                            *BufferSize
  );

/**
  ProcessPacket() is called to Supplicant driver to encrypt or decrypt the data
  depending type of authentication type.

  @param[in]       This           Pointer to the EFI_SUPPLICANT_PROTOCOL
                                  instance.
  @param[in, out]  FragmentTable  Pointer to a list of fragment. The caller
                                  will take responsible to handle the original
                                  FragmentTable while it may be reallocated in
                                  Supplicant driver.
  @param[in]       FragmentCount  Number of fragment.
  @param[in]       CryptMode      Crypt mode.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  FragmentTable is NULL.
                                  FragmentCount is NULL.
                                  CryptMode is invalid.
  @retval EFI_NOT_READY           Current supplicant state is NOT Authenticated.
  @retval EFI_ABORTED             Something wrong decryption the message.
  @retval EFI_UNSUPPORTED         This API is not supported.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SUPPLICANT_PROCESS_PACKET)(
  IN     EFI_SUPPLICANT_PROTOCOL          *This,
  IN OUT EFI_SUPPLICANT_FRAGMENT_DATA     **FragmentTable,
  IN     UINT32                           *FragmentCount,
  IN     EFI_SUPPLICANT_CRYPT_MODE        CryptMode
  );

/**
  Set Supplicant configuration data.

  @param[in]  This                Pointer to the EFI_SUPPLICANT_PROTOCOL
                                  instance.
  @param[in]  DataType            The type of data.
  @param[in]  Data                Pointer to the buffer to hold the data.
  @param[in]  DataSize            Pointer to the buffer size in bytes.

  @retval EFI_SUCCESS             The Supplicant configuration data is set
                                  successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  Data is NULL.
                                  DataSize is 0.
  @retval EFI_UNSUPPORTED         The DataType is unsupported.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SUPPLICANT_SET_DATA)(
  IN EFI_SUPPLICANT_PROTOCOL              *This,
  IN EFI_SUPPLICANT_DATA_TYPE             DataType,
  IN VOID                                 *Data,
  IN UINTN                                DataSize
  );

/**
  Get Supplicant configuration data.

  @param[in]       This           Pointer to the EFI_SUPPLICANT_PROTOCOL
                                  instance.
  @param[in]       DataType       The type of data.
  @param[out]      Data           Pointer to the buffer to hold the data.
                                  Ignored if DataSize is 0.
  @param[in, out]  DataSize       Pointer to the buffer size in bytes. On
                                  input, it is the buffer size provided by the
                                  caller. On output, it is the buffer size in
                                  fact needed to contain the packet.

  @retval EFI_SUCCESS             The Supplicant configuration data is got
                                  successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  This is NULL.
                                  DataSize is NULL.
                                  Data is NULL if *DataSize is not zero.
  @retval EFI_UNSUPPORTED         The DataType is unsupported.
  @retval EFI_NOT_FOUND           The Supplicant configuration data is not
                                  found.
  @retval EFI_BUFFER_TOO_SMALL    The size of Data is too small for the
                                  specified configuration data and the required
                                  size is returned in DataSize.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SUPPLICANT_GET_DATA)(
  IN     EFI_SUPPLICANT_PROTOCOL          *This,
  IN     EFI_SUPPLICANT_DATA_TYPE         DataType,
  OUT UINT8                            *Data      OPTIONAL,
  IN OUT UINTN                            *DataSize
  );

///
/// The EFI_SUPPLICANT_PROTOCOL is designed to provide unified place for WIFI
/// and EAP security management. Both PSK authentication and 802.1X EAP
/// authentication can be managed via this protocol and driver or application
/// as a consumer can only focus on about packet transmitting or receiving.
///
struct _EFI_SUPPLICANT_PROTOCOL {
  EFI_SUPPLICANT_BUILD_RESPONSE_PACKET    BuildResponsePacket;
  EFI_SUPPLICANT_PROCESS_PACKET           ProcessPacket;
  EFI_SUPPLICANT_SET_DATA                 SetData;
  EFI_SUPPLICANT_GET_DATA                 GetData;
};

extern EFI_GUID  gEfiSupplicantServiceBindingProtocolGuid;
extern EFI_GUID  gEfiSupplicantProtocolGuid;

#endif
