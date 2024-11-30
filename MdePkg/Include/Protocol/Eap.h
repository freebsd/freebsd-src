/** @file
  EFI EAP(Extended Authenticaton Protocol) Protocol Definition
  The EFI EAP Protocol is used to abstract the ability to configure and extend the
  EAP framework.
  The definitions in this file are defined in UEFI Specification 2.3.1B, which have
  not been verified by one implementation yet.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.2

**/

#ifndef __EFI_EAP_PROTOCOL_H__
#define __EFI_EAP_PROTOCOL_H__

#define EFI_EAP_PROTOCOL_GUID \
  { \
    0x5d9f96db, 0xe731, 0x4caa, {0xa0, 0xd, 0x72, 0xe1, 0x87, 0xcd, 0x77, 0x62 } \
  }

typedef struct _EFI_EAP_PROTOCOL EFI_EAP_PROTOCOL;

///
/// Type for the identification number assigned to the Port by the
/// System in which the Port resides.
///
typedef VOID *EFI_PORT_HANDLE;

///
/// EAP Authentication Method Type (RFC 3748)
///@{
#define EFI_EAP_TYPE_TLS  13///< REQUIRED - RFC 5216
///@}

//
// EAP_TYPE MD5, OTP and TOEKN_CARD has been removed from UEFI2.3.1B.
// Definitions are kept for backward compatibility.
//
#define EFI_EAP_TYPE_MD5         4
#define EFI_EAP_TYPE_OTP         5
#define EFI_EAP_TYPE_TOKEN_CARD  6

/**
  One user provided EAP authentication method.

  Build EAP response packet in response to the EAP request packet specified by
  (RequestBuffer, RequestSize).

  @param[in]      PortNumber     Specified the Port where the EAP request packet comes.
  @param[in]      RequestBuffer  Pointer to the most recently received EAP- Request packet.
  @param[in]      RequestSize    Packet size in bytes for the most recently received
                                 EAP-Request packet.
  @param[in]      Buffer         Pointer to the buffer to hold the built packet.
  @param[in, out] BufferSize     Pointer to the buffer size in bytes.
                                 On input, it is the buffer size provided by the caller.
                                 On output, it is the buffer size in fact needed to contain
                                 the packet.

  @retval EFI_SUCCESS            The required EAP response packet is built successfully.
  @retval others                 Failures are encountered during the packet building process.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EAP_BUILD_RESPONSE_PACKET)(
  IN EFI_PORT_HANDLE        PortNumber,
  IN UINT8                  *RequestBuffer,
  IN UINTN                  RequestSize,
  IN UINT8                  *Buffer,
  IN OUT UINTN              *BufferSize
  );

/**
  Set the desired EAP authentication method for the Port.

  The SetDesiredAuthMethod() function sets the desired EAP authentication method indicated
  by EapAuthType for the Port.

  If EapAuthType is an invalid EAP authentication type, then EFI_INVALID_PARAMETER is
  returned.
  If the EAP authentication method of EapAuthType is unsupported by the Ports, then it will
  return EFI_UNSUPPORTED.
  The cryptographic strength of EFI_EAP_TYPE_TLS shall be at least of hash strength
  SHA-256 and RSA key length of at least 2048 bits.

  @param[in] This                A pointer to the EFI_EAP_PROTOCOL instance that indicates
                                 the calling context.
  @param[in] EapAuthType         The type of the EAP authentication method to register. It should
                                 be the type value defined by RFC. See RFC 2284 for details.
  @param[in] Handler             The handler of the EAP authentication method to register.

  @retval EFI_SUCCESS            The EAP authentication method of EapAuthType is
                                 registered successfully.
  @retval EFI_INVALID_PARAMETER  EapAuthType is an invalid EAP authentication type.
  @retval EFI_UNSUPPORTED        The EAP authentication method of EapAuthType is
                                 unsupported by the Port.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EAP_SET_DESIRED_AUTHENTICATION_METHOD)(
  IN EFI_EAP_PROTOCOL            *This,
  IN UINT8                       EapAuthType
  );

/**
  Register an EAP authentication method.

  The RegisterAuthMethod() function registers the user provided EAP authentication method,
  the type of which is EapAuthType and the handler of which is Handler.

  If EapAuthType is an invalid EAP authentication type, then EFI_INVALID_PARAMETER is
  returned.
  If there is not enough system memory to perform the registration, then
  EFI_OUT_OF_RESOURCES is returned.

  @param[in] This                A pointer to the EFI_EAP_PROTOCOL instance that indicates
                                 the calling context.
  @param[in] EapAuthType         The type of the EAP authentication method to register. It should
                                 be the type value defined by RFC. See RFC 2284 for details.
  @param[in] Handler             The handler of the EAP authentication method to register.

  @retval EFI_SUCCESS            The EAP authentication method of EapAuthType is
                                 registered successfully.
  @retval EFI_INVALID_PARAMETER  EapAuthType is an invalid EAP authentication type.
  @retval EFI_OUT_OF_RESOURCES   There is not enough system memory to perform the registration.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_EAP_REGISTER_AUTHENTICATION_METHOD)(
  IN EFI_EAP_PROTOCOL                     *This,
  IN UINT8                                EapAuthType,
  IN EFI_EAP_BUILD_RESPONSE_PACKET        Handler
  );

///
/// EFI_EAP_PROTOCOL
/// is used to configure the desired EAP authentication method for the EAP
/// framework and extend the EAP framework by registering new EAP authentication
/// method on a Port. The EAP framework is built on a per-Port basis. Herein, a
/// Port means a NIC. For the details of EAP protocol, please refer to RFC 2284.
///
struct _EFI_EAP_PROTOCOL {
  EFI_EAP_SET_DESIRED_AUTHENTICATION_METHOD    SetDesiredAuthMethod;
  EFI_EAP_REGISTER_AUTHENTICATION_METHOD       RegisterAuthMethod;
};

extern EFI_GUID  gEfiEapProtocolGuid;

#endif
