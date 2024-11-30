/** @file
  High-level Io/Mmio functions.

  All assertions for bit field operations are handled bit field functions in the
  Base Library.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The following IoLib instances contain the same copy of this file:

    BaseIoLibIntrinsic
    DxeIoLibCpuIo
    PeiIoLibCpuIo

**/

#include "BaseIoLibIntrinsicInternal.h"

/**
  Reads an 8-bit I/O port, performs a bitwise OR, and writes the
  result back to the 8-bit I/O port.

  Reads the 8-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 8-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to write.
  @param  OrData  The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
IoOr8 (
  IN      UINTN  Port,
  IN      UINT8  OrData
  )
{
  return IoWrite8 (Port, (UINT8)(IoRead8 (Port) | OrData));
}

/**
  Reads an 8-bit I/O port, performs a bitwise AND, and writes the result back
  to the 8-bit I/O port.

  Reads the 8-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 8-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to write.
  @param  AndData The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
IoAnd8 (
  IN      UINTN  Port,
  IN      UINT8  AndData
  )
{
  return IoWrite8 (Port, (UINT8)(IoRead8 (Port) & AndData));
}

/**
  Reads an 8-bit I/O port, performs a bitwise AND followed by a bitwise
  OR, and writes the result back to the 8-bit I/O port.

  Reads the 8-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 8-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port    The I/O port to write.
  @param  AndData The value to AND with the read value from the I/O port.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
IoAndThenOr8 (
  IN      UINTN  Port,
  IN      UINT8  AndData,
  IN      UINT8  OrData
  )
{
  return IoWrite8 (Port, (UINT8)((IoRead8 (Port) & AndData) | OrData));
}

/**
  Reads a bit field of an I/O register.

  Reads the bit field in an 8-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Port      The I/O port to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.

  @return The value read.

**/
UINT8
EFIAPI
IoBitFieldRead8 (
  IN      UINTN  Port,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  return BitFieldRead8 (IoRead8 (Port), StartBit, EndBit);
}

/**
  Writes a bit field to an I/O register.

  Writes Value to the bit field of the I/O register. The bit field is specified
  by the StartBit and the EndBit. All other bits in the destination I/O
  register are preserved. The value written to the I/O port is returned.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  Value     The new value of the bit field.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
IoBitFieldWrite8 (
  IN      UINTN  Port,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  Value
  )
{
  return IoWrite8 (
           Port,
           BitFieldWrite8 (IoRead8 (Port), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in an 8-bit port, performs a bitwise OR, and writes the
  result back to the bit field in the 8-bit port.

  Reads the 8-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 8-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized. Extra bits left in OrData are stripped.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  OrData    The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
IoBitFieldOr8 (
  IN      UINTN  Port,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  OrData
  )
{
  return IoWrite8 (
           Port,
           BitFieldOr8 (IoRead8 (Port), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in an 8-bit port, performs a bitwise AND, and writes the
  result back to the bit field in the 8-bit port.

  Reads the 8-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 8-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized. Extra bits left in AndData are stripped.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
IoBitFieldAnd8 (
  IN      UINTN  Port,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  AndData
  )
{
  return IoWrite8 (
           Port,
           BitFieldAnd8 (IoRead8 (Port), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in an 8-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  8-bit port.

  Reads the 8-bit I/O port specified by Port, performs a bitwise AND followed
  by a bitwise OR between the read result and the value specified by
  AndData, and writes the result to the 8-bit I/O port specified by Port. The
  value written to the I/O port is returned. This function must guarantee that
  all I/O read and write operations are serialized. Extra bits left in both
  AndData and OrData are stripped.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with the read value from the I/O port.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
IoBitFieldAndThenOr8 (
  IN      UINTN  Port,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  AndData,
  IN      UINT8  OrData
  )
{
  return IoWrite8 (
           Port,
           BitFieldAndThenOr8 (IoRead8 (Port), StartBit, EndBit, AndData, OrData)
           );
}

/**
  Reads a 16-bit I/O port, performs a bitwise OR, and writes the
  result back to the 16-bit I/O port.

  Reads the 16-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 16-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().

  @param  Port    The I/O port to write.
  @param  OrData  The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
IoOr16 (
  IN      UINTN   Port,
  IN      UINT16  OrData
  )
{
  return IoWrite16 (Port, (UINT16)(IoRead16 (Port) | OrData));
}

/**
  Reads a 16-bit I/O port, performs a bitwise AND, and writes the result back
  to the 16-bit I/O port.

  Reads the 16-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 16-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().

  @param  Port    The I/O port to write.
  @param  AndData The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
IoAnd16 (
  IN      UINTN   Port,
  IN      UINT16  AndData
  )
{
  return IoWrite16 (Port, (UINT16)(IoRead16 (Port) & AndData));
}

/**
  Reads a 16-bit I/O port, performs a bitwise AND followed by a bitwise
  OR, and writes the result back to the 16-bit I/O port.

  Reads the 16-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 16-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().

  @param  Port    The I/O port to write.
  @param  AndData The value to AND with the read value from the I/O port.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
IoAndThenOr16 (
  IN      UINTN   Port,
  IN      UINT16  AndData,
  IN      UINT16  OrData
  )
{
  return IoWrite16 (Port, (UINT16)((IoRead16 (Port) & AndData) | OrData));
}

/**
  Reads a bit field of an I/O register.

  Reads the bit field in a 16-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Port      The I/O port to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.

  @return The value read.

**/
UINT16
EFIAPI
IoBitFieldRead16 (
  IN      UINTN  Port,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  return BitFieldRead16 (IoRead16 (Port), StartBit, EndBit);
}

/**
  Writes a bit field to an I/O register.

  Writes Value to the bit field of the I/O register. The bit field is specified
  by the StartBit and the EndBit. All other bits in the destination I/O
  register are preserved. The value written to the I/O port is returned. Extra
  bits left in Value are stripped.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  Value     The new value of the bit field.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
IoBitFieldWrite16 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  Value
  )
{
  return IoWrite16 (
           Port,
           BitFieldWrite16 (IoRead16 (Port), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in a 16-bit port, performs a bitwise OR, and writes the
  result back to the bit field in the 16-bit port.

  Reads the 16-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 16-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized. Extra bits left in OrData are stripped.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  OrData    The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
IoBitFieldOr16 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  OrData
  )
{
  return IoWrite16 (
           Port,
           BitFieldOr16 (IoRead16 (Port), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in a 16-bit port, performs a bitwise AND, and writes the
  result back to the bit field in the 16-bit port.

  Reads the 16-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 16-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized. Extra bits left in AndData are stripped.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
IoBitFieldAnd16 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  AndData
  )
{
  return IoWrite16 (
           Port,
           BitFieldAnd16 (IoRead16 (Port), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in a 16-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  16-bit port.

  Reads the 16-bit I/O port specified by Port, performs a bitwise AND followed
  by a bitwise OR between the read result and the value specified by
  AndData, and writes the result to the 16-bit I/O port specified by Port. The
  value written to the I/O port is returned. This function must guarantee that
  all I/O read and write operations are serialized. Extra bits left in both
  AndData and OrData are stripped.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with the read value from the I/O port.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
IoBitFieldAndThenOr16 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  AndData,
  IN      UINT16  OrData
  )
{
  return IoWrite16 (
           Port,
           BitFieldAndThenOr16 (IoRead16 (Port), StartBit, EndBit, AndData, OrData)
           );
}

/**
  Reads a 32-bit I/O port, performs a bitwise OR, and writes the
  result back to the 32-bit I/O port.

  Reads the 32-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 32-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().

  @param  Port    The I/O port to write.
  @param  OrData  The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
IoOr32 (
  IN      UINTN   Port,
  IN      UINT32  OrData
  )
{
  return IoWrite32 (Port, IoRead32 (Port) | OrData);
}

/**
  Reads a 32-bit I/O port, performs a bitwise AND, and writes the result back
  to the 32-bit I/O port.

  Reads the 32-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 32-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().

  @param  Port    The I/O port to write.
  @param  AndData The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
IoAnd32 (
  IN      UINTN   Port,
  IN      UINT32  AndData
  )
{
  return IoWrite32 (Port, IoRead32 (Port) & AndData);
}

/**
  Reads a 32-bit I/O port, performs a bitwise AND followed by a bitwise
  OR, and writes the result back to the 32-bit I/O port.

  Reads the 32-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 32-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().

  @param  Port    The I/O port to write.
  @param  AndData The value to AND with the read value from the I/O port.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
IoAndThenOr32 (
  IN      UINTN   Port,
  IN      UINT32  AndData,
  IN      UINT32  OrData
  )
{
  return IoWrite32 (Port, (IoRead32 (Port) & AndData) | OrData);
}

/**
  Reads a bit field of an I/O register.

  Reads the bit field in a 32-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Port      The I/O port to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.

  @return The value read.

**/
UINT32
EFIAPI
IoBitFieldRead32 (
  IN      UINTN  Port,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  return BitFieldRead32 (IoRead32 (Port), StartBit, EndBit);
}

/**
  Writes a bit field to an I/O register.

  Writes Value to the bit field of the I/O register. The bit field is specified
  by the StartBit and the EndBit. All other bits in the destination I/O
  register are preserved. The value written to the I/O port is returned. Extra
  bits left in Value are stripped.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  Value     The new value of the bit field.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
IoBitFieldWrite32 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  Value
  )
{
  return IoWrite32 (
           Port,
           BitFieldWrite32 (IoRead32 (Port), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in a 32-bit port, performs a bitwise OR, and writes the
  result back to the bit field in the 32-bit port.

  Reads the 32-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 32-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized. Extra bits left in OrData are stripped.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  OrData    The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
IoBitFieldOr32 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  OrData
  )
{
  return IoWrite32 (
           Port,
           BitFieldOr32 (IoRead32 (Port), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in a 32-bit port, performs a bitwise AND, and writes the
  result back to the bit field in the 32-bit port.

  Reads the 32-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 32-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized. Extra bits left in AndData are stripped.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
IoBitFieldAnd32 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  AndData
  )
{
  return IoWrite32 (
           Port,
           BitFieldAnd32 (IoRead32 (Port), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in a 32-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  32-bit port.

  Reads the 32-bit I/O port specified by Port, performs a bitwise AND followed
  by a bitwise OR between the read result and the value specified by
  AndData, and writes the result to the 32-bit I/O port specified by Port. The
  value written to the I/O port is returned. This function must guarantee that
  all I/O read and write operations are serialized. Extra bits left in both
  AndData and OrData are stripped.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the read value from the I/O port.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
IoBitFieldAndThenOr32 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  AndData,
  IN      UINT32  OrData
  )
{
  return IoWrite32 (
           Port,
           BitFieldAndThenOr32 (IoRead32 (Port), StartBit, EndBit, AndData, OrData)
           );
}

/**
  Reads a 64-bit I/O port, performs a bitwise OR, and writes the
  result back to the 64-bit I/O port.

  Reads the 64-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 64-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().

  @param  Port    The I/O port to write.
  @param  OrData  The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
IoOr64 (
  IN      UINTN   Port,
  IN      UINT64  OrData
  )
{
  return IoWrite64 (Port, IoRead64 (Port) | OrData);
}

/**
  Reads a 64-bit I/O port, performs a bitwise AND, and writes the result back
  to the 64-bit I/O port.

  Reads the 64-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 64-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().

  @param  Port    The I/O port to write.
  @param  AndData The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
IoAnd64 (
  IN      UINTN   Port,
  IN      UINT64  AndData
  )
{
  return IoWrite64 (Port, IoRead64 (Port) & AndData);
}

/**
  Reads a 64-bit I/O port, performs a bitwise AND followed by a bitwise
  OR, and writes the result back to the 64-bit I/O port.

  Reads the 64-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 64-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().

  @param  Port    The I/O port to write.
  @param  AndData The value to AND with the read value from the I/O port.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
IoAndThenOr64 (
  IN      UINTN   Port,
  IN      UINT64  AndData,
  IN      UINT64  OrData
  )
{
  return IoWrite64 (Port, (IoRead64 (Port) & AndData) | OrData);
}

/**
  Reads a bit field of an I/O register.

  Reads the bit field in a 64-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Port      The I/O port to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.

  @return The value read.

**/
UINT64
EFIAPI
IoBitFieldRead64 (
  IN      UINTN  Port,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  return BitFieldRead64 (IoRead64 (Port), StartBit, EndBit);
}

/**
  Writes a bit field to an I/O register.

  Writes Value to the bit field of the I/O register. The bit field is specified
  by the StartBit and the EndBit. All other bits in the destination I/O
  register are preserved. The value written to the I/O port is returned. Extra
  bits left in Value are stripped.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  Value     The new value of the bit field.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
IoBitFieldWrite64 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  Value
  )
{
  return IoWrite64 (
           Port,
           BitFieldWrite64 (IoRead64 (Port), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in a 64-bit port, performs a bitwise OR, and writes the
  result back to the bit field in the 64-bit port.

  Reads the 64-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 64-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized. Extra bits left in OrData are stripped.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  OrData    The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
IoBitFieldOr64 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  OrData
  )
{
  return IoWrite64 (
           Port,
           BitFieldOr64 (IoRead64 (Port), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in a 64-bit port, performs a bitwise AND, and writes the
  result back to the bit field in the 64-bit port.

  Reads the 64-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 64-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized. Extra bits left in AndData are stripped.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  AndData   The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
IoBitFieldAnd64 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  AndData
  )
{
  return IoWrite64 (
           Port,
           BitFieldAnd64 (IoRead64 (Port), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in a 64-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  64-bit port.

  Reads the 64-bit I/O port specified by Port, performs a bitwise AND followed
  by a bitwise OR between the read result and the value specified by
  AndData, and writes the result to the 64-bit I/O port specified by Port. The
  value written to the I/O port is returned. This function must guarantee that
  all I/O read and write operations are serialized. Extra bits left in both
  AndData and OrData are stripped.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If Port is not aligned on a 64-bit boundary, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port      The I/O port to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  AndData   The value to AND with the read value from the I/O port.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
IoBitFieldAndThenOr64 (
  IN      UINTN   Port,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  AndData,
  IN      UINT64  OrData
  )
{
  return IoWrite64 (
           Port,
           BitFieldAndThenOr64 (IoRead64 (Port), StartBit, EndBit, AndData, OrData)
           );
}

/**
  Reads an 8-bit MMIO register, performs a bitwise OR, and writes the
  result back to the 8-bit MMIO register.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise
  OR between the read result and the value specified by OrData, and
  writes the result to the 8-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to write.
  @param  OrData  The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
MmioOr8 (
  IN      UINTN  Address,
  IN      UINT8  OrData
  )
{
  return MmioWrite8 (Address, (UINT8)(MmioRead8 (Address) | OrData));
}

/**
  Reads an 8-bit MMIO register, performs a bitwise AND, and writes the result
  back to the 8-bit MMIO register.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 8-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address The MMIO register to write.
  @param  AndData The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
MmioAnd8 (
  IN      UINTN  Address,
  IN      UINT8  AndData
  )
{
  return MmioWrite8 (Address, (UINT8)(MmioRead8 (Address) & AndData));
}

/**
  Reads an 8-bit MMIO register, performs a bitwise AND followed by a bitwise
  OR, and writes the result back to the 8-bit MMIO register.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 8-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().


  @param  Address The MMIO register to write.
  @param  AndData The value to AND with the read value from the MMIO register.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
MmioAndThenOr8 (
  IN      UINTN  Address,
  IN      UINT8  AndData,
  IN      UINT8  OrData
  )
{
  return MmioWrite8 (Address, (UINT8)((MmioRead8 (Address) & AndData) | OrData));
}

/**
  Reads a bit field of a MMIO register.

  Reads the bit field in an 8-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address   The MMIO register to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.

  @return The value read.

**/
UINT8
EFIAPI
MmioBitFieldRead8 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  return BitFieldRead8 (MmioRead8 (Address), StartBit, EndBit);
}

/**
  Writes a bit field to a MMIO register.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 8-bit register is returned.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  Value     The new value of the bit field.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
MmioBitFieldWrite8 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  Value
  )
{
  return MmioWrite8 (
           Address,
           BitFieldWrite8 (MmioRead8 (Address), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in an 8-bit MMIO register, performs a bitwise OR, and
  writes the result back to the bit field in the 8-bit MMIO register.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise
  OR between the read result and the value specified by OrData, and
  writes the result to the 8-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized. Extra bits left in OrData
  are stripped.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  OrData    The value to OR with read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
MmioBitFieldOr8 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  OrData
  )
{
  return MmioWrite8 (
           Address,
           BitFieldOr8 (MmioRead8 (Address), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in an 8-bit MMIO register, performs a bitwise AND, and
  writes the result back to the bit field in the 8-bit MMIO register.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 8-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized. Extra bits left in AndData are
  stripped.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
MmioBitFieldAnd8 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  AndData
  )
{
  return MmioWrite8 (
           Address,
           BitFieldAnd8 (MmioRead8 (Address), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in an 8-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, and writes the result back to the bit field in the
  8-bit MMIO register.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise AND
  followed by a bitwise OR between the read result and the value
  specified by AndData, and writes the result to the 8-bit MMIO register
  specified by Address. The value written to the MMIO register is returned.
  This function must guarantee that all MMIO read and write operations are
  serialized. Extra bits left in both AndData and OrData are stripped.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with read value from the MMIO register.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
MmioBitFieldAndThenOr8 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit,
  IN      UINT8  AndData,
  IN      UINT8  OrData
  )
{
  return MmioWrite8 (
           Address,
           BitFieldAndThenOr8 (MmioRead8 (Address), StartBit, EndBit, AndData, OrData)
           );
}

/**
  Reads a 16-bit MMIO register, performs a bitwise OR, and writes the
  result back to the 16-bit MMIO register.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise
  OR between the read result and the value specified by OrData, and
  writes the result to the 16-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  OrData  The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
MmioOr16 (
  IN      UINTN   Address,
  IN      UINT16  OrData
  )
{
  return MmioWrite16 (Address, (UINT16)(MmioRead16 (Address) | OrData));
}

/**
  Reads a 16-bit MMIO register, performs a bitwise AND, and writes the result
  back to the 16-bit MMIO register.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 16-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  AndData The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
MmioAnd16 (
  IN      UINTN   Address,
  IN      UINT16  AndData
  )
{
  return MmioWrite16 (Address, (UINT16)(MmioRead16 (Address) & AndData));
}

/**
  Reads a 16-bit MMIO register, performs a bitwise AND followed by a bitwise
  OR, and writes the result back to the 16-bit MMIO register.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 16-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  AndData The value to AND with the read value from the MMIO register.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
MmioAndThenOr16 (
  IN      UINTN   Address,
  IN      UINT16  AndData,
  IN      UINT16  OrData
  )
{
  return MmioWrite16 (Address, (UINT16)((MmioRead16 (Address) & AndData) | OrData));
}

/**
  Reads a bit field of a MMIO register.

  Reads the bit field in a 16-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address   The MMIO register to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.

  @return The value read.

**/
UINT16
EFIAPI
MmioBitFieldRead16 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  return BitFieldRead16 (MmioRead16 (Address), StartBit, EndBit);
}

/**
  Writes a bit field to a MMIO register.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 16-bit register is returned.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  Value     The new value of the bit field.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
MmioBitFieldWrite16 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  Value
  )
{
  return MmioWrite16 (
           Address,
           BitFieldWrite16 (MmioRead16 (Address), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in a 16-bit MMIO register, performs a bitwise OR, and
  writes the result back to the bit field in the 16-bit MMIO register.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise
  OR between the read result and the value specified by OrData, and
  writes the result to the 16-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized. Extra bits left in OrData
  are stripped.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  OrData    The value to OR with read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
MmioBitFieldOr16 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  OrData
  )
{
  return MmioWrite16 (
           Address,
           BitFieldOr16 (MmioRead16 (Address), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in a 16-bit MMIO register, performs a bitwise AND, and
  writes the result back to the bit field in the 16-bit MMIO register.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 16-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized. Extra bits left in AndData are
  stripped.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
MmioBitFieldAnd16 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  AndData
  )
{
  return MmioWrite16 (
           Address,
           BitFieldAnd16 (MmioRead16 (Address), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in a 16-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, and writes the result back to the bit field in the
  16-bit MMIO register.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise AND
  followed by a bitwise OR between the read result and the value
  specified by AndData, and writes the result to the 16-bit MMIO register
  specified by Address. The value written to the MMIO register is returned.
  This function must guarantee that all MMIO read and write operations are
  serialized. Extra bits left in both AndData and OrData are stripped.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with read value from the MMIO register.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
MmioBitFieldAndThenOr16 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT16  AndData,
  IN      UINT16  OrData
  )
{
  return MmioWrite16 (
           Address,
           BitFieldAndThenOr16 (MmioRead16 (Address), StartBit, EndBit, AndData, OrData)
           );
}

/**
  Reads a 32-bit MMIO register, performs a bitwise OR, and writes the
  result back to the 32-bit MMIO register.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise
  OR between the read result and the value specified by OrData, and
  writes the result to the 32-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  OrData  The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
MmioOr32 (
  IN      UINTN   Address,
  IN      UINT32  OrData
  )
{
  return MmioWrite32 (Address, MmioRead32 (Address) | OrData);
}

/**
  Reads a 32-bit MMIO register, performs a bitwise AND, and writes the result
  back to the 32-bit MMIO register.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 32-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  AndData The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
MmioAnd32 (
  IN      UINTN   Address,
  IN      UINT32  AndData
  )
{
  return MmioWrite32 (Address, MmioRead32 (Address) & AndData);
}

/**
  Reads a 32-bit MMIO register, performs a bitwise AND followed by a bitwise
  OR, and writes the result back to the 32-bit MMIO register.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 32-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  AndData The value to AND with the read value from the MMIO register.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
MmioAndThenOr32 (
  IN      UINTN   Address,
  IN      UINT32  AndData,
  IN      UINT32  OrData
  )
{
  return MmioWrite32 (Address, (MmioRead32 (Address) & AndData) | OrData);
}

/**
  Reads a bit field of a MMIO register.

  Reads the bit field in a 32-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address   The MMIO register to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.

  @return The value read.

**/
UINT32
EFIAPI
MmioBitFieldRead32 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  return BitFieldRead32 (MmioRead32 (Address), StartBit, EndBit);
}

/**
  Writes a bit field to a MMIO register.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 32-bit register is returned.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  Value     The new value of the bit field.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
MmioBitFieldWrite32 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  Value
  )
{
  return MmioWrite32 (
           Address,
           BitFieldWrite32 (MmioRead32 (Address), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in a 32-bit MMIO register, performs a bitwise OR, and
  writes the result back to the bit field in the 32-bit MMIO register.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise
  OR between the read result and the value specified by OrData, and
  writes the result to the 32-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized. Extra bits left in OrData
  are stripped.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  OrData    The value to OR with read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
MmioBitFieldOr32 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  OrData
  )
{
  return MmioWrite32 (
           Address,
           BitFieldOr32 (MmioRead32 (Address), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in a 32-bit MMIO register, performs a bitwise AND, and
  writes the result back to the bit field in the 32-bit MMIO register.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 32-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized. Extra bits left in AndData are
  stripped.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
MmioBitFieldAnd32 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  AndData
  )
{
  return MmioWrite32 (
           Address,
           BitFieldAnd32 (MmioRead32 (Address), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in a 32-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, and writes the result back to the bit field in the
  32-bit MMIO register.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise AND
  followed by a bitwise OR between the read result and the value
  specified by AndData, and writes the result to the 32-bit MMIO register
  specified by Address. The value written to the MMIO register is returned.
  This function must guarantee that all MMIO read and write operations are
  serialized. Extra bits left in both AndData and OrData are stripped.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with read value from the MMIO register.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
MmioBitFieldAndThenOr32 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  AndData,
  IN      UINT32  OrData
  )
{
  return MmioWrite32 (
           Address,
           BitFieldAndThenOr32 (MmioRead32 (Address), StartBit, EndBit, AndData, OrData)
           );
}

/**
  Reads a 64-bit MMIO register, performs a bitwise OR, and writes the
  result back to the 64-bit MMIO register.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise
  OR between the read result and the value specified by OrData, and
  writes the result to the 64-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  OrData  The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
MmioOr64 (
  IN      UINTN   Address,
  IN      UINT64  OrData
  )
{
  return MmioWrite64 (Address, MmioRead64 (Address) | OrData);
}

/**
  Reads a 64-bit MMIO register, performs a bitwise AND, and writes the result
  back to the 64-bit MMIO register.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 64-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  AndData The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
MmioAnd64 (
  IN      UINTN   Address,
  IN      UINT64  AndData
  )
{
  return MmioWrite64 (Address, MmioRead64 (Address) & AndData);
}

/**
  Reads a 64-bit MMIO register, performs a bitwise AND followed by a bitwise
  OR, and writes the result back to the 64-bit MMIO register.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 64-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().

  @param  Address The MMIO register to write.
  @param  AndData The value to AND with the read value from the MMIO register.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
MmioAndThenOr64 (
  IN      UINTN   Address,
  IN      UINT64  AndData,
  IN      UINT64  OrData
  )
{
  return MmioWrite64 (Address, (MmioRead64 (Address) & AndData) | OrData);
}

/**
  Reads a bit field of a MMIO register.

  Reads the bit field in a 64-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address   The MMIO register to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.

  @return The value read.

**/
UINT64
EFIAPI
MmioBitFieldRead64 (
  IN      UINTN  Address,
  IN      UINTN  StartBit,
  IN      UINTN  EndBit
  )
{
  return BitFieldRead64 (MmioRead64 (Address), StartBit, EndBit);
}

/**
  Writes a bit field to a MMIO register.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 64-bit register is returned.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  Value     The new value of the bit field.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
MmioBitFieldWrite64 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  Value
  )
{
  return MmioWrite64 (
           Address,
           BitFieldWrite64 (MmioRead64 (Address), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in a 64-bit MMIO register, performs a bitwise OR, and
  writes the result back to the bit field in the 64-bit MMIO register.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise
  OR between the read result and the value specified by OrData, and
  writes the result to the 64-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized. Extra bits left in OrData
  are stripped.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  OrData    The value to OR with read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
MmioBitFieldOr64 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  OrData
  )
{
  return MmioWrite64 (
           Address,
           BitFieldOr64 (MmioRead64 (Address), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in a 64-bit MMIO register, performs a bitwise AND, and
  writes the result back to the bit field in the 64-bit MMIO register.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 64-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized. Extra bits left in AndData are
  stripped.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  AndData   The value to AND with read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
MmioBitFieldAnd64 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  AndData
  )
{
  return MmioWrite64 (
           Address,
           BitFieldAnd64 (MmioRead64 (Address), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in a 64-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, and writes the result back to the bit field in the
  64-bit MMIO register.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise AND
  followed by a bitwise OR between the read result and the value
  specified by AndData, and writes the result to the 64-bit MMIO register
  specified by Address. The value written to the MMIO register is returned.
  This function must guarantee that all MMIO read and write operations are
  serialized. Extra bits left in both AndData and OrData are stripped.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If Address is not aligned on a 64-bit boundary, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   The MMIO register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  AndData   The value to AND with read value from the MMIO register.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
MmioBitFieldAndThenOr64 (
  IN      UINTN   Address,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  AndData,
  IN      UINT64  OrData
  )
{
  return MmioWrite64 (
           Address,
           BitFieldAndThenOr64 (MmioRead64 (Address), StartBit, EndBit, AndData, OrData)
           );
}
