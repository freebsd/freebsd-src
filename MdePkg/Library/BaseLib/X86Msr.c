/** @file
  IA-32/x64 MSR functions.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Returns the lower 32-bits of a Machine Specific Register(MSR).

  Reads and returns the lower 32-bits of the MSR specified by Index.
  No parameter checking is performed on Index, and some Index values may cause
  CPU exceptions. The caller must either guarantee that Index is valid, or the
  caller must set up exception handlers to catch the exceptions. This function
  is only available on IA-32 and x64.

  @param  Index The 32-bit MSR index to read.

  @return The lower 32 bits of the MSR identified by Index.

**/
UINT32
EFIAPI
AsmReadMsr32 (
  IN      UINT32  Index
  )
{
  return (UINT32)AsmReadMsr64 (Index);
}

/**
  Writes a 32-bit value to a Machine Specific Register(MSR), and returns the value.
  The upper 32-bits of the MSR are set to zero.

  Writes the 32-bit value specified by Value to the MSR specified by Index. The
  upper 32-bits of the MSR write are set to zero. The 32-bit value written to
  the MSR is returned. No parameter checking is performed on Index or Value,
  and some of these may cause CPU exceptions. The caller must either guarantee
  that Index and Value are valid, or the caller must establish proper exception
  handlers. This function is only available on IA-32 and x64.

  @param  Index The 32-bit MSR index to write.
  @param  Value The 32-bit value to write to the MSR.

  @return Value

**/
UINT32
EFIAPI
AsmWriteMsr32 (
  IN      UINT32  Index,
  IN      UINT32  Value
  )
{
  return (UINT32)AsmWriteMsr64 (Index, Value);
}

/**
  Reads a 64-bit MSR, performs a bitwise OR on the lower 32-bits, and
  writes the result back to the 64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise OR
  between the lower 32-bits of the read result and the value specified by
  OrData, and writes the result to the 64-bit MSR specified by Index. The lower
  32-bits of the value written to the MSR is returned. No parameter checking is
  performed on Index or OrData, and some of these may cause CPU exceptions. The
  caller must either guarantee that Index and OrData are valid, or the caller
  must establish proper exception handlers. This function is only available on
  IA-32 and x64.

  @param  Index   The 32-bit MSR index to write.
  @param  OrData  The value to OR with the read value from the MSR.

  @return The lower 32-bit value written to the MSR.

**/
UINT32
EFIAPI
AsmMsrOr32 (
  IN      UINT32  Index,
  IN      UINT32  OrData
  )
{
  return (UINT32)AsmMsrOr64 (Index, OrData);
}

/**
  Reads a 64-bit MSR, performs a bitwise AND on the lower 32-bits, and writes
  the result back to the 64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise AND between the
  lower 32-bits of the read result and the value specified by AndData, and
  writes the result to the 64-bit MSR specified by Index. The lower 32-bits of
  the value written to the MSR is returned. No parameter checking is performed
  on Index or AndData, and some of these may cause CPU exceptions. The caller
  must either guarantee that Index and AndData are valid, or the caller must
  establish proper exception handlers. This function is only available on IA-32
  and x64.

  @param  Index   The 32-bit MSR index to write.
  @param  AndData The value to AND with the read value from the MSR.

  @return The lower 32-bit value written to the MSR.

**/
UINT32
EFIAPI
AsmMsrAnd32 (
  IN      UINT32  Index,
  IN      UINT32  AndData
  )
{
  return (UINT32)AsmMsrAnd64 (Index, AndData);
}

/**
  Reads a 64-bit MSR, performs a bitwise AND followed by a bitwise OR
  on the lower 32-bits, and writes the result back to the 64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise AND between the
  lower 32-bits of the read result and the value specified by AndData
  preserving the upper 32-bits, performs a bitwise OR between the
  result of the AND operation and the value specified by OrData, and writes the
  result to the 64-bit MSR specified by Address. The lower 32-bits of the value
  written to the MSR is returned. No parameter checking is performed on Index,
  AndData, or OrData, and some of these may cause CPU exceptions. The caller
  must either guarantee that Index, AndData, and OrData are valid, or the
  caller must establish proper exception handlers. This function is only
  available on IA-32 and x64.

  @param  Index   The 32-bit MSR index to write.
  @param  AndData The value to AND with the read value from the MSR.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The lower 32-bit value written to the MSR.

**/
UINT32
EFIAPI
AsmMsrAndThenOr32 (
  IN      UINT32  Index,
  IN      UINT32  AndData,
  IN      UINT32  OrData
  )
{
  return (UINT32)AsmMsrAndThenOr64 (Index, AndData, OrData);
}

/**
  Reads a bit field of an MSR.

  Reads the bit field in the lower 32-bits of a 64-bit MSR. The bit field is
  specified by the StartBit and the EndBit. The value of the bit field is
  returned. The caller must either guarantee that Index is valid, or the caller
  must set up exception handlers to catch the exceptions. This function is only
  available on IA-32 and x64.

  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Index     The 32-bit MSR index to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.

  @return The bit field read from the MSR.

**/
UINT32
EFIAPI
AsmMsrBitFieldRead32 (
  IN      UINT32  Index,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit
  )
{
  return BitFieldRead32 (AsmReadMsr32 (Index), StartBit, EndBit);
}

/**
  Writes a bit field to an MSR.

  Writes Value to a bit field in the lower 32-bits of a 64-bit MSR. The bit
  field is specified by the StartBit and the EndBit. All other bits in the
  destination MSR are preserved. The lower 32-bits of the MSR written is
  returned. The caller must either guarantee that Index and the data written
  is valid, or the caller must set up exception handlers to catch the exceptions.
  This function is only available on IA-32 and x64.

  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Index     The 32-bit MSR index to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  Value     The new value of the bit field.

  @return The lower 32-bit of the value written to the MSR.

**/
UINT32
EFIAPI
AsmMsrBitFieldWrite32 (
  IN      UINT32  Index,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  Value
  )
{
  ASSERT (EndBit < sizeof (Value) * 8);
  ASSERT (StartBit <= EndBit);
  return (UINT32)AsmMsrBitFieldWrite64 (Index, StartBit, EndBit, Value);
}

/**
  Reads a bit field in a 64-bit MSR, performs a bitwise OR, and writes the
  result back to the bit field in the 64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 64-bit MSR specified by Index. The lower 32-bits of the value
  written to the MSR are returned. Extra left bits in OrData are stripped. The
  caller must either guarantee that Index and the data written is valid, or
  the caller must set up exception handlers to catch the exceptions. This
  function is only available on IA-32 and x64.

  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Index     The 32-bit MSR index to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  OrData    The value to OR with the read value from the MSR.

  @return The lower 32-bit of the value written to the MSR.

**/
UINT32
EFIAPI
AsmMsrBitFieldOr32 (
  IN      UINT32  Index,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  OrData
  )
{
  ASSERT (EndBit < sizeof (OrData) * 8);
  ASSERT (StartBit <= EndBit);
  return (UINT32)AsmMsrBitFieldOr64 (Index, StartBit, EndBit, OrData);
}

/**
  Reads a bit field in a 64-bit MSR, performs a bitwise AND, and writes the
  result back to the bit field in the 64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise AND between the
  read result and the value specified by AndData, and writes the result to the
  64-bit MSR specified by Index. The lower 32-bits of the value written to the
  MSR are returned. Extra left bits in AndData are stripped. The caller must
  either guarantee that Index and the data written is valid, or the caller must
  set up exception handlers to catch the exceptions. This function is only
  available on IA-32 and x64.

  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Index     The 32-bit MSR index to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the read value from the MSR.

  @return The lower 32-bit of the value written to the MSR.

**/
UINT32
EFIAPI
AsmMsrBitFieldAnd32 (
  IN      UINT32  Index,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  AndData
  )
{
  ASSERT (EndBit < sizeof (AndData) * 8);
  ASSERT (StartBit <= EndBit);
  return (UINT32)AsmMsrBitFieldAnd64 (Index, StartBit, EndBit, AndData);
}

/**
  Reads a bit field in a 64-bit MSR, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise AND followed by a
  bitwise OR between the read result and the value specified by
  AndData, and writes the result to the 64-bit MSR specified by Index. The
  lower 32-bits of the value written to the MSR are returned. Extra left bits
  in both AndData and OrData are stripped. The caller must either guarantee
  that Index and the data written is valid, or the caller must set up exception
  handlers to catch the exceptions. This function is only available on IA-32
  and x64.

  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Index     The 32-bit MSR index to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the read value from the MSR.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The lower 32-bit of the value written to the MSR.

**/
UINT32
EFIAPI
AsmMsrBitFieldAndThenOr32 (
  IN      UINT32  Index,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT32  AndData,
  IN      UINT32  OrData
  )
{
  ASSERT (EndBit < sizeof (AndData) * 8);
  ASSERT (StartBit <= EndBit);
  return (UINT32)AsmMsrBitFieldAndThenOr64 (
                   Index,
                   StartBit,
                   EndBit,
                   AndData,
                   OrData
                   );
}

/**
  Reads a 64-bit MSR, performs a bitwise OR, and writes the result
  back to the 64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 64-bit MSR specified by Index. The value written to the MSR is
  returned. No parameter checking is performed on Index or OrData, and some of
  these may cause CPU exceptions. The caller must either guarantee that Index
  and OrData are valid, or the caller must establish proper exception handlers.
  This function is only available on IA-32 and x64.

  @param  Index   The 32-bit MSR index to write.
  @param  OrData  The value to OR with the read value from the MSR.

  @return The value written back to the MSR.

**/
UINT64
EFIAPI
AsmMsrOr64 (
  IN      UINT32  Index,
  IN      UINT64  OrData
  )
{
  return AsmWriteMsr64 (Index, AsmReadMsr64 (Index) | OrData);
}

/**
  Reads a 64-bit MSR, performs a bitwise AND, and writes the result back to the
  64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise AND between the
  read result and the value specified by OrData, and writes the result to the
  64-bit MSR specified by Index. The value written to the MSR is returned. No
  parameter checking is performed on Index or OrData, and some of these may
  cause CPU exceptions. The caller must either guarantee that Index and OrData
  are valid, or the caller must establish proper exception handlers. This
  function is only available on IA-32 and x64.

  @param  Index   The 32-bit MSR index to write.
  @param  AndData The value to AND with the read value from the MSR.

  @return The value written back to the MSR.

**/
UINT64
EFIAPI
AsmMsrAnd64 (
  IN      UINT32  Index,
  IN      UINT64  AndData
  )
{
  return AsmWriteMsr64 (Index, AsmReadMsr64 (Index) & AndData);
}

/**
  Reads a 64-bit MSR, performs a bitwise AND followed by a bitwise
  OR, and writes the result back to the 64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise AND between read
  result and the value specified by AndData, performs a bitwise OR
  between the result of the AND operation and the value specified by OrData,
  and writes the result to the 64-bit MSR specified by Index. The value written
  to the MSR is returned. No parameter checking is performed on Index, AndData,
  or OrData, and some of these may cause CPU exceptions. The caller must either
  guarantee that Index, AndData, and OrData are valid, or the caller must
  establish proper exception handlers. This function is only available on IA-32
  and x64.

  @param  Index   The 32-bit MSR index to write.
  @param  AndData The value to AND with the read value from the MSR.
  @param  OrData  The value to OR with the result of the AND operation.

  @return The value written back to the MSR.

**/
UINT64
EFIAPI
AsmMsrAndThenOr64 (
  IN      UINT32  Index,
  IN      UINT64  AndData,
  IN      UINT64  OrData
  )
{
  return AsmWriteMsr64 (Index, (AsmReadMsr64 (Index) & AndData) | OrData);
}

/**
  Reads a bit field of an MSR.

  Reads the bit field in the 64-bit MSR. The bit field is specified by the
  StartBit and the EndBit. The value of the bit field is returned. The caller
  must either guarantee that Index is valid, or the caller must set up
  exception handlers to catch the exceptions. This function is only available
  on IA-32 and x64.

  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Index     The 32-bit MSR index to read.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.

  @return The value read from the MSR.

**/
UINT64
EFIAPI
AsmMsrBitFieldRead64 (
  IN      UINT32  Index,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit
  )
{
  return BitFieldRead64 (AsmReadMsr64 (Index), StartBit, EndBit);
}

/**
  Writes a bit field to an MSR.

  Writes Value to a bit field in a 64-bit MSR. The bit field is specified by
  the StartBit and the EndBit. All other bits in the destination MSR are
  preserved. The MSR written is returned. The caller must either guarantee
  that Index and the data written is valid, or the caller must set up exception
  handlers to catch the exceptions. This function is only available on IA-32 and x64.

  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Index     The 32-bit MSR index to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  Value     The new value of the bit field.

  @return The value written back to the MSR.

**/
UINT64
EFIAPI
AsmMsrBitFieldWrite64 (
  IN      UINT32  Index,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  Value
  )
{
  return AsmWriteMsr64 (
           Index,
           BitFieldWrite64 (AsmReadMsr64 (Index), StartBit, EndBit, Value)
           );
}

/**
  Reads a bit field in a 64-bit MSR, performs a bitwise OR, and
  writes the result back to the bit field in the 64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise OR
  between the read result and the value specified by OrData, and writes the
  result to the 64-bit MSR specified by Index. The value written to the MSR is
  returned. Extra left bits in OrData are stripped. The caller must either
  guarantee that Index and the data written is valid, or the caller must set up
  exception handlers to catch the exceptions. This function is only available
  on IA-32 and x64.

  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Index     The 32-bit MSR index to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  OrData    The value to OR with the read value from the bit field.

  @return The value written back to the MSR.

**/
UINT64
EFIAPI
AsmMsrBitFieldOr64 (
  IN      UINT32  Index,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  OrData
  )
{
  return AsmWriteMsr64 (
           Index,
           BitFieldOr64 (AsmReadMsr64 (Index), StartBit, EndBit, OrData)
           );
}

/**
  Reads a bit field in a 64-bit MSR, performs a bitwise AND, and writes the
  result back to the bit field in the 64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise AND between the
  read result and the value specified by AndData, and writes the result to the
  64-bit MSR specified by Index. The value written to the MSR is returned.
  Extra left bits in AndData are stripped. The caller must either guarantee
  that Index and the data written is valid, or the caller must set up exception
  handlers to catch the exceptions. This function is only available on IA-32
  and x64.

  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Index     The 32-bit MSR index to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  AndData   The value to AND with the read value from the bit field.

  @return The value written back to the MSR.

**/
UINT64
EFIAPI
AsmMsrBitFieldAnd64 (
  IN      UINT32  Index,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  AndData
  )
{
  return AsmWriteMsr64 (
           Index,
           BitFieldAnd64 (AsmReadMsr64 (Index), StartBit, EndBit, AndData)
           );
}

/**
  Reads a bit field in a 64-bit MSR, performs a bitwise AND followed by a
  bitwise OR, and writes the result back to the bit field in the
  64-bit MSR.

  Reads the 64-bit MSR specified by Index, performs a bitwise AND followed by
  a bitwise OR between the read result and the value specified by
  AndData, and writes the result to the 64-bit MSR specified by Index. The
  value written to the MSR is returned. Extra left bits in both AndData and
  OrData are stripped. The caller must either guarantee that Index and the data
  written is valid, or the caller must set up exception handlers to catch the
  exceptions. This function is only available on IA-32 and x64.

  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Index     The 32-bit MSR index to write.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  AndData   The value to AND with the read value from the bit field.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The value written back to the MSR.

**/
UINT64
EFIAPI
AsmMsrBitFieldAndThenOr64 (
  IN      UINT32  Index,
  IN      UINTN   StartBit,
  IN      UINTN   EndBit,
  IN      UINT64  AndData,
  IN      UINT64  OrData
  )
{
  return AsmWriteMsr64 (
           Index,
           BitFieldAndThenOr64 (
             AsmReadMsr64 (Index),
             StartBit,
             EndBit,
             AndData,
             OrData
             )
           );
}
