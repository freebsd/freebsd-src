/** @file
  DXE PCI Library instance layered on top of ESAL services.

  Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Protocol/ExtendedSalServiceClasses.h>

#include <Library/PciLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/ExtendedSalLib.h>

/**
  Assert the validity of a PCI address. A valid PCI address should contain 1's
  only in the low 28 bits.

  @param  A The address to validate.
  @param  M Additional bits to assert to be zero.

**/
#define ASSERT_INVALID_PCI_ADDRESS(A,M) \
  ASSERT (((A) & (~0xfffffff | (M))) == 0)

/**
  Converts a PCI Library Address to a ESAL PCI Service Address.
  Based on SAL Spec 3.2, there are two SAL PCI Address:

  If address type = 0
  Bits 0..7 - Register address
  Bits 8..10 - Function number
  Bits 11..15 - Device number
  Bits 16..23 - Bus number
  Bits 24..31 - PCI segment group
  Bits 32..63 - Reserved (0)

  If address type = 1
  Bits 0..7 - Register address
  Bits 8..11 - Extended Register address
  Bits 12..14 - Function number
  Bits 15..19 - Device number
  Bits 20..27 - Bus number
  Bits 28..43 - PCI segment group
  Bits 44..63 - Reserved (0)

  @param  A The PCI Library Address to convert.

**/
#define CONVERT_PCI_LIB_ADDRESS_TO_PCI_ESAL_ADDRESS0(Address)  ((((Address) >> 4) & 0x00ffff00) | ((Address) & 0xff))
#define CONVERT_PCI_LIB_ADDRESS_TO_PCI_ESAL_ADDRESS1(Address)  (Address)

/**
  Check a PCI Library Address is a PCI Compatible Address or not.
**/
#define IS_PCI_COMPATIBLE_ADDRESS(Address)  (((Address) & 0xf00) == 0)

/**
  Internal worker function to read a PCI configuration register.

  This function wraps EsalPciConfigRead function of Extended SAL PCI
  Services Class.
  It reads and returns the PCI configuration register specified by Address,
  the width of data is specified by Width.

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Width   Width of data to read

  @return The value read from the PCI configuration register.

**/
UINT32
DxePciLibEsalReadWorker (
  IN    UINTN                       Address,
  IN    UINTN                       Width
  )
{
 SAL_RETURN_REGS Return;

  if (IS_PCI_COMPATIBLE_ADDRESS(Address)) {
    Return = EsalCall (
               EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_LO,
               EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_HI,
               SalPciConfigReadFunctionId,
               CONVERT_PCI_LIB_ADDRESS_TO_PCI_ESAL_ADDRESS0 (Address),
               Width,
               EFI_SAL_PCI_COMPATIBLE_ADDRESS,
               0,
               0,
               0,
               0
               );
  } else {
    Return = EsalCall (
               EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_LO,
               EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_HI,
               SalPciConfigReadFunctionId,
               CONVERT_PCI_LIB_ADDRESS_TO_PCI_ESAL_ADDRESS1 (Address),
               Width,
               EFI_SAL_PCI_EXTENDED_REGISTER_ADDRESS,
               0,
               0,
               0,
               0
               );
  }

  return (UINT32) Return.r9;
}

/**
  Internal worker function to writes a PCI configuration register.

  This function wraps EsalPciConfigWrite function of Extended SAL PCI
  Services Class.
  It writes the PCI configuration register specified by Address with the
  value specified by Data. The width of data is specified by Width.
  Data is returned.

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Width   Width of data to write
  @param  Data    The value to write.

  @return The value written to the PCI configuration register.

**/
UINT32
DxePciLibEsalWriteWorker (
  IN    UINTN                       Address,
  IN    UINTN                       Width,
  IN    UINT32                      Data
  )
{
  if (IS_PCI_COMPATIBLE_ADDRESS(Address)) {
    EsalCall (
      EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_LO,
      EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_HI,
      SalPciConfigWriteFunctionId,
      CONVERT_PCI_LIB_ADDRESS_TO_PCI_ESAL_ADDRESS0 (Address),
      Width,
      Data,
      EFI_SAL_PCI_COMPATIBLE_ADDRESS,
      0,
      0,
      0
      );
  } else {
    EsalCall (
      EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_LO,
      EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_HI,
      SalPciConfigWriteFunctionId,
      CONVERT_PCI_LIB_ADDRESS_TO_PCI_ESAL_ADDRESS1 (Address),
      Width,
      Data,
      EFI_SAL_PCI_EXTENDED_REGISTER_ADDRESS,
      0,
      0,
      0
      );
  }

  return Data;
}

/**
  Register a PCI device so PCI configuration registers may be accessed after
  SetVirtualAddressMap().

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.

  @retval RETURN_SUCCESS           The PCI device was registered for runtime access.
  @retval RETURN_UNSUPPORTED       An attempt was made to call this function
                                   after ExitBootServices().
  @retval RETURN_UNSUPPORTED       The resources required to access the PCI device
                                   at runtime could not be mapped.
  @retval RETURN_OUT_OF_RESOURCES  There are not enough resources available to
                                   complete the registration.

**/
RETURN_STATUS
EFIAPI
PciRegisterForRuntimeAccess (
  IN UINTN  Address
  )
{
  ASSERT_INVALID_PCI_ADDRESS (Address, 0);
  return RETURN_SUCCESS;
}

/**
  Reads an 8-bit PCI configuration register.

  Reads and returns the 8-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.

  @return The value read from the PCI configuration register.

**/
UINT8
EFIAPI
PciRead8 (
  IN      UINTN                     Address
  )
{
  ASSERT_INVALID_PCI_ADDRESS (Address, 0);

  return (UINT8) DxePciLibEsalReadWorker (Address, 1);
}

/**
  Writes an 8-bit PCI configuration register.

  Writes the 8-bit PCI configuration register specified by Address with the
  value specified by Value. Value is returned. This function must guarantee
  that all PCI read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Data    The value to write.

  @return The value written to the PCI configuration register.

**/
UINT8
EFIAPI
PciWrite8 (
  IN      UINTN                     Address,
  IN      UINT8                     Data
  )
{
  ASSERT_INVALID_PCI_ADDRESS (Address, 0);

  return (UINT8) DxePciLibEsalWriteWorker (Address, 1, Data);
}

/**
  Performs a bitwise OR of an 8-bit PCI configuration register with
  an 8-bit value.

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
PciOr8 (
  IN      UINTN                     Address,
  IN      UINT8                     OrData
  )
{
  return PciWrite8 (Address, (UINT8)(PciRead8 (Address) | OrData));
}

/**
  Performs a bitwise AND of an 8-bit PCI configuration register with an 8-bit
  value.

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
PciAnd8 (
  IN      UINTN                     Address,
  IN      UINT8                     AndData
  )
{
  return PciWrite8 (Address, (UINT8)(PciRead8 (Address) & AndData));
}

/**
  Performs a bitwise AND of an 8-bit PCI configuration register with an 8-bit
  value, followed a  bitwise OR with another 8-bit value.

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
PciAndThenOr8 (
  IN      UINTN                     Address,
  IN      UINT8                     AndData,
  IN      UINT8                     OrData
  )
{
  return PciWrite8 (Address, (UINT8)((PciRead8 (Address) & AndData) | OrData));
}

/**
  Reads a bit field of a PCI configuration register.

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
PciBitFieldRead8 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit
  )
{
  return BitFieldRead8 (PciRead8 (Address), StartBit, EndBit);
}

/**
  Writes a bit field to a PCI configuration register.

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
PciBitFieldWrite8 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT8                     Value
  )
{
  return PciWrite8 (
           Address,
           BitFieldWrite8 (PciRead8 (Address), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in an 8-bit PCI configuration, performs a bitwise OR, and
  writes the result back to the bit field in the 8-bit port.

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
PciBitFieldOr8 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT8                     OrData
  )
{
  return PciWrite8 (
           Address,
           BitFieldOr8 (PciRead8 (Address), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in an 8-bit PCI configuration register, performs a bitwise
  AND, and writes the result back to the bit field in the 8-bit register.

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
PciBitFieldAnd8 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT8                     AndData
  )
{
  return PciWrite8 (
           Address,
           BitFieldAnd8 (PciRead8 (Address), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in an 8-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  8-bit port.

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
PciBitFieldAndThenOr8 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT8                     AndData,
  IN      UINT8                     OrData
  )
{
  return PciWrite8 (
           Address,
           BitFieldAndThenOr8 (PciRead8 (Address), StartBit, EndBit, AndData, OrData)
           );
}

/**
  Reads a 16-bit PCI configuration register.

  Reads and returns the 16-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.

  @return The value read from the PCI configuration register.

**/
UINT16
EFIAPI
PciRead16 (
  IN      UINTN                     Address
  )
{
  ASSERT_INVALID_PCI_ADDRESS (Address, 1);

  return (UINT16) DxePciLibEsalReadWorker (Address, 2);
}

/**
  Writes a 16-bit PCI configuration register.

  Writes the 16-bit PCI configuration register specified by Address with the
  value specified by Value. Value is returned. This function must guarantee
  that all PCI read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 16-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Data    The value to write.

  @return The value written to the PCI configuration register.

**/
UINT16
EFIAPI
PciWrite16 (
  IN      UINTN                     Address,
  IN      UINT16                    Data
  )
{
  ASSERT_INVALID_PCI_ADDRESS (Address, 1);

  return (UINT16) DxePciLibEsalWriteWorker (Address, 2, Data);
}

/**
  Performs a bitwise OR of a 16-bit PCI configuration register with
  a 16-bit value.

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
PciOr16 (
  IN      UINTN                     Address,
  IN      UINT16                    OrData
  )
{
  return PciWrite16 (Address, (UINT16)(PciRead16 (Address) | OrData));
}

/**
  Performs a bitwise AND of a 16-bit PCI configuration register with a 16-bit
  value.

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
PciAnd16 (
  IN      UINTN                     Address,
  IN      UINT16                    AndData
  )
{
  return PciWrite16 (Address, (UINT16)(PciRead16 (Address) & AndData));
}

/**
  Performs a bitwise AND of a 16-bit PCI configuration register with a 16-bit
  value, followed a  bitwise OR with another 16-bit value.

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
PciAndThenOr16 (
  IN      UINTN                     Address,
  IN      UINT16                    AndData,
  IN      UINT16                    OrData
  )
{
  return PciWrite16 (Address, (UINT16)((PciRead16 (Address) & AndData) | OrData));
}

/**
  Reads a bit field of a PCI configuration register.

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
PciBitFieldRead16 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit
  )
{
  return BitFieldRead16 (PciRead16 (Address), StartBit, EndBit);
}

/**
  Writes a bit field to a PCI configuration register.

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
PciBitFieldWrite16 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT16                    Value
  )
{
  return PciWrite16 (
           Address,
           BitFieldWrite16 (PciRead16 (Address), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in a 16-bit PCI configuration, performs a bitwise OR, and
  writes the result back to the bit field in the 16-bit port.

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
PciBitFieldOr16 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT16                    OrData
  )
{
  return PciWrite16 (
           Address,
           BitFieldOr16 (PciRead16 (Address), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in a 16-bit PCI configuration register, performs a bitwise
  AND, and writes the result back to the bit field in the 16-bit register.

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
PciBitFieldAnd16 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT16                    AndData
  )
{
  return PciWrite16 (
           Address,
           BitFieldAnd16 (PciRead16 (Address), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in a 16-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  16-bit port.

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
PciBitFieldAndThenOr16 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT16                    AndData,
  IN      UINT16                    OrData
  )
{
  return PciWrite16 (
           Address,
           BitFieldAndThenOr16 (PciRead16 (Address), StartBit, EndBit, AndData, OrData)
           );
}

/**
  Reads a 32-bit PCI configuration register.

  Reads and returns the 32-bit PCI configuration register specified by Address.
  This function must guarantee that all PCI read and write operations are
  serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.

  @return The value read from the PCI configuration register.

**/
UINT32
EFIAPI
PciRead32 (
  IN      UINTN                     Address
  )
{
  ASSERT_INVALID_PCI_ADDRESS (Address, 3);

  return DxePciLibEsalReadWorker (Address, 4);
}

/**
  Writes a 32-bit PCI configuration register.

  Writes the 32-bit PCI configuration register specified by Address with the
  value specified by Value. Value is returned. This function must guarantee
  that all PCI read and write operations are serialized.

  If Address > 0x0FFFFFFF, then ASSERT().
  If Address is not aligned on a 32-bit boundary, then ASSERT().

  @param  Address Address that encodes the PCI Bus, Device, Function and
                  Register.
  @param  Data    The value to write.

  @return The value written to the PCI configuration register.

**/
UINT32
EFIAPI
PciWrite32 (
  IN      UINTN                     Address,
  IN      UINT32                    Data
  )
{
  ASSERT_INVALID_PCI_ADDRESS (Address, 3);

  return DxePciLibEsalWriteWorker (Address, 4, Data);
}

/**
  Performs a bitwise OR of a 32-bit PCI configuration register with
  a 32-bit value.

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
PciOr32 (
  IN      UINTN                     Address,
  IN      UINT32                    OrData
  )
{
  return PciWrite32 (Address, PciRead32 (Address) | OrData);
}

/**
  Performs a bitwise AND of a 32-bit PCI configuration register with a 32-bit
  value.

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
PciAnd32 (
  IN      UINTN                     Address,
  IN      UINT32                    AndData
  )
{
  return PciWrite32 (Address, PciRead32 (Address) & AndData);
}

/**
  Performs a bitwise AND of a 32-bit PCI configuration register with a 32-bit
  value, followed a  bitwise OR with another 32-bit value.

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
PciAndThenOr32 (
  IN      UINTN                     Address,
  IN      UINT32                    AndData,
  IN      UINT32                    OrData
  )
{
  return PciWrite32 (Address, (PciRead32 (Address) & AndData) | OrData);
}

/**
  Reads a bit field of a PCI configuration register.

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
PciBitFieldRead32 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit
  )
{
  return BitFieldRead32 (PciRead32 (Address), StartBit, EndBit);
}

/**
  Writes a bit field to a PCI configuration register.

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
PciBitFieldWrite32 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    Value
  )
{
  return PciWrite32 (
           Address,
           BitFieldWrite32 (PciRead32 (Address), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in a 32-bit PCI configuration, performs a bitwise OR, and
  writes the result back to the bit field in the 32-bit port.

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
PciBitFieldOr32 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    OrData
  )
{
  return PciWrite32 (
           Address,
           BitFieldOr32 (PciRead32 (Address), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in a 32-bit PCI configuration register, performs a bitwise
  AND, and writes the result back to the bit field in the 32-bit register.

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
PciBitFieldAnd32 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    AndData
  )
{
  return PciWrite32 (
           Address,
           BitFieldAnd32 (PciRead32 (Address), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in a 32-bit port, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  32-bit port.

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
PciBitFieldAndThenOr32 (
  IN      UINTN                     Address,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    AndData,
  IN      UINT32                    OrData
  )
{
  return PciWrite32 (
           Address,
           BitFieldAndThenOr32 (PciRead32 (Address), StartBit, EndBit, AndData, OrData)
           );
}

/**
  Reads a range of PCI configuration registers into a caller supplied buffer.

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
PciReadBuffer (
  IN      UINTN                     StartAddress,
  IN      UINTN                     Size,
  OUT     VOID                      *Buffer
  )
{
  UINTN                             ReturnValue;

  ASSERT_INVALID_PCI_ADDRESS (StartAddress, 0);
  ASSERT (((StartAddress & 0xFFF) + Size) <= 0x100);

  if (Size == 0) {
    return Size;
  }

  ASSERT (Buffer != NULL);

  //
  // Save Size for return
  //
  ReturnValue = Size;

  if ((StartAddress & 1) != 0) {
    //
    // Read a byte if StartAddress is byte aligned
    //
    *(volatile UINT8 *)Buffer = PciRead8 (StartAddress);
    StartAddress += sizeof (UINT8);
    Size -= sizeof (UINT8);
    Buffer = (UINT8*)Buffer + 1;
  }

  if (Size >= sizeof (UINT16) && (StartAddress & 2) != 0) {
    //
    // Read a word if StartAddress is word aligned
    //
    *(volatile UINT16 *)Buffer = PciRead16 (StartAddress);
    StartAddress += sizeof (UINT16);
    Size -= sizeof (UINT16);
    Buffer = (UINT16*)Buffer + 1;
  }

  while (Size >= sizeof (UINT32)) {
    //
    // Read as many double words as possible
    //
    *(volatile UINT32 *)Buffer = PciRead32 (StartAddress);
    StartAddress += sizeof (UINT32);
    Size -= sizeof (UINT32);
    Buffer = (UINT32*)Buffer + 1;
  }

  if (Size >= sizeof (UINT16)) {
    //
    // Read the last remaining word if exist
    //
    *(volatile UINT16 *)Buffer = PciRead16 (StartAddress);
    StartAddress += sizeof (UINT16);
    Size -= sizeof (UINT16);
    Buffer = (UINT16*)Buffer + 1;
  }

  if (Size >= sizeof (UINT8)) {
    //
    // Read the last remaining byte if exist
    //
    *(volatile UINT8 *)Buffer = PciRead8 (StartAddress);
  }

  return ReturnValue;
}

/**
  Copies the data in a caller supplied buffer to a specified range of PCI
  configuration space.

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
PciWriteBuffer (
  IN      UINTN                     StartAddress,
  IN      UINTN                     Size,
  IN      VOID                      *Buffer
  )
{
  UINTN                             ReturnValue;

  ASSERT_INVALID_PCI_ADDRESS (StartAddress, 0);
  ASSERT (((StartAddress & 0xFFF) + Size) <= 0x100);

  if (Size == 0) {
    return 0;
  }

  ASSERT (Buffer != NULL);

  //
  // Save Size for return
  //
  ReturnValue = Size;

  if ((StartAddress & 1) != 0) {
    //
    // Write a byte if StartAddress is byte aligned
    //
    PciWrite8 (StartAddress, *(UINT8*)Buffer);
    StartAddress += sizeof (UINT8);
    Size -= sizeof (UINT8);
    Buffer = (UINT8*)Buffer + 1;
  }

  if (Size >= sizeof (UINT16) && (StartAddress & 2) != 0) {
    //
    // Write a word if StartAddress is word aligned
    //
    PciWrite16 (StartAddress, *(UINT16*)Buffer);
    StartAddress += sizeof (UINT16);
    Size -= sizeof (UINT16);
    Buffer = (UINT16*)Buffer + 1;
  }

  while (Size >= sizeof (UINT32)) {
    //
    // Write as many double words as possible
    //
    PciWrite32 (StartAddress, *(UINT32*)Buffer);
    StartAddress += sizeof (UINT32);
    Size -= sizeof (UINT32);
    Buffer = (UINT32*)Buffer + 1;
  }

  if (Size >= sizeof (UINT16)) {
    //
    // Write the last remaining word if exist
    //
    PciWrite16 (StartAddress, *(UINT16*)Buffer);
    StartAddress += sizeof (UINT16);
    Size -= sizeof (UINT16);
    Buffer = (UINT16*)Buffer + 1;
  }

  if (Size >= sizeof (UINT8)) {
    //
    // Write the last remaining byte if exist
    //
    PciWrite8 (StartAddress, *(UINT8*)Buffer);
  }

  return ReturnValue;
}
