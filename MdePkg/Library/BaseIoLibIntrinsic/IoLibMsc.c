/** @file
  I/O Library. This file has compiler specifics for Microsft C as there is no
  ANSI C standard for doing IO.

  MSC - uses intrinsic functions and the optimize will remove the function call
  overhead.

  We don't advocate putting compiler specifics in libraries or drivers but there
  is no other way to make this work.

  Copyright (c) 2006 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseIoLibIntrinsicInternal.h"
#include "IoLibTdx.h"

//
// Microsoft Visual Studio 7.1 Function Prototypes for I/O Intrinsics.
//

int
_inp (
  unsigned short  port
  );

unsigned short
_inpw (
  unsigned short  port
  );

unsigned long
_inpd (
  unsigned short  port
  );

int
_outp (
  unsigned short  port,
  int             databyte
  );

unsigned short
_outpw (
  unsigned short  port,
  unsigned short  dataword
  );

unsigned long
_outpd (
  unsigned short  port,
  unsigned long   dataword
  );

void
_ReadWriteBarrier (
  void
  );

#pragma intrinsic(_inp)
#pragma intrinsic(_inpw)
#pragma intrinsic(_inpd)
#pragma intrinsic(_outp)
#pragma intrinsic(_outpw)
#pragma intrinsic(_outpd)
#pragma intrinsic(_ReadWriteBarrier)

//
// _ReadWriteBarrier() forces memory reads and writes to complete at the point
// in the call. This is only a hint to the compiler and does emit code.
// In past versions of the compiler, _ReadWriteBarrier was enforced only
// locally and did not affect functions up the call tree. In Visual C++
// 2005, _ReadWriteBarrier is enforced all the way up the call tree.
//

/**
  Reads an 8-bit I/O port.

  Reads the 8-bit I/O port specified by Port. The 8-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  For Td guest TDVMCALL_IO is invoked to read I/O port.

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT8
EFIAPI
IoRead8 (
  IN      UINTN  Port
  )
{
  UINT8    Value;
  BOOLEAN  Flag;

  Flag = FilterBeforeIoRead (FilterWidth8, Port, &Value);
  if (Flag) {
    if (IsTdxGuest ()) {
      Value = TdIoRead8 (Port);
    } else {
      _ReadWriteBarrier ();
      Value = (UINT8)_inp ((UINT16)Port);
      _ReadWriteBarrier ();
    }
  }

  FilterAfterIoRead (FilterWidth8, Port, &Value);

  return Value;
}

/**
  Writes an 8-bit I/O port.

  Writes the 8-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  For Td guest TDVMCALL_IO is invoked to write I/O port.

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written to the I/O port.

**/
UINT8
EFIAPI
IoWrite8 (
  IN      UINTN  Port,
  IN      UINT8  Value
  )
{
  BOOLEAN  Flag;

  Flag = FilterBeforeIoWrite (FilterWidth8, Port, &Value);
  if (Flag) {
    if (IsTdxGuest ()) {
      TdIoWrite8 (Port, Value);
    } else {
      _ReadWriteBarrier ();
      (UINT8)_outp ((UINT16)Port, Value);
      _ReadWriteBarrier ();
    }
  }

  FilterAfterIoWrite (FilterWidth8, Port, &Value);

  return Value;
}

/**
  Reads a 16-bit I/O port.

  Reads the 16-bit I/O port specified by Port. The 16-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().

  For Td guest TDVMCALL_IO is invoked to read I/O port.

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT16
EFIAPI
IoRead16 (
  IN      UINTN  Port
  )
{
  UINT16   Value;
  BOOLEAN  Flag;

  ASSERT ((Port & 1) == 0);

  Flag = FilterBeforeIoRead (FilterWidth16, Port, &Value);
  if (Flag) {
    if (IsTdxGuest ()) {
      Value = TdIoRead16 (Port);
    } else {
      _ReadWriteBarrier ();
      Value = _inpw ((UINT16)Port);
      _ReadWriteBarrier ();
    }
  }

  FilterBeforeIoRead (FilterWidth16, Port, &Value);

  return Value;
}

/**
  Writes a 16-bit I/O port.

  Writes the 16-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().

  For Td guest TDVMCALL_IO is invoked to write I/O port.

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written to the I/O port.

**/
UINT16
EFIAPI
IoWrite16 (
  IN      UINTN   Port,
  IN      UINT16  Value
  )
{
  BOOLEAN  Flag;

  ASSERT ((Port & 1) == 0);

  Flag = FilterBeforeIoWrite (FilterWidth16, Port, &Value);
  if (Flag) {
    if (IsTdxGuest ()) {
      TdIoWrite16 (Port, Value);
    } else {
      _ReadWriteBarrier ();
      _outpw ((UINT16)Port, Value);
      _ReadWriteBarrier ();
    }
  }

  FilterAfterIoWrite (FilterWidth16, Port, &Value);

  return Value;
}

/**
  Reads a 32-bit I/O port.

  Reads the 32-bit I/O port specified by Port. The 32-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().

  For Td guest TDVMCALL_IO is invoked to read I/O port.

  @param  Port  The I/O port to read.

  @return The value read.

**/
UINT32
EFIAPI
IoRead32 (
  IN      UINTN  Port
  )
{
  UINT32   Value;
  BOOLEAN  Flag;

  ASSERT ((Port & 3) == 0);

  Flag = FilterBeforeIoRead (FilterWidth32, Port, &Value);
  if (Flag) {
    if (IsTdxGuest ()) {
      Value = TdIoRead32 (Port);
    } else {
      _ReadWriteBarrier ();
      Value = _inpd ((UINT16)Port);
      _ReadWriteBarrier ();
    }
  }

  FilterAfterIoRead (FilterWidth32, Port, &Value);

  return Value;
}

/**
  Writes a 32-bit I/O port.

  Writes the 32-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().

  For Td guest TDVMCALL_IO is invoked to write I/O port.

  @param  Port  The I/O port to write.
  @param  Value The value to write to the I/O port.

  @return The value written to the I/O port.

**/
UINT32
EFIAPI
IoWrite32 (
  IN      UINTN   Port,
  IN      UINT32  Value
  )
{
  BOOLEAN  Flag;

  ASSERT ((Port & 3) == 0);

  Flag = FilterBeforeIoWrite (FilterWidth32, Port, &Value);
  if (Flag) {
    if (IsTdxGuest ()) {
      TdIoWrite32 (Port, Value);
    } else {
      _ReadWriteBarrier ();
      _outpd ((UINT16)Port, Value);
      _ReadWriteBarrier ();
    }
  }

  FilterAfterIoWrite (FilterWidth32, Port, &Value);

  return Value;
}
