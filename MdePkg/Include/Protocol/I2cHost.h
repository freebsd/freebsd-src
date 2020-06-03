/** @file
  I2C Host Protocol as defined in the PI 1.3 specification.

  This protocol provides callers with the ability to do I/O transactions
  to all of the devices on the I2C bus.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This protocol is from PI Version 1.3.

**/

#ifndef __I2C_HOST_H__
#define __I2C_HOST_H__

#include <Pi/PiI2c.h>

#define EFI_I2C_HOST_PROTOCOL_GUID  { 0xa5aab9e3, 0xc727, 0x48cd, { 0x8b, 0xbf, 0x42, 0x72, 0x33, 0x85, 0x49, 0x48 }}

///
/// I2C Host Protocol
///
/// The I2C bus driver uses the services of the EFI_I2C_HOST_PROTOCOL
/// to produce an instance of the EFI_I2C_IO_PROTOCOL for each I2C
/// device on an I2C bus.
///
/// The EFI_I2C_HOST_PROTOCOL exposes an asynchronous interface to
/// callers to perform transactions to any device on the I2C bus.
/// Internally, the I2C host protocol manages the flow of the I2C
/// transactions to the host controller, keeping them in FIFO order.
/// Prior to each transaction, the I2C host protocol ensures that the
/// switches and multiplexers are properly configured.  The I2C host
/// protocol then starts the transaction on the host controller using
/// the EFI_I2C_MASTER_PROTOCOL.
///
typedef struct _EFI_I2C_HOST_PROTOCOL EFI_I2C_HOST_PROTOCOL;


/**
  Queue an I2C transaction for execution on the I2C controller.

  This routine must be called at or below TPL_NOTIFY.  For
  synchronous requests this routine must be called at or below
  TPL_CALLBACK.

  The I2C host protocol uses the concept of I2C bus configurations
  to describe the I2C bus.  An I2C bus configuration is defined as
  a unique setting of the multiplexers and switches in the I2C bus
  which enable access to one or more I2C devices.  When using a
  switch to divide a bus, due to bus frequency differences, the
  I2C bus configuration management protocol defines an I2C bus
  configuration for the I2C devices on each side of the switch.
  When using a multiplexer, the I2C bus configuration management
  defines an I2C bus configuration for each of the selector values
  required to control the multiplexer.  See Figure 1 in the I2C -bus
  specification and user manual for a complex I2C bus configuration.

  The I2C host protocol processes all transactions in FIFO order.
  Prior to performing the transaction, the I2C host protocol calls
  EnableI2cBusConfiguration to reconfigure the switches and
  multiplexers in the I2C bus enabling access to the specified I2C
  device.  The EnableI2cBusConfiguration also selects the I2C bus
  frequency for the I2C device.  After the I2C bus is configured,
  the I2C host protocol calls the I2C master protocol to start the
  I2C transaction.

  When Event is NULL, QueueRequest() operates synchronously and
  returns the I2C completion status as its return value.

  When Event is not NULL, QueueRequest() synchronously returns
  EFI_SUCCESS indicating that the asynchronously I2C transaction was
  queued.  The values above are returned in the buffer pointed to by
  I2cStatus upon the completion of the I2C transaction when I2cStatus
  is not NULL.

  @param[in] This             Pointer to an EFI_I2C_HOST_PROTOCOL structure.
  @param[in] I2cBusConfiguration  I2C bus configuration to access the I2C
                                  device
  @param[in] SlaveAddress     Address of the device on the I2C bus.  Set
                              the I2C_ADDRESSING_10_BIT when using 10-bit
                              addresses, clear this bit for 7-bit addressing.
                              Bits 0-6 are used for 7-bit I2C slave addresses
                              and bits 0-9 are used for 10-bit I2C slave
                              addresses.
  @param[in] Event            Event to signal for asynchronous transactions,
                              NULL for synchronous transactions
  @param[in] RequestPacket    Pointer to an EFI_I2C_REQUEST_PACKET structure
                              describing the I2C transaction
  @param[out] I2cStatus       Optional buffer to receive the I2C transaction
                              completion status

  @retval EFI_SUCCESS           The asynchronous transaction was successfully
                                queued when Event is not NULL.
  @retval EFI_SUCCESS           The transaction completed successfully when
                                Event is NULL.
  @retval EFI_BAD_BUFFER_SIZE   The RequestPacket->LengthInBytes value is
                                too large.
  @retval EFI_DEVICE_ERROR      There was an I2C error (NACK) during the
                                transaction.
  @retval EFI_INVALID_PARAMETER RequestPacket is NULL
  @retval EFI_NOT_FOUND         Reserved bit set in the SlaveAddress parameter
  @retval EFI_NO_MAPPING        Invalid I2cBusConfiguration value
  @retval EFI_NO_RESPONSE       The I2C device is not responding to the slave
                                address.  EFI_DEVICE_ERROR will be returned
                                if the controller cannot distinguish when the
                                NACK occurred.
  @retval EFI_OUT_OF_RESOURCES  Insufficient memory for I2C transaction
  @retval EFI_UNSUPPORTED       The controller does not support the requested
                                transaction.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_I2C_HOST_PROTOCOL_QUEUE_REQUEST) (
  IN CONST EFI_I2C_HOST_PROTOCOL *This,
  IN UINTN                       I2cBusConfiguration,
  IN UINTN                       SlaveAddress,
  IN EFI_EVENT                   Event      OPTIONAL,
  IN EFI_I2C_REQUEST_PACKET      *RequestPacket,
  OUT EFI_STATUS                 *I2cStatus OPTIONAL
  );

///
/// I2C Host Protocol
///
struct _EFI_I2C_HOST_PROTOCOL {
  ///
  /// Queue an I2C transaction for execution on the I2C bus
  ///
  EFI_I2C_HOST_PROTOCOL_QUEUE_REQUEST     QueueRequest;

  ///
  /// Pointer to an EFI_I2C_CONTROLLER_CAPABILITIES data structure
  /// containing the capabilities of the I2C host controller.
  ///
  CONST EFI_I2C_CONTROLLER_CAPABILITIES   *I2cControllerCapabilities;
};

///
/// Reference to variable defined in the .DEC file
///
extern EFI_GUID gEfiI2cHostProtocolGuid;

#endif  //  __I2C_HOST_H__
