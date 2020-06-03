/** @file
  Base Print Library instance implementation.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PrintLibInternal.h"

//
// Declare a VA_LIST global variable that is used in calls to BasePrintLibSPrintMarker()
// when the BASE_LIST parameter is valid and the VA_LIST parameter is ignored.
// A NULL VA_LIST can not be passed into  BasePrintLibSPrintMarker() because some
// compilers define VA_LIST to be a structure.
//
VA_LIST gNullVaList;

#define ASSERT_UNICODE_BUFFER(Buffer) ASSERT ((((UINTN) (Buffer)) & 0x01) == 0)

/**
  Produces a Null-terminated Unicode string in an output buffer based on
  a Null-terminated Unicode format string and a VA_LIST argument list.

  This function is similar as vsnprintf_s defined in C11.

  Produces a Null-terminated Unicode string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The Unicode string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list specified by Marker based on the
  contents of the format string.
  The number of Unicode characters in the produced output buffer is returned not including
  the Null-terminator.

  If StartOfBuffer is not aligned on a 16-bit boundary, then ASSERT().
  If FormatString is not aligned on a 16-bit boundary, then ASSERT().

  If BufferSize > 1 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 1 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and BufferSize >
  (PcdMaximumUnicodeStringLength * sizeof (CHAR16) + 1), then ASSERT(). Also, the output
  buffer is unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and FormatString contains more than
  PcdMaximumUnicodeStringLength Unicode characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0 or 1, then the output buffer is unmodified and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          Unicode string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated Unicode format string.
  @param  Marker          VA_LIST marker for the variable argument list.

  @return The number of Unicode characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
UnicodeVSPrint (
  OUT CHAR16        *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR16  *FormatString,
  IN  VA_LIST       Marker
  )
{
  ASSERT_UNICODE_BUFFER (StartOfBuffer);
  ASSERT_UNICODE_BUFFER (FormatString);
  return BasePrintLibSPrintMarker ((CHAR8 *)StartOfBuffer, BufferSize >> 1, FORMAT_UNICODE | OUTPUT_UNICODE, (CHAR8 *)FormatString, Marker, NULL);
}

/**
  Produces a Null-terminated Unicode string in an output buffer based on
  a Null-terminated Unicode format string and a BASE_LIST argument list.

  Produces a Null-terminated Unicode string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The Unicode string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list specified by Marker based on the
  contents of the format string.
  The number of Unicode characters in the produced output buffer is returned not including
  the Null-terminator.

  If StartOfBuffer is not aligned on a 16-bit boundary, then ASSERT().
  If FormatString is not aligned on a 16-bit boundary, then ASSERT().

  If BufferSize > 1 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 1 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and BufferSize >
  (PcdMaximumUnicodeStringLength * sizeof (CHAR16) + 1), then ASSERT(). Also, the output
  buffer is unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and FormatString contains more than
  PcdMaximumUnicodeStringLength Unicode characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0 or 1, then the output buffer is unmodified and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          Unicode string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated Unicode format string.
  @param  Marker          BASE_LIST marker for the variable argument list.

  @return The number of Unicode characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
UnicodeBSPrint (
  OUT CHAR16        *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR16  *FormatString,
  IN  BASE_LIST     Marker
  )
{
  ASSERT_UNICODE_BUFFER (StartOfBuffer);
  ASSERT_UNICODE_BUFFER (FormatString);
  return BasePrintLibSPrintMarker ((CHAR8 *)StartOfBuffer, BufferSize >> 1, FORMAT_UNICODE | OUTPUT_UNICODE, (CHAR8 *)FormatString, gNullVaList, Marker);
}

/**
  Produces a Null-terminated Unicode string in an output buffer based on a Null-terminated
  Unicode format string and variable argument list.

  This function is similar as snprintf_s defined in C11.

  Produces a Null-terminated Unicode string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The Unicode string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list based on the contents of the format string.
  The number of Unicode characters in the produced output buffer is returned not including
  the Null-terminator.

  If StartOfBuffer is not aligned on a 16-bit boundary, then ASSERT().
  If FormatString is not aligned on a 16-bit boundary, then ASSERT().

  If BufferSize > 1 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 1 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and BufferSize >
  (PcdMaximumUnicodeStringLength * sizeof (CHAR16) + 1), then ASSERT(). Also, the output
  buffer is unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and FormatString contains more than
  PcdMaximumUnicodeStringLength Unicode characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0 or 1, then the output buffer is unmodified and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          Unicode string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated Unicode format string.
  @param  ...             Variable argument list whose contents are accessed based on the
                          format string specified by FormatString.

  @return The number of Unicode characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
UnicodeSPrint (
  OUT CHAR16        *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR16  *FormatString,
  ...
  )
{
  VA_LIST Marker;
  UINTN   NumberOfPrinted;

  VA_START (Marker, FormatString);
  NumberOfPrinted = UnicodeVSPrint (StartOfBuffer, BufferSize, FormatString, Marker);
  VA_END (Marker);
  return NumberOfPrinted;
}

/**
  Produces a Null-terminated Unicode string in an output buffer based on a Null-terminated
  ASCII format string and a VA_LIST argument list.

  This function is similar as vsnprintf_s defined in C11.

  Produces a Null-terminated Unicode string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The Unicode string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list specified by Marker based on the
  contents of the format string.
  The number of Unicode characters in the produced output buffer is returned not including
  the Null-terminator.

  If StartOfBuffer is not aligned on a 16-bit boundary, then ASSERT().

  If BufferSize > 1 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 1 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and BufferSize >
  (PcdMaximumUnicodeStringLength * sizeof (CHAR16) + 1), then ASSERT(). Also, the output
  buffer is unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and FormatString contains more than
  PcdMaximumAsciiStringLength Ascii characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0 or 1, then no output buffer is produced and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          Unicode string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated ASCII format string.
  @param  Marker          VA_LIST marker for the variable argument list.

  @return The number of Unicode characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
UnicodeVSPrintAsciiFormat (
  OUT CHAR16       *StartOfBuffer,
  IN  UINTN        BufferSize,
  IN  CONST CHAR8  *FormatString,
  IN  VA_LIST      Marker
  )
{
  ASSERT_UNICODE_BUFFER (StartOfBuffer);
  return BasePrintLibSPrintMarker ((CHAR8 *)StartOfBuffer, BufferSize >> 1, OUTPUT_UNICODE, FormatString, Marker, NULL);
}

/**
  Produces a Null-terminated Unicode string in an output buffer based on a Null-terminated
  ASCII format string and a BASE_LIST argument list.

  Produces a Null-terminated Unicode string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The Unicode string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list specified by Marker based on the
  contents of the format string.
  The number of Unicode characters in the produced output buffer is returned not including
  the Null-terminator.

  If StartOfBuffer is not aligned on a 16-bit boundary, then ASSERT().

  If BufferSize > 1 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 1 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and BufferSize >
  (PcdMaximumUnicodeStringLength * sizeof (CHAR16) + 1), then ASSERT(). Also, the output
  buffer is unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and FormatString contains more than
  PcdMaximumAsciiStringLength Ascii characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0 or 1, then no output buffer is produced and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          Unicode string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated ASCII format string.
  @param  Marker          BASE_LIST marker for the variable argument list.

  @return The number of Unicode characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
UnicodeBSPrintAsciiFormat (
  OUT CHAR16       *StartOfBuffer,
  IN  UINTN        BufferSize,
  IN  CONST CHAR8  *FormatString,
  IN  BASE_LIST    Marker
  )
{
  ASSERT_UNICODE_BUFFER (StartOfBuffer);
  return BasePrintLibSPrintMarker ((CHAR8 *)StartOfBuffer, BufferSize >> 1, OUTPUT_UNICODE, FormatString, gNullVaList, Marker);
}

/**
  Produces a Null-terminated Unicode string in an output buffer based on a Null-terminated
  ASCII format string and  variable argument list.

  This function is similar as snprintf_s defined in C11.

  Produces a Null-terminated Unicode string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The Unicode string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list based on the contents of the
  format string.
  The number of Unicode characters in the produced output buffer is returned not including
  the Null-terminator.

  If StartOfBuffer is not aligned on a 16-bit boundary, then ASSERT().

  If BufferSize > 1 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 1 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and BufferSize >
  (PcdMaximumUnicodeStringLength * sizeof (CHAR16) + 1), then ASSERT(). Also, the output
  buffer is unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and FormatString contains more than
  PcdMaximumAsciiStringLength Ascii characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0 or 1, then no output buffer is produced and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          Unicode string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated ASCII format string.
  @param  ...             Variable argument list whose contents are accessed based on the
                          format string specified by FormatString.

  @return The number of Unicode characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
UnicodeSPrintAsciiFormat (
  OUT CHAR16       *StartOfBuffer,
  IN  UINTN        BufferSize,
  IN  CONST CHAR8  *FormatString,
  ...
  )
{
  VA_LIST Marker;
  UINTN   NumberOfPrinted;

  VA_START (Marker, FormatString);
  NumberOfPrinted = UnicodeVSPrintAsciiFormat (StartOfBuffer, BufferSize, FormatString, Marker);
  VA_END (Marker);
  return NumberOfPrinted;
}

#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function is deprecated for security reason.

  Converts a decimal value to a Null-terminated Unicode string.

  Converts the decimal number specified by Value to a Null-terminated Unicode
  string specified by Buffer containing at most Width characters. No padding of spaces
  is ever performed. If Width is 0 then a width of MAXIMUM_VALUE_CHARACTERS is assumed.
  The number of Unicode characters in Buffer is returned not including the Null-terminator.
  If the conversion contains more than Width characters, then only the first
  Width characters are returned, and the total number of characters
  required to perform the conversion is returned.
  Additional conversion parameters are specified in Flags.

  The Flags bit LEFT_JUSTIFY is always ignored.
  All conversions are left justified in Buffer.
  If Width is 0, PREFIX_ZERO is ignored in Flags.
  If COMMA_TYPE is set in Flags, then PREFIX_ZERO is ignored in Flags, and commas
  are inserted every 3rd digit starting from the right.
  If RADIX_HEX is set in Flags, then the output buffer will be
  formatted in hexadecimal format.
  If Value is < 0 and RADIX_HEX is not set in Flags, then the fist character in Buffer is a '-'.
  If PREFIX_ZERO is set in Flags and PREFIX_ZERO is not being ignored,
  then Buffer is padded with '0' characters so the combination of the optional '-'
  sign character, '0' characters, digit characters for Value, and the Null-terminator
  add up to Width characters.
  If both COMMA_TYPE and RADIX_HEX are set in Flags, then ASSERT().
  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-bit boundary, then ASSERT().
  If unsupported bits are set in Flags, then ASSERT().
  If both COMMA_TYPE and RADIX_HEX are set in Flags, then ASSERT().
  If Width >= MAXIMUM_VALUE_CHARACTERS, then ASSERT()

  @param  Buffer  The pointer to the output buffer for the produced Null-terminated
                  Unicode string.
  @param  Flags   The bitmask of flags that specify left justification, zero pad, and commas.
  @param  Value   The 64-bit signed value to convert to a string.
  @param  Width   The maximum number of Unicode characters to place in Buffer, not including
                  the Null-terminator.

  @return The number of Unicode characters in Buffer not including the Null-terminator.

**/
UINTN
EFIAPI
UnicodeValueToString (
  IN OUT CHAR16  *Buffer,
  IN UINTN       Flags,
  IN INT64       Value,
  IN UINTN       Width
  )
{
  ASSERT_UNICODE_BUFFER(Buffer);
  return BasePrintLibConvertValueToString ((CHAR8 *)Buffer, Flags, Value, Width, 2);
}

#endif

/**
  Converts a decimal value to a Null-terminated Unicode string.

  Converts the decimal number specified by Value to a Null-terminated Unicode
  string specified by Buffer containing at most Width characters. No padding of
  spaces is ever performed. If Width is 0 then a width of
  MAXIMUM_VALUE_CHARACTERS is assumed. If the conversion contains more than
  Width characters, then only the first Width characters are placed in Buffer.
  Additional conversion parameters are specified in Flags.

  The Flags bit LEFT_JUSTIFY is always ignored.
  All conversions are left justified in Buffer.
  If Width is 0, PREFIX_ZERO is ignored in Flags.
  If COMMA_TYPE is set in Flags, then PREFIX_ZERO is ignored in Flags, and
  commas are inserted every 3rd digit starting from the right.
  If RADIX_HEX is set in Flags, then the output buffer will be formatted in
  hexadecimal format.
  If Value is < 0 and RADIX_HEX is not set in Flags, then the fist character in
  Buffer is a '-'.
  If PREFIX_ZERO is set in Flags and PREFIX_ZERO is not being ignored, then
  Buffer is padded with '0' characters so the combination of the optional '-'
  sign character, '0' characters, digit characters for Value, and the
  Null-terminator add up to Width characters.

  If Buffer is not aligned on a 16-bit boundary, then ASSERT().
  If an error would be returned, then the function will also ASSERT().

  @param  Buffer      The pointer to the output buffer for the produced
                      Null-terminated Unicode string.
  @param  BufferSize  The size of Buffer in bytes, including the
                      Null-terminator.
  @param  Flags       The bitmask of flags that specify left justification,
                      zero pad, and commas.
  @param  Value       The 64-bit signed value to convert to a string.
  @param  Width       The maximum number of Unicode characters to place in
                      Buffer, not including the Null-terminator.

  @retval RETURN_SUCCESS           The decimal value is converted.
  @retval RETURN_BUFFER_TOO_SMALL  If BufferSize cannot hold the converted
                                   value.
  @retval RETURN_INVALID_PARAMETER If Buffer is NULL.
                                   If PcdMaximumUnicodeStringLength is not
                                   zero, and BufferSize is greater than
                                   (PcdMaximumUnicodeStringLength *
                                   sizeof (CHAR16) + 1).
                                   If unsupported bits are set in Flags.
                                   If both COMMA_TYPE and RADIX_HEX are set in
                                   Flags.
                                   If Width >= MAXIMUM_VALUE_CHARACTERS.

**/
RETURN_STATUS
EFIAPI
UnicodeValueToStringS (
  IN OUT CHAR16  *Buffer,
  IN UINTN       BufferSize,
  IN UINTN       Flags,
  IN INT64       Value,
  IN UINTN       Width
  )
{
  ASSERT_UNICODE_BUFFER(Buffer);
  return BasePrintLibConvertValueToStringS ((CHAR8 *)Buffer, BufferSize, Flags, Value, Width, 2);
}

/**
  Produces a Null-terminated ASCII string in an output buffer based on a Null-terminated
  ASCII format string and a VA_LIST argument list.

  This function is similar as vsnprintf_s defined in C11.

  Produces a Null-terminated ASCII string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The ASCII string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list specified by Marker based on
  the contents of the format string.
  The number of ASCII characters in the produced output buffer is returned not including
  the Null-terminator.

  If BufferSize > 0 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 0 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and BufferSize >
  (PcdMaximumAsciiStringLength * sizeof (CHAR8)), then ASSERT(). Also, the output buffer
  is unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and FormatString contains more than
  PcdMaximumAsciiStringLength Ascii characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0, then no output buffer is produced and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          ASCII string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated ASCII format string.
  @param  Marker          VA_LIST marker for the variable argument list.

  @return The number of ASCII characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
AsciiVSPrint (
  OUT CHAR8         *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR8   *FormatString,
  IN  VA_LIST       Marker
  )
{
  return BasePrintLibSPrintMarker (StartOfBuffer, BufferSize, 0, FormatString, Marker, NULL);
}

/**
  Produces a Null-terminated ASCII string in an output buffer based on a Null-terminated
  ASCII format string and a BASE_LIST argument list.

  Produces a Null-terminated ASCII string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The ASCII string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list specified by Marker based on
  the contents of the format string.
  The number of ASCII characters in the produced output buffer is returned not including
  the Null-terminator.

  If BufferSize > 0 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 0 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and BufferSize >
  (PcdMaximumAsciiStringLength * sizeof (CHAR8)), then ASSERT(). Also, the output buffer
  is unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and FormatString contains more than
  PcdMaximumAsciiStringLength Ascii characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0, then no output buffer is produced and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          ASCII string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated ASCII format string.
  @param  Marker          BASE_LIST marker for the variable argument list.

  @return The number of ASCII characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
AsciiBSPrint (
  OUT CHAR8         *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR8   *FormatString,
  IN  BASE_LIST     Marker
  )
{
  return BasePrintLibSPrintMarker (StartOfBuffer, BufferSize, 0, FormatString, gNullVaList, Marker);
}

/**
  Produces a Null-terminated ASCII string in an output buffer based on a Null-terminated
  ASCII format string and  variable argument list.

  This function is similar as snprintf_s defined in C11.

  Produces a Null-terminated ASCII string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The ASCII string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list based on the contents of the
  format string.
  The number of ASCII characters in the produced output buffer is returned not including
  the Null-terminator.

  If BufferSize > 0 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 0 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and BufferSize >
  (PcdMaximumAsciiStringLength * sizeof (CHAR8)), then ASSERT(). Also, the output buffer
  is unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and FormatString contains more than
  PcdMaximumAsciiStringLength Ascii characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0, then no output buffer is produced and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          ASCII string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated ASCII format string.
  @param  ...             Variable argument list whose contents are accessed based on the
                          format string specified by FormatString.

  @return The number of ASCII characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
AsciiSPrint (
  OUT CHAR8        *StartOfBuffer,
  IN  UINTN        BufferSize,
  IN  CONST CHAR8  *FormatString,
  ...
  )
{
  VA_LIST Marker;
  UINTN   NumberOfPrinted;

  VA_START (Marker, FormatString);
  NumberOfPrinted = AsciiVSPrint (StartOfBuffer, BufferSize, FormatString, Marker);
  VA_END (Marker);
  return NumberOfPrinted;
}

/**
  Produces a Null-terminated ASCII string in an output buffer based on a Null-terminated
  Unicode format string and a VA_LIST argument list.

  This function is similar as vsnprintf_s defined in C11.

  Produces a Null-terminated ASCII string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The ASCII string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list specified by Marker based on
  the contents of the format string.
  The number of ASCII characters in the produced output buffer is returned not including
  the Null-terminator.

  If FormatString is not aligned on a 16-bit boundary, then ASSERT().

  If BufferSize > 0 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 0 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and BufferSize >
  (PcdMaximumAsciiStringLength * sizeof (CHAR8)), then ASSERT(). Also, the output buffer
  is unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and FormatString contains more than
  PcdMaximumUnicodeStringLength Unicode characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0, then no output buffer is produced and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          ASCII string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated Unicode format string.
  @param  Marker          VA_LIST marker for the variable argument list.

  @return The number of ASCII characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
AsciiVSPrintUnicodeFormat (
  OUT CHAR8         *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR16  *FormatString,
  IN  VA_LIST       Marker
  )
{
  ASSERT_UNICODE_BUFFER (FormatString);
  return BasePrintLibSPrintMarker (StartOfBuffer, BufferSize, FORMAT_UNICODE, (CHAR8 *)FormatString, Marker, NULL);
}

/**
  Produces a Null-terminated ASCII string in an output buffer based on a Null-terminated
  Unicode format string and a BASE_LIST argument list.

  Produces a Null-terminated ASCII string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The ASCII string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list specified by Marker based on
  the contents of the format string.
  The number of ASCII characters in the produced output buffer is returned not including
  the Null-terminator.

  If FormatString is not aligned on a 16-bit boundary, then ASSERT().

  If BufferSize > 0 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 0 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and BufferSize >
  (PcdMaximumAsciiStringLength * sizeof (CHAR8)), then ASSERT(). Also, the output buffer
  is unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and FormatString contains more than
  PcdMaximumUnicodeStringLength Unicode characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0, then no output buffer is produced and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          ASCII string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated Unicode format string.
  @param  Marker          BASE_LIST marker for the variable argument list.

  @return The number of ASCII characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
AsciiBSPrintUnicodeFormat (
  OUT CHAR8         *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR16  *FormatString,
  IN  BASE_LIST     Marker
  )
{
  ASSERT_UNICODE_BUFFER (FormatString);
  return BasePrintLibSPrintMarker (StartOfBuffer, BufferSize, FORMAT_UNICODE, (CHAR8 *)FormatString, gNullVaList, Marker);
}

/**
  Produces a Null-terminated ASCII string in an output buffer based on a Null-terminated
  Unicode format string and  variable argument list.

  This function is similar as snprintf_s defined in C11.

  Produces a Null-terminated ASCII string in the output buffer specified by StartOfBuffer
  and BufferSize.
  The ASCII string is produced by parsing the format string specified by FormatString.
  Arguments are pulled from the variable argument list based on the contents of the
  format string.
  The number of ASCII characters in the produced output buffer is returned not including
  the Null-terminator.

  If FormatString is not aligned on a 16-bit boundary, then ASSERT().

  If BufferSize > 0 and StartOfBuffer is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If BufferSize > 0 and FormatString is NULL, then ASSERT(). Also, the output buffer is
  unmodified and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and BufferSize >
  (PcdMaximumAsciiStringLength * sizeof (CHAR8)), then ASSERT(). Also, the output buffer
  is unmodified and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and FormatString contains more than
  PcdMaximumUnicodeStringLength Unicode characters not including the Null-terminator, then
  ASSERT(). Also, the output buffer is unmodified and 0 is returned.

  If BufferSize is 0, then no output buffer is produced and 0 is returned.

  @param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
                          ASCII string.
  @param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
  @param  FormatString    A Null-terminated Unicode format string.
  @param  ...             Variable argument list whose contents are accessed based on the
                          format string specified by FormatString.

  @return The number of ASCII characters in the produced output buffer not including the
          Null-terminator.

**/
UINTN
EFIAPI
AsciiSPrintUnicodeFormat (
  OUT CHAR8         *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR16  *FormatString,
  ...
  )
{
  VA_LIST Marker;
  UINTN   NumberOfPrinted;

  VA_START (Marker, FormatString);
  NumberOfPrinted = AsciiVSPrintUnicodeFormat (StartOfBuffer, BufferSize, FormatString, Marker);
  VA_END (Marker);
  return NumberOfPrinted;
}


#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function is deprecated for security reason.

  Converts a decimal value to a Null-terminated ASCII string.

  Converts the decimal number specified by Value to a Null-terminated ASCII string
  specified by Buffer containing at most Width characters. No padding of spaces
  is ever performed.
  If Width is 0 then a width of  MAXIMUM_VALUE_CHARACTERS is assumed.
  The number of ASCII characters in Buffer is returned not including the Null-terminator.
  If the conversion contains more than Width characters, then only the first Width
  characters are returned, and the total number of characters required to perform
  the conversion is returned.
  Additional conversion parameters are specified in Flags.
  The Flags bit LEFT_JUSTIFY is always ignored.
  All conversions are left justified in Buffer.
  If Width is 0, PREFIX_ZERO is ignored in Flags.
  If COMMA_TYPE is set in Flags, then PREFIX_ZERO is ignored in Flags, and commas
  are inserted every 3rd digit starting from the right.
  If RADIX_HEX is set in Flags, then the output buffer will be
  formatted in hexadecimal format.
  If Value is < 0 and RADIX_HEX is not set in Flags, then the fist character in Buffer is a '-'.
  If PREFIX_ZERO is set in Flags and PREFIX_ZERO is not being ignored,
  then Buffer is padded with '0' characters so the combination of the optional '-'
  sign character, '0' characters, digit characters for Value, and the Null-terminator
  add up to Width characters.

  If Buffer is NULL, then ASSERT().
  If unsupported bits are set in Flags, then ASSERT().
  If both COMMA_TYPE and RADIX_HEX are set in Flags, then ASSERT().
  If Width >= MAXIMUM_VALUE_CHARACTERS, then ASSERT()

  @param  Buffer  The pointer to the output buffer for the produced Null-terminated
                  ASCII string.
  @param  Flags   The bitmask of flags that specify left justification, zero pad, and commas.
  @param  Value   The 64-bit signed value to convert to a string.
  @param  Width   The maximum number of ASCII characters to place in Buffer, not including
                  the Null-terminator.

  @return The number of ASCII characters in Buffer not including the Null-terminator.

**/
UINTN
EFIAPI
AsciiValueToString (
  OUT CHAR8      *Buffer,
  IN  UINTN      Flags,
  IN  INT64      Value,
  IN  UINTN      Width
  )
{
  return BasePrintLibConvertValueToString (Buffer, Flags, Value, Width, 1);
}

#endif

/**
  Converts a decimal value to a Null-terminated Ascii string.

  Converts the decimal number specified by Value to a Null-terminated Ascii
  string specified by Buffer containing at most Width characters. No padding of
  spaces is ever performed. If Width is 0 then a width of
  MAXIMUM_VALUE_CHARACTERS is assumed. If the conversion contains more than
  Width characters, then only the first Width characters are placed in Buffer.
  Additional conversion parameters are specified in Flags.

  The Flags bit LEFT_JUSTIFY is always ignored.
  All conversions are left justified in Buffer.
  If Width is 0, PREFIX_ZERO is ignored in Flags.
  If COMMA_TYPE is set in Flags, then PREFIX_ZERO is ignored in Flags, and
  commas are inserted every 3rd digit starting from the right.
  If RADIX_HEX is set in Flags, then the output buffer will be formatted in
  hexadecimal format.
  If Value is < 0 and RADIX_HEX is not set in Flags, then the fist character in
  Buffer is a '-'.
  If PREFIX_ZERO is set in Flags and PREFIX_ZERO is not being ignored, then
  Buffer is padded with '0' characters so the combination of the optional '-'
  sign character, '0' characters, digit characters for Value, and the
  Null-terminator add up to Width characters.

  If an error would be returned, then the function will ASSERT().

  @param  Buffer      The pointer to the output buffer for the produced
                      Null-terminated Ascii string.
  @param  BufferSize  The size of Buffer in bytes, including the
                      Null-terminator.
  @param  Flags       The bitmask of flags that specify left justification,
                      zero pad, and commas.
  @param  Value       The 64-bit signed value to convert to a string.
  @param  Width       The maximum number of Ascii characters to place in
                      Buffer, not including the Null-terminator.

  @retval RETURN_SUCCESS           The decimal value is converted.
  @retval RETURN_BUFFER_TOO_SMALL  If BufferSize cannot hold the converted
                                   value.
  @retval RETURN_INVALID_PARAMETER If Buffer is NULL.
                                   If PcdMaximumAsciiStringLength is not
                                   zero, and BufferSize is greater than
                                   PcdMaximumAsciiStringLength.
                                   If unsupported bits are set in Flags.
                                   If both COMMA_TYPE and RADIX_HEX are set in
                                   Flags.
                                   If Width >= MAXIMUM_VALUE_CHARACTERS.

**/
RETURN_STATUS
EFIAPI
AsciiValueToStringS (
  IN OUT CHAR8   *Buffer,
  IN UINTN       BufferSize,
  IN UINTN       Flags,
  IN INT64       Value,
  IN UINTN       Width
  )
{
  return BasePrintLibConvertValueToStringS (Buffer, BufferSize, Flags, Value, Width, 1);
}

/**
  Returns the number of characters that would be produced by if the formatted
  output were produced not including the Null-terminator.

  If FormatString is not aligned on a 16-bit boundary, then ASSERT().

  If FormatString is NULL, then ASSERT() and 0 is returned.
  If PcdMaximumUnicodeStringLength is not zero, and FormatString contains more
  than PcdMaximumUnicodeStringLength Unicode characters not including the
  Null-terminator, then ASSERT() and 0 is returned.

  @param[in]  FormatString    A Null-terminated Unicode format string.
  @param[in]  Marker          VA_LIST marker for the variable argument list.

  @return The number of characters that would be produced, not including the
          Null-terminator.
**/
UINTN
EFIAPI
SPrintLength (
  IN  CONST CHAR16   *FormatString,
  IN  VA_LIST       Marker
  )
{
  ASSERT_UNICODE_BUFFER (FormatString);
  return BasePrintLibSPrintMarker (NULL, 0, FORMAT_UNICODE | OUTPUT_UNICODE | COUNT_ONLY_NO_PRINT, (CHAR8 *)FormatString, Marker, NULL);
}

/**
  Returns the number of characters that would be produced by if the formatted
  output were produced not including the Null-terminator.

  If FormatString is NULL, then ASSERT() and 0 is returned.
  If PcdMaximumAsciiStringLength is not zero, and FormatString contains more
  than PcdMaximumAsciiStringLength Ascii characters not including the
  Null-terminator, then ASSERT() and 0 is returned.

  @param[in]  FormatString    A Null-terminated ASCII format string.
  @param[in]  Marker          VA_LIST marker for the variable argument list.

  @return The number of characters that would be produced, not including the
          Null-terminator.
**/
UINTN
EFIAPI
SPrintLengthAsciiFormat (
  IN  CONST CHAR8   *FormatString,
  IN  VA_LIST       Marker
  )
{
  return BasePrintLibSPrintMarker (NULL, 0, OUTPUT_UNICODE | COUNT_ONLY_NO_PRINT, (CHAR8 *)FormatString, Marker, NULL);
}
