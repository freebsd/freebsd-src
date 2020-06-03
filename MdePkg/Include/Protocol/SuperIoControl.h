/** @file
  The Super I/O Control Protocol is installed by the Super I/O driver. It provides
  the low-level services for SIO devices that enable them to be used in the UEFI
  driver model.

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This protocol is from PI Version 1.2.1.

**/

#ifndef __EFI_SUPER_IO_CONTROL_PROTOCOL_H__
#define __EFI_SUPER_IO_CONTROL_PROTOCOL_H__

#define EFI_SIO_CONTROL_PROTOCOL_GUID \
  { \
    0xb91978df, 0x9fc1, 0x427d, { 0xbb, 0x5, 0x4c, 0x82, 0x84, 0x55, 0xca, 0x27 } \
  }

typedef struct _EFI_SIO_CONTROL_PROTOCOL EFI_SIO_CONTROL_PROTOCOL;
typedef struct _EFI_SIO_CONTROL_PROTOCOL *PEFI_SIO_CONTROL_PROTOCOL;

/**
  Enable an ISA-style device.

  This function enables a logical ISA device and, if necessary, configures it
  to default settings, including memory, I/O, DMA and IRQ resources.

  @param This A pointer to this instance of the EFI_SIO_CONTROL_PROTOCOL.

  @retval EFI_SUCCESS          The device is enabled successfully.
  @retval EFI_OUT_OF_RESOURCES The device could not be enabled because there
                               were insufficient resources either for the device
                               itself or for the records needed to track the device.
  @retval EFI_ALREADY_STARTED  The device is already enabled.
  @retval EFI_UNSUPPORTED      The device cannot be enabled.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SIO_CONTROL_ENABLE)(
  IN CONST EFI_SIO_CONTROL_PROTOCOL *This
  );

/**
  Disable a logical ISA device.

  This function disables a logical ISA device so that it no longer consumes
  system resources, such as memory, I/O, DMA and IRQ resources. Enough information
  must be available so that subsequent Enable() calls would properly reconfigure
  the device.

  @param This A pointer to this instance of the EFI_SIO_CONTROL_PROTOCOL.

  @retval EFI_SUCCESS          The device is disabled successfully.
  @retval EFI_OUT_OF_RESOURCES The device could not be disabled because there
                               were insufficient resources either for the device
                               itself or for the records needed to track the device.
  @retval EFI_ALREADY_STARTED  The device is already disabled.
  @retval EFI_UNSUPPORTED      The device cannot be disabled.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SIO_CONTROL_DISABLE)(
  IN CONST EFI_SIO_CONTROL_PROTOCOL *This
  );

struct _EFI_SIO_CONTROL_PROTOCOL {
  ///
  /// The version of this protocol.
  ///
  UINT32                  Version;
  ///
  /// Enable a device.
  ///
  EFI_SIO_CONTROL_ENABLE  EnableDevice;
  ///
  /// Disable a device.
  ///
  EFI_SIO_CONTROL_DISABLE DisableDevice;
};

extern EFI_GUID gEfiSioControlProtocolGuid;

#endif // __EFI_SUPER_IO_CONTROL_PROTOCOL_H__
