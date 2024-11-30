/** @file
  EFI Bluetooth LE Config Protocol as defined in UEFI 2.7.
  This protocol abstracts user interface configuration for BluetoothLe device.

  Copyright (c) 2017 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.7

**/

#ifndef __EFI_BLUETOOTH_LE_CONFIG_H__
#define __EFI_BLUETOOTH_LE_CONFIG_H__

#include <Protocol/BluetoothConfig.h>
#include <Protocol/BluetoothAttribute.h>

#define EFI_BLUETOOTH_LE_CONFIG_PROTOCOL_GUID \
  { \
    0x8f76da58, 0x1f99, 0x4275, { 0xa4, 0xec, 0x47, 0x56, 0x51, 0x5b, 0x1c, 0xe8 } \
  }

typedef struct _EFI_BLUETOOTH_LE_CONFIG_PROTOCOL EFI_BLUETOOTH_LE_CONFIG_PROTOCOL;

/**
  Initialize BluetoothLE host controller and local device.

  The Init() function initializes BluetoothLE host controller and local device.

  @param[in]  This              Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.

  @retval EFI_SUCCESS           The BluetoothLE host controller and local device is initialized successfully.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to initialize the BluetoothLE host controller
                                and local device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_INIT)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL  *This
  );

typedef struct {
  ///
  /// The version of the structure. A value of zero represents the EFI_BLUETOOTH_LE_CONFIG_SCAN_PARAMETER
  /// structure as defined here. Future version of this specification may extend this data structure in a
  /// backward compatible way and increase the value of Version.
  ///
  UINT32    Version;
  ///
  /// Passive scanning or active scanning. See Bluetooth specification.
  ///
  UINT8     ScanType;
  ///
  /// Recommended scan interval to be used while performing scan.
  ///
  UINT16    ScanInterval;
  ///
  /// Recommended scan window to be used while performing a scan.
  ///
  UINT16    ScanWindow;
  ///
  /// Recommended scanning filter policy to be used while performing a scan.
  ///
  UINT8     ScanningFilterPolicy;
  ///
  /// This is one byte flag to serve as a filter to remove unneeded scan
  /// result. For example, set BIT0 means scan in LE Limited Discoverable
  /// Mode. Set BIT1 means scan in LE General Discoverable Mode.
  ///
  UINT8     AdvertisementFlagFilter;
} EFI_BLUETOOTH_LE_CONFIG_SCAN_PARAMETER;

typedef struct {
  BLUETOOTH_LE_ADDRESS    BDAddr;
  BLUETOOTH_LE_ADDRESS    DirectAddress;
  UINT8                   RemoteDeviceState;
  INT8                    RSSI;
  UINTN                   AdvertisementDataSize;
  VOID                    *AdvertisementData;
} EFI_BLUETOOTH_LE_SCAN_CALLBACK_INFORMATION;

/**
  Callback function, it is called if a BluetoothLE device is found during scan process.

  @param[in]  This            Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  Context         Context passed from scan request.
  @param[in]  CallbackInfo    Data related to scan result. NULL CallbackInfo means scan complete.

  @retval EFI_SUCCESS         The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_SCAN_CALLBACK_FUNCTION)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL             *This,
  IN VOID                                         *Context,
  IN EFI_BLUETOOTH_LE_SCAN_CALLBACK_INFORMATION   *CallbackInfo
  );

/**
  Scan BluetoothLE device.

  The Scan() function scans BluetoothLE device. When this function is returned, it just means scan
  request is submitted. It does not mean scan process is started or finished. Whenever there is a
  BluetoothLE device is found, the Callback function will be called. Callback function might be
  called before this function returns or after this function returns

  @param[in]  This              Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  ReScan            If TRUE, a new scan request is submitted no matter there is scan result before.
                                If FALSE and there is scan result, the previous scan result is returned and no scan request
                                is submitted.
  @param[in]  Timeout           Duration in milliseconds for which to scan.
  @param[in]  ScanParameter     If it is not NULL, the ScanParameter is used to perform a scan by the BluetoothLE bus driver.
                                If it is NULL, the default parameter is used.
  @param[in]  Callback          The callback function. This function is called if a BluetoothLE device is found during
                                scan process.
  @param[in]  Context           Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS           The Bluetooth scan request is submitted.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to scan the BluetoothLE device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_SCAN)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL                *This,
  IN BOOLEAN                                         ReScan,
  IN UINT32                                          Timeout,
  IN EFI_BLUETOOTH_LE_CONFIG_SCAN_PARAMETER          *ScanParameter  OPTIONAL,
  IN EFI_BLUETOOTH_LE_CONFIG_SCAN_CALLBACK_FUNCTION  Callback,
  IN VOID                                            *Context
  );

typedef struct {
  ///
  /// The version of the structure. A value of zero represents the
  /// EFI_BLUETOOTH_LE_CONFIG_CONNECT_PARAMETER
  /// structure as defined here. Future version of this specification may
  /// extend this data structure in a backward compatible way and
  /// increase the value of Version.
  ///
  UINT32    Version;
  ///
  /// Recommended scan interval to be used while performing scan before connect.
  ///
  UINT16    ScanInterval;
  ///
  /// Recommended scan window to be used while performing a connection
  ///
  UINT16    ScanWindow;
  ///
  /// Minimum allowed connection interval. Shall be less than or equal to ConnIntervalMax.
  ///
  UINT16    ConnIntervalMin;
  ///
  /// Maximum allowed connection interval. Shall be greater than or equal to ConnIntervalMin.
  ///
  UINT16    ConnIntervalMax;
  ///
  /// Slave latency for the connection in number of connection events.
  ///
  UINT16    ConnLatency;
  ///
  /// Link supervision timeout for the connection.
  ///
  UINT16    SupervisionTimeout;
} EFI_BLUETOOTH_LE_CONFIG_CONNECT_PARAMETER;

/**
  Connect a BluetoothLE device.

  The Connect() function connects a Bluetooth device. When this function is returned successfully,
  a new EFI_BLUETOOTH_IO_PROTOCOL is created.

  @param[in]  This              Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  AutoReconnect     If TRUE, the BluetoothLE host controller needs to do an auto
                                reconnect. If FALSE, the BluetoothLE host controller does not do
                                an auto reconnect.
  @param[in]  DoBonding         If TRUE, the BluetoothLE host controller needs to do a bonding.
                                If FALSE, the BluetoothLE host controller does not do a bonding.
  @param[in]  ConnectParameter  If it is not NULL, the ConnectParameter is used to perform a
                                scan by the BluetoothLE bus driver. If it is NULL, the default
                                parameter is used.
  @param[in]  BD_ADDR           The address of the BluetoothLE device to be connected.

  @retval EFI_SUCCESS           The BluetoothLE device is connected successfully.
  @retval EFI_ALREADY_STARTED   The BluetoothLE device is already connected.
  @retval EFI_NOT_FOUND         The BluetoothLE device is not found.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to connect the BluetoothLE device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_CONNECT)(
  IN  EFI_BLUETOOTH_LE_CONFIG_PROTOCOL            *This,
  IN  BOOLEAN                                     AutoReconnect,
  IN  BOOLEAN                                     DoBonding,
  IN  EFI_BLUETOOTH_LE_CONFIG_CONNECT_PARAMETER   *ConnectParameter  OPTIONAL,
  IN  BLUETOOTH_LE_ADDRESS                        *BD_ADDR
  );

/**
  Disconnect a BluetoothLE device.

  The Disconnect() function disconnects a BluetoothLE device. When this function is returned
  successfully, the EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL associated with this device is
  destroyed and all services associated are stopped.

  @param[in]  This          Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  BD_ADDR       The address of BluetoothLE device to be connected.
  @param[in]  Reason        Bluetooth disconnect reason. See Bluetooth specification for detail.

  @retval EFI_SUCCESS           The BluetoothLE device is disconnected successfully.
  @retval EFI_NOT_STARTED       The BluetoothLE device is not connected.
  @retval EFI_NOT_FOUND         The BluetoothLE device is not found.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to disconnect the BluetoothLE device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_DISCONNECT)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL  *This,
  IN BLUETOOTH_LE_ADDRESS              *BD_ADDR,
  IN UINT8                             Reason
  );

/**
  Get BluetoothLE configuration data.

  The GetData() function returns BluetoothLE configuration data. For remote BluetoothLE device
  configuration data, please use GetRemoteData() function with valid BD_ADDR.

  @param[in]       This         Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]       DataType     Configuration data type.
  @param[in, out]  DataSize     On input, indicates the size, in bytes, of the data buffer specified by Data.
                                On output, indicates the amount of data actually returned.
  @param[in, out]  Data         A pointer to the buffer of data that will be returned.

  @retval EFI_SUCCESS           The BluetoothLE configuration data is returned successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - DataSize is NULL.
                                - *DataSize is 0.
                                - Data is NULL.
  @retval EFI_UNSUPPORTED       The DataType is unsupported.
  @retval EFI_NOT_FOUND         The DataType is not found.
  @retval EFI_BUFFER_TOO_SMALL  The buffer is too small to hold the buffer.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_GET_DATA)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL       *This,
  IN EFI_BLUETOOTH_CONFIG_DATA_TYPE      DataType,
  IN OUT UINTN                           *DataSize,
  IN OUT VOID                            *Data OPTIONAL
  );

/**
  Set BluetoothLE configuration data.

  The SetData() function sets local BluetoothLE device configuration data. Not all DataType can be
  set.

  @param[in]  This              Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  DataType          Configuration data type.
  @param[in]  DataSize          Indicates the size, in bytes, of the data buffer specified by Data.
  @param[in]  Data              A pointer to the buffer of data that will be set.

  @retval EFI_SUCCESS           The BluetoothLE configuration data is set successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - DataSize is 0.
                                - Data is NULL.
  @retval EFI_UNSUPPORTED       The DataType is unsupported.
  @retval EFI_WRITE_PROTECTED   Cannot set configuration data.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_SET_DATA)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL       *This,
  IN EFI_BLUETOOTH_CONFIG_DATA_TYPE         DataType,
  IN UINTN                                  DataSize,
  IN VOID                                   *Data
  );

/**
  Get remove BluetoothLE device configuration data.

  The GetRemoteData() function returns remote BluetoothLE device configuration data.

  @param[in]  This              Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  DataType          Configuration data type.
  @param[in]  BDAddr            Remote BluetoothLE device address.
  @param[in, out] DataSize      On input, indicates the size, in bytes, of the data buffer specified by Data.
                                On output, indicates the amount of data actually returned.
  @param[in, out] Data          A pointer to the buffer of data that will be returned.

  @retval EFI_SUCCESS           The remote BluetoothLE device configuration data is returned successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - DataSize is NULL.
                                - *DataSize is 0.
                                - Data is NULL.
  @retval EFI_UNSUPPORTED       The DataType is unsupported.
  @retval EFI_NOT_FOUND         The DataType is not found.
  @retval EFI_BUFFER_TOO_SMALL  The buffer is too small to hold the buffer.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_GET_REMOTE_DATA)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL       *This,
  IN EFI_BLUETOOTH_CONFIG_DATA_TYPE         DataType,
  IN BLUETOOTH_LE_ADDRESS                   *BDAddr,
  IN OUT UINTN                              *DataSize,
  IN OUT VOID                               *Data
  );

typedef enum {
  ///
  /// It indicates an authorization request. No data is associated with the callback
  /// input. In the output data, the application should return the authorization value.
  /// The data structure is BOOLEAN. TRUE means YES. FALSE means NO.
  ///
  EfiBluetoothSmpAuthorizationRequestEvent,
  ///
  /// It indicates that a passkey has been generated locally by the driver, and the same
  /// passkey should be entered at the remote device. The callback input data is the
  /// passkey of type UINT32, to be displayed by the application. No output data
  /// should be returned.
  ///
  EfiBluetoothSmpPasskeyReadyEvent,
  ///
  /// It indicates that the driver is requesting for the passkey has been generated at
  /// the remote device. No data is associated with the callback input. The output data
  /// is the passkey of type UINT32, to be entered by the user.
  ///
  EfiBluetoothSmpPasskeyRequestEvent,
  ///
  /// It indicates that the driver is requesting for the passkey that has been pre-shared
  /// out-of-band with the remote device. No data is associated with the callback
  /// input. The output data is the stored OOB data of type UINT8[16].
  ///
  EfiBluetoothSmpOOBDataRequestEvent,
  ///
  /// In indicates that a number have been generated locally by the bus driver, and
  /// also at the remote device, and the bus driver wants to know if the two numbers
  /// match. The callback input data is the number of type UINT32. The output data
  /// is confirmation value of type BOOLEAN. TRUE means comparison pass. FALSE
  /// means comparison fail.
  ///
  EfiBluetoothSmpNumericComparisonEvent,
} EFI_BLUETOOTH_LE_SMP_EVENT_DATA_TYPE;

/**
  The callback function for SMP.

  @param[in]  This                Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  Context             Data passed into callback function. This is optional parameter
                                  and may be NULL.
  @param[in]  BDAddr              Remote BluetoothLE device address.
  @param[in]  EventDataType       Event data type in EFI_BLUETOOTH_LE_SMP_EVENT_DATA_TYPE.
  @param[in]  DataSize            Indicates the size, in bytes, of the data buffer specified by Data.
  @param[in]  Data                A pointer to the buffer of data.

  @retval EFI_SUCCESS   The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_SMP_CALLBACK)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL       *This,
  IN VOID                                   *Context,
  IN BLUETOOTH_LE_ADDRESS                   *BDAddr,
  IN EFI_BLUETOOTH_LE_SMP_EVENT_DATA_TYPE   EventDataType,
  IN UINTN                                  DataSize,
  IN VOID                                   *Data
  );

/**
  Register Security Manager Protocol callback function for user authentication/authorization.

  The RegisterSmpAuthCallback() function register Security Manager Protocol callback
  function for user authentication/authorization.

  @param[in]  This            Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  Callback        Callback function for user authentication/authorization.
  @param[in]  Context         Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS         The SMP callback function is registered successfully.
  @retval EFI_ALREADY_STARTED A callback function is already registered on the same attribute
                              opcode and attribute handle, when the Callback is not NULL.
  @retval EFI_NOT_STARTED     A callback function is not registered on the same attribute opcode
                              and attribute handle, when the Callback is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_REGISTER_SMP_AUTH_CALLBACK)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL  *This,
  IN EFI_BLUETOOTH_LE_SMP_CALLBACK     Callback,
  IN VOID                              *Context
  );

/**
  Send user authentication/authorization to remote device.

  The SendSmpAuthData() function sends user authentication/authorization to remote device. It
  should be used to send these information after the caller gets the request data from the callback
  function by RegisterSmpAuthCallback().

  @param[in]  This            Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  BDAddr          Remote BluetoothLE device address.
  @param[in]  EventDataType   Event data type in EFI_BLUETOOTH_LE_SMP_EVENT_DATA_TYPE.
  @param[in]  DataSize        The size of Data in bytes, of the data buffer specified by Data.
  @param[in]  Data            A pointer to the buffer of data that will be sent. The data format
                              depends on the type of SMP event data being responded to.

  @retval EFI_SUCCESS         The SMP authorization data is sent successfully.
  @retval EFI_NOT_READY       SMP is not in the correct state to receive the auth data.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_SEND_SMP_AUTH_DATA)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL       *This,
  IN BLUETOOTH_LE_ADDRESS                   *BDAddr,
  IN EFI_BLUETOOTH_LE_SMP_EVENT_DATA_TYPE   EventDataType,
  IN UINTN                                  DataSize,
  IN VOID                                   *Data
  );

typedef enum {
  // For local device only
  EfiBluetoothSmpLocalIR,  /* If Key hierarchy is supported */
  EfiBluetoothSmpLocalER,  /* If Key hierarchy is supported */
  EfiBluetoothSmpLocalDHK, /* If Key hierarchy is supported. OPTIONAL */
  // For peer specific
  EfiBluetoothSmpKeysDistributed = 0x1000,
  EfiBluetoothSmpKeySize,
  EfiBluetoothSmpKeyType,
  EfiBluetoothSmpPeerLTK,
  EfiBluetoothSmpPeerIRK,
  EfiBluetoothSmpPeerCSRK,
  EfiBluetoothSmpPeerRand,
  EfiBluetoothSmpPeerEDIV,
  EfiBluetoothSmpPeerSignCounter,
  EfiBluetoothSmpLocalLTK,  /* If Key hierarchy not supported */
  EfiBluetoothSmpLocalIRK,  /* If Key hierarchy not supported */
  EfiBluetoothSmpLocalCSRK, /* If Key hierarchy not supported */
  EfiBluetoothSmpLocalSignCounter,
  EfiBluetoothSmpLocalDIV,
  EfiBluetoothSmpPeerAddressList,
  EfiBluetoothSmpMax,
} EFI_BLUETOOTH_LE_SMP_DATA_TYPE;

/**
  The callback function to get SMP data.

  @param[in]      This            Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]      Context         Data passed into callback function. This is optional parameter
                                  and may be NULL.
  @param[in]      BDAddr          Remote BluetoothLE device address. For Local device setting, it
                                  should be NULL.
  @param[in]      DataType        Data type in EFI_BLUETOOTH_LE_SMP_DATA_TYPE.
  @param[in, out] DataSize        On input, indicates the size, in bytes, of the data buffer specified
                                  by Data. On output, indicates the amount of data actually returned.
  @param[out]     Data            A pointer to the buffer of data that will be returned.

  @retval EFI_SUCCESS   The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_SMP_GET_DATA_CALLBACK)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL  *This,
  IN VOID                              *Context,
  IN BLUETOOTH_LE_ADDRESS              *BDAddr,
  IN EFI_BLUETOOTH_LE_SMP_DATA_TYPE    DataType,
  IN OUT UINTN                         *DataSize,
  OUT VOID                             *Data
  );

/**
  Register a callback function to get SMP related data.

  The RegisterSmpGetDataCallback() function registers a callback function to get SMP related data.

  @param[in]  This            Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  Callback        Callback function for SMP get data.
  @param[in]  Context         Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS         The SMP get data callback function is registered successfully.
  @retval EFI_ALREADY_STARTED A callback function is already registered on the same attribute
                              opcode and attribute handle, when the Callback is not NULL.
  @retval EFI_NOT_STARTED     A callback function is not registered on the same attribute opcode
                              and attribute handle, when the Callback is NULL
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_REGISTER_SMP_GET_DATA_CALLBACK)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL              *This,
  IN EFI_BLUETOOTH_LE_CONFIG_SMP_GET_DATA_CALLBACK Callback,
  IN VOID                                          *Context
  );

/**
  The callback function to set SMP data.

  @param[in]  This                Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  Context             Data passed into callback function. This is optional parameter
                                  and may be NULL.
  @param[in]  BDAddr              Remote BluetoothLE device address.
  @param[in]  DataType            Data type in EFI_BLUETOOTH_LE_SMP_DATA_TYPE.
  @param[in]  DataSize            Indicates the size, in bytes, of the data buffer specified by Data.
  @param[in]  Data                A pointer to the buffer of data.

  @retval EFI_SUCCESS   The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_SMP_SET_DATA_CALLBACK)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL  *This,
  IN VOID                              *Context,
  IN BLUETOOTH_LE_ADDRESS              *BDAddr,
  IN EFI_BLUETOOTH_LE_SMP_DATA_TYPE    Type,
  IN UINTN                             DataSize,
  IN VOID                              *Data
  );

/**
  Register a callback function to set SMP related data.

  The RegisterSmpSetDataCallback() function registers a callback function to set SMP related data.

  @param[in]  This            Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  Callback        Callback function for SMP set data.
  @param[in]  Context         Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS         The SMP set data callback function is registered successfully.
  @retval EFI_ALREADY_STARTED A callback function is already registered on the same attribute
                              opcode and attribute handle, when the Callback is not NULL.
  @retval EFI_NOT_STARTED     A callback function is not registered on the same attribute opcode
                              and attribute handle, when the Callback is NULL
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_REGISTER_SMP_SET_DATA_CALLBACK)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL              *This,
  IN EFI_BLUETOOTH_LE_CONFIG_SMP_SET_DATA_CALLBACK Callback,
  IN VOID                                          *Context
  );

/**
  The callback function to hook connect complete event.

  @param[in]  This                Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  Context             Data passed into callback function. This is optional parameter
                                  and may be NULL.
  @param[in]  CallbackType        The value defined in EFI_BLUETOOTH_CONNECT_COMPLETE_CALLBACK_TYPE.
  @param[in]  BDAddr              Remote BluetoothLE device address.
  @param[in]  InputBuffer         A pointer to the buffer of data that is input from callback caller.
  @param[in]  InputBufferSize     Indicates the size, in bytes, of the data buffer specified by InputBuffer.

  @retval EFI_SUCCESS   The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_CONNECT_COMPLETE_CALLBACK)(
  IN  EFI_BLUETOOTH_LE_CONFIG_PROTOCOL                 *This,
  IN  VOID                                             *Context,
  IN  EFI_BLUETOOTH_CONNECT_COMPLETE_CALLBACK_TYPE     CallbackType,
  IN  BLUETOOTH_LE_ADDRESS                             *BDAddr,
  IN  VOID                                             *InputBuffer,
  IN  UINTN                                            InputBufferSize
  );

/**
  Register link connect complete callback function.

  The RegisterLinkConnectCompleteCallback() function registers Bluetooth link connect
  complete callback function. The Bluetooth Configuration driver may call
  RegisterLinkConnectCompleteCallback() to register a callback function. During pairing,
  Bluetooth bus driver must trigger this callback function to report device state, if it is registered.
  Then Bluetooth Configuration driver will get information on device connection, according to
  CallbackType defined by EFI_BLUETOOTH_CONNECT_COMPLETE_CALLBACK_TYPE

  @param[in]  This            Pointer to the EFI_BLUETOOTH_LE_CONFIG_PROTOCOL instance.
  @param[in]  Callback        The callback function. NULL means unregister.
  @param[in]  Context         Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS         The link connect complete callback function is registered successfully.
  @retval EFI_ALREADY_STARTED A callback function is already registered on the same attribute
                              opcode and attribute handle, when the Callback is not NULL.
  @retval EFI_NOT_STARTED     A callback function is not registered on the same attribute opcode
                              and attribute handle, when the Callback is NULL
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_LE_CONFIG_REGISTER_CONNECT_COMPLETE_CALLBACK)(
  IN EFI_BLUETOOTH_LE_CONFIG_PROTOCOL                        *This,
  IN EFI_BLUETOOTH_LE_CONFIG_CONNECT_COMPLETE_CALLBACK       Callback,
  IN VOID                                                    *Context
  );

///
/// This protocol abstracts user interface configuration for BluetoothLe device.
///
struct _EFI_BLUETOOTH_LE_CONFIG_PROTOCOL {
  EFI_BLUETOOTH_LE_CONFIG_INIT                                  Init;
  EFI_BLUETOOTH_LE_CONFIG_SCAN                                  Scan;
  EFI_BLUETOOTH_LE_CONFIG_CONNECT                               Connect;
  EFI_BLUETOOTH_LE_CONFIG_DISCONNECT                            Disconnect;
  EFI_BLUETOOTH_LE_CONFIG_GET_DATA                              GetData;
  EFI_BLUETOOTH_LE_CONFIG_SET_DATA                              SetData;
  EFI_BLUETOOTH_LE_CONFIG_GET_REMOTE_DATA                       GetRemoteData;
  EFI_BLUETOOTH_LE_REGISTER_SMP_AUTH_CALLBACK                   RegisterSmpAuthCallback;
  EFI_BLUETOOTH_LE_SEND_SMP_AUTH_DATA                           SendSmpAuthData;
  EFI_BLUETOOTH_LE_CONFIG_REGISTER_SMP_GET_DATA_CALLBACK        RegisterSmpGetDataCallback;
  EFI_BLUETOOTH_LE_CONFIG_REGISTER_SMP_SET_DATA_CALLBACK        RegisterSmpSetDataCallback;
  EFI_BLUETOOTH_LE_CONFIG_REGISTER_CONNECT_COMPLETE_CALLBACK    RegisterLinkConnectCompleteCallback;
};

extern EFI_GUID  gEfiBluetoothLeConfigProtocolGuid;

#endif
