/** @file
  I/O and MMIO Library Services that do I/O and also enable the I/O operation
  to be replayed during an S3 resume. This library class maps directly on top
  of the IoLib class.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __S3_IO_LIB_H__
#define __S3_IO_LIB_H__

/**
  Reads an 8-bit I/O port and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 8-bit I/O port specified by Port. The 8-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port   The I/O port to read.

  @return   The value read.

**/
UINT8
EFIAPI
S3IoRead8 (
  IN UINTN  Port
  );

/**
  Writes an 8-bit I/O port, and saves the value in the S3 script to be replayed
  on S3 resume.

  Writes the 8-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port    The I/O port to write.
  @param[in]  Value   The value to write to the I/O port.

  @return   The value written the I/O port.

**/
UINT8
EFIAPI
S3IoWrite8 (
  IN UINTN  Port,
  IN UINT8  Value
  );

/**
  Reads an 8-bit I/O port, performs a bitwise OR, writes the
  result back to the 8-bit I/O port, and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 8-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 8-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port     The I/O port to write.
  @param[in]  OrData   The value to OR with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoOr8 (
  IN UINTN  Port,
  IN UINT8  OrData
  );

/**
  Reads an 8-bit I/O port, performs a bitwise AND, writes the result back
  to the 8-bit I/O port, and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 8-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 8-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port      The I/O port to write.
  @param[in]  AndData   The value to AND with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoAnd8 (
  IN UINTN  Port,
  IN UINT8  AndData
  );

/**
  Reads an 8-bit I/O port, performs a bitwise AND followed by a bitwise
  inclusive OR, writes the result back to the 8-bit I/O port, and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 8-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port      The I/O port to write.
  @param[in]  AndData   The value to AND with the read value from the I/O port.
  @param[in]  OrData    The value to OR with the result of the AND operation.

  @return   The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoAndThenOr8 (
  IN UINTN  Port,
  IN UINT8  AndData,
  IN UINT8  OrData
  );

/**
  Reads a bit field of an I/O register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in an 8-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param[in]  Port       The I/O port to read.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..7.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..7.

  @return   The value read.

**/
UINT8
EFIAPI
S3IoBitFieldRead8 (
  IN UINTN  Port,
  IN UINTN  StartBit,
  IN UINTN  EndBit
  );

/**
  Writes a bit field to an I/O register and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes Value to the bit field of the I/O register. The bit field is specified
  by the StartBit and the EndBit. All other bits in the destination I/O
  register are preserved. The value written to the I/O port is returned.
  Remaining bits in Value are stripped.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..7.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..7.
  @param[in]  Value      New value of the bit field.

  @return   The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoBitFieldWrite8 (
  IN UINTN  Port,
  IN UINTN  StartBit,
  IN UINTN  EndBit,
  IN UINT8  Value
  );

/**
  Reads a bit field in an 8-bit port, performs a bitwise OR, writes the
  result back to the bit field in the 8-bit port, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the 8-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 8-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized. Extra left bits in OrData are stripped.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..7.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..7.
  @param[in]  OrData     The value to OR with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoBitFieldOr8 (
  IN UINTN  Port,
  IN UINTN  StartBit,
  IN UINTN  EndBit,
  IN UINT8  OrData
  );

/**
  Reads a bit field in an 8-bit port, performs a bitwise AND, writes the
  result back to the bit field in the 8-bit port, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the 8-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 8-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized. Extra left bits in AndData are stripped.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..7.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..7.
  @param[in]  AndData    The value to AND with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoBitFieldAnd8 (
  IN UINTN  Port,
  IN UINTN  StartBit,
  IN UINTN  EndBit,
  IN UINT8  AndData
  );

/**
  Reads a bit field in an 8-bit port, performs a bitwise AND followed by a
  bitwise OR, writes the result back to the bit field in the
  8-bit port, and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit I/O port specified by Port, performs a bitwise AND followed
  by a bitwise OR between the read result and the value specified by
  AndData, and writes the result to the 8-bit I/O port specified by Port. The
  value written to the I/O port is returned. This function must guarantee that
  all I/O read and write operations are serialized. Extra left bits in both
  AndData and OrData are stripped.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..7.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..7.
  @param[in]  AndData    The value to AND with the read value from the I/O port.
  @param[in]  OrData     The value to OR with the result of the AND operation.

  @return   The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoBitFieldAndThenOr8 (
  IN UINTN  Port,
  IN UINTN  StartBit,
  IN UINTN  EndBit,
  IN UINT8  AndData,
  IN UINT8  OrData
  );

/**
  Reads a 16-bit I/O port, and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 16-bit I/O port specified by Port. The 16-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port   The I/O port to read.

  @return   The value read.

**/
UINT16
EFIAPI
S3IoRead16 (
  IN UINTN  Port
  );

/**
  Writes a 16-bit I/O port, and saves the value in the S3 script to be replayed
  on S3 resume.

  Writes the 16-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port    The I/O port to write.
  @param[in]  Value   The value to write to the I/O port.

  @return   The value written the I/O port.

**/
UINT16
EFIAPI
S3IoWrite16 (
  IN UINTN   Port,
  IN UINT16  Value
  );

/**
  Reads a 16-bit I/O port, performs a bitwise OR, writes the
  result back to the 16-bit I/O port, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the 16-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 16-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port     The I/O port to write.
  @param[in]  OrData   The value to OR with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoOr16 (
  IN UINTN   Port,
  IN UINT16  OrData
  );

/**
  Reads a 16-bit I/O port, performs a bitwise AND, writes the result back
  to the 16-bit I/O port , and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 16-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 16-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port      The I/O port to write.
  @param[in]  AndData   The value to AND with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoAnd16 (
  IN UINTN   Port,
  IN UINT16  AndData
  );

/**
  Reads a 16-bit I/O port, performs a bitwise AND followed by a bitwise
  inclusive OR, writes the result back to the 16-bit I/O port, and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 16-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port      The I/O port to write.
  @param[in]  AndData   The value to AND with the read value from the I/O port.
  @param[in]  OrData    The value to OR with the result of the AND operation.

  @return   The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoAndThenOr16 (
  IN UINTN   Port,
  IN UINT16  AndData,
  IN UINT16  OrData
  );

/**
  Reads a bit field of an I/O register saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the bit field in a 16-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param[in]  Port       The I/O port to read.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..15.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..15.

  @return   The value read.

**/
UINT16
EFIAPI
S3IoBitFieldRead16 (
  IN UINTN  Port,
  IN UINTN  StartBit,
  IN UINTN  EndBit
  );

/**
  Writes a bit field to an I/O register, and saves the value in the S3 script
  to be replayed on S3 resume.

  Writes Value to the bit field of the I/O register. The bit field is specified
  by the StartBit and the EndBit. All other bits in the destination I/O
  register are preserved. The value written to the I/O port is returned. Extra
  left bits in Value are stripped.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..15.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..15.
  @param[in]  Value      New value of the bit field.

  @return   The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoBitFieldWrite16 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  Value
  );

/**
  Reads a bit field in a 16-bit port, performs a bitwise OR, writes the
  result back to the bit field in the 16-bit port, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the 16-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 16-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized. Extra left bits in OrData are stripped.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..15.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..15.
  @param[in]  OrData     The value to OR with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoBitFieldOr16 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  OrData
  );

/**
  Reads a bit field in a 16-bit port, performs a bitwise AND, writes the
  result back to the bit field in the 16-bit port, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the 16-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 16-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized. Extra left bits in AndData are stripped.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..15.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..15.
  @param[in]  AndData    The value to AND with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoBitFieldAnd16 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  AndData
  );

/**
  Reads a bit field in a 16-bit port, performs a bitwise AND followed by a
  bitwise OR, writes the result back to the bit field in the
  16-bit port, and saves the value in the S3 script to be replayed on S3
  resume.

  Reads the 16-bit I/O port specified by Port, performs a bitwise AND followed
  by a bitwise OR between the read result and the value specified by
  AndData, and writes the result to the 16-bit I/O port specified by Port. The
  value written to the I/O port is returned. This function must guarantee that
  all I/O read and write operations are serialized. Extra left bits in both
  AndData and OrData are stripped.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..15.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..15.
  @param[in]  AndData    The value to AND with the read value from the I/O port.
  @param[in]  OrData     The value to OR with the result of the AND operation.

  @return   The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoBitFieldAndThenOr16 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  AndData,
  IN UINT16  OrData
  );

/**
  Reads a 32-bit I/O port, and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 32-bit I/O port specified by Port. The 32-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port   The I/O port to read.

  @return   The value read.

**/
UINT32
EFIAPI
S3IoRead32 (
  IN UINTN  Port
  );

/**
  Writes a 32-bit I/O port, and saves the value in the S3 script to be replayed
  on S3 resume.

  Writes the 32-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port    The I/O port to write.
  @param[in]  Value   The value to write to the I/O port.

  @return   The value written the I/O port.

**/
UINT32
EFIAPI
S3IoWrite32 (
  IN UINTN   Port,
  IN UINT32  Value
  );

/**
  Reads a 32-bit I/O port, performs a bitwise OR, writes the
  result back to the 32-bit I/O port, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the 32-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 32-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port     The I/O port to write.
  @param[in]  OrData   The value to OR with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoOr32 (
  IN UINTN   Port,
  IN UINT32  OrData
  );

/**
  Reads a 32-bit I/O port, performs a bitwise AND, writes the result back
  to the 32-bit I/O port, and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 32-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 32-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port      The I/O port to write.
  @param[in]  AndData   The value to AND with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoAnd32 (
  IN UINTN   Port,
  IN UINT32  AndData
  );

/**
  Reads a 32-bit I/O port, performs a bitwise AND followed by a bitwise
  inclusive OR, writes the result back to the 32-bit I/O port, and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 32-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port      The I/O port to write.
  @param[in]  AndData   The value to AND with the read value from the I/O port.
  @param[in]  OrData    The value to OR with the result of the AND operation.

  @return   The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoAndThenOr32 (
  IN UINTN   Port,
  IN UINT32  AndData,
  IN UINT32  OrData
  );

/**
  Reads a bit field of an I/O register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in a 32-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param[in]  Port       The I/O port to read.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..31.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..31.

  @return   The value read.

**/
UINT32
EFIAPI
S3IoBitFieldRead32 (
  IN UINTN  Port,
  IN UINTN  StartBit,
  IN UINTN  EndBit
  );

/**
  Writes a bit field to an I/O register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes Value to the bit field of the I/O register. The bit field is specified
  by the StartBit and the EndBit. All other bits in the destination I/O
  register are preserved. The value written to the I/O port is returned. Extra
  left bits in Value are stripped.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..31.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..31.
  @param[in]  Value      New value of the bit field.

  @return   The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoBitFieldWrite32 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  Value
  );

/**
  Reads a bit field in a 32-bit port, performs a bitwise OR, writes the
  result back to the bit field in the 32-bit port, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the 32-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 32-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized. Extra left bits in OrData are stripped.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..31.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..31.
  @param[in]  OrData     The value to OR with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoBitFieldOr32 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  OrData
  );

/**
  Reads a bit field in a 32-bit port, performs a bitwise AND, writes the
  result back to the bit field in the 32-bit port, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the 32-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 32-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized. Extra left bits in AndData are stripped.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..31.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..31.
  @param[in]  AndData    The value to AND with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoBitFieldAnd32 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  AndData
  );

/**
  Reads a bit field in a 32-bit port, performs a bitwise AND followed by a
  bitwise OR, writes the result back to the bit field in the
  32-bit port, and saves the value in the S3 script to be replayed on S3
  resume.

  Reads the 32-bit I/O port specified by Port, performs a bitwise AND followed
  by a bitwise OR between the read result and the value specified by
  AndData, and writes the result to the 32-bit I/O port specified by Port. The
  value written to the I/O port is returned. This function must guarantee that
  all I/O read and write operations are serialized. Extra left bits in both
  AndData and OrData are stripped.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..31.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..31.
  @param[in]  AndData    The value to AND with the read value from the I/O port.
  @param[in]  OrData     The value to OR with the result of the AND operation.

  @return   The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoBitFieldAndThenOr32 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  AndData,
  IN UINT32  OrData
  );

/**
  Reads a 64-bit I/O port, and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 64-bit I/O port specified by Port. The 64-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port   The I/O port to read.

  @return   The value read.

**/
UINT64
EFIAPI
S3IoRead64 (
  IN UINTN  Port
  );

/**
  Writes a 64-bit I/O port, and saves the value in the S3 script to be replayed
  on S3 resume.

  Writes the 64-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port    The I/O port to write.
  @param[in]  Value   The value to write to the I/O port.

  @return   The value written to the I/O port.

**/
UINT64
EFIAPI
S3IoWrite64 (
  IN UINTN   Port,
  IN UINT64  Value
  );

/**
  Reads a 64-bit I/O port, performs a bitwise OR, writes the
  result back to the 64-bit I/O port, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the 64-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 64-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port     The I/O port to write.
  @param[in]  OrData   The value to OR with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoOr64 (
  IN UINTN   Port,
  IN UINT64  OrData
  );

/**
  Reads a 64-bit I/O port, performs a bitwise AND, writes the result back
  to the 64-bit I/O port, and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 64-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 64-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port      The I/O port to write.
  @param[in]  AndData   The value to AND with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoAnd64 (
  IN UINTN   Port,
  IN UINT64  AndData
  );

/**
  Reads a 64-bit I/O port, performs a bitwise AND followed by a bitwise
  inclusive OR, writes the result back to the 64-bit I/O port, and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 64-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 64-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().

  @param[in]  Port      The I/O port to write.
  @param[in]  AndData   The value to AND with the read value from the I/O port.
  @param[in]  OrData    The value to OR with the result of the AND operation.

  @return   The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoAndThenOr64 (
  IN UINTN   Port,
  IN UINT64  AndData,
  IN UINT64  OrData
  );

/**
  Reads a bit field of an I/O register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in a 64-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param[in]  Port       The I/O port to read.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..63.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..63.

  @return   The value read.

**/
UINT64
EFIAPI
S3IoBitFieldRead64 (
  IN UINTN  Port,
  IN UINTN  StartBit,
  IN UINTN  EndBit
  );

/**
  Writes a bit field to an I/O register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes Value to the bit field of the I/O register. The bit field is specified
  by the StartBit and the EndBit. All other bits in the destination I/O
  register are preserved. The value written to the I/O port is returned. Extra
  left bits in Value are stripped.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..63.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..63.
  @param[in]  Value      New value of the bit field.

  @return   The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoBitFieldWrite64 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT64  Value
  );

/**
  Reads a bit field in a 64-bit port, performs a bitwise OR, writes the
  result back to the bit field in the 64-bit port, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the 64-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 64-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized. Extra left bits in OrData are stripped.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..63.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..63.
  @param[in]  OrData     The value to OR with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoBitFieldOr64 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT64  OrData
  );

/**
  Reads a bit field in a 64-bit port, performs a bitwise AND, writes the
  result back to the bit field in the 64-bit port, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the 64-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 64-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized. Extra left bits in AndData are stripped.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..63.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..63.
  @param[in]  AndData    The value to AND with the read value from the I/O port.

  @return   The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoBitFieldAnd64 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT64  AndData
  );

/**
  Reads a bit field in a 64-bit port, performs a bitwise AND followed by a
  bitwise OR, writes the result back to the bit field in the
  64-bit port, and saves the value in the S3 script to be replayed on S3
  resume.

  Reads the 64-bit I/O port specified by Port, performs a bitwise AND followed
  by a bitwise OR between the read result and the value specified by
  AndData, and writes the result to the 64-bit I/O port specified by Port. The
  value written to the I/O port is returned. This function must guarantee that
  all I/O read and write operations are serialized. Extra left bits in both
  AndData and OrData are stripped.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Port       The I/O port to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..63.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..63.
  @param[in]  AndData    The value to AND with the read value from the I/O port.
  @param[in]  OrData     The value to OR with the result of the AND operation.

  @return   The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoBitFieldAndThenOr64 (
  IN UINTN   Port,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT64  AndData,
  IN UINT64  OrData
  );

/**
  Reads an 8-bit MMIO register, and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 8-bit MMIO register specified by Address. The 8-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to read.

  @return   The value read.

**/
UINT8
EFIAPI
S3MmioRead8 (
  IN UINTN  Address
  );

/**
  Writes an 8-bit MMIO register, and saves the value in the S3 script to be
  replayed on S3 resume.

  Writes the 8-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  Value     The value to write to the MMIO register.

  @return   The value written the MMIO register.

**/
UINT8
EFIAPI
S3MmioWrite8 (
  IN UINTN  Address,
  IN UINT8  Value
  );

/**
  Reads an 8-bit MMIO register, performs a bitwise OR, writes the
  result back to the 8-bit MMIO register, and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 8-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  OrData    The value to OR with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioOr8 (
  IN UINTN  Address,
  IN UINT8  OrData
  );

/**
  Reads an 8-bit MMIO register, performs a bitwise AND, writes the result
  back to the 8-bit MMIO register, and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 8-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  AndData   The value to AND with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioAnd8 (
  IN UINTN  Address,
  IN UINT8  AndData
  );

/**
  Reads an 8-bit MMIO register, performs a bitwise AND followed by a bitwise
  inclusive OR, writes the result back to the 8-bit MMIO register, and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 8-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  AndData   The value to AND with the read value from the MMIO register.
  @param[in]  OrData    The value to OR with the result of the AND operation.

  @return   The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioAndThenOr8 (
  IN UINTN  Address,
  IN UINT8  AndData,
  IN UINT8  OrData
  );

/**
  Reads a bit field of a MMIO register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in an 8-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param[in]  Address    MMIO register to read.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..7.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..7.

  @return   The value read.

**/
UINT8
EFIAPI
S3MmioBitFieldRead8 (
  IN UINTN  Address,
  IN UINTN  StartBit,
  IN UINTN  EndBit
  );

/**
  Writes a bit field to an MMIO register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 8-bit register is returned.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..7.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..7.
  @param[in]  Value      New value of the bit field.

  @return   The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioBitFieldWrite8 (
  IN UINTN  Address,
  IN UINTN  StartBit,
  IN UINTN  EndBit,
  IN UINT8  Value
  );

/**
  Reads a bit field in an 8-bit MMIO register, performs a bitwise OR,
  writes the result back to the bit field in the 8-bit MMIO register, and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 8-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized. Extra left bits in OrData
  are stripped.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..7.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..7.
  @param[in]  OrData     The value to OR with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioBitFieldOr8 (
  IN UINTN  Address,
  IN UINTN  StartBit,
  IN UINTN  EndBit,
  IN UINT8  OrData
  );

/**
  Reads a bit field in an 8-bit MMIO register, performs a bitwise AND, and
  writes the result back to the bit field in the 8-bit MMIO register, and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 8-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized. Extra left bits in AndData are
  stripped.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..7.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..7.
  @param[in]  AndData    The value to AND with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioBitFieldAnd8 (
  IN UINTN  Address,
  IN UINTN  StartBit,
  IN UINTN  EndBit,
  IN UINT8  AndData
  );

/**
  Reads a bit field in an 8-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, writes the result back to the bit field in the
  8-bit MMIO register, and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise AND
  followed by a bitwise OR between the read result and the value
  specified by AndData, and writes the result to the 8-bit MMIO register
  specified by Address. The value written to the MMIO register is returned.
  This function must guarantee that all MMIO read and write operations are
  serialized. Extra left bits in both AndData and OrData are stripped.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..7.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..7.
  @param[in]  AndData    The value to AND with the read value from the MMIO register.
  @param[in]  OrData     The value to OR with the result of the AND operation.

  @return   The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioBitFieldAndThenOr8 (
  IN UINTN  Address,
  IN UINTN  StartBit,
  IN UINTN  EndBit,
  IN UINT8  AndData,
  IN UINT8  OrData
  );

/**
  Reads a 16-bit MMIO register, and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 16-bit MMIO register specified by Address. The 16-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to read.

  @return   The value read.

**/
UINT16
EFIAPI
S3MmioRead16 (
  IN UINTN  Address
  );

/**
  Writes a 16-bit MMIO register, and saves the value in the S3 script to be replayed
  on S3 resume.

  Writes the 16-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized, and saves the value in the S3 script to be
  replayed on S3 resume.

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  Value     The value to write to the MMIO register.

  @return   The value written the MMIO register.

**/
UINT16
EFIAPI
S3MmioWrite16 (
  IN UINTN   Address,
  IN UINT16  Value
  );

/**
  Reads a 16-bit MMIO register, performs a bitwise OR, writes the
  result back to the 16-bit MMIO register, and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 16-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  OrData    The value to OR with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioOr16 (
  IN UINTN   Address,
  IN UINT16  OrData
  );

/**
  Reads a 16-bit MMIO register, performs a bitwise AND, writes the result
  back to the 16-bit MMIO register, and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 16-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  AndData   The value to AND with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioAnd16 (
  IN UINTN   Address,
  IN UINT16  AndData
  );

/**
  Reads a 16-bit MMIO register, performs a bitwise AND followed by a bitwise
  inclusive OR, writes the result back to the 16-bit MMIO register, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 16-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  AndData   The value to AND with the read value from the MMIO register.
  @param[in]  OrData    The value to OR with the result of the AND operation.

  @return   The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioAndThenOr16 (
  IN UINTN   Address,
  IN UINT16  AndData,
  IN UINT16  OrData
  );

/**
  Reads a bit field of a MMIO register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in a 16-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param[in]  Address    MMIO register to read.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..15.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..15.

  @return   The value read.

**/
UINT16
EFIAPI
S3MmioBitFieldRead16 (
  IN UINTN  Address,
  IN UINTN  StartBit,
  IN UINTN  EndBit
  );

/**
  Writes a bit field to a MMIO register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 16-bit register is returned.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..15.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..15.
  @param[in]  Value      New value of the bit field.

  @return   The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioBitFieldWrite16 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  Value
  );

/**
  Reads a bit field in a 16-bit MMIO register, performs a bitwise OR,
  writes the result back to the bit field in the 16-bit MMIO register, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 16-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized. Extra left bits in OrData
  are stripped.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..15.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..15.
  @param[in]  OrData     The value to OR with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioBitFieldOr16 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  OrData
  );

/**
  Reads a bit field in a 16-bit MMIO register, performs a bitwise AND, and
  writes the result back to the bit field in the 16-bit MMIO register and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 16-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized. Extra left bits in AndData are
  stripped.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..15.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..15.
  @param[in]  AndData    The value to AND with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioBitFieldAnd16 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  AndData
  );

/**
  Reads a bit field in a 16-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, writes the result back to the bit field in the
  16-bit MMIO register, and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise AND
  followed by a bitwise OR between the read result and the value
  specified by AndData, and writes the result to the 16-bit MMIO register
  specified by Address. The value written to the MMIO register is returned.
  This function must guarantee that all MMIO read and write operations are
  serialized. Extra left bits in both AndData and OrData are stripped.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..15.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..15.
  @param[in]  AndData    The value to AND with the read value from the MMIO register.
  @param[in]  OrData     The value to OR with the result of the AND operation.

  @return   The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioBitFieldAndThenOr16 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  AndData,
  IN UINT16  OrData
  );

/**
  Reads a 32-bit MMIO register saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 32-bit MMIO register specified by Address. The 32-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to read.

  @return   The value read.

**/
UINT32
EFIAPI
S3MmioRead32 (
  IN UINTN  Address
  );

/**
  Writes a 32-bit MMIO register, and saves the value in the S3 script to be
  replayed on S3 resume.

  Writes the 32-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  Value     The value to write to the MMIO register.

  @return   The value written the MMIO register.

**/
UINT32
EFIAPI
S3MmioWrite32 (
  IN UINTN   Address,
  IN UINT32  Value
  );

/**
  Reads a 32-bit MMIO register, performs a bitwise OR, writes the
  result back to the 32-bit MMIO register, and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 32-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  OrData    The value to OR with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioOr32 (
  IN UINTN   Address,
  IN UINT32  OrData
  );

/**
  Reads a 32-bit MMIO register, performs a bitwise AND, writes the result
  back to the 32-bit MMIO register, and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 32-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  AndData   The value to AND with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioAnd32 (
  IN UINTN   Address,
  IN UINT32  AndData
  );

/**
  Reads a 32-bit MMIO register, performs a bitwise AND followed by a bitwise
  inclusive OR, writes the result back to the 32-bit MMIO register, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 32-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  AndData   The value to AND with the read value from the MMIO register.
  @param[in]  OrData    The value to OR with the result of the AND operation.

  @return   The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioAndThenOr32 (
  IN UINTN   Address,
  IN UINT32  AndData,
  IN UINT32  OrData
  );

/**
  Reads a bit field of a MMIO register, and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads the bit field in a 32-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param[in]  Address    MMIO register to read.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..31.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..31.

  @return   The value read.

**/
UINT32
EFIAPI
S3MmioBitFieldRead32 (
  IN UINTN  Address,
  IN UINTN  StartBit,
  IN UINTN  EndBit
  );

/**
  Writes a bit field to a MMIO register, and saves the value in the S3 script
  to be replayed on S3 resume.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 32-bit register is returned.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..31.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..31.
  @param[in]  Value      New value of the bit field.

  @return   The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioBitFieldWrite32 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  Value
  );

/**
  Reads a bit field in a 32-bit MMIO register, performs a bitwise OR,
  writes the result back to the bit field in the 32-bit MMIO register, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 32-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized. Extra left bits in OrData
  are stripped.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..31.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..31.
  @param[in]  OrData     The value to OR with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioBitFieldOr32 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  OrData
  );

/**
  Reads a bit field in a 32-bit MMIO register, performs a bitwise AND, and
  writes the result back to the bit field in the 32-bit MMIO register and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 32-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized. Extra left bits in AndData are
  stripped.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..31.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..31.
  @param[in]  AndData    The value to AND with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioBitFieldAnd32 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  AndData
  );

/**
  Reads a bit field in a 32-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, writes the result back to the bit field in the
  32-bit MMIO register, and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise AND
  followed by a bitwise OR between the read result and the value
  specified by AndData, and writes the result to the 32-bit MMIO register
  specified by Address. The value written to the MMIO register is returned.
  This function must guarantee that all MMIO read and write operations are
  serialized. Extra left bits in both AndData and OrData are stripped.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..31.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..31.
  @param[in]  AndData    The value to AND with the read value from the MMIO register.
  @param[in]  OrData     The value to OR with the result of the AND operation.

  @return   The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioBitFieldAndThenOr32 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  AndData,
  IN UINT32  OrData
  );

/**
  Reads a 64-bit MMIO register, and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 64-bit MMIO register specified by Address. The 64-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to read.

  @return   The value read.

**/
UINT64
EFIAPI
S3MmioRead64 (
  IN UINTN  Address
  );

/**
  Writes a 64-bit MMIO register, and saves the value in the S3 script to be
  replayed on S3 resume.

  Writes the 64-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  Value     The value to write to the MMIO register.

  @return   The value written the MMIO register.

**/
UINT64
EFIAPI
S3MmioWrite64 (
  IN UINTN   Address,
  IN UINT64  Value
  );

/**
  Reads a 64-bit MMIO register, performs a bitwise OR, writes the
  result back to the 64-bit MMIO register, and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 64-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  OrData    The value to OR with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioOr64 (
  IN UINTN   Address,
  IN UINT64  OrData
  );

/**
  Reads a 64-bit MMIO register, performs a bitwise AND, writes the result
  back to the 64-bit MMIO register, and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 64-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  AndData   The value to AND with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioAnd64 (
  IN UINTN   Address,
  IN UINT64  AndData
  );

/**
  Reads a 64-bit MMIO register, performs a bitwise AND followed by a bitwise
  inclusive OR, writes the result back to the 64-bit MMIO register, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 64-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().

  @param[in]  Address   The MMIO register to write.
  @param[in]  AndData   The value to AND with the read value from the MMIO register.
  @param[in]  OrData    The value to OR with the result of the AND operation.

  @return   The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioAndThenOr64 (
  IN UINTN   Address,
  IN UINT64  AndData,
  IN UINT64  OrData
  );

/**
  Reads a bit field of a MMIO register saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in a 64-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param[in]  Address    MMIO register to read.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..63.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..63.

  @return   The value read.

**/
UINT64
EFIAPI
S3MmioBitFieldRead64 (
  IN UINTN  Address,
  IN UINTN  StartBit,
  IN UINTN  EndBit
  );

/**
  Writes a bit field to a MMIO register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 64-bit register is returned.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..63.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..63.
  @param[in]  Value      New value of the bit field.

  @return   The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioBitFieldWrite64 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT64  Value
  );

/**
  Reads a bit field in a 64-bit MMIO register, performs a bitwise OR,
  writes the result back to the bit field in the 64-bit MMIO register, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 64-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized. Extra left bits in OrData
  are stripped.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..63.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..63.
  @param[in]  OrData     The value to OR with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioBitFieldOr64 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT64  OrData
  );

/**
  Reads a bit field in a 64-bit MMIO register, performs a bitwise AND, and
  writes the result back to the bit field in the 64-bit MMIO register, and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 64-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized. Extra left bits in AndData are
  stripped.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..63.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..63.
  @param[in]  AndData    The value to AND with the read value from the MMIO register.

  @return   The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioBitFieldAnd64 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT64  AndData
  );

/**
  Reads a bit field in a 64-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, writes the result back to the bit field in the
  64-bit MMIO register, and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise AND
  followed by a bitwise OR between the read result and the value
  specified by AndData, and writes the result to the 64-bit MMIO register
  specified by Address. The value written to the MMIO register is returned.
  This function must guarantee that all MMIO read and write operations are
  serialized. Extra left bits in both AndData and OrData are stripped.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param[in]  Address    The MMIO register to write.
  @param[in]  StartBit   The ordinal of the least significant bit in the bit field.
                         Range 0..63.
  @param[in]  EndBit     The ordinal of the most significant bit in the bit field.
                         Range 0..63.
  @param[in]  AndData    The value to AND with the read value from the MMIO register.
  @param[in]  OrData     The value to OR with the result of the AND operation.

  @return   The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioBitFieldAndThenOr64 (
  IN UINTN   Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT64  AndData,
  IN UINT64  OrData
  );

/**
  Copies data from MMIO region to system memory by using 8-bit access,
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from MMIO region specified by starting address StartAddress
  to system memory specified by Buffer by using 8-bit access. The total
  number of bytes to be copied is specified by Length. Buffer is returned.

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().


  @param[in]  StartAddress   Starting address for the MMIO region to be copied from.
  @param[in]  Length         Size in bytes of the copy.
  @param[out] Buffer         Pointer to a system memory buffer receiving the data read.

  @return   Buffer.

**/
UINT8 *
EFIAPI
S3MmioReadBuffer8 (
  IN  UINTN  StartAddress,
  IN  UINTN  Length,
  OUT UINT8  *Buffer
  );

/**
  Copies data from MMIO region to system memory by using 16-bit access,
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from MMIO region specified by starting address StartAddress
  to system memory specified by Buffer by using 16-bit access. The total
  number of bytes to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 16-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  If Length is not aligned on a 16-bit boundary, then ASSERT().
  If Buffer is not aligned on a 16-bit boundary, then ASSERT().

  @param[in]  StartAddress   Starting address for the MMIO region to be copied from.
  @param[in]  Length         Size in bytes of the copy.
  @param[out] Buffer         Pointer to a system memory buffer receiving the data read.

  @return   Buffer.

**/
UINT16 *
EFIAPI
S3MmioReadBuffer16 (
  IN  UINTN   StartAddress,
  IN  UINTN   Length,
  OUT UINT16  *Buffer
  );

/**
  Copies data from MMIO region to system memory by using 32-bit access,
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from MMIO region specified by starting address StartAddress
  to system memory specified by Buffer by using 32-bit access. The total
  number of byte to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 32-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  If Length is not aligned on a 32-bit boundary, then ASSERT().
  If Buffer is not aligned on a 32-bit boundary, then ASSERT().

  @param[in]  StartAddress   Starting address for the MMIO region to be copied from.
  @param[in]  Length         Size in bytes of the copy.
  @param[out] Buffer         Pointer to a system memory buffer receiving the data read.

  @return   Buffer.

**/
UINT32 *
EFIAPI
S3MmioReadBuffer32 (
  IN  UINTN   StartAddress,
  IN  UINTN   Length,
  OUT UINT32  *Buffer
  );

/**
  Copies data from MMIO region to system memory by using 64-bit access,
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from MMIO region specified by starting address StartAddress
  to system memory specified by Buffer by using 64-bit access. The total
  number of byte to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 64-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  If Length is not aligned on a 64-bit boundary, then ASSERT().
  If Buffer is not aligned on a 64-bit boundary, then ASSERT().

  @param[in]  StartAddress   Starting address for the MMIO region to be copied from.
  @param[in]  Length         Size in bytes of the copy.
  @param[out] Buffer         Pointer to a system memory buffer receiving the data read.

  @return   Buffer.

**/
UINT64 *
EFIAPI
S3MmioReadBuffer64 (
  IN  UINTN   StartAddress,
  IN  UINTN   Length,
  OUT UINT64  *Buffer
  );

/**
  Copies data from system memory to MMIO region by using 8-bit access,
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from system memory specified by Buffer to MMIO region specified
  by starting address StartAddress by using 8-bit access. The total number
  of byte to be copied is specified by Length. Buffer is returned.

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS -Buffer + 1), then ASSERT().


  @param[in]  StartAddress   Starting address for the MMIO region to be copied to.
  @param[in]  Length         Size in bytes of the copy.
  @param[in]  Buffer         Pointer to a system memory buffer containing the data to write.

  @return   Buffer.

**/
UINT8 *
EFIAPI
S3MmioWriteBuffer8 (
  IN  UINTN        StartAddress,
  IN  UINTN        Length,
  IN  CONST UINT8  *Buffer
  );

/**
  Copies data from system memory to MMIO region by using 16-bit access,
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from system memory specified by Buffer to MMIO region specified
  by starting address StartAddress by using 16-bit access. The total number
  of bytes to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 16-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS -Buffer + 1), then ASSERT().

  If Length is not aligned on a 16-bit boundary, then ASSERT().

  If Buffer is not aligned on a 16-bit boundary, then ASSERT().

  @param[in]  StartAddress   Starting address for the MMIO region to be copied to.
  @param[in]  Length         Size in bytes of the copy.
  @param[in]  Buffer         Pointer to a system memory buffer containing the data to write.

  @return   Buffer.

**/
UINT16 *
EFIAPI
S3MmioWriteBuffer16 (
  IN  UINTN         StartAddress,
  IN  UINTN         Length,
  IN  CONST UINT16  *Buffer
  );

/**
  Copies data from system memory to MMIO region by using 32-bit access,
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from system memory specified by Buffer to MMIO region specified
  by starting address StartAddress by using 32-bit access. The total number
  of bytes to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 32-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS -Buffer + 1), then ASSERT().

  If Length is not aligned on a 32-bit boundary, then ASSERT().

  If Buffer is not aligned on a 32-bit boundary, then ASSERT().

  @param[in]  StartAddress   Starting address for the MMIO region to be copied to.
  @param[in]  Length         Size in bytes of the copy.
  @param[in]  Buffer         Pointer to a system memory buffer containing the data to write.

  @return   Buffer.

**/
UINT32 *
EFIAPI
S3MmioWriteBuffer32 (
  IN  UINTN         StartAddress,
  IN  UINTN         Length,
  IN  CONST UINT32  *Buffer
  );

/**
  Copies data from system memory to MMIO region by using 64-bit access,
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from system memory specified by Buffer to MMIO region specified
  by starting address StartAddress by using 64-bit access. The total number
  of bytes to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 64-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS -Buffer + 1), then ASSERT().

  If Length is not aligned on a 64-bit boundary, then ASSERT().

  If Buffer is not aligned on a 64-bit boundary, then ASSERT().

  @param[in]  StartAddress   Starting address for the MMIO region to be copied to.
  @param[in]  Length         Size in bytes of the copy.
  @param[in]  Buffer         Pointer to a system memory buffer containing the data to write.

  @return   Buffer.

**/
UINT64 *
EFIAPI
S3MmioWriteBuffer64 (
  IN  UINTN         StartAddress,
  IN  UINTN         Length,
  IN  CONST UINT64  *Buffer
  );

#endif
