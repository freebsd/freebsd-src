/** @file
  I2C Master Protocol as defined in the PI 1.3 specification.

  This protocol manipulates the I2C host controller to perform transactions as a master
  on the I2C bus using the current state of any switches or multiplexers in the I2C bus.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This protocol is from PI Version 1.3.

**/

#ifndef __I2C_MASTER_H__
#define __I2C_MASTER_H__

#include <Pi/PiI2c.h>

#define EFI_I2C_MASTER_PROTOCOL_GUID  { 0xcd72881f, 0x45b5, 0x4feb, { 0x98, 0xc8, 0x31, 0x3d, 0xa8, 0x11, 0x74, 0x62 }}

typedef struct _EFI_I2C_MASTER_PROTOCOL EFI_I2C_MASTER_PROTOCOL;

/**
  Set the frequency for the I2C clock line.

  This routine must be called at or below TPL_NOTIFY.

  The software and controller do a best case effort of using the specified
  frequency for the I2C bus.  If the frequency does not match exactly then
  the I2C master protocol selects the next lower frequency to avoid
  exceeding the operating conditions for any of the I2C devices on the bus.
  For example if 400 KHz was specified and the controller's divide network
  only supports 402 KHz or 398 KHz then the I2C master protocol selects 398
  KHz.  If there are not lower frequencies available, then return
  EFI_UNSUPPORTED.

  @param[in] This           Pointer to an EFI_I2C_MASTER_PROTOCOL structure
  @param[in] BusClockHertz  Pointer to the requested I2C bus clock frequency
                            in Hertz.  Upon return this value contains the
                            actual frequency in use by the I2C controller.

  @retval EFI_SUCCESS           The bus frequency was set successfully.
  @retval EFI_ALREADY_STARTED   The controller is busy with another transaction.
  @retval EFI_INVALID_PARAMETER BusClockHertz is NULL
  @retval EFI_UNSUPPORTED       The controller does not support this frequency.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_I2C_MASTER_PROTOCOL_SET_BUS_FREQUENCY)(
  IN CONST EFI_I2C_MASTER_PROTOCOL   *This,
  IN OUT UINTN                       *BusClockHertz
  );

/**
  Reset the I2C controller and configure it for use

  This routine must be called at or below TPL_NOTIFY.

  The I2C controller is reset.  The caller must call SetBusFrequench() after
  calling Reset().

  @param[in]     This       Pointer to an EFI_I2C_MASTER_PROTOCOL structure.

  @retval EFI_SUCCESS         The reset completed successfully.
  @retval EFI_ALREADY_STARTED The controller is busy with another transaction.
  @retval EFI_DEVICE_ERROR    The reset operation failed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_I2C_MASTER_PROTOCOL_RESET)(
  IN CONST EFI_I2C_MASTER_PROTOCOL *This
  );

/**
  Start an I2C transaction on the host controller.

  This routine must be called at or below TPL_NOTIFY.  For synchronous
  requests this routine must be called at or below TPL_CALLBACK.

  This function initiates an I2C transaction on the controller.  To
  enable proper error handling by the I2C protocol stack, the I2C
  master protocol does not support queuing but instead only manages
  one I2C transaction at a time.  This API requires that the I2C bus
  is in the correct configuration for the I2C transaction.

  The transaction is performed by sending a start-bit and selecting the
  I2C device with the specified I2C slave address and then performing
  the specified I2C operations.  When multiple operations are requested
  they are separated with a repeated start bit and the slave address.
  The transaction is terminated with a stop bit.

  When Event is NULL, StartRequest operates synchronously and returns
  the I2C completion status as its return value.

  When Event is not NULL, StartRequest synchronously returns EFI_SUCCESS
  indicating that the I2C transaction was started asynchronously.  The
  transaction status value is returned in the buffer pointed to by
  I2cStatus upon the completion of the I2C transaction when I2cStatus
  is not NULL.  After the transaction status is returned the Event is
  signaled.

  Note: The typical consumer of this API is the I2C host protocol.
  Extreme care must be taken by other consumers of this API to prevent
  confusing the third party I2C drivers due to a state change at the
  I2C device which the third party I2C drivers did not initiate.  I2C
  platform specific code may use this API within these guidelines.

  @param[in] This           Pointer to an EFI_I2C_MASTER_PROTOCOL structure.
  @param[in] SlaveAddress   Address of the device on the I2C bus.  Set the
                            I2C_ADDRESSING_10_BIT when using 10-bit addresses,
                            clear this bit for 7-bit addressing.  Bits 0-6
                            are used for 7-bit I2C slave addresses and bits
                            0-9 are used for 10-bit I2C slave addresses.
  @param[in] RequestPacket  Pointer to an EFI_I2C_REQUEST_PACKET
                            structure describing the I2C transaction.
  @param[in] Event          Event to signal for asynchronous transactions,
                            NULL for asynchronous transactions
  @param[out] I2cStatus     Optional buffer to receive the I2C transaction
                            completion status

  @retval EFI_SUCCESS           The asynchronous transaction was successfully
                                started when Event is not NULL.
  @retval EFI_SUCCESS           The transaction completed successfully when
                                Event is NULL.
  @retval EFI_ALREADY_STARTED   The controller is busy with another transaction.
  @retval EFI_BAD_BUFFER_SIZE   The RequestPacket->LengthInBytes value is too
                                large.
  @retval EFI_DEVICE_ERROR      There was an I2C error (NACK) during the
                                transaction.
  @retval EFI_INVALID_PARAMETER RequestPacket is NULL
  @retval EFI_NOT_FOUND         Reserved bit set in the SlaveAddress parameter
  @retval EFI_NO_RESPONSE       The I2C device is not responding to the slave
                                address.  EFI_DEVICE_ERROR will be returned if
                                the controller cannot distinguish when the NACK
                                occurred.
  @retval EFI_OUT_OF_RESOURCES  Insufficient memory for I2C transaction
  @retval EFI_UNSUPPORTED       The controller does not support the requested
                                transaction.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_I2C_MASTER_PROTOCOL_START_REQUEST)(
  IN CONST EFI_I2C_MASTER_PROTOCOL *This,
  IN UINTN                         SlaveAddress,
  IN EFI_I2C_REQUEST_PACKET        *RequestPacket,
  IN EFI_EVENT                     Event      OPTIONAL,
  OUT EFI_STATUS                   *I2cStatus OPTIONAL
  );

///
/// I2C master mode protocol
///
/// This protocol manipulates the I2C host controller to perform transactions as a
/// master on the I2C bus using the current state of any switches or multiplexers
/// in the I2C bus.
///
struct _EFI_I2C_MASTER_PROTOCOL {
  ///
  /// Set the clock frequency for the I2C bus.
  ///
  EFI_I2C_MASTER_PROTOCOL_SET_BUS_FREQUENCY    SetBusFrequency;

  ///
  /// Reset the I2C host controller.
  ///
  EFI_I2C_MASTER_PROTOCOL_RESET                Reset;

  ///
  /// Start an I2C transaction in master mode on the host controller.
  ///
  EFI_I2C_MASTER_PROTOCOL_START_REQUEST        StartRequest;

  ///
  /// Pointer to an EFI_I2C_CONTROLLER_CAPABILITIES data structure containing
  /// the capabilities of the I2C host controller.
  ///
  CONST EFI_I2C_CONTROLLER_CAPABILITIES        *I2cControllerCapabilities;
};

extern EFI_GUID  gEfiI2cMasterProtocolGuid;

#endif //  __I2C_MASTER_H__
