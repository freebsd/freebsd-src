/** @file
  EFI Bluetooth Host Controller Protocol as defined in UEFI 2.5.
  This protocol abstracts the Bluetooth host controller layer message transmit and receive.

  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials are licensed and made available under 
  the terms and conditions of the BSD License that accompanies this distribution.  
  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.                                          
    
  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Revision Reference:          
  This Protocol is introduced in UEFI Specification 2.5

**/

#ifndef __EFI_BLUETOOTH_HC_PROTOCOL_H__
#define __EFI_BLUETOOTH_HC_PROTOCOL_H__

#define EFI_BLUETOOTH_HC_PROTOCOL_GUID \
  { \
    0xb3930571, 0xbeba, 0x4fc5, { 0x92, 0x3, 0x94, 0x27, 0x24, 0x2e, 0x6a, 0x43 } \
  }
  
typedef struct _EFI_BLUETOOTH_HC_PROTOCOL EFI_BLUETOOTH_HC_PROTOCOL;

/**
  Send HCI command packet.

  @param  This          Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param  BufferSize    On input, indicates the size, in bytes, of the data buffer specified by Buffer. 
                        On output, indicates the amount of data actually transferred.
  @param  Buffer        A pointer to the buffer of data that will be transmitted to Bluetooth host 
                        controller.
  @param  Timeout       Indicating the transfer should be completed within this time frame. The units are 
                        in milliseconds. If Timeout is 0, then the caller must wait for the function to 
                        be completed until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS           The HCI command packet is sent successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - BufferSize is NULL.
                                - *BufferSize is 0.
                                - Buffer is NULL.
  @retval EFI_TIMEOUT           Sending HCI command packet fail due to timeout.
  @retval EFI_DEVICE_ERROR      Sending HCI command packet fail due to host controller or device error.

**/
typedef 
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_SEND_COMMAND)(
  IN EFI_BLUETOOTH_HC_PROTOCOL  *This,
  IN OUT UINTN                  *BufferSize,
  IN VOID                       *Buffer,
  IN UINTN                      Timeout
  );
  

/**
  Receive HCI event packet.

  @param  This          Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param  BufferSize    On input, indicates the size, in bytes, of the data buffer specified by Buffer. 
                        On output, indicates the amount of data actually transferred.
  @param  Buffer        A pointer to the buffer of data that will be received from Bluetooth host controller.
  @param  Timeout       Indicating the transfer should be completed within this time frame. The units are 
                        in milliseconds. If Timeout is 0, then the caller must wait for the function to 
                        be completed until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS           The HCI event packet is received successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - BufferSize is NULL.
                                - *BufferSize is 0.
                                - Buffer is NULL.
  @retval EFI_TIMEOUT           Receiving HCI event packet fail due to timeout.
  @retval EFI_DEVICE_ERROR      Receiving HCI event packet fail due to host controller or device error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_RECEIVE_EVENT)(
  IN EFI_BLUETOOTH_HC_PROTOCOL  *This,
  IN OUT UINTN                  *BufferSize,
  OUT VOID                      *Buffer,
  IN UINTN                      Timeout
  );
  
/**
  Callback function, it is called when asynchronous transfer is completed.

  @param  Data              Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param  DataLength        Specifies the length, in bytes, of the data to be received.
  @param  Context           Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS             The callback function complete successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_ASYNC_FUNC_CALLBACK) (
  IN VOID                       *Data,
  IN UINTN                      DataLength,
  IN VOID                       *Context
  );
  
/**
  Receive HCI event packet in non-blocking way.

  @param  This              Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param  IsNewTransfer     If TRUE, a new transfer will be submitted. If FALSE, the request is deleted.
  @param  PollingInterval   Indicates the periodic rate, in milliseconds, that the transfer is to be executed.
  @param  DataLength        Specifies the length, in bytes, of the data to be received.
  @param  Callback          The callback function. This function is called if the asynchronous transfer is 
                            completed.
  @param  Context           Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS           The HCI asynchronous receive request is submitted successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - DataLength is 0.
                                - If IsNewTransfer is TRUE, and an asynchronous receive request already exists.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_ASYNC_RECEIVE_EVENT)(
  IN EFI_BLUETOOTH_HC_PROTOCOL            *This,
  IN BOOLEAN                              IsNewTransfer,
  IN UINTN                                PollingInterval,
  IN UINTN                                DataLength,
  IN EFI_BLUETOOTH_HC_ASYNC_FUNC_CALLBACK Callback,
  IN VOID                                 *Context
  );
  
/**
  Send HCI ACL data packet.

  @param  This          Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param  BufferSize    On input, indicates the size, in bytes, of the data buffer specified by Buffer. 
                        On output, indicates the amount of data actually transferred.
  @param  Buffer        A pointer to the buffer of data that will be transmitted to Bluetooth host 
                        controller.
  @param  Timeout       Indicating the transfer should be completed within this time frame. The units are 
                        in milliseconds. If Timeout is 0, then the caller must wait for the function to 
                        be completed until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS           The HCI ACL data packet is sent successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - BufferSize is NULL.
                                - *BufferSize is 0.
                                - Buffer is NULL.
  @retval EFI_TIMEOUT           Sending HCI ACL data packet fail due to timeout.
  @retval EFI_DEVICE_ERROR      Sending HCI ACL data packet fail due to host controller or device error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_SEND_ACL_DATA)(
  IN EFI_BLUETOOTH_HC_PROTOCOL  *This,
  IN OUT UINTN                  *BufferSize,
  IN VOID                       *Buffer,
  IN UINTN                      Timeout
  );
  
/**
  Receive HCI ACL data packet.

  @param  This          Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param  BufferSize    On input, indicates the size, in bytes, of the data buffer specified by Buffer. 
                        On output, indicates the amount of data actually transferred.
  @param  Buffer        A pointer to the buffer of data that will be received from Bluetooth host controller.
  @param  Timeout       Indicating the transfer should be completed within this time frame. The units are 
                        in milliseconds. If Timeout is 0, then the caller must wait for the function to 
                        be completed until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS           The HCI ACL data packet is received successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - BufferSize is NULL.
                                - *BufferSize is 0.
                                - Buffer is NULL.
  @retval EFI_TIMEOUT           Receiving HCI ACL data packet fail due to timeout.
  @retval EFI_DEVICE_ERROR      Receiving HCI ACL data packet fail due to host controller or device error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_RECEIVE_ACL_DATA)(
  IN EFI_BLUETOOTH_HC_PROTOCOL  *This,
  IN OUT UINTN                  *BufferSize,
  OUT VOID                      *Buffer,
  IN UINTN                      Timeout
  );
  

/**
  Receive HCI ACL data packet in non-blocking way.

  @param  This              Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param  IsNewTransfer     If TRUE, a new transfer will be submitted. If FALSE, the request is deleted.
  @param  PollingInterval   Indicates the periodic rate, in milliseconds, that the transfer is to be executed.
  @param  DataLength        Specifies the length, in bytes, of the data to be received.
  @param  Callback          The callback function. This function is called if the asynchronous transfer is 
                            completed.
  @param  Context           Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS           The HCI asynchronous receive request is submitted successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - DataLength is 0.
                                - If IsNewTransfer is TRUE, and an asynchronous receive request already exists.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_ASYNC_RECEIVE_ACL_DATA) (
  IN EFI_BLUETOOTH_HC_PROTOCOL            *This,
  IN BOOLEAN                              IsNewTransfer,
  IN UINTN                                PollingInterval,
  IN UINTN                                DataLength,
  IN EFI_BLUETOOTH_HC_ASYNC_FUNC_CALLBACK Callback,
  IN VOID                                 *Context
  );
  
/**
  Send HCI SCO data packet.

  @param  This          Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param  BufferSize    On input, indicates the size, in bytes, of the data buffer specified by Buffer. 
                        On output, indicates the amount of data actually transferred.
  @param  Buffer        A pointer to the buffer of data that will be transmitted to Bluetooth host 
                        controller.
  @param  Timeout       Indicating the transfer should be completed within this time frame. The units are 
                        in milliseconds. If Timeout is 0, then the caller must wait for the function to 
                        be completed until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS           The HCI SCO data packet is sent successfully.
  @retval EFI_UNSUPPORTED       The implementation does not support HCI SCO transfer.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - BufferSize is NULL.
                                - *BufferSize is 0.
                                - Buffer is NULL.
  @retval EFI_TIMEOUT           Sending HCI SCO data packet fail due to timeout.
  @retval EFI_DEVICE_ERROR      Sending HCI SCO data packet fail due to host controller or device error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_SEND_SCO_DATA)(
  IN EFI_BLUETOOTH_HC_PROTOCOL  *This,
  IN OUT UINTN                  *BufferSize,
  IN VOID                       *Buffer,
  IN UINTN                      Timeout
  );
  
/**
  Receive HCI SCO data packet.

  @param  This          Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param  BufferSize    On input, indicates the size, in bytes, of the data buffer specified by Buffer. 
                        On output, indicates the amount of data actually transferred.
  @param  Buffer        A pointer to the buffer of data that will be received from Bluetooth host controller.
  @param  Timeout       Indicating the transfer should be completed within this time frame. The units are 
                        in milliseconds. If Timeout is 0, then the caller must wait for the function to 
                        be completed until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS           The HCI SCO data packet is received successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - BufferSize is NULL.
                                - *BufferSize is 0.
                                - Buffer is NULL.
  @retval EFI_TIMEOUT           Receiving HCI SCO data packet fail due to timeout
  @retval EFI_DEVICE_ERROR      Receiving HCI SCO data packet fail due to host controller or device error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_RECEIVE_SCO_DATA)(
  IN EFI_BLUETOOTH_HC_PROTOCOL  *This,
  IN OUT UINTN                  *BufferSize,
  OUT VOID                      *Buffer,
  IN UINTN                      Timeout
  );

/**
  Receive HCI SCO data packet in non-blocking way.

  @param  This              Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param  IsNewTransfer     If TRUE, a new transfer will be submitted. If FALSE, the request is deleted.
  @param  PollingInterval   Indicates the periodic rate, in milliseconds, that the transfer is to be executed.
  @param  DataLength        Specifies the length, in bytes, of the data to be received.
  @param  Callback          The callback function. This function is called if the asynchronous transfer is 
                            completed.
  @param  Context           Data passed into Callback function. This is optional parameter and may be NULL.

  @retval EFI_SUCCESS           The HCI asynchronous receive request is submitted successfully.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - DataLength is 0.
                                - If IsNewTransfer is TRUE, and an asynchronous receive request already exists.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_ASYNC_RECEIVE_SCO_DATA) (
  IN EFI_BLUETOOTH_HC_PROTOCOL            *This,
  IN BOOLEAN                              IsNewTransfer,
  IN UINTN                                PollingInterval,
  IN UINTN                                DataLength,
  IN EFI_BLUETOOTH_HC_ASYNC_FUNC_CALLBACK Callback,
  IN VOID                                 *Context
  );
  
///
/// This protocol abstracts the Bluetooth host controller layer message transmit and receive.
///
struct _EFI_BLUETOOTH_HC_PROTOCOL {
  EFI_BLUETOOTH_HC_SEND_COMMAND               SendCommand;
  EFI_BLUETOOTH_HC_RECEIVE_EVENT              ReceiveEvent;
  EFI_BLUETOOTH_HC_ASYNC_RECEIVE_EVENT        AsyncReceiveEvent;
  EFI_BLUETOOTH_HC_SEND_ACL_DATA              SendACLData;
  EFI_BLUETOOTH_HC_RECEIVE_ACL_DATA           ReceiveACLData;
  EFI_BLUETOOTH_HC_ASYNC_RECEIVE_ACL_DATA     AsyncReceiveACLData;
  EFI_BLUETOOTH_HC_SEND_SCO_DATA              SendSCOData;
  EFI_BLUETOOTH_HC_RECEIVE_SCO_DATA           ReceiveSCOData;
  EFI_BLUETOOTH_HC_ASYNC_RECEIVE_SCO_DATA     AsyncReceiveSCOData;
};
  
extern EFI_GUID gEfiBluetoothHcProtocolGuid;

#endif
