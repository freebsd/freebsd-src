/** @file
  Provides services for SMM Memory Operation.

  The SMM Mem Library provides function for checking if buffer is outside SMRAM and valid.
  It also provides functions for copy data from SMRAM to non-SMRAM, from non-SMRAM to SMRAM,
  from non-SMRAM to non-SMRAM, or set data in non-SMRAM.

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMM_MEM_LIB_H_
#define _SMM_MEM_LIB_H_

/**
  This function check if the buffer is valid per processor architecture and not overlap with SMRAM.

  @param Buffer  The buffer start address to be checked.
  @param Length  The buffer length to be checked.

  @retval TRUE  This buffer is valid per processor architecture and not overlap with SMRAM.
  @retval FALSE This buffer is not valid per processor architecture or overlap with SMRAM.
**/
BOOLEAN
EFIAPI
SmmIsBufferOutsideSmmValid (
  IN EFI_PHYSICAL_ADDRESS  Buffer,
  IN UINT64                Length
  );

/**
  Copies a source buffer (non-SMRAM) to a destination buffer (SMRAM).

  This function copies a source buffer (non-SMRAM) to a destination buffer (SMRAM).
  It checks if source buffer is valid per processor architecture and not overlap with SMRAM.
  If the check passes, it copies memory and returns EFI_SUCCESS.
  If the check fails, it return EFI_SECURITY_VIOLATION.
  The implementation must be reentrant.

  @param  DestinationBuffer   The pointer to the destination buffer of the memory copy.
  @param  SourceBuffer        The pointer to the source buffer of the memory copy.
  @param  Length              The number of bytes to copy from SourceBuffer to DestinationBuffer.

  @retval EFI_SECURITY_VIOLATION The SourceBuffer is invalid per processor architecture or overlap with SMRAM.
  @retval EFI_SUCCESS            Memory is copied.

**/
EFI_STATUS
EFIAPI
SmmCopyMemToSmram (
  OUT VOID       *DestinationBuffer,
  IN CONST VOID  *SourceBuffer,
  IN UINTN       Length
  );

/**
  Copies a source buffer (SMRAM) to a destination buffer (NON-SMRAM).

  This function copies a source buffer (non-SMRAM) to a destination buffer (SMRAM).
  It checks if destination buffer is valid per processor architecture and not overlap with SMRAM.
  If the check passes, it copies memory and returns EFI_SUCCESS.
  If the check fails, it returns EFI_SECURITY_VIOLATION.
  The implementation must be reentrant.

  @param  DestinationBuffer   The pointer to the destination buffer of the memory copy.
  @param  SourceBuffer        The pointer to the source buffer of the memory copy.
  @param  Length              The number of bytes to copy from SourceBuffer to DestinationBuffer.

  @retval EFI_SECURITY_VIOLATION The DestinationBuffer is invalid per processor architecture or overlap with SMRAM.
  @retval EFI_SUCCESS            Memory is copied.

**/
EFI_STATUS
EFIAPI
SmmCopyMemFromSmram (
  OUT VOID       *DestinationBuffer,
  IN CONST VOID  *SourceBuffer,
  IN UINTN       Length
  );

/**
  Copies a source buffer (NON-SMRAM) to a destination buffer (NON-SMRAM).

  This function copies a source buffer (non-SMRAM) to a destination buffer (SMRAM).
  It checks if source buffer and destination buffer are valid per processor architecture and not overlap with SMRAM.
  If the check passes, it copies memory and returns EFI_SUCCESS.
  If the check fails, it returns EFI_SECURITY_VIOLATION.
  The implementation must be reentrant, and it must handle the case where source buffer overlaps destination buffer.

  @param  DestinationBuffer   The pointer to the destination buffer of the memory copy.
  @param  SourceBuffer        The pointer to the source buffer of the memory copy.
  @param  Length              The number of bytes to copy from SourceBuffer to DestinationBuffer.

  @retval EFI_SECURITY_VIOLATION The DestinationBuffer is invalid per processor architecture or overlap with SMRAM.
  @retval EFI_SECURITY_VIOLATION The SourceBuffer is invalid per processor architecture or overlap with SMRAM.
  @retval EFI_SUCCESS            Memory is copied.

**/
EFI_STATUS
EFIAPI
SmmCopyMem (
  OUT VOID       *DestinationBuffer,
  IN CONST VOID  *SourceBuffer,
  IN UINTN       Length
  );

/**
  Fills a target buffer (NON-SMRAM) with a byte value.

  This function fills a target buffer (non-SMRAM) with a byte value.
  It checks if target buffer is valid per processor architecture and not overlap with SMRAM.
  If the check passes, it fills memory and returns EFI_SUCCESS.
  If the check fails, it returns EFI_SECURITY_VIOLATION.

  @param  Buffer    The memory to set.
  @param  Length    The number of bytes to set.
  @param  Value     The value with which to fill Length bytes of Buffer.

  @retval EFI_SECURITY_VIOLATION The Buffer is invalid per processor architecture or overlap with SMRAM.
  @retval EFI_SUCCESS            Memory is set.

**/
EFI_STATUS
EFIAPI
SmmSetMem (
  OUT VOID  *Buffer,
  IN UINTN  Length,
  IN UINT8  Value
  );

#endif
