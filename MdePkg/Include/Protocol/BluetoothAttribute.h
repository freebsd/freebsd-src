/** @file
  EFI Bluetooth Attribute Protocol as defined in UEFI 2.7.
  This protocol provides service for Bluetooth ATT (Attribute Protocol) and GATT (Generic
  Attribute Profile) based protocol interfaces.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.7

**/

#ifndef __EFI_BLUETOOTH_ATTRIBUTE_H__
#define __EFI_BLUETOOTH_ATTRIBUTE_H__

#define EFI_BLUETOOTH_ATTRIBUTE_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0x5639867a, 0x8c8e, 0x408d, { 0xac, 0x2f, 0x4b, 0x61, 0xbd, 0xc0, 0xbb, 0xbb } \
  }

#define EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL_GUID \
  { \
    0x898890e9, 0x84b2, 0x4f3a, { 0x8c, 0x58, 0xd8, 0x57, 0x78, 0x13, 0xe0, 0xac } \
  }

typedef struct _EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL;

#pragma pack(1)

//
// Bluetooth UUID
//
typedef struct {
  UINT8                 Length;
  union {
    UINT16              Uuid16;
    UINT32              Uuid32;
    UINT8               Uuid128[16];
  } Data;
} EFI_BLUETOOTH_UUID;


#define UUID_16BIT_TYPE_LEN   2
#define UUID_32BIT_TYPE_LEN   4
#define UUID_128BIT_TYPE_LEN  16

#define BLUETOOTH_IS_ATTRIBUTE_OF_TYPE(a,t) ((a)->Type.Length == UUID_16BIT_TYPE_LEN && (a)->Type.Data.Uuid16 == (t))

//
// Bluetooth Attribute Permission
//
typedef union {
  struct {
    UINT16  Readable            : 1;
    UINT16  ReadEncryption      : 1;
    UINT16  ReadAuthentication  : 1;
    UINT16  ReadAuthorization   : 1;
    UINT16  ReadKeySize         : 5;
    UINT16  Reserved1           : 7;
    UINT16  Writeable           : 1;
    UINT16  WriteEncryption     : 1;
    UINT16  WriteAuthentication : 1;
    UINT16  WriteAuthorization  : 1;
    UINT16  WriteKeySize        : 5;
    UINT16  Reserved2           : 7;
  } Permission;
  UINT32  Data32;
} EFI_BLUETOOTH_ATTRIBUTE_PERMISSION;

typedef struct {
  EFI_BLUETOOTH_UUID                 Type;
  UINT16                             Length;
  UINT16                             AttributeHandle;
  EFI_BLUETOOTH_ATTRIBUTE_PERMISSION AttributePermission;
} EFI_BLUETOOTH_ATTRIBUTE_HEADER;

typedef struct {
  EFI_BLUETOOTH_ATTRIBUTE_HEADER Header;
  UINT16                         EndGroupHandle;
  EFI_BLUETOOTH_UUID             ServiceUuid;
} EFI_BLUETOOTH_GATT_PRIMARY_SERVICE_INFO;

typedef struct {
  EFI_BLUETOOTH_ATTRIBUTE_HEADER Header;
  UINT16                         StartGroupHandle;
  UINT16                         EndGroupHandle;
  EFI_BLUETOOTH_UUID             ServiceUuid;
} EFI_BLUETOOTH_GATT_INCLUDE_SERVICE_INFO;

typedef struct {
  EFI_BLUETOOTH_ATTRIBUTE_HEADER Header;
  UINT8                          CharacteristicProperties;
  UINT16                         CharacteristicValueHandle;
  EFI_BLUETOOTH_UUID             CharacteristicUuid;
} EFI_BLUETOOTH_GATT_CHARACTERISTIC_INFO;

typedef struct {
  EFI_BLUETOOTH_ATTRIBUTE_HEADER Header;
  EFI_BLUETOOTH_UUID             CharacteristicDescriptorUuid;
} EFI_BLUETOOTH_GATT_CHARACTERISTIC_DESCRIPTOR_INFO;

#pragma pack()

typedef struct {
  UINT16                    AttributeHandle;
} EFI_BLUETOOTH_ATTRIBUTE_CALLBACK_PARAMETER_NOTIFICATION;

typedef struct {
  UINT16                    AttributeHandle;
} EFI_BLUETOOTH_ATTRIBUTE_CALLBACK_PARAMETER_INDICATION;

typedef struct {
  UINT32                                                     Version;
  UINT8                                                      AttributeOpCode;
  union {
    EFI_BLUETOOTH_ATTRIBUTE_CALLBACK_PARAMETER_NOTIFICATION  Notification;
    EFI_BLUETOOTH_ATTRIBUTE_CALLBACK_PARAMETER_INDICATION    Indication;
  } Parameter;
} EFI_BLUETOOTH_ATTRIBUTE_CALLBACK_PARAMETER;

typedef struct {
  UINT32               Version;
  BLUETOOTH_LE_ADDRESS BD_ADDR;
  BLUETOOTH_LE_ADDRESS DirectAddress;
  UINT8                RSSI;
  UINTN                AdvertisementDataSize;
  VOID                 *AdvertisementData;
} EFI_BLUETOOTH_LE_DEVICE_INFO;

/**
  The callback function to send request.

  @param[in]  This                Pointer to the EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL instance.
  @param[in]  Data                Data received. The first byte is the attribute opcode, followed by opcode specific
                                  fields. See Bluetooth specification, Vol 3, Part F, Attribute Protocol. It might be a
                                  normal RESPONSE message, or ERROR RESPONSE messag
  @param[in]  DataLength          The length of Data in bytes.
  @param[in]  Context             The context passed from the callback registration request.

  @retval EFI_SUCCESS   The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_ATTRIBUTE_CALLBACK_FUNCTION) (
  IN EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL *This,
  IN VOID                             *Data,
  IN UINTN                            DataLength,
  IN VOID                             *Context
  );

/**
  Send a "REQUEST" or "COMMAND" message to remote server and receive a "RESPONSE" message
  for "REQUEST" from remote server according to Bluetooth attribute protocol data unit(PDU).

  @param[in]  This              Pointer to the EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL instance.
  @param[in]  Data              Data of a REQUEST or COMMAND message. The first byte is the attribute PDU
                                related opcode, followed by opcode specific fields. See Bluetooth specification,
                                Vol 3, Part F, Attribute Protocol.
  @param[in]  DataLength        The length of Data in bytes.
  @param[in]  Callback          Callback function to notify the RESPONSE is received to the caller, with the
                                response buffer. Caller must check the response buffer content to know if the
                                request action is success or fail. It may be NULL if the data is a COMMAND.
  @param[in]  Context           Data passed into Callback function. It is optional parameter and may be NULL.

  @retval EFI_SUCCESS           The request is sent successfully.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid due to following conditions:
                                - The Buffer is NULL.
                                - The BufferLength is 0.
                                - The opcode in Buffer is not a valid OPCODE according to Bluetooth specification.
                                - The Callback is NULL.
  @retval EFI_DEVICE_ERROR      Sending the request failed due to the host controller or the device error.
  @retval EFI_NOT_READY         A GATT operation is already underway for this device.
  @retval EFI_UNSUPPORTED       The attribute does not support the corresponding operation.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_ATTRIBUTE_SEND_REQUEST) (
  IN EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL            *This,
  IN VOID                                        *Data,
  IN UINTN                                       DataLength,
  IN EFI_BLUETOOTH_ATTRIBUTE_CALLBACK_FUNCTION   Callback,
  IN VOID                                        *Context
  );

/**
  Register or unregister a server initiated message, such as NOTIFICATION or INDICATION, on a
  characteristic value on remote server.

  @param[in]  This              Pointer to the EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL instance.
  @param[in]  CallbackParameter The parameter of the callback.
  @param[in]  Callback          Callback function for server initiated attribute protocol. NULL callback
                                function means unregister the server initiated callback.
  @param[in]  Context           Data passed into Callback function. It is optional parameter and may be NULL.

  @retval EFI_SUCCESS           The callback function is registered or unregistered successfully
  @retval EFI_INVALID_PARAMETER The attribute opcode is not server initiated message opcode. See
                                Bluetooth specification, Vol 3, Part F, Attribute Protocol.
  @retval EFI_ALREADY_STARTED   A callback function is already registered on the same attribute
                                opcode and attribute handle, when the Callback is not NULL.
  @retval EFI_NOT_STARTED       A callback function is not registered on the same attribute opcode
                                and attribute handle, when the Callback is NULL.
  @retval EFI_NOT_READY         A GATT operation is already underway for this device.
  @retval EFI_UNSUPPORTED       The attribute does not support notification.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_ATTRIBUTE_REGISTER_FOR_SERVER_NOTIFICATION)(
  IN  EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL           *This,
  IN  EFI_BLUETOOTH_ATTRIBUTE_CALLBACK_PARAMETER *CallbackParameter,
  IN  EFI_BLUETOOTH_ATTRIBUTE_CALLBACK_FUNCTION  Callback,
  IN  VOID                                       *Context
  );

/**
  Get Bluetooth discovered service information.

  @param[in]  This              Pointer to the EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL instance.
  @param[out] ServiceInfoSize   A pointer to the size, in bytes, of the ServiceInfo buffer.
  @param[out] ServiceInfo       A pointer to a callee allocated buffer that returns Bluetooth
                                discovered service information. Callee allocates this buffer by
                                using EFI Boot Service AllocatePool().

  @retval EFI_SUCCESS           The Bluetooth discovered service information is returned successfully.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to retrieve the Bluetooth discovered
                                service information.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_ATTRIBUTE_GET_SERVICE_INFO)(
  IN EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL      *This,
  OUT UINTN                                *ServiceInfoSize,
  OUT VOID                                 **ServiceInfo
  );

/**
  Get Bluetooth device information.

  @param[in]  This              Pointer to the EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL instance.
  @param[out] DeviceInfoSize    A pointer to the size, in bytes, of the DeviceInfo buffer.
  @param[out] DeviceInfo        A pointer to a callee allocated buffer that returns Bluetooth
                                device information. Callee allocates this buffer by using EFI Boot
                                Service AllocatePool(). If this device is Bluetooth classic
                                device, EFI_BLUETOOTH_DEVICE_INFO should be used. If
                                this device is Bluetooth LE device, EFI_BLUETOOTH_LE_DEVICE_INFO
                                should be used.

  @retval EFI_SUCCESS           The Bluetooth device information is returned successfully.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to retrieve the Bluetooth device
                                information

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_ATTRIBUTE_GET_DEVICE_INFO)(
  IN  EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL  *This,
  OUT UINTN                             *DeviceInfoSize,
  OUT VOID                              **DeviceInfo
  );

struct _EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL {
  EFI_BLUETOOTH_ATTRIBUTE_SEND_REQUEST                     SendRequest;
  EFI_BLUETOOTH_ATTRIBUTE_REGISTER_FOR_SERVER_NOTIFICATION RegisterForServerNotification;
  EFI_BLUETOOTH_ATTRIBUTE_GET_SERVICE_INFO                 GetServiceInfo;
  EFI_BLUETOOTH_ATTRIBUTE_GET_DEVICE_INFO                  GetDeviceInfo;
};


extern EFI_GUID gEfiBluetoothAttributeProtocolGuid;
extern EFI_GUID gEfiBluetoothAttributeServiceBindingProtocolGuid;

#endif

