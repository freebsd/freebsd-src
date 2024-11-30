/** @file
  EFI Bluetooth Configuration Protocol as defined in UEFI 2.7.
  This protocol abstracts user interface configuration for Bluetooth device.

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.7

**/

#ifndef __EFI_BLUETOOTH_CONFIG_PROTOCOL_H__
#define __EFI_BLUETOOTH_CONFIG_PROTOCOL_H__

#include <IndustryStandard/Bluetooth.h>

#define EFI_BLUETOOTH_CONFIG_PROTOCOL_GUID \
  { \
    0x62960cf3, 0x40ff, 0x4263, { 0xa7, 0x7c, 0xdf, 0xde, 0xbd, 0x19, 0x1b, 0x4b } \
  }

typedef struct _EFI_BLUETOOTH_CONFIG_PROTOCOL EFI_BLUETOOTH_CONFIG_PROTOCOL;

typedef UINT32 EFI_BLUETOOTH_CONFIG_REMOTE_DEVICE_STATE_TYPE;
#define EFI_BLUETOOTH_CONFIG_REMOTE_DEVICE_STATE_CONNECTED  0x1
#define EFI_BLUETOOTH_CONFIG_REMOTE_DEVICE_STATE_PAIRED     0x2

///
/// EFI_BLUETOOTH_SCAN_CALLBACK_INFORMATION
///
typedef struct {
  ///
  /// 48bit Bluetooth device address.
  ///
  BLUETOOTH_ADDRESS            BDAddr;
  ///
  /// State of the remote deive
  ///
  UINT8                        RemoteDeviceState;
  ///
  /// Bluetooth ClassOfDevice. See Bluetooth specification for detail.
  ///
  BLUETOOTH_CLASS_OF_DEVICE    ClassOfDevice;
  ///
  /// Remote device name
  ///
  UINT8                        RemoteDeviceName[BLUETOOTH_HCI_COMMAND_LOCAL_READABLE_NAME_MAX_SIZE];
} EFI_BLUETOOTH_SCAN_CALLBACK_INFORMATION;

///
/// EFI_BLUETOOTH_CONFIG_DATA_TYPE
///
typedef enum {
  ///
  /// Local/Remote Bluetooth device name. Data structure is zero terminated CHAR8[].
  ///
  EfiBluetoothConfigDataTypeDeviceName,
  ///
  /// Local/Remote Bluetooth device ClassOfDevice. Data structure is BLUETOOTH_CLASS_OF_DEVICE.
  ///
  EfiBluetoothConfigDataTypeClassOfDevice,
  ///
  /// Remote Bluetooth device state. Data structure is EFI_BLUETOOTH_CONFIG_REMOTE_DEVICE_STATE_TYPE.
  ///
  EfiBluetoothConfigDataTypeRemoteDeviceState, /* Relevant for LE*/
  ///
  /// Local/Remote Bluetooth device SDP information. Data structure is UINT8[].
  ///
  EfiBluetoothConfigDataTypeSdpInfo,
  ///
  /// Local Bluetooth device address. Data structure is BLUETOOTH_ADDRESS.
  ///
  EfiBluetoothConfigDataTypeBDADDR, /* Relevant for LE*/
  ///
  /// Local Bluetooth discoverable state. Data structure is UINT8. (Page scan and/or Inquiry scan)
  ///
  EfiBluetoothConfigDataTypeDiscoverable, /* Relevant for LE*/
  ///
  /// Local Bluetooth controller stored paired device list. Data structure is BLUETOOTH_ADDRESS[].
  ///
  EfiBluetoothConfigDataTypeControllerStoredPairedDeviceList,
  ///
  /// Local available device list. Data structure is BLUETOOTH_ADDRESS[].
  ///
  EfiBluetoothConfigDataTypeAvailableDeviceList,
  EfiBluetoothConfigDataTypeRandomAddress, /* Relevant for LE*/
  EfiBluetoothConfigDataTypeRSSI,          /* Relevant for LE*/
  ///
  /// Advertisement report. Data structure is UNIT8[].
  ///
  EfiBluetoothConfigDataTypeAdvertisementData, /* Relevant for LE*/
  EfiBluetoothConfigDataTypeIoCapability,      /* Relevant for LE*/
  EfiBluetoothConfigDataTypeOOBDataFlag,       /* Relevant for LE*/
  ///
  /// KeyType of Authentication Requirements flag of local
  /// device as UINT8, indicating requested security properties.
  /// See Bluetooth specification 3.H.3.5.1. BIT0: MITM, BIT1:SC.
  ///
  EfiBluetoothConfigDataTypeKeyType,    /* Relevant for LE*/
  EfiBluetoothConfigDataTypeEncKeySize, /* Relevant for LE*/
  EfiBluetoothConfigDataTypeMax,
} EFI_BLUETOOTH_CONFIG_DATA_TYPE;

///
/// EFI_BLUETOOTH_PIN_CALLBACK_TYPE.
///
typedef enum {
  ///
  /// For SSP - passkey entry. Input buffer is Passkey (4 bytes). No output buffer.
  /// See Bluetooth HCI command for detail.
  ///
  EfiBluetoothCallbackTypeUserPasskeyNotification,
  ///
  /// For SSP - just work and numeric comparison. Input buffer is numeric value (4 bytes).
  /// Output buffer is BOOLEAN (1 byte). See Bluetooth HCI command for detail.
  ///
  EfiBluetoothCallbackTypeUserConfirmationRequest,
  ///
  /// For SSP - OOB. See Bluetooth HCI command for detail.
  ///
  EfiBluetoothCallbackTypeOOBDataRequest,
  ///
  /// For legacy paring. No input buffer. Output buffer is PIN code( <= 16 bytes).
  /// See Bluetooth HCI command for detail.
  ///
  EfiBluetoothCallbackTypePinCodeRequest,
  EfiBluetoothCallbackTypeMax
} EFI_BLUETOOTH_PIN_CALLBACK_TYPE;

///
/// EFI_BLUETOOTH_CONNECT_COMPLETE_CALLBACK_TYPE.
///
typedef enum {
  ///
  /// This callback is called when Bluetooth receive Disconnection_Complete event. Input buffer is Event
  /// Parameters of Disconnection_Complete Event defined in Bluetooth specification.
  ///
  EfiBluetoothConnCallbackTypeDisconnected,
  ///
  /// This callback is called when Bluetooth receive Connection_Complete event. Input buffer is Event
  /// Parameters of Connection_Complete Event defined in Bluetooth specification.
  ///
  EfiBluetoothConnCallbackTypeConnected,
  ///
  /// This callback is called when Bluetooth receive Authentication_Complete event. Input buffer is Event
  /// Parameters of Authentication_Complete Event defined in Bluetooth specification.
  ///
  EfiBluetoothConnCallbackTypeAuthenticated,
  ///
  /// This callback is called when Bluetooth receive Encryption_Change event. Input buffer is Event
  /// Parameters of Encryption_Change Event defined in Bluetooth specification.
  ///
  EfiBluetoothConnCallbackTypeEncrypted
} EFI_BLUETOOTH_CONNECT_COMPLETE_CALLBACK_TYPE;

/**
  Initialize Bluetooth host controller and local device.

  @param  This          Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.

  @retval EFI_SUCCESS           The Bluetooth host controller and local device is initialized successfully.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to initialize the Bluetooth host controller
                                and local device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_INIT)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL  *This
  );

/**
  Callback function, it is called if a Bluetooth device is found during scan process.

  @param  This            Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  Context         Context passed from scan request.
  @param  CallbackInfo    Data related to scan result. NULL CallbackInfo means scan complete.

  @retval EFI_SUCCESS       The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_SCAN_CALLBACK_FUNCTION)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL            *This,
  IN VOID                                     *Context,
  IN EFI_BLUETOOTH_SCAN_CALLBACK_INFORMATION  *CallbackInfo
  );

/**
  Scan Bluetooth device.

  @param  This          Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  ReScan        If TRUE, a new scan request is submitted no matter there is scan result before.
                        If FALSE and there is scan result, the previous scan result is returned and no scan request
                        is submitted.
  @param  ScanType      Bluetooth scan type, Inquiry and/or Page. See Bluetooth specification for detail.
  @param  Callback      The callback function. This function is called if a Bluetooth device is found during scan
                        process.
  @param  Context       Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS           The Bluetooth scan request is submitted.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to scan the Bluetooth device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_SCAN)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                  *This,
  IN BOOLEAN                                        ReScan,
  IN UINT8                                          ScanType,
  IN EFI_BLUETOOTH_CONFIG_SCAN_CALLBACK_FUNCTION    Callback,
  IN VOID                                           *Context
  );

/**
  Connect a Bluetooth device.

  @param  This          Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  BD_ADDR       The address of Bluetooth device to be connected.

  @retval EFI_SUCCESS           The Bluetooth device is connected successfully.
  @retval EFI_ALREADY_STARTED   The Bluetooth device is already connected.
  @retval EFI_NOT_FOUND         The Bluetooth device is not found.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to connect the Bluetooth device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_CONNECT)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                  *This,
  IN BLUETOOTH_ADDRESS                              *BD_ADDR
  );

/**
  Disconnect a Bluetooth device.

  @param  This          Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  BD_ADDR       The address of Bluetooth device to be connected.
  @param  Reason        Bluetooth disconnect reason. See Bluetooth specification for detail.

  @retval EFI_SUCCESS           The Bluetooth device is disconnected successfully.
  @retval EFI_NOT_STARTED       The Bluetooth device is not connected.
  @retval EFI_NOT_FOUND         The Bluetooth device is not found.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to disconnect the Bluetooth device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_DISCONNECT)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                  *This,
  IN BLUETOOTH_ADDRESS                              *BD_ADDR,
  IN UINT8                                          Reason
  );

/**
  Get Bluetooth configuration data.

  @param  This          Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  DataType      Configuration data type.
  @param  DataSize      On input, indicates the size, in bytes, of the data buffer specified by Data.
                        On output, indicates the amount of data actually returned.
  @param  Data          A pointer to the buffer of data that will be returned.

  @retval EFI_SUCCESS           The Bluetooth configuration data is returned successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - DataSize is NULL.
                                - *DataSize is not 0 and Data is NULL.
  @retval EFI_UNSUPPORTED       The DataType is unsupported.
  @retval EFI_NOT_FOUND         The DataType is not found.
  @retval EFI_BUFFER_TOO_SMALL  The buffer is too small to hold the buffer.
                                *DataSize has been updated with the size needed to complete the request.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_GET_DATA)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                  *This,
  IN EFI_BLUETOOTH_CONFIG_DATA_TYPE                 DataType,
  IN OUT UINTN                                      *DataSize,
  IN OUT VOID                                       *Data
  );

/**
  Set Bluetooth configuration data.

  @param  This          Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  DataType      Configuration data type.
  @param  DataSize      Indicates the size, in bytes, of the data buffer specified by Data.
  @param  Data          A pointer to the buffer of data that will be set.

  @retval EFI_SUCCESS           The Bluetooth configuration data is set successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - DataSize is 0.
                                - Data is NULL.
  @retval EFI_UNSUPPORTED       The DataType is unsupported.
  @retval EFI_BUFFER_TOO_SMALL  Cannot set configuration data.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_SET_DATA)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                  *This,
  IN EFI_BLUETOOTH_CONFIG_DATA_TYPE                 DataType,
  IN UINTN                                          DataSize,
  IN VOID                                           *Data
  );

/**
  Get remove Bluetooth device configuration data.

  @param  This          Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  DataType      Configuration data type.
  @param  BDAddr        Remote Bluetooth device address.
  @param  DataSize      On input, indicates the size, in bytes, of the data buffer specified by Data.
                        On output, indicates the amount of data actually returned.
  @param  Data          A pointer to the buffer of data that will be returned.

  @retval EFI_SUCCESS           The remote Bluetooth device configuration data is returned successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - DataSize is NULL.
                                - *DataSize is not 0 and Data is NULL.
  @retval EFI_UNSUPPORTED       The DataType is unsupported.
  @retval EFI_NOT_FOUND         The DataType is not found.
  @retval EFI_BUFFER_TOO_SMALL  The buffer is too small to hold the buffer.
                                *DataSize has been updated with the size needed to complete the request.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_GET_REMOTE_DATA)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                  *This,
  IN EFI_BLUETOOTH_CONFIG_DATA_TYPE                 DataType,
  IN BLUETOOTH_ADDRESS                              *BDAddr,
  IN OUT UINTN                                      *DataSize,
  IN OUT VOID                                       *Data
  );

/**
  The callback function for PIN code.

  @param  This                Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  Context             Context passed from registration.
  @param  CallbackType        Callback type in EFI_BLUETOOTH_PIN_CALLBACK_TYPE.
  @param  InputBuffer         A pointer to the buffer of data that is input from callback caller.
  @param  InputBufferSize     Indicates the size, in bytes, of the data buffer specified by InputBuffer.
  @param  OutputBuffer        A pointer to the buffer of data that will be output from callback callee.
                              Callee allocates this buffer by using EFI Boot Service AllocatePool().
  @param  OutputBufferSize    Indicates the size, in bytes, of the data buffer specified by OutputBuffer.

  @retval EFI_SUCCESS   The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_REGISTER_PIN_CALLBACK_FUNCTION)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                        *This,
  IN VOID                                                 *Context,
  IN EFI_BLUETOOTH_PIN_CALLBACK_TYPE                      CallbackType,
  IN VOID                                                 *InputBuffer,
  IN UINTN                                                InputBufferSize,
  OUT VOID                                                **OutputBuffer,
  OUT UINTN                                               *OutputBufferSize
  );

/**
  Register PIN callback function.

  @param  This          Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  Callback      The callback function. NULL means unregister.
  @param  Context       Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS   The PIN callback function is registered successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_REGISTER_PIN_CALLBACK)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                        *This,
  IN EFI_BLUETOOTH_CONFIG_REGISTER_PIN_CALLBACK_FUNCTION  Callback,
  IN VOID                                                 *Context
  );

/**
  The callback function to get link key.

  @param  This                Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  Context             Context passed from registration.
  @param  BDAddr              A pointer to Bluetooth device address.
  @param  LinkKey             A pointer to the buffer of link key.

  @retval EFI_SUCCESS   The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_REGISTER_GET_LINK_KEY_CALLBACK_FUNCTION)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL        *This,
  IN VOID                                 *Context,
  IN BLUETOOTH_ADDRESS                    *BDAddr,
  OUT UINT8                               LinkKey[BLUETOOTH_HCI_LINK_KEY_SIZE]
  );

/**
  Register get link key callback function.

  @param  This          Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  Callback      The callback function. NULL means unregister.
  @param  Context       Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS   The link key callback function is registered successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_REGISTER_GET_LINK_KEY_CALLBACK)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                                  *This,
  IN EFI_BLUETOOTH_CONFIG_REGISTER_GET_LINK_KEY_CALLBACK_FUNCTION   Callback,
  IN VOID                                                           *Context
  );

/**
  The callback function to set link key.

  @param  This                Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  Context             Context passed from registration.
  @param  BDAddr              A pointer to Bluetooth device address.
  @param  LinkKey             A pointer to the buffer of link key.

  @retval EFI_SUCCESS   The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_REGISTER_SET_LINK_KEY_CALLBACK_FUNCTION)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL        *This,
  IN VOID                                 *Context,
  IN BLUETOOTH_ADDRESS                    *BDAddr,
  IN UINT8                                LinkKey[BLUETOOTH_HCI_LINK_KEY_SIZE]
  );

/**
  Register set link key callback function.

  @param  This          Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  Callback      The callback function. NULL means unregister.
  @param  Context       Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS   The link key callback function is registered successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_REGISTER_SET_LINK_KEY_CALLBACK)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                                  *This,
  IN EFI_BLUETOOTH_CONFIG_REGISTER_SET_LINK_KEY_CALLBACK_FUNCTION   Callback,
  IN VOID                                                           *Context
  );

/**
  The callback function. It is called after connect completed.

  @param  This                Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  Context             Context passed from registration.
  @param  CallbackType        Callback type in EFI_BLUETOOTH_CONNECT_COMPLETE_CALLBACK_TYPE.
  @param  BDAddr              A pointer to Bluetooth device address.
  @param  InputBuffer         A pointer to the buffer of data that is input from callback caller.
  @param  InputBufferSize     Indicates the size, in bytes, of the data buffer specified by InputBuffer.

  @retval EFI_SUCCESS   The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_REGISTER_CONNECT_COMPLETE_CALLBACK_FUNCTION)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                  *This,
  IN VOID                                           *Context,
  IN EFI_BLUETOOTH_CONNECT_COMPLETE_CALLBACK_TYPE   CallbackType,
  IN BLUETOOTH_ADDRESS                              *BDAddr,
  IN VOID                                           *InputBuffer,
  IN UINTN                                          InputBufferSize
  );

/**
  Register link connect complete callback function.

  @param  This          Pointer to the EFI_BLUETOOTH_CONFIG_PROTOCOL instance.
  @param  Callback      The callback function. NULL means unregister.
  @param  Context       Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS   The link connect complete callback function is registered successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_CONFIG_REGISTER_CONNECT_COMPLETE_CALLBACK)(
  IN EFI_BLUETOOTH_CONFIG_PROTOCOL                                      *This,
  IN EFI_BLUETOOTH_CONFIG_REGISTER_CONNECT_COMPLETE_CALLBACK_FUNCTION   Callback,
  IN VOID                                                               *Context
  );

///
/// This protocol abstracts user interface configuration for Bluetooth device.
///
struct _EFI_BLUETOOTH_CONFIG_PROTOCOL {
  EFI_BLUETOOTH_CONFIG_INIT                                  Init;
  EFI_BLUETOOTH_CONFIG_SCAN                                  Scan;
  EFI_BLUETOOTH_CONFIG_CONNECT                               Connect;
  EFI_BLUETOOTH_CONFIG_DISCONNECT                            Disconnect;
  EFI_BLUETOOTH_CONFIG_GET_DATA                              GetData;
  EFI_BLUETOOTH_CONFIG_SET_DATA                              SetData;
  EFI_BLUETOOTH_CONFIG_GET_REMOTE_DATA                       GetRemoteData;
  EFI_BLUETOOTH_CONFIG_REGISTER_PIN_CALLBACK                 RegisterPinCallback;
  EFI_BLUETOOTH_CONFIG_REGISTER_GET_LINK_KEY_CALLBACK        RegisterGetLinkKeyCallback;
  EFI_BLUETOOTH_CONFIG_REGISTER_SET_LINK_KEY_CALLBACK        RegisterSetLinkKeyCallback;
  EFI_BLUETOOTH_CONFIG_REGISTER_CONNECT_COMPLETE_CALLBACK    RegisterLinkConnectCompleteCallback;
};

extern EFI_GUID  gEfiBluetoothConfigProtocolGuid;

#endif
