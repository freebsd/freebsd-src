/** @file
  SMM CPU I/O 2 protocol as defined in the PI 1.2 specification.

  This protocol provides CPU I/O and memory access within SMM.

  Copyright (c) 2009 - 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMM_CPU_IO2_H_
#define _SMM_CPU_IO2_H_

#include <Protocol/MmCpuIo.h>

#define EFI_SMM_CPU_IO2_PROTOCOL_GUID  EFI_MM_CPU_IO_PROTOCOL_GUID

typedef EFI_MM_CPU_IO_PROTOCOL EFI_SMM_CPU_IO2_PROTOCOL;

///
/// Width of the SMM CPU I/O operations
///
#define SMM_IO_UINT8   MM_IO_UINT8
#define SMM_IO_UINT16  MM_IO_UINT16
#define SMM_IO_UINT32  MM_IO_UINT32
#define SMM_IO_UINT64  MM_IO_UINT64

typedef EFI_MM_IO_WIDTH EFI_SMM_IO_WIDTH;
typedef EFI_MM_CPU_IO   EFI_SMM_CPU_IO2;

typedef EFI_MM_IO_ACCESS EFI_SMM_IO_ACCESS2;

extern EFI_GUID  gEfiSmmCpuIo2ProtocolGuid;

#endif
