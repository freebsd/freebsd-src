/** @file
  This file defines the EFI REST Protocol interface.

  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.5

**/

#ifndef __EFI_REST_PROTOCOL_H__
#define __EFI_REST_PROTOCOL_H__

#include <Protocol/Http.h>

#define EFI_REST_PROTOCOL_GUID \
  { \
    0x0db48a36, 0x4e54, 0xea9c, {0x9b, 0x09, 0x1e, 0xa5, 0xbe, 0x3a, 0x66, 0x0b } \
  }

typedef struct _EFI_REST_PROTOCOL EFI_REST_PROTOCOL;

/**
  Provides a simple HTTP-like interface to send and receive resources from a REST
  service.

  The SendReceive() function sends an HTTP request to this REST service, and returns a
  response when the data is retrieved from the service. RequestMessage contains the HTTP
  request to the REST resource identified by RequestMessage.Request.Url. The
  ResponseMessage is the returned HTTP response for that request, including any HTTP
  status.

  @param[in]  This                Pointer to EFI_REST_PROTOCOL instance for a particular
                                  REST service.
  @param[in]  RequestMessage      Pointer to the HTTP request data for this resource.
  @param[out] ResponseMessage     Pointer to the HTTP response data obtained for this
                                  requested.

  @retval EFI_SUCCESS             Operation succeeded.
  @retval EFI_INVALID_PARAMETER   This, RequestMessage, or ResponseMessage are NULL.
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_REST_SEND_RECEIVE) (
  IN  EFI_REST_PROTOCOL         *This,
  IN  EFI_HTTP_MESSAGE          *RequestMessage,
  OUT EFI_HTTP_MESSAGE          *ResponseMessage
  );

/**
  The GetServiceTime() function is an optional interface to obtain the current time from
  this REST service instance. If this REST service does not support retrieving the time,
  this function returns EFI_UNSUPPORTED.

  @param[in]  This                Pointer to EFI_REST_PROTOCOL instance.
  @param[out] Time                A pointer to storage to receive a snapshot of the
                                  current time of the REST service.

  @retval EFI_SUCCESS             Operation succeeded
  @retval EFI_INVALID_PARAMETER   This or Time are NULL.
  @retval EFI_UNSUPPORTED         The RESTful service does not support returning the
                                  time.
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_REST_GET_TIME) (
  IN  EFI_REST_PROTOCOL         *This,
  OUT EFI_TIME                  *Time
  );

///
/// The EFI REST protocol is designed to be used by EFI drivers and applications to send
/// and receive resources from a RESTful service. This protocol abstracts REST
/// (Representational State Transfer) client functionality. This EFI protocol could be
/// implemented to use an underlying EFI HTTP protocol, or it could rely on other
/// interfaces that abstract HTTP access to the resources.
///
struct _EFI_REST_PROTOCOL {
  EFI_REST_SEND_RECEIVE         SendReceive;
  EFI_REST_GET_TIME             GetServiceTime;
};

extern EFI_GUID gEfiRestProtocolGuid;

#endif
