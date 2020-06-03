/** @file
  This file defines the EFI HTTP Protocol interface. It is split into
  the following two main sections:
  HTTP Service Binding Protocol (HTTPSB)
  HTTP Protocol (HTTP)

  Copyright (c) 2016 - 2018, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2015-2017 Hewlett Packard Enterprise Development LP<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.5

**/

#ifndef __EFI_HTTP_PROTOCOL_H__
#define __EFI_HTTP_PROTOCOL_H__

#define EFI_HTTP_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0xbdc8e6af, 0xd9bc, 0x4379, {0xa7, 0x2a, 0xe0, 0xc4, 0xe7, 0x5d, 0xae, 0x1c } \
  }

#define EFI_HTTP_PROTOCOL_GUID \
  { \
    0x7a59b29b, 0x910b, 0x4171, {0x82, 0x42, 0xa8, 0x5a, 0x0d, 0xf2, 0x5b, 0x5b } \
  }

typedef struct _EFI_HTTP_PROTOCOL EFI_HTTP_PROTOCOL;

///
/// EFI_HTTP_VERSION
///
typedef enum {
  HttpVersion10,
  HttpVersion11,
  HttpVersionUnsupported
} EFI_HTTP_VERSION;

///
/// EFI_HTTP_METHOD
///
typedef enum {
  HttpMethodGet,
  HttpMethodPost,
  HttpMethodPatch,
  HttpMethodOptions,
  HttpMethodConnect,
  HttpMethodHead,
  HttpMethodPut,
  HttpMethodDelete,
  HttpMethodTrace,
  HttpMethodMax
} EFI_HTTP_METHOD;

///
/// EFI_HTTP_STATUS_CODE
///
typedef enum {
  HTTP_STATUS_UNSUPPORTED_STATUS = 0,
  HTTP_STATUS_100_CONTINUE,
  HTTP_STATUS_101_SWITCHING_PROTOCOLS,
  HTTP_STATUS_200_OK,
  HTTP_STATUS_201_CREATED,
  HTTP_STATUS_202_ACCEPTED,
  HTTP_STATUS_203_NON_AUTHORITATIVE_INFORMATION,
  HTTP_STATUS_204_NO_CONTENT,
  HTTP_STATUS_205_RESET_CONTENT,
  HTTP_STATUS_206_PARTIAL_CONTENT,
  HTTP_STATUS_300_MULTIPLE_CHOICES,
  HTTP_STATUS_301_MOVED_PERMANENTLY,
  HTTP_STATUS_302_FOUND,
  HTTP_STATUS_303_SEE_OTHER,
  HTTP_STATUS_304_NOT_MODIFIED,
  HTTP_STATUS_305_USE_PROXY,
  HTTP_STATUS_307_TEMPORARY_REDIRECT,
  HTTP_STATUS_400_BAD_REQUEST,
  HTTP_STATUS_401_UNAUTHORIZED,
  HTTP_STATUS_402_PAYMENT_REQUIRED,
  HTTP_STATUS_403_FORBIDDEN,
  HTTP_STATUS_404_NOT_FOUND,
  HTTP_STATUS_405_METHOD_NOT_ALLOWED,
  HTTP_STATUS_406_NOT_ACCEPTABLE,
  HTTP_STATUS_407_PROXY_AUTHENTICATION_REQUIRED,
  HTTP_STATUS_408_REQUEST_TIME_OUT,
  HTTP_STATUS_409_CONFLICT,
  HTTP_STATUS_410_GONE,
  HTTP_STATUS_411_LENGTH_REQUIRED,
  HTTP_STATUS_412_PRECONDITION_FAILED,
  HTTP_STATUS_413_REQUEST_ENTITY_TOO_LARGE,
  HTTP_STATUS_414_REQUEST_URI_TOO_LARGE,
  HTTP_STATUS_415_UNSUPPORTED_MEDIA_TYPE,
  HTTP_STATUS_416_REQUESTED_RANGE_NOT_SATISFIED,
  HTTP_STATUS_417_EXPECTATION_FAILED,
  HTTP_STATUS_500_INTERNAL_SERVER_ERROR,
  HTTP_STATUS_501_NOT_IMPLEMENTED,
  HTTP_STATUS_502_BAD_GATEWAY,
  HTTP_STATUS_503_SERVICE_UNAVAILABLE,
  HTTP_STATUS_504_GATEWAY_TIME_OUT,
  HTTP_STATUS_505_HTTP_VERSION_NOT_SUPPORTED,
  HTTP_STATUS_308_PERMANENT_REDIRECT
} EFI_HTTP_STATUS_CODE;

///
/// EFI_HTTPv4_ACCESS_POINT
///
typedef struct {
  ///
  /// Set to TRUE to instruct the EFI HTTP instance to use the default address
  /// information in every TCP connection made by this instance. In addition, when set
  /// to TRUE, LocalAddress and LocalSubnet are ignored.
  ///
  BOOLEAN                       UseDefaultAddress;
  ///
  /// If UseDefaultAddress is set to FALSE, this defines the local IP address to be
  /// used in every TCP connection opened by this instance.
  ///
  EFI_IPv4_ADDRESS              LocalAddress;
  ///
  /// If UseDefaultAddress is set to FALSE, this defines the local subnet to be used
  /// in every TCP connection opened by this instance.
  ///
  EFI_IPv4_ADDRESS              LocalSubnet;
  ///
  /// This defines the local port to be used in
  /// every TCP connection opened by this instance.
  ///
  UINT16                        LocalPort;
} EFI_HTTPv4_ACCESS_POINT;

///
/// EFI_HTTPv6_ACCESS_POINT
///
typedef struct {
  ///
  /// Local IP address to be used in every TCP connection opened by this instance.
  ///
  EFI_IPv6_ADDRESS              LocalAddress;
  ///
  /// Local port to be used in every TCP connection opened by this instance.
  ///
  UINT16                        LocalPort;
} EFI_HTTPv6_ACCESS_POINT;

///
/// EFI_HTTP_CONFIG_DATA_ACCESS_POINT
///


typedef struct {
  ///
  /// HTTP version that this instance will support.
  ///
  EFI_HTTP_VERSION                   HttpVersion;
  ///
  /// Time out (in milliseconds) when blocking for requests.
  ///
  UINT32                             TimeOutMillisec;
  ///
  /// Defines behavior of EFI DNS and TCP protocols consumed by this instance. If
  /// FALSE, this instance will use EFI_DNS4_PROTOCOL and EFI_TCP4_PROTOCOL. If TRUE,
  /// this instance will use EFI_DNS6_PROTOCOL and EFI_TCP6_PROTOCOL.
  ///
  BOOLEAN                            LocalAddressIsIPv6;

  union {
    ///
    /// When LocalAddressIsIPv6 is FALSE, this points to the local address, subnet, and
    /// port used by the underlying TCP protocol.
    ///
    EFI_HTTPv4_ACCESS_POINT          *IPv4Node;
    ///
    /// When LocalAddressIsIPv6 is TRUE, this points to the local IPv6 address and port
    /// used by the underlying TCP protocol.
    ///
    EFI_HTTPv6_ACCESS_POINT          *IPv6Node;
  } AccessPoint;
} EFI_HTTP_CONFIG_DATA;

///
/// EFI_HTTP_REQUEST_DATA
///
typedef struct {
  ///
  /// The HTTP method (e.g. GET, POST) for this HTTP Request.
  ///
  EFI_HTTP_METHOD               Method;
  ///
  /// The URI of a remote host. From the information in this field, the HTTP instance
  /// will be able to determine whether to use HTTP or HTTPS and will also be able to
  /// determine the port number to use. If no port number is specified, port 80 (HTTP)
  /// is assumed. See RFC 3986 for more details on URI syntax.
  ///
  CHAR16                        *Url;
} EFI_HTTP_REQUEST_DATA;

///
/// EFI_HTTP_RESPONSE_DATA
///
typedef struct {
  ///
  /// Response status code returned by the remote host.
  ///
  EFI_HTTP_STATUS_CODE          StatusCode;
} EFI_HTTP_RESPONSE_DATA;

///
/// EFI_HTTP_HEADER
///
typedef struct {
  ///
  /// Null terminated string which describes a field name. See RFC 2616 Section 14 for
  /// detailed information about field names.
  ///
  CHAR8                         *FieldName;
  ///
  /// Null terminated string which describes the corresponding field value. See RFC 2616
  /// Section 14 for detailed information about field values.
  ///
  CHAR8                         *FieldValue;
} EFI_HTTP_HEADER;

///
/// EFI_HTTP_MESSAGE
///
typedef struct {
  ///
  /// HTTP message data.
  ///
  union {
    ///
    /// When the token is used to send a HTTP request, Request is a pointer to storage that
    /// contains such data as URL and HTTP method.
    ///
    EFI_HTTP_REQUEST_DATA       *Request;
    ///
    /// When used to await a response, Response points to storage containing HTTP response
    /// status code.
    ///
    EFI_HTTP_RESPONSE_DATA      *Response;
  } Data;
  ///
  /// Number of HTTP header structures in Headers list. On request, this count is
  /// provided by the caller. On response, this count is provided by the HTTP driver.
  ///
  UINTN                         HeaderCount;
  ///
  /// Array containing list of HTTP headers. On request, this array is populated by the
  /// caller. On response, this array is allocated and populated by the HTTP driver. It
  /// is the responsibility of the caller to free this memory on both request and
  /// response.
  ///
  EFI_HTTP_HEADER               *Headers;
  ///
  /// Length in bytes of the HTTP body. This can be zero depending on the HttpMethod type.
  ///
  UINTN                         BodyLength;
  ///
  /// Body associated with the HTTP request or response. This can be NULL depending on
  /// the HttpMethod type.
  ///
  VOID                          *Body;
} EFI_HTTP_MESSAGE;


///
/// EFI_HTTP_TOKEN
///
typedef struct {
  ///
  /// This Event will be signaled after the Status field is updated by the EFI HTTP
  /// Protocol driver. The type of Event must be EFI_NOTIFY_SIGNAL. The Task Priority
  /// Level (TPL) of Event must be lower than or equal to TPL_CALLBACK.
  ///
  EFI_EVENT                     Event;
  ///
  /// Status will be set to one of the following value if the HTTP request is
  /// successfully sent or if an unexpected error occurs:
  ///   EFI_SUCCESS:      The HTTP request was successfully sent to the remote host.
  ///   EFI_HTTP_ERROR:   The response message was successfully received but contains a
  ///                     HTTP error. The response status code is returned in token.
  ///   EFI_ABORTED:      The HTTP request was cancelled by the caller and removed from
  ///                     the transmit queue.
  ///   EFI_TIMEOUT:      The HTTP request timed out before reaching the remote host.
  ///   EFI_DEVICE_ERROR: An unexpected system or network error occurred.
  ///
  EFI_STATUS                    Status;
  ///
  /// Pointer to storage containing HTTP message data.
  ///
  EFI_HTTP_MESSAGE              *Message;
} EFI_HTTP_TOKEN;

/**
  Returns the operational parameters for the current HTTP child instance.

  The GetModeData() function is used to read the current mode data (operational
  parameters) for this HTTP protocol instance.

  @param[in]  This                Pointer to EFI_HTTP_PROTOCOL instance.
  @param[out] HttpConfigData      Point to buffer for operational parameters of this
                                  HTTP instance. It is the responsibility of the caller
                                  to allocate the memory for HttpConfigData and
                                  HttpConfigData->AccessPoint.IPv6Node/IPv4Node. In fact,
                                  it is recommended to allocate sufficient memory to record
                                  IPv6Node since it is big enough for all possibilities.

  @retval EFI_SUCCESS             Operation succeeded.
  @retval EFI_INVALID_PARAMETER   This is NULL.
                                  HttpConfigData is NULL.
                                  HttpConfigData->AccessPoint.IPv4Node or
                                  HttpConfigData->AccessPoint.IPv6Node is NULL.
  @retval EFI_NOT_STARTED         This EFI HTTP Protocol instance has not been started.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_HTTP_GET_MODE_DATA)(
  IN  EFI_HTTP_PROTOCOL         *This,
  OUT EFI_HTTP_CONFIG_DATA      *HttpConfigData
  );

/**
  Initialize or brutally reset the operational parameters for this EFI HTTP instance.

  The Configure() function does the following:
  When HttpConfigData is not NULL Initialize this EFI HTTP instance by configuring
  timeout, local address, port, etc.
  When HttpConfigData is NULL, reset this EFI HTTP instance by closing all active
  connections with remote hosts, canceling all asynchronous tokens, and flush request
  and response buffers without informing the appropriate hosts.

  No other EFI HTTP function can be executed by this instance until the Configure()
  function is executed and returns successfully.

  @param[in]  This                Pointer to EFI_HTTP_PROTOCOL instance.
  @param[in]  HttpConfigData      Pointer to the configure data to configure the instance.

  @retval EFI_SUCCESS             Operation succeeded.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  HttpConfigData->LocalAddressIsIPv6 is FALSE and
                                  HttpConfigData->AccessPoint.IPv4Node is NULL.
                                  HttpConfigData->LocalAddressIsIPv6 is TRUE and
                                  HttpConfigData->AccessPoint.IPv6Node is NULL.
  @retval EFI_ALREADY_STARTED     Reinitialize this HTTP instance without calling
                                  Configure() with NULL to reset it.
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred.
  @retval EFI_OUT_OF_RESOURCES    Could not allocate enough system resources when
                                  executing Configure().
  @retval EFI_UNSUPPORTED         One or more options in ConfigData are not supported
                                  in the implementation.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_HTTP_CONFIGURE)(
  IN  EFI_HTTP_PROTOCOL         *This,
  IN  EFI_HTTP_CONFIG_DATA      *HttpConfigData OPTIONAL
  );

/**
  The Request() function queues an HTTP request to this HTTP instance,
  similar to Transmit() function in the EFI TCP driver. When the HTTP request is sent
  successfully, or if there is an error, Status in token will be updated and Event will
  be signaled.

  @param[in]  This                Pointer to EFI_HTTP_PROTOCOL instance.
  @param[in]  Token               Pointer to storage containing HTTP request token.

  @retval EFI_SUCCESS             Outgoing data was processed.
  @retval EFI_NOT_STARTED         This EFI HTTP Protocol instance has not been started.
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred.
  @retval EFI_TIMEOUT             Data was dropped out of the transmit or receive queue.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  Token is NULL.
                                  Token->Message is NULL.
                                  Token->Message->Body is not NULL,
                                  Token->Message->BodyLength is non-zero, and
                                  Token->Message->Data is NULL, but a previous call to
                                  Request()has not been completed successfully.
  @retval EFI_OUT_OF_RESOURCES    Could not allocate enough system resources.
  @retval EFI_UNSUPPORTED         The HTTP method is not supported in current implementation.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_HTTP_REQUEST) (
  IN  EFI_HTTP_PROTOCOL         *This,
  IN  EFI_HTTP_TOKEN            *Token
  );

/**
  Abort an asynchronous HTTP request or response token.

  The Cancel() function aborts a pending HTTP request or response transaction. If
  Token is not NULL and the token is in transmit or receive queues when it is being
  cancelled, its Token->Status will be set to EFI_ABORTED and then Token->Event will
  be signaled. If the token is not in one of the queues, which usually means that the
  asynchronous operation has completed, EFI_NOT_FOUND is returned. If Token is NULL,
  all asynchronous tokens issued by Request() or Response() will be aborted.

  @param[in]  This                Pointer to EFI_HTTP_PROTOCOL instance.
  @param[in]  Token               Point to storage containing HTTP request or response
                                  token.

  @retval EFI_SUCCESS             Request and Response queues are successfully flushed.
  @retval EFI_INVALID_PARAMETER   This is NULL.
  @retval EFI_NOT_STARTED         This instance hasn't been configured.
  @retval EFI_NOT_FOUND           The asynchronous request or response token is not
                                  found.
  @retval EFI_UNSUPPORTED         The implementation does not support this function.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_HTTP_CANCEL)(
  IN  EFI_HTTP_PROTOCOL         *This,
  IN  EFI_HTTP_TOKEN            *Token
  );

/**
  The Response() function queues an HTTP response to this HTTP instance, similar to
  Receive() function in the EFI TCP driver. When the HTTP Response is received successfully,
  or if there is an error, Status in token will be updated and Event will be signaled.

  The HTTP driver will queue a receive token to the underlying TCP instance. When data
  is received in the underlying TCP instance, the data will be parsed and Token will
  be populated with the response data. If the data received from the remote host
  contains an incomplete or invalid HTTP header, the HTTP driver will continue waiting
  (asynchronously) for more data to be sent from the remote host before signaling
  Event in Token.

  It is the responsibility of the caller to allocate a buffer for Body and specify the
  size in BodyLength. If the remote host provides a response that contains a content
  body, up to BodyLength bytes will be copied from the receive buffer into Body and
  BodyLength will be updated with the amount of bytes received and copied to Body. This
  allows the client to download a large file in chunks instead of into one contiguous
  block of memory. Similar to HTTP request, if Body is not NULL and BodyLength is
  non-zero and all other fields are NULL or 0, the HTTP driver will queue a receive
  token to underlying TCP instance. If data arrives in the receive buffer, up to
  BodyLength bytes of data will be copied to Body. The HTTP driver will then update
  BodyLength with the amount of bytes received and copied to Body.

  If the HTTP driver does not have an open underlying TCP connection with the host
  specified in the response URL, Request() will return EFI_ACCESS_DENIED. This is
  consistent with RFC 2616 recommendation that HTTP clients should attempt to maintain
  an open TCP connection between client and host.

  @param[in]  This                Pointer to EFI_HTTP_PROTOCOL instance.
  @param[in]  Token               Pointer to storage containing HTTP response token.

  @retval EFI_SUCCESS             Allocation succeeded.
  @retval EFI_NOT_STARTED         This EFI HTTP Protocol instance has not been
                                  initialized.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is TRUE:
                                  This is NULL.
                                  Token is NULL.
                                  Token->Message->Headers is NULL.
                                  Token->Message is NULL.
                                  Token->Message->Body is not NULL,
                                  Token->Message->BodyLength is non-zero, and
                                  Token->Message->Data is NULL, but a previous call to
                                  Response() has not been completed successfully.
  @retval EFI_OUT_OF_RESOURCES    Could not allocate enough system resources.
  @retval EFI_ACCESS_DENIED       An open TCP connection is not present with the host
                                  specified by response URL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_HTTP_RESPONSE) (
  IN  EFI_HTTP_PROTOCOL         *This,
  IN  EFI_HTTP_TOKEN            *Token
  );

/**
  The Poll() function can be used by network drivers and applications to increase the
  rate that data packets are moved between the communication devices and the transmit
  and receive queues.

  In some systems, the periodic timer event in the managed network driver may not poll
  the underlying communications device fast enough to transmit and/or receive all data
  packets without missing incoming packets or dropping outgoing packets. Drivers and
  applications that are experiencing packet loss should try calling the Poll() function
  more often.

  @param[in]  This                Pointer to EFI_HTTP_PROTOCOL instance.

  @retval EFI_SUCCESS             Incoming or outgoing data was processed..
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred
  @retval EFI_INVALID_PARAMETER   This is NULL.
  @retval EFI_NOT_READY           No incoming or outgoing data is processed.
  @retval EFI_NOT_STARTED         This EFI HTTP Protocol instance has not been started.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_HTTP_POLL) (
  IN  EFI_HTTP_PROTOCOL         *This
  );

///
/// The EFI HTTP protocol is designed to be used by EFI drivers and applications to
/// create and transmit HTTP Requests, as well as handle HTTP responses that are
/// returned by a remote host. This EFI protocol uses and relies on an underlying EFI
/// TCP protocol.
///
struct _EFI_HTTP_PROTOCOL {
  EFI_HTTP_GET_MODE_DATA        GetModeData;
  EFI_HTTP_CONFIGURE            Configure;
  EFI_HTTP_REQUEST              Request;
  EFI_HTTP_CANCEL               Cancel;
  EFI_HTTP_RESPONSE             Response;
  EFI_HTTP_POLL                 Poll;
};

extern EFI_GUID gEfiHttpServiceBindingProtocolGuid;
extern EFI_GUID gEfiHttpProtocolGuid;

#endif
