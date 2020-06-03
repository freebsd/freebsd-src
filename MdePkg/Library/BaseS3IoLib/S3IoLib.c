/** @file
  I/O and MMIO Library Services that do I/O and also enable the I/O operatation
  to be replayed during an S3 resume.

  Copyright (c) 2006 -2018, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

#include <Library/S3IoLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/S3BootScriptLib.h>


/**
  Saves an I/O port value to the boot script.

  This internal worker function saves an I/O port value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Width         The width of I/O port.
  @param  Port          The I/O port to write.
  @param  Buffer        The buffer containing value.

**/
VOID
InternalSaveIoWriteValueToBootScript (
  IN S3_BOOT_SCRIPT_LIB_WIDTH  Width,
  IN UINTN                  Port,
  IN VOID                   *Buffer
  )
{
  RETURN_STATUS                Status;

  Status = S3BootScriptSaveIoWrite (
             Width,
             Port,
             1,
             Buffer
             );
  ASSERT (Status == RETURN_SUCCESS);
}

/**
  Saves an 8-bit I/O port value to the boot script.

  This internal worker function saves an 8-bit I/O port value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Port          The I/O port to write.
  @param  Value         The value saved to boot script.

  @return Value.

**/
UINT8
InternalSaveIoWrite8ValueToBootScript (
  IN UINTN              Port,
  IN UINT8              Value
  )
{
  InternalSaveIoWriteValueToBootScript (S3BootScriptWidthUint8, Port, &Value);

  return Value;
}

/**
  Reads an 8-bit I/O port and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 8-bit I/O port specified by Port. The 8-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to read.

  @return The value read.

**/
UINT8
EFIAPI
S3IoRead8 (
  IN UINTN              Port
  )
{
  return InternalSaveIoWrite8ValueToBootScript (Port, IoRead8 (Port));
}

/**
  Writes an 8-bit I/O port and saves the value in the S3 script to be replayed
  on S3 resume.

  Writes the 8-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  Value         The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT8
EFIAPI
S3IoWrite8 (
  IN UINTN              Port,
  IN UINT8              Value
  )
{
  return InternalSaveIoWrite8ValueToBootScript (Port, IoWrite8 (Port, Value));
}

/**
  Reads an 8-bit I/O port, performs a bitwise OR, and writes the
  result back to the 8-bit I/O port and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 8-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 8-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  OrData        The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoOr8 (
  IN UINTN              Port,
  IN UINT8              OrData
  )
{
  return InternalSaveIoWrite8ValueToBootScript (Port, IoOr8 (Port, OrData));
}

/**
  Reads an 8-bit I/O port, performs a bitwise AND, and writes the result back
  to the 8-bit I/O port  and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 8-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 8-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  AndData       The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoAnd8 (
  IN UINTN              Port,
  IN UINT8              AndData
  )
{
  return InternalSaveIoWrite8ValueToBootScript (Port, IoAnd8 (Port, AndData));
}

/**
  Reads an 8-bit I/O port, performs a bitwise AND followed by a bitwise
  inclusive OR, and writes the result back to the 8-bit I/O port and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 8-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 8-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  AndData       The value to AND with the read value from the I/O port.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoAndThenOr8 (
  IN UINTN              Port,
  IN UINT8              AndData,
  IN UINT8              OrData
  )
{
  return InternalSaveIoWrite8ValueToBootScript (Port, IoAndThenOr8 (Port, AndData, OrData));
}

/**
  Reads a bit field of an I/O register and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in an 8-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Port          The I/O port to read.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..7.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..7.

  @return The value read.

**/
UINT8
EFIAPI
S3IoBitFieldRead8 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit
  )
{
  return InternalSaveIoWrite8ValueToBootScript (Port, IoBitFieldRead8 (Port, StartBit, EndBit));
}

/**
  Writes a bit field to an I/O register and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes Value to the bit field of the I/O register. The bit field is specified
  by the StartBit and the EndBit. All other bits in the destination I/O
  register are preserved. The value written to the I/O port is returned. Extra
  left bits in Value are stripped.

  If 8-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..7.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..7.
  @param  Value         New value of the bit field.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoBitFieldWrite8 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT8              Value
  )
{
  return InternalSaveIoWrite8ValueToBootScript (Port, IoBitFieldWrite8 (Port, StartBit, EndBit, Value));
}

/**
  Reads a bit field in an 8-bit port, performs a bitwise OR, and writes the
  result back to the bit field in the 8-bit port and saves the value in the
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..7.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..7.
  @param  OrData        The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoBitFieldOr8 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT8              OrData
  )
{
  return InternalSaveIoWrite8ValueToBootScript (Port, IoBitFieldOr8 (Port, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in an 8-bit port, performs a bitwise AND, and writes the
  result back to the bit field in the 8-bit port  and saves the value in the
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..7.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..7.
  @param  AndData       The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoBitFieldAnd8 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT8              AndData
  )
{
  return InternalSaveIoWrite8ValueToBootScript (Port, IoBitFieldAnd8 (Port, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in an 8-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  8-bit port and saves the value in the S3 script to be replayed on S3 resume.

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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..7.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..7.
  @param  AndData       The value to AND with the read value from the I/O port.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT8
EFIAPI
S3IoBitFieldAndThenOr8 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT8              AndData,
  IN UINT8              OrData
  )
{
  return InternalSaveIoWrite8ValueToBootScript (Port, IoBitFieldAndThenOr8 (Port, StartBit, EndBit, AndData, OrData));
}

/**
  Saves a 16-bit I/O port value to the boot script.

  This internal worker function saves a 16-bit I/O port value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Port          The I/O port to write.
  @param  Value         The value saved to boot script.

  @return Value.

**/
UINT16
InternalSaveIoWrite16ValueToBootScript (
  IN UINTN              Port,
  IN UINT16             Value
  )
{
  InternalSaveIoWriteValueToBootScript (S3BootScriptWidthUint16, Port, &Value);

  return Value;
}

/**
  Reads a 16-bit I/O port and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 16-bit I/O port specified by Port. The 16-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to read.

  @return The value read.

**/
UINT16
EFIAPI
S3IoRead16 (
  IN UINTN              Port
  )
{
  return InternalSaveIoWrite16ValueToBootScript (Port, IoRead16 (Port));
}

/**
  Writes a 16-bit I/O port and saves the value in the S3 script to be replayed
  on S3 resume.

  Writes the 16-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  Value         The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT16
EFIAPI
S3IoWrite16 (
  IN UINTN              Port,
  IN UINT16             Value
  )
{
  return InternalSaveIoWrite16ValueToBootScript (Port, IoWrite16 (Port, Value));
}

/**
  Reads a 16-bit I/O port, performs a bitwise OR, and writes the
  result back to the 16-bit I/O port and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the 16-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 16-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  OrData        The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoOr16 (
  IN UINTN              Port,
  IN UINT16             OrData
  )
{
  return InternalSaveIoWrite16ValueToBootScript (Port, IoOr16 (Port, OrData));
}

/**
  Reads a 16-bit I/O port, performs a bitwise AND, and writes the result back
  to the 16-bit I/O port  and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 16-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 16-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  AndData       The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoAnd16 (
  IN UINTN              Port,
  IN UINT16             AndData
  )
{
  return InternalSaveIoWrite16ValueToBootScript (Port, IoAnd16 (Port, AndData));
}

/**
  Reads a 16-bit I/O port, performs a bitwise AND followed by a bitwise
  inclusive OR, and writes the result back to the 16-bit I/O port and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 16-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 16-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  AndData       The value to AND with the read value from the I/O port.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoAndThenOr16 (
  IN UINTN              Port,
  IN UINT16             AndData,
  IN UINT16             OrData
  )
{
  return InternalSaveIoWrite16ValueToBootScript (Port, IoAndThenOr16 (Port, AndData, OrData));
}

/**
  Reads a bit field of an I/O register saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the bit field in a 16-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 16-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Port          The I/O port to read.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..15.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..15.

  @return The value read.

**/
UINT16
EFIAPI
S3IoBitFieldRead16 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit
  )
{
  return InternalSaveIoWrite16ValueToBootScript (Port, IoBitFieldRead16 (Port, StartBit, EndBit));
}

/**
  Writes a bit field to an I/O register and saves the value in the S3 script
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..15.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..15.
  @param  Value         New value of the bit field.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoBitFieldWrite16 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT16             Value
  )
{
  return InternalSaveIoWrite16ValueToBootScript (Port, IoBitFieldWrite16 (Port, StartBit, EndBit, Value));
}

/**
  Reads a bit field in a 16-bit port, performs a bitwise OR, and writes the
  result back to the bit field in the 16-bit port and saves the value in the
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..15.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..15.
  @param  OrData        The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoBitFieldOr16 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT16             OrData
  )
{
  return InternalSaveIoWrite16ValueToBootScript (Port, IoBitFieldOr16 (Port, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in a 16-bit port, performs a bitwise AND, and writes the
  result back to the bit field in the 16-bit port and saves the value in the
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..15.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..15.
  @param  AndData       The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoBitFieldAnd16 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT16             AndData
  )
{
  return InternalSaveIoWrite16ValueToBootScript (Port, IoBitFieldAnd16 (Port, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in a 16-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  16-bit port  and saves the value in the S3 script to be replayed on S3
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..15.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..15.
  @param  AndData       The value to AND with the read value from the I/O port.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT16
EFIAPI
S3IoBitFieldAndThenOr16 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT16             AndData,
  IN UINT16             OrData
  )
{
  return InternalSaveIoWrite16ValueToBootScript (Port, IoBitFieldAndThenOr16 (Port, StartBit, EndBit, AndData, OrData));
}

/**
  Saves a 32-bit I/O port value to the boot script.

  This internal worker function saves a 32-bit I/O port value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Port          The I/O port to write.
  @param  Value         The value saved to boot script.

  @return Value.

**/
UINT32
InternalSaveIoWrite32ValueToBootScript (
  IN UINTN              Port,
  IN UINT32             Value
  )
{
  InternalSaveIoWriteValueToBootScript (S3BootScriptWidthUint32, Port, &Value);

  return Value;
}

/**
  Reads a 32-bit I/O port and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 32-bit I/O port specified by Port. The 32-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to read.

  @return The value read.

**/
UINT32
EFIAPI
S3IoRead32 (
  IN UINTN              Port
  )
{
  return InternalSaveIoWrite32ValueToBootScript (Port, IoRead32 (Port));
}

/**
  Writes a 32-bit I/O port and saves the value in the S3 script to be replayed
  on S3 resume.

  Writes the 32-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  Value         The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT32
EFIAPI
S3IoWrite32 (
  IN UINTN              Port,
  IN UINT32             Value
  )
{
  return InternalSaveIoWrite32ValueToBootScript (Port, IoWrite32 (Port, Value));
}

/**
  Reads a 32-bit I/O port, performs a bitwise OR, and writes the
  result back to the 32-bit I/O port and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the 32-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 32-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  OrData        The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoOr32 (
  IN UINTN              Port,
  IN UINT32             OrData
  )
{
  return InternalSaveIoWrite32ValueToBootScript (Port, IoOr32 (Port, OrData));
}

/**
  Reads a 32-bit I/O port, performs a bitwise AND, and writes the result back
  to the 32-bit I/O port and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 32-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 32-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  AndData       The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoAnd32 (
  IN UINTN              Port,
  IN UINT32             AndData
  )
{
  return InternalSaveIoWrite32ValueToBootScript (Port, IoAnd32 (Port, AndData));
}

/**
  Reads a 32-bit I/O port, performs a bitwise AND followed by a bitwise
  inclusive OR, and writes the result back to the 32-bit I/O port and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 32-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 32-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  AndData       The value to AND with the read value from the I/O port.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoAndThenOr32 (
  IN UINTN              Port,
  IN UINT32             AndData,
  IN UINT32             OrData
  )
{
  return InternalSaveIoWrite32ValueToBootScript (Port, IoAndThenOr32 (Port, AndData, OrData));
}

/**
  Reads a bit field of an I/O register and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in a 32-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 32-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Port          The I/O port to read.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..31.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..31.

  @return The value read.

**/
UINT32
EFIAPI
S3IoBitFieldRead32 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit
  )
{
  return InternalSaveIoWrite32ValueToBootScript (Port, IoBitFieldRead32 (Port, StartBit, EndBit));
}

/**
  Writes a bit field to an I/O register and saves the value in the S3 script to
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..31.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..31.
  @param  Value         New value of the bit field.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoBitFieldWrite32 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT32             Value
  )
{
  return InternalSaveIoWrite32ValueToBootScript (Port, IoBitFieldWrite32 (Port, StartBit, EndBit, Value));
}

/**
  Reads a bit field in a 32-bit port, performs a bitwise OR, and writes the
  result back to the bit field in the 32-bit port and saves the value in the
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..31.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..31.
  @param  OrData        The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoBitFieldOr32 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT32             OrData
  )
{
  return InternalSaveIoWrite32ValueToBootScript (Port, IoBitFieldOr32 (Port, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in a 32-bit port, performs a bitwise AND, and writes the
  result back to the bit field in the 32-bit port and saves the value in the
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..31.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..31.
  @param  AndData       The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoBitFieldAnd32 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT32             AndData
  )
{
  return InternalSaveIoWrite32ValueToBootScript (Port, IoBitFieldAnd32 (Port, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in a 32-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  32-bit port and saves the value in the S3 script to be replayed on S3
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..31.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..31.
  @param  AndData       The value to AND with the read value from the I/O port.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT32
EFIAPI
S3IoBitFieldAndThenOr32 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT32             AndData,
  IN UINT32             OrData
  )
{
  return InternalSaveIoWrite32ValueToBootScript (Port, IoBitFieldAndThenOr32 (Port, StartBit, EndBit, AndData, OrData));
}

/**
  Saves a 64-bit I/O port value to the boot script.

  This internal worker function saves a 64-bit I/O port value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Port          The I/O port to write.
  @param  Value         The value saved to boot script.

  @return Value.

**/
UINT64
InternalSaveIoWrite64ValueToBootScript (
  IN UINTN              Port,
  IN UINT64             Value
  )
{
  InternalSaveIoWriteValueToBootScript (S3BootScriptWidthUint64, Port, &Value);

  return Value;
}

/**
  Reads a 64-bit I/O port and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 64-bit I/O port specified by Port. The 64-bit read value is returned.
  This function must guarantee that all I/O read and write operations are
  serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to read.

  @return The value read.

**/
UINT64
EFIAPI
S3IoRead64 (
  IN UINTN              Port
  )
{
  return InternalSaveIoWrite64ValueToBootScript (Port, IoRead64 (Port));
}

/**
  Writes a 64-bit I/O port and saves the value in the S3 script to be replayed
  on S3 resume.

  Writes the 64-bit I/O port specified by Port with the value specified by Value
  and returns Value. This function must guarantee that all I/O read and write
  operations are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  Value         The value to write to the I/O port.

  @return The value written the I/O port.

**/
UINT64
EFIAPI
S3IoWrite64 (
  IN UINTN              Port,
  IN UINT64             Value
  )
{
  return InternalSaveIoWrite64ValueToBootScript (Port, IoWrite64 (Port, Value));
}

/**
  Reads a 64-bit I/O port, performs a bitwise OR, and writes the
  result back to the 64-bit I/O port and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the 64-bit I/O port specified by Port, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 64-bit I/O port specified by Port. The value written to the I/O
  port is returned. This function must guarantee that all I/O read and write
  operations are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  OrData        The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoOr64 (
  IN UINTN              Port,
  IN UINT64             OrData
  )
{
  return InternalSaveIoWrite64ValueToBootScript (Port, IoOr64 (Port, OrData));
}

/**
  Reads a 64-bit I/O port, performs a bitwise AND, and writes the result back
  to the 64-bit I/O port and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 64-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, and writes the result to
  the 64-bit I/O port specified by Port. The value written to the I/O port is
  returned. This function must guarantee that all I/O read and write operations
  are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  AndData       The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoAnd64 (
  IN UINTN              Port,
  IN UINT64             AndData
  )
{
  return InternalSaveIoWrite64ValueToBootScript (Port, IoAnd64 (Port, AndData));
}

/**
  Reads a 64-bit I/O port, performs a bitwise AND followed by a bitwise
  inclusive OR, and writes the result back to the 64-bit I/O port and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 64-bit I/O port specified by Port, performs a bitwise AND between
  the read result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 64-bit I/O port specified by Port. The value
  written to the I/O port is returned. This function must guarantee that all
  I/O read and write operations are serialized.

  If 64-bit I/O port operations are not supported, then ASSERT().

  @param  Port          The I/O port to write.
  @param  AndData       The value to AND with the read value from the I/O port.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoAndThenOr64 (
  IN UINTN              Port,
  IN UINT64             AndData,
  IN UINT64             OrData
  )
{
  return InternalSaveIoWrite64ValueToBootScript (Port, IoAndThenOr64 (Port, AndData, OrData));
}

/**
  Reads a bit field of an I/O register and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in a 64-bit I/O register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 64-bit I/O port operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Port          The I/O port to read.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..63.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..63.

  @return The value read.

**/
UINT64
EFIAPI
S3IoBitFieldRead64 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit
  )
{
  return InternalSaveIoWrite64ValueToBootScript (Port, IoBitFieldRead64 (Port, StartBit, EndBit));
}

/**
  Writes a bit field to an I/O register and saves the value in the S3 script to
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..63.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..63.
  @param  Value         New value of the bit field.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoBitFieldWrite64 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT64             Value
  )
{
  return InternalSaveIoWrite64ValueToBootScript (Port, IoBitFieldWrite64 (Port, StartBit, EndBit, Value));
}

/**
  Reads a bit field in a 64-bit port, performs a bitwise OR, and writes the
  result back to the bit field in the 64-bit port and saves the value in the
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..63.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..63.
  @param  OrData        The value to OR with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoBitFieldOr64 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT64             OrData
  )
{
  return InternalSaveIoWrite64ValueToBootScript (Port, IoBitFieldOr64 (Port, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in a 64-bit port, performs a bitwise AND, and writes the
  result back to the bit field in the 64-bit port and saves the value in the
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..63.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..63.
  @param  AndData       The value to AND with the read value from the I/O port.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoBitFieldAnd64 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT64             AndData
  )
{
  return InternalSaveIoWrite64ValueToBootScript (Port, IoBitFieldAnd64 (Port, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in a 64-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  64-bit port and saves the value in the S3 script to be replayed on S3
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

  @param  Port          The I/O port to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..63.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..63.
  @param  AndData       The value to AND with the read value from the I/O port.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the I/O port.

**/
UINT64
EFIAPI
S3IoBitFieldAndThenOr64 (
  IN UINTN              Port,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT64             AndData,
  IN UINT64             OrData
  )
{
  return InternalSaveIoWrite64ValueToBootScript (Port, IoBitFieldAndThenOr64 (Port, StartBit, EndBit, AndData, OrData));
}

/**
  Saves an MMIO register value to the boot script.

  This internal worker function saves an MMIO register value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Width         The width of MMIO register.
  @param  Address       The MMIO register to write.
  @param  Buffer        The buffer containing value.

**/
VOID
InternalSaveMmioWriteValueToBootScript (
  IN S3_BOOT_SCRIPT_LIB_WIDTH  Width,
  IN UINTN                  Address,
  IN VOID                   *Buffer
  )
{
  RETURN_STATUS            Status;

  Status = S3BootScriptSaveMemWrite (
             Width,
             Address,
             1,
             Buffer
             );
  ASSERT (Status == RETURN_SUCCESS);
}

/**
  Saves an 8-bit MMIO register value to the boot script.

  This internal worker function saves an 8-bit MMIO register value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  Value         The value saved to boot script.

  @return Value.

**/
UINT8
InternalSaveMmioWrite8ValueToBootScript (
  IN UINTN              Address,
  IN UINT8              Value
  )
{
  InternalSaveMmioWriteValueToBootScript (S3BootScriptWidthUint8, Address, &Value);

  return Value;
}

/**
  Reads an 8-bit MMIO register and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 8-bit MMIO register specified by Address. The 8-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to read.

  @return The value read.

**/
UINT8
EFIAPI
S3MmioRead8 (
  IN UINTN              Address
  )
{
  return InternalSaveMmioWrite8ValueToBootScript (Address, MmioRead8 (Address));
}

/**
  Writes an 8-bit MMIO register and saves the value in the S3 script to be
  replayed on S3 resume.

  Writes the 8-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  Value         The value to write to the MMIO register.

  @return The value written the MMIO register.

**/
UINT8
EFIAPI
S3MmioWrite8 (
  IN UINTN              Address,
  IN UINT8              Value
  )
{
  return InternalSaveMmioWrite8ValueToBootScript (Address, MmioWrite8 (Address, Value));
}

/**
  Reads an 8-bit MMIO register, performs a bitwise OR, and writes the
  result back to the 8-bit MMIO register and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 8-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  OrData        The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioOr8 (
  IN UINTN              Address,
  IN UINT8              OrData
  )
{
  return InternalSaveMmioWrite8ValueToBootScript (Address, MmioOr8 (Address, OrData));
}

/**
  Reads an 8-bit MMIO register, performs a bitwise AND, and writes the result
  back to the 8-bit MMIO register and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 8-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  AndData       The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioAnd8 (
  IN UINTN              Address,
  IN UINT8              AndData
  )
{
  return InternalSaveMmioWrite8ValueToBootScript (Address, MmioAnd8 (Address, AndData));
}

/**
  Reads an 8-bit MMIO register, performs a bitwise AND followed by a bitwise
  inclusive OR, and writes the result back to the 8-bit MMIO register and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 8-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 8-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  AndData       The value to AND with the read value from the MMIO register.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioAndThenOr8 (
  IN UINTN              Address,
  IN UINT8              AndData,
  IN UINT8              OrData
  )
{
  return InternalSaveMmioWrite8ValueToBootScript (Address, MmioAndThenOr8 (Address, AndData, OrData));
}

/**
  Reads a bit field of a MMIO register and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in an 8-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address       MMIO register to read.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..7.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..7.

  @return The value read.

**/
UINT8
EFIAPI
S3MmioBitFieldRead8 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit
  )
{
  return InternalSaveMmioWrite8ValueToBootScript (Address, MmioBitFieldRead8 (Address, StartBit, EndBit));
}

/**
  Writes a bit field to an MMIO register and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 8-bit register is returned.

  If 8-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..7.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..7.
  @param  Value         New value of the bit field.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioBitFieldWrite8 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT8              Value
  )
{
  return InternalSaveMmioWrite8ValueToBootScript (Address, MmioBitFieldWrite8 (Address, StartBit, EndBit, Value));
}

/**
  Reads a bit field in an 8-bit MMIO register, performs a bitwise OR, and
  writes the result back to the bit field in the 8-bit MMIO register and saves
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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..7.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..7.
  @param  OrData        The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioBitFieldOr8 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT8              OrData
  )
{
  return InternalSaveMmioWrite8ValueToBootScript (Address, MmioBitFieldOr8 (Address, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in an 8-bit MMIO register, performs a bitwise AND, and
  writes the result back to the bit field in the 8-bit MMIO register and saves
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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..7.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..7.
  @param  AndData       The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioBitFieldAnd8 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT8              AndData
  )
{
  return InternalSaveMmioWrite8ValueToBootScript (Address, MmioBitFieldAnd8 (Address, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in an 8-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, and writes the result back to the bit field in the
  8-bit MMIO register  and saves the value in the S3 script to be replayed
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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..7.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..7.
  @param  AndData       The value to AND with the read value from the MMIO register.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT8
EFIAPI
S3MmioBitFieldAndThenOr8 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT8              AndData,
  IN UINT8              OrData
  )
{
  return InternalSaveMmioWrite8ValueToBootScript (Address, MmioBitFieldAndThenOr8 (Address, StartBit, EndBit, AndData, OrData));
}

/**
  Saves a 16-bit MMIO register value to the boot script.

  This internal worker function saves a 16-bit MMIO register value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  Value         The value saved to boot script.

  @return Value.

**/
UINT16
InternalSaveMmioWrite16ValueToBootScript (
  IN UINTN              Address,
  IN UINT16             Value
  )
{
  InternalSaveMmioWriteValueToBootScript (S3BootScriptWidthUint16, Address, &Value);

  return Value;
}

/**
  Reads a 16-bit MMIO register and saves the value in the S3 script to be replayed
  on S3 resume.

  Reads the 16-bit MMIO register specified by Address. The 16-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to read.

  @return The value read.

**/
UINT16
EFIAPI
S3MmioRead16 (
  IN UINTN              Address
  )
{
  return InternalSaveMmioWrite16ValueToBootScript (Address, MmioRead16 (Address));
}

/**
  Writes a 16-bit MMIO register and saves the value in the S3 script to be replayed
  on S3 resume.

  Writes the 16-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized and saves the value in the S3 script to be
  replayed on S3 resume.

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  Value         The value to write to the MMIO register.

  @return The value written the MMIO register.

**/
UINT16
EFIAPI
S3MmioWrite16 (
  IN UINTN              Address,
  IN UINT16             Value
  )
{
  return InternalSaveMmioWrite16ValueToBootScript (Address, MmioWrite16 (Address, Value));
}

/**
  Reads a 16-bit MMIO register, performs a bitwise OR, and writes the
  result back to the 16-bit MMIO register and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 16-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  OrData        The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioOr16 (
  IN UINTN              Address,
  IN UINT16             OrData
  )
{
  return InternalSaveMmioWrite16ValueToBootScript (Address, MmioOr16 (Address, OrData));
}

/**
  Reads a 16-bit MMIO register, performs a bitwise AND, and writes the result
  back to the 16-bit MMIO register and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 16-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  AndData       The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioAnd16 (
  IN UINTN              Address,
  IN UINT16             AndData
  )
{
  return InternalSaveMmioWrite16ValueToBootScript (Address, MmioAnd16 (Address, AndData));
}

/**
  Reads a 16-bit MMIO register, performs a bitwise AND followed by a bitwise
  inclusive OR, and writes the result back to the 16-bit MMIO register and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 16-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 16-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  AndData       The value to AND with the read value from the MMIO register.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioAndThenOr16 (
  IN UINTN              Address,
  IN UINT16             AndData,
  IN UINT16             OrData
  )
{
  return InternalSaveMmioWrite16ValueToBootScript (Address, MmioAndThenOr16 (Address, AndData, OrData));
}

/**
  Reads a bit field of a MMIO register and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in a 16-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address       MMIO register to read.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..15.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..15.

  @return The value read.

**/
UINT16
EFIAPI
S3MmioBitFieldRead16 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit
  )
{
  return InternalSaveMmioWrite16ValueToBootScript (Address, MmioBitFieldRead16 (Address, StartBit, EndBit));
}

/**
  Writes a bit field to a MMIO register and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 16-bit register is returned.

  If 16-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..15.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..15.
  @param  Value         New value of the bit field.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioBitFieldWrite16 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT16             Value
  )
{
  return InternalSaveMmioWrite16ValueToBootScript (Address, MmioBitFieldWrite16 (Address, StartBit, EndBit, Value));
}

/**
  Reads a bit field in a 16-bit MMIO register, performs a bitwise OR, and
  writes the result back to the bit field in the 16-bit MMIO register and
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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..15.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..15.
  @param  OrData        The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioBitFieldOr16 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT16             OrData
  )
{
  return InternalSaveMmioWrite16ValueToBootScript (Address, MmioBitFieldOr16 (Address, StartBit, EndBit, OrData));
}

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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..15.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..15.
  @param  AndData       The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioBitFieldAnd16 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT16             AndData
  )
{
  return InternalSaveMmioWrite16ValueToBootScript (Address, MmioBitFieldAnd16 (Address, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in a 16-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, and writes the result back to the bit field in the
  16-bit MMIO register and saves the value in the S3 script to be replayed
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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..15.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..15.
  @param  AndData       The value to AND with the read value from the MMIO register.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT16
EFIAPI
S3MmioBitFieldAndThenOr16 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT16             AndData,
  IN UINT16             OrData
  )
{
  return InternalSaveMmioWrite16ValueToBootScript (Address, MmioBitFieldAndThenOr16 (Address, StartBit, EndBit, AndData, OrData));
}

/**
  Saves a 32-bit MMIO register value to the boot script.

  This internal worker function saves a 32-bit MMIO register value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  Value         The value saved to boot script.

  @return Value.

**/
UINT32
InternalSaveMmioWrite32ValueToBootScript (
  IN UINTN              Address,
  IN UINT32             Value
  )
{
  InternalSaveMmioWriteValueToBootScript (S3BootScriptWidthUint32, Address, &Value);

  return Value;
}

/**
  Reads a 32-bit MMIO register saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 32-bit MMIO register specified by Address. The 32-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to read.

  @return The value read.

**/
UINT32
EFIAPI
S3MmioRead32 (
  IN UINTN              Address
  )
{
  return InternalSaveMmioWrite32ValueToBootScript (Address, MmioRead32 (Address));
}

/**
  Writes a 32-bit MMIO register and saves the value in the S3 script to be
  replayed on S3 resume.

  Writes the 32-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  Value         The value to write to the MMIO register.

  @return The value written the MMIO register.

**/
UINT32
EFIAPI
S3MmioWrite32 (
  IN UINTN              Address,
  IN UINT32             Value
  )
{
  return InternalSaveMmioWrite32ValueToBootScript (Address, MmioWrite32 (Address, Value));
}

/**
  Reads a 32-bit MMIO register, performs a bitwise OR, and writes the
  result back to the 32-bit MMIO register and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 32-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  OrData        The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioOr32 (
  IN UINTN              Address,
  IN UINT32             OrData
  )
{
  return InternalSaveMmioWrite32ValueToBootScript (Address, MmioOr32 (Address, OrData));
}

/**
  Reads a 32-bit MMIO register, performs a bitwise AND, and writes the result
  back to the 32-bit MMIO register and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 32-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  AndData       The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioAnd32 (
  IN UINTN              Address,
  IN UINT32             AndData
  )
{
  return InternalSaveMmioWrite32ValueToBootScript (Address, MmioAnd32 (Address, AndData));
}

/**
  Reads a 32-bit MMIO register, performs a bitwise AND followed by a bitwise
  inclusive OR, and writes the result back to the 32-bit MMIO register and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 32-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 32-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  AndData       The value to AND with the read value from the MMIO register.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioAndThenOr32 (
  IN UINTN              Address,
  IN UINT32             AndData,
  IN UINT32             OrData
  )
{
  return InternalSaveMmioWrite32ValueToBootScript (Address, MmioAndThenOr32 (Address, AndData, OrData));
}

/**
  Reads a bit field of a MMIO register and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads the bit field in a 32-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address       MMIO register to read.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..31.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..31.

  @return The value read.

**/
UINT32
EFIAPI
S3MmioBitFieldRead32 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit
  )
{
  return InternalSaveMmioWrite32ValueToBootScript (Address, MmioBitFieldRead32 (Address, StartBit, EndBit));
}

/**
  Writes a bit field to a MMIO register and saves the value in the S3 script
  to be replayed on S3 resume.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 32-bit register is returned.

  If 32-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..31.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..31.
  @param  Value         New value of the bit field.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioBitFieldWrite32 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT32             Value
  )
{
  return InternalSaveMmioWrite32ValueToBootScript (Address, MmioBitFieldWrite32 (Address, StartBit, EndBit, Value));
}

/**
  Reads a bit field in a 32-bit MMIO register, performs a bitwise OR, and
  writes the result back to the bit field in the 32-bit MMIO register and
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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..31.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..31.
  @param  OrData        The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioBitFieldOr32 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT32             OrData
  )
{
  return InternalSaveMmioWrite32ValueToBootScript (Address, MmioBitFieldOr32 (Address, StartBit, EndBit, OrData));
}

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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..31.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..31.
  @param  AndData       The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioBitFieldAnd32 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT32             AndData
  )
{
  return InternalSaveMmioWrite32ValueToBootScript (Address, MmioBitFieldAnd32 (Address, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in a 32-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, and writes the result back to the bit field in the
  32-bit MMIO register and saves the value in the S3 script to be replayed
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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..31.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..31.
  @param  AndData       The value to AND with the read value from the MMIO register.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT32
EFIAPI
S3MmioBitFieldAndThenOr32 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT32             AndData,
  IN UINT32             OrData
  )
{
  return InternalSaveMmioWrite32ValueToBootScript (Address, MmioBitFieldAndThenOr32 (Address, StartBit, EndBit, AndData, OrData));
}

/**
  Saves a 64-bit MMIO register value to the boot script.

  This internal worker function saves a 64-bit MMIO register value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  Value         The value saved to boot script.

  @return Value.

**/
UINT64
InternalSaveMmioWrite64ValueToBootScript (
  IN UINTN              Address,
  IN UINT64             Value
  )
{
  InternalSaveMmioWriteValueToBootScript (S3BootScriptWidthUint64, Address, &Value);

  return Value;
}

/**
  Reads a 64-bit MMIO register and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 64-bit MMIO register specified by Address. The 64-bit read value is
  returned. This function must guarantee that all MMIO read and write
  operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to read.

  @return The value read.

**/
UINT64
EFIAPI
S3MmioRead64 (
  IN UINTN              Address
  )
{
  return InternalSaveMmioWrite64ValueToBootScript (Address, MmioRead64 (Address));
}

/**
  Writes a 64-bit MMIO register and saves the value in the S3 script to be
  replayed on S3 resume.

  Writes the 64-bit MMIO register specified by Address with the value specified
  by Value and returns Value. This function must guarantee that all MMIO read
  and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  Value         The value to write to the MMIO register.

  @return The value written the MMIO register.

**/
UINT64
EFIAPI
S3MmioWrite64 (
  IN UINTN              Address,
  IN UINT64             Value
  )
{
  return InternalSaveMmioWrite64ValueToBootScript (Address, MmioWrite64 (Address, Value));
}

/**
  Reads a 64-bit MMIO register, performs a bitwise OR, and writes the
  result back to the 64-bit MMIO register and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise
  inclusive OR between the read result and the value specified by OrData, and
  writes the result to the 64-bit MMIO register specified by Address. The value
  written to the MMIO register is returned. This function must guarantee that
  all MMIO read and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  OrData        The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioOr64 (
  IN UINTN              Address,
  IN UINT64             OrData
  )
{
  return InternalSaveMmioWrite64ValueToBootScript (Address, MmioOr64 (Address, OrData));
}

/**
  Reads a 64-bit MMIO register, performs a bitwise AND, and writes the result
  back to the 64-bit MMIO register and saves the value in the S3 script to be
  replayed on S3 resume.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, and writes the
  result to the 64-bit MMIO register specified by Address. The value written to
  the MMIO register is returned. This function must guarantee that all MMIO
  read and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  AndData       The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioAnd64 (
  IN UINTN              Address,
  IN UINT64             AndData
  )
{
  return InternalSaveMmioWrite64ValueToBootScript (Address, MmioAnd64 (Address, AndData));
}

/**
  Reads a 64-bit MMIO register, performs a bitwise AND followed by a bitwise
  inclusive OR, and writes the result back to the 64-bit MMIO register and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 64-bit MMIO register specified by Address, performs a bitwise AND
  between the read result and the value specified by AndData, performs a
  bitwise OR between the result of the AND operation and the value specified by
  OrData, and writes the result to the 64-bit MMIO register specified by
  Address. The value written to the MMIO register is returned. This function
  must guarantee that all MMIO read and write operations are serialized.

  If 64-bit MMIO register operations are not supported, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  AndData       The value to AND with the read value from the MMIO register.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioAndThenOr64 (
  IN UINTN              Address,
  IN UINT64             AndData,
  IN UINT64             OrData
  )
{
  return InternalSaveMmioWrite64ValueToBootScript (Address, MmioAndThenOr64 (Address, AndData, OrData));
}

/**
  Reads a bit field of a MMIO register saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the bit field in a 64-bit MMIO register. The bit field is specified by
  the StartBit and the EndBit. The value of the bit field is returned.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address       MMIO register to read.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..63.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..63.

  @return The value read.

**/
UINT64
EFIAPI
S3MmioBitFieldRead64 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit
  )
{
  return InternalSaveMmioWrite64ValueToBootScript (Address, MmioBitFieldRead64 (Address, StartBit, EndBit));
}

/**
  Writes a bit field to a MMIO register and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes Value to the bit field of the MMIO register. The bit field is
  specified by the StartBit and the EndBit. All other bits in the destination
  MMIO register are preserved. The new value of the 64-bit register is returned.

  If 64-bit MMIO register operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..63.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..63.
  @param  Value         New value of the bit field.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioBitFieldWrite64 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT64             Value
  )
{
  return InternalSaveMmioWrite64ValueToBootScript (Address, MmioBitFieldWrite64 (Address, StartBit, EndBit, Value));
}

/**
  Reads a bit field in a 64-bit MMIO register, performs a bitwise OR, and
  writes the result back to the bit field in the 64-bit MMIO register and
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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..63.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..63.
  @param  OrData        The value to OR with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioBitFieldOr64 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT64             OrData
  )
{
  return InternalSaveMmioWrite64ValueToBootScript (Address, MmioBitFieldOr64 (Address, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in a 64-bit MMIO register, performs a bitwise AND, and
  writes the result back to the bit field in the 64-bit MMIO register and saves
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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..63.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..63.
  @param  AndData       The value to AND with the read value from the MMIO register.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioBitFieldAnd64 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT64             AndData
  )
{
  return InternalSaveMmioWrite64ValueToBootScript (Address, MmioBitFieldAnd64 (Address, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in a 64-bit MMIO register, performs a bitwise AND followed
  by a bitwise OR, and writes the result back to the bit field in the
  64-bit MMIO register and saves the value in the S3 script to be replayed
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

  @param  Address       The MMIO register to write.
  @param  StartBit      The ordinal of the least significant bit in the bit field.
                        Range 0..63.
  @param  EndBit        The ordinal of the most significant bit in the bit field.
                        Range 0..63.
  @param  AndData       The value to AND with the read value from the MMIO register.
  @param  OrData        The value to OR with the result of the AND operation.

  @return The value written back to the MMIO register.

**/
UINT64
EFIAPI
S3MmioBitFieldAndThenOr64 (
  IN UINTN              Address,
  IN UINTN              StartBit,
  IN UINTN              EndBit,
  IN UINT64             AndData,
  IN UINT64             OrData
  )
{
  return InternalSaveMmioWrite64ValueToBootScript (Address, MmioBitFieldAndThenOr64 (Address, StartBit, EndBit, AndData, OrData));
}

/**
  Copy data from MMIO region to system memory by using 8-bit access
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from MMIO region specified by starting address StartAddress
  to system memory specified by Buffer by using 8-bit access. The total
  number of byte to be copied is specified by Length. Buffer is returned.

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().


  @param  StartAddress    Starting address for the MMIO region to be copied from.
  @param  Length          Size in bytes of the copy.
  @param  Buffer          Pointer to a system memory buffer receiving the data read.

  @return Buffer

**/
UINT8 *
EFIAPI
S3MmioReadBuffer8 (
  IN  UINTN       StartAddress,
  IN  UINTN       Length,
  OUT UINT8       *Buffer
  )
{
  UINT8       *ReturnBuffer;
  RETURN_STATUS  Status;

  ReturnBuffer = MmioReadBuffer8 (StartAddress, Length, Buffer);

  Status = S3BootScriptSaveMemWrite (
             S3BootScriptWidthUint8,
             StartAddress,
             Length / sizeof (UINT8),
             ReturnBuffer
             );
  ASSERT (Status == RETURN_SUCCESS);

  return ReturnBuffer;
}

/**
  Copy data from MMIO region to system memory by using 16-bit access
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from MMIO region specified by starting address StartAddress
  to system memory specified by Buffer by using 16-bit access. The total
  number of byte to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 16-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  If Length is not aligned on a 16-bit boundary, then ASSERT().
  If Buffer is not aligned on a 16-bit boundary, then ASSERT().

  @param  StartAddress    Starting address for the MMIO region to be copied from.
  @param  Length          Size in bytes of the copy.
  @param  Buffer          Pointer to a system memory buffer receiving the data read.

  @return Buffer

**/
UINT16 *
EFIAPI
S3MmioReadBuffer16 (
  IN  UINTN       StartAddress,
  IN  UINTN       Length,
  OUT UINT16      *Buffer
  )
{
  UINT16       *ReturnBuffer;
  RETURN_STATUS   Status;

  ReturnBuffer = MmioReadBuffer16 (StartAddress, Length, Buffer);

  Status = S3BootScriptSaveMemWrite (
             S3BootScriptWidthUint16,
             StartAddress,
             Length / sizeof (UINT16),
             ReturnBuffer
             );
  ASSERT (Status == RETURN_SUCCESS);

  return ReturnBuffer;
}

/**
  Copy data from MMIO region to system memory by using 32-bit access
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from MMIO region specified by starting address StartAddress
  to system memory specified by Buffer by using 32-bit access. The total
  number of byte to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 32-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  If Length is not aligned on a 32-bit boundary, then ASSERT().
  If Buffer is not aligned on a 32-bit boundary, then ASSERT().

  @param  StartAddress    Starting address for the MMIO region to be copied from.
  @param  Length          Size in bytes of the copy.
  @param  Buffer          Pointer to a system memory buffer receiving the data read.

  @return Buffer

**/
UINT32 *
EFIAPI
S3MmioReadBuffer32 (
  IN  UINTN       StartAddress,
  IN  UINTN       Length,
  OUT UINT32      *Buffer
  )
{
  UINT32      *ReturnBuffer;
  RETURN_STATUS  Status;

  ReturnBuffer = MmioReadBuffer32 (StartAddress, Length, Buffer);

  Status = S3BootScriptSaveMemWrite (
             S3BootScriptWidthUint32,
             StartAddress,
             Length / sizeof (UINT32),
             ReturnBuffer
             );
  ASSERT (Status == RETURN_SUCCESS);

  return ReturnBuffer;
}

/**
  Copy data from MMIO region to system memory by using 64-bit access
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from MMIO region specified by starting address StartAddress
  to system memory specified by Buffer by using 64-bit access. The total
  number of byte to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 64-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  If Length is not aligned on a 64-bit boundary, then ASSERT().
  If Buffer is not aligned on a 64-bit boundary, then ASSERT().

  @param  StartAddress    Starting address for the MMIO region to be copied from.
  @param  Length          Size in bytes of the copy.
  @param  Buffer          Pointer to a system memory buffer receiving the data read.

  @return Buffer

**/
UINT64 *
EFIAPI
S3MmioReadBuffer64 (
  IN  UINTN       StartAddress,
  IN  UINTN       Length,
  OUT UINT64      *Buffer
  )
{
  UINT64      *ReturnBuffer;
  RETURN_STATUS  Status;

  ReturnBuffer = MmioReadBuffer64 (StartAddress, Length, Buffer);

  Status = S3BootScriptSaveMemWrite (
             S3BootScriptWidthUint64,
             StartAddress,
             Length / sizeof (UINT64),
             ReturnBuffer
             );
  ASSERT (Status == RETURN_SUCCESS);

  return ReturnBuffer;
}


/**
  Copy data from system memory to MMIO region by using 8-bit access
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from system memory specified by Buffer to MMIO region specified
  by starting address StartAddress by using 8-bit access. The total number
  of byte to be copied is specified by Length. Buffer is returned.

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS -Buffer + 1), then ASSERT().


  @param  StartAddress    Starting address for the MMIO region to be copied to.
  @param  Length     Size in bytes of the copy.
  @param  Buffer          Pointer to a system memory buffer containing the data to write.

  @return Buffer

**/
UINT8 *
EFIAPI
S3MmioWriteBuffer8 (
  IN  UINTN         StartAddress,
  IN  UINTN         Length,
  IN  CONST UINT8   *Buffer
  )
{
  UINT8       *ReturnBuffer;
  RETURN_STATUS  Status;

  ReturnBuffer = MmioWriteBuffer8 (StartAddress, Length, Buffer);

  Status = S3BootScriptSaveMemWrite (
             S3BootScriptWidthUint8,
             StartAddress,
             Length / sizeof (UINT8),
             ReturnBuffer
             );
  ASSERT (Status == RETURN_SUCCESS);

  return ReturnBuffer;
}

/**
  Copy data from system memory to MMIO region by using 16-bit access
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from system memory specified by Buffer to MMIO region specified
  by starting address StartAddress by using 16-bit access. The total number
  of byte to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 16-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS -Buffer + 1), then ASSERT().

  If Length is not aligned on a 16-bit boundary, then ASSERT().

  If Buffer is not aligned on a 16-bit boundary, then ASSERT().

  @param  StartAddress    Starting address for the MMIO region to be copied to.
  @param  Length     Size in bytes of the copy.
  @param  Buffer          Pointer to a system memory buffer containing the data to write.

  @return Buffer

**/
UINT16 *
EFIAPI
S3MmioWriteBuffer16 (
  IN  UINTN        StartAddress,
  IN  UINTN        Length,
  IN  CONST UINT16 *Buffer
  )
{
  UINT16      *ReturnBuffer;
  RETURN_STATUS  Status;

  ReturnBuffer = MmioWriteBuffer16 (StartAddress, Length, Buffer);

  Status = S3BootScriptSaveMemWrite (
             S3BootScriptWidthUint16,
             StartAddress,
             Length / sizeof (UINT16),
             ReturnBuffer
             );
  ASSERT (Status == RETURN_SUCCESS);

  return ReturnBuffer;
}


/**
  Copy data from system memory to MMIO region by using 32-bit access
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from system memory specified by Buffer to MMIO region specified
  by starting address StartAddress by using 32-bit access. The total number
  of byte to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 32-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS -Buffer + 1), then ASSERT().

  If Length is not aligned on a 32-bit boundary, then ASSERT().

  If Buffer is not aligned on a 32-bit boundary, then ASSERT().

  @param  StartAddress    Starting address for the MMIO region to be copied to.
  @param  Length     Size in bytes of the copy.
  @param  Buffer          Pointer to a system memory buffer containing the data to write.

  @return Buffer

**/
UINT32 *
EFIAPI
S3MmioWriteBuffer32 (
  IN  UINTN        StartAddress,
  IN  UINTN        Length,
  IN  CONST UINT32 *Buffer
  )
{
  UINT32      *ReturnBuffer;
  RETURN_STATUS  Status;

  ReturnBuffer = MmioWriteBuffer32 (StartAddress, Length, Buffer);

  Status = S3BootScriptSaveMemWrite (
             S3BootScriptWidthUint32,
             StartAddress,
             Length / sizeof (UINT32),
             ReturnBuffer
             );
  ASSERT (Status == RETURN_SUCCESS);

  return ReturnBuffer;
}

/**
  Copy data from system memory to MMIO region by using 64-bit access
  and saves the value in the S3 script to be replayed on S3 resume.

  Copy data from system memory specified by Buffer to MMIO region specified
  by starting address StartAddress by using 64-bit access. The total number
  of byte to be copied is specified by Length. Buffer is returned.

  If StartAddress is not aligned on a 64-bit boundary, then ASSERT().

  If Length is greater than (MAX_ADDRESS - StartAddress + 1), then ASSERT().
  If Length is greater than (MAX_ADDRESS -Buffer + 1), then ASSERT().

  If Length is not aligned on a 64-bit boundary, then ASSERT().

  If Buffer is not aligned on a 64-bit boundary, then ASSERT().

  @param  StartAddress    Starting address for the MMIO region to be copied to.
  @param  Length     Size in bytes of the copy.
  @param  Buffer          Pointer to a system memory buffer containing the data to write.

  @return Buffer

**/
UINT64 *
EFIAPI
S3MmioWriteBuffer64 (
  IN  UINTN        StartAddress,
  IN  UINTN        Length,
  IN  CONST UINT64 *Buffer
  )
{
  UINT64      *ReturnBuffer;
  RETURN_STATUS  Status;

  ReturnBuffer = MmioWriteBuffer64 (StartAddress, Length, Buffer);

  Status = S3BootScriptSaveMemWrite (
             S3BootScriptWidthUint64,
             StartAddress,
             Length / sizeof (UINT64),
             ReturnBuffer
             );
  ASSERT (Status == RETURN_SUCCESS);

  return ReturnBuffer;
}

