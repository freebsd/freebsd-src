/** @file
  Print Library internal worker functions.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PrintLibInternal.h"

#define WARNING_STATUS_NUMBER         5
#define ERROR_STATUS_NUMBER           33

//
// Safe print checks
//
#define RSIZE_MAX             (PcdGet32 (PcdMaximumUnicodeStringLength))
#define ASCII_RSIZE_MAX       (PcdGet32 (PcdMaximumAsciiStringLength))

#define SAFE_PRINT_CONSTRAINT_CHECK(Expression, RetVal)  \
  do { \
    ASSERT (Expression); \
    if (!(Expression)) { \
      return RetVal; \
    } \
  } while (FALSE)

GLOBAL_REMOVE_IF_UNREFERENCED CONST CHAR8 mHexStr[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

GLOBAL_REMOVE_IF_UNREFERENCED CONST CHAR8 * CONST mStatusString[] = {
  "Success",                      //  RETURN_SUCCESS                = 0
  "Warning Unknown Glyph",        //  RETURN_WARN_UNKNOWN_GLYPH     = 1
  "Warning Delete Failure",       //  RETURN_WARN_DELETE_FAILURE    = 2
  "Warning Write Failure",        //  RETURN_WARN_WRITE_FAILURE     = 3
  "Warning Buffer Too Small",     //  RETURN_WARN_BUFFER_TOO_SMALL  = 4
  "Warning Stale Data",           //  RETURN_WARN_STALE_DATA        = 5
  "Load Error",                   //  RETURN_LOAD_ERROR             = 1  | MAX_BIT
  "Invalid Parameter",            //  RETURN_INVALID_PARAMETER      = 2  | MAX_BIT
  "Unsupported",                  //  RETURN_UNSUPPORTED            = 3  | MAX_BIT
  "Bad Buffer Size",              //  RETURN_BAD_BUFFER_SIZE        = 4  | MAX_BIT
  "Buffer Too Small",             //  RETURN_BUFFER_TOO_SMALL,      = 5  | MAX_BIT
  "Not Ready",                    //  RETURN_NOT_READY              = 6  | MAX_BIT
  "Device Error",                 //  RETURN_DEVICE_ERROR           = 7  | MAX_BIT
  "Write Protected",              //  RETURN_WRITE_PROTECTED        = 8  | MAX_BIT
  "Out of Resources",             //  RETURN_OUT_OF_RESOURCES       = 9  | MAX_BIT
  "Volume Corrupt",               //  RETURN_VOLUME_CORRUPTED       = 10 | MAX_BIT
  "Volume Full",                  //  RETURN_VOLUME_FULL            = 11 | MAX_BIT
  "No Media",                     //  RETURN_NO_MEDIA               = 12 | MAX_BIT
  "Media changed",                //  RETURN_MEDIA_CHANGED          = 13 | MAX_BIT
  "Not Found",                    //  RETURN_NOT_FOUND              = 14 | MAX_BIT
  "Access Denied",                //  RETURN_ACCESS_DENIED          = 15 | MAX_BIT
  "No Response",                  //  RETURN_NO_RESPONSE            = 16 | MAX_BIT
  "No mapping",                   //  RETURN_NO_MAPPING             = 17 | MAX_BIT
  "Time out",                     //  RETURN_TIMEOUT                = 18 | MAX_BIT
  "Not started",                  //  RETURN_NOT_STARTED            = 19 | MAX_BIT
  "Already started",              //  RETURN_ALREADY_STARTED        = 20 | MAX_BIT
  "Aborted",                      //  RETURN_ABORTED                = 21 | MAX_BIT
  "ICMP Error",                   //  RETURN_ICMP_ERROR             = 22 | MAX_BIT
  "TFTP Error",                   //  RETURN_TFTP_ERROR             = 23 | MAX_BIT
  "Protocol Error",               //  RETURN_PROTOCOL_ERROR         = 24 | MAX_BIT
  "Incompatible Version",         //  RETURN_INCOMPATIBLE_VERSION   = 25 | MAX_BIT
  "Security Violation",           //  RETURN_SECURITY_VIOLATION     = 26 | MAX_BIT
  "CRC Error",                    //  RETURN_CRC_ERROR              = 27 | MAX_BIT
  "End of Media",                 //  RETURN_END_OF_MEDIA           = 28 | MAX_BIT
  "Reserved (29)",                //  RESERVED                      = 29 | MAX_BIT
  "Reserved (30)",                //  RESERVED                      = 30 | MAX_BIT
  "End of File",                  //  RETURN_END_OF_FILE            = 31 | MAX_BIT
  "Invalid Language",             //  RETURN_INVALID_LANGUAGE       = 32 | MAX_BIT
  "Compromised Data"              //  RETURN_COMPROMISED_DATA       = 33 | MAX_BIT
};


/**
  Internal function that places the character into the Buffer.

  Internal function that places ASCII or Unicode character into the Buffer.

  @param  Buffer      The buffer to place the Unicode or ASCII string.
  @param  EndBuffer   The end of the input Buffer. No characters will be
                      placed after that.
  @param  Length      The count of character to be placed into Buffer.
                      (Negative value indicates no buffer fill.)
  @param  Character   The character to be placed into Buffer.
  @param  Increment   The character increment in Buffer.

  @return Buffer.

**/
CHAR8 *
BasePrintLibFillBuffer (
  OUT CHAR8   *Buffer,
  IN  CHAR8   *EndBuffer,
  IN  INTN    Length,
  IN  UINTN   Character,
  IN  INTN    Increment
  )
{
  INTN  Index;

  for (Index = 0; Index < Length && Buffer < EndBuffer; Index++) {
    *Buffer = (CHAR8) Character;
    if (Increment != 1) {
      *(Buffer + 1) = (CHAR8)(Character >> 8);
    }
    Buffer += Increment;
  }

  return Buffer;
}

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
  )
{
  UINT32  Remainder;

  //
  // Loop to convert one digit at a time in reverse order
  //
  *Buffer = 0;
  do {
    Value = (INT64)DivU64x32Remainder ((UINT64)Value, (UINT32)Radix, &Remainder);
    *(++Buffer) = mHexStr[Remainder];
  } while (Value != 0);

  //
  // Return pointer of the end of filled buffer.
  //
  return Buffer;
}

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
  @param  Increment The character increment in Buffer.

  @return Total number of characters required to perform the conversion.

**/
UINTN
BasePrintLibConvertValueToString (
  IN OUT CHAR8   *Buffer,
  IN UINTN       Flags,
  IN INT64       Value,
  IN UINTN       Width,
  IN UINTN       Increment
  )
{
  CHAR8  *OriginalBuffer;
  CHAR8  *EndBuffer;
  CHAR8  ValueBuffer[MAXIMUM_VALUE_CHARACTERS];
  CHAR8  *ValueBufferPtr;
  UINTN  Count;
  UINTN  Digits;
  UINTN  Index;
  UINTN  Radix;

  //
  // Make sure Buffer is not NULL and Width < MAXIMUM
  //
  ASSERT (Buffer != NULL);
  ASSERT (Width < MAXIMUM_VALUE_CHARACTERS);
  //
  // Make sure Flags can only contain supported bits.
  //
  ASSERT ((Flags & ~(LEFT_JUSTIFY | COMMA_TYPE | PREFIX_ZERO | RADIX_HEX)) == 0);

  //
  // If both COMMA_TYPE and RADIX_HEX are set, then ASSERT ()
  //
  ASSERT (((Flags & COMMA_TYPE) == 0) || ((Flags & RADIX_HEX) == 0));

  OriginalBuffer = Buffer;

  //
  // Width is 0 or COMMA_TYPE is set, PREFIX_ZERO is ignored.
  //
  if (Width == 0 || (Flags & COMMA_TYPE) != 0) {
    Flags &= ~((UINTN) PREFIX_ZERO);
  }
  //
  // If Width is 0 then a width of  MAXIMUM_VALUE_CHARACTERS is assumed.
  //
  if (Width == 0) {
    Width = MAXIMUM_VALUE_CHARACTERS - 1;
  }
  //
  // Set the tag for the end of the input Buffer.
  //
  EndBuffer = Buffer + Width * Increment;

  //
  // Convert decimal negative
  //
  if ((Value < 0) && ((Flags & RADIX_HEX) == 0)) {
    Value = -Value;
    Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, 1, '-', Increment);
    Width--;
  }

  //
  // Count the length of the value string.
  //
  Radix = ((Flags & RADIX_HEX) == 0)? 10 : 16;
  ValueBufferPtr = BasePrintLibValueToString (ValueBuffer, Value, Radix);
  Count = ValueBufferPtr - ValueBuffer;

  //
  // Append Zero
  //
  if ((Flags & PREFIX_ZERO) != 0) {
    Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, Width - Count, '0', Increment);
  }

  //
  // Print Comma type for every 3 characters
  //
  Digits = Count % 3;
  if (Digits != 0) {
    Digits = 3 - Digits;
  }
  for (Index = 0; Index < Count; Index++) {
    Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, 1, *ValueBufferPtr--, Increment);
    if ((Flags & COMMA_TYPE) != 0) {
      Digits++;
      if (Digits == 3) {
        Digits = 0;
        if ((Index + 1) < Count) {
          Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, 1, ',', Increment);
        }
      }
    }
  }

  //
  // Print Null-terminator
  //
  BasePrintLibFillBuffer (Buffer, EndBuffer + Increment, 1, 0, Increment);

  return ((Buffer - OriginalBuffer) / Increment);
}

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
  IN OUT CHAR8   *Buffer,
  IN UINTN       BufferSize,
  IN UINTN       Flags,
  IN INT64       Value,
  IN UINTN       Width,
  IN UINTN       Increment
  )
{
  CHAR8  *EndBuffer;
  CHAR8  ValueBuffer[MAXIMUM_VALUE_CHARACTERS];
  CHAR8  *ValueBufferPtr;
  UINTN  Count;
  UINTN  Digits;
  UINTN  Index;
  UINTN  Radix;

  //
  // 1. Buffer shall not be a null pointer.
  //
  SAFE_PRINT_CONSTRAINT_CHECK ((Buffer != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. BufferSize shall not be greater than (RSIZE_MAX * sizeof (CHAR16)) for
  //    Unicode output string or shall not be greater than ASCII_RSIZE_MAX for
  //    Ascii output string.
  //
  if (Increment == 1) {
    //
    // Ascii output string
    //
    if (ASCII_RSIZE_MAX != 0) {
      SAFE_PRINT_CONSTRAINT_CHECK ((BufferSize <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
    }
  } else {
    //
    // Unicode output string
    //
    if (RSIZE_MAX != 0) {
      SAFE_PRINT_CONSTRAINT_CHECK ((BufferSize <= RSIZE_MAX * sizeof (CHAR16) + 1), RETURN_INVALID_PARAMETER);
    }
  }

  //
  // 3. Flags shall be set properly.
  //
  SAFE_PRINT_CONSTRAINT_CHECK (((Flags & ~(LEFT_JUSTIFY | COMMA_TYPE | PREFIX_ZERO | RADIX_HEX)) == 0), RETURN_INVALID_PARAMETER);
  SAFE_PRINT_CONSTRAINT_CHECK ((((Flags & COMMA_TYPE) == 0) || ((Flags & RADIX_HEX) == 0)), RETURN_INVALID_PARAMETER);

  //
  // 4. Width shall be smaller than MAXIMUM_VALUE_CHARACTERS.
  //
  SAFE_PRINT_CONSTRAINT_CHECK ((Width < MAXIMUM_VALUE_CHARACTERS), RETURN_INVALID_PARAMETER);

  //
  // Width is 0 or COMMA_TYPE is set, PREFIX_ZERO is ignored.
  //
  if (Width == 0 || (Flags & COMMA_TYPE) != 0) {
    Flags &= ~((UINTN) PREFIX_ZERO);
  }
  //
  // If Width is 0 then a width of MAXIMUM_VALUE_CHARACTERS is assumed.
  //
  if (Width == 0) {
    Width = MAXIMUM_VALUE_CHARACTERS - 1;
  }

  //
  // Count the characters of the output string.
  //
  Count = 0;
  Radix = ((Flags & RADIX_HEX) == 0)? 10 : 16;

  if ((Flags & PREFIX_ZERO) != 0) {
    Count = Width;
  } else {
    if ((Value < 0) && ((Flags & RADIX_HEX) == 0)) {
      Count++;  // minus sign
      ValueBufferPtr = BasePrintLibValueToString (ValueBuffer, -Value, Radix);
    } else {
      ValueBufferPtr = BasePrintLibValueToString (ValueBuffer, Value, Radix);
    }
    Digits = ValueBufferPtr - ValueBuffer;
    Count += Digits;

    if ((Flags & COMMA_TYPE) != 0) {
      Count += (Digits - 1) / 3;  // commas
    }
  }

  Width = MIN (Count, Width);

  //
  // 5. BufferSize shall be large enough to hold the converted string.
  //
  SAFE_PRINT_CONSTRAINT_CHECK ((BufferSize >= (Width + 1) * Increment), RETURN_BUFFER_TOO_SMALL);

  //
  // Set the tag for the end of the input Buffer.
  //
  EndBuffer = Buffer + Width * Increment;

  //
  // Convert decimal negative
  //
  if ((Value < 0) && ((Flags & RADIX_HEX) == 0)) {
    Value = -Value;
    Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, 1, '-', Increment);
    Width--;
  }

  //
  // Count the length of the value string.
  //
  ValueBufferPtr = BasePrintLibValueToString (ValueBuffer, Value, Radix);
  Count = ValueBufferPtr - ValueBuffer;

  //
  // Append Zero
  //
  if ((Flags & PREFIX_ZERO) != 0) {
    Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, Width - Count, '0', Increment);
  }

  //
  // Print Comma type for every 3 characters
  //
  Digits = Count % 3;
  if (Digits != 0) {
    Digits = 3 - Digits;
  }
  for (Index = 0; Index < Count; Index++) {
    Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, 1, *ValueBufferPtr--, Increment);
    if ((Flags & COMMA_TYPE) != 0) {
      Digits++;
      if (Digits == 3) {
        Digits = 0;
        if ((Index + 1) < Count) {
          Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, 1, ',', Increment);
        }
      }
    }
  }

  //
  // Print Null-terminator
  //
  BasePrintLibFillBuffer (Buffer, EndBuffer + Increment, 1, 0, Increment);

  return RETURN_SUCCESS;
}

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
  IN  VA_LIST      VaListMarker,   OPTIONAL
  IN  BASE_LIST    BaseListMarker  OPTIONAL
  )
{
  CHAR8             *OriginalBuffer;
  CHAR8             *EndBuffer;
  CHAR8             ValueBuffer[MAXIMUM_VALUE_CHARACTERS];
  UINT32            BytesPerOutputCharacter;
  UINTN             BytesPerFormatCharacter;
  UINTN             FormatMask;
  UINTN             FormatCharacter;
  UINTN             Width;
  UINTN             Precision;
  INT64             Value;
  CONST CHAR8       *ArgumentString;
  UINTN             Character;
  GUID              *TmpGuid;
  TIME              *TmpTime;
  UINTN             Count;
  UINTN             ArgumentMask;
  INTN              BytesPerArgumentCharacter;
  UINTN             ArgumentCharacter;
  BOOLEAN           Done;
  UINTN             Index;
  CHAR8             Prefix;
  BOOLEAN           ZeroPad;
  BOOLEAN           Comma;
  UINTN             Digits;
  UINTN             Radix;
  RETURN_STATUS     Status;
  UINT32            GuidData1;
  UINT16            GuidData2;
  UINT16            GuidData3;
  UINTN             LengthToReturn;

  //
  // If you change this code be sure to match the 2 versions of this function.
  // Nearly identical logic is found in the BasePrintLib and
  // DxePrintLibPrint2Protocol (both PrintLib instances).
  //

  //
  // 1. Buffer shall not be a null pointer when both BufferSize > 0 and
  //    COUNT_ONLY_NO_PRINT is not set in Flags.
  //
  if ((BufferSize > 0) && ((Flags & COUNT_ONLY_NO_PRINT) == 0)) {
    SAFE_PRINT_CONSTRAINT_CHECK ((Buffer != NULL), 0);
  }

  //
  // 2. Format shall not be a null pointer when BufferSize > 0 or when
  //    COUNT_ONLY_NO_PRINT is set in Flags.
  //
  if ((BufferSize > 0) || ((Flags & COUNT_ONLY_NO_PRINT) != 0)) {
    SAFE_PRINT_CONSTRAINT_CHECK ((Format != NULL), 0);
  }

  //
  // 3. BufferSize shall not be greater than RSIZE_MAX for Unicode output or
  //    ASCII_RSIZE_MAX for Ascii output.
  //
  if ((Flags & OUTPUT_UNICODE) != 0) {
    if (RSIZE_MAX != 0) {
      SAFE_PRINT_CONSTRAINT_CHECK ((BufferSize <= RSIZE_MAX), 0);
    }
    BytesPerOutputCharacter = 2;
  } else {
    if (ASCII_RSIZE_MAX != 0) {
      SAFE_PRINT_CONSTRAINT_CHECK ((BufferSize <= ASCII_RSIZE_MAX), 0);
    }
    BytesPerOutputCharacter = 1;
  }

  //
  // 4. Format shall not contain more than RSIZE_MAX Unicode characters or
  //    ASCII_RSIZE_MAX Ascii characters.
  //
  if ((Flags & FORMAT_UNICODE) != 0) {
    if (RSIZE_MAX != 0) {
      SAFE_PRINT_CONSTRAINT_CHECK ((StrnLenS ((CHAR16 *)Format, RSIZE_MAX + 1) <= RSIZE_MAX), 0);
    }
    BytesPerFormatCharacter = 2;
    FormatMask = 0xffff;
  } else {
    if (ASCII_RSIZE_MAX != 0) {
      SAFE_PRINT_CONSTRAINT_CHECK ((AsciiStrnLenS (Format, ASCII_RSIZE_MAX + 1) <= ASCII_RSIZE_MAX), 0);
    }
    BytesPerFormatCharacter = 1;
    FormatMask = 0xff;
  }

  if ((Flags & COUNT_ONLY_NO_PRINT) != 0) {
    if (BufferSize == 0) {
      Buffer = NULL;
    }
  } else {
    //
    // We can run without a Buffer for counting only.
    //
    if (BufferSize == 0) {
      return 0;
    }
  }

  LengthToReturn = 0;
  EndBuffer = NULL;
  OriginalBuffer = NULL;

  //
  // Reserve space for the Null terminator.
  //
  if (Buffer != NULL) {
    BufferSize--;
    OriginalBuffer = Buffer;

    //
    // Set the tag for the end of the input Buffer.
    //
    EndBuffer = Buffer + BufferSize * BytesPerOutputCharacter;
  }

  //
  // Get the first character from the format string
  //
  FormatCharacter = ((*Format & 0xff) | ((BytesPerFormatCharacter == 1) ? 0 : (*(Format + 1) << 8))) & FormatMask;

  //
  // Loop until the end of the format string is reached or the output buffer is full
  //
  while (FormatCharacter != 0) {
    if ((Buffer != NULL) && (Buffer >= EndBuffer)) {
      break;
    }
    //
    // Clear all the flag bits except those that may have been passed in
    //
    Flags &= (UINTN) (OUTPUT_UNICODE | FORMAT_UNICODE | COUNT_ONLY_NO_PRINT);

    //
    // Set the default width to zero, and the default precision to 1
    //
    Width     = 0;
    Precision = 1;
    Prefix    = 0;
    Comma     = FALSE;
    ZeroPad   = FALSE;
    Count     = 0;
    Digits    = 0;

    switch (FormatCharacter) {
    case '%':
      //
      // Parse Flags and Width
      //
      for (Done = FALSE; !Done; ) {
        Format += BytesPerFormatCharacter;
        FormatCharacter = ((*Format & 0xff) | ((BytesPerFormatCharacter == 1) ? 0 : (*(Format + 1) << 8))) & FormatMask;
        switch (FormatCharacter) {
        case '.':
          Flags |= PRECISION;
          break;
        case '-':
          Flags |= LEFT_JUSTIFY;
          break;
        case '+':
          Flags |= PREFIX_SIGN;
          break;
        case ' ':
          Flags |= PREFIX_BLANK;
          break;
        case ',':
          Flags |= COMMA_TYPE;
          break;
        case 'L':
        case 'l':
          Flags |= LONG_TYPE;
          break;
        case '*':
          if ((Flags & PRECISION) == 0) {
            Flags |= PAD_TO_WIDTH;
            if (BaseListMarker == NULL) {
              Width = VA_ARG (VaListMarker, UINTN);
            } else {
              Width = BASE_ARG (BaseListMarker, UINTN);
            }
          } else {
            if (BaseListMarker == NULL) {
              Precision = VA_ARG (VaListMarker, UINTN);
            } else {
              Precision = BASE_ARG (BaseListMarker, UINTN);
            }
          }
          break;
        case '0':
          if ((Flags & PRECISION) == 0) {
            Flags |= PREFIX_ZERO;
          }
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          for (Count = 0; ((FormatCharacter >= '0') &&  (FormatCharacter <= '9')); ){
            Count = (Count * 10) + FormatCharacter - '0';
            Format += BytesPerFormatCharacter;
            FormatCharacter = ((*Format & 0xff) | ((BytesPerFormatCharacter == 1) ? 0 : (*(Format + 1) << 8))) & FormatMask;
          }
          Format -= BytesPerFormatCharacter;
          if ((Flags & PRECISION) == 0) {
            Flags |= PAD_TO_WIDTH;
            Width = Count;
          } else {
            Precision = Count;
          }
          break;

        case '\0':
          //
          // Make no output if Format string terminates unexpectedly when
          // looking up for flag, width, precision and type.
          //
          Format   -= BytesPerFormatCharacter;
          Precision = 0;
          //
          // break skipped on purpose.
          //
        default:
          Done = TRUE;
          break;
        }
      }

      //
      // Handle each argument type
      //
      switch (FormatCharacter) {
      case 'p':
        //
        // Flag space, +, 0, L & l are invalid for type p.
        //
        Flags &= ~((UINTN) (PREFIX_BLANK | PREFIX_SIGN | PREFIX_ZERO | LONG_TYPE));
        if (sizeof (VOID *) > 4) {
          Flags |= LONG_TYPE;
        }
        //
        // break skipped on purpose
        //
      case 'X':
        Flags |= PREFIX_ZERO;
        //
        // break skipped on purpose
        //
      case 'x':
        Flags |= RADIX_HEX;
        //
        // break skipped on purpose
        //
      case 'u':
        if ((Flags & RADIX_HEX) == 0) {
          Flags &= ~((UINTN) (PREFIX_SIGN));
          Flags |= UNSIGNED_TYPE;
        }
        //
        // break skipped on purpose
        //
      case 'd':
        if ((Flags & LONG_TYPE) == 0) {
          //
          // 'd', 'u', 'x', and 'X' that are not preceded by 'l' or 'L' are assumed to be type "int".
          // This assumption is made so the format string definition is compatible with the ANSI C
          // Specification for formatted strings.  It is recommended that the Base Types be used
          // everywhere, but in this one case, compliance with ANSI C is more important, and
          // provides an implementation that is compatible with that largest possible set of CPU
          // architectures.  This is why the type "int" is used in this one case.
          //
          if (BaseListMarker == NULL) {
            Value = VA_ARG (VaListMarker, int);
          } else {
            Value = BASE_ARG (BaseListMarker, int);
          }
        } else {
          if (BaseListMarker == NULL) {
            Value = VA_ARG (VaListMarker, INT64);
          } else {
            Value = BASE_ARG (BaseListMarker, INT64);
          }
        }
        if ((Flags & PREFIX_BLANK) != 0) {
          Prefix = ' ';
        }
        if ((Flags & PREFIX_SIGN) != 0) {
          Prefix = '+';
        }
        if ((Flags & COMMA_TYPE) != 0) {
          Comma = TRUE;
        }
        if ((Flags & RADIX_HEX) == 0) {
          Radix = 10;
          if (Comma) {
            Flags &= ~((UINTN) PREFIX_ZERO);
            Precision = 1;
          }
          if (Value < 0 && (Flags & UNSIGNED_TYPE) == 0) {
            Flags |= PREFIX_SIGN;
            Prefix = '-';
            Value = -Value;
          } else if ((Flags & UNSIGNED_TYPE) != 0 && (Flags & LONG_TYPE) == 0) {
            //
            // 'd', 'u', 'x', and 'X' that are not preceded by 'l' or 'L' are assumed to be type "int".
            // This assumption is made so the format string definition is compatible with the ANSI C
            // Specification for formatted strings.  It is recommended that the Base Types be used
            // everywhere, but in this one case, compliance with ANSI C is more important, and
            // provides an implementation that is compatible with that largest possible set of CPU
            // architectures.  This is why the type "unsigned int" is used in this one case.
            //
            Value = (unsigned int)Value;
          }
        } else {
          Radix = 16;
          Comma = FALSE;
          if ((Flags & LONG_TYPE) == 0 && Value < 0) {
            //
            // 'd', 'u', 'x', and 'X' that are not preceded by 'l' or 'L' are assumed to be type "int".
            // This assumption is made so the format string definition is compatible with the ANSI C
            // Specification for formatted strings.  It is recommended that the Base Types be used
            // everywhere, but in this one case, compliance with ANSI C is more important, and
            // provides an implementation that is compatible with that largest possible set of CPU
            // architectures.  This is why the type "unsigned int" is used in this one case.
            //
            Value = (unsigned int)Value;
          }
        }
        //
        // Convert Value to a reversed string
        //
        Count = BasePrintLibValueToString (ValueBuffer, Value, Radix) - ValueBuffer;
        if (Value == 0 && Precision == 0) {
          Count = 0;
        }
        ArgumentString = (CHAR8 *)ValueBuffer + Count;

        Digits = Count % 3;
        if (Digits != 0) {
          Digits = 3 - Digits;
        }
        if (Comma && Count != 0) {
          Count += ((Count - 1) / 3);
        }
        if (Prefix != 0) {
          Count++;
          Precision++;
        }
        Flags |= ARGUMENT_REVERSED;
        ZeroPad = TRUE;
        if ((Flags & PREFIX_ZERO) != 0) {
          if ((Flags & LEFT_JUSTIFY) == 0) {
            if ((Flags & PAD_TO_WIDTH) != 0) {
              if ((Flags & PRECISION) == 0) {
                Precision = Width;
              }
            }
          }
        }
        break;

      case 's':
      case 'S':
        Flags |= ARGUMENT_UNICODE;
        //
        // break skipped on purpose
        //
      case 'a':
        if (BaseListMarker == NULL) {
          ArgumentString = VA_ARG (VaListMarker, CHAR8 *);
        } else {
          ArgumentString = BASE_ARG (BaseListMarker, CHAR8 *);
        }
        if (ArgumentString == NULL) {
          Flags &= ~((UINTN) ARGUMENT_UNICODE);
          ArgumentString = "<null string>";
        }
        //
        // Set the default precision for string to be zero if not specified.
        //
        if ((Flags & PRECISION) == 0) {
          Precision = 0;
        }
        break;

      case 'c':
        if (BaseListMarker == NULL) {
          Character = VA_ARG (VaListMarker, UINTN) & 0xffff;
        } else {
          Character = BASE_ARG (BaseListMarker, UINTN) & 0xffff;
        }
        ArgumentString = (CHAR8 *)&Character;
        Flags |= ARGUMENT_UNICODE;
        break;

      case 'g':
        if (BaseListMarker == NULL) {
          TmpGuid = VA_ARG (VaListMarker, GUID *);
        } else {
          TmpGuid = BASE_ARG (BaseListMarker, GUID *);
        }
        if (TmpGuid == NULL) {
          ArgumentString = "<null guid>";
        } else {
          GuidData1 = ReadUnaligned32 (&(TmpGuid->Data1));
          GuidData2 = ReadUnaligned16 (&(TmpGuid->Data2));
          GuidData3 = ReadUnaligned16 (&(TmpGuid->Data3));
          BasePrintLibSPrint (
            ValueBuffer,
            MAXIMUM_VALUE_CHARACTERS,
            0,
            "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            GuidData1,
            GuidData2,
            GuidData3,
            TmpGuid->Data4[0],
            TmpGuid->Data4[1],
            TmpGuid->Data4[2],
            TmpGuid->Data4[3],
            TmpGuid->Data4[4],
            TmpGuid->Data4[5],
            TmpGuid->Data4[6],
            TmpGuid->Data4[7]
            );
          ArgumentString = ValueBuffer;
        }
        break;

      case 't':
        if (BaseListMarker == NULL) {
          TmpTime = VA_ARG (VaListMarker, TIME *);
        } else {
          TmpTime = BASE_ARG (BaseListMarker, TIME *);
        }
        if (TmpTime == NULL) {
          ArgumentString = "<null time>";
        } else {
          BasePrintLibSPrint (
            ValueBuffer,
            MAXIMUM_VALUE_CHARACTERS,
            0,
            "%02d/%02d/%04d  %02d:%02d",
            TmpTime->Month,
            TmpTime->Day,
            TmpTime->Year,
            TmpTime->Hour,
            TmpTime->Minute
            );
          ArgumentString = ValueBuffer;
        }
        break;

      case 'r':
        if (BaseListMarker == NULL) {
          Status = VA_ARG (VaListMarker, RETURN_STATUS);
        } else {
          Status = BASE_ARG (BaseListMarker, RETURN_STATUS);
        }
        ArgumentString = ValueBuffer;
        if (RETURN_ERROR (Status)) {
          //
          // Clear error bit
          //
          Index = Status & ~MAX_BIT;
          if (Index > 0 && Index <= ERROR_STATUS_NUMBER) {
            ArgumentString = mStatusString [Index + WARNING_STATUS_NUMBER];
          }
        } else {
          Index = Status;
          if (Index <= WARNING_STATUS_NUMBER) {
            ArgumentString = mStatusString [Index];
          }
        }
        if (ArgumentString == ValueBuffer) {
          BasePrintLibSPrint ((CHAR8 *) ValueBuffer, MAXIMUM_VALUE_CHARACTERS, 0, "%08X", Status);
        }
        break;

      case '\r':
        Format += BytesPerFormatCharacter;
        FormatCharacter = ((*Format & 0xff) | ((BytesPerFormatCharacter == 1) ? 0 : (*(Format + 1) << 8))) & FormatMask;
        if (FormatCharacter == '\n') {
          //
          // Translate '\r\n' to '\r\n'
          //
          ArgumentString = "\r\n";
        } else {
          //
          // Translate '\r' to '\r'
          //
          ArgumentString = "\r";
          Format   -= BytesPerFormatCharacter;
        }
        break;

      case '\n':
        //
        // Translate '\n' to '\r\n' and '\n\r' to '\r\n'
        //
        ArgumentString = "\r\n";
        Format += BytesPerFormatCharacter;
        FormatCharacter = ((*Format & 0xff) | ((BytesPerFormatCharacter == 1) ? 0 : (*(Format + 1) << 8))) & FormatMask;
        if (FormatCharacter != '\r') {
          Format   -= BytesPerFormatCharacter;
        }
        break;

      case '%':
      default:
        //
        // if the type is '%' or unknown, then print it to the screen
        //
        ArgumentString = (CHAR8 *)&FormatCharacter;
        Flags |= ARGUMENT_UNICODE;
        break;
      }
      break;

    case '\r':
      Format += BytesPerFormatCharacter;
      FormatCharacter = ((*Format & 0xff) | ((BytesPerFormatCharacter == 1) ? 0 : (*(Format + 1) << 8))) & FormatMask;
      if (FormatCharacter == '\n') {
        //
        // Translate '\r\n' to '\r\n'
        //
        ArgumentString = "\r\n";
      } else {
        //
        // Translate '\r' to '\r'
        //
        ArgumentString = "\r";
        Format   -= BytesPerFormatCharacter;
      }
      break;

    case '\n':
      //
      // Translate '\n' to '\r\n' and '\n\r' to '\r\n'
      //
      ArgumentString = "\r\n";
      Format += BytesPerFormatCharacter;
      FormatCharacter = ((*Format & 0xff) | ((BytesPerFormatCharacter == 1) ? 0 : (*(Format + 1) << 8))) & FormatMask;
      if (FormatCharacter != '\r') {
        Format   -= BytesPerFormatCharacter;
      }
      break;

    default:
      ArgumentString = (CHAR8 *)&FormatCharacter;
      Flags |= ARGUMENT_UNICODE;
      break;
    }

    //
    // Retrieve the ArgumentString attriubutes
    //
    if ((Flags & ARGUMENT_UNICODE) != 0) {
      ArgumentMask = 0xffff;
      BytesPerArgumentCharacter = 2;
    } else {
      ArgumentMask = 0xff;
      BytesPerArgumentCharacter = 1;
    }
    if ((Flags & ARGUMENT_REVERSED) != 0) {
      BytesPerArgumentCharacter = -BytesPerArgumentCharacter;
    } else {
      //
      // Compute the number of characters in ArgumentString and store it in Count
      // ArgumentString is either null-terminated, or it contains Precision characters
      //
      for (Count = 0;
            (ArgumentString[Count * BytesPerArgumentCharacter] != '\0' ||
             (BytesPerArgumentCharacter > 1 &&
              ArgumentString[Count * BytesPerArgumentCharacter + 1]!= '\0')) &&
            (Count < Precision || ((Flags & PRECISION) == 0));
              Count++) {
        ArgumentCharacter = ((ArgumentString[Count * BytesPerArgumentCharacter] & 0xff) | ((ArgumentString[Count * BytesPerArgumentCharacter + 1]) << 8)) & ArgumentMask;
        if (ArgumentCharacter == 0) {
          break;
        }
      }
    }

    if (Precision < Count) {
      Precision = Count;
    }

    //
    // Pad before the string
    //
    if ((Flags & (PAD_TO_WIDTH | LEFT_JUSTIFY)) == (PAD_TO_WIDTH)) {
      LengthToReturn += ((Width - Precision) * BytesPerOutputCharacter);
      if ((Flags & COUNT_ONLY_NO_PRINT) == 0 && Buffer != NULL) {
        Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, Width - Precision, ' ', BytesPerOutputCharacter);
      }
    }

    if (ZeroPad) {
      if (Prefix != 0) {
        LengthToReturn += (1 * BytesPerOutputCharacter);
        if ((Flags & COUNT_ONLY_NO_PRINT) == 0 && Buffer != NULL) {
          Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, 1, Prefix, BytesPerOutputCharacter);
        }
      }
      LengthToReturn += ((Precision - Count) * BytesPerOutputCharacter);
      if ((Flags & COUNT_ONLY_NO_PRINT) == 0 && Buffer != NULL) {
        Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, Precision - Count, '0', BytesPerOutputCharacter);
      }
    } else {
      LengthToReturn += ((Precision - Count) * BytesPerOutputCharacter);
      if ((Flags & COUNT_ONLY_NO_PRINT) == 0 && Buffer != NULL) {
        Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, Precision - Count, ' ', BytesPerOutputCharacter);
      }
      if (Prefix != 0) {
        LengthToReturn += (1 * BytesPerOutputCharacter);
        if ((Flags & COUNT_ONLY_NO_PRINT) == 0 && Buffer != NULL) {
          Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, 1, Prefix, BytesPerOutputCharacter);
        }
      }
    }

    //
    // Output the Prefix character if it is present
    //
    Index = 0;
    if (Prefix != 0) {
      Index++;
    }

    //
    // Copy the string into the output buffer performing the required type conversions
    //
    while (Index < Count &&
           (ArgumentString[0] != '\0' ||
            (BytesPerArgumentCharacter > 1 && ArgumentString[1] != '\0'))) {
      ArgumentCharacter = ((*ArgumentString & 0xff) | (((UINT8)*(ArgumentString + 1)) << 8)) & ArgumentMask;

      LengthToReturn += (1 * BytesPerOutputCharacter);
      if ((Flags & COUNT_ONLY_NO_PRINT) == 0 && Buffer != NULL) {
        Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, 1, ArgumentCharacter, BytesPerOutputCharacter);
      }
      ArgumentString    += BytesPerArgumentCharacter;
      Index++;
      if (Comma) {
        Digits++;
        if (Digits == 3) {
          Digits = 0;
          Index++;
          if (Index < Count) {
            LengthToReturn += (1 * BytesPerOutputCharacter);
            if ((Flags & COUNT_ONLY_NO_PRINT) == 0 && Buffer != NULL) {
              Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, 1, ',', BytesPerOutputCharacter);
            }
          }
        }
      }
    }

    //
    // Pad after the string
    //
    if ((Flags & (PAD_TO_WIDTH | LEFT_JUSTIFY)) == (PAD_TO_WIDTH | LEFT_JUSTIFY)) {
      LengthToReturn += ((Width - Precision) * BytesPerOutputCharacter);
      if ((Flags & COUNT_ONLY_NO_PRINT) == 0 && Buffer != NULL) {
        Buffer = BasePrintLibFillBuffer (Buffer, EndBuffer, Width - Precision, ' ', BytesPerOutputCharacter);
      }
    }

    //
    // Get the next character from the format string
    //
    Format += BytesPerFormatCharacter;

    //
    // Get the next character from the format string
    //
    FormatCharacter = ((*Format & 0xff) | ((BytesPerFormatCharacter == 1) ? 0 : (*(Format + 1) << 8))) & FormatMask;
  }

  if ((Flags & COUNT_ONLY_NO_PRINT) != 0) {
    return (LengthToReturn / BytesPerOutputCharacter);
  }

  ASSERT (Buffer != NULL);
  //
  // Null terminate the Unicode or ASCII string
  //
  BasePrintLibFillBuffer (Buffer, EndBuffer + BytesPerOutputCharacter, 1, 0, BytesPerOutputCharacter);

  return ((Buffer - OriginalBuffer) / BytesPerOutputCharacter);
}

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
  @param  FormatString  A Null-terminated format string.
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
  )
{
  VA_LIST  Marker;
  UINTN    NumberOfPrinted;

  VA_START (Marker, FormatString);
  NumberOfPrinted = BasePrintLibSPrintMarker (StartOfBuffer, BufferSize, Flags, FormatString, Marker, NULL);
  VA_END (Marker);
  return NumberOfPrinted;
}
