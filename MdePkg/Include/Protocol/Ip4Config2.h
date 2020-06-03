/** @file
  This file provides a definition of the EFI IPv4 Configuration II
  Protocol.

Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

@par Revision Reference:
This Protocol is introduced in UEFI Specification 2.5

**/
#ifndef __EFI_IP4CONFIG2_PROTOCOL_H__
#define __EFI_IP4CONFIG2_PROTOCOL_H__

#include <Protocol/Ip4.h>

#define EFI_IP4_CONFIG2_PROTOCOL_GUID \
  { \
    0x5b446ed1, 0xe30b, 0x4faa, {0x87, 0x1a, 0x36, 0x54, 0xec, 0xa3, 0x60, 0x80 } \
  }

typedef struct _EFI_IP4_CONFIG2_PROTOCOL EFI_IP4_CONFIG2_PROTOCOL;


///
/// EFI_IP4_CONFIG2_DATA_TYPE
///
typedef enum {
  ///
  /// The interface information of the communication device this EFI
  /// IPv4 Configuration II Protocol instance manages. This type of
  /// data is read only. The corresponding Data is of type
  /// EFI_IP4_CONFIG2_INTERFACE_INFO.
  ///
  Ip4Config2DataTypeInterfaceInfo,
  ///
  /// The general configuration policy for the EFI IPv4 network stack
  /// running on the communication device this EFI IPv4
  /// Configuration II Protocol instance manages. The policy will
  /// affect other configuration settings. The corresponding Data is of
  /// type EFI_IP4_CONFIG2_POLICY.
  ///
  Ip4Config2DataTypePolicy,
  ///
  /// The station addresses set manually for the EFI IPv4 network
  /// stack. It is only configurable when the policy is
  /// Ip4Config2PolicyStatic. The corresponding Data is of
  /// type EFI_IP4_CONFIG2_MANUAL_ADDRESS. When DataSize
  /// is 0 and Data is NULL, the existing configuration is cleared
  /// from the EFI IPv4 Configuration II Protocol instance.
  ///
  Ip4Config2DataTypeManualAddress,
  ///
  /// The gateway addresses set manually for the EFI IPv4 network
  /// stack running on the communication device this EFI IPv4
  /// Configuration II Protocol manages. It is not configurable when
  /// the policy is Ip4Config2PolicyDhcp. The gateway
  /// addresses must be unicast IPv4 addresses. The corresponding
  /// Data is a pointer to an array of EFI_IPv4_ADDRESS instances.
  /// When DataSize is 0 and Data is NULL, the existing configuration
  /// is cleared from the EFI IPv4 Configuration II Protocol instance.
  ///
  Ip4Config2DataTypeGateway,
  ///
  /// The DNS server list for the EFI IPv4 network stack running on
  /// the communication device this EFI IPv4 Configuration II
  /// Protocol manages. It is not configurable when the policy is
  /// Ip4Config2PolicyDhcp. The DNS server addresses must be
  /// unicast IPv4 addresses. The corresponding Data is a pointer to
  /// an array of EFI_IPv4_ADDRESS instances. When DataSize
  /// is 0 and Data is NULL, the existing configuration is cleared
  /// from the EFI IPv4 Configuration II Protocol instance.
  ///
  Ip4Config2DataTypeDnsServer,
  Ip4Config2DataTypeMaximum
} EFI_IP4_CONFIG2_DATA_TYPE;

///
/// EFI_IP4_CONFIG2_INTERFACE_INFO related definitions
///
#define EFI_IP4_CONFIG2_INTERFACE_INFO_NAME_SIZE 32

///
/// EFI_IP4_CONFIG2_INTERFACE_INFO
///
typedef struct {
  ///
  /// The name of the interface. It is a NULL-terminated Unicode string.
  ///
  CHAR16                Name[EFI_IP4_CONFIG2_INTERFACE_INFO_NAME_SIZE];
  ///
  /// The interface type of the network interface. See RFC 1700,
  /// section "Number Hardware Type".
  ///
  UINT8                 IfType;
  ///
  /// The size, in bytes, of the network interface's hardware address.
  ///
  UINT32                HwAddressSize;
  ///
  /// The hardware address for the network interface.
  ///
  EFI_MAC_ADDRESS       HwAddress;
  ///
  /// The station IPv4 address of this EFI IPv4 network stack.
  ///
  EFI_IPv4_ADDRESS      StationAddress;
  ///
  /// The subnet address mask that is associated with the station address.
  ///
  EFI_IPv4_ADDRESS      SubnetMask;
  ///
  /// Size of the following RouteTable, in bytes. May be zero.
  ///
  UINT32                RouteTableSize;
  ///
  /// The route table of the IPv4 network stack runs on this interface.
  /// Set to NULL if RouteTableSize is zero. Type EFI_IP4_ROUTE_TABLE is defined in
  /// EFI_IP4_PROTOCOL.GetModeData().
  ///
  EFI_IP4_ROUTE_TABLE   *RouteTable     OPTIONAL;
} EFI_IP4_CONFIG2_INTERFACE_INFO;

///
/// EFI_IP4_CONFIG2_POLICY
///
typedef enum {
  ///
  /// Under this policy, the Ip4Config2DataTypeManualAddress,
  /// Ip4Config2DataTypeGateway and Ip4Config2DataTypeDnsServer configuration
  /// data are required to be set manually. The EFI IPv4 Protocol will get all
  /// required configuration such as IPv4 address, subnet mask and
  /// gateway settings from the EFI IPv4 Configuration II protocol.
  ///
  Ip4Config2PolicyStatic,
  ///
  /// Under this policy, the Ip4Config2DataTypeManualAddress,
  /// Ip4Config2DataTypeGateway and Ip4Config2DataTypeDnsServer configuration data are
  /// not allowed to set via SetData(). All of these configurations are retrieved from DHCP
  /// server or other auto-configuration mechanism.
  ///
  Ip4Config2PolicyDhcp,
  Ip4Config2PolicyMax
} EFI_IP4_CONFIG2_POLICY;

///
/// EFI_IP4_CONFIG2_MANUAL_ADDRESS
///
typedef struct {
  ///
  /// The IPv4 unicast address.
  ///
  EFI_IPv4_ADDRESS        Address;
  ///
  /// The subnet mask.
  ///
  EFI_IPv4_ADDRESS        SubnetMask;
} EFI_IP4_CONFIG2_MANUAL_ADDRESS;

/**
  Set the configuration for the EFI IPv4 network stack running on the communication device this EFI
  IPv4 Configuration II Protocol instance manages.

  This function is used to set the configuration data of type DataType for the EFI IPv4 network stack
  running on the communication device this EFI IPv4 Configuration II Protocol instance manages.
  The successfully configured data is valid after system reset or power-off.
  The DataSize is used to calculate the count of structure instances in the Data for some
  DataType that multiple structure instances are allowed.
  This function is always non-blocking. When setting some typeof configuration data, an
  asynchronous process is invoked to check the correctness of the data, such as doing address conflict
  detection on the manually set local IPv4 address. EFI_NOT_READY is returned immediately to
  indicate that such an asynchronous process is invoked and the process is not finished yet. The caller
  willing to get the result of the asynchronous process is required to call RegisterDataNotify()
  to register an event on the specified configuration data. Once the event is signaled, the caller can call
  GetData()to get back the configuration data in order to know the result. For other types of
  configuration data that do not require an asynchronous configuration process, the result of the
  operation is immediately returned.

  @param[in]   This               Pointer to the EFI_IP4_CONFIG2_PROTOCOL instance.
  @param[in]   DataType           The type of data to set.
  @param[in]   DataSize           Size of the buffer pointed to by Data in bytes.
  @param[in]   Data               The data buffer to set. The type ofthe data buffer is associated
                                  with the DataType.

  @retval EFI_SUCCESS             The specified configuration data for the EFI IPv4 network stack is set
                                  successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following are TRUE:
                                  This is NULL.
                                  One or more fields in Data and DataSize do not match the
                                  requirement of the data type indicated by DataType.
  @retval EFI_WRITE_PROTECTED     The specified configuration data is read-only or the specified configuration
                                  data can not be set under the current policy.
  @retval EFI_ACCESS_DENIED       Another set operation on the specified configuration data is already in process.
  @retval EFI_NOT_READY           An asynchronous process is invoked to set the specified configuration data and
                                  the process is not finished yet.
  @retval EFI_BAD_BUFFER_SIZE     The DataSize does not match the size of the type indicated by DataType.
  @retval EFI_UNSUPPORTED         This DataType is not supported.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.
  @retval EFI_DEVICE_ERROR        An unexpected system error or network error occurred.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP4_CONFIG2_SET_DATA) (
  IN EFI_IP4_CONFIG2_PROTOCOL   *This,
  IN EFI_IP4_CONFIG2_DATA_TYPE  DataType,
  IN UINTN                      DataSize,
  IN VOID                       *Data
  );

/**
  Get the configuration data for the EFI IPv4 network stack running on the communication device this
  EFI IPv4 Configuration II Protocol instance manages.

  This function returns the configuration data of type DataType for the EFI IPv4 network stack
  running on the communication device this EFI IPv4 Configuration II Protocol instance manages.
  The caller is responsible for allocating the buffer usedto return the specified configuration data and
  the required size will be returned to the caller if the size of the buffer is too small.
  EFI_NOT_READY is returned if the specified configuration data is not ready due to an already in
  progress asynchronous configuration process. The caller can call RegisterDataNotify() to
  register an event on the specified configuration data. Once the asynchronous configuration process is
  finished, the event will be signaled and a subsequent GetData() call will return the specified
  configuration data.

  @param[in]   This               Pointer to the EFI_IP4_CONFIG2_PROTOCOL instance.
  @param[in]   DataType           The type of data to get.
  @param[out]  DataSize           On input, in bytes, the size of Data. On output, in bytes, the size
                                  of buffer required to store the specified configuration data.
  @param[in]   Data               The data buffer in which the configuration data is returned. The
                                  type of the data buffer is associated with the DataType. Ignored
                                  if DataSize is 0.

  @retval EFI_SUCCESS             The specified configuration data is got successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the followings are TRUE:
                                  This is NULL.
                                  DataSize is NULL.
                                  Data is NULL if *DataSizeis not zero.
  @retval EFI_BUFFER_TOO_SMALL    The size of Data is too small for the specified configuration data
                                  and the required size is returned in DataSize.
  @retval EFI_NOT_READY           The specified configuration data is not ready due to an already in
                                  progress asynchronous configuration process.
  @retval EFI_NOT_FOUND           The specified configuration data is not found.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP4_CONFIG2_GET_DATA) (
  IN EFI_IP4_CONFIG2_PROTOCOL     *This,
  IN EFI_IP4_CONFIG2_DATA_TYPE    DataType,
  IN OUT UINTN                    *DataSize,
  IN VOID                         *Data        OPTIONAL
  );

/**
  Register an event that is to be signaled whenever a configuration process on the specified
  configuration data is done.

  This function registers an event that is to be signaled whenever a configuration process on the
  specified configuration data is done. An event can be registered for different DataType
  simultaneously and the caller is responsible for determining which type of configuration data causes
  the signaling of the event in such case.

  @param[in]   This               Pointer to the EFI_IP4_CONFIG2_PROTOCOL instance.
  @param[in]   DataType           The type of data to unregister the event for.
  @param[in]   Event              The event to register.

  @retval EFI_SUCCESS             The notification event for the specified configuration data is
                                  registered.
  @retval EFI_INVALID_PARAMETER   This is NULL or Event is NULL.
  @retval EFI_UNSUPPORTED         The configuration data type specified by DataType is not supported.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.
  @retval EFI_ACCESS_DENIED       The Event is already registered for the DataType.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP4_CONFIG2_REGISTER_NOTIFY) (
  IN EFI_IP4_CONFIG2_PROTOCOL     *This,
  IN EFI_IP4_CONFIG2_DATA_TYPE    DataType,
  IN EFI_EVENT                    Event
  );

/**
  Remove a previously registered event for the specified configuration data.

  This function removes a previously registeredevent for the specified configuration data.

  @param[in]   This               Pointer to the EFI_IP4_CONFIG2_PROTOCOL instance.
  @param[in]   DataType           The type of data to remove the previously registered event for.
  @param[in]   Event              The event to unregister.

  @retval EFI_SUCCESS             The event registered for the specified configuration data is removed.
  @retval EFI_INVALID_PARAMETER   This is NULL or Event is NULL.
  @retval EFI_NOT_FOUND           The Eventhas not been registered for the specified DataType.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_IP4_CONFIG2_UNREGISTER_NOTIFY) (
  IN EFI_IP4_CONFIG2_PROTOCOL     *This,
  IN EFI_IP4_CONFIG2_DATA_TYPE    DataType,
  IN EFI_EVENT                    Event
  );

///
/// The EFI_IP4_CONFIG2_PROTOCOL is designed to be the central repository for the common
/// configurations and the administrator configurable settings for the EFI IPv4 network stack.
/// An EFI IPv4 Configuration II Protocol instance will be installed on each communication device that
/// the EFI IPv4 network stack runs on.
///
struct _EFI_IP4_CONFIG2_PROTOCOL {
  EFI_IP4_CONFIG2_SET_DATA           SetData;
  EFI_IP4_CONFIG2_GET_DATA           GetData;
  EFI_IP4_CONFIG2_REGISTER_NOTIFY    RegisterDataNotify;
  EFI_IP4_CONFIG2_UNREGISTER_NOTIFY  UnregisterDataNotify;
};

extern EFI_GUID gEfiIp4Config2ProtocolGuid;

#endif

