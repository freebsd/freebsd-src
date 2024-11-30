/** @file
  I2C Device Enumerate Protocol as defined in the PI 1.3 specification.

  This protocol supports the enumerations of device on the I2C bus.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This protocol is from PI Version 1.3.

**/

#ifndef __I2C_ENUMERATE_H__
#define __I2C_ENUMERATE_H__

#include <Pi/PiI2c.h>

#define EFI_I2C_ENUMERATE_PROTOCOL_GUID  { 0xda8cd7c4, 0x1c00, 0x49e2, { 0x80, 0x3e, 0x52, 0x14, 0xe7, 0x01, 0x89, 0x4c }}

typedef struct _EFI_I2C_ENUMERATE_PROTOCOL EFI_I2C_ENUMERATE_PROTOCOL;

/**
  Enumerate the I2C devices

  This function enables the caller to traverse the set of I2C devices
  on an I2C bus.

  @param[in]  This              The platform data for the next device on
                                the I2C bus was returned successfully.
  @param[in, out] Device        Pointer to a buffer containing an
                                EFI_I2C_DEVICE structure.  Enumeration is
                                started by setting the initial EFI_I2C_DEVICE
                                structure pointer to NULL.  The buffer
                                receives an EFI_I2C_DEVICE structure pointer
                                to the next I2C device.

  @retval EFI_SUCCESS           The platform data for the next device on
                                the I2C bus was returned successfully.
  @retval EFI_INVALID_PARAMETER Device is NULL
  @retval EFI_NO_MAPPING        *Device does not point to a valid
                                EFI_I2C_DEVICE structure returned in a
                                previous call Enumerate().

**/
typedef
EFI_STATUS
(EFIAPI *EFI_I2C_ENUMERATE_PROTOCOL_ENUMERATE)(
  IN CONST EFI_I2C_ENUMERATE_PROTOCOL *This,
  IN OUT CONST EFI_I2C_DEVICE         **Device
  );

/**
  Get the requested I2C bus frequency for a specified bus configuration.

  This function returns the requested I2C bus clock frequency for the
  I2cBusConfiguration.  This routine is provided for diagnostic purposes
  and is meant to be called after calling Enumerate to get the
  I2cBusConfiguration value.

  @param[in] This                 Pointer to an EFI_I2C_ENUMERATE_PROTOCOL
                                  structure.
  @param[in] I2cBusConfiguration  I2C bus configuration to access the I2C
                                  device
  @param[out] *BusClockHertz      Pointer to a buffer to receive the I2C
                                  bus clock frequency in Hertz

  @retval EFI_SUCCESS           The I2C bus frequency was returned
                                successfully.
  @retval EFI_INVALID_PARAMETER BusClockHertz was NULL
  @retval EFI_NO_MAPPING        Invalid I2cBusConfiguration value

**/
typedef
EFI_STATUS
(EFIAPI *EFI_I2C_ENUMERATE_PROTOCOL_GET_BUS_FREQUENCY)(
  IN CONST EFI_I2C_ENUMERATE_PROTOCOL *This,
  IN UINTN                            I2cBusConfiguration,
  OUT UINTN                           *BusClockHertz
  );

///
/// I2C Enumerate protocol
///
struct _EFI_I2C_ENUMERATE_PROTOCOL {
  ///
  /// Traverse the set of I2C devices on an I2C bus.  This routine
  /// returns the next I2C device on an I2C bus.
  ///
  EFI_I2C_ENUMERATE_PROTOCOL_ENUMERATE            Enumerate;

  ///
  /// Get the requested I2C bus frequency for a specified bus
  /// configuration.
  ///
  EFI_I2C_ENUMERATE_PROTOCOL_GET_BUS_FREQUENCY    GetBusFrequency;
};

///
/// Reference to variable defined in the .DEC file
///
extern EFI_GUID  gEfiI2cEnumerateProtocolGuid;

#endif //  __I2C_ENUMERATE_H__
