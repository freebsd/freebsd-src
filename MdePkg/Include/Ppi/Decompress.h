/** @file
  Provides decompression services to the PEI Foundation.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __DECOMPRESS_PPI_H__
#define __DECOMPRESS_PPI_H__

#define EFI_PEI_DECOMPRESS_PPI_GUID \
  { 0x1a36e4e7, 0xfab6, 0x476a, { 0x8e, 0x75, 0x69, 0x5a, 0x5, 0x76, 0xfd, 0xd7 } }

typedef struct _EFI_PEI_DECOMPRESS_PPI EFI_PEI_DECOMPRESS_PPI;

/**
  Decompress a single compression section in a firmware file.

  Decompresses the data in a compressed section and returns it
  as a series of standard PI Firmware File Sections. The
  required memory is allocated from permanent memory.

  @param This                   Points to this instance of the
                                EFI_PEI_DECOMPRESS_PEI PPI.
  @param InputSection           Points to the compressed section.
  @param OutputBuffer           Holds the returned pointer to the
                                decompressed sections.
  @param OutputSize             Holds the returned size of the decompress
                                section streams.

  @retval EFI_SUCCESS           The section was decompressed
                                successfully. OutputBuffer contains the
                                resulting data and OutputSize contains
                                the resulting size.
  @retval EFI_OUT_OF_RESOURCES  Unable to allocate sufficient
                                memory to hold the decompressed data.
  @retval EFI_UNSUPPORTED       The compression type specified
                                in the compression header is unsupported.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_DECOMPRESS_DECOMPRESS)(
  IN  CONST EFI_PEI_DECOMPRESS_PPI  *This,
  IN  CONST EFI_COMPRESSION_SECTION *InputSection,
  OUT VOID                           **OutputBuffer,
  OUT UINTN                          *OutputSize
  );

///
/// This PPI's single member function decompresses a compression
/// encapsulated section. It is used by the PEI Foundation to
/// process sectioned files. Prior to the installation of this PPI,
/// compression sections will be ignored.
///
struct _EFI_PEI_DECOMPRESS_PPI {
  EFI_PEI_DECOMPRESS_DECOMPRESS    Decompress;
};

extern EFI_GUID  gEfiPeiDecompressPpiGuid;

#endif
