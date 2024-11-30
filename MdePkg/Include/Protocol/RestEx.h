/** @file
  This file defines the EFI REST EX Protocol interface. It is
  split into the following two main sections.

  - REST EX Service Binding Protocol
  - REST EX Protocol

   Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.8

**/

#ifndef EFI_REST_EX_PROTOCOL_H_
#define EFI_REST_EX_PROTOCOL_H_

#include <Protocol/Http.h>

//
// GUID definitions
//
#define EFI_REST_EX_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0x456bbe01, 0x99d0, 0x45ea, {0xbb, 0x5f, 0x16, 0xd8, 0x4b, 0xed, 0xc5, 0x59 } \
  }

#define EFI_REST_EX_PROTOCOL_GUID \
  { \
    0x55648b91, 0xe7d, 0x40a3, {0xa9, 0xb3, 0xa8, 0x15, 0xd7, 0xea, 0xdf, 0x97 } \
  }

typedef struct _EFI_REST_EX_PROTOCOL EFI_REST_EX_PROTOCOL;

// *******************************************************
// EFI_REST_EX_SERVICE_INFO_VER
// *******************************************************
typedef struct {
  UINT8    Major;
  UINT8    Minor;
} EFI_REST_EX_SERVICE_INFO_VER;

// *******************************************************
// EFI_REST_EX_SERVICE_INFO_HEADER
// *******************************************************
typedef struct {
  UINT32                          Length;
  EFI_REST_EX_SERVICE_INFO_VER    RestServiceInfoVer;
} EFI_REST_EX_SERVICE_INFO_HEADER;

// *******************************************************
// EFI_REST_EX_SERVICE_TYPE
// *******************************************************
typedef enum {
  EfiRestExServiceUnspecific = 1,
  EfiRestExServiceRedfish,
  EfiRestExServiceOdata,
  EfiRestExServiceVendorSpecific = 0xff,
  EfiRestExServiceTypeMax
} EFI_REST_EX_SERVICE_TYPE;

// *******************************************************
// EFI_REST_EX_SERVICE_ACCESS_MODE
// *******************************************************
typedef enum {
  EfiRestExServiceInBandAccess    = 1,
  EfiRestExServiceOutOfBandAccess = 2,
  EfiRestExServiceModeMax
} EFI_REST_EX_SERVICE_ACCESS_MODE;

// *******************************************************
// EFI_REST_EX_CONFIG_TYPE
// *******************************************************
typedef enum {
  EfiRestExConfigHttp,
  EfiRestExConfigUnspecific,
  EfiRestExConfigTypeMax
} EFI_REST_EX_CONFIG_TYPE;

// *******************************************************
// EFI_REST_EX_SERVICE_INFO v1.0
// *******************************************************
typedef struct {
  EFI_REST_EX_SERVICE_INFO_HEADER    EfiRestExServiceInfoHeader;
  EFI_REST_EX_SERVICE_TYPE           RestServiceType;
  EFI_REST_EX_SERVICE_ACCESS_MODE    RestServiceAccessMode;
  EFI_GUID                           VendorRestServiceName;
  UINT32                             VendorSpecificDataLength;
  UINT8                              *VendorSpecifcData;
  EFI_REST_EX_CONFIG_TYPE            RestExConfigType;
  UINT8                              RestExConfigDataLength;
} EFI_REST_EX_SERVICE_INFO_V_1_0;

// *******************************************************
// EFI_REST_EX_SERVICE_INFO
// *******************************************************
typedef union {
  EFI_REST_EX_SERVICE_INFO_HEADER    EfiRestExServiceInfoHeader;
  EFI_REST_EX_SERVICE_INFO_V_1_0     EfiRestExServiceInfoV10;
} EFI_REST_EX_SERVICE_INFO;

// *******************************************************
// EFI_REST_EX_HTTP_CONFIG_DATA
// *******************************************************
typedef struct {
  EFI_HTTP_CONFIG_DATA    HttpConfigData;
  UINT32                  SendReceiveTimeout;
} EFI_REST_EX_HTTP_CONFIG_DATA;

// *******************************************************
// EFI_REST_EX_CONFIG_DATA
// *******************************************************
typedef UINT8 *EFI_REST_EX_CONFIG_DATA;

// *******************************************************
// EFI_REST_EX_TOKEN
// *******************************************************
typedef struct {
  EFI_EVENT           Event;
  EFI_STATUS          Status;
  EFI_HTTP_MESSAGE    *ResponseMessage;
} EFI_REST_EX_TOKEN;

/**
  Provides a simple HTTP-like interface to send and receive resources from a REST service.

  The SendReceive() function sends an HTTP request to this REST service, and returns a
  response when the data is retrieved from the service. RequestMessage contains the HTTP
  request to the REST resource identified by RequestMessage.Request.Url. The
  ResponseMessage is the returned HTTP response for that request, including any HTTP
  status. It's caller's responsibility to free this ResponseMessage using FreePool().
  RestConfigFreeHttpMessage() in RedfishLib is an example to release ResponseMessage structure.

  @param[in]  This                Pointer to EFI_REST_EX_PROTOCOL instance for a particular
                                  REST service.
  @param[in]  RequestMessage      Pointer to the HTTP request data for this resource
  @param[out] ResponseMessage     Pointer to the HTTP response data obtained for this requested.

  @retval EFI_SUCCESS             operation succeeded.
  @retval EFI_INVALID_PARAMETER   This, RequestMessage, or ResponseMessage are NULL.
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REST_SEND_RECEIVE)(
  IN      EFI_REST_EX_PROTOCOL   *This,
  IN      EFI_HTTP_MESSAGE       *RequestMessage,
  OUT     EFI_HTTP_MESSAGE       *ResponseMessage
  );

/**
  Obtain the current time from this REST service instance.

  The GetServiceTime() function is an optional interface to obtain the current time from
  this REST service instance. If this REST service does not support to retrieve the time,
  this function returns EFI_UNSUPPORTED. This function must returns EFI_UNSUPPORTED if
  EFI_REST_EX_SERVICE_TYPE returned in EFI_REST_EX_SERVICE_INFO from GetService() is
  EFI_REST_EX_SERVICE_UNSPECIFIC.

  @param[in]  This                Pointer to EFI_REST_EX_PROTOCOL instance for a particular
                                  REST service.
  @param[out] Time                A pointer to storage to receive a snapshot of the current time of
                                  the REST service.

  @retval EFI_SUCCESS             operation succeeded.
  @retval EFI_INVALID_PARAMETER   This or Time are NULL.
  @retval EFI_UNSUPPORTED         The RESTful service does not support returning the time.
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred.
  @retval EFI_NOT_READY           The configuration of this instance is not set yet. Configure() must
                                  be executed and returns successfully prior to invoke this function.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REST_GET_TIME)(
  IN      EFI_REST_EX_PROTOCOL   *This,
  OUT     EFI_TIME               *Time
  );

/**
  This function returns the information of REST service provided by this EFI REST EX driver instance.

  The information such as the type of REST service and the access mode of REST EX driver instance
  (In-band or Out-of-band) are described in EFI_REST_EX_SERVICE_INFO structure. For the vendor-specific
  REST service, vendor-specific REST service information is returned in VendorSpecifcData.
  REST EX driver designer is well know what REST service this REST EX driver instance intends to
  communicate with. The designer also well know this driver instance is used to talk to BMC through
  specific platform mechanism or talk to REST server through UEFI HTTP protocol. REST EX driver is
  responsible to fill up the correct information in EFI_REST_EX_SERVICE_INFO. EFI_REST_EX_SERVICE_INFO
  is referred by EFI REST clients to pickup the proper EFI REST EX driver instance to get and set resource.
  GetService() is a basic and mandatory function which must be able to use even Configure() is not invoked
  in previously.

  @param[in]  This                Pointer to EFI_REST_EX_PROTOCOL instance for a particular
                                  REST service.
  @param[out] RestExServiceInfo   Pointer to receive a pointer to EFI_REST_EX_SERVICE_INFO structure. The
                                  format of EFI_REST_EX_SERVICE_INFO is version controlled for the future
                                  extension. The version of EFI_REST_EX_SERVICE_INFO structure is returned
                                  in the header within this structure. EFI REST client refers to the correct
                                  format of structure according to the version number. The pointer to
                                  EFI_REST_EX_SERVICE_INFO is a memory block allocated by EFI REST EX driver
                                  instance. That is caller's responsibility to free this memory when this
                                  structure is no longer needed. Refer to Related Definitions below for the
                                  definitions of EFI_REST_EX_SERVICE_INFO structure.

  @retval EFI_SUCCESS             EFI_REST_EX_SERVICE_INFO is returned in RestExServiceInfo. This function
                                  is not supported in this REST EX Protocol driver instance.
  @retval EFI_UNSUPPORTED         This function is not supported in this REST EX Protocol driver instance.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REST_EX_GET_SERVICE)(
  IN   EFI_REST_EX_PROTOCOL      *This,
  OUT  EFI_REST_EX_SERVICE_INFO  **RestExServiceInfo
  );

/**
  This function returns operational configuration of current EFI REST EX child instance.

  This function returns the current configuration of EFI REST EX child instance. The format of
  operational configuration depends on the implementation of EFI REST EX driver instance. For
  example, HTTP-aware EFI REST EX driver instance uses EFI HTTP protocol as the undying protocol
  to communicate with REST service. In this case, the type of configuration is
  EFI_REST_EX_CONFIG_TYPE_HTTP returned from GetService(). EFI_HTTP_CONFIG_DATA is used as EFI REST
  EX configuration format and returned to EFI REST client. User has to type cast RestExConfigData
  to EFI_HTTP_CONFIG_DATA. For those non HTTP-aware REST EX driver instances, the type of configuration
  is EFI_REST_EX_CONFIG_TYPE_UNSPECIFIC returned from GetService(). In this case, the format of
  returning data could be non industrial. Instead, the format of configuration data is system/platform
  specific definition such as BMC mechanism used in EFI REST EX driver instance. EFI REST client and
  EFI REST EX driver instance have to refer to the specific system /platform spec which is out of UEFI scope.

  @param[in]  This                This is the EFI_REST_EX_PROTOCOL instance.
  @param[out] RestExConfigData    Pointer to receive a pointer to EFI_REST_EX_CONFIG_DATA.
                                  The memory allocated for configuration data should be freed
                                  by caller. See Related Definitions for the details.

  @retval EFI_SUCCESS             EFI_REST_EX_CONFIG_DATA is returned in successfully.
  @retval EFI_UNSUPPORTED         This function is not supported in this REST EX Protocol driver instance.
  @retval EFI_NOT_READY           The configuration of this instance is not set yet. Configure() must be
                                  executed and returns successfully prior to invoke this function.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REST_EX_GET_MODE_DATA)(
  IN  EFI_REST_EX_PROTOCOL  *This,
  OUT EFI_REST_EX_CONFIG_DATA *RestExConfigData
  );

/**
  This function is used to configure EFI REST EX child instance.

  This function is used to configure the setting of underlying protocol of REST EX child
  instance. The type of configuration is according to the implementation of EFI REST EX
  driver instance. For example, HTTP-aware EFI REST EX driver instance uses EFI HTTP protocol
  as the undying protocol to communicate with REST service. The type of configuration is
  EFI_REST_EX_CONFIG_TYPE_HTTP and RestExConfigData is the same format with EFI_HTTP_CONFIG_DATA.
  Akin to HTTP configuration, REST EX child instance can be configure to use different HTTP
  local access point for the data transmission. Multiple REST clients may use different
  configuration of HTTP to distinguish themselves, such as to use the different TCP port.
  For those non HTTP-aware REST EX driver instance, the type of configuration is
  EFI_REST_EX_CONFIG_TYPE_UNSPECIFIC. RestExConfigData refers to the non industrial standard.
  Instead, the format of configuration data is system/platform specific definition such as BMC.
  In this case, EFI REST client and EFI REST EX driver instance have to refer to the specific
  system/platform spec which is out of the UEFI scope. Besides GetService()function, no other
  EFI REST EX functions can be executed by this instance until Configure()is executed and returns
  successfully. All other functions must returns EFI_NOT_READY if this instance is not configured
  yet. Set RestExConfigData to NULL means to put EFI REST EX child instance into the unconfigured
  state.

  @param[in]  This                This is the EFI_REST_EX_PROTOCOL instance.
  @param[in]  RestExConfigData    Pointer to EFI_REST_EX_CONFIG_DATA. See Related Definitions in
                                  GetModeData() protocol interface.

  @retval EFI_SUCCESS             EFI_REST_EX_CONFIG_DATA is set in successfully.
  @retval EFI_DEVICE_ERROR        Configuration for this REST EX child instance is failed with the given
                                  EFI_REST_EX_CONFIG_DATA.
  @retval EFI_UNSUPPORTED         This function is not supported in this REST EX Protocol driver instance.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REST_EX_CONFIGURE)(
  IN  EFI_REST_EX_PROTOCOL  *This,
  IN  EFI_REST_EX_CONFIG_DATA RestExConfigData
  );

/**
  This function sends REST request to REST service and signal caller's event asynchronously when
  the final response is received by REST EX Protocol driver instance.

  The essential design of this function is to handle asynchronous send/receive implicitly according
  to REST service asynchronous request mechanism. Caller will get the notification once the response
  is returned from REST service.

  @param[in]  This                  This is the EFI_REST_EX_PROTOCOL instance.
  @param[in]  RequestMessage        This is the HTTP request message sent to REST service. Set RequestMessage
                                    to NULL to cancel the previous asynchronous request associated with the
                                    corresponding RestExToken. See descriptions for the details.
  @param[in]  RestExToken           REST EX token which REST EX Protocol instance uses to notify REST client
                                    the status of response of asynchronous REST request. See related definition
                                    of EFI_REST_EX_TOKEN.
  @param[in]  TimeOutInMilliSeconds The pointer to the timeout in milliseconds which REST EX Protocol driver
                                    instance refers as the duration to drop asynchronous REST request. NULL
                                    pointer means no timeout for this REST request. REST EX Protocol driver
                                    signals caller's event with EFI_STATUS set to EFI_TIMEOUT in RestExToken
                                    if REST EX Protocol can't get the response from REST service within
                                    TimeOutInMilliSeconds.

  @retval EFI_SUCCESS               Asynchronous REST request is established.
  @retval EFI_UNSUPPORTED           This REST EX Protocol driver instance doesn't support asynchronous request.
  @retval EFI_TIMEOUT               Asynchronous REST request is not established and timeout is expired.
  @retval EFI_ABORT                 Previous asynchronous REST request has been canceled.
  @retval EFI_DEVICE_ERROR          Otherwise, returns EFI_DEVICE_ERROR for other errors according to HTTP Status Code.
  @retval EFI_NOT_READY             The configuration of this instance is not set yet. Configure() must be executed
                                    and returns successfully prior to invoke this function.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REST_EX_ASYNC_SEND_RECEIVE)(
  IN      EFI_REST_EX_PROTOCOL   *This,
  IN      EFI_HTTP_MESSAGE       *RequestMessage OPTIONAL,
  IN      EFI_REST_EX_TOKEN      *RestExToken,
  IN      UINTN                  *TimeOutInMilliSeconds OPTIONAL
  );

/**
  This function sends REST request to a REST Event service and signals caller's event
  token asynchronously when the URI resource change event is received by REST EX
  Protocol driver instance.

  The essential design of this function is to monitor event implicitly according to
  REST service event service mechanism. Caller will get the notification if certain
  resource is changed.

  @param[in]  This                  This is the EFI_REST_EX_PROTOCOL instance.
  @param[in]  RequestMessage        This is the HTTP request message sent to REST service. Set RequestMessage
                                    to NULL to cancel the previous event service associated with the corresponding
                                    RestExToken. See descriptions for the details.
  @param[in]  RestExToken           REST EX token which REST EX Protocol driver instance uses to notify REST client
                                    the URI resource which monitored by REST client has been changed. See the related
                                    definition of EFI_REST_EX_TOKEN in EFI_REST_EX_PROTOCOL.AsyncSendReceive().

  @retval EFI_SUCCESS               Asynchronous REST request is established.
  @retval EFI_UNSUPPORTED           This REST EX Protocol driver instance doesn't support asynchronous request.
  @retval EFI_ABORT                 Previous asynchronous REST request has been canceled or event subscription has been
                                    delete from service.
  @retval EFI_DEVICE_ERROR          Otherwise, returns EFI_DEVICE_ERROR for other errors according to HTTP Status Code.
  @retval EFI_NOT_READY             The configuration of this instance is not set yet. Configure() must be executed
                                    and returns successfully prior to invoke this function.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REST_EX_EVENT_SERVICE)(
  IN      EFI_REST_EX_PROTOCOL   *This,
  IN      EFI_HTTP_MESSAGE       *RequestMessage OPTIONAL,
  IN      EFI_REST_EX_TOKEN      *RestExToken
  );

///
/// EFI REST(EX) protocols are designed to support REST communication between EFI REST client
/// applications/drivers and REST services. EFI REST client tool uses EFI REST(EX) protocols
/// to send/receive resources to/from REST service to manage systems, configure systems or
/// manipulate resources on REST service. Due to HTTP protocol is commonly used to communicate
/// with REST service in practice, EFI REST(EX) protocols adopt HTTP as the message format to
/// send and receive REST service resource. EFI REST(EX) driver instance abstracts EFI REST
/// client functionality and provides underlying interface to communicate with REST service.
/// EFI REST(EX) driver instance knows how to communicate with REST service through certain
/// interface after the corresponding configuration is initialized.
///
struct _EFI_REST_EX_PROTOCOL {
  EFI_REST_SEND_RECEIVE             SendReceive;
  EFI_REST_GET_TIME                 GetServiceTime;
  EFI_REST_EX_GET_SERVICE           GetService;
  EFI_REST_EX_GET_MODE_DATA         GetModeData;
  EFI_REST_EX_CONFIGURE             Configure;
  EFI_REST_EX_ASYNC_SEND_RECEIVE    AyncSendReceive;
  EFI_REST_EX_EVENT_SERVICE         EventService;
};

extern EFI_GUID  gEfiRestExServiceBindingProtocolGuid;
extern EFI_GUID  gEfiRestExProtocolGuid;

#endif
