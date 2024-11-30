/** @file
  This PPI opens or closes an I/O aperture in a ISA HOST controller.

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is from PI Version 1.2.1.

**/

#ifndef __ISA_HC_PPI_H__
#define __ISA_HC_PPI_H__

#define EFI_ISA_HC_PPI_GUID \
  { \
    0x8d48bd70, 0xc8a3, 0x4c06, {0x90, 0x1b, 0x74, 0x79, 0x46, 0xaa, 0xc3, 0x58} \
  }

typedef struct _EFI_ISA_HC_PPI  EFI_ISA_HC_PPI;
typedef struct _EFI_ISA_HC_PPI  *PEFI_ISA_HC_PPI;

/**
  Open I/O aperture.

  This function opens an I/O aperture in a ISA Host Controller for the I/O
  addresses specified by IoAddress to IoAddress + IoLength - 1. It is possible
  that more than one caller may be assigned to the same aperture.
  It may be possible that a single hardware aperture may be used for more than
  one device. This function tracks the number of times that each aperture is
  referenced, and does not close the hardware aperture (via CloseIoAperture())
  until there are no more references to it.

  @param This             A pointer to this instance of the EFI_ISA_HC_PPI.
  @param IoAddress        An unsigned integer that specifies the first byte of
                          the I/O space required.
  @param IoLength         An unsigned integer that specifies the number of
                          bytes of the I/O space required.
  @param IoApertureHandle A pointer to the returned I/O aperture handle.
                          This value can be used on subsequent calls to CloseIoAperture().

  @retval EFI_SUCCESS          The I/O aperture was opened successfully.
  @retval EFI_UNSUPPORTED      The ISA Host Controller is a subtractive-decode controller.
  @retval EFI_OUT_OF_RESOURCES There is no available I/O aperture.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_ISA_HC_OPEN_IO)(
  IN CONST EFI_ISA_HC_PPI   *This,
  IN UINT16                 IoAddress,
  IN UINT16                 IoLength,
  OUT UINT64                *IoApertureHandle
  );

/**
  Close I/O aperture.

  This function closes a previously opened I/O aperture handle. If there are no
  more I/O aperture handles that refer to the hardware I/O aperture resource,
  then the hardware I/O aperture is closed.
  It may be possible that a single hardware aperture may be used for more than
  one device. This function tracks the number of times that each aperture is
  referenced, and does not close the hardware aperture (via CloseIoAperture())
  until there are no more references to it.

  @param This             A pointer to this instance of the EFI_ISA_HC_PPI.
  @param IoApertureHandle The I/O aperture handle previously returned from a
                          call to OpenIoAperture().

  @retval EFI_SUCCESS   The I/O aperture was closed successfully.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_ISA_HC_CLOSE_IO)(
  IN CONST EFI_ISA_HC_PPI     *This,
  IN UINT64                   IoApertureHandle
  );

///
/// This PPI provides functions for opening or closing an I/O aperture.
///
struct _EFI_ISA_HC_PPI {
  ///
  /// An unsigned integer that specifies the version of the PPI structure.
  ///
  UINT32    Version;
  ///
  /// The address of the ISA/LPC Bridge device.
  /// For PCI, this is the segment, bus, device and function of the a ISA/LPC
  /// Bridge device.
  ///
  /// If bits 24-31 are 0, then the definition is:
  /// Bits 0:2   - Function
  /// Bits 3-7   - Device
  /// Bits 8:15  - Bus
  /// Bits 16-23 - Segment
  /// Bits 24-31 - Bus Type
  /// If bits 24-31 are 0xff, then the definition is platform-specific.
  ///
  UINT32                     Address;
  ///
  /// Opens an aperture on a positive-decode ISA Host Controller.
  ///
  EFI_PEI_ISA_HC_OPEN_IO     OpenIoAperture;
  ///
  /// Closes an aperture on a positive-decode ISA Host Controller.
  ///
  EFI_PEI_ISA_HC_CLOSE_IO    CloseIoAperture;
};

extern EFI_GUID  gEfiIsaHcPpiGuid;

#endif
