/** @file
  IoFifo read/write routines.

  Copyright (c) 2021 - 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseIoLibIntrinsicInternal.h"
#include "IoLibSev.h"
#include "IoLibTdx.h"
#include <Uefi/UefiBaseType.h>
#include <Library/TdxLib.h>

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
IoReadFifo8 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  OUT     VOID   *Buffer
  )
{
  if (IsTdxGuest ()) {
    TdIoReadFifo8 (Port, Count, Buffer);
  } else {
    SevIoReadFifo8 (Port, Count, Buffer);
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
IoWriteFifo8 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  IN      VOID   *Buffer
  )
{
  if (IsTdxGuest ()) {
    TdIoWriteFifo8 (Port, Count, Buffer);
  } else {
    SevIoWriteFifo8 (Port, Count, Buffer);
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
IoReadFifo16 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  OUT     VOID   *Buffer
  )
{
  if (IsTdxGuest ()) {
    TdIoReadFifo16 (Port, Count, Buffer);
  } else {
    SevIoReadFifo16 (Port, Count, Buffer);
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
IoWriteFifo16 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  IN      VOID   *Buffer
  )
{
  if (IsTdxGuest ()) {
    TdIoWriteFifo16 (Port, Count, Buffer);
  } else {
    SevIoWriteFifo16 (Port, Count, Buffer);
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
IoReadFifo32 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  OUT     VOID   *Buffer
  )
{
  if (IsTdxGuest ()) {
    TdIoReadFifo32 (Port, Count, Buffer);
  } else {
    SevIoReadFifo32 (Port, Count, Buffer);
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
IoWriteFifo32 (
  IN      UINTN  Port,
  IN      UINTN  Count,
  IN      VOID   *Buffer
  )
{
  if (IsTdxGuest ()) {
    TdIoWriteFifo32 (Port, Count, Buffer);
  } else {
    SevIoWriteFifo32 (Port, Count, Buffer);
  }
}
