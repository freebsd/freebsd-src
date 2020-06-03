/** @file
  The protocol provides information about the version of the EFI
  specification that a driver is following. This protocol is
  required for EFI drivers that are on PCI and other plug-in
  cards.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DRIVER_SUPPORTED_EFI_VERSION_H__
#define __DRIVER_SUPPORTED_EFI_VERSION_H__

#define EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL_GUID  \
  { 0x5c198761, 0x16a8, 0x4e69, { 0x97, 0x2c, 0x89, 0xd6, 0x79, 0x54, 0xf8, 0x1d } }


///
/// The EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL provides a
/// mechanism for an EFI driver to publish the version of the EFI
/// specification it conforms to. This protocol must be placed on
/// the driver's image handle when the driver's entry point is
/// called.
///
typedef struct _EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL {
  ///
  /// The size, in bytes, of the entire structure. Future versions of this
  /// specification may grow the size of the structure.
  ///
  UINT32 Length;
  ///
  /// The latest version of the UEFI specification that this driver conforms to.
  ///
  UINT32 FirmwareVersion;
} EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL;

extern EFI_GUID gEfiDriverSupportedEfiVersionProtocolGuid;

#endif
