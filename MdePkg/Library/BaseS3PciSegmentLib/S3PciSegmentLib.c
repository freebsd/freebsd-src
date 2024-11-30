/** @file
  The multiple segments PCI configuration Library Services that carry out
  PCI configuration and enable the PCI operations to be replayed during an
  S3 resume. This library class maps directly on top of the PciSegmentLib class.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

#include <Library/DebugLib.h>
#include <Library/S3BootScriptLib.h>
#include <Library/PciSegmentLib.h>

/**
  Macro that converts address in PciSegmentLib format to the new address that can be pass
  to the S3 Boot Script Library functions. The Segment is dropped.

  @param Address  Address in PciSegmentLib format.

  @retval New address that can be pass to the S3 Boot Script Library functions.
**/
#define PCI_SEGMENT_LIB_ADDRESS_TO_S3_BOOT_SCRIPT_PCI_ADDRESS(Address) \
          ((((UINT32)(Address) >> 20) & 0xff) << 24) | \
          ((((UINT32)(Address) >> 15) & 0x1f) << 16) | \
          ((((UINT32)(Address) >> 12) & 0x07) <<  8) | \
          LShiftU64 ((Address) & 0xfff, 32)    // Always put Register in high four bytes.

/**
  Saves a PCI configuration value to the boot script.

  This internal worker function saves a PCI configuration value in
  the S3 script to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Width   The width of PCI configuration.
  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Buffer  The buffer containing value.

**/
VOID
InternalSavePciSegmentWriteValueToBootScript (
  IN S3_BOOT_SCRIPT_LIB_WIDTH  Width,
  IN UINT64                    Address,
  IN VOID                      *Buffer
  )
{
  RETURN_STATUS  Status;

  Status = S3BootScriptSavePciCfg2Write (
             Width,
             RShiftU64 ((Address), 32) & 0xffff,
             PCI_SEGMENT_LIB_ADDRESS_TO_S3_BOOT_SCRIPT_PCI_ADDRESS (Address),
             1,
             Buffer
             );
  ASSERT_RETURN_ERROR (Status);
}

/**
  Saves an 8-bit PCI configuration value to the boot script.

  This internal worker function saves an 8-bit PCI configuration value in
  the S3 script to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Value   The value saved to boot script.

  @return Value.

**/
UINT8
InternalSavePciSegmentWrite8ValueToBootScript (
  IN UINT64  Address,
  IN UINT8   Value
  )
{
  InternalSavePciSegmentWriteValueToBootScript (S3BootScriptWidthUint8, Address, &Value);

  return Value;
}

/**
  Reads an 8-bit PCI configuration register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads and returns the 8-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.

  @return The 8-bit PCI configuration register specified by Address.

**/
UINT8
EFIAPI
S3PciSegmentRead8 (
  IN UINT64  Address
  )
{
  return InternalSavePciSegmentWrite8ValueToBootScript (Address, PciSegmentRead8 (Address));
}

/**
  Writes an 8-bit PCI configuration register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes the 8-bit PCI configuration register specified by Address with the value specified by Value.
  Value is returned.  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().

  @param  Address     Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  Value       The value to write.

  @return The value written to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciSegmentWrite8 (
  IN UINT64  Address,
  IN UINT8   Value
  )
{
  return InternalSavePciSegmentWrite8ValueToBootScript (Address, PciSegmentWrite8 (Address, Value));
}

/**
  Performs a bitwise OR of an 8-bit PCI configuration register with an 8-bit value, and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address,
  performs a bitwise OR between the read result and the value specified by OrData,
  and writes the result to the 8-bit PCI configuration register specified by Address.
  The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  OrData    The value to OR with the PCI configuration register.

  @return The value written to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciSegmentOr8 (
  IN UINT64  Address,
  IN UINT8   OrData
  )
{
  return InternalSavePciSegmentWrite8ValueToBootScript (Address, PciSegmentOr8 (Address, OrData));
}

/**
  Performs a bitwise AND of an 8-bit PCI configuration register with an 8-bit value, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address,
  performs a bitwise AND between the read result and the value specified by AndData,
  and writes the result to the 8-bit PCI configuration register specified by Address.
  The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are serialized.
  If any reserved bits in Address are set, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  AndData   The value to AND with the PCI configuration register.

  @return The value written to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciSegmentAnd8 (
  IN UINT64  Address,
  IN UINT8   AndData
  )
{
  return InternalSavePciSegmentWrite8ValueToBootScript (Address, PciSegmentAnd8 (Address, AndData));
}

/**
  Performs a bitwise AND of an 8-bit PCI configuration register with an 8-bit value,
  followed a bitwise OR with another 8-bit value, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address,
  performs a bitwise AND between the read result and the value specified by AndData,
  performs a bitwise OR between the result of the AND operation and the value specified by OrData,
  and writes the result to the 8-bit PCI configuration register specified by Address.
  The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  AndData    The value to AND with the PCI configuration register.
  @param  OrData    The value to OR with the PCI configuration register.

  @return The value written to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciSegmentAndThenOr8 (
  IN UINT64  Address,
  IN UINT8   AndData,
  IN UINT8   OrData
  )
{
  return InternalSavePciSegmentWrite8ValueToBootScript (Address, PciSegmentAndThenOr8 (Address, AndData, OrData));
}

/**
  Reads a bit field of a PCI configuration register, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the bit field in an 8-bit PCI configuration register. The bit field is
  specified by the StartBit and the EndBit. The value of the bit field is
  returned.

  If any reserved bits in Address are set, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address   PCI configuration register to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.

  @return The value of the bit field read from the PCI configuration register.

**/
UINT8
EFIAPI
S3PciSegmentBitFieldRead8 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit
  )
{
  return InternalSavePciSegmentWrite8ValueToBootScript (Address, PciSegmentBitFieldRead8 (Address, StartBit, EndBit));
}

/**
  Writes a bit field to a PCI configuration register, and saves the value in
  the S3 script to be replayed on S3 resume.

  Writes Value to the bit field of the PCI configuration register. The bit
  field is specified by the StartBit and the EndBit. All other bits in the
  destination PCI configuration register are preserved. The new value of the
  8-bit register is returned.

  If any reserved bits in Address are set, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  Value     New value of the bit field.

  @return The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciSegmentBitFieldWrite8 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT8   Value
  )
{
  return InternalSavePciSegmentWrite8ValueToBootScript (Address, PciSegmentBitFieldWrite8 (Address, StartBit, EndBit, Value));
}

/**
  Reads a bit field in an 8-bit PCI configuration, performs a bitwise OR, writes
  the result back to the bit field in the 8-bit port, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 8-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized. Extra left bits in OrData are stripped.

  If any reserved bits in Address are set, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  OrData    The value to OR with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciSegmentBitFieldOr8 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT8   OrData
  )
{
  return InternalSavePciSegmentWrite8ValueToBootScript (Address, PciSegmentBitFieldOr8 (Address, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in an 8-bit PCI configuration register, performs a bitwise
  AND, writes the result back to the bit field in the 8-bit register, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 8-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized. Extra left bits in AndData are stripped.

  If any reserved bits in Address are set, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciSegmentBitFieldAnd8 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT8   AndData
  )
{
  return InternalSavePciSegmentWrite8ValueToBootScript (Address, PciSegmentBitFieldAnd8 (Address, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in an 8-bit port, performs a bitwise AND followed by a
  bitwise OR, writes the result back to the bit field in the 8-bit port,
  and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise AND followed by a bitwise OR between the read result and
  the value specified by AndData, and writes the result to the 8-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized. Extra left bits in both AndData and
  OrData are stripped.

  If any reserved bits in Address are set, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with the PCI configuration register.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciSegmentBitFieldAndThenOr8 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT8   AndData,
  IN UINT8   OrData
  )
{
  return InternalSavePciSegmentWrite8ValueToBootScript (Address, PciSegmentBitFieldAndThenOr8 (Address, StartBit, EndBit, AndData, OrData));
}

/**
  Saves a 16-bit PCI configuration value to the boot script.

  This internal worker function saves a 16-bit PCI configuration value in
  the S3 script to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Value   The value saved to boot script.

  @return Value.

**/
UINT16
InternalSavePciSegmentWrite16ValueToBootScript (
  IN UINT64  Address,
  IN UINT16  Value
  )
{
  InternalSavePciSegmentWriteValueToBootScript (S3BootScriptWidthUint16, Address, &Value);

  return Value;
}

/**
  Reads a 16-bit PCI configuration register, and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads and returns the 16-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.

  @return The 16-bit PCI configuration register specified by Address.

**/
UINT16
EFIAPI
S3PciSegmentRead16 (
  IN UINT64  Address
  )
{
  return InternalSavePciSegmentWrite16ValueToBootScript (Address, PciSegmentRead16 (Address));
}

/**
  Writes a 16-bit PCI configuration register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes the 16-bit PCI configuration register specified by Address with the value specified by Value.
  Value is returned.  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address     Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  Value       The value to write.

  @return The parameter of Value.

**/
UINT16
EFIAPI
S3PciSegmentWrite16 (
  IN UINT64  Address,
  IN UINT16  Value
  )
{
  return InternalSavePciSegmentWrite16ValueToBootScript (Address, PciSegmentWrite16 (Address, Value));
}

/**
  Performs a bitwise OR of a 16-bit PCI configuration register with a 16-bit
  value, and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by OrData, and
  writes the result to the 16-bit PCI configuration register specified by Address.
  The value written to the PCI configuration register is returned. This function
  must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Segment, Bus, Device, Function and
                  Register.
  @param  OrData  The value to OR with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciSegmentOr16 (
  IN UINT64  Address,
  IN UINT16  OrData
  )
{
  return InternalSavePciSegmentWrite16ValueToBootScript (Address, PciSegmentOr16 (Address, OrData));
}

/**
  Performs a bitwise AND of a 16-bit PCI configuration register with a 16-bit value, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address,
  performs a bitwise AND between the read result and the value specified by AndData,
  and writes the result to the 16-bit PCI configuration register specified by Address.
  The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  AndData   The value to AND with the PCI configuration register.

  @return The value written to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciSegmentAnd16 (
  IN UINT64  Address,
  IN UINT16  AndData
  )
{
  return InternalSavePciSegmentWrite16ValueToBootScript (Address, PciSegmentAnd16 (Address, AndData));
}

/**
  Performs a bitwise AND of a 16-bit PCI configuration register with a 16-bit value,
  followed a bitwise OR with another 16-bit value, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address,
  performs a bitwise AND between the read result and the value specified by AndData,
  performs a bitwise OR between the result of the AND operation and the value specified by OrData,
  and writes the result to the 16-bit PCI configuration register specified by Address.
  The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  AndData   The value to AND with the PCI configuration register.
  @param  OrData    The value to OR with the PCI configuration register.

  @return The value written to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciSegmentAndThenOr16 (
  IN UINT64  Address,
  IN UINT16  AndData,
  IN UINT16  OrData
  )
{
  return InternalSavePciSegmentWrite16ValueToBootScript (Address, PciSegmentAndThenOr16 (Address, AndData, OrData));
}

/**
  Reads a bit field of a PCI configuration register, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the bit field in a 16-bit PCI configuration register. The bit field is
  specified by the StartBit and the EndBit. The value of the bit field is
  returned.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address   PCI configuration register to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.

  @return The value of the bit field read from the PCI configuration register.

**/
UINT16
EFIAPI
S3PciSegmentBitFieldRead16 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit
  )
{
  return InternalSavePciSegmentWrite16ValueToBootScript (Address, PciSegmentBitFieldRead16 (Address, StartBit, EndBit));
}

/**
  Writes a bit field to a PCI configuration register, and saves the value in
  the S3 script to be replayed on S3 resume.

  Writes Value to the bit field of the PCI configuration register. The bit
  field is specified by the StartBit and the EndBit. All other bits in the
  destination PCI configuration register are preserved. The new value of the
  16-bit register is returned.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  Value     New value of the bit field.

  @return The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciSegmentBitFieldWrite16 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  Value
  )
{
  return InternalSavePciSegmentWrite16ValueToBootScript (Address, PciSegmentBitFieldWrite16 (Address, StartBit, EndBit, Value));
}

/**
  Reads a bit field in a 16-bit PCI configuration, performs a bitwise OR, writes
  the result back to the bit field in the 16-bit port, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 16-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized. Extra left bits in OrData are stripped.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  OrData    The value to OR with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciSegmentBitFieldOr16 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  OrData
  )
{
  return InternalSavePciSegmentWrite16ValueToBootScript (Address, PciSegmentBitFieldOr16 (Address, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in a 16-bit PCI configuration register, performs a bitwise
  AND, writes the result back to the bit field in the 16-bit register, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 16-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized. Extra left bits in AndData are stripped.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciSegmentBitFieldAnd16 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  AndData
  )
{
  return InternalSavePciSegmentWrite16ValueToBootScript (Address, PciSegmentBitFieldAnd16 (Address, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in a 16-bit port, performs a bitwise AND followed by a
  bitwise OR, writes the result back to the bit field in the 16-bit port,
  and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise AND followed by a bitwise OR between the read result and
  the value specified by AndData, and writes the result to the 16-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized. Extra left bits in both AndData and
  OrData are stripped.

  If any reserved bits in Address are set, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with the PCI configuration register.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciSegmentBitFieldAndThenOr16 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT16  AndData,
  IN UINT16  OrData
  )
{
  return InternalSavePciSegmentWrite16ValueToBootScript (Address, PciSegmentBitFieldAndThenOr16 (Address, StartBit, EndBit, AndData, OrData));
}

/**
  Saves a 32-bit PCI configuration value to the boot script.

  This internal worker function saves a 32-bit PCI configuration value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Value   The value saved to boot script.

  @return Value.

**/
UINT32
InternalSavePciSegmentWrite32ValueToBootScript (
  IN UINT64  Address,
  IN UINT32  Value
  )
{
  InternalSavePciSegmentWriteValueToBootScript (S3BootScriptWidthUint32, Address, &Value);

  return Value;
}

/**
  Reads a 32-bit PCI configuration register, and saves the value in the S3 script
  to be replayed on S3 resume.

  Reads and returns the 32-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.

  @return The 32-bit PCI configuration register specified by Address.

**/
UINT32
EFIAPI
S3PciSegmentRead32 (
  IN UINT64  Address
  )
{
  return InternalSavePciSegmentWrite32ValueToBootScript (Address, PciSegmentRead32 (Address));
}

/**
  Writes a 32-bit PCI configuration register, and saves the value in the S3 script to
  be replayed on S3 resume.

  Writes the 32-bit PCI configuration register specified by Address with the value specified by Value.
  Value is returned.  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address     Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  Value       The value to write.

  @return The parameter of Value.

**/
UINT32
EFIAPI
S3PciSegmentWrite32 (
  IN UINT64  Address,
  IN UINT32  Value
  )
{
  return InternalSavePciSegmentWrite32ValueToBootScript (Address, PciSegmentWrite32 (Address, Value));
}

/**
  Performs a bitwise OR of a 32-bit PCI configuration register with a 32-bit
  value, and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by OrData, and
  writes the result to the 32-bit PCI configuration register specified by Address.
  The value written to the PCI configuration register is returned. This function
  must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and
                    Register.
  @param  OrData    The value to OR with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciSegmentOr32 (
  IN UINT64  Address,
  IN UINT32  OrData
  )
{
  return InternalSavePciSegmentWrite32ValueToBootScript (Address, PciSegmentOr32 (Address, OrData));
}

/**
  Performs a bitwise AND of a 32-bit PCI configuration register with a 32-bit value, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address,
  performs a bitwise AND between the read result and the value specified by AndData,
  and writes the result to the 32-bit PCI configuration register specified by Address.
  The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  AndData   The value to AND with the PCI configuration register.

  @return The value written to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciSegmentAnd32 (
  IN UINT64  Address,
  IN UINT32  AndData
  )
{
  return InternalSavePciSegmentWrite32ValueToBootScript (Address, PciSegmentAnd32 (Address, AndData));
}

/**
  Performs a bitwise AND of a 32-bit PCI configuration register with a 32-bit value,
  followed a bitwise OR with another 32-bit value, and saves the value in the S3 script to
  be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address,
  performs a bitwise AND between the read result and the value specified by AndData,
  performs a bitwise OR between the result of the AND operation and the value specified by OrData,
  and writes the result to the 32-bit PCI configuration register specified by Address.
  The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are serialized.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  AndData   The value to AND with the PCI configuration register.
  @param  OrData    The value to OR with the PCI configuration register.

  @return The value written to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciSegmentAndThenOr32 (
  IN UINT64  Address,
  IN UINT32  AndData,
  IN UINT32  OrData
  )
{
  return InternalSavePciSegmentWrite32ValueToBootScript (Address, PciSegmentAndThenOr32 (Address, AndData, OrData));
}

/**
  Reads a bit field of a PCI configuration register, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the bit field in a 32-bit PCI configuration register. The bit field is
  specified by the StartBit and the EndBit. The value of the bit field is
  returned.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Address   PCI configuration register to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.

  @return The value of the bit field read from the PCI configuration register.

**/
UINT32
EFIAPI
S3PciSegmentBitFieldRead32 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit
  )
{
  return InternalSavePciSegmentWrite32ValueToBootScript (Address, PciSegmentBitFieldRead32 (Address, StartBit, EndBit));
}

/**
  Writes a bit field to a PCI configuration register, and saves the value in
  the S3 script to be replayed on S3 resume.

  Writes Value to the bit field of the PCI configuration register. The bit
  field is specified by the StartBit and the EndBit. All other bits in the
  destination PCI configuration register are preserved. The new value of the
  32-bit register is returned.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  Value     New value of the bit field.

  @return The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciSegmentBitFieldWrite32 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  Value
  )
{
  return InternalSavePciSegmentWrite32ValueToBootScript (Address, PciSegmentBitFieldWrite32 (Address, StartBit, EndBit, Value));
}

/**
  Reads a bit field in a 32-bit PCI configuration, performs a bitwise OR, writes
  the result back to the bit field in the 32-bit port, and saves the value in the
  S3 script to be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 32-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized. Extra left bits in OrData are stripped.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  OrData    The value to OR with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciSegmentBitFieldOr32 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  OrData
  )
{
  return InternalSavePciSegmentWrite32ValueToBootScript (Address, PciSegmentBitFieldOr32 (Address, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in a 32-bit PCI configuration register, performs a bitwise
  AND, and writes the result back to the bit field in the 32-bit register, and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 32-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized. Extra left bits in AndData are stripped.

  If any reserved bits in Address are set, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   Address that encodes the PCI Segment, Bus, Device, Function, and Register.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciSegmentBitFieldAnd32 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  AndData
  )
{
  return InternalSavePciSegmentWrite32ValueToBootScript (Address, PciSegmentBitFieldAnd32 (Address, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in a 32-bit port, performs a bitwise AND followed by a
  bitwise OR, writes the result back to the bit field in the 32-bit port,
  and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise AND followed by a bitwise OR between the read result and
  the value specified by AndData, and writes the result to the 32-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized. Extra left bits in both AndData and
  OrData are stripped.

  If any reserved bits in Address are set, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the PCI configuration register.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciSegmentBitFieldAndThenOr32 (
  IN UINT64  Address,
  IN UINTN   StartBit,
  IN UINTN   EndBit,
  IN UINT32  AndData,
  IN UINT32  OrData
  )
{
  return InternalSavePciSegmentWrite32ValueToBootScript (Address, PciSegmentBitFieldAndThenOr32 (Address, StartBit, EndBit, AndData, OrData));
}

/**
  Reads a range of PCI configuration registers into a caller supplied buffer,
  and saves the value in the S3 script to be replayed on S3 resume.

  Reads the range of PCI configuration registers specified by StartAddress and
  Size into the buffer specified by Buffer. This function only allows the PCI
  configuration registers from a single PCI function to be read. Size is
  returned. When possible 32-bit PCI configuration read cycles are used to read
  from StartAdress to StartAddress + Size. Due to alignment restrictions, 8-bit
  and 16-bit PCI configuration read cycles may be used at the beginning and the
  end of the range.

  If any reserved bits in StartAddress are set, then ASSERT().
  If ((StartAddress & 0xFFF) + Size) > 0x1000, then ASSERT().
  If Size > 0 and Buffer is NULL, then ASSERT().

  @param  StartAddress  Starting address that encodes the PCI Segment, Bus, Device,
                        Function and Register.
  @param  Size          Size in bytes of the transfer.
  @param  Buffer        Pointer to a buffer receiving the data read.

  @return Size

**/
UINTN
EFIAPI
S3PciSegmentReadBuffer (
  IN  UINT64  StartAddress,
  IN  UINTN   Size,
  OUT VOID    *Buffer
  )
{
  RETURN_STATUS  Status;

  Status = S3BootScriptSavePciCfg2Write (
             S3BootScriptWidthUint8,
             RShiftU64 (StartAddress, 32) & 0xffff,
             PCI_SEGMENT_LIB_ADDRESS_TO_S3_BOOT_SCRIPT_PCI_ADDRESS (StartAddress),
             PciSegmentReadBuffer (StartAddress, Size, Buffer),
             Buffer
             );
  ASSERT_RETURN_ERROR (Status);
  return Size;
}

/**
  Copies the data in a caller supplied buffer to a specified range of PCI
  configuration space, and saves the value in the S3 script to be replayed on S3
  resume.

  Writes the range of PCI configuration registers specified by StartAddress and
  Size from the buffer specified by Buffer. This function only allows the PCI
  configuration registers from a single PCI function to be written. Size is
  returned. When possible 32-bit PCI configuration write cycles are used to
  write from StartAdress to StartAddress + Size. Due to alignment restrictions,
  8-bit and 16-bit PCI configuration write cycles may be used at the beginning
  and the end of the range.

  If any reserved bits in StartAddress are set, then ASSERT().
  If ((StartAddress & 0xFFF) + Size) > 0x1000, then ASSERT().
  If Size > 0 and Buffer is NULL, then ASSERT().

  @param  StartAddress  Starting address that encodes the PCI Segment, Bus, Device,
                        Function and Register.
  @param  Size          Size in bytes of the transfer.
  @param  Buffer        Pointer to a buffer containing the data to write.

  @return The parameter of Size.

**/
UINTN
EFIAPI
S3PciSegmentWriteBuffer (
  IN UINT64  StartAddress,
  IN UINTN   Size,
  IN VOID    *Buffer
  )
{
  RETURN_STATUS  Status;

  Status = S3BootScriptSavePciCfg2Write (
             S3BootScriptWidthUint8,
             RShiftU64 (StartAddress, 32) & 0xffff,
             PCI_SEGMENT_LIB_ADDRESS_TO_S3_BOOT_SCRIPT_PCI_ADDRESS (StartAddress),
             PciSegmentWriteBuffer (StartAddress, Size, Buffer),
             Buffer
             );
  ASSERT_RETURN_ERROR (Status);
  return Size;
}
