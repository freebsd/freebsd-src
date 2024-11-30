/** @file
  Safe String functions.

  Copyright (c) 2014 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

#define RSIZE_MAX  (PcdGet32 (PcdMaximumUnicodeStringLength))

#define ASCII_RSIZE_MAX  (PcdGet32 (PcdMaximumAsciiStringLength))

#define SAFE_STRING_CONSTRAINT_CHECK(Expression, Status)  \
  do { \
    if (!(Expression)) { \
      DEBUG ((DEBUG_VERBOSE, \
        "%a(%d) %a: SAFE_STRING_CONSTRAINT_CHECK(%a) failed.  Return %r\n", \
        __FILE__, DEBUG_LINE_NUMBER, __func__, DEBUG_EXPRESSION_STRING (Expression), Status)); \
      return Status; \
    } \
  } while (FALSE)

/**
  Returns if 2 memory blocks are overlapped.

  @param  Base1  Base address of 1st memory block.
  @param  Size1  Size of 1st memory block.
  @param  Base2  Base address of 2nd memory block.
  @param  Size2  Size of 2nd memory block.

  @retval TRUE  2 memory blocks are overlapped.
  @retval FALSE 2 memory blocks are not overlapped.
**/
BOOLEAN
InternalSafeStringIsOverlap (
  IN VOID   *Base1,
  IN UINTN  Size1,
  IN VOID   *Base2,
  IN UINTN  Size2
  )
{
  if ((((UINTN)Base1 >= (UINTN)Base2) && ((UINTN)Base1 < (UINTN)Base2 + Size2)) ||
      (((UINTN)Base2 >= (UINTN)Base1) && ((UINTN)Base2 < (UINTN)Base1 + Size1)))
  {
    return TRUE;
  }

  return FALSE;
}

/**
  Returns if 2 Unicode strings are not overlapped.

  @param  Str1   Start address of 1st Unicode string.
  @param  Size1  The number of char in 1st Unicode string,
                 including terminating null char.
  @param  Str2   Start address of 2nd Unicode string.
  @param  Size2  The number of char in 2nd Unicode string,
                 including terminating null char.

  @retval TRUE  2 Unicode strings are NOT overlapped.
  @retval FALSE 2 Unicode strings are overlapped.
**/
BOOLEAN
InternalSafeStringNoStrOverlap (
  IN CHAR16  *Str1,
  IN UINTN   Size1,
  IN CHAR16  *Str2,
  IN UINTN   Size2
  )
{
  return !InternalSafeStringIsOverlap (Str1, Size1 * sizeof (CHAR16), Str2, Size2 * sizeof (CHAR16));
}

/**
  Returns if 2 Ascii strings are not overlapped.

  @param  Str1   Start address of 1st Ascii string.
  @param  Size1  The number of char in 1st Ascii string,
                 including terminating null char.
  @param  Str2   Start address of 2nd Ascii string.
  @param  Size2  The number of char in 2nd Ascii string,
                 including terminating null char.

  @retval TRUE  2 Ascii strings are NOT overlapped.
  @retval FALSE 2 Ascii strings are overlapped.
**/
BOOLEAN
InternalSafeStringNoAsciiStrOverlap (
  IN CHAR8  *Str1,
  IN UINTN  Size1,
  IN CHAR8  *Str2,
  IN UINTN  Size2
  )
{
  return !InternalSafeStringIsOverlap (Str1, Size1, Str2, Size2);
}

/**
  Returns the length of a Null-terminated Unicode string.

  This function is similar as strlen_s defined in C11.

  If String is not aligned on a 16-bit boundary, then ASSERT().

  @param  String   A pointer to a Null-terminated Unicode string.
  @param  MaxSize  The maximum number of Destination Unicode
                   char, including terminating null char.

  @retval 0        If String is NULL.
  @retval MaxSize  If there is no null character in the first MaxSize characters of String.
  @return The number of characters that percede the terminating null character.

**/
UINTN
EFIAPI
StrnLenS (
  IN CONST CHAR16  *String,
  IN UINTN         MaxSize
  )
{
  UINTN  Length;

  ASSERT (((UINTN)String & BIT0) == 0);

  //
  // If String is a null pointer or MaxSize is 0, then the StrnLenS function returns zero.
  //
  if ((String == NULL) || (MaxSize == 0)) {
    return 0;
  }

  //
  // Otherwise, the StrnLenS function returns the number of characters that precede the
  // terminating null character. If there is no null character in the first MaxSize characters of
  // String then StrnLenS returns MaxSize. At most the first MaxSize characters of String shall
  // be accessed by StrnLenS.
  //
  Length = 0;
  while (String[Length] != 0) {
    if (Length >= MaxSize - 1) {
      return MaxSize;
    }

    Length++;
  }

  return Length;
}

/**
  Returns the size of a Null-terminated Unicode string in bytes, including the
  Null terminator.

  This function returns the size of the Null-terminated Unicode string
  specified by String in bytes, including the Null terminator.

  If String is not aligned on a 16-bit boundary, then ASSERT().

  @param  String   A pointer to a Null-terminated Unicode string.
  @param  MaxSize  The maximum number of Destination Unicode
                   char, including the Null terminator.

  @retval 0  If String is NULL.
  @retval (sizeof (CHAR16) * (MaxSize + 1))
             If there is no Null terminator in the first MaxSize characters of
             String.
  @return The size of the Null-terminated Unicode string in bytes, including
          the Null terminator.

**/
UINTN
EFIAPI
StrnSizeS (
  IN CONST CHAR16  *String,
  IN UINTN         MaxSize
  )
{
  //
  // If String is a null pointer, then the StrnSizeS function returns zero.
  //
  if (String == NULL) {
    return 0;
  }

  //
  // Otherwise, the StrnSizeS function returns the size of the Null-terminated
  // Unicode string in bytes, including the Null terminator. If there is no
  // Null terminator in the first MaxSize characters of String, then StrnSizeS
  // returns (sizeof (CHAR16) * (MaxSize + 1)) to keep a consistent map with
  // the StrnLenS function.
  //
  return (StrnLenS (String, MaxSize) + 1) * sizeof (*String);
}

/**
  Copies the string pointed to by Source (including the terminating null char)
  to the array pointed to by Destination.

  This function is similar as strcpy_s defined in C11.

  If Destination is not aligned on a 16-bit boundary, then ASSERT().
  If Source is not aligned on a 16-bit boundary, then ASSERT().

  If an error is returned, then the Destination is unmodified.

  @param  Destination              A pointer to a Null-terminated Unicode string.
  @param  DestMax                  The maximum number of Destination Unicode
                                   char, including terminating null char.
  @param  Source                   A pointer to a Null-terminated Unicode string.

  @retval RETURN_SUCCESS           String is copied.
  @retval RETURN_BUFFER_TOO_SMALL  If DestMax is NOT greater than StrLen(Source).
  @retval RETURN_INVALID_PARAMETER If Destination is NULL.
                                   If Source is NULL.
                                   If PcdMaximumUnicodeStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumUnicodeStringLength.
                                   If DestMax is 0.
  @retval RETURN_ACCESS_DENIED     If Source and Destination overlap.
**/
RETURN_STATUS
EFIAPI
StrCpyS (
  OUT CHAR16        *Destination,
  IN  UINTN         DestMax,
  IN  CONST CHAR16  *Source
  )
{
  UINTN  SourceLen;

  ASSERT (((UINTN)Destination & BIT0) == 0);
  ASSERT (((UINTN)Source & BIT0) == 0);

  //
  // 1. Neither Destination nor Source shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. DestMax shall not be greater than RSIZE_MAX.
  //
  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. DestMax shall be greater than StrnLenS(Source, DestMax).
  //
  SourceLen = StrnLenS (Source, DestMax);
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax > SourceLen), RETURN_BUFFER_TOO_SMALL);

  //
  // 5. Copying shall not take place between objects that overlap.
  //
  SAFE_STRING_CONSTRAINT_CHECK (InternalSafeStringNoStrOverlap (Destination, DestMax, (CHAR16 *)Source, SourceLen + 1), RETURN_ACCESS_DENIED);

  //
  // The StrCpyS function copies the string pointed to by Source (including the terminating
  // null character) into the array pointed to by Destination.
  //
  while (*Source != 0) {
    *(Destination++) = *(Source++);
  }

  *Destination = 0;

  return RETURN_SUCCESS;
}

/**
  Copies not more than Length successive char from the string pointed to by
  Source to the array pointed to by Destination. If no null char is copied from
  Source, then Destination[Length] is always set to null.

  This function is similar as strncpy_s defined in C11.

  If Length > 0 and Destination is not aligned on a 16-bit boundary, then ASSERT().
  If Length > 0 and Source is not aligned on a 16-bit boundary, then ASSERT().

  If an error is returned, then the Destination is unmodified.

  @param  Destination              A pointer to a Null-terminated Unicode string.
  @param  DestMax                  The maximum number of Destination Unicode
                                   char, including terminating null char.
  @param  Source                   A pointer to a Null-terminated Unicode string.
  @param  Length                   The maximum number of Unicode characters to copy.

  @retval RETURN_SUCCESS           String is copied.
  @retval RETURN_BUFFER_TOO_SMALL  If DestMax is NOT greater than
                                   MIN(StrLen(Source), Length).
  @retval RETURN_INVALID_PARAMETER If Destination is NULL.
                                   If Source is NULL.
                                   If PcdMaximumUnicodeStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumUnicodeStringLength.
                                   If DestMax is 0.
  @retval RETURN_ACCESS_DENIED     If Source and Destination overlap.
**/
RETURN_STATUS
EFIAPI
StrnCpyS (
  OUT CHAR16        *Destination,
  IN  UINTN         DestMax,
  IN  CONST CHAR16  *Source,
  IN  UINTN         Length
  )
{
  UINTN  SourceLen;

  ASSERT (((UINTN)Destination & BIT0) == 0);
  ASSERT (((UINTN)Source & BIT0) == 0);

  //
  // 1. Neither Destination nor Source shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. Neither DestMax nor Length shall be greater than RSIZE_MAX
  //
  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Length <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. If Length is not less than DestMax, then DestMax shall be greater than StrnLenS(Source, DestMax).
  //
  SourceLen = StrnLenS (Source, MIN (DestMax, Length));
  if (Length >= DestMax) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax > SourceLen), RETURN_BUFFER_TOO_SMALL);
  }

  //
  // 5. Copying shall not take place between objects that overlap.
  //
  if (SourceLen > Length) {
    SourceLen = Length;
  }

  SAFE_STRING_CONSTRAINT_CHECK (InternalSafeStringNoStrOverlap (Destination, DestMax, (CHAR16 *)Source, SourceLen + 1), RETURN_ACCESS_DENIED);

  //
  // The StrnCpyS function copies not more than Length successive characters (characters that
  // follow a null character are not copied) from the array pointed to by Source to the array
  // pointed to by Destination. If no null character was copied from Source, then Destination[Length] is set to a null
  // character.
  //
  while ((SourceLen > 0) && (*Source != 0)) {
    *(Destination++) = *(Source++);
    SourceLen--;
  }

  *Destination = 0;

  return RETURN_SUCCESS;
}

/**
  Appends a copy of the string pointed to by Source (including the terminating
  null char) to the end of the string pointed to by Destination.

  This function is similar as strcat_s defined in C11.

  If Destination is not aligned on a 16-bit boundary, then ASSERT().
  If Source is not aligned on a 16-bit boundary, then ASSERT().

  If an error is returned, then the Destination is unmodified.

  @param  Destination              A pointer to a Null-terminated Unicode string.
  @param  DestMax                  The maximum number of Destination Unicode
                                   char, including terminating null char.
  @param  Source                   A pointer to a Null-terminated Unicode string.

  @retval RETURN_SUCCESS           String is appended.
  @retval RETURN_BAD_BUFFER_SIZE   If DestMax is NOT greater than
                                   StrLen(Destination).
  @retval RETURN_BUFFER_TOO_SMALL  If (DestMax - StrLen(Destination)) is NOT
                                   greater than StrLen(Source).
  @retval RETURN_INVALID_PARAMETER If Destination is NULL.
                                   If Source is NULL.
                                   If PcdMaximumUnicodeStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumUnicodeStringLength.
                                   If DestMax is 0.
  @retval RETURN_ACCESS_DENIED     If Source and Destination overlap.
**/
RETURN_STATUS
EFIAPI
StrCatS (
  IN OUT CHAR16        *Destination,
  IN     UINTN         DestMax,
  IN     CONST CHAR16  *Source
  )
{
  UINTN  DestLen;
  UINTN  CopyLen;
  UINTN  SourceLen;

  ASSERT (((UINTN)Destination & BIT0) == 0);
  ASSERT (((UINTN)Source & BIT0) == 0);

  //
  // Let CopyLen denote the value DestMax - StrnLenS(Destination, DestMax) upon entry to StrCatS.
  //
  DestLen = StrnLenS (Destination, DestMax);
  CopyLen = DestMax - DestLen;

  //
  // 1. Neither Destination nor Source shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. DestMax shall not be greater than RSIZE_MAX.
  //
  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. CopyLen shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((CopyLen != 0), RETURN_BAD_BUFFER_SIZE);

  //
  // 5. CopyLen shall be greater than StrnLenS(Source, CopyLen).
  //
  SourceLen = StrnLenS (Source, CopyLen);
  SAFE_STRING_CONSTRAINT_CHECK ((CopyLen > SourceLen), RETURN_BUFFER_TOO_SMALL);

  //
  // 6. Copying shall not take place between objects that overlap.
  //
  SAFE_STRING_CONSTRAINT_CHECK (InternalSafeStringNoStrOverlap (Destination, DestMax, (CHAR16 *)Source, SourceLen + 1), RETURN_ACCESS_DENIED);

  //
  // The StrCatS function appends a copy of the string pointed to by Source (including the
  // terminating null character) to the end of the string pointed to by Destination. The initial character
  // from Source overwrites the null character at the end of Destination.
  //
  Destination = Destination + DestLen;
  while (*Source != 0) {
    *(Destination++) = *(Source++);
  }

  *Destination = 0;

  return RETURN_SUCCESS;
}

/**
  Appends not more than Length successive char from the string pointed to by
  Source to the end of the string pointed to by Destination. If no null char is
  copied from Source, then Destination[StrLen(Destination) + Length] is always
  set to null.

  This function is similar as strncat_s defined in C11.

  If Destination is not aligned on a 16-bit boundary, then ASSERT().
  If Source is not aligned on a 16-bit boundary, then ASSERT().

  If an error is returned, then the Destination is unmodified.

  @param  Destination              A pointer to a Null-terminated Unicode string.
  @param  DestMax                  The maximum number of Destination Unicode
                                   char, including terminating null char.
  @param  Source                   A pointer to a Null-terminated Unicode string.
  @param  Length                   The maximum number of Unicode characters to copy.

  @retval RETURN_SUCCESS           String is appended.
  @retval RETURN_BAD_BUFFER_SIZE   If DestMax is NOT greater than
                                   StrLen(Destination).
  @retval RETURN_BUFFER_TOO_SMALL  If (DestMax - StrLen(Destination)) is NOT
                                   greater than MIN(StrLen(Source), Length).
  @retval RETURN_INVALID_PARAMETER If Destination is NULL.
                                   If Source is NULL.
                                   If PcdMaximumUnicodeStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumUnicodeStringLength.
                                   If DestMax is 0.
  @retval RETURN_ACCESS_DENIED     If Source and Destination overlap.
**/
RETURN_STATUS
EFIAPI
StrnCatS (
  IN OUT CHAR16        *Destination,
  IN     UINTN         DestMax,
  IN     CONST CHAR16  *Source,
  IN     UINTN         Length
  )
{
  UINTN  DestLen;
  UINTN  CopyLen;
  UINTN  SourceLen;

  ASSERT (((UINTN)Destination & BIT0) == 0);
  ASSERT (((UINTN)Source & BIT0) == 0);

  //
  // Let CopyLen denote the value DestMax - StrnLenS(Destination, DestMax) upon entry to StrnCatS.
  //
  DestLen = StrnLenS (Destination, DestMax);
  CopyLen = DestMax - DestLen;

  //
  // 1. Neither Destination nor Source shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. Neither DestMax nor Length shall be greater than RSIZE_MAX.
  //
  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Length <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. CopyLen shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((CopyLen != 0), RETURN_BAD_BUFFER_SIZE);

  //
  // 5. If Length is not less than CopyLen, then CopyLen shall be greater than StrnLenS(Source, CopyLen).
  //
  SourceLen = StrnLenS (Source, MIN (CopyLen, Length));
  if (Length >= CopyLen) {
    SAFE_STRING_CONSTRAINT_CHECK ((CopyLen > SourceLen), RETURN_BUFFER_TOO_SMALL);
  }

  //
  // 6. Copying shall not take place between objects that overlap.
  //
  if (SourceLen > Length) {
    SourceLen = Length;
  }

  SAFE_STRING_CONSTRAINT_CHECK (InternalSafeStringNoStrOverlap (Destination, DestMax, (CHAR16 *)Source, SourceLen + 1), RETURN_ACCESS_DENIED);

  //
  // The StrnCatS function appends not more than Length successive characters (characters
  // that follow a null character are not copied) from the array pointed to by Source to the end of
  // the string pointed to by Destination. The initial character from Source overwrites the null character at
  // the end of Destination. If no null character was copied from Source, then Destination[DestMax-CopyLen+Length] is set to
  // a null character.
  //
  Destination = Destination + DestLen;
  while ((SourceLen > 0) && (*Source != 0)) {
    *(Destination++) = *(Source++);
    SourceLen--;
  }

  *Destination = 0;

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Unicode decimal string to a value of type UINTN.

  This function outputs a value of type UINTN by interpreting the contents of
  the Unicode string specified by String as a decimal number. The format of the
  input Unicode string String is:

                  [spaces] [decimal digits].

  The valid decimal digit character is in the range [0-9]. The function will
  ignore the pad space, which includes spaces or tab characters, before
  [decimal digits]. The running zero in the beginning of [decimal digits] will
  be ignored. Then, the function stops at the first character that is a not a
  valid decimal character or a Null-terminator, whichever one comes first.

  If String is not aligned in a 16-bit boundary, then ASSERT().

  If String has no valid decimal digits in the above format, then 0 is stored
  at the location pointed to by Data.
  If the number represented by String exceeds the range defined by UINTN, then
  MAX_UINTN is stored at the location pointed to by Data.

  If EndPointer is not NULL, a pointer to the character that stopped the scan
  is stored at the location pointed to by EndPointer. If String has no valid
  decimal digits right after the optional pad spaces, the value of String is
  stored at the location pointed to by EndPointer.

  @param  String                   Pointer to a Null-terminated Unicode string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Data                     Pointer to the converted value.

  @retval RETURN_SUCCESS           Value is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
                                   If PcdMaximumUnicodeStringLength is not
                                   zero, and String contains more than
                                   PcdMaximumUnicodeStringLength Unicode
                                   characters, not including the
                                   Null-terminator.
  @retval RETURN_UNSUPPORTED       If the number represented by String exceeds
                                   the range defined by UINTN.

**/
RETURN_STATUS
EFIAPI
StrDecimalToUintnS (
  IN  CONST CHAR16  *String,
  OUT       CHAR16  **EndPointer   OPTIONAL,
  OUT       UINTN   *Data
  )
{
  ASSERT (((UINTN)String & BIT0) == 0);

  //
  // 1. Neither String nor Data shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Data != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. The length of String shall not be greater than RSIZE_MAX.
  //
  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((StrnLenS (String, RSIZE_MAX + 1) <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR16 *)String;
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*String == L' ') || (*String == L'\t')) {
    String++;
  }

  //
  // Ignore leading Zeros after the spaces
  //
  while (*String == L'0') {
    String++;
  }

  *Data = 0;

  while (InternalIsDecimalDigitCharacter (*String)) {
    //
    // If the number represented by String overflows according to the range
    // defined by UINTN, then MAX_UINTN is stored in *Data and
    // RETURN_UNSUPPORTED is returned.
    //
    if (*Data > ((MAX_UINTN - (*String - L'0')) / 10)) {
      *Data = MAX_UINTN;
      if (EndPointer != NULL) {
        *EndPointer = (CHAR16 *)String;
      }

      return RETURN_UNSUPPORTED;
    }

    *Data = *Data * 10 + (*String - L'0');
    String++;
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR16 *)String;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Unicode decimal string to a value of type UINT64.

  This function outputs a value of type UINT64 by interpreting the contents of
  the Unicode string specified by String as a decimal number. The format of the
  input Unicode string String is:

                  [spaces] [decimal digits].

  The valid decimal digit character is in the range [0-9]. The function will
  ignore the pad space, which includes spaces or tab characters, before
  [decimal digits]. The running zero in the beginning of [decimal digits] will
  be ignored. Then, the function stops at the first character that is a not a
  valid decimal character or a Null-terminator, whichever one comes first.

  If String is not aligned in a 16-bit boundary, then ASSERT().

  If String has no valid decimal digits in the above format, then 0 is stored
  at the location pointed to by Data.
  If the number represented by String exceeds the range defined by UINT64, then
  MAX_UINT64 is stored at the location pointed to by Data.

  If EndPointer is not NULL, a pointer to the character that stopped the scan
  is stored at the location pointed to by EndPointer. If String has no valid
  decimal digits right after the optional pad spaces, the value of String is
  stored at the location pointed to by EndPointer.

  @param  String                   Pointer to a Null-terminated Unicode string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Data                     Pointer to the converted value.

  @retval RETURN_SUCCESS           Value is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
                                   If PcdMaximumUnicodeStringLength is not
                                   zero, and String contains more than
                                   PcdMaximumUnicodeStringLength Unicode
                                   characters, not including the
                                   Null-terminator.
  @retval RETURN_UNSUPPORTED       If the number represented by String exceeds
                                   the range defined by UINT64.

**/
RETURN_STATUS
EFIAPI
StrDecimalToUint64S (
  IN  CONST CHAR16  *String,
  OUT       CHAR16  **EndPointer   OPTIONAL,
  OUT       UINT64  *Data
  )
{
  ASSERT (((UINTN)String & BIT0) == 0);

  //
  // 1. Neither String nor Data shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Data != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. The length of String shall not be greater than RSIZE_MAX.
  //
  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((StrnLenS (String, RSIZE_MAX + 1) <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR16 *)String;
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*String == L' ') || (*String == L'\t')) {
    String++;
  }

  //
  // Ignore leading Zeros after the spaces
  //
  while (*String == L'0') {
    String++;
  }

  *Data = 0;

  while (InternalIsDecimalDigitCharacter (*String)) {
    //
    // If the number represented by String overflows according to the range
    // defined by UINT64, then MAX_UINT64 is stored in *Data and
    // RETURN_UNSUPPORTED is returned.
    //
    if (*Data > DivU64x32 (MAX_UINT64 - (*String - L'0'), 10)) {
      *Data = MAX_UINT64;
      if (EndPointer != NULL) {
        *EndPointer = (CHAR16 *)String;
      }

      return RETURN_UNSUPPORTED;
    }

    *Data = MultU64x32 (*Data, 10) + (*String - L'0');
    String++;
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR16 *)String;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Unicode hexadecimal string to a value of type
  UINTN.

  This function outputs a value of type UINTN by interpreting the contents of
  the Unicode string specified by String as a hexadecimal number. The format of
  the input Unicode string String is:

                  [spaces][zeros][x][hexadecimal digits].

  The valid hexadecimal digit character is in the range [0-9], [a-f] and [A-F].
  The prefix "0x" is optional. Both "x" and "X" is allowed in "0x" prefix.
  If "x" appears in the input string, it must be prefixed with at least one 0.
  The function will ignore the pad space, which includes spaces or tab
  characters, before [zeros], [x] or [hexadecimal digit]. The running zero
  before [x] or [hexadecimal digit] will be ignored. Then, the decoding starts
  after [x] or the first valid hexadecimal digit. Then, the function stops at
  the first character that is a not a valid hexadecimal character or NULL,
  whichever one comes first.

  If String is not aligned in a 16-bit boundary, then ASSERT().

  If String has no valid hexadecimal digits in the above format, then 0 is
  stored at the location pointed to by Data.
  If the number represented by String exceeds the range defined by UINTN, then
  MAX_UINTN is stored at the location pointed to by Data.

  If EndPointer is not NULL, a pointer to the character that stopped the scan
  is stored at the location pointed to by EndPointer. If String has no valid
  hexadecimal digits right after the optional pad spaces, the value of String
  is stored at the location pointed to by EndPointer.

  @param  String                   Pointer to a Null-terminated Unicode string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Data                     Pointer to the converted value.

  @retval RETURN_SUCCESS           Value is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
                                   If PcdMaximumUnicodeStringLength is not
                                   zero, and String contains more than
                                   PcdMaximumUnicodeStringLength Unicode
                                   characters, not including the
                                   Null-terminator.
  @retval RETURN_UNSUPPORTED       If the number represented by String exceeds
                                   the range defined by UINTN.

**/
RETURN_STATUS
EFIAPI
StrHexToUintnS (
  IN  CONST CHAR16  *String,
  OUT       CHAR16  **EndPointer   OPTIONAL,
  OUT       UINTN   *Data
  )
{
  BOOLEAN  FoundLeadingZero;

  FoundLeadingZero = FALSE;
  ASSERT (((UINTN)String & BIT0) == 0);

  //
  // 1. Neither String nor Data shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Data != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. The length of String shall not be greater than RSIZE_MAX.
  //
  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((StrnLenS (String, RSIZE_MAX + 1) <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR16 *)String;
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*String == L' ') || (*String == L'\t')) {
    String++;
  }

  //
  // Ignore leading Zeros after the spaces
  //

  FoundLeadingZero = *String == L'0';
  while (*String == L'0') {
    String++;
  }

  if (CharToUpper (*String) == L'X') {
    if (!FoundLeadingZero) {
      *Data = 0;
      return RETURN_SUCCESS;
    }

    //
    // Skip the 'X'
    //
    String++;
  }

  *Data = 0;

  while (InternalIsHexaDecimalDigitCharacter (*String)) {
    //
    // If the number represented by String overflows according to the range
    // defined by UINTN, then MAX_UINTN is stored in *Data and
    // RETURN_UNSUPPORTED is returned.
    //
    if (*Data > ((MAX_UINTN - InternalHexCharToUintn (*String)) >> 4)) {
      *Data = MAX_UINTN;
      if (EndPointer != NULL) {
        *EndPointer = (CHAR16 *)String;
      }

      return RETURN_UNSUPPORTED;
    }

    *Data = (*Data << 4) + InternalHexCharToUintn (*String);
    String++;
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR16 *)String;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Unicode hexadecimal string to a value of type
  UINT64.

  This function outputs a value of type UINT64 by interpreting the contents of
  the Unicode string specified by String as a hexadecimal number. The format of
  the input Unicode string String is:

                  [spaces][zeros][x][hexadecimal digits].

  The valid hexadecimal digit character is in the range [0-9], [a-f] and [A-F].
  The prefix "0x" is optional. Both "x" and "X" is allowed in "0x" prefix.
  If "x" appears in the input string, it must be prefixed with at least one 0.
  The function will ignore the pad space, which includes spaces or tab
  characters, before [zeros], [x] or [hexadecimal digit]. The running zero
  before [x] or [hexadecimal digit] will be ignored. Then, the decoding starts
  after [x] or the first valid hexadecimal digit. Then, the function stops at
  the first character that is a not a valid hexadecimal character or NULL,
  whichever one comes first.

  If String is not aligned in a 16-bit boundary, then ASSERT().

  If String has no valid hexadecimal digits in the above format, then 0 is
  stored at the location pointed to by Data.
  If the number represented by String exceeds the range defined by UINT64, then
  MAX_UINT64 is stored at the location pointed to by Data.

  If EndPointer is not NULL, a pointer to the character that stopped the scan
  is stored at the location pointed to by EndPointer. If String has no valid
  hexadecimal digits right after the optional pad spaces, the value of String
  is stored at the location pointed to by EndPointer.

  @param  String                   Pointer to a Null-terminated Unicode string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Data                     Pointer to the converted value.

  @retval RETURN_SUCCESS           Value is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
                                   If PcdMaximumUnicodeStringLength is not
                                   zero, and String contains more than
                                   PcdMaximumUnicodeStringLength Unicode
                                   characters, not including the
                                   Null-terminator.
  @retval RETURN_UNSUPPORTED       If the number represented by String exceeds
                                   the range defined by UINT64.

**/
RETURN_STATUS
EFIAPI
StrHexToUint64S (
  IN  CONST CHAR16  *String,
  OUT       CHAR16  **EndPointer   OPTIONAL,
  OUT       UINT64  *Data
  )
{
  BOOLEAN  FoundLeadingZero;

  FoundLeadingZero = FALSE;
  ASSERT (((UINTN)String & BIT0) == 0);

  //
  // 1. Neither String nor Data shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Data != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. The length of String shall not be greater than RSIZE_MAX.
  //
  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((StrnLenS (String, RSIZE_MAX + 1) <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR16 *)String;
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*String == L' ') || (*String == L'\t')) {
    String++;
  }

  //
  // Ignore leading Zeros after the spaces
  //
  FoundLeadingZero = *String == L'0';
  while (*String == L'0') {
    String++;
  }

  if (CharToUpper (*String) == L'X') {
    if (!FoundLeadingZero) {
      *Data = 0;
      return RETURN_SUCCESS;
    }

    //
    // Skip the 'X'
    //
    String++;
  }

  *Data = 0;

  while (InternalIsHexaDecimalDigitCharacter (*String)) {
    //
    // If the number represented by String overflows according to the range
    // defined by UINT64, then MAX_UINT64 is stored in *Data and
    // RETURN_UNSUPPORTED is returned.
    //
    if (*Data > RShiftU64 (MAX_UINT64 - InternalHexCharToUintn (*String), 4)) {
      *Data = MAX_UINT64;
      if (EndPointer != NULL) {
        *EndPointer = (CHAR16 *)String;
      }

      return RETURN_UNSUPPORTED;
    }

    *Data = LShiftU64 (*Data, 4) + InternalHexCharToUintn (*String);
    String++;
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR16 *)String;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Unicode string to IPv6 address and prefix length.

  This function outputs a value of type IPv6_ADDRESS and may output a value
  of type UINT8 by interpreting the contents of the Unicode string specified
  by String. The format of the input Unicode string String is as follows:

                  X:X:X:X:X:X:X:X[/P]

  X contains one to four hexadecimal digit characters in the range [0-9], [a-f] and
  [A-F]. X is converted to a value of type UINT16, whose low byte is stored in low
  memory address and high byte is stored in high memory address. P contains decimal
  digit characters in the range [0-9]. The running zero in the beginning of P will
  be ignored. /P is optional.

  When /P is not in the String, the function stops at the first character that is
  not a valid hexadecimal digit character after eight X's are converted.

  When /P is in the String, the function stops at the first character that is not
  a valid decimal digit character after P is converted.

  "::" can be used to compress one or more groups of X when X contains only 0.
  The "::" can only appear once in the String.

  If String is not aligned in a 16-bit boundary, then ASSERT().

  If EndPointer is not NULL and Address is translated from String, a pointer
  to the character that stopped the scan is stored at the location pointed to
  by EndPointer.

  @param  String                   Pointer to a Null-terminated Unicode string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Address                  Pointer to the converted IPv6 address.
  @param  PrefixLength             Pointer to the converted IPv6 address prefix
                                   length. MAX_UINT8 is returned when /P is
                                   not in the String.

  @retval RETURN_SUCCESS           Address is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
  @retval RETURN_UNSUPPORTED       If X contains more than four hexadecimal
                                    digit characters.
                                   If String contains "::" and number of X
                                    is not less than 8.
                                   If P starts with character that is not a
                                    valid decimal digit character.
                                   If the decimal number converted from P
                                    exceeds 128.

**/
RETURN_STATUS
EFIAPI
StrToIpv6Address (
  IN  CONST CHAR16  *String,
  OUT CHAR16        **EndPointer  OPTIONAL,
  OUT IPv6_ADDRESS  *Address,
  OUT UINT8         *PrefixLength OPTIONAL
  )
{
  RETURN_STATUS  Status;
  UINTN          AddressIndex;
  UINTN          Uintn;
  IPv6_ADDRESS   LocalAddress;
  UINT8          LocalPrefixLength;
  CONST CHAR16   *Pointer;
  CHAR16         *End;
  UINTN          CompressStart;
  BOOLEAN        ExpectPrefix;

  LocalPrefixLength = MAX_UINT8;
  CompressStart     = ARRAY_SIZE (Address->Addr);
  ExpectPrefix      = FALSE;

  ASSERT (((UINTN)String & BIT0) == 0);

  //
  // 1. None of String or Guid shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Address != NULL), RETURN_INVALID_PARAMETER);

  for (Pointer = String, AddressIndex = 0; AddressIndex < ARRAY_SIZE (Address->Addr) + 1;) {
    if (!InternalIsHexaDecimalDigitCharacter (*Pointer)) {
      if (*Pointer != L':') {
        //
        // ":" or "/" should be followed by digit characters.
        //
        return RETURN_UNSUPPORTED;
      }

      //
      // Meet second ":" after previous ":" or "/"
      // or meet first ":" in the beginning of String.
      //
      if (ExpectPrefix) {
        //
        // ":" shall not be after "/"
        //
        return RETURN_UNSUPPORTED;
      }

      if ((CompressStart != ARRAY_SIZE (Address->Addr)) || (AddressIndex == ARRAY_SIZE (Address->Addr))) {
        //
        // "::" can only appear once.
        // "::" can only appear when address is not full length.
        //
        return RETURN_UNSUPPORTED;
      } else {
        //
        // Remember the start of zero compressing.
        //
        CompressStart = AddressIndex;
        Pointer++;

        if (CompressStart == 0) {
          if (*Pointer != L':') {
            //
            // Single ":" shall not be in the beginning of String.
            //
            return RETURN_UNSUPPORTED;
          }

          Pointer++;
        }
      }
    }

    if (!InternalIsHexaDecimalDigitCharacter (*Pointer)) {
      if (*Pointer == L'/') {
        //
        // Might be optional "/P" after "::".
        //
        if (CompressStart != AddressIndex) {
          return RETURN_UNSUPPORTED;
        }
      } else {
        break;
      }
    } else {
      if (!ExpectPrefix) {
        //
        // Get X.
        //
        Status = StrHexToUintnS (Pointer, &End, &Uintn);
        if (RETURN_ERROR (Status) || (End - Pointer > 4)) {
          //
          // Number of hexadecimal digit characters is no more than 4.
          //
          return RETURN_UNSUPPORTED;
        }

        Pointer = End;
        //
        // Uintn won't exceed MAX_UINT16 if number of hexadecimal digit characters is no more than 4.
        //
        ASSERT (AddressIndex + 1 < ARRAY_SIZE (Address->Addr));
        LocalAddress.Addr[AddressIndex]     = (UINT8)((UINT16)Uintn >> 8);
        LocalAddress.Addr[AddressIndex + 1] = (UINT8)Uintn;
        AddressIndex                       += 2;
      } else {
        //
        // Get P, then exit the loop.
        //
        Status = StrDecimalToUintnS (Pointer, &End, &Uintn);
        if (RETURN_ERROR (Status) || (End == Pointer) || (Uintn > 128)) {
          //
          // Prefix length should not exceed 128.
          //
          return RETURN_UNSUPPORTED;
        }

        LocalPrefixLength = (UINT8)Uintn;
        Pointer           = End;
        break;
      }
    }

    //
    // Skip ':' or "/"
    //
    if (*Pointer == L'/') {
      ExpectPrefix = TRUE;
    } else if (*Pointer == L':') {
      if (AddressIndex == ARRAY_SIZE (Address->Addr)) {
        //
        // Meet additional ":" after all 8 16-bit address
        //
        break;
      }
    } else {
      //
      // Meet other character that is not "/" or ":" after all 8 16-bit address
      //
      break;
    }

    Pointer++;
  }

  if (((AddressIndex == ARRAY_SIZE (Address->Addr)) && (CompressStart != ARRAY_SIZE (Address->Addr))) ||
      ((AddressIndex != ARRAY_SIZE (Address->Addr)) && (CompressStart == ARRAY_SIZE (Address->Addr)))
      )
  {
    //
    // Full length of address shall not have compressing zeros.
    // Non-full length of address shall have compressing zeros.
    //
    return RETURN_UNSUPPORTED;
  }

  CopyMem (&Address->Addr[0], &LocalAddress.Addr[0], CompressStart);
  ZeroMem (&Address->Addr[CompressStart], ARRAY_SIZE (Address->Addr) - AddressIndex);
  if (AddressIndex > CompressStart) {
    CopyMem (
      &Address->Addr[CompressStart + ARRAY_SIZE (Address->Addr) - AddressIndex],
      &LocalAddress.Addr[CompressStart],
      AddressIndex - CompressStart
      );
  }

  if (PrefixLength != NULL) {
    *PrefixLength = LocalPrefixLength;
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR16 *)Pointer;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Unicode string to IPv4 address and prefix length.

  This function outputs a value of type IPv4_ADDRESS and may output a value
  of type UINT8 by interpreting the contents of the Unicode string specified
  by String. The format of the input Unicode string String is as follows:

                  D.D.D.D[/P]

  D and P are decimal digit characters in the range [0-9]. The running zero in
  the beginning of D and P will be ignored. /P is optional.

  When /P is not in the String, the function stops at the first character that is
  not a valid decimal digit character after four D's are converted.

  When /P is in the String, the function stops at the first character that is not
  a valid decimal digit character after P is converted.

  If String is not aligned in a 16-bit boundary, then ASSERT().

  If EndPointer is not NULL and Address is translated from String, a pointer
  to the character that stopped the scan is stored at the location pointed to
  by EndPointer.

  @param  String                   Pointer to a Null-terminated Unicode string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Address                  Pointer to the converted IPv4 address.
  @param  PrefixLength             Pointer to the converted IPv4 address prefix
                                   length. MAX_UINT8 is returned when /P is
                                   not in the String.

  @retval RETURN_SUCCESS           Address is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
  @retval RETURN_UNSUPPORTED       If String is not in the correct format.
                                   If any decimal number converted from D
                                    exceeds 255.
                                   If the decimal number converted from P
                                    exceeds 32.

**/
RETURN_STATUS
EFIAPI
StrToIpv4Address (
  IN  CONST CHAR16  *String,
  OUT CHAR16        **EndPointer  OPTIONAL,
  OUT IPv4_ADDRESS  *Address,
  OUT UINT8         *PrefixLength OPTIONAL
  )
{
  RETURN_STATUS  Status;
  UINTN          AddressIndex;
  UINTN          Uintn;
  IPv4_ADDRESS   LocalAddress;
  UINT8          LocalPrefixLength;
  CHAR16         *Pointer;

  LocalPrefixLength = MAX_UINT8;

  ASSERT (((UINTN)String & BIT0) == 0);

  //
  // 1. None of String or Guid shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Address != NULL), RETURN_INVALID_PARAMETER);

  for (Pointer = (CHAR16 *)String, AddressIndex = 0; AddressIndex < ARRAY_SIZE (Address->Addr) + 1;) {
    if (!InternalIsDecimalDigitCharacter (*Pointer)) {
      //
      // D or P contains invalid characters.
      //
      break;
    }

    //
    // Get D or P.
    //
    Status = StrDecimalToUintnS ((CONST CHAR16 *)Pointer, &Pointer, &Uintn);
    if (RETURN_ERROR (Status)) {
      return RETURN_UNSUPPORTED;
    }

    if (AddressIndex == ARRAY_SIZE (Address->Addr)) {
      //
      // It's P.
      //
      if (Uintn > 32) {
        return RETURN_UNSUPPORTED;
      }

      LocalPrefixLength = (UINT8)Uintn;
    } else {
      //
      // It's D.
      //
      if (Uintn > MAX_UINT8) {
        return RETURN_UNSUPPORTED;
      }

      LocalAddress.Addr[AddressIndex] = (UINT8)Uintn;
      AddressIndex++;
    }

    //
    // Check the '.' or '/', depending on the AddressIndex.
    //
    if (AddressIndex == ARRAY_SIZE (Address->Addr)) {
      if (*Pointer == L'/') {
        //
        // '/P' is in the String.
        // Skip "/" and get P in next loop.
        //
        Pointer++;
      } else {
        //
        // '/P' is not in the String.
        //
        break;
      }
    } else if (AddressIndex < ARRAY_SIZE (Address->Addr)) {
      if (*Pointer == L'.') {
        //
        // D should be followed by '.'
        //
        Pointer++;
      } else {
        return RETURN_UNSUPPORTED;
      }
    }
  }

  if (AddressIndex < ARRAY_SIZE (Address->Addr)) {
    return RETURN_UNSUPPORTED;
  }

  CopyMem (Address, &LocalAddress, sizeof (*Address));
  if (PrefixLength != NULL) {
    *PrefixLength = LocalPrefixLength;
  }

  if (EndPointer != NULL) {
    *EndPointer = Pointer;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Unicode GUID string to a value of type
  EFI_GUID.

  This function outputs a GUID value by interpreting the contents of
  the Unicode string specified by String. The format of the input
  Unicode string String consists of 36 characters, as follows:

                  aabbccdd-eeff-gghh-iijj-kkllmmnnoopp

  The pairs aa - pp are two characters in the range [0-9], [a-f] and
  [A-F], with each pair representing a single byte hexadecimal value.

  The mapping between String and the EFI_GUID structure is as follows:
                  aa          Data1[24:31]
                  bb          Data1[16:23]
                  cc          Data1[8:15]
                  dd          Data1[0:7]
                  ee          Data2[8:15]
                  ff          Data2[0:7]
                  gg          Data3[8:15]
                  hh          Data3[0:7]
                  ii          Data4[0:7]
                  jj          Data4[8:15]
                  kk          Data4[16:23]
                  ll          Data4[24:31]
                  mm          Data4[32:39]
                  nn          Data4[40:47]
                  oo          Data4[48:55]
                  pp          Data4[56:63]

  If String is not aligned in a 16-bit boundary, then ASSERT().

  @param  String                   Pointer to a Null-terminated Unicode string.
  @param  Guid                     Pointer to the converted GUID.

  @retval RETURN_SUCCESS           Guid is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
  @retval RETURN_UNSUPPORTED       If String is not as the above format.

**/
RETURN_STATUS
EFIAPI
StrToGuid (
  IN  CONST CHAR16  *String,
  OUT GUID          *Guid
  )
{
  RETURN_STATUS  Status;
  GUID           LocalGuid;

  ASSERT (((UINTN)String & BIT0) == 0);

  //
  // 1. None of String or Guid shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Guid != NULL), RETURN_INVALID_PARAMETER);

  //
  // Get aabbccdd in big-endian.
  //
  Status = StrHexToBytes (String, 2 * sizeof (LocalGuid.Data1), (UINT8 *)&LocalGuid.Data1, sizeof (LocalGuid.Data1));
  if (RETURN_ERROR (Status) || (String[2 * sizeof (LocalGuid.Data1)] != L'-')) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Convert big-endian to little-endian.
  //
  LocalGuid.Data1 = SwapBytes32 (LocalGuid.Data1);
  String         += 2 * sizeof (LocalGuid.Data1) + 1;

  //
  // Get eeff in big-endian.
  //
  Status = StrHexToBytes (String, 2 * sizeof (LocalGuid.Data2), (UINT8 *)&LocalGuid.Data2, sizeof (LocalGuid.Data2));
  if (RETURN_ERROR (Status) || (String[2 * sizeof (LocalGuid.Data2)] != L'-')) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Convert big-endian to little-endian.
  //
  LocalGuid.Data2 = SwapBytes16 (LocalGuid.Data2);
  String         += 2 * sizeof (LocalGuid.Data2) + 1;

  //
  // Get gghh in big-endian.
  //
  Status = StrHexToBytes (String, 2 * sizeof (LocalGuid.Data3), (UINT8 *)&LocalGuid.Data3, sizeof (LocalGuid.Data3));
  if (RETURN_ERROR (Status) || (String[2 * sizeof (LocalGuid.Data3)] != L'-')) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Convert big-endian to little-endian.
  //
  LocalGuid.Data3 = SwapBytes16 (LocalGuid.Data3);
  String         += 2 * sizeof (LocalGuid.Data3) + 1;

  //
  // Get iijj.
  //
  Status = StrHexToBytes (String, 2 * 2, &LocalGuid.Data4[0], 2);
  if (RETURN_ERROR (Status) || (String[2 * 2] != L'-')) {
    return RETURN_UNSUPPORTED;
  }

  String += 2 * 2 + 1;

  //
  // Get kkllmmnnoopp.
  //
  Status = StrHexToBytes (String, 2 * 6, &LocalGuid.Data4[2], 6);
  if (RETURN_ERROR (Status)) {
    return RETURN_UNSUPPORTED;
  }

  CopyGuid (Guid, &LocalGuid);
  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Unicode hexadecimal string to a byte array.

  This function outputs a byte array by interpreting the contents of
  the Unicode string specified by String in hexadecimal format. The format of
  the input Unicode string String is:

                  [XX]*

  X is a hexadecimal digit character in the range [0-9], [a-f] and [A-F].
  The function decodes every two hexadecimal digit characters as one byte. The
  decoding stops after Length of characters and outputs Buffer containing
  (Length / 2) bytes.

  If String is not aligned in a 16-bit boundary, then ASSERT().

  @param  String                   Pointer to a Null-terminated Unicode string.
  @param  Length                   The number of Unicode characters to decode.
  @param  Buffer                   Pointer to the converted bytes array.
  @param  MaxBufferSize            The maximum size of Buffer.

  @retval RETURN_SUCCESS           Buffer is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
                                   If Length is not multiple of 2.
                                   If PcdMaximumUnicodeStringLength is not zero,
                                    and Length is greater than
                                    PcdMaximumUnicodeStringLength.
  @retval RETURN_UNSUPPORTED       If Length of characters from String contain
                                    a character that is not valid hexadecimal
                                    digit characters, or a Null-terminator.
  @retval RETURN_BUFFER_TOO_SMALL  If MaxBufferSize is less than (Length / 2).
**/
RETURN_STATUS
EFIAPI
StrHexToBytes (
  IN  CONST CHAR16  *String,
  IN  UINTN         Length,
  OUT UINT8         *Buffer,
  IN  UINTN         MaxBufferSize
  )
{
  UINTN  Index;

  ASSERT (((UINTN)String & BIT0) == 0);

  //
  // 1. None of String or Buffer shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Buffer != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. Length shall not be greater than RSIZE_MAX.
  //
  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((Length <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. Length shall not be odd.
  //
  SAFE_STRING_CONSTRAINT_CHECK (((Length & BIT0) == 0), RETURN_INVALID_PARAMETER);

  //
  // 4. MaxBufferSize shall equal to or greater than Length / 2.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((MaxBufferSize >= Length / 2), RETURN_BUFFER_TOO_SMALL);

  //
  // 5. String shall not contains invalid hexadecimal digits.
  //
  for (Index = 0; Index < Length; Index++) {
    if (!InternalIsHexaDecimalDigitCharacter (String[Index])) {
      break;
    }
  }

  if (Index != Length) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Convert the hex string to bytes.
  //
  for (Index = 0; Index < Length; Index++) {
    //
    // For even characters, write the upper nibble for each buffer byte,
    // and for even characters, the lower nibble.
    //
    if ((Index & BIT0) == 0) {
      Buffer[Index / 2] = (UINT8)InternalHexCharToUintn (String[Index]) << 4;
    } else {
      Buffer[Index / 2] |= (UINT8)InternalHexCharToUintn (String[Index]);
    }
  }

  return RETURN_SUCCESS;
}

/**
  Returns the length of a Null-terminated Ascii string.

  This function is similar as strlen_s defined in C11.

  @param  String   A pointer to a Null-terminated Ascii string.
  @param  MaxSize  The maximum number of Destination Ascii
                   char, including terminating null char.

  @retval 0        If String is NULL.
  @retval MaxSize  If there is no null character in the first MaxSize characters of String.
  @return The number of characters that percede the terminating null character.

**/
UINTN
EFIAPI
AsciiStrnLenS (
  IN CONST CHAR8  *String,
  IN UINTN        MaxSize
  )
{
  UINTN  Length;

  //
  // If String is a null pointer or MaxSize is 0, then the AsciiStrnLenS function returns zero.
  //
  if ((String == NULL) || (MaxSize == 0)) {
    return 0;
  }

  //
  // Otherwise, the AsciiStrnLenS function returns the number of characters that precede the
  // terminating null character. If there is no null character in the first MaxSize characters of
  // String then AsciiStrnLenS returns MaxSize. At most the first MaxSize characters of String shall
  // be accessed by AsciiStrnLenS.
  //
  Length = 0;
  while (String[Length] != 0) {
    if (Length >= MaxSize - 1) {
      return MaxSize;
    }

    Length++;
  }

  return Length;
}

/**
  Returns the size of a Null-terminated Ascii string in bytes, including the
  Null terminator.

  This function returns the size of the Null-terminated Ascii string specified
  by String in bytes, including the Null terminator.

  @param  String   A pointer to a Null-terminated Ascii string.
  @param  MaxSize  The maximum number of Destination Ascii
                   char, including the Null terminator.

  @retval 0  If String is NULL.
  @retval (sizeof (CHAR8) * (MaxSize + 1))
             If there is no Null terminator in the first MaxSize characters of
             String.
  @return The size of the Null-terminated Ascii string in bytes, including the
          Null terminator.

**/
UINTN
EFIAPI
AsciiStrnSizeS (
  IN CONST CHAR8  *String,
  IN UINTN        MaxSize
  )
{
  //
  // If String is a null pointer, then the AsciiStrnSizeS function returns
  // zero.
  //
  if (String == NULL) {
    return 0;
  }

  //
  // Otherwise, the AsciiStrnSizeS function returns the size of the
  // Null-terminated Ascii string in bytes, including the Null terminator. If
  // there is no Null terminator in the first MaxSize characters of String,
  // then AsciiStrnSizeS returns (sizeof (CHAR8) * (MaxSize + 1)) to keep a
  // consistent map with the AsciiStrnLenS function.
  //
  return (AsciiStrnLenS (String, MaxSize) + 1) * sizeof (*String);
}

/**
  Copies the string pointed to by Source (including the terminating null char)
  to the array pointed to by Destination.

  This function is similar as strcpy_s defined in C11.

  If an error is returned, then the Destination is unmodified.

  @param  Destination              A pointer to a Null-terminated Ascii string.
  @param  DestMax                  The maximum number of Destination Ascii
                                   char, including terminating null char.
  @param  Source                   A pointer to a Null-terminated Ascii string.

  @retval RETURN_SUCCESS           String is copied.
  @retval RETURN_BUFFER_TOO_SMALL  If DestMax is NOT greater than StrLen(Source).
  @retval RETURN_INVALID_PARAMETER If Destination is NULL.
                                   If Source is NULL.
                                   If PcdMaximumAsciiStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumAsciiStringLength.
                                   If DestMax is 0.
  @retval RETURN_ACCESS_DENIED     If Source and Destination overlap.
**/
RETURN_STATUS
EFIAPI
AsciiStrCpyS (
  OUT CHAR8        *Destination,
  IN  UINTN        DestMax,
  IN  CONST CHAR8  *Source
  )
{
  UINTN  SourceLen;

  //
  // 1. Neither Destination nor Source shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. DestMax shall not be greater than ASCII_RSIZE_MAX.
  //
  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. DestMax shall be greater than AsciiStrnLenS(Source, DestMax).
  //
  SourceLen = AsciiStrnLenS (Source, DestMax);
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax > SourceLen), RETURN_BUFFER_TOO_SMALL);

  //
  // 5. Copying shall not take place between objects that overlap.
  //
  SAFE_STRING_CONSTRAINT_CHECK (InternalSafeStringNoAsciiStrOverlap (Destination, DestMax, (CHAR8 *)Source, SourceLen + 1), RETURN_ACCESS_DENIED);

  //
  // The AsciiStrCpyS function copies the string pointed to by Source (including the terminating
  // null character) into the array pointed to by Destination.
  //
  while (*Source != 0) {
    *(Destination++) = *(Source++);
  }

  *Destination = 0;

  return RETURN_SUCCESS;
}

/**
  Copies not more than Length successive char from the string pointed to by
  Source to the array pointed to by Destination. If no null char is copied from
  Source, then Destination[Length] is always set to null.

  This function is similar as strncpy_s defined in C11.

  If an error is returned, then the Destination is unmodified.

  @param  Destination              A pointer to a Null-terminated Ascii string.
  @param  DestMax                  The maximum number of Destination Ascii
                                   char, including terminating null char.
  @param  Source                   A pointer to a Null-terminated Ascii string.
  @param  Length                   The maximum number of Ascii characters to copy.

  @retval RETURN_SUCCESS           String is copied.
  @retval RETURN_BUFFER_TOO_SMALL  If DestMax is NOT greater than
                                   MIN(StrLen(Source), Length).
  @retval RETURN_INVALID_PARAMETER If Destination is NULL.
                                   If Source is NULL.
                                   If PcdMaximumAsciiStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumAsciiStringLength.
                                   If DestMax is 0.
  @retval RETURN_ACCESS_DENIED     If Source and Destination overlap.
**/
RETURN_STATUS
EFIAPI
AsciiStrnCpyS (
  OUT CHAR8        *Destination,
  IN  UINTN        DestMax,
  IN  CONST CHAR8  *Source,
  IN  UINTN        Length
  )
{
  UINTN  SourceLen;

  //
  // 1. Neither Destination nor Source shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. Neither DestMax nor Length shall be greater than ASCII_RSIZE_MAX
  //
  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Length <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. If Length is not less than DestMax, then DestMax shall be greater than AsciiStrnLenS(Source, DestMax).
  //
  SourceLen = AsciiStrnLenS (Source, MIN (DestMax, Length));
  if (Length >= DestMax) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax > SourceLen), RETURN_BUFFER_TOO_SMALL);
  }

  //
  // 5. Copying shall not take place between objects that overlap.
  //
  if (SourceLen > Length) {
    SourceLen = Length;
  }

  SAFE_STRING_CONSTRAINT_CHECK (InternalSafeStringNoAsciiStrOverlap (Destination, DestMax, (CHAR8 *)Source, SourceLen + 1), RETURN_ACCESS_DENIED);

  //
  // The AsciiStrnCpyS function copies not more than Length successive characters (characters that
  // follow a null character are not copied) from the array pointed to by Source to the array
  // pointed to by Destination. If no null character was copied from Source, then Destination[Length] is set to a null
  // character.
  //
  while ((SourceLen > 0) && (*Source != 0)) {
    *(Destination++) = *(Source++);
    SourceLen--;
  }

  *Destination = 0;

  return RETURN_SUCCESS;
}

/**
  Appends a copy of the string pointed to by Source (including the terminating
  null char) to the end of the string pointed to by Destination.

  This function is similar as strcat_s defined in C11.

  If an error is returned, then the Destination is unmodified.

  @param  Destination              A pointer to a Null-terminated Ascii string.
  @param  DestMax                  The maximum number of Destination Ascii
                                   char, including terminating null char.
  @param  Source                   A pointer to a Null-terminated Ascii string.

  @retval RETURN_SUCCESS           String is appended.
  @retval RETURN_BAD_BUFFER_SIZE   If DestMax is NOT greater than
                                   StrLen(Destination).
  @retval RETURN_BUFFER_TOO_SMALL  If (DestMax - StrLen(Destination)) is NOT
                                   greater than StrLen(Source).
  @retval RETURN_INVALID_PARAMETER If Destination is NULL.
                                   If Source is NULL.
                                   If PcdMaximumAsciiStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumAsciiStringLength.
                                   If DestMax is 0.
  @retval RETURN_ACCESS_DENIED     If Source and Destination overlap.
**/
RETURN_STATUS
EFIAPI
AsciiStrCatS (
  IN OUT CHAR8        *Destination,
  IN     UINTN        DestMax,
  IN     CONST CHAR8  *Source
  )
{
  UINTN  DestLen;
  UINTN  CopyLen;
  UINTN  SourceLen;

  //
  // Let CopyLen denote the value DestMax - AsciiStrnLenS(Destination, DestMax) upon entry to AsciiStrCatS.
  //
  DestLen = AsciiStrnLenS (Destination, DestMax);
  CopyLen = DestMax - DestLen;

  //
  // 1. Neither Destination nor Source shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. DestMax shall not be greater than ASCII_RSIZE_MAX.
  //
  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. CopyLen shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((CopyLen != 0), RETURN_BAD_BUFFER_SIZE);

  //
  // 5. CopyLen shall be greater than AsciiStrnLenS(Source, CopyLen).
  //
  SourceLen = AsciiStrnLenS (Source, CopyLen);
  SAFE_STRING_CONSTRAINT_CHECK ((CopyLen > SourceLen), RETURN_BUFFER_TOO_SMALL);

  //
  // 6. Copying shall not take place between objects that overlap.
  //
  SAFE_STRING_CONSTRAINT_CHECK (InternalSafeStringNoAsciiStrOverlap (Destination, DestMax, (CHAR8 *)Source, SourceLen + 1), RETURN_ACCESS_DENIED);

  //
  // The AsciiStrCatS function appends a copy of the string pointed to by Source (including the
  // terminating null character) to the end of the string pointed to by Destination. The initial character
  // from Source overwrites the null character at the end of Destination.
  //
  Destination = Destination + DestLen;
  while (*Source != 0) {
    *(Destination++) = *(Source++);
  }

  *Destination = 0;

  return RETURN_SUCCESS;
}

/**
  Appends not more than Length successive char from the string pointed to by
  Source to the end of the string pointed to by Destination. If no null char is
  copied from Source, then Destination[StrLen(Destination) + Length] is always
  set to null.

  This function is similar as strncat_s defined in C11.

  If an error is returned, then the Destination is unmodified.

  @param  Destination              A pointer to a Null-terminated Ascii string.
  @param  DestMax                  The maximum number of Destination Ascii
                                   char, including terminating null char.
  @param  Source                   A pointer to a Null-terminated Ascii string.
  @param  Length                   The maximum number of Ascii characters to copy.

  @retval RETURN_SUCCESS           String is appended.
  @retval RETURN_BAD_BUFFER_SIZE   If DestMax is NOT greater than
                                   StrLen(Destination).
  @retval RETURN_BUFFER_TOO_SMALL  If (DestMax - StrLen(Destination)) is NOT
                                   greater than MIN(StrLen(Source), Length).
  @retval RETURN_INVALID_PARAMETER If Destination is NULL.
                                   If Source is NULL.
                                   If PcdMaximumAsciiStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumAsciiStringLength.
                                   If DestMax is 0.
  @retval RETURN_ACCESS_DENIED     If Source and Destination overlap.
**/
RETURN_STATUS
EFIAPI
AsciiStrnCatS (
  IN OUT CHAR8        *Destination,
  IN     UINTN        DestMax,
  IN     CONST CHAR8  *Source,
  IN     UINTN        Length
  )
{
  UINTN  DestLen;
  UINTN  CopyLen;
  UINTN  SourceLen;

  //
  // Let CopyLen denote the value DestMax - AsciiStrnLenS(Destination, DestMax) upon entry to AsciiStrnCatS.
  //
  DestLen = AsciiStrnLenS (Destination, DestMax);
  CopyLen = DestMax - DestLen;

  //
  // 1. Neither Destination nor Source shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. Neither DestMax nor Length shall be greater than ASCII_RSIZE_MAX.
  //
  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((Length <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. CopyLen shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((CopyLen != 0), RETURN_BAD_BUFFER_SIZE);

  //
  // 5. If Length is not less than CopyLen, then CopyLen shall be greater than AsciiStrnLenS(Source, CopyLen).
  //
  SourceLen = AsciiStrnLenS (Source, MIN (CopyLen, Length));
  if (Length >= CopyLen) {
    SAFE_STRING_CONSTRAINT_CHECK ((CopyLen > SourceLen), RETURN_BUFFER_TOO_SMALL);
  }

  //
  // 6. Copying shall not take place between objects that overlap.
  //
  if (SourceLen > Length) {
    SourceLen = Length;
  }

  SAFE_STRING_CONSTRAINT_CHECK (InternalSafeStringNoAsciiStrOverlap (Destination, DestMax, (CHAR8 *)Source, SourceLen + 1), RETURN_ACCESS_DENIED);

  //
  // The AsciiStrnCatS function appends not more than Length successive characters (characters
  // that follow a null character are not copied) from the array pointed to by Source to the end of
  // the string pointed to by Destination. The initial character from Source overwrites the null character at
  // the end of Destination. If no null character was copied from Source, then Destination[DestMax-CopyLen+Length] is set to
  // a null character.
  //
  Destination = Destination + DestLen;
  while ((SourceLen > 0) && (*Source != 0)) {
    *(Destination++) = *(Source++);
    SourceLen--;
  }

  *Destination = 0;

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Ascii decimal string to a value of type UINTN.

  This function outputs a value of type UINTN by interpreting the contents of
  the Ascii string specified by String as a decimal number. The format of the
  input Ascii string String is:

                  [spaces] [decimal digits].

  The valid decimal digit character is in the range [0-9]. The function will
  ignore the pad space, which includes spaces or tab characters, before
  [decimal digits]. The running zero in the beginning of [decimal digits] will
  be ignored. Then, the function stops at the first character that is a not a
  valid decimal character or a Null-terminator, whichever one comes first.

  If String has no valid decimal digits in the above format, then 0 is stored
  at the location pointed to by Data.
  If the number represented by String exceeds the range defined by UINTN, then
  MAX_UINTN is stored at the location pointed to by Data.

  If EndPointer is not NULL, a pointer to the character that stopped the scan
  is stored at the location pointed to by EndPointer. If String has no valid
  decimal digits right after the optional pad spaces, the value of String is
  stored at the location pointed to by EndPointer.

  @param  String                   Pointer to a Null-terminated Ascii string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Data                     Pointer to the converted value.

  @retval RETURN_SUCCESS           Value is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
                                   If PcdMaximumAsciiStringLength is not zero,
                                   and String contains more than
                                   PcdMaximumAsciiStringLength Ascii
                                   characters, not including the
                                   Null-terminator.
  @retval RETURN_UNSUPPORTED       If the number represented by String exceeds
                                   the range defined by UINTN.

**/
RETURN_STATUS
EFIAPI
AsciiStrDecimalToUintnS (
  IN  CONST CHAR8  *String,
  OUT       CHAR8  **EndPointer   OPTIONAL,
  OUT       UINTN  *Data
  )
{
  //
  // 1. Neither String nor Data shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Data != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. The length of String shall not be greater than ASCII_RSIZE_MAX.
  //
  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((AsciiStrnLenS (String, ASCII_RSIZE_MAX + 1) <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR8 *)String;
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*String == ' ') || (*String == '\t')) {
    String++;
  }

  //
  // Ignore leading Zeros after the spaces
  //
  while (*String == '0') {
    String++;
  }

  *Data = 0;

  while (InternalAsciiIsDecimalDigitCharacter (*String)) {
    //
    // If the number represented by String overflows according to the range
    // defined by UINTN, then MAX_UINTN is stored in *Data and
    // RETURN_UNSUPPORTED is returned.
    //
    if (*Data > ((MAX_UINTN - (*String - '0')) / 10)) {
      *Data = MAX_UINTN;
      if (EndPointer != NULL) {
        *EndPointer = (CHAR8 *)String;
      }

      return RETURN_UNSUPPORTED;
    }

    *Data = *Data * 10 + (*String - '0');
    String++;
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR8 *)String;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Ascii decimal string to a value of type UINT64.

  This function outputs a value of type UINT64 by interpreting the contents of
  the Ascii string specified by String as a decimal number. The format of the
  input Ascii string String is:

                  [spaces] [decimal digits].

  The valid decimal digit character is in the range [0-9]. The function will
  ignore the pad space, which includes spaces or tab characters, before
  [decimal digits]. The running zero in the beginning of [decimal digits] will
  be ignored. Then, the function stops at the first character that is a not a
  valid decimal character or a Null-terminator, whichever one comes first.

  If String has no valid decimal digits in the above format, then 0 is stored
  at the location pointed to by Data.
  If the number represented by String exceeds the range defined by UINT64, then
  MAX_UINT64 is stored at the location pointed to by Data.

  If EndPointer is not NULL, a pointer to the character that stopped the scan
  is stored at the location pointed to by EndPointer. If String has no valid
  decimal digits right after the optional pad spaces, the value of String is
  stored at the location pointed to by EndPointer.

  @param  String                   Pointer to a Null-terminated Ascii string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Data                     Pointer to the converted value.

  @retval RETURN_SUCCESS           Value is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
                                   If PcdMaximumAsciiStringLength is not zero,
                                   and String contains more than
                                   PcdMaximumAsciiStringLength Ascii
                                   characters, not including the
                                   Null-terminator.
  @retval RETURN_UNSUPPORTED       If the number represented by String exceeds
                                   the range defined by UINT64.

**/
RETURN_STATUS
EFIAPI
AsciiStrDecimalToUint64S (
  IN  CONST CHAR8   *String,
  OUT       CHAR8   **EndPointer   OPTIONAL,
  OUT       UINT64  *Data
  )
{
  //
  // 1. Neither String nor Data shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Data != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. The length of String shall not be greater than ASCII_RSIZE_MAX.
  //
  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((AsciiStrnLenS (String, ASCII_RSIZE_MAX + 1) <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR8 *)String;
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*String == ' ') || (*String == '\t')) {
    String++;
  }

  //
  // Ignore leading Zeros after the spaces
  //
  while (*String == '0') {
    String++;
  }

  *Data = 0;

  while (InternalAsciiIsDecimalDigitCharacter (*String)) {
    //
    // If the number represented by String overflows according to the range
    // defined by UINT64, then MAX_UINT64 is stored in *Data and
    // RETURN_UNSUPPORTED is returned.
    //
    if (*Data > DivU64x32 (MAX_UINT64 - (*String - '0'), 10)) {
      *Data = MAX_UINT64;
      if (EndPointer != NULL) {
        *EndPointer = (CHAR8 *)String;
      }

      return RETURN_UNSUPPORTED;
    }

    *Data = MultU64x32 (*Data, 10) + (*String - '0');
    String++;
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR8 *)String;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Ascii hexadecimal string to a value of type UINTN.

  This function outputs a value of type UINTN by interpreting the contents of
  the Ascii string specified by String as a hexadecimal number. The format of
  the input Ascii string String is:

                  [spaces][zeros][x][hexadecimal digits].

  The valid hexadecimal digit character is in the range [0-9], [a-f] and [A-F].
  The prefix "0x" is optional. Both "x" and "X" is allowed in "0x" prefix. If
  "x" appears in the input string, it must be prefixed with at least one 0. The
  function will ignore the pad space, which includes spaces or tab characters,
  before [zeros], [x] or [hexadecimal digits]. The running zero before [x] or
  [hexadecimal digits] will be ignored. Then, the decoding starts after [x] or
  the first valid hexadecimal digit. Then, the function stops at the first
  character that is a not a valid hexadecimal character or Null-terminator,
  whichever on comes first.

  If String has no valid hexadecimal digits in the above format, then 0 is
  stored at the location pointed to by Data.
  If the number represented by String exceeds the range defined by UINTN, then
  MAX_UINTN is stored at the location pointed to by Data.

  If EndPointer is not NULL, a pointer to the character that stopped the scan
  is stored at the location pointed to by EndPointer. If String has no valid
  hexadecimal digits right after the optional pad spaces, the value of String
  is stored at the location pointed to by EndPointer.

  @param  String                   Pointer to a Null-terminated Ascii string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Data                     Pointer to the converted value.

  @retval RETURN_SUCCESS           Value is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
                                   If PcdMaximumAsciiStringLength is not zero,
                                   and String contains more than
                                   PcdMaximumAsciiStringLength Ascii
                                   characters, not including the
                                   Null-terminator.
  @retval RETURN_UNSUPPORTED       If the number represented by String exceeds
                                   the range defined by UINTN.

**/
RETURN_STATUS
EFIAPI
AsciiStrHexToUintnS (
  IN  CONST CHAR8  *String,
  OUT       CHAR8  **EndPointer   OPTIONAL,
  OUT       UINTN  *Data
  )
{
  BOOLEAN  FoundLeadingZero;

  FoundLeadingZero = FALSE;
  //
  // 1. Neither String nor Data shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Data != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. The length of String shall not be greater than ASCII_RSIZE_MAX.
  //
  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((AsciiStrnLenS (String, ASCII_RSIZE_MAX + 1) <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR8 *)String;
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*String == ' ') || (*String == '\t')) {
    String++;
  }

  //
  // Ignore leading Zeros after the spaces
  //
  FoundLeadingZero = *String == '0';
  while (*String == '0') {
    String++;
  }

  if (AsciiCharToUpper (*String) == 'X') {
    if (!FoundLeadingZero) {
      *Data = 0;
      return RETURN_SUCCESS;
    }

    //
    // Skip the 'X'
    //
    String++;
  }

  *Data = 0;

  while (InternalAsciiIsHexaDecimalDigitCharacter (*String)) {
    //
    // If the number represented by String overflows according to the range
    // defined by UINTN, then MAX_UINTN is stored in *Data and
    // RETURN_UNSUPPORTED is returned.
    //
    if (*Data > ((MAX_UINTN - InternalAsciiHexCharToUintn (*String)) >> 4)) {
      *Data = MAX_UINTN;
      if (EndPointer != NULL) {
        *EndPointer = (CHAR8 *)String;
      }

      return RETURN_UNSUPPORTED;
    }

    *Data = (*Data << 4) + InternalAsciiHexCharToUintn (*String);
    String++;
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR8 *)String;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Ascii hexadecimal string to a value of type UINT64.

  This function outputs a value of type UINT64 by interpreting the contents of
  the Ascii string specified by String as a hexadecimal number. The format of
  the input Ascii string String is:

                  [spaces][zeros][x][hexadecimal digits].

  The valid hexadecimal digit character is in the range [0-9], [a-f] and [A-F].
  The prefix "0x" is optional. Both "x" and "X" is allowed in "0x" prefix. If
  "x" appears in the input string, it must be prefixed with at least one 0. The
  function will ignore the pad space, which includes spaces or tab characters,
  before [zeros], [x] or [hexadecimal digits]. The running zero before [x] or
  [hexadecimal digits] will be ignored. Then, the decoding starts after [x] or
  the first valid hexadecimal digit. Then, the function stops at the first
  character that is a not a valid hexadecimal character or Null-terminator,
  whichever on comes first.

  If String has no valid hexadecimal digits in the above format, then 0 is
  stored at the location pointed to by Data.
  If the number represented by String exceeds the range defined by UINT64, then
  MAX_UINT64 is stored at the location pointed to by Data.

  If EndPointer is not NULL, a pointer to the character that stopped the scan
  is stored at the location pointed to by EndPointer. If String has no valid
  hexadecimal digits right after the optional pad spaces, the value of String
  is stored at the location pointed to by EndPointer.

  @param  String                   Pointer to a Null-terminated Ascii string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Data                     Pointer to the converted value.

  @retval RETURN_SUCCESS           Value is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
                                   If PcdMaximumAsciiStringLength is not zero,
                                   and String contains more than
                                   PcdMaximumAsciiStringLength Ascii
                                   characters, not including the
                                   Null-terminator.
  @retval RETURN_UNSUPPORTED       If the number represented by String exceeds
                                   the range defined by UINT64.

**/
RETURN_STATUS
EFIAPI
AsciiStrHexToUint64S (
  IN  CONST CHAR8   *String,
  OUT       CHAR8   **EndPointer   OPTIONAL,
  OUT       UINT64  *Data
  )
{
  BOOLEAN  FoundLeadingZero;

  FoundLeadingZero = FALSE;
  //
  // 1. Neither String nor Data shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Data != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. The length of String shall not be greater than ASCII_RSIZE_MAX.
  //
  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((AsciiStrnLenS (String, ASCII_RSIZE_MAX + 1) <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR8 *)String;
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*String == ' ') || (*String == '\t')) {
    String++;
  }

  //
  // Ignore leading Zeros after the spaces
  //
  FoundLeadingZero = *String == '0';
  while (*String == '0') {
    String++;
  }

  if (AsciiCharToUpper (*String) == 'X') {
    if (!FoundLeadingZero) {
      *Data = 0;
      return RETURN_SUCCESS;
    }

    //
    // Skip the 'X'
    //
    String++;
  }

  *Data = 0;

  while (InternalAsciiIsHexaDecimalDigitCharacter (*String)) {
    //
    // If the number represented by String overflows according to the range
    // defined by UINT64, then MAX_UINT64 is stored in *Data and
    // RETURN_UNSUPPORTED is returned.
    //
    if (*Data > RShiftU64 (MAX_UINT64 - InternalAsciiHexCharToUintn (*String), 4)) {
      *Data = MAX_UINT64;
      if (EndPointer != NULL) {
        *EndPointer = (CHAR8 *)String;
      }

      return RETURN_UNSUPPORTED;
    }

    *Data = LShiftU64 (*Data, 4) + InternalAsciiHexCharToUintn (*String);
    String++;
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR8 *)String;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated Unicode string to a Null-terminated
  ASCII string.

  This function is similar to AsciiStrCpyS.

  This function converts the content of the Unicode string Source
  to the ASCII string Destination by copying the lower 8 bits of
  each Unicode character. The function terminates the ASCII string
  Destination by appending a Null-terminator character at the end.

  The caller is responsible to make sure Destination points to a buffer with size
  equal or greater than ((StrLen (Source) + 1) * sizeof (CHAR8)) in bytes.

  If any Unicode characters in Source contain non-zero value in
  the upper 8 bits, then ASSERT().

  If Source is not aligned on a 16-bit boundary, then ASSERT().

  If an error is returned, then the Destination is unmodified.

  @param  Source        The pointer to a Null-terminated Unicode string.
  @param  Destination   The pointer to a Null-terminated ASCII string.
  @param  DestMax       The maximum number of Destination Ascii
                        char, including terminating null char.

  @retval RETURN_SUCCESS           String is converted.
  @retval RETURN_BUFFER_TOO_SMALL  If DestMax is NOT greater than StrLen(Source).
  @retval RETURN_INVALID_PARAMETER If Destination is NULL.
                                   If Source is NULL.
                                   If PcdMaximumAsciiStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumAsciiStringLength.
                                   If PcdMaximumUnicodeStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumUnicodeStringLength.
                                   If DestMax is 0.
  @retval RETURN_ACCESS_DENIED     If Source and Destination overlap.

**/
RETURN_STATUS
EFIAPI
UnicodeStrToAsciiStrS (
  IN      CONST CHAR16  *Source,
  OUT     CHAR8         *Destination,
  IN      UINTN         DestMax
  )
{
  UINTN  SourceLen;

  ASSERT (((UINTN)Source & BIT0) == 0);

  //
  // 1. Neither Destination nor Source shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. DestMax shall not be greater than ASCII_RSIZE_MAX or RSIZE_MAX.
  //
  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. DestMax shall be greater than StrnLenS (Source, DestMax).
  //
  SourceLen = StrnLenS (Source, DestMax);
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax > SourceLen), RETURN_BUFFER_TOO_SMALL);

  //
  // 5. Copying shall not take place between objects that overlap.
  //
  SAFE_STRING_CONSTRAINT_CHECK (!InternalSafeStringIsOverlap (Destination, DestMax, (VOID *)Source, (SourceLen + 1) * sizeof (CHAR16)), RETURN_ACCESS_DENIED);

  //
  // convert string
  //
  while (*Source != '\0') {
    //
    // If any Unicode characters in Source contain
    // non-zero value in the upper 8 bits, then ASSERT().
    //
    ASSERT (*Source < 0x100);
    *(Destination++) = (CHAR8)*(Source++);
  }

  *Destination = '\0';

  return RETURN_SUCCESS;
}

/**
  Convert not more than Length successive characters from a Null-terminated
  Unicode string to a Null-terminated Ascii string. If no null char is copied
  from Source, then Destination[Length] is always set to null.

  This function converts not more than Length successive characters from the
  Unicode string Source to the Ascii string Destination by copying the lower 8
  bits of each Unicode character. The function terminates the Ascii string
  Destination by appending a Null-terminator character at the end.

  The caller is responsible to make sure Destination points to a buffer with
  size not smaller than ((MIN(StrLen(Source), Length) + 1) * sizeof (CHAR8))
  in bytes.

  If any Unicode characters in Source contain non-zero value in the upper 8
  bits, then ASSERT().
  If Source is not aligned on a 16-bit boundary, then ASSERT().

  If an error is returned, then Destination and DestinationLength are
  unmodified.

  @param  Source             The pointer to a Null-terminated Unicode string.
  @param  Length             The maximum number of Unicode characters to
                             convert.
  @param  Destination        The pointer to a Null-terminated Ascii string.
  @param  DestMax            The maximum number of Destination Ascii char,
                             including terminating null char.
  @param  DestinationLength  The number of Unicode characters converted.

  @retval RETURN_SUCCESS            String is converted.
  @retval RETURN_INVALID_PARAMETER  If Destination is NULL.
                                    If Source is NULL.
                                    If DestinationLength is NULL.
                                    If PcdMaximumAsciiStringLength is not zero,
                                    and Length or DestMax is greater than
                                    PcdMaximumAsciiStringLength.
                                    If PcdMaximumUnicodeStringLength is not
                                    zero, and Length or DestMax is greater than
                                    PcdMaximumUnicodeStringLength.
                                    If DestMax is 0.
  @retval RETURN_BUFFER_TOO_SMALL   If DestMax is NOT greater than
                                    MIN(StrLen(Source), Length).
  @retval RETURN_ACCESS_DENIED      If Source and Destination overlap.

**/
RETURN_STATUS
EFIAPI
UnicodeStrnToAsciiStrS (
  IN      CONST CHAR16  *Source,
  IN      UINTN         Length,
  OUT     CHAR8         *Destination,
  IN      UINTN         DestMax,
  OUT     UINTN         *DestinationLength
  )
{
  UINTN  SourceLen;

  ASSERT (((UINTN)Source & BIT0) == 0);

  //
  // 1. None of Destination, Source or DestinationLength shall be a null
  // pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((DestinationLength != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. Neither Length nor DestMax shall be greater than ASCII_RSIZE_MAX or
  // RSIZE_MAX.
  //
  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((Length <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((Length <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. If Length is not less than DestMax, then DestMax shall be greater than
  // StrnLenS(Source, DestMax).
  //
  SourceLen = StrnLenS (Source, DestMax);
  if (Length >= DestMax) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax > SourceLen), RETURN_BUFFER_TOO_SMALL);
  }

  //
  // 5. Copying shall not take place between objects that overlap.
  //
  if (SourceLen > Length) {
    SourceLen = Length;
  }

  SAFE_STRING_CONSTRAINT_CHECK (!InternalSafeStringIsOverlap (Destination, DestMax, (VOID *)Source, (SourceLen + 1) * sizeof (CHAR16)), RETURN_ACCESS_DENIED);

  *DestinationLength = 0;

  //
  // Convert string
  //
  while ((*Source != 0) && (SourceLen > 0)) {
    //
    // If any Unicode characters in Source contain non-zero value in the upper
    // 8 bits, then ASSERT().
    //
    ASSERT (*Source < 0x100);
    *(Destination++) = (CHAR8)*(Source++);
    SourceLen--;
    (*DestinationLength)++;
  }

  *Destination = 0;

  return RETURN_SUCCESS;
}

/**
  Convert one Null-terminated ASCII string to a Null-terminated
  Unicode string.

  This function is similar to StrCpyS.

  This function converts the contents of the ASCII string Source to the Unicode
  string Destination. The function terminates the Unicode string Destination by
  appending a Null-terminator character at the end.

  The caller is responsible to make sure Destination points to a buffer with size
  equal or greater than ((AsciiStrLen (Source) + 1) * sizeof (CHAR16)) in bytes.

  If Destination is not aligned on a 16-bit boundary, then ASSERT().

  If an error is returned, then the Destination is unmodified.

  @param  Source        The pointer to a Null-terminated ASCII string.
  @param  Destination   The pointer to a Null-terminated Unicode string.
  @param  DestMax       The maximum number of Destination Unicode
                        char, including terminating null char.

  @retval RETURN_SUCCESS           String is converted.
  @retval RETURN_BUFFER_TOO_SMALL  If DestMax is NOT greater than StrLen(Source).
  @retval RETURN_INVALID_PARAMETER If Destination is NULL.
                                   If Source is NULL.
                                   If PcdMaximumUnicodeStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumUnicodeStringLength.
                                   If PcdMaximumAsciiStringLength is not zero,
                                    and DestMax is greater than
                                    PcdMaximumAsciiStringLength.
                                   If DestMax is 0.
  @retval RETURN_ACCESS_DENIED     If Source and Destination overlap.

**/
RETURN_STATUS
EFIAPI
AsciiStrToUnicodeStrS (
  IN      CONST CHAR8  *Source,
  OUT     CHAR16       *Destination,
  IN      UINTN        DestMax
  )
{
  UINTN  SourceLen;

  ASSERT (((UINTN)Destination & BIT0) == 0);

  //
  // 1. Neither Destination nor Source shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. DestMax shall not be greater than RSIZE_MAX or ASCII_RSIZE_MAX.
  //
  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. DestMax shall be greater than AsciiStrnLenS(Source, DestMax).
  //
  SourceLen = AsciiStrnLenS (Source, DestMax);
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax > SourceLen), RETURN_BUFFER_TOO_SMALL);

  //
  // 5. Copying shall not take place between objects that overlap.
  //
  SAFE_STRING_CONSTRAINT_CHECK (!InternalSafeStringIsOverlap (Destination, DestMax * sizeof (CHAR16), (VOID *)Source, SourceLen + 1), RETURN_ACCESS_DENIED);

  //
  // Convert string
  //
  while (*Source != '\0') {
    *(Destination++) = (CHAR16)(UINT8)*(Source++);
  }

  *Destination = '\0';

  return RETURN_SUCCESS;
}

/**
  Convert not more than Length successive characters from a Null-terminated
  Ascii string to a Null-terminated Unicode string. If no null char is copied
  from Source, then Destination[Length] is always set to null.

  This function converts not more than Length successive characters from the
  Ascii string Source to the Unicode string Destination. The function
  terminates the Unicode string Destination by appending a Null-terminator
  character at the end.

  The caller is responsible to make sure Destination points to a buffer with
  size not smaller than
  ((MIN(AsciiStrLen(Source), Length) + 1) * sizeof (CHAR8)) in bytes.

  If Destination is not aligned on a 16-bit boundary, then ASSERT().

  If an error is returned, then Destination and DestinationLength are
  unmodified.

  @param  Source             The pointer to a Null-terminated Ascii string.
  @param  Length             The maximum number of Ascii characters to convert.
  @param  Destination        The pointer to a Null-terminated Unicode string.
  @param  DestMax            The maximum number of Destination Unicode char,
                             including terminating null char.
  @param  DestinationLength  The number of Ascii characters converted.

  @retval RETURN_SUCCESS            String is converted.
  @retval RETURN_INVALID_PARAMETER  If Destination is NULL.
                                    If Source is NULL.
                                    If DestinationLength is NULL.
                                    If PcdMaximumUnicodeStringLength is not
                                    zero, and Length or DestMax is greater than
                                    PcdMaximumUnicodeStringLength.
                                    If PcdMaximumAsciiStringLength is not zero,
                                    and Length or DestMax is greater than
                                    PcdMaximumAsciiStringLength.
                                    If DestMax is 0.
  @retval RETURN_BUFFER_TOO_SMALL   If DestMax is NOT greater than
                                    MIN(AsciiStrLen(Source), Length).
  @retval RETURN_ACCESS_DENIED      If Source and Destination overlap.

**/
RETURN_STATUS
EFIAPI
AsciiStrnToUnicodeStrS (
  IN      CONST CHAR8  *Source,
  IN      UINTN        Length,
  OUT     CHAR16       *Destination,
  IN      UINTN        DestMax,
  OUT     UINTN        *DestinationLength
  )
{
  UINTN  SourceLen;

  ASSERT (((UINTN)Destination & BIT0) == 0);

  //
  // 1. None of Destination, Source or DestinationLength shall be a null
  // pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((Destination != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Source != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((DestinationLength != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. Neither Length nor DestMax shall be greater than ASCII_RSIZE_MAX or
  // RSIZE_MAX.
  //
  if (RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((Length <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((Length <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. DestMax shall not equal zero.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((DestMax != 0), RETURN_INVALID_PARAMETER);

  //
  // 4. If Length is not less than DestMax, then DestMax shall be greater than
  // AsciiStrnLenS(Source, DestMax).
  //
  SourceLen = AsciiStrnLenS (Source, DestMax);
  if (Length >= DestMax) {
    SAFE_STRING_CONSTRAINT_CHECK ((DestMax > SourceLen), RETURN_BUFFER_TOO_SMALL);
  }

  //
  // 5. Copying shall not take place between objects that overlap.
  //
  if (SourceLen > Length) {
    SourceLen = Length;
  }

  SAFE_STRING_CONSTRAINT_CHECK (!InternalSafeStringIsOverlap (Destination, DestMax * sizeof (CHAR16), (VOID *)Source, SourceLen + 1), RETURN_ACCESS_DENIED);

  *DestinationLength = 0;

  //
  // Convert string
  //
  while ((*Source != 0) && (SourceLen > 0)) {
    *(Destination++) = (CHAR16)(UINT8)*(Source++);
    SourceLen--;
    (*DestinationLength)++;
  }

  *Destination = 0;

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated ASCII string to IPv6 address and prefix length.

  This function outputs a value of type IPv6_ADDRESS and may output a value
  of type UINT8 by interpreting the contents of the ASCII string specified
  by String. The format of the input ASCII string String is as follows:

                  X:X:X:X:X:X:X:X[/P]

  X contains one to four hexadecimal digit characters in the range [0-9], [a-f] and
  [A-F]. X is converted to a value of type UINT16, whose low byte is stored in low
  memory address and high byte is stored in high memory address. P contains decimal
  digit characters in the range [0-9]. The running zero in the beginning of P will
  be ignored. /P is optional.

  When /P is not in the String, the function stops at the first character that is
  not a valid hexadecimal digit character after eight X's are converted.

  When /P is in the String, the function stops at the first character that is not
  a valid decimal digit character after P is converted.

  "::" can be used to compress one or more groups of X when X contains only 0.
  The "::" can only appear once in the String.

  If EndPointer is not NULL and Address is translated from String, a pointer
  to the character that stopped the scan is stored at the location pointed to
  by EndPointer.

  @param  String                   Pointer to a Null-terminated ASCII string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Address                  Pointer to the converted IPv6 address.
  @param  PrefixLength             Pointer to the converted IPv6 address prefix
                                   length. MAX_UINT8 is returned when /P is
                                   not in the String.

  @retval RETURN_SUCCESS           Address is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
  @retval RETURN_UNSUPPORTED       If X contains more than four hexadecimal
                                    digit characters.
                                   If String contains "::" and number of X
                                    is not less than 8.
                                   If P starts with character that is not a
                                    valid decimal digit character.
                                   If the decimal number converted from P
                                    exceeds 128.

**/
RETURN_STATUS
EFIAPI
AsciiStrToIpv6Address (
  IN  CONST CHAR8   *String,
  OUT CHAR8         **EndPointer  OPTIONAL,
  OUT IPv6_ADDRESS  *Address,
  OUT UINT8         *PrefixLength OPTIONAL
  )
{
  RETURN_STATUS  Status;
  UINTN          AddressIndex;
  UINTN          Uintn;
  IPv6_ADDRESS   LocalAddress;
  UINT8          LocalPrefixLength;
  CONST CHAR8    *Pointer;
  CHAR8          *End;
  UINTN          CompressStart;
  BOOLEAN        ExpectPrefix;

  LocalPrefixLength = MAX_UINT8;
  CompressStart     = ARRAY_SIZE (Address->Addr);
  ExpectPrefix      = FALSE;

  //
  // None of String or Address shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Address != NULL), RETURN_INVALID_PARAMETER);

  for (Pointer = String, AddressIndex = 0; AddressIndex < ARRAY_SIZE (Address->Addr) + 1;) {
    if (!InternalAsciiIsHexaDecimalDigitCharacter (*Pointer)) {
      if (*Pointer != ':') {
        //
        // ":" or "/" should be followed by digit characters.
        //
        return RETURN_UNSUPPORTED;
      }

      //
      // Meet second ":" after previous ":" or "/"
      // or meet first ":" in the beginning of String.
      //
      if (ExpectPrefix) {
        //
        // ":" shall not be after "/"
        //
        return RETURN_UNSUPPORTED;
      }

      if ((CompressStart != ARRAY_SIZE (Address->Addr)) || (AddressIndex == ARRAY_SIZE (Address->Addr))) {
        //
        // "::" can only appear once.
        // "::" can only appear when address is not full length.
        //
        return RETURN_UNSUPPORTED;
      } else {
        //
        // Remember the start of zero compressing.
        //
        CompressStart = AddressIndex;
        Pointer++;

        if (CompressStart == 0) {
          if (*Pointer != ':') {
            //
            // Single ":" shall not be in the beginning of String.
            //
            return RETURN_UNSUPPORTED;
          }

          Pointer++;
        }
      }
    }

    if (!InternalAsciiIsHexaDecimalDigitCharacter (*Pointer)) {
      if (*Pointer == '/') {
        //
        // Might be optional "/P" after "::".
        //
        if (CompressStart != AddressIndex) {
          return RETURN_UNSUPPORTED;
        }
      } else {
        break;
      }
    } else {
      if (!ExpectPrefix) {
        //
        // Get X.
        //
        Status = AsciiStrHexToUintnS (Pointer, &End, &Uintn);
        if (RETURN_ERROR (Status) || (End - Pointer > 4)) {
          //
          // Number of hexadecimal digit characters is no more than 4.
          //
          return RETURN_UNSUPPORTED;
        }

        Pointer = End;
        //
        // Uintn won't exceed MAX_UINT16 if number of hexadecimal digit characters is no more than 4.
        //
        ASSERT (AddressIndex + 1 < ARRAY_SIZE (Address->Addr));
        LocalAddress.Addr[AddressIndex]     = (UINT8)((UINT16)Uintn >> 8);
        LocalAddress.Addr[AddressIndex + 1] = (UINT8)Uintn;
        AddressIndex                       += 2;
      } else {
        //
        // Get P, then exit the loop.
        //
        Status = AsciiStrDecimalToUintnS (Pointer, &End, &Uintn);
        if (RETURN_ERROR (Status) || (End == Pointer) || (Uintn > 128)) {
          //
          // Prefix length should not exceed 128.
          //
          return RETURN_UNSUPPORTED;
        }

        LocalPrefixLength = (UINT8)Uintn;
        Pointer           = End;
        break;
      }
    }

    //
    // Skip ':' or "/"
    //
    if (*Pointer == '/') {
      ExpectPrefix = TRUE;
    } else if (*Pointer == ':') {
      if (AddressIndex == ARRAY_SIZE (Address->Addr)) {
        //
        // Meet additional ":" after all 8 16-bit address
        //
        break;
      }
    } else {
      //
      // Meet other character that is not "/" or ":" after all 8 16-bit address
      //
      break;
    }

    Pointer++;
  }

  if (((AddressIndex == ARRAY_SIZE (Address->Addr)) && (CompressStart != ARRAY_SIZE (Address->Addr))) ||
      ((AddressIndex != ARRAY_SIZE (Address->Addr)) && (CompressStart == ARRAY_SIZE (Address->Addr)))
      )
  {
    //
    // Full length of address shall not have compressing zeros.
    // Non-full length of address shall have compressing zeros.
    //
    return RETURN_UNSUPPORTED;
  }

  CopyMem (&Address->Addr[0], &LocalAddress.Addr[0], CompressStart);
  ZeroMem (&Address->Addr[CompressStart], ARRAY_SIZE (Address->Addr) - AddressIndex);
  if (AddressIndex > CompressStart) {
    CopyMem (
      &Address->Addr[CompressStart + ARRAY_SIZE (Address->Addr) - AddressIndex],
      &LocalAddress.Addr[CompressStart],
      AddressIndex - CompressStart
      );
  }

  if (PrefixLength != NULL) {
    *PrefixLength = LocalPrefixLength;
  }

  if (EndPointer != NULL) {
    *EndPointer = (CHAR8 *)Pointer;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated ASCII string to IPv4 address and prefix length.

  This function outputs a value of type IPv4_ADDRESS and may output a value
  of type UINT8 by interpreting the contents of the ASCII string specified
  by String. The format of the input ASCII string String is as follows:

                  D.D.D.D[/P]

  D and P are decimal digit characters in the range [0-9]. The running zero in
  the beginning of D and P will be ignored. /P is optional.

  When /P is not in the String, the function stops at the first character that is
  not a valid decimal digit character after four D's are converted.

  When /P is in the String, the function stops at the first character that is not
  a valid decimal digit character after P is converted.

  If EndPointer is not NULL and Address is translated from String, a pointer
  to the character that stopped the scan is stored at the location pointed to
  by EndPointer.

  @param  String                   Pointer to a Null-terminated ASCII string.
  @param  EndPointer               Pointer to character that stops scan.
  @param  Address                  Pointer to the converted IPv4 address.
  @param  PrefixLength             Pointer to the converted IPv4 address prefix
                                   length. MAX_UINT8 is returned when /P is
                                   not in the String.

  @retval RETURN_SUCCESS           Address is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
  @retval RETURN_UNSUPPORTED       If String is not in the correct format.
                                   If any decimal number converted from D
                                    exceeds 255.
                                   If the decimal number converted from P
                                    exceeds 32.

**/
RETURN_STATUS
EFIAPI
AsciiStrToIpv4Address (
  IN  CONST CHAR8   *String,
  OUT CHAR8         **EndPointer  OPTIONAL,
  OUT IPv4_ADDRESS  *Address,
  OUT UINT8         *PrefixLength OPTIONAL
  )
{
  RETURN_STATUS  Status;
  UINTN          AddressIndex;
  UINTN          Uintn;
  IPv4_ADDRESS   LocalAddress;
  UINT8          LocalPrefixLength;
  CHAR8          *Pointer;

  LocalPrefixLength = MAX_UINT8;

  //
  // None of String or Address shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Address != NULL), RETURN_INVALID_PARAMETER);

  for (Pointer = (CHAR8 *)String, AddressIndex = 0; AddressIndex < ARRAY_SIZE (Address->Addr) + 1;) {
    if (!InternalAsciiIsDecimalDigitCharacter (*Pointer)) {
      //
      // D or P contains invalid characters.
      //
      break;
    }

    //
    // Get D or P.
    //
    Status = AsciiStrDecimalToUintnS ((CONST CHAR8 *)Pointer, &Pointer, &Uintn);
    if (RETURN_ERROR (Status)) {
      return RETURN_UNSUPPORTED;
    }

    if (AddressIndex == ARRAY_SIZE (Address->Addr)) {
      //
      // It's P.
      //
      if (Uintn > 32) {
        return RETURN_UNSUPPORTED;
      }

      LocalPrefixLength = (UINT8)Uintn;
    } else {
      //
      // It's D.
      //
      if (Uintn > MAX_UINT8) {
        return RETURN_UNSUPPORTED;
      }

      LocalAddress.Addr[AddressIndex] = (UINT8)Uintn;
      AddressIndex++;
    }

    //
    // Check the '.' or '/', depending on the AddressIndex.
    //
    if (AddressIndex == ARRAY_SIZE (Address->Addr)) {
      if (*Pointer == '/') {
        //
        // '/P' is in the String.
        // Skip "/" and get P in next loop.
        //
        Pointer++;
      } else {
        //
        // '/P' is not in the String.
        //
        break;
      }
    } else if (AddressIndex < ARRAY_SIZE (Address->Addr)) {
      if (*Pointer == '.') {
        //
        // D should be followed by '.'
        //
        Pointer++;
      } else {
        return RETURN_UNSUPPORTED;
      }
    }
  }

  if (AddressIndex < ARRAY_SIZE (Address->Addr)) {
    return RETURN_UNSUPPORTED;
  }

  CopyMem (Address, &LocalAddress, sizeof (*Address));
  if (PrefixLength != NULL) {
    *PrefixLength = LocalPrefixLength;
  }

  if (EndPointer != NULL) {
    *EndPointer = Pointer;
  }

  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated ASCII GUID string to a value of type
  EFI_GUID.

  This function outputs a GUID value by interpreting the contents of
  the ASCII string specified by String. The format of the input
  ASCII string String consists of 36 characters, as follows:

                  aabbccdd-eeff-gghh-iijj-kkllmmnnoopp

  The pairs aa - pp are two characters in the range [0-9], [a-f] and
  [A-F], with each pair representing a single byte hexadecimal value.

  The mapping between String and the EFI_GUID structure is as follows:
                  aa          Data1[24:31]
                  bb          Data1[16:23]
                  cc          Data1[8:15]
                  dd          Data1[0:7]
                  ee          Data2[8:15]
                  ff          Data2[0:7]
                  gg          Data3[8:15]
                  hh          Data3[0:7]
                  ii          Data4[0:7]
                  jj          Data4[8:15]
                  kk          Data4[16:23]
                  ll          Data4[24:31]
                  mm          Data4[32:39]
                  nn          Data4[40:47]
                  oo          Data4[48:55]
                  pp          Data4[56:63]

  @param  String                   Pointer to a Null-terminated ASCII string.
  @param  Guid                     Pointer to the converted GUID.

  @retval RETURN_SUCCESS           Guid is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
  @retval RETURN_UNSUPPORTED       If String is not as the above format.

**/
RETURN_STATUS
EFIAPI
AsciiStrToGuid (
  IN  CONST CHAR8  *String,
  OUT GUID         *Guid
  )
{
  RETURN_STATUS  Status;
  GUID           LocalGuid;

  //
  // None of String or Guid shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Guid != NULL), RETURN_INVALID_PARAMETER);

  //
  // Get aabbccdd in big-endian.
  //
  Status = AsciiStrHexToBytes (String, 2 * sizeof (LocalGuid.Data1), (UINT8 *)&LocalGuid.Data1, sizeof (LocalGuid.Data1));
  if (RETURN_ERROR (Status) || (String[2 * sizeof (LocalGuid.Data1)] != '-')) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Convert big-endian to little-endian.
  //
  LocalGuid.Data1 = SwapBytes32 (LocalGuid.Data1);
  String         += 2 * sizeof (LocalGuid.Data1) + 1;

  //
  // Get eeff in big-endian.
  //
  Status = AsciiStrHexToBytes (String, 2 * sizeof (LocalGuid.Data2), (UINT8 *)&LocalGuid.Data2, sizeof (LocalGuid.Data2));
  if (RETURN_ERROR (Status) || (String[2 * sizeof (LocalGuid.Data2)] != '-')) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Convert big-endian to little-endian.
  //
  LocalGuid.Data2 = SwapBytes16 (LocalGuid.Data2);
  String         += 2 * sizeof (LocalGuid.Data2) + 1;

  //
  // Get gghh in big-endian.
  //
  Status = AsciiStrHexToBytes (String, 2 * sizeof (LocalGuid.Data3), (UINT8 *)&LocalGuid.Data3, sizeof (LocalGuid.Data3));
  if (RETURN_ERROR (Status) || (String[2 * sizeof (LocalGuid.Data3)] != '-')) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Convert big-endian to little-endian.
  //
  LocalGuid.Data3 = SwapBytes16 (LocalGuid.Data3);
  String         += 2 * sizeof (LocalGuid.Data3) + 1;

  //
  // Get iijj.
  //
  Status = AsciiStrHexToBytes (String, 2 * 2, &LocalGuid.Data4[0], 2);
  if (RETURN_ERROR (Status) || (String[2 * 2] != '-')) {
    return RETURN_UNSUPPORTED;
  }

  String += 2 * 2 + 1;

  //
  // Get kkllmmnnoopp.
  //
  Status = AsciiStrHexToBytes (String, 2 * 6, &LocalGuid.Data4[2], 6);
  if (RETURN_ERROR (Status)) {
    return RETURN_UNSUPPORTED;
  }

  CopyGuid (Guid, &LocalGuid);
  return RETURN_SUCCESS;
}

/**
  Convert a Null-terminated ASCII hexadecimal string to a byte array.

  This function outputs a byte array by interpreting the contents of
  the ASCII string specified by String in hexadecimal format. The format of
  the input ASCII string String is:

                  [XX]*

  X is a hexadecimal digit character in the range [0-9], [a-f] and [A-F].
  The function decodes every two hexadecimal digit characters as one byte. The
  decoding stops after Length of characters and outputs Buffer containing
  (Length / 2) bytes.

  @param  String                   Pointer to a Null-terminated ASCII string.
  @param  Length                   The number of ASCII characters to decode.
  @param  Buffer                   Pointer to the converted bytes array.
  @param  MaxBufferSize            The maximum size of Buffer.

  @retval RETURN_SUCCESS           Buffer is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
                                   If Length is not multiple of 2.
                                   If PcdMaximumAsciiStringLength is not zero,
                                    and Length is greater than
                                    PcdMaximumAsciiStringLength.
  @retval RETURN_UNSUPPORTED       If Length of characters from String contain
                                    a character that is not valid hexadecimal
                                    digit characters, or a Null-terminator.
  @retval RETURN_BUFFER_TOO_SMALL  If MaxBufferSize is less than (Length / 2).
**/
RETURN_STATUS
EFIAPI
AsciiStrHexToBytes (
  IN  CONST CHAR8  *String,
  IN  UINTN        Length,
  OUT UINT8        *Buffer,
  IN  UINTN        MaxBufferSize
  )
{
  UINTN  Index;

  //
  // 1. None of String or Buffer shall be a null pointer.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((String != NULL), RETURN_INVALID_PARAMETER);
  SAFE_STRING_CONSTRAINT_CHECK ((Buffer != NULL), RETURN_INVALID_PARAMETER);

  //
  // 2. Length shall not be greater than ASCII_RSIZE_MAX.
  //
  if (ASCII_RSIZE_MAX != 0) {
    SAFE_STRING_CONSTRAINT_CHECK ((Length <= ASCII_RSIZE_MAX), RETURN_INVALID_PARAMETER);
  }

  //
  // 3. Length shall not be odd.
  //
  SAFE_STRING_CONSTRAINT_CHECK (((Length & BIT0) == 0), RETURN_INVALID_PARAMETER);

  //
  // 4. MaxBufferSize shall equal to or greater than Length / 2.
  //
  SAFE_STRING_CONSTRAINT_CHECK ((MaxBufferSize >= Length / 2), RETURN_BUFFER_TOO_SMALL);

  //
  // 5. String shall not contains invalid hexadecimal digits.
  //
  for (Index = 0; Index < Length; Index++) {
    if (!InternalAsciiIsHexaDecimalDigitCharacter (String[Index])) {
      break;
    }
  }

  if (Index != Length) {
    return RETURN_UNSUPPORTED;
  }

  //
  // Convert the hex string to bytes.
  //
  for (Index = 0; Index < Length; Index++) {
    //
    // For even characters, write the upper nibble for each buffer byte,
    // and for even characters, the lower nibble.
    //
    if ((Index & BIT0) == 0) {
      Buffer[Index / 2] = (UINT8)InternalAsciiHexCharToUintn (String[Index]) << 4;
    } else {
      Buffer[Index / 2] |= (UINT8)InternalAsciiHexCharToUintn (String[Index]);
    }
  }

  return RETURN_SUCCESS;
}
