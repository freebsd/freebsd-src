/** @file
  EFI TLS Configuration Protocol as defined in UEFI 2.5.
  The EFI TLS Configuration Protocol provides a way to set and get TLS configuration.

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.5

**/

#ifndef __EFI_TLS_CONFIGURATION_PROTOCOL_H__
#define __EFI_TLS_CONFIGURATION_PROTOCOL_H__

///
/// The EFI Configuration protocol provides a way to set and get TLS configuration.
///
#define EFI_TLS_CONFIGURATION_PROTOCOL_GUID  \
  { \
    0x1682fe44, 0xbd7a, 0x4407, { 0xb7, 0xc7, 0xdc, 0xa3, 0x7c, 0xa3, 0x92, 0x2d }  \
  }

typedef struct _EFI_TLS_CONFIGURATION_PROTOCOL EFI_TLS_CONFIGURATION_PROTOCOL;

///
/// EFI_TLS_CONFIG_DATA_TYPE
///
typedef enum {
  ///
  /// Local host configuration data: public certificate data.
  /// This data should be DER-encoded binary X.509 certificate
  /// or PEM-encoded X.509 certificate.
  ///
  EfiTlsConfigDataTypeHostPublicCert,
  ///
  /// Local host configuration data: private key data.
  ///
  EfiTlsConfigDataTypeHostPrivateKey,
  ///
  /// CA certificate to verify peer. This data should be PEM-encoded
  /// RSA or PKCS#8 private key.
  ///
  EfiTlsConfigDataTypeCACertificate,
  ///
  /// CA-supplied Certificate Revocation List data. This data should
  /// be DER-encoded CRL data.
  ///
  EfiTlsConfigDataTypeCertRevocationList,

  EfiTlsConfigDataTypeMaximum
} EFI_TLS_CONFIG_DATA_TYPE;

/**
  Set TLS configuration data.

  The SetData() function sets TLS configuration to non-volatile storage or volatile
  storage.

  @param[in]  This                Pointer to the EFI_TLS_CONFIGURATION_PROTOCOL instance.
  @param[in]  DataType            Configuration data type.
  @param[in]  Data                Pointer to configuration data.
  @param[in]  DataSize            Total size of configuration data.

  @retval EFI_SUCCESS             The TLS configuration data is set successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  Data is NULL.
                                  DataSize is 0.
  @retval EFI_UNSUPPORTED         The DataType is unsupported.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TLS_CONFIGURATION_SET_DATA)(
  IN EFI_TLS_CONFIGURATION_PROTOCOL  *This,
  IN EFI_TLS_CONFIG_DATA_TYPE        DataType,
  IN VOID                            *Data,
  IN UINTN                           DataSize
  );

/**
  Get TLS configuration data.

  The GetData() function gets TLS configuration.

  @param[in]       This           Pointer to the EFI_TLS_CONFIGURATION_PROTOCOL instance.
  @param[in]       DataType       Configuration data type.
  @param[in, out]  Data           Pointer to configuration data.
  @param[in, out]  DataSize       Total size of configuration data. On input, it means
                                  the size of Data buffer. On output, it means the size
                                  of copied Data buffer if EFI_SUCCESS, and means the
                                  size of desired Data buffer if EFI_BUFFER_TOO_SMALL.

  @retval EFI_SUCCESS             The TLS configuration data is got successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  DataSize is NULL.
                                  Data is NULL if *DataSize is not zero.
  @retval EFI_UNSUPPORTED         The DataType is unsupported.
  @retval EFI_NOT_FOUND           The TLS configuration data is not found.
  @retval EFI_BUFFER_TOO_SMALL    The buffer is too small to hold the data.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TLS_CONFIGURATION_GET_DATA)(
  IN EFI_TLS_CONFIGURATION_PROTOCOL  *This,
  IN EFI_TLS_CONFIG_DATA_TYPE        DataType,
  IN OUT VOID                        *Data   OPTIONAL,
  IN OUT UINTN                       *DataSize
  );

///
/// The EFI_TLS_CONFIGURATION_PROTOCOL is designed to provide a way to set and get
/// TLS configuration, such as Certificate, private key data.
///
struct _EFI_TLS_CONFIGURATION_PROTOCOL {
  EFI_TLS_CONFIGURATION_SET_DATA    SetData;
  EFI_TLS_CONFIGURATION_GET_DATA    GetData;
};

extern EFI_GUID  gEfiTlsConfigurationProtocolGuid;

#endif //__EFI_TLS_CONFIGURATION_PROTOCOL_H__
