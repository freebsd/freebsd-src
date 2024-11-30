/** @file
  Internal include file of SMM CPU IO Library.
  It includes all necessary protocol/library class's header file
  for implementation of IoLib library instance. It is included
  all source code of this library instance.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMM_CPUIO_LIB_INTERNAL_H_
#define _SMM_CPUIO_LIB_INTERNAL_H_

#include <PiSmm.h>
#include <Protocol/SmmCpuIo2.h>
#include <Protocol/SmmPciRootBridgeIo.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/SmmServicesTableLib.h>

/**
  Reads registers in the EFI CPU I/O space.

  Reads the I/O port specified by Port with registers width specified by Width.
  The read value is returned. If such operations are not supported, then ASSERT().
  This function must guarantee that all I/O read and write operations are serialized.

  @param  Port          The base address of the I/O operation.
                        The caller is responsible for aligning the Address if required.
  @param  Width         The width of the I/O operation.

  @return Data read from registers in the EFI CPU I/O space.

**/
UINT64
EFIAPI
IoReadWorker (
  IN      UINTN             Port,
  IN      EFI_SMM_IO_WIDTH  Width
  );

/**
  Writes registers in the EFI CPU I/O space.

  Writes the I/O port specified by Port with registers width and value specified by Width
  and Data respectively.  Data is returned. If such operations are not supported, then ASSERT().
  This function must guarantee that all I/O read and write operations are serialized.

  @param  Port          The base address of the I/O operation.
                        The caller is responsible for aligning the Address if required.
  @param  Width         The width of the I/O operation.
  @param  Data          The value to write to the I/O port.

  @return The parameter of Data.

**/
UINT64
EFIAPI
IoWriteWorker (
  IN      UINTN             Port,
  IN      EFI_SMM_IO_WIDTH  Width,
  IN      UINT64            Data
  );

/**
  Reads memory-mapped registers in the EFI system memory space.

  Reads the MMIO registers specified by Address with registers width specified by Width.
  The read value is returned. If such operations are not supported, then ASSERT().
  This function must guarantee that all MMIO read and write operations are serialized.

  @param  Address       The MMIO register to read.
                        The caller is responsible for aligning the Address if required.
  @param  Width         The width of the I/O operation.

  @return Data read from registers in the EFI system memory space.

**/
UINT64
EFIAPI
MmioReadWorker (
  IN      UINTN             Address,
  IN      EFI_SMM_IO_WIDTH  Width
  );

/**
  Writes memory-mapped registers in the EFI system memory space.

  Writes the MMIO registers specified by Address with registers width and value specified by Width
  and Data respectively. Data is returned. If such operations are not supported, then ASSERT().
  This function must guarantee that all MMIO read and write operations are serialized.

  @param  Address       The MMIO register to read.
                        The caller is responsible for aligning the Address if required.
  @param  Width         The width of the I/O operation.
  @param  Data          The value to write to the I/O port.

  @return Data read from registers in the EFI system memory space.

**/
UINT64
EFIAPI
MmioWriteWorker (
  IN      UINTN             Address,
  IN      EFI_SMM_IO_WIDTH  Width,
  IN      UINT64            Data
  );

#endif
