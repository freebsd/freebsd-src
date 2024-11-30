/** @file
  I/O Library.
  The implementation of I/O operation for this library instance
  are based on EFI_CPU_IO_PROTOCOL.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SmmCpuIoLibInternal.h"

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
  )
{
  EFI_STATUS  Status;
  UINT64      Data;

  Status = gSmst->SmmIo.Io.Read (&gSmst->SmmIo, Width, Port, 1, &Data);
  ASSERT_EFI_ERROR (Status);

  return Data;
}

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
  )
{
  EFI_STATUS  Status;

  Status = gSmst->SmmIo.Io.Write (&gSmst->SmmIo, Width, Port, 1, &Data);
  ASSERT_EFI_ERROR (Status);

  return Data;
}

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
  )
{
  EFI_STATUS  Status;
  UINT64      Data;

  Status = gSmst->SmmIo.Mem.Read (&gSmst->SmmIo, Width, Address, 1, &Data);
  ASSERT_EFI_ERROR (Status);

  return Data;
}

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
  )
{
  EFI_STATUS  Status;

  Status = gSmst->SmmIo.Mem.Write (&gSmst->SmmIo, Width, Address, 1, &Data);
  ASSERT_EFI_ERROR (Status);

  return Data;
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
  return (UINT8)IoReadWorker (Port, SMM_IO_UINT8);
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
  return (UINT8)IoWriteWorker (Port, SMM_IO_UINT8, Value);
}

/**
  Reads a 16-bit I/O port.

  Reads the 16-bit I/O port specified by Port. The 16-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If Port is not aligned on a 16-bit boundary, then ASSERT().

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT16
EFIAPI
IoRead16 (
  IN      UINTN  Port
  )
{
  //
  // Make sure Port is aligned on a 16-bit boundary.
  //
  ASSERT ((Port & 1) == 0);
  return (UINT16)IoReadWorker (Port, SMM_IO_UINT16);
}

/**
  Writes a 16-bit I/O port.

  Writes the 16-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If Port is not aligned on a 16-bit boundary, then ASSERT().

  If 16-bit I/O port operations are not supported, then ASSERT().

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
  //
  // Make sure Port is aligned on a 16-bit boundary.
  //
  ASSERT ((Port & 1) == 0);
  return (UINT16)IoWriteWorker (Port, SMM_IO_UINT16, Value);
}

/**
  Reads a 32-bit I/O port.

  Reads the 32-bit I/O port specified by Port. The 32-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If Port is not aligned on a 32-bit boundary, then ASSERT().

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT32
EFIAPI
IoRead32 (
  IN      UINTN  Port
  )
{
  //
  // Make sure Port is aligned on a 32-bit boundary.
  //
  ASSERT ((Port & 3) == 0);
  return (UINT32)IoReadWorker (Port, SMM_IO_UINT32);
}

/**
  Writes a 32-bit I/O port.

  Writes the 32-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If Port is not aligned on a 32-bit boundary, then ASSERT().

  If 32-bit I/O port operations are not supported, then ASSERT().

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
  //
  // Make sure Port is aligned on a 32-bit boundary.
  //
  ASSERT ((Port & 3) == 0);
  return (UINT32)IoWriteWorker (Port, SMM_IO_UINT32, Value);
}

/**
  Reads a 64-bit I/O port.

  Reads the 64-bit I/O port specified by Port. The 64-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If Port is not aligned on a 64-bit boundary, then ASSERT().

  If 64-bit I/O port operations are not supported, then ASSERT().

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT64
EFIAPI
IoRead64 (
  IN      UINTN  Port
  )
{
  //
  // Make sure Port is aligned on a 64-bit boundary.
  //
  ASSERT ((Port & 7) == 0);
  return IoReadWorker (Port, SMM_IO_UINT64);
}

/**
  Writes a 64-bit I/O port.

  Writes the 64-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If Port is not aligned on a 64-bit boundary, then ASSERT().

  If 64-bit I/O port operations are not supported, then ASSERT().

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
  //
  // Make sure Port is aligned on a 64-bit boundary.
  //
  ASSERT ((Port & 7) == 0);
  return IoWriteWorker (Port, SMM_IO_UINT64, Value);
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
  UINT8  *Buffer8;

  Buffer8 = (UINT8 *)Buffer;
  while (Count-- > 0) {
    *Buffer8++ = IoRead8 (Port);
  }
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
  UINT8  *Buffer8;

  Buffer8 = (UINT8 *)Buffer;
  while (Count-- > 0) {
    IoWrite8 (Port, *Buffer8++);
  }
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
  UINT16  *Buffer16;

  //
  // Make sure Port is aligned on a 16-bit boundary.
  //
  ASSERT ((Port & 1) == 0);
  Buffer16 = (UINT16 *)Buffer;
  while (Count-- > 0) {
    *Buffer16++ = IoRead16 (Port);
  }
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
  UINT16  *Buffer16;

  //
  // Make sure Port is aligned on a 16-bit boundary.
  //
  ASSERT ((Port & 1) == 0);
  Buffer16 = (UINT16 *)Buffer;
  while (Count-- > 0) {
    IoWrite16 (Port, *Buffer16++);
  }
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
  UINT32  *Buffer32;

  //
  // Make sure Port is aligned on a 32-bit boundary.
  //
  ASSERT ((Port & 3) == 0);
  Buffer32 = (UINT32 *)Buffer;
  while (Count-- > 0) {
    *Buffer32++ = IoRead32 (Port);
  }
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
  UINT32  *Buffer32;

  //
  // Make sure Port is aligned on a 32-bit boundary.
  //
  ASSERT ((Port & 3) == 0);
  Buffer32 = (UINT32 *)Buffer;
  while (Count-- > 0) {
    IoWrite32 (Port, *Buffer32++);
  }
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
  return (UINT8)MmioReadWorker (Address, SMM_IO_UINT8);
}

/**
  Writes an 8-bit MMIO register.

  Writes the 8-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

**/
UINT8
EFIAPI
MmioWrite8 (
  IN      UINTN  Address,
  IN      UINT8  Value
  )
{
  return (UINT8)MmioWriteWorker (Address, SMM_IO_UINT8, Value);
}

/**
  Reads a 16-bit MMIO register.

  Reads the 16-bit MMIO register specified by Address. The 16-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If Address is not aligned on a 16-bit boundary, then ASSERT().

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT16
EFIAPI
MmioRead16 (
  IN      UINTN  Address
  )
{
  //
  // Make sure Address is aligned on a 16-bit boundary.
  //
  ASSERT ((Address & 1) == 0);
  return (UINT16)MmioReadWorker (Address, SMM_IO_UINT16);
}

/**
  Writes a 16-bit MMIO register.

  Writes the 16-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If Address is not aligned on a 16-bit boundary, then ASSERT().

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

**/
UINT16
EFIAPI
MmioWrite16 (
  IN      UINTN   Address,
  IN      UINT16  Value
  )
{
  //
  // Make sure Address is aligned on a 16-bit boundary.
  //
  ASSERT ((Address & 1) == 0);
  return (UINT16)MmioWriteWorker (Address, SMM_IO_UINT16, Value);
}

/**
  Reads a 32-bit MMIO register.

  Reads the 32-bit MMIO register specified by Address. The 32-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If Address is not aligned on a 32-bit boundary, then ASSERT().

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT32
EFIAPI
MmioRead32 (
  IN      UINTN  Address
  )
{
  //
  // Make sure Address is aligned on a 32-bit boundary.
  //
  ASSERT ((Address & 3) == 0);
  return (UINT32)MmioReadWorker (Address, SMM_IO_UINT32);
}

/**
  Writes a 32-bit MMIO register.

  Writes the 32-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If Address is not aligned on a 32-bit boundary, then ASSERT().

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

**/
UINT32
EFIAPI
MmioWrite32 (
  IN      UINTN   Address,
  IN      UINT32  Value
  )
{
  //
  // Make sure Address is aligned on a 32-bit boundary.
  //
  ASSERT ((Address & 3) == 0);
  return (UINT32)MmioWriteWorker (Address, SMM_IO_UINT32, Value);
}

/**
  Reads a 64-bit MMIO register.

  Reads the 64-bit MMIO register specified by Address. The 64-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If Address is not aligned on a 64-bit boundary, then ASSERT().

  If 64-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT64
EFIAPI
MmioRead64 (
  IN      UINTN  Address
  )
{
  //
  // Make sure Address is aligned on a 64-bit boundary.
  //
  ASSERT ((Address & 7) == 0);
  return (UINT64)MmioReadWorker (Address, SMM_IO_UINT64);
}

/**
  Writes a 64-bit MMIO register.

  Writes the 64-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If Address is not aligned on a 64-bit boundary, then ASSERT().

  If 64-bit MMIO register operations are not supported, then ASSERT().

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
  //
  // Make sure Address is aligned on a 64-bit boundary.
  //
  ASSERT ((Address & 7) == 0);
  return (UINT64)MmioWriteWorker (Address, SMM_IO_UINT64, Value);
}
