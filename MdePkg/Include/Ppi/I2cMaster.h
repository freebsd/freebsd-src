/** @file
  This PPI manipulates the I2C host controller to perform transactions as a master
  on the I2C bus using the current state of any switches or multiplexers in the I2C bus.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.3.

**/

#ifndef __I2C_MASTER_PPI_H__
#define __I2C_MASTER_PPI_H__

#include <Pi/PiI2c.h>

#define EFI_PEI_I2C_MASTER_PPI_GUID \
  { 0xb3bfab9b, 0x9f9c, 0x4e8b, { 0xad, 0x37, 0x7f, 0x8c, 0x51, 0xfc, 0x62, 0x80 }}

typedef struct _EFI_PEI_I2C_MASTER_PPI EFI_PEI_I2C_MASTER_PPI;

/**
  Set the frequency for the I2C clock line.

  @param This                   Pointer to an EFI_PEI_I2C_MASTER_PPI structure.
  @param BusClockHertz          Pointer to the requested I2C bus clock frequency in Hertz.
                                Upon return this value contains the actual frequency
                                in use by the I2C controller.

  @retval EFI_SUCCESS           The bus frequency was set successfully.
  @retval EFI_INVALID_PARAMETER BusClockHertz is NULL
  @retval EFI_UNSUPPORTED       The controller does not support this frequency.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_I2C_MASTER_PPI_SET_BUS_FREQUENCY) (
  IN EFI_PEI_I2C_MASTER_PPI   *This,
  IN UINTN                    *BusClockHertz
  );

/**
  Reset the I2C controller and configure it for use.

  @param  This                  Pointer to an EFI_PEI_I2C_MASTER_PPI structure.

  @retval EFI_SUCCESS           The reset completed successfully.
  @retval EFI_DEVICE_ERROR      The reset operation failed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_I2C_MASTER_PPI_RESET) (
  IN CONST EFI_PEI_I2C_MASTER_PPI  *This
  );

/**
  Start an I2C transaction on the host controller.

  @param  This                   Pointer to an EFI_PEI_I2C_MASTER_PPI structure.
  @param  SlaveAddress           Address of the device on the I2C bus.
                                 Set the I2C_ADDRESSING_10_BIT when using 10-bit addresses,
                                 clear this bit for 7-bit addressing.
                                 Bits 0-6 are used for 7-bit I2C slave addresses and
                                 bits 0-9 are used for 10-bit I2C slave addresses.
  @param  RequestPacket          Pointer to an EFI_I2C_REQUEST_PACKET structure describing the I2C transaction.

  @retval EFI_SUCCESS             The transaction completed successfully.
  @retval EFI_BAD_BUFFER_SIZE     The RequestPacket->LengthInBytes value is too large.
  @retval EFI_DEVICE_ERROR       There was an I2C error (NACK) during the transaction.
  @retval EFI_INVALID_PARAMETER   RequestPacket is NULL
  @retval EFI_NO_RESPONSE         The I2C device is not responding to the slave address.
                                 EFI_DEVICE_ERROR will be returned if the controller cannot distinguish when the NACK occurred.
  @retval EFI_NOT_FOUND           Reserved bit set in the SlaveAddress parameter
  @retval EFI_OUT_OF_RESOURCES   Insufficient memory for I2C transaction
  @retval EFI_UNSUPPORTED         The controller does not support the requested transaction.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_I2C_MASTER_PPI_START_REQUEST) (
  IN CONST EFI_PEI_I2C_MASTER_PPI     *This,
  IN UINTN                            SlaveAddress,
  IN EFI_I2C_REQUEST_PACKET           *RequestPacket
  );

///
/// This PPI manipulates the I2C host controller to perform transactions as a master on the I2C bus
/// using the current state of any switches or multiplexers in the I2C bus.
///
struct _EFI_PEI_I2C_MASTER_PPI {
  EFI_PEI_I2C_MASTER_PPI_SET_BUS_FREQUENCY   SetBusFrequency;
  EFI_PEI_I2C_MASTER_PPI_RESET               Reset;
  EFI_PEI_I2C_MASTER_PPI_START_REQUEST       StartRequest;
  CONST EFI_I2C_CONTROLLER_CAPABILITIES      *I2cControllerCapabilities;
  EFI_GUID                                   Identifier;
};

extern EFI_GUID gEfiPeiI2cMasterPpiGuid;

#endif
