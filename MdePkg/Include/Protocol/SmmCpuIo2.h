/** @file
  SMM CPU I/O 2 protocol as defined in the PI 1.2 specification.

  This protocol provides CPU I/O and memory access within SMM.

  Copyright (c) 2009 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _SMM_CPU_IO2_H_
#define _SMM_CPU_IO2_H_

#define EFI_SMM_CPU_IO2_PROTOCOL_GUID \
  { \
    0x3242A9D8, 0xCE70, 0x4AA0, { 0x95, 0x5D, 0x5E, 0x7B, 0x14, 0x0D, 0xE4, 0xD2 } \
  }

typedef struct _EFI_SMM_CPU_IO2_PROTOCOL  EFI_SMM_CPU_IO2_PROTOCOL;

///
/// Width of the SMM CPU I/O operations
///
typedef enum {
  SMM_IO_UINT8  = 0,
  SMM_IO_UINT16 = 1,
  SMM_IO_UINT32 = 2,
  SMM_IO_UINT64 = 3
} EFI_SMM_IO_WIDTH;

/**
  Provides the basic memory and I/O interfaces used toabstract accesses to devices.

  The I/O operations are carried out exactly as requested.  The caller is 
  responsible for any alignment and I/O width issues that the bus, device, 
  platform, or type of I/O might require.

  @param[in]      This     The EFI_SMM_CPU_IO2_PROTOCOL instance.
  @param[in]      Width    Signifies the width of the I/O operations.
  @param[in]      Address  The base address of the I/O operations.  The caller is 
                           responsible for aligning the Address if required. 
  @param[in]      Count    The number of I/O operations to perform.
  @param[in,out]  Buffer   For read operations, the destination buffer to store 
                           the results.  For write operations, the source buffer 
                           from which to write data.

  @retval EFI_SUCCESS            The data was read from or written to the device.
  @retval EFI_UNSUPPORTED        The Address is not valid for this system.
  @retval EFI_INVALID_PARAMETER  Width or Count, or both, were invalid.
  @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a lack
                                 of resources.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_CPU_IO2)(
  IN     CONST EFI_SMM_CPU_IO2_PROTOCOL  *This,
  IN     EFI_SMM_IO_WIDTH                Width,
  IN     UINT64                          Address,
  IN     UINTN                           Count,
  IN OUT VOID                            *Buffer
  );

typedef struct {
  ///
  /// This service provides the various modalities of memory and I/O read.
  ///
  EFI_SMM_CPU_IO2  Read;
  ///
  /// This service provides the various modalities of memory and I/O write.
  ///
  EFI_SMM_CPU_IO2  Write;
} EFI_SMM_IO_ACCESS2;

///
/// SMM CPU I/O Protocol provides CPU I/O and memory access within SMM.
///
struct _EFI_SMM_CPU_IO2_PROTOCOL {
  ///
  /// Allows reads and writes to memory-mapped I/O space.
  ///
  EFI_SMM_IO_ACCESS2 Mem;
  ///
  /// Allows reads and writes to I/O space.
  ///
  EFI_SMM_IO_ACCESS2 Io;
};

extern EFI_GUID gEfiSmmCpuIo2ProtocolGuid;

#endif
