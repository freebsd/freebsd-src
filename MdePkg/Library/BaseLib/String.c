/** @file
  Unicode and ASCII string primitives.

  Copyright (c) 2006 - 2017, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "BaseLibInternals.h"

#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function will be deprecated for security reason.

  Copies one Null-terminated Unicode string to another Null-terminated Unicode
  string and returns the new Unicode string.

  This function copies the contents of the Unicode string Source to the Unicode
  string Destination, and returns Destination. If Source and Destination
  overlap, then the results are undefined.

  If Destination is NULL, then ASSERT().
  If Destination is not aligned on a 16-bit boundary, then ASSERT().
  If Source is NULL, then ASSERT().
  If Source is not aligned on a 16-bit boundary, then ASSERT().
  If Source and Destination overlap, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and Source contains more than
  PcdMaximumUnicodeStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().

  @param  Destination A pointer to a Null-terminated Unicode string.
  @param  Source      A pointer to a Null-terminated Unicode string.

  @return Destination.

**/
CHAR16 *
EFIAPI
StrCpy (
  OUT     CHAR16                    *Destination,
  IN      CONST CHAR16              *Source
  )
{
  CHAR16                            *ReturnValue;

  //
  // Destination cannot be NULL
  //
  ASSERT (Destination != NULL);
  ASSERT (((UINTN) Destination & BIT0) == 0);

  //
  // Destination and source cannot overlap
  //
  ASSERT ((UINTN)(Destination - Source) > StrLen (Source));
  ASSERT ((UINTN)(Source - Destination) > StrLen (Source));

  ReturnValue = Destination;
  while (*Source != 0) {
    *(Destination++) = *(Source++);
  }
  *Destination = 0;
  return ReturnValue;
}

/**
  [ATTENTION] This function will be deprecated for security reason.

  Copies up to a specified length from one Null-terminated Unicode string  to 
  another Null-terminated Unicode string and returns the new Unicode string.

  This function copies the contents of the Unicode string Source to the Unicode
  string Destination, and returns Destination. At most, Length Unicode
  characters are copied from Source to Destination. If Length is 0, then
  Destination is returned unmodified. If Length is greater that the number of
  Unicode characters in Source, then Destination is padded with Null Unicode
  characters. If Source and Destination overlap, then the results are
  undefined.

  If Length > 0 and Destination is NULL, then ASSERT().
  If Length > 0 and Destination is not aligned on a 16-bit boundary, then ASSERT().
  If Length > 0 and Source is NULL, then ASSERT().
  If Length > 0 and Source is not aligned on a 16-bit boundary, then ASSERT().
  If Source and Destination overlap, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and Length is greater than 
  PcdMaximumUnicodeStringLength, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and Source contains more than
  PcdMaximumUnicodeStringLength Unicode characters, not including the Null-terminator,
  then ASSERT().

  @param  Destination A pointer to a Null-terminated Unicode string.
  @param  Source      A pointer to a Null-terminated Unicode string.
  @param  Length      The maximum number of Unicode characters to copy.

  @return Destination.

**/
CHAR16 *
EFIAPI
StrnCpy (
  OUT     CHAR16                    *Destination,
  IN      CONST CHAR16              *Source,
  IN      UINTN                     Length
  )
{
  CHAR16                            *ReturnValue;

  if (Length == 0) {
    return Destination;
  }

  //
  // Destination cannot be NULL if Length is not zero
  //
  ASSERT (Destination != NULL);
  ASSERT (((UINTN) Destination & BIT0) == 0);

  //
  // Destination and source cannot overlap
  //
  ASSERT ((UINTN)(Destination - Source) > StrLen (Source));
  ASSERT ((UINTN)(Source - Destination) >= Length);

  if (PcdGet32 (PcdMaximumUnicodeStringLength) != 0) {
    ASSERT (Length <= PcdGet32 (PcdMaximumUnicodeStringLength));
  }

  ReturnValue = Destination;

  while ((*Source != L'\0') && (Length > 0)) {
    *(Destination++) = *(Source++);
    Length--;
  }

  ZeroMem (Destination, Length * sizeof (*Destination));
  return ReturnValue;
}
#endif

/**
  Returns the length of a Null-terminated Unicode string.

  This function returns the number of Unicode characters in the Null-terminated
  Unicode string specified by String.

  If String is NULL, then ASSERT().
  If String is not aligned on a 16-bit boundary, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and String contains more than
  PcdMaximumUnicodeStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().

  @param  String  A pointer to a Null-terminated Unicode string.

  @return The length of String.

**/
UINTN
EFIAPI
StrLen (
  IN      CONST CHAR16              *String
  )
{
  UINTN                             Length;

  ASSERT (String != NULL);
  ASSERT (((UINTN) String & BIT0) == 0);

  for (Length = 0; *String != L'\0'; String++, Length++) {
    //
    // If PcdMaximumUnicodeStringLength is not zero,
    // length should not more than PcdMaximumUnicodeStringLength
    //
    if (PcdGet32 (PcdMaximumUnicodeStringLength) != 0) {
      ASSERT (Length < PcdGet32 (PcdMaximumUnicodeStringLength));
    }
  }
  return Length;
}

/**
  Returns the size of a Null-terminated Unicode string in bytes, including the
  Null terminator.

  This function returns the size, in bytes, of the Null-terminated Unicode string 
  specified by String.

  If String is NULL, then ASSERT().
  If String is not aligned on a 16-bit boundary, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and String contains more than
  PcdMaximumUnicodeStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().

  @param  String  A pointer to a Null-terminated Unicode string.

  @return The size of String.

**/
UINTN
EFIAPI
StrSize (
  IN      CONST CHAR16              *String
  )
{
  return (StrLen (String) + 1) * sizeof (*String);
}

/**
  Compares two Null-terminated Unicode strings, and returns the difference
  between the first mismatched Unicode characters.

  This function compares the Null-terminated Unicode string FirstString to the
  Null-terminated Unicode string SecondString. If FirstString is identical to
  SecondString, then 0 is returned. Otherwise, the value returned is the first
  mismatched Unicode character in SecondString subtracted from the first
  mismatched Unicode character in FirstString.

  If FirstString is NULL, then ASSERT().
  If FirstString is not aligned on a 16-bit boundary, then ASSERT().
  If SecondString is NULL, then ASSERT().
  If SecondString is not aligned on a 16-bit boundary, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and FirstString contains more
  than PcdMaximumUnicodeStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and SecondString contains more
  than PcdMaximumUnicodeStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().

  @param  FirstString   A pointer to a Null-terminated Unicode string.
  @param  SecondString  A pointer to a Null-terminated Unicode string.

  @retval 0      FirstString is identical to SecondString.
  @return others FirstString is not identical to SecondString.

**/
INTN
EFIAPI
StrCmp (
  IN      CONST CHAR16              *FirstString,
  IN      CONST CHAR16              *SecondString
  )
{
  //
  // ASSERT both strings are less long than PcdMaximumUnicodeStringLength
  //
  ASSERT (StrSize (FirstString) != 0);
  ASSERT (StrSize (SecondString) != 0);

  while ((*FirstString != L'\0') && (*FirstString == *SecondString)) {
    FirstString++;
    SecondString++;
  }
  return *FirstString - *SecondString;
}

/**
  Compares up to a specified length the contents of two Null-terminated Unicode strings,
  and returns the difference between the first mismatched Unicode characters.
  
  This function compares the Null-terminated Unicode string FirstString to the
  Null-terminated Unicode string SecondString. At most, Length Unicode
  characters will be compared. If Length is 0, then 0 is returned. If
  FirstString is identical to SecondString, then 0 is returned. Otherwise, the
  value returned is the first mismatched Unicode character in SecondString
  subtracted from the first mismatched Unicode character in FirstString.

  If Length > 0 and FirstString is NULL, then ASSERT().
  If Length > 0 and FirstString is not aligned on a 16-bit boundary, then ASSERT().
  If Length > 0 and SecondString is NULL, then ASSERT().
  If Length > 0 and SecondString is not aligned on a 16-bit boundary, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and Length is greater than
  PcdMaximumUnicodeStringLength, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and FirstString contains more than
  PcdMaximumUnicodeStringLength Unicode characters, not including the Null-terminator,
  then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and SecondString contains more than
  PcdMaximumUnicodeStringLength Unicode characters, not including the Null-terminator,
  then ASSERT().

  @param  FirstString   A pointer to a Null-terminated Unicode string.
  @param  SecondString  A pointer to a Null-terminated Unicode string.
  @param  Length        The maximum number of Unicode characters to compare.

  @retval 0      FirstString is identical to SecondString.
  @return others FirstString is not identical to SecondString.

**/
INTN
EFIAPI
StrnCmp (
  IN      CONST CHAR16              *FirstString,
  IN      CONST CHAR16              *SecondString,
  IN      UINTN                     Length
  )
{
  if (Length == 0) {
    return 0;
  }

  //
  // ASSERT both strings are less long than PcdMaximumUnicodeStringLength.
  // Length tests are performed inside StrLen().
  //
  ASSERT (StrSize (FirstString) != 0);
  ASSERT (StrSize (SecondString) != 0);

  if (PcdGet32 (PcdMaximumUnicodeStringLength) != 0) {
    ASSERT (Length <= PcdGet32 (PcdMaximumUnicodeStringLength));
  }

  while ((*FirstString != L'\0') &&
         (*SecondString != L'\0') &&
         (*FirstString == *SecondString) &&
         (Length > 1)) {
    FirstString++;
    SecondString++;
    Length--;
  }

  return *FirstString - *SecondString;
}

#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function will be deprecated for security reason.

  Concatenates one Null-terminated Unicode string to another Null-terminated
  Unicode string, and returns the concatenated Unicode string.

  This function concatenates two Null-terminated Unicode strings. The contents
  of Null-terminated Unicode string Source are concatenated to the end of
  Null-terminated Unicode string Destination. The Null-terminated concatenated
  Unicode String is returned. If Source and Destination overlap, then the
  results are undefined.

  If Destination is NULL, then ASSERT().
  If Destination is not aligned on a 16-bit boundary, then ASSERT().
  If Source is NULL, then ASSERT().
  If Source is not aligned on a 16-bit boundary, then ASSERT().
  If Source and Destination overlap, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and Destination contains more
  than PcdMaximumUnicodeStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and Source contains more than
  PcdMaximumUnicodeStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and concatenating Destination
  and Source results in a Unicode string with more than
  PcdMaximumUnicodeStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().

  @param  Destination A pointer to a Null-terminated Unicode string.
  @param  Source      A pointer to a Null-terminated Unicode string.

  @return Destination.

**/
CHAR16 *
EFIAPI
StrCat (
  IN OUT  CHAR16                    *Destination,
  IN      CONST CHAR16              *Source
  )
{
  StrCpy (Destination + StrLen (Destination), Source);

  //
  // Size of the resulting string should never be zero.
  // PcdMaximumUnicodeStringLength is tested inside StrLen().
  //
  ASSERT (StrSize (Destination) != 0);
  return Destination;
}

/**
  [ATTENTION] This function will be deprecated for security reason.

  Concatenates up to a specified length one Null-terminated Unicode to the end 
  of another Null-terminated Unicode string, and returns the concatenated 
  Unicode string.

  This function concatenates two Null-terminated Unicode strings. The contents
  of Null-terminated Unicode string Source are concatenated to the end of
  Null-terminated Unicode string Destination, and Destination is returned. At
  most, Length Unicode characters are concatenated from Source to the end of
  Destination, and Destination is always Null-terminated. If Length is 0, then
  Destination is returned unmodified. If Source and Destination overlap, then
  the results are undefined.

  If Destination is NULL, then ASSERT().
  If Length > 0 and Destination is not aligned on a 16-bit boundary, then ASSERT().
  If Length > 0 and Source is NULL, then ASSERT().
  If Length > 0 and Source is not aligned on a 16-bit boundary, then ASSERT().
  If Source and Destination overlap, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and Length is greater than 
  PcdMaximumUnicodeStringLength, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and Destination contains more
  than PcdMaximumUnicodeStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and Source contains more than
  PcdMaximumUnicodeStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and concatenating Destination
  and Source results in a Unicode string with more than PcdMaximumUnicodeStringLength
  Unicode characters, not including the Null-terminator, then ASSERT().

  @param  Destination A pointer to a Null-terminated Unicode string.
  @param  Source      A pointer to a Null-terminated Unicode string.
  @param  Length      The maximum number of Unicode characters to concatenate from
                      Source.

  @return Destination.

**/
CHAR16 *
EFIAPI
StrnCat (
  IN OUT  CHAR16                    *Destination,
  IN      CONST CHAR16              *Source,
  IN      UINTN                     Length
  )
{
  UINTN   DestinationLen;

  DestinationLen = StrLen (Destination);
  StrnCpy (Destination + DestinationLen, Source, Length);
  Destination[DestinationLen + Length] = L'\0';

  //
  // Size of the resulting string should never be zero.
  // PcdMaximumUnicodeStringLength is tested inside StrLen().
  //
  ASSERT (StrSize (Destination) != 0);
  return Destination;
}
#endif

/**
  Returns the first occurrence of a Null-terminated Unicode sub-string
  in a Null-terminated Unicode string.

  This function scans the contents of the Null-terminated Unicode string
  specified by String and returns the first occurrence of SearchString.
  If SearchString is not found in String, then NULL is returned.  If
  the length of SearchString is zero, then String is
  returned.

  If String is NULL, then ASSERT().
  If String is not aligned on a 16-bit boundary, then ASSERT().
  If SearchString is NULL, then ASSERT().
  If SearchString is not aligned on a 16-bit boundary, then ASSERT().

  If PcdMaximumUnicodeStringLength is not zero, and SearchString
  or String contains more than PcdMaximumUnicodeStringLength Unicode
  characters, not including the Null-terminator, then ASSERT().

  @param  String          A pointer to a Null-terminated Unicode string.
  @param  SearchString    A pointer to a Null-terminated Unicode string to search for.

  @retval NULL            If the SearchString does not appear in String.
  @return others          If there is a match.

**/
CHAR16 *
EFIAPI
StrStr (
  IN      CONST CHAR16              *String,
  IN      CONST CHAR16              *SearchString
  )
{
  CONST CHAR16 *FirstMatch;
  CONST CHAR16 *SearchStringTmp;

  //
  // ASSERT both strings are less long than PcdMaximumUnicodeStringLength.
  // Length tests are performed inside StrLen().
  //
  ASSERT (StrSize (String) != 0);
  ASSERT (StrSize (SearchString) != 0);

  if (*SearchString == L'\0') {
    return (CHAR16 *) String;
  }

  while (*String != L'\0') {
    SearchStringTmp = SearchString;
    FirstMatch = String;
    
    while ((*String == *SearchStringTmp) 
            && (*String != L'\0')) {
      String++;
      SearchStringTmp++;
    } 
    
    if (*SearchStringTmp == L'\0') {
      return (CHAR16 *) FirstMatch;
    }

    if (*String == L'\0') {
      return NULL;
    }

    String = FirstMatch + 1;
  }

  return NULL;
}

/**
  Check if a Unicode character is a decimal character.

  This internal function checks if a Unicode character is a 
  decimal character. The valid decimal character is from
  L'0' to L'9'.

  @param  Char  The character to check against.

  @retval TRUE  If the Char is a decmial character.
  @retval FALSE If the Char is not a decmial character.

**/
BOOLEAN
EFIAPI
InternalIsDecimalDigitCharacter (
  IN      CHAR16                    Char
  )
{
  return (BOOLEAN) (Char >= L'0' && Char <= L'9');
}

/**
  Convert a Unicode character to upper case only if 
  it maps to a valid small-case ASCII character.

  This internal function only deal with Unicode character
  which maps to a valid small-case ASCII character, i.e.
  L'a' to L'z'. For other Unicode character, the input character
  is returned directly.

  @param  Char  The character to convert.

  @retval LowerCharacter   If the Char is with range L'a' to L'z'.
  @retval Unchanged        Otherwise.

**/
CHAR16
EFIAPI
InternalCharToUpper (
  IN      CHAR16                    Char
  )
{
  if (Char >= L'a' && Char <= L'z') {
    return (CHAR16) (Char - (L'a' - L'A'));
  }

  return Char;
}

/**
  Convert a Unicode character to numerical value.

  This internal function only deal with Unicode character
  which maps to a valid hexadecimal ASII character, i.e.
  L'0' to L'9', L'a' to L'f' or L'A' to L'F'. For other 
  Unicode character, the value returned does not make sense.

  @param  Char  The character to convert.

  @return The numerical value converted.

**/
UINTN
EFIAPI
InternalHexCharToUintn (
  IN      CHAR16                    Char
  )
{
  if (InternalIsDecimalDigitCharacter (Char)) {
    return Char - L'0';
  }

  return (10 + InternalCharToUpper (Char) - L'A');
}

/**
  Check if a Unicode character is a hexadecimal character.

  This internal function checks if a Unicode character is a 
  decimal character.  The valid hexadecimal character is 
  L'0' to L'9', L'a' to L'f', or L'A' to L'F'.


  @param  Char  The character to check against.

  @retval TRUE  If the Char is a hexadecmial character.
  @retval FALSE If the Char is not a hexadecmial character.

**/
BOOLEAN
EFIAPI
InternalIsHexaDecimalDigitCharacter (
  IN      CHAR16                    Char
  )
{

  return (BOOLEAN) (InternalIsDecimalDigitCharacter (Char) ||
    (Char >= L'A' && Char <= L'F') ||
    (Char >= L'a' && Char <= L'f'));
}

/**
  Convert a Null-terminated Unicode decimal string to a value of
  type UINTN.

  This function returns a value of type UINTN by interpreting the contents
  of the Unicode string specified by String as a decimal number. The format
  of the input Unicode string String is:

                  [spaces] [decimal digits].

  The valid decimal digit character is in the range [0-9]. The
  function will ignore the pad space, which includes spaces or
  tab characters, before [decimal digits]. The running zero in the
  beginning of [decimal digits] will be ignored. Then, the function
  stops at the first character that is a not a valid decimal character
  or a Null-terminator, whichever one comes first.

  If String is NULL, then ASSERT().
  If String is not aligned in a 16-bit boundary, then ASSERT().
  If String has only pad spaces, then 0 is returned.
  If String has no pad spaces or valid decimal digits,
  then 0 is returned.
  If the number represented by String overflows according
  to the range defined by UINTN, then MAX_UINTN is returned.

  If PcdMaximumUnicodeStringLength is not zero, and String contains
  more than PcdMaximumUnicodeStringLength Unicode characters, not including
  the Null-terminator, then ASSERT().

  @param  String      A pointer to a Null-terminated Unicode string.

  @retval Value translated from String.

**/
UINTN
EFIAPI
StrDecimalToUintn (
  IN      CONST CHAR16              *String
  )
{
  UINTN     Result;

  StrDecimalToUintnS (String, (CHAR16 **) NULL, &Result);
  return Result;
}


/**
  Convert a Null-terminated Unicode decimal string to a value of
  type UINT64.

  This function returns a value of type UINT64 by interpreting the contents
  of the Unicode string specified by String as a decimal number. The format
  of the input Unicode string String is:

                  [spaces] [decimal digits].

  The valid decimal digit character is in the range [0-9]. The
  function will ignore the pad space, which includes spaces or
  tab characters, before [decimal digits]. The running zero in the
  beginning of [decimal digits] will be ignored. Then, the function
  stops at the first character that is a not a valid decimal character
  or a Null-terminator, whichever one comes first.

  If String is NULL, then ASSERT().
  If String is not aligned in a 16-bit boundary, then ASSERT().
  If String has only pad spaces, then 0 is returned.
  If String has no pad spaces or valid decimal digits,
  then 0 is returned.
  If the number represented by String overflows according
  to the range defined by UINT64, then MAX_UINT64 is returned.

  If PcdMaximumUnicodeStringLength is not zero, and String contains
  more than PcdMaximumUnicodeStringLength Unicode characters, not including
  the Null-terminator, then ASSERT().

  @param  String          A pointer to a Null-terminated Unicode string.

  @retval Value translated from String.

**/
UINT64
EFIAPI
StrDecimalToUint64 (
  IN      CONST CHAR16              *String
  )
{
  UINT64     Result;
  
  StrDecimalToUint64S (String, (CHAR16 **) NULL, &Result);
  return Result;
}

/**
  Convert a Null-terminated Unicode hexadecimal string to a value of type UINTN.

  This function returns a value of type UINTN by interpreting the contents
  of the Unicode string specified by String as a hexadecimal number.
  The format of the input Unicode string String is:

                  [spaces][zeros][x][hexadecimal digits].

  The valid hexadecimal digit character is in the range [0-9], [a-f] and [A-F].
  The prefix "0x" is optional. Both "x" and "X" is allowed in "0x" prefix.
  If "x" appears in the input string, it must be prefixed with at least one 0.
  The function will ignore the pad space, which includes spaces or tab characters,
  before [zeros], [x] or [hexadecimal digit]. The running zero before [x] or
  [hexadecimal digit] will be ignored. Then, the decoding starts after [x] or the
  first valid hexadecimal digit. Then, the function stops at the first character that is
  a not a valid hexadecimal character or NULL, whichever one comes first.

  If String is NULL, then ASSERT().
  If String is not aligned in a 16-bit boundary, then ASSERT().
  If String has only pad spaces, then zero is returned.
  If String has no leading pad spaces, leading zeros or valid hexadecimal digits,
  then zero is returned.
  If the number represented by String overflows according to the range defined by
  UINTN, then MAX_UINTN is returned.

  If PcdMaximumUnicodeStringLength is not zero, and String contains more than
  PcdMaximumUnicodeStringLength Unicode characters, not including the Null-terminator,
  then ASSERT().

  @param  String          A pointer to a Null-terminated Unicode string.

  @retval Value translated from String.

**/
UINTN
EFIAPI
StrHexToUintn (
  IN      CONST CHAR16              *String
  )
{
  UINTN     Result;

  StrHexToUintnS (String, (CHAR16 **) NULL, &Result);
  return Result;
}


/**
  Convert a Null-terminated Unicode hexadecimal string to a value of type UINT64.

  This function returns a value of type UINT64 by interpreting the contents
  of the Unicode string specified by String as a hexadecimal number.
  The format of the input Unicode string String is

                  [spaces][zeros][x][hexadecimal digits].

  The valid hexadecimal digit character is in the range [0-9], [a-f] and [A-F].
  The prefix "0x" is optional. Both "x" and "X" is allowed in "0x" prefix.
  If "x" appears in the input string, it must be prefixed with at least one 0.
  The function will ignore the pad space, which includes spaces or tab characters,
  before [zeros], [x] or [hexadecimal digit]. The running zero before [x] or
  [hexadecimal digit] will be ignored. Then, the decoding starts after [x] or the
  first valid hexadecimal digit. Then, the function stops at the first character that is
  a not a valid hexadecimal character or NULL, whichever one comes first.

  If String is NULL, then ASSERT().
  If String is not aligned in a 16-bit boundary, then ASSERT().
  If String has only pad spaces, then zero is returned.
  If String has no leading pad spaces, leading zeros or valid hexadecimal digits,
  then zero is returned.
  If the number represented by String overflows according to the range defined by
  UINT64, then MAX_UINT64 is returned.

  If PcdMaximumUnicodeStringLength is not zero, and String contains more than
  PcdMaximumUnicodeStringLength Unicode characters, not including the Null-terminator,
  then ASSERT().

  @param  String          A pointer to a Null-terminated Unicode string.

  @retval Value translated from String.

**/
UINT64
EFIAPI
StrHexToUint64 (
  IN      CONST CHAR16             *String
  )
{
  UINT64    Result;

  StrHexToUint64S (String, (CHAR16 **) NULL, &Result);
  return Result;
}

/**
  Check if a ASCII character is a decimal character.

  This internal function checks if a Unicode character is a 
  decimal character. The valid decimal character is from
  '0' to '9'.

  @param  Char  The character to check against.

  @retval TRUE  If the Char is a decmial character.
  @retval FALSE If the Char is not a decmial character.

**/
BOOLEAN
EFIAPI
InternalAsciiIsDecimalDigitCharacter (
  IN      CHAR8                     Char
  )
{
  return (BOOLEAN) (Char >= '0' && Char <= '9');
}

/**
  Check if a ASCII character is a hexadecimal character.

  This internal function checks if a ASCII character is a 
  decimal character.  The valid hexadecimal character is 
  L'0' to L'9', L'a' to L'f', or L'A' to L'F'.


  @param  Char  The character to check against.

  @retval TRUE  If the Char is a hexadecmial character.
  @retval FALSE If the Char is not a hexadecmial character.

**/
BOOLEAN
EFIAPI
InternalAsciiIsHexaDecimalDigitCharacter (
  IN      CHAR8                    Char
  )
{

  return (BOOLEAN) (InternalAsciiIsDecimalDigitCharacter (Char) ||
    (Char >= 'A' && Char <= 'F') ||
    (Char >= 'a' && Char <= 'f'));
}

#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function is deprecated for security reason.

  Convert a Null-terminated Unicode string to a Null-terminated
  ASCII string and returns the ASCII string.

  This function converts the content of the Unicode string Source
  to the ASCII string Destination by copying the lower 8 bits of
  each Unicode character. It returns Destination.

  The caller is responsible to make sure Destination points to a buffer with size
  equal or greater than ((StrLen (Source) + 1) * sizeof (CHAR8)) in bytes.

  If any Unicode characters in Source contain non-zero value in
  the upper 8 bits, then ASSERT().

  If Destination is NULL, then ASSERT().
  If Source is NULL, then ASSERT().
  If Source is not aligned on a 16-bit boundary, then ASSERT().
  If Source and Destination overlap, then ASSERT().

  If PcdMaximumUnicodeStringLength is not zero, and Source contains
  more than PcdMaximumUnicodeStringLength Unicode characters, not including
  the Null-terminator, then ASSERT().

  If PcdMaximumAsciiStringLength is not zero, and Source contains more
  than PcdMaximumAsciiStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().

  @param  Source        A pointer to a Null-terminated Unicode string.
  @param  Destination   A pointer to a Null-terminated ASCII string.

  @return Destination.

**/
CHAR8 *
EFIAPI
UnicodeStrToAsciiStr (
  IN      CONST CHAR16              *Source,
  OUT     CHAR8                     *Destination
  )
{
  CHAR8                               *ReturnValue;

  ASSERT (Destination != NULL);

  //
  // ASSERT if Source is long than PcdMaximumUnicodeStringLength.
  // Length tests are performed inside StrLen().
  //
  ASSERT (StrSize (Source) != 0);

  //
  // Source and Destination should not overlap
  //
  ASSERT ((UINTN) (Destination - (CHAR8 *) Source) >= StrSize (Source));
  ASSERT ((UINTN) ((CHAR8 *) Source - Destination) > StrLen (Source));


  ReturnValue = Destination;
  while (*Source != '\0') {
    //
    // If any Unicode characters in Source contain 
    // non-zero value in the upper 8 bits, then ASSERT().
    //
    ASSERT (*Source < 0x100);
    *(Destination++) = (CHAR8) *(Source++);
  }

  *Destination = '\0';

  //
  // ASSERT Original Destination is less long than PcdMaximumAsciiStringLength.
  // Length tests are performed inside AsciiStrLen().
  //
  ASSERT (AsciiStrSize (ReturnValue) != 0);

  return ReturnValue;
}

/**
  [ATTENTION] This function will be deprecated for security reason.

  Copies one Null-terminated ASCII string to another Null-terminated ASCII
  string and returns the new ASCII string.

  This function copies the contents of the ASCII string Source to the ASCII
  string Destination, and returns Destination. If Source and Destination
  overlap, then the results are undefined.

  If Destination is NULL, then ASSERT().
  If Source is NULL, then ASSERT().
  If Source and Destination overlap, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and Source contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().

  @param  Destination A pointer to a Null-terminated ASCII string.
  @param  Source      A pointer to a Null-terminated ASCII string.

  @return Destination

**/
CHAR8 *
EFIAPI
AsciiStrCpy (
  OUT     CHAR8                     *Destination,
  IN      CONST CHAR8               *Source
  )
{
  CHAR8                             *ReturnValue;

  //
  // Destination cannot be NULL
  //
  ASSERT (Destination != NULL);

  //
  // Destination and source cannot overlap
  //
  ASSERT ((UINTN)(Destination - Source) > AsciiStrLen (Source));
  ASSERT ((UINTN)(Source - Destination) > AsciiStrLen (Source));

  ReturnValue = Destination;
  while (*Source != 0) {
    *(Destination++) = *(Source++);
  }
  *Destination = 0;
  return ReturnValue;
}

/**
  [ATTENTION] This function will be deprecated for security reason.

  Copies up to a specified length one Null-terminated ASCII string to another 
  Null-terminated ASCII string and returns the new ASCII string.

  This function copies the contents of the ASCII string Source to the ASCII
  string Destination, and returns Destination. At most, Length ASCII characters
  are copied from Source to Destination. If Length is 0, then Destination is
  returned unmodified. If Length is greater that the number of ASCII characters
  in Source, then Destination is padded with Null ASCII characters. If Source
  and Destination overlap, then the results are undefined.

  If Destination is NULL, then ASSERT().
  If Source is NULL, then ASSERT().
  If Source and Destination overlap, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and Length is greater than 
  PcdMaximumAsciiStringLength, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and Source contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().

  @param  Destination A pointer to a Null-terminated ASCII string.
  @param  Source      A pointer to a Null-terminated ASCII string.
  @param  Length      The maximum number of ASCII characters to copy.

  @return Destination

**/
CHAR8 *
EFIAPI
AsciiStrnCpy (
  OUT     CHAR8                     *Destination,
  IN      CONST CHAR8               *Source,
  IN      UINTN                     Length
  )
{
  CHAR8                             *ReturnValue;

  if (Length == 0) {
    return Destination;
  }

  //
  // Destination cannot be NULL
  //
  ASSERT (Destination != NULL);

  //
  // Destination and source cannot overlap
  //
  ASSERT ((UINTN)(Destination - Source) > AsciiStrLen (Source));
  ASSERT ((UINTN)(Source - Destination) >= Length);

  if (PcdGet32 (PcdMaximumAsciiStringLength) != 0) {
    ASSERT (Length <= PcdGet32 (PcdMaximumAsciiStringLength));
  }

  ReturnValue = Destination;

  while (*Source != 0 && Length > 0) {
    *(Destination++) = *(Source++);
    Length--;
  }

  ZeroMem (Destination, Length * sizeof (*Destination));
  return ReturnValue;
}
#endif

/**
  Returns the length of a Null-terminated ASCII string.

  This function returns the number of ASCII characters in the Null-terminated
  ASCII string specified by String.

  If Length > 0 and Destination is NULL, then ASSERT().
  If Length > 0 and Source is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and String contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().

  @param  String  A pointer to a Null-terminated ASCII string.

  @return The length of String.

**/
UINTN
EFIAPI
AsciiStrLen (
  IN      CONST CHAR8               *String
  )
{
  UINTN                             Length;

  ASSERT (String != NULL);

  for (Length = 0; *String != '\0'; String++, Length++) {
    //
    // If PcdMaximumUnicodeStringLength is not zero,
    // length should not more than PcdMaximumUnicodeStringLength
    //
    if (PcdGet32 (PcdMaximumAsciiStringLength) != 0) {
      ASSERT (Length < PcdGet32 (PcdMaximumAsciiStringLength));
    }
  }
  return Length;
}

/**
  Returns the size of a Null-terminated ASCII string in bytes, including the
  Null terminator.

  This function returns the size, in bytes, of the Null-terminated ASCII string
  specified by String.

  If String is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and String contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().

  @param  String  A pointer to a Null-terminated ASCII string.

  @return The size of String.

**/
UINTN
EFIAPI
AsciiStrSize (
  IN      CONST CHAR8               *String
  )
{
  return (AsciiStrLen (String) + 1) * sizeof (*String);
}

/**
  Compares two Null-terminated ASCII strings, and returns the difference
  between the first mismatched ASCII characters.

  This function compares the Null-terminated ASCII string FirstString to the
  Null-terminated ASCII string SecondString. If FirstString is identical to
  SecondString, then 0 is returned. Otherwise, the value returned is the first
  mismatched ASCII character in SecondString subtracted from the first
  mismatched ASCII character in FirstString.

  If FirstString is NULL, then ASSERT().
  If SecondString is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and FirstString contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and SecondString contains more
  than PcdMaximumAsciiStringLength ASCII characters, not including the
  Null-terminator, then ASSERT().

  @param  FirstString   A pointer to a Null-terminated ASCII string.
  @param  SecondString  A pointer to a Null-terminated ASCII string.

  @retval ==0      FirstString is identical to SecondString.
  @retval !=0      FirstString is not identical to SecondString.

**/
INTN
EFIAPI
AsciiStrCmp (
  IN      CONST CHAR8               *FirstString,
  IN      CONST CHAR8               *SecondString
  )
{
  //
  // ASSERT both strings are less long than PcdMaximumAsciiStringLength
  //
  ASSERT (AsciiStrSize (FirstString));
  ASSERT (AsciiStrSize (SecondString));

  while ((*FirstString != '\0') && (*FirstString == *SecondString)) {
    FirstString++;
    SecondString++;
  }

  return *FirstString - *SecondString;
}

/**
  Converts a lowercase Ascii character to upper one.

  If Chr is lowercase Ascii character, then converts it to upper one.

  If Value >= 0xA0, then ASSERT().
  If (Value & 0x0F) >= 0x0A, then ASSERT().

  @param  Chr   one Ascii character

  @return The uppercase value of Ascii character 

**/
CHAR8
EFIAPI
InternalBaseLibAsciiToUpper (
  IN      CHAR8                     Chr
  )
{
  return (UINT8) ((Chr >= 'a' && Chr <= 'z') ? Chr - ('a' - 'A') : Chr);
}

/**
  Convert a ASCII character to numerical value.

  This internal function only deal with Unicode character
  which maps to a valid hexadecimal ASII character, i.e.
  '0' to '9', 'a' to 'f' or 'A' to 'F'. For other 
  ASCII character, the value returned does not make sense.

  @param  Char  The character to convert.

  @return The numerical value converted.

**/
UINTN
EFIAPI
InternalAsciiHexCharToUintn (
  IN      CHAR8                    Char
  )
{
  if (InternalIsDecimalDigitCharacter (Char)) {
    return Char - '0';
  }

  return (10 + InternalBaseLibAsciiToUpper (Char) - 'A');
}


/**
  Performs a case insensitive comparison of two Null-terminated ASCII strings,
  and returns the difference between the first mismatched ASCII characters.

  This function performs a case insensitive comparison of the Null-terminated
  ASCII string FirstString to the Null-terminated ASCII string SecondString. If
  FirstString is identical to SecondString, then 0 is returned. Otherwise, the
  value returned is the first mismatched lower case ASCII character in
  SecondString subtracted from the first mismatched lower case ASCII character
  in FirstString.

  If FirstString is NULL, then ASSERT().
  If SecondString is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and FirstString contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and SecondString contains more
  than PcdMaximumAsciiStringLength ASCII characters, not including the
  Null-terminator, then ASSERT().

  @param  FirstString   A pointer to a Null-terminated ASCII string.
  @param  SecondString  A pointer to a Null-terminated ASCII string.

  @retval ==0    FirstString is identical to SecondString using case insensitive
                 comparisons.
  @retval !=0    FirstString is not identical to SecondString using case
                 insensitive comparisons.

**/
INTN
EFIAPI
AsciiStriCmp (
  IN      CONST CHAR8               *FirstString,
  IN      CONST CHAR8               *SecondString
  )
{
  CHAR8  UpperFirstString;
  CHAR8  UpperSecondString;

  //
  // ASSERT both strings are less long than PcdMaximumAsciiStringLength
  //
  ASSERT (AsciiStrSize (FirstString));
  ASSERT (AsciiStrSize (SecondString));

  UpperFirstString  = InternalBaseLibAsciiToUpper (*FirstString);
  UpperSecondString = InternalBaseLibAsciiToUpper (*SecondString);
  while ((*FirstString != '\0') && (UpperFirstString == UpperSecondString)) {
    FirstString++;
    SecondString++;
    UpperFirstString  = InternalBaseLibAsciiToUpper (*FirstString);
    UpperSecondString = InternalBaseLibAsciiToUpper (*SecondString);
  }

  return UpperFirstString - UpperSecondString;
}

/**
  Compares two Null-terminated ASCII strings with maximum lengths, and returns
  the difference between the first mismatched ASCII characters.

  This function compares the Null-terminated ASCII string FirstString to the
  Null-terminated ASCII  string SecondString. At most, Length ASCII characters
  will be compared. If Length is 0, then 0 is returned. If FirstString is
  identical to SecondString, then 0 is returned. Otherwise, the value returned
  is the first mismatched ASCII character in SecondString subtracted from the
  first mismatched ASCII character in FirstString.

  If Length > 0 and FirstString is NULL, then ASSERT().
  If Length > 0 and SecondString is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and Length is greater than 
  PcdMaximumAsciiStringLength, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and FirstString contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and SecondString contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().

  @param  FirstString   A pointer to a Null-terminated ASCII string.
  @param  SecondString  A pointer to a Null-terminated ASCII string.
  @param  Length        The maximum number of ASCII characters for compare.
  
  @retval ==0       FirstString is identical to SecondString.
  @retval !=0       FirstString is not identical to SecondString.

**/
INTN
EFIAPI
AsciiStrnCmp (
  IN      CONST CHAR8               *FirstString,
  IN      CONST CHAR8               *SecondString,
  IN      UINTN                     Length
  )
{
  if (Length == 0) {
    return 0;
  }

  //
  // ASSERT both strings are less long than PcdMaximumAsciiStringLength
  //
  ASSERT (AsciiStrSize (FirstString));
  ASSERT (AsciiStrSize (SecondString));

  if (PcdGet32 (PcdMaximumAsciiStringLength) != 0) {
    ASSERT (Length <= PcdGet32 (PcdMaximumAsciiStringLength));
  }

  while ((*FirstString != '\0') &&
         (*SecondString != '\0') &&
         (*FirstString == *SecondString) &&
         (Length > 1)) {
    FirstString++;
    SecondString++;
    Length--;
  }
  return *FirstString - *SecondString;
}

#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function will be deprecated for security reason.

  Concatenates one Null-terminated ASCII string to another Null-terminated
  ASCII string, and returns the concatenated ASCII string.

  This function concatenates two Null-terminated ASCII strings. The contents of
  Null-terminated ASCII string Source are concatenated to the end of Null-
  terminated ASCII string Destination. The Null-terminated concatenated ASCII
  String is returned.

  If Destination is NULL, then ASSERT().
  If Source is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and Destination contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and Source contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and concatenating Destination and
  Source results in a ASCII string with more than PcdMaximumAsciiStringLength
  ASCII characters, then ASSERT().

  @param  Destination A pointer to a Null-terminated ASCII string.
  @param  Source      A pointer to a Null-terminated ASCII string.

  @return Destination

**/
CHAR8 *
EFIAPI
AsciiStrCat (
  IN OUT CHAR8    *Destination,
  IN CONST CHAR8  *Source
  )
{
  AsciiStrCpy (Destination + AsciiStrLen (Destination), Source);

  //
  // Size of the resulting string should never be zero.
  // PcdMaximumUnicodeStringLength is tested inside StrLen().
  //
  ASSERT (AsciiStrSize (Destination) != 0);
  return Destination;
}

/**
  [ATTENTION] This function will be deprecated for security reason.

  Concatenates up to a specified length one Null-terminated ASCII string to 
  the end of another Null-terminated ASCII string, and returns the 
  concatenated ASCII string.

  This function concatenates two Null-terminated ASCII strings. The contents
  of Null-terminated ASCII string Source are concatenated to the end of Null-
  terminated ASCII string Destination, and Destination is returned. At most,
  Length ASCII characters are concatenated from Source to the end of
  Destination, and Destination is always Null-terminated. If Length is 0, then
  Destination is returned unmodified. If Source and Destination overlap, then
  the results are undefined.

  If Length > 0 and Destination is NULL, then ASSERT().
  If Length > 0 and Source is NULL, then ASSERT().
  If Source and Destination overlap, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and Length is greater than
  PcdMaximumAsciiStringLength, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and Destination contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and Source contains more than
  PcdMaximumAsciiStringLength ASCII characters, not including the Null-terminator,
  then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and concatenating Destination and
  Source results in a ASCII string with more than PcdMaximumAsciiStringLength
  ASCII characters, not including the Null-terminator, then ASSERT().

  @param  Destination A pointer to a Null-terminated ASCII string.
  @param  Source      A pointer to a Null-terminated ASCII string.
  @param  Length      The maximum number of ASCII characters to concatenate from
                      Source.

  @return Destination

**/
CHAR8 *
EFIAPI
AsciiStrnCat (
  IN OUT  CHAR8                     *Destination,
  IN      CONST CHAR8               *Source,
  IN      UINTN                     Length
  )
{
  UINTN   DestinationLen;

  DestinationLen = AsciiStrLen (Destination);
  AsciiStrnCpy (Destination + DestinationLen, Source, Length);
  Destination[DestinationLen + Length] = '\0';

  //
  // Size of the resulting string should never be zero.
  // PcdMaximumUnicodeStringLength is tested inside StrLen().
  //
  ASSERT (AsciiStrSize (Destination) != 0);
  return Destination;
}
#endif

/**
  Returns the first occurrence of a Null-terminated ASCII sub-string
  in a Null-terminated ASCII string.

  This function scans the contents of the ASCII string specified by String
  and returns the first occurrence of SearchString. If SearchString is not
  found in String, then NULL is returned. If the length of SearchString is zero,
  then String is returned.

  If String is NULL, then ASSERT().
  If SearchString is NULL, then ASSERT().

  If PcdMaximumAsciiStringLength is not zero, and SearchString or
  String contains more than PcdMaximumAsciiStringLength Unicode characters
  not including the Null-terminator, then ASSERT().

  @param  String          A pointer to a Null-terminated ASCII string.
  @param  SearchString    A pointer to a Null-terminated ASCII string to search for.

  @retval NULL            If the SearchString does not appear in String.
  @retval others          If there is a match return the first occurrence of SearchingString.
                          If the length of SearchString is zero,return String.

**/
CHAR8 *
EFIAPI
AsciiStrStr (
  IN      CONST CHAR8               *String,
  IN      CONST CHAR8               *SearchString
  )
{
  CONST CHAR8 *FirstMatch;
  CONST CHAR8 *SearchStringTmp;

  //
  // ASSERT both strings are less long than PcdMaximumAsciiStringLength
  //
  ASSERT (AsciiStrSize (String) != 0);
  ASSERT (AsciiStrSize (SearchString) != 0);

  if (*SearchString == '\0') {
    return (CHAR8 *) String;
  }

  while (*String != '\0') {
    SearchStringTmp = SearchString;
    FirstMatch = String;
    
    while ((*String == *SearchStringTmp) 
            && (*String != '\0')) {
      String++;
      SearchStringTmp++;
    } 
    
    if (*SearchStringTmp == '\0') {
      return (CHAR8 *) FirstMatch;
    }

    if (*String == '\0') {
      return NULL;
    }

    String = FirstMatch + 1;
  }

  return NULL;
}

/**
  Convert a Null-terminated ASCII decimal string to a value of type
  UINTN.

  This function returns a value of type UINTN by interpreting the contents
  of the ASCII string String as a decimal number. The format of the input
  ASCII string String is:

                    [spaces] [decimal digits].

  The valid decimal digit character is in the range [0-9]. The function will
  ignore the pad space, which includes spaces or tab characters, before the digits.
  The running zero in the beginning of [decimal digits] will be ignored. Then, the
  function stops at the first character that is a not a valid decimal character or
  Null-terminator, whichever on comes first.

  If String has only pad spaces, then 0 is returned.
  If String has no pad spaces or valid decimal digits, then 0 is returned.
  If the number represented by String overflows according to the range defined by
  UINTN, then MAX_UINTN is returned.
  If String is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and String contains more than
  PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator,
  then ASSERT().

  @param  String          A pointer to a Null-terminated ASCII string.

  @retval Value translated from String.

**/
UINTN
EFIAPI
AsciiStrDecimalToUintn (
  IN      CONST CHAR8               *String
  )
{
  UINTN     Result;
  
  AsciiStrDecimalToUintnS (String, (CHAR8 **) NULL, &Result);
  return Result;
}


/**
  Convert a Null-terminated ASCII decimal string to a value of type
  UINT64.

  This function returns a value of type UINT64 by interpreting the contents
  of the ASCII string String as a decimal number. The format of the input
  ASCII string String is:

                    [spaces] [decimal digits].

  The valid decimal digit character is in the range [0-9]. The function will
  ignore the pad space, which includes spaces or tab characters, before the digits.
  The running zero in the beginning of [decimal digits] will be ignored. Then, the
  function stops at the first character that is a not a valid decimal character or
  Null-terminator, whichever on comes first.

  If String has only pad spaces, then 0 is returned.
  If String has no pad spaces or valid decimal digits, then 0 is returned.
  If the number represented by String overflows according to the range defined by
  UINT64, then MAX_UINT64 is returned.
  If String is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and String contains more than
  PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator,
  then ASSERT().

  @param  String          A pointer to a Null-terminated ASCII string.

  @retval Value translated from String.

**/
UINT64
EFIAPI
AsciiStrDecimalToUint64 (
  IN      CONST CHAR8               *String
  )
{
  UINT64     Result;
  
  AsciiStrDecimalToUint64S (String, (CHAR8 **) NULL, &Result);
  return Result;
}

/**
  Convert a Null-terminated ASCII hexadecimal string to a value of type UINTN.

  This function returns a value of type UINTN by interpreting the contents of
  the ASCII string String as a hexadecimal number. The format of the input ASCII
  string String is:

                  [spaces][zeros][x][hexadecimal digits].

  The valid hexadecimal digit character is in the range [0-9], [a-f] and [A-F].
  The prefix "0x" is optional. Both "x" and "X" is allowed in "0x" prefix. If "x"
  appears in the input string, it must be prefixed with at least one 0. The function
  will ignore the pad space, which includes spaces or tab characters, before [zeros],
  [x] or [hexadecimal digits]. The running zero before [x] or [hexadecimal digits]
  will be ignored. Then, the decoding starts after [x] or the first valid hexadecimal
  digit. Then, the function stops at the first character that is a not a valid
  hexadecimal character or Null-terminator, whichever on comes first.

  If String has only pad spaces, then 0 is returned.
  If String has no leading pad spaces, leading zeros or valid hexadecimal digits, then
  0 is returned.

  If the number represented by String overflows according to the range defined by UINTN,
  then MAX_UINTN is returned.
  If String is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero,
  and String contains more than PcdMaximumAsciiStringLength ASCII characters not including
  the Null-terminator, then ASSERT().

  @param  String          A pointer to a Null-terminated ASCII string.

  @retval Value translated from String.

**/
UINTN
EFIAPI
AsciiStrHexToUintn (
  IN      CONST CHAR8               *String
  )
{
  UINTN     Result;

  AsciiStrHexToUintnS (String, (CHAR8 **) NULL, &Result);
  return Result;
}


/**
  Convert a Null-terminated ASCII hexadecimal string to a value of type UINT64.

  This function returns a value of type UINT64 by interpreting the contents of
  the ASCII string String as a hexadecimal number. The format of the input ASCII
  string String is:

                  [spaces][zeros][x][hexadecimal digits].

  The valid hexadecimal digit character is in the range [0-9], [a-f] and [A-F].
  The prefix "0x" is optional. Both "x" and "X" is allowed in "0x" prefix. If "x"
  appears in the input string, it must be prefixed with at least one 0. The function
  will ignore the pad space, which includes spaces or tab characters, before [zeros],
  [x] or [hexadecimal digits]. The running zero before [x] or [hexadecimal digits]
  will be ignored. Then, the decoding starts after [x] or the first valid hexadecimal
  digit. Then, the function stops at the first character that is a not a valid
  hexadecimal character or Null-terminator, whichever on comes first.

  If String has only pad spaces, then 0 is returned.
  If String has no leading pad spaces, leading zeros or valid hexadecimal digits, then
  0 is returned.

  If the number represented by String overflows according to the range defined by UINT64,
  then MAX_UINT64 is returned.
  If String is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero,
  and String contains more than PcdMaximumAsciiStringLength ASCII characters not including
  the Null-terminator, then ASSERT().

  @param  String          A pointer to a Null-terminated ASCII string.

  @retval Value translated from String.

**/
UINT64
EFIAPI
AsciiStrHexToUint64 (
  IN      CONST CHAR8                *String
  )
{
  UINT64    Result;

  AsciiStrHexToUint64S (String, (CHAR8 **) NULL, &Result);
  return Result;
}

#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function is deprecated for security reason.

  Convert one Null-terminated ASCII string to a Null-terminated
  Unicode string and returns the Unicode string.

  This function converts the contents of the ASCII string Source to the Unicode
  string Destination, and returns Destination.  The function terminates the
  Unicode string Destination by appending a Null-terminator character at the end.
  The caller is responsible to make sure Destination points to a buffer with size
  equal or greater than ((AsciiStrLen (Source) + 1) * sizeof (CHAR16)) in bytes.

  If Destination is NULL, then ASSERT().
  If Destination is not aligned on a 16-bit boundary, then ASSERT().
  If Source is NULL, then ASSERT().
  If Source and Destination overlap, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero, and Source contains more than
  PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator,
  then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and Source contains more than
  PcdMaximumUnicodeStringLength ASCII characters not including the
  Null-terminator, then ASSERT().

  @param  Source        A pointer to a Null-terminated ASCII string.
  @param  Destination   A pointer to a Null-terminated Unicode string.

  @return Destination.

**/
CHAR16 *
EFIAPI
AsciiStrToUnicodeStr (
  IN      CONST CHAR8               *Source,
  OUT     CHAR16                    *Destination
  )
{
  CHAR16                            *ReturnValue;

  ASSERT (Destination != NULL);

  //
  // ASSERT Source is less long than PcdMaximumAsciiStringLength
  //
  ASSERT (AsciiStrSize (Source) != 0);

  //
  // Source and Destination should not overlap
  //
  ASSERT ((UINTN) ((CHAR8 *) Destination - Source) > AsciiStrLen (Source));
  ASSERT ((UINTN) (Source - (CHAR8 *) Destination) >= (AsciiStrSize (Source) * sizeof (CHAR16)));


  ReturnValue = Destination;
  while (*Source != '\0') {
    *(Destination++) = (CHAR16) *(Source++);
  }
  //
  // End the Destination with a NULL.
  //
  *Destination = '\0';

  //
  // ASSERT Original Destination is less long than PcdMaximumUnicodeStringLength
  //
  ASSERT (StrSize (ReturnValue) != 0);

  return ReturnValue;
}

#endif

/**
  Converts an 8-bit value to an 8-bit BCD value.

  Converts the 8-bit value specified by Value to BCD. The BCD value is
  returned.

  If Value >= 100, then ASSERT().

  @param  Value The 8-bit value to convert to BCD. Range 0..99.

  @return The BCD value.

**/
UINT8
EFIAPI
DecimalToBcd8 (
  IN      UINT8                     Value
  )
{
  ASSERT (Value < 100);
  return (UINT8) (((Value / 10) << 4) | (Value % 10));
}

/**
  Converts an 8-bit BCD value to an 8-bit value.

  Converts the 8-bit BCD value specified by Value to an 8-bit value. The 8-bit
  value is returned.

  If Value >= 0xA0, then ASSERT().
  If (Value & 0x0F) >= 0x0A, then ASSERT().

  @param  Value The 8-bit BCD value to convert to an 8-bit value.

  @return The 8-bit value is returned.

**/
UINT8
EFIAPI
BcdToDecimal8 (
  IN      UINT8                     Value
  )
{
  ASSERT (Value < 0xa0);
  ASSERT ((Value & 0xf) < 0xa);
  return (UINT8) ((Value >> 4) * 10 + (Value & 0xf));
}
