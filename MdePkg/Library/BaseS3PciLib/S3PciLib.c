/** @file
  PCI configuration Library Services that do PCI configuration and also enable
  the PCI operations to be replayed during an S3 resume. This library class
  maps directly on top of the PciLib class.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include <Base.h>

#include <Library/DebugLib.h>
#include <Library/S3BootScriptLib.h>
#include <Library/PciLib.h>
#include <Library/S3PciLib.h>

#define PCILIB_TO_COMMON_ADDRESS(Address) \
        ((((UINTN) ((Address>>20) & 0xff)) << 24) + (((UINTN) ((Address>>15) & 0x1f)) << 16) + (((UINTN) ((Address>>12) & 0x07)) << 8) + ((UINTN) (Address & 0xfff )))

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
InternalSavePciWriteValueToBootScript (
  IN S3_BOOT_SCRIPT_LIB_WIDTH  Width,
  IN UINTN                  Address,
  IN VOID                   *Buffer
  )
{
  RETURN_STATUS                Status;

  Status = S3BootScriptSavePciCfgWrite (
             Width,
             PCILIB_TO_COMMON_ADDRESS(Address),
             1,
             Buffer
             );
  ASSERT (Status == RETURN_SUCCESS);
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
InternalSavePciWrite8ValueToBootScript (
  IN UINTN              Address,
  IN UINT8              Value
  )
{
  InternalSavePciWriteValueToBootScript (S3BootScriptWidthUint8, Address, &Value);

  return Value;
}

/**
  Reads an 8-bit PCI configuration register and saves the value in the S3
  script to be replayed on S3 resume.

  Reads and returns the 8-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.

  @return The read value from the PCI configuration register.

**/
UINT8
EFIAPI
S3PciRead8 (
  IN UINTN                     Address
  )
{
  return InternalSavePciWrite8ValueToBootScript (Address, PciRead8 (Address));
}

/**
  Writes an 8-bit PCI configuration register and saves the value in the S3
  script to be replayed on S3 resume.

  Writes the 8-bit PCI configuration register specified by Address with the
  value specified by Value. Value is returned. This function must guarantee
  that all PCI read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Value   The value to write.

  @return The value written to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciWrite8 (
  IN UINTN                     Address,
  IN UINT8                     Value
  )
{
  return InternalSavePciWrite8ValueToBootScript (Address, PciWrite8 (Address, Value));
}

/**
  Performs a bitwise OR of an 8-bit PCI configuration register with
  an 8-bit value and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 8-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  OrData  The value to OR with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciOr8 (
  IN UINTN                     Address,
  IN UINT8                     OrData
  )
{
  return InternalSavePciWrite8ValueToBootScript (Address, PciOr8 (Address, OrData));
}

/**
  Performs a bitwise AND of an 8-bit PCI configuration register with an 8-bit
  value and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 8-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciAnd8 (
  IN UINTN                     Address,
  IN UINT8                     AndData
  )
{
  return InternalSavePciWrite8ValueToBootScript (Address, PciAnd8 (Address, AndData));
}

/**
  Performs a bitwise AND of an 8-bit PCI configuration register with an 8-bit
  value, followed a  bitwise OR with another 8-bit value and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData,
  performs a bitwise OR between the result of the AND operation and
  the value specified by OrData, and writes the result to the 8-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the PCI configuration register.

**/
UINT8
EFIAPI
S3PciAndThenOr8 (
  IN UINTN                     Address,
  IN UINT8                     AndData,
  IN UINT8                     OrData
  )
{
  return InternalSavePciWrite8ValueToBootScript (Address, PciAndThenOr8 (Address, AndData, OrData));
}

/**
  Reads a bit field of a PCI configuration register and saves the value in
  the S3 script to be replayed on S3 resume.

  Reads the bit field in an 8-bit PCI configuration register. The bit field is
  specified by the StartBit and the EndBit. The value of the bit field is
  returned.

  If Address > 0x0FFFFFFF, then ASSERT().
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
S3PciBitFieldRead8 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit
  )
{
  return InternalSavePciWrite8ValueToBootScript (Address, PciBitFieldRead8 (Address, StartBit, EndBit));
}

/**
  Writes a bit field to a PCI configuration register and saves the value in
  the S3 script to be replayed on S3 resume.

  Writes Value to the bit field of the PCI configuration register. The bit
  field is specified by the StartBit and the EndBit. All other bits in the
  destination PCI configuration register are preserved. The new value of the
  8-bit register is returned.

  If Address > 0x0FFFFFFF, then ASSERT().
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
S3PciBitFieldWrite8 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT8                     Value
  )
{
  return InternalSavePciWrite8ValueToBootScript (Address, PciBitFieldWrite8 (Address, StartBit, EndBit, Value));
}

/**
  Reads a bit field in an 8-bit PCI configuration, performs a bitwise OR, and
  writes the result back to the bit field in the 8-bit port and saves the value
  in the S3 script to be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 8-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized. Extra left bits in OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
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
S3PciBitFieldOr8 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT8                     OrData
  )
{
  return InternalSavePciWrite8ValueToBootScript (Address, PciBitFieldOr8 (Address, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in an 8-bit PCI configuration register, performs a bitwise
  AND, and writes the result back to the bit field in the 8-bit register and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 8-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized. Extra left bits in AndData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
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
S3PciBitFieldAnd8 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT8                     AndData
  )
{
  return InternalSavePciWrite8ValueToBootScript (Address, PciBitFieldAnd8 (Address, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in an 8-bit Address, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  8-bit port and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 8-bit PCI configuration register specified by Address, performs a
  bitwise AND followed by a bitwise OR between the read result and
  the value specified by AndData, and writes the result to the 8-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized. Extra left bits in both AndData and
  OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
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
S3PciBitFieldAndThenOr8 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT8                     AndData,
  IN UINT8                     OrData
  )
{
  return InternalSavePciWrite8ValueToBootScript (Address, PciBitFieldAndThenOr8 (Address, StartBit, EndBit, AndData, OrData));
}

/**
  Saves a 16-bit PCI configuration value to the boot script.

  This internal worker function saves a 16-bit PCI configuration value in
  the S3 script to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Value   The value to write.

  @return Value.

**/
UINT16
InternalSavePciWrite16ValueToBootScript (
  IN UINTN              Address,
  IN UINT16             Value
  )
{
  InternalSavePciWriteValueToBootScript (S3BootScriptWidthUint16, Address, &Value);

  return Value;
}

/**
  Reads a 16-bit PCI configuration register and saves the value in the S3
  script to be replayed on S3 resume.

  Reads and returns the 16-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.

  @return The read value from the PCI configuration register.

**/
UINT16
EFIAPI
S3PciRead16 (
  IN UINTN                     Address
  )
{
  return InternalSavePciWrite16ValueToBootScript (Address, PciRead16 (Address));
}

/**
  Writes a 16-bit PCI configuration register and saves the value in the S3
  script to be replayed on S3 resume.

  Writes the 16-bit PCI configuration register specified by Address with the
  value specified by Value. Value is returned. This function must guarantee
  that all PCI read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Value   The value to write.

  @return The value written to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciWrite16 (
  IN UINTN                     Address,
  IN UINT16                    Value
  )
{
  return InternalSavePciWrite16ValueToBootScript (Address, PciWrite16 (Address, Value));
}

/**
  Performs a bitwise OR of a 16-bit PCI configuration register with
  a 16-bit value and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 16-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  OrData  The value to OR with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciOr16 (
  IN UINTN                     Address,
  IN UINT16                    OrData
  )
{
  return InternalSavePciWrite16ValueToBootScript (Address, PciOr16 (Address, OrData));
}

/**
  Performs a bitwise AND of a 16-bit PCI configuration register with a 16-bit
  value and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 16-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciAnd16 (
  IN UINTN                     Address,
  IN UINT16                    AndData
  )
{
  return InternalSavePciWrite16ValueToBootScript (Address, PciAnd16 (Address, AndData));
}

/**
  Performs a bitwise AND of a 16-bit PCI configuration register with a 16-bit
  value, followed a  bitwise OR with another 16-bit value and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData,
  performs a bitwise OR between the result of the AND operation and
  the value specified by OrData, and writes the result to the 16-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciAndThenOr16 (
  IN UINTN                     Address,
  IN UINT16                    AndData,
  IN UINT16                    OrData
  )
{
  return InternalSavePciWrite16ValueToBootScript (Address, PciAndThenOr16 (Address, AndData, OrData));
}

/**
  Reads a bit field of a PCI configuration register and saves the value in
  the S3 script to be replayed on S3 resume.

  Reads the bit field in a 16-bit PCI configuration register. The bit field is
  specified by the StartBit and the EndBit. The value of the bit field is
  returned.

  If Address > 0x0FFFFFFF, then ASSERT().
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
S3PciBitFieldRead16 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit
  )
{
  return InternalSavePciWrite16ValueToBootScript (Address, PciBitFieldRead16 (Address, StartBit, EndBit));
}

/**
  Writes a bit field to a PCI configuration register and saves the value in
  the S3 script to be replayed on S3 resume.

  Writes Value to the bit field of the PCI configuration register. The bit
  field is specified by the StartBit and the EndBit. All other bits in the
  destination PCI configuration register are preserved. The new value of the
  16-bit register is returned.

  If Address > 0x0FFFFFFF, then ASSERT().
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
S3PciBitFieldWrite16 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT16                    Value
  )
{
  return InternalSavePciWrite16ValueToBootScript (Address, PciBitFieldWrite16 (Address, StartBit, EndBit, Value));
}

/**
  Reads a bit field in a 16-bit PCI configuration, performs a bitwise OR, and
  writes the result back to the bit field in the 16-bit port and saves the value
  in the S3 script to be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 16-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized. Extra left bits in OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
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
S3PciBitFieldOr16 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT16                    OrData
  )
{
  return InternalSavePciWrite16ValueToBootScript (Address, PciBitFieldOr16 (Address, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in a 16-bit PCI configuration register, performs a bitwise
  AND, and writes the result back to the bit field in the 16-bit register and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 16-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized. Extra left bits in AndData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT16
EFIAPI
S3PciBitFieldAnd16 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT16                    AndData
  )
{
  return InternalSavePciWrite16ValueToBootScript (Address, PciBitFieldAnd16 (Address, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in a 16-bit Address, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  16-bit port and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 16-bit PCI configuration register specified by Address, performs a
  bitwise AND followed by a bitwise OR between the read result and
  the value specified by AndData, and writes the result to the 16-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized. Extra left bits in both AndData and
  OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().
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
S3PciBitFieldAndThenOr16 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT16                    AndData,
  IN UINT16                    OrData
  )
{
  return InternalSavePciWrite16ValueToBootScript (Address, PciBitFieldAndThenOr16 (Address, StartBit, EndBit, AndData, OrData));
}

/**
  Saves a 32-bit PCI configuration value to the boot script.

  This internal worker function saves a 32-bit PCI configuration value in the S3 script
  to be replayed on S3 resume.

  If the saving process fails, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Value   The value to write.

  @return Value.

**/
UINT32
InternalSavePciWrite32ValueToBootScript (
  IN UINTN              Address,
  IN UINT32             Value
  )
{
  InternalSavePciWriteValueToBootScript (S3BootScriptWidthUint32, Address, &Value);

  return Value;
}

/**
  Reads a 32-bit PCI configuration register and saves the value in the S3
  script to be replayed on S3 resume.

  Reads and returns the 32-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.

  @return The read value from the PCI configuration register.

**/
UINT32
EFIAPI
S3PciRead32 (
  IN UINTN                     Address
  )
{
  return InternalSavePciWrite32ValueToBootScript (Address, PciRead32 (Address));
}

/**
  Writes a 32-bit PCI configuration register and saves the value in the S3
  script to be replayed on S3 resume.

  Writes the 32-bit PCI configuration register specified by Address with the
  value specified by Value. Value is returned. This function must guarantee
  that all PCI read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Value   The value to write.

  @return The value written to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciWrite32 (
  IN UINTN                     Address,
  IN UINT32                    Value
  )
{
  return InternalSavePciWrite32ValueToBootScript (Address, PciWrite32 (Address, Value));
}

/**
  Performs a bitwise OR of a 32-bit PCI configuration register with
  a 32-bit value and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 32-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  OrData  The value to OR with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciOr32 (
  IN UINTN                     Address,
  IN UINT32                    OrData
  )
{
  return InternalSavePciWrite32ValueToBootScript (Address, PciOr32 (Address, OrData));
}

/**
  Performs a bitwise AND of a 32-bit PCI configuration register with a 32-bit
  value and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 32-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciAnd32 (
  IN UINTN                     Address,
  IN UINT32                    AndData
  )
{
  return InternalSavePciWrite32ValueToBootScript (Address, PciAnd32 (Address, AndData));
}

/**
  Performs a bitwise AND of a 32-bit PCI configuration register with a 32-bit
  value, followed a  bitwise OR with another 32-bit value and saves
  the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData,
  performs a bitwise OR between the result of the AND operation and
  the value specified by OrData, and writes the result to the 32-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  AndData The value to AND with the PCI configuration register.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciAndThenOr32 (
  IN UINTN                     Address,
  IN UINT32                    AndData,
  IN UINT32                    OrData
  )
{
  return InternalSavePciWrite32ValueToBootScript (Address, PciAndThenOr32 (Address, AndData, OrData));
}

/**
  Reads a bit field of a PCI configuration register and saves the value in
  the S3 script to be replayed on S3 resume.

  Reads the bit field in a 32-bit PCI configuration register. The bit field is
  specified by the StartBit and the EndBit. The value of the bit field is
  returned.

  If Address > 0x0FFFFFFF, then ASSERT().
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
S3PciBitFieldRead32 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit
  )
{
  return InternalSavePciWrite32ValueToBootScript (Address, PciBitFieldRead32 (Address, StartBit, EndBit));
}

/**
  Writes a bit field to a PCI configuration register and saves the value in
  the S3 script to be replayed on S3 resume.

  Writes Value to the bit field of the PCI configuration register. The bit
  field is specified by the StartBit and the EndBit. All other bits in the
  destination PCI configuration register are preserved. The new value of the
  32-bit register is returned.

  If Address > 0x0FFFFFFF, then ASSERT().
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
S3PciBitFieldWrite32 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT32                    Value
  )
{
  return InternalSavePciWrite32ValueToBootScript (Address, PciBitFieldWrite32 (Address, StartBit, EndBit, Value));
}

/**
  Reads a bit field in a 32-bit PCI configuration, performs a bitwise OR, and
  writes the result back to the bit field in the 32-bit port and saves the value
  in the S3 script to be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise OR between the read result and the value specified by
  OrData, and writes the result to the 32-bit PCI configuration register
  specified by Address. The value written to the PCI configuration register is
  returned. This function must guarantee that all PCI read and write operations
  are serialized. Extra left bits in OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
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
S3PciBitFieldOr32 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT32                    OrData
  )
{
  return InternalSavePciWrite32ValueToBootScript (Address, PciBitFieldOr32 (Address, StartBit, EndBit, OrData));
}

/**
  Reads a bit field in a 32-bit PCI configuration register, performs a bitwise
  AND, and writes the result back to the bit field in the 32-bit register and
  saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise AND between the read result and the value specified by AndData, and
  writes the result to the 32-bit PCI configuration register specified by
  Address. The value written to the PCI configuration register is returned.
  This function must guarantee that all PCI read and write operations are
  serialized. Extra left bits in AndData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Address   PCI configuration register to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the PCI configuration register.

  @return The value written back to the PCI configuration register.

**/
UINT32
EFIAPI
S3PciBitFieldAnd32 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT32                    AndData
  )
{
  return InternalSavePciWrite32ValueToBootScript (Address, PciBitFieldAnd32 (Address, StartBit, EndBit, AndData));
}

/**
  Reads a bit field in a 32-bit Address, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  32-bit port and saves the value in the S3 script to be replayed on S3 resume.

  Reads the 32-bit PCI configuration register specified by Address, performs a
  bitwise AND followed by a bitwise OR between the read result and
  the value specified by AndData, and writes the result to the 32-bit PCI
  configuration register specified by Address. The value written to the PCI
  configuration register is returned. This function must guarantee that all PCI
  read and write operations are serialized. Extra left bits in both AndData and
  OrData are stripped.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().
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
S3PciBitFieldAndThenOr32 (
  IN UINTN                     Address,
  IN UINTN                     StartBit,
  IN UINTN                     EndBit,
  IN UINT32                    AndData,
  IN UINT32                    OrData
  )
{
  return InternalSavePciWrite32ValueToBootScript (Address, PciBitFieldAndThenOr32 (Address, StartBit, EndBit, AndData, OrData));
}

/**
  Reads a range of PCI configuration registers into a caller supplied buffer
  and saves the value in the S3 script to be replayed on S3 resume.

  Reads the range of PCI configuration registers specified by StartAddress and
  Size into the buffer specified by Buffer. This function only allows the PCI
  configuration registers from a single PCI function to be read. Size is
  returned. When possible 32-bit PCI configuration read cycles are used to read
  from StartAdress to StartAddress + Size. Due to alignment restrictions, 8-bit
  and 16-bit PCI configuration read cycles may be used at the beginning and the
  end of the range.

  If StartAddress > 0x0FFFFFFF, then ASSERT().
  If ((StartAddress & 0xFFF) + Size) > 0x1000, then ASSERT().
  If Size > 0 and Buffer is NULL, then ASSERT().

  @param  StartAddress  Starting address that encodes the PCI Bus, Device,
                        Function and Register.
  @param  Size          Size in bytes of the transfer.
  @param  Buffer        Pointer to a buffer receiving the data read.

  @return Size

**/
UINTN
EFIAPI
S3PciReadBuffer (
  IN  UINTN                    StartAddress,
  IN  UINTN                    Size,
  OUT VOID                     *Buffer
  )
{
  RETURN_STATUS    Status;

  Status = S3BootScriptSavePciCfgWrite (
             S3BootScriptWidthUint8,
             PCILIB_TO_COMMON_ADDRESS (StartAddress),
             PciReadBuffer (StartAddress, Size, Buffer),
             Buffer
             );
 ASSERT (Status == RETURN_SUCCESS);

  return Size;
}

/**
  Copies the data in a caller supplied buffer to a specified range of PCI
  configuration space and saves the value in the S3 script to be replayed on S3
  resume.

  Writes the range of PCI configuration registers specified by StartAddress and
  Size from the buffer specified by Buffer. This function only allows the PCI
  configuration registers from a single PCI function to be written. Size is
  returned. When possible 32-bit PCI configuration write cycles are used to
  write from StartAdress to StartAddress + Size. Due to alignment restrictions,
  8-bit and 16-bit PCI configuration write cycles may be used at the beginning
  and the end of the range.

  If StartAddress > 0x0FFFFFFF, then ASSERT().
  If ((StartAddress & 0xFFF) + Size) > 0x1000, then ASSERT().
  If Size > 0 and Buffer is NULL, then ASSERT().

  @param  StartAddress  Starting address that encodes the PCI Bus, Device,
                        Function and Register.
  @param  Size          Size in bytes of the transfer.
  @param  Buffer        Pointer to a buffer containing the data to write.

  @return Size

**/
UINTN
EFIAPI
S3PciWriteBuffer (
  IN UINTN                     StartAddress,
  IN UINTN                     Size,
  IN VOID                      *Buffer
  )
{
  RETURN_STATUS    Status;

  Status = S3BootScriptSavePciCfgWrite (
             S3BootScriptWidthUint8,
             PCILIB_TO_COMMON_ADDRESS (StartAddress),
             PciWriteBuffer (StartAddress, Size, Buffer),
             Buffer
             );
  ASSERT (Status == RETURN_SUCCESS);

  return Size;
}
