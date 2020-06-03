/** @file
  EFI HTTP Utilities protocol provides a platform independent abstraction for HTTP
  message comprehension.

  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.5

**/

#ifndef __EFI_HTTP_UTILITIES_PROTOCOL_H__
#define __EFI_HTTP_UTILITIES_PROTOCOL_H__

#include <Protocol/Http.h>

#define EFI_HTTP_UTILITIES_PROTOCOL_GUID  \
  { \
    0x3e35c163, 0x4074, 0x45dd, {0x43, 0x1e, 0x23, 0x98, 0x9d, 0xd8, 0x6b, 0x32 } \
  }

typedef struct _EFI_HTTP_UTILITIES_PROTOCOL EFI_HTTP_UTILITIES_PROTOCOL;


/**
  Create HTTP header based on a combination of seed header, fields
  to delete, and fields to append.

  The Build() function is used to manage the headers portion of an
  HTTP message by providing the ability to add, remove, or replace
  HTTP headers.

  @param[in]  This                Pointer to EFI_HTTP_UTILITIES_PROTOCOL instance.
  @param[in]  SeedMessageSize     Size of the initial HTTP header. This can be zero.
  @param[in]  SeedMessage         Initial HTTP header to be used as a base for
                                  building a new HTTP header. If NULL,
                                  SeedMessageSize is ignored.
  @param[in]  DeleteCount         Number of null-terminated HTTP header field names
                                  in DeleteList.
  @param[in]  DeleteList          List of null-terminated HTTP header field names to
                                  remove from SeedMessage. Only the field names are
                                  in this list because the field values are irrelevant
                                  to this operation.
  @param[in]  AppendCount         Number of header fields in AppendList.
  @param[in]  AppendList          List of HTTP headers to populate NewMessage with.
                                  If SeedMessage is not NULL, AppendList will be
                                  appended to the existing list from SeedMessage in
                                  NewMessage.
  @param[out] NewMessageSize      Pointer to number of header fields in NewMessage.
  @param[out] NewMessage          Pointer to a new list of HTTP headers based on.

  @retval EFI_SUCCESS             Add, remove, and replace operations succeeded.
  @retval EFI_OUT_OF_RESOURCES    Could not allocate memory for NewMessage.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_HTTP_UTILS_BUILD) (
  IN  EFI_HTTP_UTILITIES_PROTOCOL  *This,
  IN  UINTN                        SeedMessageSize,
  IN  VOID                         *SeedMessage,   OPTIONAL
  IN  UINTN                        DeleteCount,
  IN  CHAR8                        *DeleteList[],  OPTIONAL
  IN  UINTN                        AppendCount,
  IN  EFI_HTTP_HEADER              *AppendList[],  OPTIONAL
  OUT UINTN                        *NewMessageSize,
  OUT VOID                         **NewMessage
  );

/**
  Parses HTTP header and produces an array of key/value pairs.

  The Parse() function is used to transform data stored in HttpHeader
  into a list of fields paired with their corresponding values.

  @param[in]  This                Pointer to EFI_HTTP_UTILITIES_PROTOCOL instance.
  @param[in]  HttpMessage         Contains raw unformatted HTTP header string.
  @param[in]  HttpMessageSize     Size of HTTP header.
  @param[out] HeaderFields        Array of key/value header pairs.
  @param[out] FieldCount          Number of headers in HeaderFields.

  @retval EFI_SUCCESS             Allocation succeeded.
  @retval EFI_NOT_STARTED         This EFI HTTP Protocol instance has not been
                                  initialized.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  HttpMessage is NULL.
                                  HeaderFields is NULL.
                                  FieldCount is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_HTTP_UTILS_PARSE) (
  IN  EFI_HTTP_UTILITIES_PROTOCOL  *This,
  IN  CHAR8                        *HttpMessage,
  IN  UINTN                        HttpMessageSize,
  OUT EFI_HTTP_HEADER              **HeaderFields,
  OUT UINTN                        *FieldCount
  );


///
/// EFI_HTTP_UTILITIES_PROTOCOL
/// designed to be used by EFI drivers and applications to parse HTTP
/// headers from a byte stream. This driver is neither dependent on
/// network connectivity, nor the existence of an underlying network
/// infrastructure.
///
struct _EFI_HTTP_UTILITIES_PROTOCOL {
  EFI_HTTP_UTILS_BUILD          Build;
  EFI_HTTP_UTILS_PARSE          Parse;
};

extern EFI_GUID gEfiHttpUtilitiesProtocolGuid;

#endif
