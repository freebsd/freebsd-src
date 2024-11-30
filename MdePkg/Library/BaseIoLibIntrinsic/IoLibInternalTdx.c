/** @file
  TDX I/O Library routines.

  Copyright (c) 2020-2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "BaseIoLibIntrinsicInternal.h"
#include <Uefi/UefiBaseType.h>
#include <Include/IndustryStandard/Tdx.h>
#include <Library/TdxLib.h>
#include <Register/Intel/Cpuid.h>
#include <Library/CcProbeLib.h>
#include "IoLibTdx.h"

// Size of TDVMCALL Access, including IO and MMIO
#define TDVMCALL_ACCESS_SIZE_1  1
#define TDVMCALL_ACCESS_SIZE_2  2
#define TDVMCALL_ACCESS_SIZE_4  4
#define TDVMCALL_ACCESS_SIZE_8  8

// Direction of TDVMCALL Access, including IO and MMIO
#define TDVMCALL_ACCESS_READ   0
#define TDVMCALL_ACCESS_WRITE  1

/**
  Check if it is Tdx guest.

  @return TRUE    It is Tdx guest
  @return FALSE   It is not Tdx guest

**/
BOOLEAN
EFIAPI
IsTdxGuest (
  VOID
  )
{
  return CcProbe () == CcGuestTypeIntelTdx;
}

/**
  Reads an 8-bit I/O port.

  TDVMCALL_IO is invoked to read I/O port.

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT8
EFIAPI
TdIoRead8 (
  IN      UINTN  Port
  )
{
  UINT64  Status;
  UINT64  Val;

  Status = TdVmCall (TDVMCALL_IO, TDVMCALL_ACCESS_SIZE_1, TDVMCALL_ACCESS_READ, Port, 0, &Val);
  if (Status != 0) {
    TdVmCall (TDVMCALL_HALT, 0, 0, 0, 0, 0);
  }

  return (UINT8)Val;
}

/**
  Reads a 16-bit I/O port.

  TDVMCALL_IO is invoked to write I/O port.

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT16
EFIAPI
TdIoRead16 (
  IN      UINTN  Port
  )
{
  UINT64  Status;
  UINT64  Val;

  ASSERT ((Port & 1) == 0);

  Status = TdVmCall (TDVMCALL_IO, TDVMCALL_ACCESS_SIZE_2, TDVMCALL_ACCESS_READ, Port, 0, &Val);
  if (Status != 0) {
    TdVmCall (TDVMCALL_HALT, 0, 0, 0, 0, 0);
  }

  return (UINT16)Val;
}

/**
  Reads a 32-bit I/O port.

  TDVMCALL_IO is invoked to read I/O port.

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT32
EFIAPI
TdIoRead32 (
  IN      UINTN  Port
  )
{
  UINT64  Status;
  UINT64  Val;

  ASSERT ((Port & 3) == 0);

  Status = TdVmCall (TDVMCALL_IO, TDVMCALL_ACCESS_SIZE_4, TDVMCALL_ACCESS_READ, Port, 0, &Val);
  if (Status != 0) {
    TdVmCall (TDVMCALL_HALT, 0, 0, 0, 0, 0);
  }

  return (UINT32)Val;
}

/**
  Writes an 8-bit I/O port.

  TDVMCALL_IO is invoked to write I/O port.

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT8
EFIAPI
TdIoWrite8 (
  IN      UINTN  Port,
  IN      UINT8  Value
  )
{
  UINT64  Status;
  UINT64  Val;

  Val    = Value;
  Status = TdVmCall (TDVMCALL_IO, TDVMCALL_ACCESS_SIZE_1, TDVMCALL_ACCESS_WRITE, Port, Val, 0);
  if (Status != 0) {
    TdVmCall (TDVMCALL_HALT, 0, 0, 0, 0, 0);
  }

  return Value;
}

/**
  Writes a 16-bit I/O port.

  TDVMCALL_IO is invoked to write I/O port.

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT16
EFIAPI
TdIoWrite16 (
  IN      UINTN   Port,
  IN      UINT16  Value
  )
{
  UINT64  Status;
  UINT64  Val;

  ASSERT ((Port & 1) == 0);
  Val    = Value;
  Status = TdVmCall (TDVMCALL_IO, TDVMCALL_ACCESS_SIZE_2, TDVMCALL_ACCESS_WRITE, Port, Val, 0);
  if (Status != 0) {
    TdVmCall (TDVMCALL_HALT, 0, 0, 0, 0, 0);
  }

  return Value;
}

/**
  Writes a 32-bit I/O port.

  TDVMCALL_IO is invoked to write I/O port.

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT32
EFIAPI
TdIoWrite32 (
  IN      UINTN   Port,
  IN      UINT32  Value
  )
{
  UINT64  Status;
  UINT64  Val;

  ASSERT ((Port & 3) == 0);
  Val    = Value;
  Status = TdVmCall (TDVMCALL_IO, TDVMCALL_ACCESS_SIZE_4, TDVMCALL_ACCESS_WRITE, Port, Val, 0);
  if (Status != 0) {
    TdVmCall (TDVMCALL_HALT, 0, 0, 0, 0, 0);
  }

  return Value;
}

/**
  Reads an 8-bit MMIO register.

  TDVMCALL_MMIO is invoked to read MMIO registers.

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT8
EFIAPI
TdMmioRead8 (
  IN      UINTN  Address
  )
{
  UINT64  Value;
  UINT64  Status;

  Status = TdVmCall (TDVMCALL_MMIO, TDVMCALL_ACCESS_SIZE_1, TDVMCALL_ACCESS_READ, Address | TdSharedPageMask (), 0, &Value);
  if (Status != 0) {
    Value = *(volatile UINT8 *)Address;
  }

  return (UINT8)Value;
}

/**
  Writes an 8-bit MMIO register.

  TDVMCALL_MMIO is invoked to read write registers.

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

  @return Value.

**/
UINT8
EFIAPI
TdMmioWrite8 (
  IN      UINTN  Address,
  IN      UINT8  Value
  )
{
  UINT64  Val;
  UINT64  Status;

  Val    = Value;
  Status = TdVmCall (TDVMCALL_MMIO, TDVMCALL_ACCESS_SIZE_1, TDVMCALL_ACCESS_WRITE, Address | TdSharedPageMask (), Val, 0);
  if (Status != 0) {
    *(volatile UINT8 *)Address = Value;
  }

  return Value;
}

/**
  Reads a 16-bit MMIO register.

  TDVMCALL_MMIO is invoked to read MMIO registers.

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT16
EFIAPI
TdMmioRead16 (
  IN      UINTN  Address
  )
{
  UINT64  Value;
  UINT64  Status;

  Status = TdVmCall (TDVMCALL_MMIO, TDVMCALL_ACCESS_SIZE_2, TDVMCALL_ACCESS_READ, Address | TdSharedPageMask (), 0, &Value);
  if (Status != 0) {
    Value = *(volatile UINT16 *)Address;
  }

  return (UINT16)Value;
}

/**
  Writes a 16-bit MMIO register.

  TDVMCALL_MMIO is invoked to write MMIO registers.

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

  @return Value.

**/
UINT16
EFIAPI
TdMmioWrite16 (
  IN      UINTN   Address,
  IN      UINT16  Value
  )
{
  UINT64  Val;
  UINT64  Status;

  ASSERT ((Address & 1) == 0);

  Val    = Value;
  Status = TdVmCall (TDVMCALL_MMIO, TDVMCALL_ACCESS_SIZE_2, TDVMCALL_ACCESS_WRITE, Address | TdSharedPageMask (), Val, 0);
  if (Status != 0) {
    *(volatile UINT16 *)Address = Value;
  }

  return Value;
}

/**
  Reads a 32-bit MMIO register.

  TDVMCALL_MMIO is invoked to read MMIO registers.

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT32
EFIAPI
TdMmioRead32 (
  IN      UINTN  Address
  )
{
  UINT64  Value;
  UINT64  Status;

  Status = TdVmCall (TDVMCALL_MMIO, TDVMCALL_ACCESS_SIZE_4, TDVMCALL_ACCESS_READ, Address | TdSharedPageMask (), 0, &Value);
  if (Status != 0) {
    Value = *(volatile UINT32 *)Address;
  }

  return (UINT32)Value;
}

/**
  Writes a 32-bit MMIO register.

  TDVMCALL_MMIO is invoked to write MMIO registers.

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

  @return Value.

**/
UINT32
EFIAPI
TdMmioWrite32 (
  IN      UINTN   Address,
  IN      UINT32  Value
  )
{
  UINT64  Val;
  UINT64  Status;

  ASSERT ((Address & 3) == 0);

  Val    = Value;
  Status = TdVmCall (TDVMCALL_MMIO, TDVMCALL_ACCESS_SIZE_4, TDVMCALL_ACCESS_WRITE, Address | TdSharedPageMask (), Val, 0);
  if (Status != 0) {
    *(volatile UINT32 *)Address = Value;
  }

  return Value;
}

/**
  Reads a 64-bit MMIO register.

  TDVMCALL_MMIO is invoked to read MMIO registers.

  @param  Address The MMIO register to read.

  @return The value read.

**/
UINT64
EFIAPI
TdMmioRead64 (
  IN      UINTN  Address
  )
{
  UINT64  Value;
  UINT64  Status;

  Status = TdVmCall (TDVMCALL_MMIO, TDVMCALL_ACCESS_SIZE_8, TDVMCALL_ACCESS_READ, Address | TdSharedPageMask (), 0, &Value);
  if (Status != 0) {
    Value = *(volatile UINT64 *)Address;
  }

  return Value;
}

/**
  Writes a 64-bit MMIO register.

  TDVMCALL_MMIO is invoked to write MMIO registers.

  @param  Address The MMIO register to write.
  @param  Value   The value to write to the MMIO register.

**/
UINT64
EFIAPI
TdMmioWrite64 (
  IN      UINTN   Address,
  IN      UINT64  Value
  )
{
  UINT64  Status;
  UINT64  Val;

  ASSERT ((Address & 7) == 0);

  Val    = Value;
  Status = TdVmCall (TDVMCALL_MMIO, TDVMCALL_ACCESS_SIZE_8, TDVMCALL_ACCESS_WRITE, Address | TdSharedPageMask (), Val, 0);
  if (Status != 0) {
    *(volatile UINT64 *)Address = Value;
  }

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

  In TDX a serial of TdIoRead8 is invoked to read the I/O port fifo.

  @param  Port    The I/O port to read.
  @param  Count   The number of times to read I/O port.
  @param  Buffer  The buffer to store the read data into.

**/
VOID
EFIAPI
TdIoReadFifo8 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  OUT     VOID   *Buffer
  )
{
  UINT8  *Buf8;
  UINTN  Index;

  Buf8 = (UINT8 *)Buffer;
  for (Index = 0; Index < Count; Index++) {
    Buf8[Index] = TdIoRead8 (Port);
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

  In TDX a serial of TdIoWrite8 is invoked to write data to the I/O port.

  @param  Port    The I/O port to write.
  @param  Count   The number of times to write I/O port.
  @param  Buffer  The buffer to retrieve the write data from.

**/
VOID
EFIAPI
TdIoWriteFifo8 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  IN      VOID   *Buffer
  )
{
  UINT8  *Buf8;
  UINTN  Index;

  Buf8 = (UINT8 *)Buffer;
  for (Index = 0; Index < Count; Index++) {
    TdIoWrite8 (Port, Buf8[Index]);
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

  In TDX a serial of TdIoRead16 is invoked to read data from the I/O port.

  @param  Port    The I/O port to read.
  @param  Count   The number of times to read I/O port.
  @param  Buffer  The buffer to store the read data into.

**/
VOID
EFIAPI
TdIoReadFifo16 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  OUT     VOID   *Buffer
  )
{
  UINT16  *Buf16;
  UINTN   Index;

  Buf16 = (UINT16 *)Buffer;
  for (Index = 0; Index < Count; Index++) {
    Buf16[Index] = TdIoRead16 (Port);
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

  In TDX a serial of TdIoWrite16 is invoked to write data to the I/O port.

  @param  Port    The I/O port to write.
  @param  Count   The number of times to write I/O port.
  @param  Buffer  The buffer to retrieve the write data from.

**/
VOID
EFIAPI
TdIoWriteFifo16 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  IN      VOID   *Buffer
  )
{
  UINT16  *Buf16;
  UINTN   Index;

  Buf16 = (UINT16 *)Buffer;
  for (Index = 0; Index < Count; Index++) {
    TdIoWrite16 (Port, Buf16[Index]);
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

  In TDX a serial of TdIoRead32 is invoked to read data from the I/O port.

  @param  Port    The I/O port to read.
  @param  Count   The number of times to read I/O port.
  @param  Buffer  The buffer to store the read data into.

**/
VOID
EFIAPI
TdIoReadFifo32 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  OUT     VOID   *Buffer
  )
{
  UINT32  *Buf32;
  UINTN   Index;

  Buf32 = (UINT32 *)Buffer;
  for (Index = 0; Index < Count; Index++) {
    Buf32[Index] = TdIoRead32 (Port);
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

  In TDX a serial of TdIoWrite32 is invoked to write data to the I/O port.

  @param  Port    The I/O port to write.
  @param  Count   The number of times to write I/O port.
  @param  Buffer  The buffer to retrieve the write data from.

**/
VOID
EFIAPI
TdIoWriteFifo32 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  IN      VOID   *Buffer
  )
{
  UINT32  *Buf32;
  UINTN   Index;

  Buf32 = (UINT32 *)Buffer;
  for (Index = 0; Index < Count; Index++) {
    TdIoWrite32 (Port, Buf32[Index]);
  }
}
