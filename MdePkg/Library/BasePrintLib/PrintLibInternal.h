/** @file
  Base Print Library instance Internal Functions definition.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PRINT_LIB_INTERNAL_H__
#define __PRINT_LIB_INTERNAL_H__

#include <Base.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

//
// Print primitives
//
#define PREFIX_SIGN          BIT1
#define PREFIX_BLANK         BIT2
#define LONG_TYPE            BIT4
#define OUTPUT_UNICODE       BIT6
#define FORMAT_UNICODE       BIT8
#define PAD_TO_WIDTH         BIT9
#define ARGUMENT_UNICODE     BIT10
#define PRECISION            BIT11
#define ARGUMENT_REVERSED    BIT12
#define COUNT_ONLY_NO_PRINT  BIT13
#define UNSIGNED_TYPE        BIT14

//
// Record date and time information
//
typedef struct {
  UINT16    Year;
  UINT8     Month;
  UINT8     Day;
  UINT8     Hour;
  UINT8     Minute;
  UINT8     Second;
  UINT8     Pad1;
  UINT32    Nanosecond;
  INT16     TimeZone;
  UINT8     Daylight;
  UINT8     Pad2;
} TIME;

/**
  Worker function that produces a Null-terminated string in an output buffer
  based on a Null-terminated format string and a VA_LIST argument list.

  VSPrint function to process format and place the results in Buffer. Since a
  VA_LIST is used this routine allows the nesting of Vararg routines. Thus
  this is the main print working routine.

  If COUNT_ONLY_NO_PRINT is set in Flags, Buffer will not be modified at all.

  @param[out] Buffer          The character buffer to print the results of the
                              parsing of Format into.
  @param[in]  BufferSize      The maximum number of characters to put into
                              buffer.
  @param[in]  Flags           Initial flags value.
                              Can only have FORMAT_UNICODE, OUTPUT_UNICODE,
                              and COUNT_ONLY_NO_PRINT set.
  @param[in]  Format          A Null-terminated format string.
  @param[in]  VaListMarker    VA_LIST style variable argument list consumed by
                              processing Format.
  @param[in]  BaseListMarker  BASE_LIST style variable argument list consumed
                              by processing Format.

  @return The number of characters printed not including the Null-terminator.
          If COUNT_ONLY_NO_PRINT was set returns the same, but without any
          modification to Buffer.

**/
UINTN
BasePrintLibSPrintMarker (
  OUT CHAR8        *Buffer,
  IN  UINTN        BufferSize,
  IN  UINTN        Flags,
  IN  CONST CHAR8  *Format,
  IN  VA_LIST      VaListMarker    OPTIONAL,
  IN  BASE_LIST    BaseListMarker  OPTIONAL
  );

/**
  Worker function that produces a Null-terminated string in an output buffer
  based on a Null-terminated format string and variable argument list.

  VSPrint function to process format and place the results in Buffer. Since a
  VA_LIST is used this routine allows the nesting of Vararg routines. Thus
  this is the main print working routine

  @param  StartOfBuffer The character buffer to print the results of the parsing
                        of Format into.
  @param  BufferSize    The maximum number of characters to put into buffer.
                        Zero means no limit.
  @param  Flags         Initial flags value.
                        Can only have FORMAT_UNICODE and OUTPUT_UNICODE set
  @param  FormatString  Null-terminated format string.
  @param  ...           The variable argument list.

  @return The number of characters printed.

**/
UINTN
EFIAPI
BasePrintLibSPrint (
  OUT CHAR8        *StartOfBuffer,
  IN  UINTN        BufferSize,
  IN  UINTN        Flags,
  IN  CONST CHAR8  *FormatString,
  ...
  );

/**
  Internal function that places the character into the Buffer.

  Internal function that places ASCII or Unicode character into the Buffer.

  @param  Buffer      Buffer to place the Unicode or ASCII string.
  @param  EndBuffer   The end of the input Buffer. No characters will be
                      placed after that.
  @param  Length      The count of character to be placed into Buffer.
                      (Negative value indicates no buffer fill.)
  @param  Character   The character to be placed into Buffer.
  @param  Increment   The character increment in Buffer.

  @return Buffer      Buffer filled with the input Character.

**/
CHAR8 *
BasePrintLibFillBuffer (
  OUT CHAR8  *Buffer,
  IN  CHAR8  *EndBuffer,
  IN  INTN   Length,
  IN  UINTN  Character,
  IN  INTN   Increment
  );

/**
  Internal function that convert a number to a string in Buffer.

  Print worker function that converts a decimal or hexadecimal number to an ASCII string in Buffer.

  @param  Buffer    Location to place the ASCII string of Value.
  @param  Value     The value to convert to a Decimal or Hexadecimal string in Buffer.
  @param  Radix     Radix of the value

  @return A pointer to the end of buffer filled with ASCII string.

**/
CHAR8 *
BasePrintLibValueToString (
  IN OUT CHAR8  *Buffer,
  IN INT64      Value,
  IN UINTN      Radix
  );

/**
  Internal function that converts a decimal value to a Null-terminated string.

  Converts the decimal number specified by Value to a Null-terminated
  string specified by Buffer containing at most Width characters.
  If Width is 0 then a width of  MAXIMUM_VALUE_CHARACTERS is assumed.
  The total number of characters placed in Buffer is returned.
  If the conversion contains more than Width characters, then only the first
  Width characters are returned, and the total number of characters
  required to perform the conversion is returned.
  Additional conversion parameters are specified in Flags.
  The Flags bit LEFT_JUSTIFY is always ignored.
  All conversions are left justified in Buffer.
  If Width is 0, PREFIX_ZERO is ignored in Flags.
  If COMMA_TYPE is set in Flags, then PREFIX_ZERO is ignored in Flags, and commas
  are inserted every 3rd digit starting from the right.
  If Value is < 0, then the fist character in Buffer is a '-'.
  If PREFIX_ZERO is set in Flags and PREFIX_ZERO is not being ignored,
  then Buffer is padded with '0' characters so the combination of the optional '-'
  sign character, '0' characters, digit characters for Value, and the Null-terminator
  add up to Width characters.

  If Buffer is NULL, then ASSERT().
  If unsupported bits are set in Flags, then ASSERT().
  If Width >= MAXIMUM_VALUE_CHARACTERS, then ASSERT()

  @param  Buffer    The pointer to the output buffer for the produced Null-terminated
                    string.
  @param  Flags     The bitmask of flags that specify left justification, zero pad,
                    and commas.
  @param  Value     The 64-bit signed value to convert to a string.
  @param  Width     The maximum number of characters to place in Buffer, not including
                    the Null-terminator.
  @param  Increment Character increment in Buffer.

  @return Total number of characters required to perform the conversion.

**/
UINTN
BasePrintLibConvertValueToString (
  IN OUT CHAR8  *Buffer,
  IN UINTN      Flags,
  IN INT64      Value,
  IN UINTN      Width,
  IN UINTN      Increment
  );

/**
  Internal function that converts a decimal value to a Null-terminated string.

  Converts the decimal number specified by Value to a Null-terminated string
  specified by Buffer containing at most Width characters. If Width is 0 then a
  width of MAXIMUM_VALUE_CHARACTERS is assumed. If the conversion contains more
  than Width characters, then only the first Width characters are placed in
  Buffer. Additional conversion parameters are specified in Flags.
  The Flags bit LEFT_JUSTIFY is always ignored.
  All conversions are left justified in Buffer.
  If Width is 0, PREFIX_ZERO is ignored in Flags.
  If COMMA_TYPE is set in Flags, then PREFIX_ZERO is ignored in Flags, and
  commas are inserted every 3rd digit starting from the right.
  If Value is < 0, then the fist character in Buffer is a '-'.
  If PREFIX_ZERO is set in Flags and PREFIX_ZERO is not being ignored,
  then Buffer is padded with '0' characters so the combination of the optional
  '-' sign character, '0' characters, digit characters for Value, and the
  Null-terminator add up to Width characters.

  If an error would be returned, the function will ASSERT().

  @param  Buffer      The pointer to the output buffer for the produced
                      Null-terminated string.
  @param  BufferSize  The size of Buffer in bytes, including the
                      Null-terminator.
  @param  Flags       The bitmask of flags that specify left justification,
                      zero pad, and commas.
  @param  Value       The 64-bit signed value to convert to a string.
  @param  Width       The maximum number of characters to place in Buffer,
                      not including the Null-terminator.
  @param  Increment   The character increment in Buffer.

  @retval RETURN_SUCCESS           The decimal value is converted.
  @retval RETURN_BUFFER_TOO_SMALL  If BufferSize cannot hold the converted
                                   value.
  @retval RETURN_INVALID_PARAMETER If Buffer is NULL.
                                   If Increment is 1 and
                                   PcdMaximumAsciiStringLength is not zero,
                                   BufferSize is greater than
                                   PcdMaximumAsciiStringLength.
                                   If Increment is not 1 and
                                   PcdMaximumUnicodeStringLength is not zero,
                                   BufferSize is greater than
                                   (PcdMaximumUnicodeStringLength *
                                   sizeof (CHAR16) + 1).
                                   If unsupported bits are set in Flags.
                                   If both COMMA_TYPE and RADIX_HEX are set in
                                   Flags.
                                   If Width >= MAXIMUM_VALUE_CHARACTERS.

**/
RETURN_STATUS
BasePrintLibConvertValueToStringS (
  IN OUT CHAR8  *Buffer,
  IN UINTN      BufferSize,
  IN UINTN      Flags,
  IN INT64      Value,
  IN UINTN      Width,
  IN UINTN      Increment
  );

#endif
