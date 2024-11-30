/** @file
  I/O Library. The implementations are based on EFI_PEI_SERVICE->CpuIo interface.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/PeiServicesTablePointerLib.h>

/**
  Reads registers in the EFI CPU I/O space.

  Reads the I/O port specified by Port with registers width specified by Width.
  The port is read Count times, and the read data is stored in the provided Buffer.

  This function must guarantee that all I/O read and write operations are serialized.
  If such operations are not supported, then ASSERT().

  @param  Port          The base address of the I/O operation.
                        The caller is responsible for aligning the Address if required.
  @param  Width         The width of the I/O operation.
  @param  Count         The number of times to read I/O port.
  @param  Buffer        The buffer to store the read data into.

**/
VOID
EFIAPI
IoReadFifoWorker (
  IN      UINTN                     Port,
  IN      EFI_PEI_CPU_IO_PPI_WIDTH  Width,
  IN      UINTN                     Count,
  IN      VOID                      *Buffer
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;
  EFI_STATUS              Status;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);

  Status = CpuIo->Io.Read (PeiServices, CpuIo, Width, Port, Count, Buffer);
  ASSERT_EFI_ERROR (Status);
}

/**
  Writes registers in the EFI CPU I/O space.

  Writes the I/O port specified by Port with registers width specified by Width.
  The port is written Count times, and the write data is retrieved from the provided Buffer.

  This function must guarantee that all I/O read and write operations are serialized.
  If such operations are not supported, then ASSERT().

  @param  Port          The base address of the I/O operation.
                        The caller is responsible for aligning the Address if required.
  @param  Width         The width of the I/O operation.
  @param  Count         The number of times to write I/O port.
  @param  Buffer        The buffer to store the read data into.

**/
VOID
EFIAPI
IoWriteFifoWorker (
  IN      UINTN                     Port,
  IN      EFI_PEI_CPU_IO_PPI_WIDTH  Width,
  IN      UINTN                     Count,
  IN      VOID                      *Buffer
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;
  EFI_STATUS              Status;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);

  Status = CpuIo->Io.Write (PeiServices, CpuIo, Width, Port, Count, Buffer);
  ASSERT_EFI_ERROR (Status);
}

/**
  Reads an 8-bit I/O port.

  Reads the 8-bit I/O port specified by Port. The 8-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT8
EFIAPI
IoRead8 (
  IN      UINTN  Port
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);

  return CpuIo->IoRead8 (PeiServices, CpuIo, (UINT64)Port);
}

/**
  Writes an 8-bit I/O port.

  Writes the 8-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT8
EFIAPI
IoWrite8 (
  IN      UINTN  Port,
  IN      UINT8  Value
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);

  CpuIo->IoWrite8 (PeiServices, CpuIo, (UINT64)Port, Value);
  return Value;
}

/**
  Reads a 16-bit I/O port.

  Reads the 16-bit I/O port specified by Port. The 16-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT16
EFIAPI
IoRead16 (
  IN      UINTN  Port
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Port is aligned on a 16-bit boundary.
  //
  ASSERT ((Port & 1) == 0);
  return CpuIo->IoRead16 (PeiServices, CpuIo, (UINT64)Port);
}

/**
  Writes a 16-bit I/O port.

  Writes the 16-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT16
EFIAPI
IoWrite16 (
  IN      UINTN   Port,
  IN      UINT16  Value
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Port is aligned on a 16-bit boundary.
  //
  ASSERT ((Port & 1) == 0);
  CpuIo->IoWrite16 (PeiServices, CpuIo, (UINT64)Port, Value);
  return Value;
}

/**
  Reads a 32-bit I/O port.

  Reads the 32-bit I/O port specified by Port. The 32-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT32
EFIAPI
IoRead32 (
  IN      UINTN  Port
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Port is aligned on a 32-bit boundary.
  //
  ASSERT ((Port & 3) == 0);
  return CpuIo->IoRead32 (PeiServices, CpuIo, (UINT64)Port);
}

/**
  Writes a 32-bit I/O port.

  Writes the 32-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT32
EFIAPI
IoWrite32 (
  IN      UINTN   Port,
  IN      UINT32  Value
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Port is aligned on a 32-bit boundary.
  //
  ASSERT ((Port & 3) == 0);
  CpuIo->IoWrite32 (PeiServices, CpuIo, (UINT64)Port, Value);
  return Value;
}

/**
  Reads a 64-bit I/O port.

  Reads the 64-bit I/O port specified by Port. The 64-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT64
EFIAPI
IoRead64 (
  IN      UINTN  Port
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Port is aligned on a 64-bit boundary.
  //
  ASSERT ((Port & 7) == 0);
  return CpuIo->IoRead64 (PeiServices, CpuIo, (UINT64)Port);
}

/**
  Writes a 64-bit I/O port.

  Writes the 64-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT64
EFIAPI
IoWrite64 (
  IN      UINTN   Port,
  IN      UINT64  Value
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Port is aligned on a 64-bit boundary.
  //
  ASSERT ((Port & 7) == 0);
  CpuIo->IoWrite64 (PeiServices, CpuIo, (UINT64)Port, Value);
  return Value;
}

/**
  Reads an 8-bit I/O port fifo into a block of memory.

  Reads the 8-bit I/O fifo port specified by Port.
  The port is read Count times, and the read data is
  stored in the provided Buffer.

  This function must guarantee that all I/O read and write operations are
  serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to read.
  @param  Count   The number of times to read I/O port.
  @param  Buffer  The buffer to store the read data into.

**/
VOID
EFIAPI
IoReadFifo8 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  OUT     VOID   *Buffer
  )
{
  IoReadFifoWorker (Port, EfiPeiCpuIoWidthFifoUint8, Count, Buffer);
}

/**
  Writes a block of memory into an 8-bit I/O port fifo.

  Writes the 8-bit I/O fifo port specified by Port.
  The port is written Count times, and the write data is
  retrieved from the provided Buffer.

  This function must guarantee that all I/O write and write operations are
  serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to write.
  @param  Count   The number of times to write I/O port.
  @param  Buffer  The buffer to retrieve the write data from.

**/
VOID
EFIAPI
IoWriteFifo8 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  IN      VOID   *Buffer
  )
{
  IoWriteFifoWorker (Port, EfiPeiCpuIoWidthFifoUint8, Count, Buffer);
}

/**
  Reads a 16-bit I/O port fifo into a block of memory.

  Reads the 16-bit I/O fifo port specified by Port.
  The port is read Count times, and the read data is
  stored in the provided Buffer.

  This function must guarantee that all I/O read and write operations are
  serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to read.
  @param  Count   The number of times to read I/O port.
  @param  Buffer  The buffer to store the read data into.

**/
VOID
EFIAPI
IoReadFifo16 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  OUT     VOID   *Buffer
  )
{
  //
  // Make sure Port is aligned on a 16-bit boundary.
  //
  ASSERT ((Port & 1) == 0);
  IoReadFifoWorker (Port, EfiPeiCpuIoWidthFifoUint16, Count, Buffer);
}

/**
  Writes a block of memory into a 16-bit I/O port fifo.

  Writes the 16-bit I/O fifo port specified by Port.
  The port is written Count times, and the write data is
  retrieved from the provided Buffer.

  This function must guarantee that all I/O write and write operations are
  serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to write.
  @param  Count   The number of times to write I/O port.
  @param  Buffer  The buffer to retrieve the write data from.

**/
VOID
EFIAPI
IoWriteFifo16 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  IN      VOID   *Buffer
  )
{
  //
  // Make sure Port is aligned on a 16-bit boundary.
  //
  ASSERT ((Port & 1) == 0);
  IoWriteFifoWorker (Port, EfiPeiCpuIoWidthFifoUint16, Count, Buffer);
}

/**
  Reads a 32-bit I/O port fifo into a block of memory.

  Reads the 32-bit I/O fifo port specified by Port.
  The port is read Count times, and the read data is
  stored in the provided Buffer.

  This function must guarantee that all I/O read and write operations are
  serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to read.
  @param  Count   The number of times to read I/O port.
  @param  Buffer  The buffer to store the read data into.

**/
VOID
EFIAPI
IoReadFifo32 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  OUT     VOID   *Buffer
  )
{
  //
  // Make sure Port is aligned on a 32-bit boundary.
  //
  ASSERT ((Port & 3) == 0);
  IoReadFifoWorker (Port, EfiPeiCpuIoWidthFifoUint32, Count, Buffer);
}

/**
  Writes a block of memory into a 32-bit I/O port fifo.

  Writes the 32-bit I/O fifo port specified by Port.
  The port is written Count times, and the write data is
  retrieved from the provided Buffer.

  This function must guarantee that all I/O write and write operations are
  serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to write.
  @param  Count   The number of times to write I/O port.
  @param  Buffer  The buffer to retrieve the write data from.

**/
VOID
EFIAPI
IoWriteFifo32 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  IN      VOID   *Buffer
  )
{
  //
  // Make sure Port is aligned on a 32-bit boundary.
  //
  ASSERT ((Port & 3) == 0);
  IoWriteFifoWorker (Port, EfiPeiCpuIoWidthFifoUint32, Count, Buffer);
}

/**
  Reads an 8-bit MMIO register.

  Reads the 8-bit MMIO register specified by Address. The 8-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT8
EFIAPI
MmioRead8 (
  IN      UINTN  Address
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);

  return CpuIo->MemRead8 (PeiServices, CpuIo, (UINT64)Address);
}

/**
  Writes an 8-bit MMIO register.

  Writes the 8-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

  @return Value.

**/
UINT8
EFIAPI
MmioWrite8 (
  IN      UINTN  Address,
  IN      UINT8  Value
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);

  CpuIo->MemWrite8 (PeiServices, CpuIo, (UINT64)Address, Value);
  return Value;
}

/**
  Reads a 16-bit MMIO register.

  Reads the 16-bit MMIO register specified by Address. The 16-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT16
EFIAPI
MmioRead16 (
  IN      UINTN  Address
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Address is aligned on a 16-bit boundary.
  //
  ASSERT ((Address & 1) == 0);
  return CpuIo->MemRead16 (PeiServices, CpuIo, (UINT64)Address);
}

/**
  Writes a 16-bit MMIO register.

  Writes the 16-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

  @return Value.

**/
UINT16
EFIAPI
MmioWrite16 (
  IN      UINTN   Address,
  IN      UINT16  Value
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Address is aligned on a 16-bit boundary.
  //
  ASSERT ((Address & 1) == 0);
  CpuIo->MemWrite16 (PeiServices, CpuIo, (UINT64)Address, Value);
  return Value;
}

/**
  Reads a 32-bit MMIO register.

  Reads the 32-bit MMIO register specified by Address. The 32-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT32
EFIAPI
MmioRead32 (
  IN      UINTN  Address
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Address is aligned on a 32-bit boundary.
  //
  ASSERT ((Address & 3) == 0);
  return CpuIo->MemRead32 (PeiServices, CpuIo, (UINT64)Address);
}

/**
  Writes a 32-bit MMIO register.

  Writes the 32-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

  @return Value.

**/
UINT32
EFIAPI
MmioWrite32 (
  IN      UINTN   Address,
  IN      UINT32  Value
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Address is aligned on a 32-bit boundary.
  //
  ASSERT ((Address & 3) == 0);
  CpuIo->MemWrite32 (PeiServices, CpuIo, (UINT64)Address, Value);
  return Value;
}

/**
  Reads a 64-bit MMIO register.

  Reads the 64-bit MMIO register specified by Address. The 64-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT64
EFIAPI
MmioRead64 (
  IN      UINTN  Address
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Address is aligned on a 64-bit boundary.
  //
  ASSERT ((Address & (sizeof (UINT64) - 1)) == 0);
  return CpuIo->MemRead64 (PeiServices, CpuIo, (UINT64)Address);
}

/**
  Writes a 64-bit MMIO register.

  Writes the 64-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

**/
UINT64
EFIAPI
MmioWrite64 (
  IN      UINTN   Address,
  IN      UINT64  Value
  )
{
  CONST EFI_PEI_SERVICES  **PeiServices;
  EFI_PEI_CPU_IO_PPI      *CpuIo;

  PeiServices = GetPeiServicesTablePointer ();
  CpuIo       = (*PeiServices)->CpuIo;
  ASSERT (CpuIo != NULL);
  //
  // Make sure Address is aligned on a 64-bit boundary.
  //
  ASSERT ((Address & 7) == 0);
  CpuIo->MemWrite64 (PeiServices, CpuIo, (UINT64)Address, Value);
  return Value;
}
