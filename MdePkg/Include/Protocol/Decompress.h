/** @file
  The Decompress Protocol Interface as defined in UEFI spec

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DECOMPRESS_H__
#define __DECOMPRESS_H__

#define EFI_DECOMPRESS_PROTOCOL_GUID \
  { \
    0xd8117cfe, 0x94a6, 0x11d4, {0x9a, 0x3a, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

typedef struct _EFI_DECOMPRESS_PROTOCOL  EFI_DECOMPRESS_PROTOCOL;

/**
  The GetInfo() function retrieves the size of the uncompressed buffer
  and the temporary scratch buffer required to decompress the buffer
  specified by Source and SourceSize.  If the size of the uncompressed
  buffer or the size of the scratch buffer cannot be determined from
  the compressed data specified by Source and SourceData, then
  EFI_INVALID_PARAMETER is returned.  Otherwise, the size of the uncompressed
  buffer is returned in DestinationSize, the size of the scratch buffer is
  returned in ScratchSize, and EFI_SUCCESS is returned.

  The GetInfo() function does not have a scratch buffer available to perform
  a thorough checking of the validity of the source data. It just retrieves
  the 'Original Size' field from the beginning bytes of the source data and
  output it as DestinationSize.  And ScratchSize is specific to the decompression
  implementation.

  @param  This            A pointer to the EFI_DECOMPRESS_PROTOCOL instance.
  @param  Source          The source buffer containing the compressed data.
  @param  SourceSize      The size, in bytes, of source buffer.
  @param  DestinationSize A pointer to the size, in bytes, of the uncompressed buffer
                          that will be generated when the compressed buffer specified
                          by Source and SourceSize is decompressed.
  @param  ScratchSize     A pointer to the size, in bytes, of the scratch buffer that
                          is required to decompress the compressed buffer specified by
                          Source and SourceSize.

  @retval  EFI_SUCCESS           The size of the uncompressed data was returned in DestinationSize
                                 and the size of the scratch buffer was returned in ScratchSize.
  @retval  EFI_INVALID_PARAMETER The size of the uncompressed data or the size of the scratch
                                 buffer cannot be determined from the compressed data specified by
                                 Source and SourceData.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DECOMPRESS_GET_INFO)(
  IN EFI_DECOMPRESS_PROTOCOL            *This,
  IN   VOID                             *Source,
  IN   UINT32                           SourceSize,
  OUT  UINT32                           *DestinationSize,
  OUT  UINT32                           *ScratchSize
  );

/**
  The Decompress() function extracts decompressed data to its original form.

  This protocol is designed so that the decompression algorithm can be
  implemented without using any memory services.  As a result, the
  Decompress() function is not allowed to call AllocatePool() or
  AllocatePages() in its implementation.  It is the caller's responsibility
  to allocate and free the Destination and Scratch buffers.

  If the compressed source data specified by Source and SourceSize is
  successfully decompressed into Destination, then EFI_SUCCESS is returned.
  If the compressed source data specified by Source and SourceSize is not in
  a valid compressed data format, then EFI_INVALID_PARAMETER is returned.

  @param  This            A pointer to the EFI_DECOMPRESS_PROTOCOL instance.
  @param  Source          The source buffer containing the compressed data.
  @param  SourceSize      The size of source data.
  @param  Destination     On output, the destination buffer that contains
                          the uncompressed data.
  @param  DestinationSize The size of destination buffer. The size of destination
                          buffer needed is obtained from GetInfo().
  @param  Scratch         A temporary scratch buffer that is used to perform the
                          decompression.
  @param  ScratchSize     The size of scratch buffer. The size of scratch buffer needed
                          is obtained from GetInfo().

  @retval  EFI_SUCCESS          Decompression completed successfully, and the uncompressed
                                buffer is returned in Destination.
  @retval EFI_INVALID_PARAMETER The source buffer specified by Source and SourceSize is
                                corrupted (not in a valid compressed format).

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DECOMPRESS_DECOMPRESS)(
  IN EFI_DECOMPRESS_PROTOCOL              *This,
  IN     VOID                             *Source,
  IN     UINT32                           SourceSize,
  IN OUT VOID                             *Destination,
  IN     UINT32                           DestinationSize,
  IN OUT VOID                             *Scratch,
  IN     UINT32                           ScratchSize
  );

///
/// Provides a decompression service.
///
struct _EFI_DECOMPRESS_PROTOCOL {
  EFI_DECOMPRESS_GET_INFO   GetInfo;
  EFI_DECOMPRESS_DECOMPRESS Decompress;
};

extern EFI_GUID gEfiDecompressProtocolGuid;

#endif
