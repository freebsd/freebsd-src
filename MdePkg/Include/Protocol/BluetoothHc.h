/** @file
  EFI Bluetooth Host Controller Protocol as defined in UEFI 2.5.
  This protocol abstracts the Bluetooth host controller layer message transmit and receive.

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

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

  The SendCommand() function sends HCI command packet. Buffer holds the whole HCI
  command packet, including OpCode, OCF, OGF, parameter length, and parameters. When
  this function is returned, it just means the HCI command packet is sent, it does not mean
  the command is success or complete. Caller might need to wait a command status event
  to know the command status, or wait a command complete event to know if the
  command is completed.

  @param[in]      This              Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param[in,out]  BufferSize        On input, indicates the size, in bytes, of the data buffer
                                    specified by Buffer. On output, indicates the amount of
                                    data actually transferred.
  @param[in]      Buffer            A pointer to the buffer of data that will be transmitted to
                                    Bluetooth host controller.
  @param[in]      Timeout           Indicating the transfer should be completed within this
                                    time frame. The units are in milliseconds. If Timeout is 0,
                                    then the caller must wait for the function to be completed
                                    until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS               The HCI command packet is sent successfully.
  @retval EFI_INVALID_PARAMETER     One or more of the following conditions is TRUE:
                                      BufferSize is NULL.
                                      *BufferSize is 0.
                                      Buffer is NULL.
  @retval EFI_TIMEOUT               Sending HCI command packet fail due to timeout.
  @retval EFI_DEVICE_ERROR          Sending HCI command packet fail due to host controller or device
                                    error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_SEND_COMMAND)(
  IN EFI_BLUETOOTH_HC_PROTOCOL      *This,
  IN OUT UINTN                      *BufferSize,
  IN VOID                           *Buffer,
  IN UINTN                          Timeout
  );

/**
  Receive HCI event packet.

  The ReceiveEvent() function receives HCI event packet. Buffer holds the whole HCI event
  packet, including EventCode, parameter length, and parameters.

  @param[in]      This              Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param[in,out]  BufferSize        On input, indicates the size, in bytes, of the data buffer
                                    specified by Buffer. On output, indicates the amount of
                                    data actually transferred.
  @param[out]     Buffer            A pointer to the buffer of data that will be received from
                                    Bluetooth host controller.
  @param[in]      Timeout           Indicating the transfer should be completed within this
                                    time frame. The units are in milliseconds. If Timeout is 0,
                                    then the caller must wait for the function to be completed
                                    until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS               The HCI event packet is received successfully.
  @retval EFI_INVALID_PARAMETER     One or more of the following conditions is TRUE:
                                      BufferSize is NULL.
                                      *BufferSize is 0.
                                      Buffer is NULL.
  @retval EFI_TIMEOUT               Receiving HCI event packet fail due to timeout.
  @retval EFI_DEVICE_ERROR          Receiving HCI event packet fail due to host controller or device
                                    error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_RECEIVE_EVENT)(
  IN EFI_BLUETOOTH_HC_PROTOCOL      *This,
  IN OUT UINTN                      *BufferSize,
  OUT VOID                          *Buffer,
  IN UINTN                          Timeout
  );

/**
  The async callback of AsyncReceiveEvent().

  @param[in]  Data                  Data received via asynchronous transfer.
  @param[in]  DataLength            The length of Data in bytes, received via asynchronous
                                    transfer.
  @param[in]  Context               Context passed from asynchronous transfer request.

  @retval EFI_SUCCESS               The callback does execute successfully.
  @retval Others                    The callback doesn't execute successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_ASYNC_FUNC_CALLBACK) (
  IN VOID                           *Data,
  IN UINTN                          DataLength,
  IN VOID                           *Context
  );

/**
  Receive HCI event packet in non-blocking way.

  The AsyncReceiveEvent() function receives HCI event packet in non-blocking way. Data
  in Callback function holds the whole HCI event packet, including EventCode, parameter
  length, and parameters.

  @param[in]  This                  Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param[in]  IsNewTransfer         If TRUE, a new transfer will be submitted. If FALSE, the
                                    request is deleted.
  @param[in]  PollingInterval       Indicates the periodic rate, in milliseconds, that the
                                    transfer is to be executed.
  @param[in]  DataLength            Specifies the length, in bytes, of the data to be received.
  @param[in]  Callback              The callback function. This function is called if the
                                    asynchronous transfer is completed.
  @param[in]  Context               Data passed into Callback function. This is optional
                                    parameter and may be NULL.

  @retval EFI_SUCCESS               The HCI asynchronous receive request is submitted successfully.
  @retval EFI_INVALID_PARAMETER     One or more of the following conditions is TRUE:
                                      DataLength is 0.
                                      If IsNewTransfer is TRUE, and an asynchronous receive
                                      request already exists.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_ASYNC_RECEIVE_EVENT) (
  IN EFI_BLUETOOTH_HC_PROTOCOL              *This,
  IN BOOLEAN                                IsNewTransfer,
  IN UINTN                                  PollingInterval,
  IN UINTN                                  DataLength,
  IN EFI_BLUETOOTH_HC_ASYNC_FUNC_CALLBACK   Callback,
  IN VOID                                   *Context
  );

/**
  Send HCI ACL data packet.

  The SendACLData() function sends HCI ACL data packet. Buffer holds the whole HCI ACL
  data packet, including Handle, PB flag, BC flag, data length, and data.

  The SendACLData() function and ReceiveACLData() function just send and receive data
  payload from application layer. In order to protect the payload data, the Bluetooth bus is
  required to call HCI_Set_Connection_Encryption command to enable hardware based
  encryption after authentication completed, according to pairing mode and host
  capability.

  @param[in]       This             Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param[in, out]  BufferSize       On input, indicates the size, in bytes, of the data buffer
                                    specified by Buffer. On output, indicates the amount of
                                    data actually transferred.
  @param[in]       Buffer           A pointer to the buffer of data that will be transmitted to
                                    Bluetooth host controller.
  @param[in]       Timeout          Indicating the transfer should be completed within this
                                    time frame. The units are in milliseconds. If Timeout is 0,
                                    then the caller must wait for the function to be completed
                                    until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS               The HCI ACL data packet is sent successfully.
  @retval EFI_INVALID_PARAMETER     One or more of the following conditions is TRUE:
                                      BufferSize is NULL.
                                      *BufferSize is 0.
                                      Buffer is NULL.
  @retval EFI_TIMEOUT               Sending HCI ACL data packet fail due to timeout.
  @retval EFI_DEVICE_ERROR          Sending HCI ACL data packet fail due to host controller or device
                                    error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_SEND_ACL_DATA)(
  IN EFI_BLUETOOTH_HC_PROTOCOL      *This,
  IN OUT UINTN                      *BufferSize,
  IN VOID                           *Buffer,
  IN UINTN                          Timeout
  );

/**
  Receive HCI ACL data packet.

  The ReceiveACLData() function receives HCI ACL data packet. Buffer holds the whole HCI
  ACL data packet, including Handle, PB flag, BC flag, data length, and data.

  @param[in]       This             Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param[in, out]  BufferSize       On input, indicates the size, in bytes, of the data buffer
                                    specified by Buffer. On output, indicates the amount of
                                    data actually transferred.
  @param[out]      Buffer           A pointer to the buffer of data that will be received from
                                    Bluetooth host controller.
  @param[in]       Timeout          Indicating the transfer should be completed within this
                                    time frame. The units are in milliseconds. If Timeout is 0,
                                    then the caller must wait for the function to be completed
                                    until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS               The HCI ACL data packet is received successfully.
  @retval EFI_INVALID_PARAMETER     One or more of the following conditions is TRUE:
                                      BufferSize is NULL.
                                      *BufferSize is 0.
                                      Buffer is NULL.
  @retval EFI_TIMEOUT               Receiving HCI ACL data packet fail due to timeout.
  @retval EFI_DEVICE_ERROR          Receiving HCI ACL data packet fail due to host controller or device
                                    error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_RECEIVE_ACL_DATA)(
  IN EFI_BLUETOOTH_HC_PROTOCOL      *This,
  IN OUT UINTN                      *BufferSize,
  OUT VOID                          *Buffer,
  IN UINTN                          Timeout
  );

/**
  Receive HCI ACL data packet in non-blocking way.

  The AsyncReceiveACLData() function receives HCI ACL data packet in non-blocking way.
  Data in Callback holds the whole HCI ACL data packet, including Handle, PB flag, BC flag,
  data length, and data.

  @param[in]  This                  Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param[in]  IsNewTransfer         If TRUE, a new transfer will be submitted. If FALSE, the
                                    request is deleted.
  @param[in]  PollingInterval       Indicates the periodic rate, in milliseconds, that the
                                    transfer is to be executed.
  @param[in]  DataLength            Specifies the length, in bytes, of the data to be received.
  @param[in]  Callback              The callback function. This function is called if the
                                    asynchronous transfer is completed.
  @param[in]  Context               Data passed into Callback function. This is optional
                                    parameter and may be NULL.

  @retval EFI_SUCCESS               The HCI asynchronous receive request is submitted successfully.
  @retval EFI_INVALID_PARAMETER     One or more of the following conditions is TRUE:
                                      DataLength is 0.
                                      If IsNewTransfer is TRUE, and an asynchronous receive
                                      request already exists.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_ASYNC_RECEIVE_ACL_DATA) (
  IN EFI_BLUETOOTH_HC_PROTOCOL              *This,
  IN BOOLEAN                                IsNewTransfer,
  IN UINTN                                  PollingInterval,
  IN UINTN                                  DataLength,
  IN EFI_BLUETOOTH_HC_ASYNC_FUNC_CALLBACK   Callback,
  IN VOID                                   *Context
  );

/**
  Send HCI SCO data packet.

  The SendSCOData() function sends HCI SCO data packet. Buffer holds the whole HCI SCO
  data packet, including ConnectionHandle, PacketStatus flag, data length, and data.

  @param[in]      This              Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param[in,out]  BufferSize        On input, indicates the size, in bytes, of the data buffer
                                    specified by Buffer. On output, indicates the amount of
                                    data actually transferred.
  @param[in]      Buffer            A pointer to the buffer of data that will be transmitted to
                                    Bluetooth host controller.
  @param[in]      Timeout           Indicating the transfer should be completed within this
                                    time frame. The units are in milliseconds. If Timeout is 0,
                                    then the caller must wait for the function to be completed
                                    until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS               The HCI SCO data packet is sent successfully.
  @retval EFI_UNSUPPORTED           The implementation does not support HCI SCO transfer.
  @retval EFI_INVALID_PARAMETER     One or more of the following conditions is TRUE:
                                      BufferSize is NULL.
                                      *BufferSize is 0.
                                      Buffer is NULL.
  @retval EFI_TIMEOUT               Sending HCI SCO data packet fail due to timeout.
  @retval EFI_DEVICE_ERROR          Sending HCI SCO data packet fail due to host controller or device
                                    error.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_SEND_SCO_DATA)(
  IN EFI_BLUETOOTH_HC_PROTOCOL      *This,
  IN OUT UINTN                      *BufferSize,
  IN VOID                           *Buffer,
  IN UINTN                          Timeout
  );

/**
  Receive HCI SCO data packet.

  The ReceiveSCOData() function receives HCI SCO data packet. Buffer holds the whole HCI
  SCO data packet, including ConnectionHandle, PacketStatus flag, data length, and data.

  @param[in]      This              Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param[in,out]  BufferSize        On input, indicates the size, in bytes, of the data buffer
                                    specified by Buffer. On output, indicates the amount of
                                    data actually transferred.
  @param[out]     Buffer            A pointer to the buffer of data that will be received from
                                    Bluetooth host controller.
  @param[in]      Timeout           Indicating the transfer should be completed within this
                                    time frame. The units are in milliseconds. If Timeout is 0,
                                    then the caller must wait for the function to be completed
                                    until EFI_SUCCESS or EFI_DEVICE_ERROR is returned.

  @retval EFI_SUCCESS               The HCI SCO data packet is received successfully.
  @retval EFI_INVALID_PARAMETER     One or more of the following conditions is TRUE:
                                      BufferSize is NULL.
                                      *BufferSize is 0.
                                      Buffer is NULL.
  @retval EFI_TIMEOUT               Receiving HCI SCO data packet fail due to timeout.
  @retval EFI_DEVICE_ERROR          Receiving HCI SCO data packet fail due to host controller or device
                                    error.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_RECEIVE_SCO_DATA)(
  IN EFI_BLUETOOTH_HC_PROTOCOL      *This,
  IN OUT UINTN                      *BufferSize,
  OUT VOID                          *Buffer,
  IN UINTN                          Timeout
  );

/**
  Receive HCI SCO data packet in non-blocking way.

  The AsyncReceiveSCOData() function receives HCI SCO data packet in non-blocking way.
  Data in Callback holds the whole HCI SCO data packet, including ConnectionHandle,
  PacketStatus flag, data length, and data.

  @param[in]  This                  Pointer to the EFI_BLUETOOTH_HC_PROTOCOL instance.
  @param[in]  IsNewTransfer         If TRUE, a new transfer will be submitted. If FALSE, the
                                    request is deleted.
  @param[in]  PollingInterval       Indicates the periodic rate, in milliseconds, that the
                                    transfer is to be executed.
  @param[in]  DataLength            Specifies the length, in bytes, of the data to be received.
  @param[in]  Callback              The callback function. This function is called if the
                                    asynchronous transfer is completed.
  @param[in]  Context               Data passed into Callback function. This is optional
                                    parameter and may be NULL.

  @retval EFI_SUCCESS               The HCI asynchronous receive request is submitted successfully.
  @retval EFI_INVALID_PARAMETER     One or more of the following conditions is TRUE:
                                      DataLength is 0.
                                      If IsNewTransfer is TRUE, and an asynchronous receive
                                      request already exists.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLUETOOTH_HC_ASYNC_RECEIVE_SCO_DATA) (
  IN EFI_BLUETOOTH_HC_PROTOCOL              *This,
  IN BOOLEAN                                IsNewTransfer,
  IN UINTN                                  PollingInterval,
  IN UINTN                                  DataLength,
  IN EFI_BLUETOOTH_HC_ASYNC_FUNC_CALLBACK   Callback,
  IN VOID                                   *Context
  );

//
// The EFI_BLUETOOTH_HC_PROTOCOL is used to transmit or receive HCI layer data packets.
//
struct _EFI_BLUETOOTH_HC_PROTOCOL {
  //
  // Send HCI command packet.
  //
  EFI_BLUETOOTH_HC_SEND_COMMAND               SendCommand;
  //
  // Receive HCI event packets.
  //
  EFI_BLUETOOTH_HC_RECEIVE_EVENT              ReceiveEvent;
  //
  // Non-blocking receive HCI event packets.
  //
  EFI_BLUETOOTH_HC_ASYNC_RECEIVE_EVENT        AsyncReceiveEvent;
  //
  // Send HCI ACL (asynchronous connection-oriented) data packets.
  //
  EFI_BLUETOOTH_HC_SEND_ACL_DATA              SendACLData;
  //
  // Receive HCI ACL data packets.
  //
  EFI_BLUETOOTH_HC_RECEIVE_ACL_DATA           ReceiveACLData;
  //
  // Non-blocking receive HCI ACL data packets.
  //
  EFI_BLUETOOTH_HC_ASYNC_RECEIVE_ACL_DATA     AsyncReceiveACLData;
  //
  // Send HCI synchronous (SCO and eSCO) data packets.
  //
  EFI_BLUETOOTH_HC_SEND_SCO_DATA              SendSCOData;
  //
  // Receive HCI synchronous data packets.
  //
  EFI_BLUETOOTH_HC_RECEIVE_SCO_DATA           ReceiveSCOData;
  //
  // Non-blocking receive HCI synchronous data packets.
  //
  EFI_BLUETOOTH_HC_ASYNC_RECEIVE_SCO_DATA     AsyncReceiveSCOData;
};

extern EFI_GUID gEfiBluetoothHcProtocolGuid;

#endif

