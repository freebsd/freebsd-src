/** @file
  ISA HC Protocol as defined in the PI 1.2.1 specification.

  This protocol provides registration for ISA devices on a positive- or
  subtractive-decode ISA bus. It allows devices to be registered and also
  handles opening and closing the apertures which are positively-decoded.

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This protocol is from PI Version 1.2.1.

**/

#ifndef __ISA_HC_PROTOCOL_H__
#define __ISA_HC_PROTOCOL_H__

#define EFI_ISA_HC_PROTOCOL_GUID \
  { \
    0xbcdaf080, 0x1bde, 0x4e22, {0xae, 0x6a, 0x43, 0x54, 0x1e, 0x12, 0x8e, 0xc4} \
  }

#define EFI_ISA_HC_SERVICE_BINDING_PROTOCOL_GUID \
  { \
    0xfad7933a, 0x6c21, 0x4234, {0xa4, 0x34, 0x0a, 0x8a, 0x0d, 0x2b, 0x07, 0x81} \
  }

typedef struct _EFI_ISA_HC_PROTOCOL  EFI_ISA_HC_PROTOCOL;
typedef struct _EFI_ISA_HC_PROTOCOL  *PEFI_ISA_HC_PROTOCOL;

/**
  Open I/O aperture.

  This function opens an I/O aperture in a ISA Host Controller for the I/O addresses
  specified by IoAddress to IoAddress + IoLength - 1. It may be possible that a
  single hardware aperture may be used for more than one device. This function
  tracks the number of times that each aperture is referenced, and does not close
  the hardware aperture (via CloseIoAperture()) until there are no more references to it.

  @param This             A pointer to this instance of the EFI_ISA_HC_PROTOCOL.
  @param IoAddress        An unsigned integer that specifies the first byte of the
                          I/O space required.
  @param IoLength         An unsigned integer that specifies the number of bytes
                          of the I/O space required.
  @param IoApertureHandle A pointer to the returned I/O aperture handle. This
                          value can be used on subsequent calls to CloseIoAperture().

  @retval EFI_SUCCESS          The I/O aperture was opened successfully.
  @retval EFI_UNSUPPORTED      The ISA Host Controller is a subtractive-decode controller.
  @retval EFI_OUT_OF_RESOURCES There is no available I/O aperture.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_ISA_HC_OPEN_IO)(
  IN CONST EFI_ISA_HC_PROTOCOL  *This,
  IN UINT16                     IoAddress,
  IN UINT16                     IoLength,
  OUT UINT64                    *IoApertureHandle
  );

/**
  Close I/O aperture.

  This function closes a previously opened I/O aperture handle. If there are no
  more I/O aperture handles that refer to the hardware I/O aperture resource,
  then the hardware I/O aperture is closed. It may be possible that a single
  hardware aperture may be used for more than one device. This function tracks
  the number of times that each aperture is referenced, and does not close the
  hardware aperture (via CloseIoAperture()) until there are no more references to it.

  @param This             A pointer to this instance of the EFI_ISA_HC_PROTOCOL.
  @param IoApertureHandle The I/O aperture handle previously returned from a
                          call to OpenIoAperture().

  @retval EFI_SUCCESS     The IO aperture was closed successfully.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_ISA_HC_CLOSE_IO)(
  IN CONST EFI_ISA_HC_PROTOCOL      *This,
  IN UINT64                         IoApertureHandle
  );

///
/// ISA HC Protocol
///
struct _EFI_ISA_HC_PROTOCOL {
  ///
  /// The version of this protocol. Higher version numbers are backward
  /// compatible with lower version numbers.
  ///
  UINT32                 Version;
  ///
  /// Open an I/O aperture.
  ///
  EFI_ISA_HC_OPEN_IO     OpenIoAperture;
  ///
  /// Close an I/O aperture.
  ///
  EFI_ISA_HC_CLOSE_IO    CloseIoAperture;
};

///
/// Reference to variable defined in the .DEC file
///
extern EFI_GUID  gEfiIsaHcProtocolGuid;
extern EFI_GUID  gEfiIsaHcServiceBindingProtocolGuid;

#endif //  __ISA_HC_H__
