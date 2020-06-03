/** @file
  Load image file from fv to memory.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __LOAD_FILE_PPI_H__
#define __LOAD_FILE_PPI_H__

#define EFI_PEI_LOAD_FILE_PPI_GUID \
  { 0xb9e0abfe, 0x5979, 0x4914, { 0x97, 0x7f, 0x6d, 0xee, 0x78, 0xc2, 0x78, 0xa6 } }


typedef struct _EFI_PEI_LOAD_FILE_PPI EFI_PEI_LOAD_FILE_PPI;

/**
  Loads a PEIM into memory for subsequent execution.

  This service is the single member function of EFI_LOAD_FILE_PPI.
  This service separates image loading and relocating from the PEI Foundation.

  @param This                   Interface pointer that implements
                                the Load File PPI instance.
  @param FileHandle             File handle of the file to load.
  @param ImageAddress           Pointer to the address of the loaded image.
  @param ImageSize              Pointer to the size of the loaded image.
  @param EntryPoint             Pointer to the entry point of the image.
  @param AuthenticationState    On exit, points to the attestation
                                authentication state of the image
                                or 0 if no attestation was performed.

  @retval EFI_SUCCESS           The image was loaded successfully.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory.
  @retval EFI_LOAD_ERROR        There was no supported image in the file.
  @retval EFI_INVALID_PARAMETER FileHandle was not a valid firmware file handle.
  @retval EFI_INVALID_PARAMETER EntryPoint was NULL.
  @retval EFI_UNSUPPORTED       An image requires relocations or is not
                                memory mapped.
  @retval EFI_WARN_BUFFER_TOO_SMALL
                                There is not enough heap to allocate the requested size.
                                This will not prevent the XIP image from being invoked.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_LOAD_FILE)(
  IN  CONST  EFI_PEI_LOAD_FILE_PPI   *This,
  IN  EFI_PEI_FILE_HANDLE            FileHandle,
  OUT EFI_PHYSICAL_ADDRESS           *ImageAddress,
  OUT UINT64                         *ImageSize,
  OUT EFI_PHYSICAL_ADDRESS           *EntryPoint,
  OUT UINT32                         *AuthenticationState
);

///
/// This PPI is a pointer to the Load File service.
/// This service will be published by a PEIM. The PEI Foundation
/// will use this service to launch the known PEI module images.
///
struct _EFI_PEI_LOAD_FILE_PPI {
  EFI_PEI_LOAD_FILE LoadFile;
};

extern EFI_GUID gEfiPeiLoadFilePpiGuid;

#endif
