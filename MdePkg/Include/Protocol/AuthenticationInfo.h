/** @file
  EFI_AUTHENTICATION_INFO_PROTOCOL as defined in UEFI 2.0.
  This protocol is used on any device handle to obtain authentication information
  associated with the physical or logical device.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __AUTHENTICATION_INFO_H__
#define __AUTHENTICATION_INFO_H__

#define EFI_AUTHENTICATION_INFO_PROTOCOL_GUID \
  { \
    0x7671d9d0, 0x53db, 0x4173, {0xaa, 0x69, 0x23, 0x27, 0xf2, 0x1f, 0x0b, 0xc7 } \
  }

#define EFI_AUTHENTICATION_CHAP_RADIUS_GUID \
  { \
    0xd6062b50, 0x15ca, 0x11da, {0x92, 0x19, 0x00, 0x10, 0x83, 0xff, 0xca, 0x4d } \
  }

#define EFI_AUTHENTICATION_CHAP_LOCAL_GUID \
  { \
    0xc280c73e, 0x15ca, 0x11da, {0xb0, 0xca, 0x00, 0x10, 0x83, 0xff, 0xca, 0x4d } \
  }

typedef struct _EFI_AUTHENTICATION_INFO_PROTOCOL EFI_AUTHENTICATION_INFO_PROTOCOL;

#pragma pack(1)
typedef struct {
  ///
  /// Authentication Type GUID.
  ///
  EFI_GUID         Guid;

  ///
  /// Length of this structure in bytes.
  ///
  UINT16           Length;
} AUTH_NODE_HEADER;

typedef struct {
  AUTH_NODE_HEADER Header;

  ///
  /// RADIUS Server IPv4 or IPv6 Address.
  ///
  UINT8            RadiusIpAddr[16];         ///< IPv4 or IPv6 address.

  ///
  /// Reserved for future use.
  ///
  UINT16           Reserved;

  ///
  /// Network Access Server IPv4 or IPv6 Address (OPTIONAL).
  ///
  UINT8            NasIpAddr[16];            ///< IPv4 or IPv6 address.

  ///
  /// Network Access Server Secret Length in bytes (OPTIONAL).
  ///
  UINT16           NasSecretLength;

  ///
  /// Network Access Server Secret (OPTIONAL).
  ///
  UINT8            NasSecret[1];

  ///
  /// CHAP Initiator Secret Length in bytes on offset NasSecret + NasSecretLength.
  ///
  /// UINT16           ChapSecretLength;
  ///
  /// CHAP Initiator Secret.
  ///
  /// UINT8            ChapSecret[];
  ///
  /// CHAP Initiator Name Length in bytes on offset ChapSecret + ChapSecretLength.
  ///
  /// UINT16           ChapNameLength;
  ///
  /// CHAP Initiator Name.
  ///
  /// UINT8            ChapName[];
  ///
  /// Reverse CHAP Name Length in bytes on offset ChapName + ChapNameLength.
  ///
  /// UINT16           ReverseChapNameLength;
  ///
  /// Reverse CHAP Name.
  ///
  /// UINT8            ReverseChapName[];
  ///
  /// Reverse CHAP Secret Length in bytes on offseet ReverseChapName + ReverseChapNameLength.
  ///
  /// UINT16           ReverseChapSecretLength;
  ///
  /// Reverse CHAP Secret.
  ///
  /// UINT8            ReverseChapSecret[];
  ///
} CHAP_RADIUS_AUTH_NODE;

typedef struct {
  AUTH_NODE_HEADER Header;

  ///
  /// Reserved for future use.
  ///
  UINT16           Reserved;

  ///
  /// User Secret Length in bytes.
  ///
  UINT16           UserSecretLength;

  ///
  /// User Secret.
  ///
  UINT8            UserSecret[1];

  ///
  /// User Name Length in bytes on offset UserSecret + UserSecretLength.
  ///
  /// UINT16           UserNameLength;
  ///
  /// User Name.
  ///
  /// UINT8            UserName[];
  ///
  /// CHAP Initiator Secret Length in bytes on offset UserName + UserNameLength.
  ///
  /// UINT16           ChapSecretLength;
  ///
  /// CHAP Initiator Secret.
  ///
  /// UINT8            ChapSecret[];
  ///
  /// CHAP Initiator Name Length in bytes on offset ChapSecret + ChapSecretLength.
  ///
  /// UINT16           ChapNameLength;
  ///
  /// CHAP Initiator Name.
  ///
  /// UINT8            ChapName[];
  ///
  /// Reverse CHAP Name Length in bytes on offset ChapName + ChapNameLength.
  ///
  /// UINT16           ReverseChapNameLength;
  ///
  /// Reverse CHAP Name.
  ///
  /// UINT8            ReverseChapName[];
  ///
  /// Reverse CHAP Secret Length in bytes on offset ReverseChapName + ReverseChapNameLength.
  ///
  /// UINT16           ReverseChapSecretLength;
  ///
  /// Reverse CHAP Secret.
  ///
  /// UINT8            ReverseChapSecret[];
  ///
} CHAP_LOCAL_AUTH_NODE;
#pragma pack()

/**
  Retrieves the authentication information associated with a particular controller handle.

  @param[in]  This                  The pointer to the EFI_AUTHENTICATION_INFO_PROTOCOL.
  @param[in]  ControllerHandle      The handle to the Controller.
  @param[out] Buffer                The pointer to the authentication information. This function is
                                    responsible for allocating the buffer and it is the caller's
                                    responsibility to free buffer when the caller is finished with buffer.

  @retval EFI_SUCCESS           Successfully retrieved authentication information
                                for the given ControllerHandle.
  @retval EFI_INVALID_PARAMETER No matching authentication information found for
                                the given ControllerHandle.
  @retval EFI_DEVICE_ERROR      The authentication information could not be retrieved
                                due to a hardware error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_AUTHENTICATION_INFO_PROTOCOL_GET)(
  IN  EFI_AUTHENTICATION_INFO_PROTOCOL *This,
  IN  EFI_HANDLE                       ControllerHandle,
  OUT VOID                             **Buffer
  );

/**
  Set the authentication information for a given controller handle.

  @param[in]  This                 The pointer to the EFI_AUTHENTICATION_INFO_PROTOCOL.
  @param[in]  ControllerHandle     The handle to the Controller.
  @param[in]  Buffer               The pointer to the authentication information.

  @retval EFI_SUCCESS          Successfully set authentication information for the
                               given ControllerHandle.
  @retval EFI_UNSUPPORTED      If the platform policies do not allow setting of
                               the authentication information.
  @retval EFI_DEVICE_ERROR     The authentication information could not be configured
                               due to a hardware error.
  @retval EFI_OUT_OF_RESOURCES Not enough storage is available to hold the data.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_AUTHENTICATION_INFO_PROTOCOL_SET)(
  IN EFI_AUTHENTICATION_INFO_PROTOCOL  *This,
  IN EFI_HANDLE                        ControllerHandle,
  IN VOID                              *Buffer
  );

///
/// This protocol is used on any device handle to obtain authentication
/// information associated with the physical or logical device.
///
struct _EFI_AUTHENTICATION_INFO_PROTOCOL {
  EFI_AUTHENTICATION_INFO_PROTOCOL_GET Get;
  EFI_AUTHENTICATION_INFO_PROTOCOL_SET Set;
};

extern EFI_GUID gEfiAuthenticationInfoProtocolGuid;
extern EFI_GUID gEfiAuthenticationChapRadiusGuid;
extern EFI_GUID gEfiAuthenticationChapLocalGuid;

#endif
