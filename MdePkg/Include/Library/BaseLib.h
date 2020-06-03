/** @file
  Provides string functions, linked list functions, math functions, synchronization
  functions, file path functions, and CPU architecture-specific functions.

Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.<BR>
Portions Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __BASE_LIB__
#define __BASE_LIB__

//
// Definitions for architecture-specific types
//
#if   defined (MDE_CPU_IA32)
///
/// The IA-32 architecture context buffer used by SetJump() and LongJump().
///
typedef struct {
  UINT32                            Ebx;
  UINT32                            Esi;
  UINT32                            Edi;
  UINT32                            Ebp;
  UINT32                            Esp;
  UINT32                            Eip;
  UINT32                            Ssp;
} BASE_LIBRARY_JUMP_BUFFER;

#define BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT 4

#endif // defined (MDE_CPU_IA32)

#if defined (MDE_CPU_X64)
///
/// The x64 architecture context buffer used by SetJump() and LongJump().
///
typedef struct {
  UINT64                            Rbx;
  UINT64                            Rsp;
  UINT64                            Rbp;
  UINT64                            Rdi;
  UINT64                            Rsi;
  UINT64                            R12;
  UINT64                            R13;
  UINT64                            R14;
  UINT64                            R15;
  UINT64                            Rip;
  UINT64                            MxCsr;
  UINT8                             XmmBuffer[160]; ///< XMM6-XMM15.
  UINT64                            Ssp;
} BASE_LIBRARY_JUMP_BUFFER;

#define BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT 8

#endif // defined (MDE_CPU_X64)

#if defined (MDE_CPU_EBC)
///
/// The EBC context buffer used by SetJump() and LongJump().
///
typedef struct {
  UINT64                            R0;
  UINT64                            R1;
  UINT64                            R2;
  UINT64                            R3;
  UINT64                            IP;
} BASE_LIBRARY_JUMP_BUFFER;

#define BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT 8

#endif // defined (MDE_CPU_EBC)

#if defined (MDE_CPU_ARM)

typedef struct {
  UINT32    R3;  ///< A copy of R13.
  UINT32    R4;
  UINT32    R5;
  UINT32    R6;
  UINT32    R7;
  UINT32    R8;
  UINT32    R9;
  UINT32    R10;
  UINT32    R11;
  UINT32    R12;
  UINT32    R14;
} BASE_LIBRARY_JUMP_BUFFER;

#define BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT 4

#endif  // defined (MDE_CPU_ARM)

#if defined (MDE_CPU_AARCH64)
typedef struct {
  // GP regs
  UINT64    X19;
  UINT64    X20;
  UINT64    X21;
  UINT64    X22;
  UINT64    X23;
  UINT64    X24;
  UINT64    X25;
  UINT64    X26;
  UINT64    X27;
  UINT64    X28;
  UINT64    FP;
  UINT64    LR;
  UINT64    IP0;

  // FP regs
  UINT64    D8;
  UINT64    D9;
  UINT64    D10;
  UINT64    D11;
  UINT64    D12;
  UINT64    D13;
  UINT64    D14;
  UINT64    D15;
} BASE_LIBRARY_JUMP_BUFFER;

#define BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT 8

#endif  // defined (MDE_CPU_AARCH64)

#if defined (MDE_CPU_RISCV64)
///
/// The RISC-V architecture context buffer used by SetJump() and LongJump().
///
typedef struct {
  UINT64                            RA;
  UINT64                            S0;
  UINT64                            S1;
  UINT64                            S2;
  UINT64                            S3;
  UINT64                            S4;
  UINT64                            S5;
  UINT64                            S6;
  UINT64                            S7;
  UINT64                            S8;
  UINT64                            S9;
  UINT64                            S10;
  UINT64                            S11;
  UINT64                            SP;
} BASE_LIBRARY_JUMP_BUFFER;

#define BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT 8

#endif // defined (MDE_CPU_RISCV64)

//
// String Services
//


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
  IN CONST CHAR16              *String,
  IN UINTN                     MaxSize
  );

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
  IN CONST CHAR16              *String,
  IN UINTN                     MaxSize
  );

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
  OUT CHAR16       *Destination,
  IN  UINTN        DestMax,
  IN  CONST CHAR16 *Source
  );

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
  OUT CHAR16       *Destination,
  IN  UINTN        DestMax,
  IN  CONST CHAR16 *Source,
  IN  UINTN        Length
  );

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
  IN OUT CHAR16       *Destination,
  IN     UINTN        DestMax,
  IN     CONST CHAR16 *Source
  );

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
  IN OUT CHAR16       *Destination,
  IN     UINTN        DestMax,
  IN     CONST CHAR16 *Source,
  IN     UINTN        Length
  );

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
  IN  CONST CHAR16             *String,
  OUT       CHAR16             **EndPointer,  OPTIONAL
  OUT       UINTN              *Data
  );

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
  IN  CONST CHAR16             *String,
  OUT       CHAR16             **EndPointer,  OPTIONAL
  OUT       UINT64             *Data
  );

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
  IN  CONST CHAR16             *String,
  OUT       CHAR16             **EndPointer,  OPTIONAL
  OUT       UINTN              *Data
  );

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
  IN  CONST CHAR16             *String,
  OUT       CHAR16             **EndPointer,  OPTIONAL
  OUT       UINT64             *Data
  );

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
  IN CONST CHAR8               *String,
  IN UINTN                     MaxSize
  );

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
  IN CONST CHAR8               *String,
  IN UINTN                     MaxSize
  );

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
  );

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
  );

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
  );

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
  );

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
  IN  CONST CHAR8              *String,
  OUT       CHAR8              **EndPointer,  OPTIONAL
  OUT       UINTN              *Data
  );

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
  IN  CONST CHAR8              *String,
  OUT       CHAR8              **EndPointer,  OPTIONAL
  OUT       UINT64             *Data
  );

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
  IN  CONST CHAR8              *String,
  OUT       CHAR8              **EndPointer,  OPTIONAL
  OUT       UINTN              *Data
  );

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
  IN  CONST CHAR8              *String,
  OUT       CHAR8              **EndPointer,  OPTIONAL
  OUT       UINT64             *Data
  );


#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function is deprecated for security reason.

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
  PcdMaximumUnicodeStringLength Unicode characters not including the
  Null-terminator, then ASSERT().

  @param  Destination The pointer to a Null-terminated Unicode string.
  @param  Source      The pointer to a Null-terminated Unicode string.

  @return Destination.

**/
CHAR16 *
EFIAPI
StrCpy (
  OUT     CHAR16                    *Destination,
  IN      CONST CHAR16              *Source
  );


/**
  [ATTENTION] This function is deprecated for security reason.

  Copies up to a specified length from one Null-terminated Unicode string to
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

  @param  Destination The pointer to a Null-terminated Unicode string.
  @param  Source      The pointer to a Null-terminated Unicode string.
  @param  Length      The maximum number of Unicode characters to copy.

  @return Destination.

**/
CHAR16 *
EFIAPI
StrnCpy (
  OUT     CHAR16                    *Destination,
  IN      CONST CHAR16              *Source,
  IN      UINTN                     Length
  );
#endif // !defined (DISABLE_NEW_DEPRECATED_INTERFACES)

/**
  Returns the length of a Null-terminated Unicode string.

  This function returns the number of Unicode characters in the Null-terminated
  Unicode string specified by String.

  If String is NULL, then ASSERT().
  If String is not aligned on a 16-bit boundary, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and String contains more than
  PcdMaximumUnicodeStringLength Unicode characters not including the
  Null-terminator, then ASSERT().

  @param  String  Pointer to a Null-terminated Unicode string.

  @return The length of String.

**/
UINTN
EFIAPI
StrLen (
  IN      CONST CHAR16              *String
  );


/**
  Returns the size of a Null-terminated Unicode string in bytes, including the
  Null terminator.

  This function returns the size, in bytes, of the Null-terminated Unicode string
  specified by String.

  If String is NULL, then ASSERT().
  If String is not aligned on a 16-bit boundary, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and String contains more than
  PcdMaximumUnicodeStringLength Unicode characters not including the
  Null-terminator, then ASSERT().

  @param  String  The pointer to a Null-terminated Unicode string.

  @return The size of String.

**/
UINTN
EFIAPI
StrSize (
  IN      CONST CHAR16              *String
  );


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
  than PcdMaximumUnicodeStringLength Unicode characters not including the
  Null-terminator, then ASSERT().
  If PcdMaximumUnicodeStringLength is not zero, and SecondString contains more
  than PcdMaximumUnicodeStringLength Unicode characters, not including the
  Null-terminator, then ASSERT().

  @param  FirstString   The pointer to a Null-terminated Unicode string.
  @param  SecondString  The pointer to a Null-terminated Unicode string.

  @retval 0      FirstString is identical to SecondString.
  @return others FirstString is not identical to SecondString.

**/
INTN
EFIAPI
StrCmp (
  IN      CONST CHAR16              *FirstString,
  IN      CONST CHAR16              *SecondString
  );


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

  @param  FirstString   The pointer to a Null-terminated Unicode string.
  @param  SecondString  The pointer to a Null-terminated Unicode string.
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
  );


#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function is deprecated for security reason.

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

  @param  Destination The pointer to a Null-terminated Unicode string.
  @param  Source      The pointer to a Null-terminated Unicode string.

  @return Destination.

**/
CHAR16 *
EFIAPI
StrCat (
  IN OUT  CHAR16                    *Destination,
  IN      CONST CHAR16              *Source
  );


/**
  [ATTENTION] This function is deprecated for security reason.

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

  @param  Destination The pointer to a Null-terminated Unicode string.
  @param  Source      The pointer to a Null-terminated Unicode string.
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
  );
#endif // !defined (DISABLE_NEW_DEPRECATED_INTERFACES)

/**
  Returns the first occurrence of a Null-terminated Unicode sub-string
  in a Null-terminated Unicode string.

  This function scans the contents of the Null-terminated Unicode string
  specified by String and returns the first occurrence of SearchString.
  If SearchString is not found in String, then NULL is returned.  If
  the length of SearchString is zero, then String is returned.

  If String is NULL, then ASSERT().
  If String is not aligned on a 16-bit boundary, then ASSERT().
  If SearchString is NULL, then ASSERT().
  If SearchString is not aligned on a 16-bit boundary, then ASSERT().

  If PcdMaximumUnicodeStringLength is not zero, and SearchString
  or String contains more than PcdMaximumUnicodeStringLength Unicode
  characters, not including the Null-terminator, then ASSERT().

  @param  String          The pointer to a Null-terminated Unicode string.
  @param  SearchString    The pointer to a Null-terminated Unicode string to search for.

  @retval NULL            If the SearchString does not appear in String.
  @return others          If there is a match.

**/
CHAR16 *
EFIAPI
StrStr (
  IN      CONST CHAR16              *String,
  IN      CONST CHAR16              *SearchString
  );

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
  more than PcdMaximumUnicodeStringLength Unicode characters not including
  the Null-terminator, then ASSERT().

  @param  String      The pointer to a Null-terminated Unicode string.

  @retval Value translated from String.

**/
UINTN
EFIAPI
StrDecimalToUintn (
  IN      CONST CHAR16              *String
  );

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
  more than PcdMaximumUnicodeStringLength Unicode characters not including
  the Null-terminator, then ASSERT().

  @param  String          The pointer to a Null-terminated Unicode string.

  @retval Value translated from String.

**/
UINT64
EFIAPI
StrDecimalToUint64 (
  IN      CONST CHAR16              *String
  );


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
  first valid hexadecimal digit. Then, the function stops at the first character
  that is a not a valid hexadecimal character or NULL, whichever one comes first.

  If String is NULL, then ASSERT().
  If String is not aligned in a 16-bit boundary, then ASSERT().
  If String has only pad spaces, then zero is returned.
  If String has no leading pad spaces, leading zeros or valid hexadecimal digits,
  then zero is returned.
  If the number represented by String overflows according to the range defined by
  UINTN, then MAX_UINTN is returned.

  If PcdMaximumUnicodeStringLength is not zero, and String contains more than
  PcdMaximumUnicodeStringLength Unicode characters not including the Null-terminator,
  then ASSERT().

  @param  String          The pointer to a Null-terminated Unicode string.

  @retval Value translated from String.

**/
UINTN
EFIAPI
StrHexToUintn (
  IN      CONST CHAR16              *String
  );


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
  PcdMaximumUnicodeStringLength Unicode characters not including the Null-terminator,
  then ASSERT().

  @param  String          The pointer to a Null-terminated Unicode string.

  @retval Value translated from String.

**/
UINT64
EFIAPI
StrHexToUint64 (
  IN      CONST CHAR16             *String
  );

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
  IN  CONST CHAR16       *String,
  OUT CHAR16             **EndPointer, OPTIONAL
  OUT IPv6_ADDRESS       *Address,
  OUT UINT8              *PrefixLength OPTIONAL
  );

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
  IN  CONST CHAR16       *String,
  OUT CHAR16             **EndPointer, OPTIONAL
  OUT IPv4_ADDRESS       *Address,
  OUT UINT8              *PrefixLength OPTIONAL
  );

#define GUID_STRING_LENGTH  36

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
  IN  CONST CHAR16       *String,
  OUT GUID               *Guid
  );

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
  IN  CONST CHAR16       *String,
  IN  UINTN              Length,
  OUT UINT8              *Buffer,
  IN  UINTN              MaxBufferSize
  );

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
  more than PcdMaximumUnicodeStringLength Unicode characters not including
  the Null-terminator, then ASSERT().

  If PcdMaximumAsciiStringLength is not zero, and Source contains more
  than PcdMaximumAsciiStringLength Unicode characters not including the
  Null-terminator, then ASSERT().

  @param  Source        The pointer to a Null-terminated Unicode string.
  @param  Destination   The pointer to a Null-terminated ASCII string.

  @return Destination.

**/
CHAR8 *
EFIAPI
UnicodeStrToAsciiStr (
  IN      CONST CHAR16              *Source,
  OUT     CHAR8                     *Destination
  );

#endif // !defined (DISABLE_NEW_DEPRECATED_INTERFACES)

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
  IN      CONST CHAR16              *Source,
  OUT     CHAR8                     *Destination,
  IN      UINTN                     DestMax
  );

/**
  Convert not more than Length successive characters from a Null-terminated
  Unicode string to a Null-terminated Ascii string. If no null char is copied
  from Source, then Destination[Length] is always set to null.

  This function converts not more than Length successive characters from the
  Unicode string Source to the Ascii string Destination by copying the lower 8
  bits of each Unicode character. The function terminates the Ascii string
  Destination by appending a Null-terminator character at the end.

  The caller is responsible to make sure Destination points to a buffer with size
  equal or greater than ((StrLen (Source) + 1) * sizeof (CHAR8)) in bytes.

  If any Unicode characters in Source contain non-zero value in the upper 8
  bits, then ASSERT().
  If Source is not aligned on a 16-bit boundary, then ASSERT().

  If an error is returned, then the Destination is unmodified.

  @param  Source             The pointer to a Null-terminated Unicode string.
  @param  Length             The maximum number of Unicode characters to
                             convert.
  @param  Destination        The pointer to a Null-terminated Ascii string.
  @param  DestMax            The maximum number of Destination Ascii
                             char, including terminating null char.
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
  IN      CONST CHAR16              *Source,
  IN      UINTN                     Length,
  OUT     CHAR8                     *Destination,
  IN      UINTN                     DestMax,
  OUT     UINTN                     *DestinationLength
  );

#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function is deprecated for security reason.

  Copies one Null-terminated ASCII string to another Null-terminated ASCII
  string and returns the new ASCII string.

  This function copies the contents of the ASCII string Source to the ASCII
  string Destination, and returns Destination. If Source and Destination
  overlap, then the results are undefined.

  If Destination is NULL, then ASSERT().
  If Source is NULL, then ASSERT().
  If Source and Destination overlap, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and Source contains more than
  PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator,
  then ASSERT().

  @param  Destination The pointer to a Null-terminated ASCII string.
  @param  Source      The pointer to a Null-terminated ASCII string.

  @return Destination

**/
CHAR8 *
EFIAPI
AsciiStrCpy (
  OUT     CHAR8                     *Destination,
  IN      CONST CHAR8               *Source
  );


/**
  [ATTENTION] This function is deprecated for security reason.

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

  @param  Destination The pointer to a Null-terminated ASCII string.
  @param  Source      The pointer to a Null-terminated ASCII string.
  @param  Length      The maximum number of ASCII characters to copy.

  @return Destination

**/
CHAR8 *
EFIAPI
AsciiStrnCpy (
  OUT     CHAR8                     *Destination,
  IN      CONST CHAR8               *Source,
  IN      UINTN                     Length
  );
#endif // !defined (DISABLE_NEW_DEPRECATED_INTERFACES)

/**
  Returns the length of a Null-terminated ASCII string.

  This function returns the number of ASCII characters in the Null-terminated
  ASCII string specified by String.

  If Length > 0 and Destination is NULL, then ASSERT().
  If Length > 0 and Source is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and String contains more than
  PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator,
  then ASSERT().

  @param  String  The pointer to a Null-terminated ASCII string.

  @return The length of String.

**/
UINTN
EFIAPI
AsciiStrLen (
  IN      CONST CHAR8               *String
  );


/**
  Returns the size of a Null-terminated ASCII string in bytes, including the
  Null terminator.

  This function returns the size, in bytes, of the Null-terminated ASCII string
  specified by String.

  If String is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and String contains more than
  PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator,
  then ASSERT().

  @param  String  The pointer to a Null-terminated ASCII string.

  @return The size of String.

**/
UINTN
EFIAPI
AsciiStrSize (
  IN      CONST CHAR8               *String
  );


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
  PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator,
  then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and SecondString contains more
  than PcdMaximumAsciiStringLength ASCII characters not including the
  Null-terminator, then ASSERT().

  @param  FirstString   The pointer to a Null-terminated ASCII string.
  @param  SecondString  The pointer to a Null-terminated ASCII string.

  @retval ==0      FirstString is identical to SecondString.
  @retval !=0      FirstString is not identical to SecondString.

**/
INTN
EFIAPI
AsciiStrCmp (
  IN      CONST CHAR8               *FirstString,
  IN      CONST CHAR8               *SecondString
  );


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
  PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator,
  then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and SecondString contains more
  than PcdMaximumAsciiStringLength ASCII characters not including the
  Null-terminator, then ASSERT().

  @param  FirstString   The pointer to a Null-terminated ASCII string.
  @param  SecondString  The pointer to a Null-terminated ASCII string.

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
  );


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

  @param  FirstString   The pointer to a Null-terminated ASCII string.
  @param  SecondString  The pointer to a Null-terminated ASCII string.
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
  );


#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function is deprecated for security reason.

  Concatenates one Null-terminated ASCII string to another Null-terminated
  ASCII string, and returns the concatenated ASCII string.

  This function concatenates two Null-terminated ASCII strings. The contents of
  Null-terminated ASCII string Source are concatenated to the end of Null-
  terminated ASCII string Destination. The Null-terminated concatenated ASCII
  String is returned.

  If Destination is NULL, then ASSERT().
  If Source is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and Destination contains more than
  PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator,
  then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and Source contains more than
  PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator,
  then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and concatenating Destination and
  Source results in a ASCII string with more than PcdMaximumAsciiStringLength
  ASCII characters, then ASSERT().

  @param  Destination The pointer to a Null-terminated ASCII string.
  @param  Source      The pointer to a Null-terminated ASCII string.

  @return Destination

**/
CHAR8 *
EFIAPI
AsciiStrCat (
  IN OUT CHAR8    *Destination,
  IN CONST CHAR8  *Source
  );


/**
  [ATTENTION] This function is deprecated for security reason.

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

  @param  Destination The pointer to a Null-terminated ASCII string.
  @param  Source      The pointer to a Null-terminated ASCII string.
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
  );
#endif // !defined (DISABLE_NEW_DEPRECATED_INTERFACES)

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

  @param  String          The pointer to a Null-terminated ASCII string.
  @param  SearchString    The pointer to a Null-terminated ASCII string to search for.

  @retval NULL            If the SearchString does not appear in String.
  @retval others          If there is a match return the first occurrence of SearchingString.
                          If the length of SearchString is zero,return String.

**/
CHAR8 *
EFIAPI
AsciiStrStr (
  IN      CONST CHAR8               *String,
  IN      CONST CHAR8               *SearchString
  );


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

  @param  String          The pointer to a Null-terminated ASCII string.

  @retval The value translated from String.

**/
UINTN
EFIAPI
AsciiStrDecimalToUintn (
  IN      CONST CHAR8               *String
  );


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

  @param  String          The pointer to a Null-terminated ASCII string.

  @retval Value translated from String.

**/
UINT64
EFIAPI
AsciiStrDecimalToUint64 (
  IN      CONST CHAR8               *String
  );


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

  @param  String          The pointer to a Null-terminated ASCII string.

  @retval Value translated from String.

**/
UINTN
EFIAPI
AsciiStrHexToUintn (
  IN      CONST CHAR8               *String
  );


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

  @param  String          The pointer to a Null-terminated ASCII string.

  @retval Value translated from String.

**/
UINT64
EFIAPI
AsciiStrHexToUint64 (
  IN      CONST CHAR8                *String
  );

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
  IN  CONST CHAR8        *String,
  OUT CHAR8              **EndPointer, OPTIONAL
  OUT IPv6_ADDRESS       *Address,
  OUT UINT8              *PrefixLength OPTIONAL
  );

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
  IN  CONST CHAR8        *String,
  OUT CHAR8              **EndPointer, OPTIONAL
  OUT IPv4_ADDRESS       *Address,
  OUT UINT8              *PrefixLength OPTIONAL
  );

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
  IN  CONST CHAR8        *String,
  OUT GUID               *Guid
  );

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
  IN  CONST CHAR8        *String,
  IN  UINTN              Length,
  OUT UINT8              *Buffer,
  IN  UINTN              MaxBufferSize
  );

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

  @param  Source        The pointer to a Null-terminated ASCII string.
  @param  Destination   The pointer to a Null-terminated Unicode string.

  @return Destination.

**/
CHAR16 *
EFIAPI
AsciiStrToUnicodeStr (
  IN      CONST CHAR8               *Source,
  OUT     CHAR16                    *Destination
  );

#endif // !defined (DISABLE_NEW_DEPRECATED_INTERFACES)

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
  IN      CONST CHAR8               *Source,
  OUT     CHAR16                    *Destination,
  IN      UINTN                     DestMax
  );

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
  IN      CONST CHAR8               *Source,
  IN      UINTN                     Length,
  OUT     CHAR16                    *Destination,
  IN      UINTN                     DestMax,
  OUT     UINTN                     *DestinationLength
  );

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
CharToUpper (
  IN      CHAR16                    Char
  );

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
AsciiCharToUpper (
  IN      CHAR8                     Chr
  );

/**
  Convert binary data to a Base64 encoded ascii string based on RFC4648.

  Produce a Null-terminated Ascii string in the output buffer specified by Destination and DestinationSize.
  The Ascii string is produced by converting the data string specified by Source and SourceLength.

  @param Source           Input UINT8 data
  @param SourceLength     Number of UINT8 bytes of data
  @param Destination      Pointer to output string buffer
  @param DestinationSize  Size of ascii buffer. Set to 0 to get the size needed.
                          Caller is responsible for passing in buffer of DestinationSize

  @retval RETURN_SUCCESS             When ascii buffer is filled in.
  @retval RETURN_INVALID_PARAMETER   If Source is NULL or DestinationSize is NULL.
  @retval RETURN_INVALID_PARAMETER   If SourceLength or DestinationSize is bigger than (MAX_ADDRESS - (UINTN)Destination).
  @retval RETURN_BUFFER_TOO_SMALL    If SourceLength is 0 and DestinationSize is <1.
  @retval RETURN_BUFFER_TOO_SMALL    If Destination is NULL or DestinationSize is smaller than required buffersize.

**/
RETURN_STATUS
EFIAPI
Base64Encode (
  IN  CONST UINT8  *Source,
  IN        UINTN   SourceLength,
  OUT       CHAR8  *Destination  OPTIONAL,
  IN OUT    UINTN  *DestinationSize
  );

/**
  Decode Base64 ASCII encoded data to 8-bit binary representation, based on
  RFC4648.

  Decoding occurs according to "Table 1: The Base 64 Alphabet" in RFC4648.

  Whitespace is ignored at all positions:
  - 0x09 ('\t') horizontal tab
  - 0x0A ('\n') new line
  - 0x0B ('\v') vertical tab
  - 0x0C ('\f') form feed
  - 0x0D ('\r') carriage return
  - 0x20 (' ')  space

  The minimum amount of required padding (with ASCII 0x3D, '=') is tolerated
  and enforced at the end of the Base64 ASCII encoded data, and only there.

  Other characters outside of the encoding alphabet cause the function to
  reject the Base64 ASCII encoded data.

  @param[in] Source               Array of CHAR8 elements containing the Base64
                                  ASCII encoding. May be NULL if SourceSize is
                                  zero.

  @param[in] SourceSize           Number of CHAR8 elements in Source.

  @param[out] Destination         Array of UINT8 elements receiving the decoded
                                  8-bit binary representation. Allocated by the
                                  caller. May be NULL if DestinationSize is
                                  zero on input. If NULL, decoding is
                                  performed, but the 8-bit binary
                                  representation is not stored. If non-NULL and
                                  the function returns an error, the contents
                                  of Destination are indeterminate.

  @param[in,out] DestinationSize  On input, the number of UINT8 elements that
                                  the caller allocated for Destination. On
                                  output, if the function returns
                                  RETURN_SUCCESS or RETURN_BUFFER_TOO_SMALL,
                                  the number of UINT8 elements that are
                                  required for decoding the Base64 ASCII
                                  representation. If the function returns a
                                  value different from both RETURN_SUCCESS and
                                  RETURN_BUFFER_TOO_SMALL, then DestinationSize
                                  is indeterminate on output.

  @retval RETURN_SUCCESS            SourceSize CHAR8 elements at Source have
                                    been decoded to on-output DestinationSize
                                    UINT8 elements at Destination. Note that
                                    RETURN_SUCCESS covers the case when
                                    DestinationSize is zero on input, and
                                    Source decodes to zero bytes (due to
                                    containing at most ignored whitespace).

  @retval RETURN_BUFFER_TOO_SMALL   The input value of DestinationSize is not
                                    large enough for decoding SourceSize CHAR8
                                    elements at Source. The required number of
                                    UINT8 elements has been stored to
                                    DestinationSize.

  @retval RETURN_INVALID_PARAMETER  DestinationSize is NULL.

  @retval RETURN_INVALID_PARAMETER  Source is NULL, but SourceSize is not zero.

  @retval RETURN_INVALID_PARAMETER  Destination is NULL, but DestinationSize is
                                    not zero on input.

  @retval RETURN_INVALID_PARAMETER  Source is non-NULL, and (Source +
                                    SourceSize) would wrap around MAX_ADDRESS.

  @retval RETURN_INVALID_PARAMETER  Destination is non-NULL, and (Destination +
                                    DestinationSize) would wrap around
                                    MAX_ADDRESS, as specified on input.

  @retval RETURN_INVALID_PARAMETER  None of Source and Destination are NULL,
                                    and CHAR8[SourceSize] at Source overlaps
                                    UINT8[DestinationSize] at Destination, as
                                    specified on input.

  @retval RETURN_INVALID_PARAMETER  Invalid CHAR8 element encountered in
                                    Source.
**/
RETURN_STATUS
EFIAPI
Base64Decode (
  IN     CONST CHAR8 *Source          OPTIONAL,
  IN     UINTN       SourceSize,
  OUT    UINT8       *Destination     OPTIONAL,
  IN OUT UINTN       *DestinationSize
  );

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
  );


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
  );

//
//  File Path Manipulation Functions
//

/**
  Removes the last directory or file entry in a path.

  @param[in, out] Path    The pointer to the path to modify.

  @retval FALSE     Nothing was found to remove.
  @retval TRUE      A directory or file was removed.
**/
BOOLEAN
EFIAPI
PathRemoveLastItem(
  IN OUT CHAR16 *Path
  );

/**
  Function to clean up paths.
    - Single periods in the path are removed.
    - Double periods in the path are removed along with a single parent directory.
    - Forward slashes L'/' are converted to backward slashes L'\'.

  This will be done inline and the existing buffer may be larger than required
  upon completion.

  @param[in] Path       The pointer to the string containing the path.

  @return       Returns Path, otherwise returns NULL to indicate that an error has occurred.
**/
CHAR16*
EFIAPI
PathCleanUpDirectories(
  IN CHAR16 *Path
  );

//
// Linked List Functions and Macros
//

/**
  Initializes the head node of a doubly linked list that is declared as a
  global variable in a module.

  Initializes the forward and backward links of a new linked list. After
  initializing a linked list with this macro, the other linked list functions
  may be used to add and remove nodes from the linked list. This macro results
  in smaller executables by initializing the linked list in the data section,
  instead if calling the InitializeListHead() function to perform the
  equivalent operation.

  @param  ListHead  The head note of a list to initialize.

**/
#define INITIALIZE_LIST_HEAD_VARIABLE(ListHead)  {&(ListHead), &(ListHead)}

/**
  Iterates over each node in a doubly linked list using each node's forward link.

  @param  Entry     A pointer to a list node used as a loop cursor during iteration
  @param  ListHead  The head node of the doubly linked list

**/
#define BASE_LIST_FOR_EACH(Entry, ListHead)    \
  for(Entry = (ListHead)->ForwardLink; Entry != (ListHead); Entry = Entry->ForwardLink)

/**
  Iterates over each node in a doubly linked list using each node's forward link
  with safety against node removal.

  This macro uses NextEntry to temporarily store the next list node so the node
  pointed to by Entry may be deleted in the current loop iteration step and
  iteration can continue from the node pointed to by NextEntry.

  @param  Entry     A pointer to a list node used as a loop cursor during iteration
  @param  NextEntry A pointer to a list node used to temporarily store the next node
  @param  ListHead  The head node of the doubly linked list

**/
#define BASE_LIST_FOR_EACH_SAFE(Entry, NextEntry, ListHead)            \
  for(Entry = (ListHead)->ForwardLink, NextEntry = Entry->ForwardLink;\
      Entry != (ListHead); Entry = NextEntry, NextEntry = Entry->ForwardLink)

/**
  Checks whether FirstEntry and SecondEntry are part of the same doubly-linked
  list.

  If FirstEntry is NULL, then ASSERT().
  If FirstEntry->ForwardLink is NULL, then ASSERT().
  If FirstEntry->BackLink is NULL, then ASSERT().
  If SecondEntry is NULL, then ASSERT();
  If PcdMaximumLinkedListLength is not zero, and List contains more than
  PcdMaximumLinkedListLength nodes, then ASSERT().

  @param  FirstEntry   A pointer to a node in a linked list.
  @param  SecondEntry  A pointer to the node to locate.

  @retval TRUE   SecondEntry is in the same doubly-linked list as FirstEntry.
  @retval FALSE  SecondEntry isn't in the same doubly-linked list as FirstEntry,
                 or FirstEntry is invalid.

**/
BOOLEAN
EFIAPI
IsNodeInList (
  IN      CONST LIST_ENTRY      *FirstEntry,
  IN      CONST LIST_ENTRY      *SecondEntry
  );


/**
  Initializes the head node of a doubly linked list, and returns the pointer to
  the head node of the doubly linked list.

  Initializes the forward and backward links of a new linked list. After
  initializing a linked list with this function, the other linked list
  functions may be used to add and remove nodes from the linked list. It is up
  to the caller of this function to allocate the memory for ListHead.

  If ListHead is NULL, then ASSERT().

  @param  ListHead  A pointer to the head node of a new doubly linked list.

  @return ListHead

**/
LIST_ENTRY *
EFIAPI
InitializeListHead (
  IN OUT  LIST_ENTRY                *ListHead
  );


/**
  Adds a node to the beginning of a doubly linked list, and returns the pointer
  to the head node of the doubly linked list.

  Adds the node Entry at the beginning of the doubly linked list denoted by
  ListHead, and returns ListHead.

  If ListHead is NULL, then ASSERT().
  If Entry is NULL, then ASSERT().
  If ListHead was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and prior to insertion the number
  of nodes in ListHead, including the ListHead node, is greater than or
  equal to PcdMaximumLinkedListLength, then ASSERT().

  @param  ListHead  A pointer to the head node of a doubly linked list.
  @param  Entry     A pointer to a node that is to be inserted at the beginning
                    of a doubly linked list.

  @return ListHead

**/
LIST_ENTRY *
EFIAPI
InsertHeadList (
  IN OUT  LIST_ENTRY                *ListHead,
  IN OUT  LIST_ENTRY                *Entry
  );


/**
  Adds a node to the end of a doubly linked list, and returns the pointer to
  the head node of the doubly linked list.

  Adds the node Entry to the end of the doubly linked list denoted by ListHead,
  and returns ListHead.

  If ListHead is NULL, then ASSERT().
  If Entry is NULL, then ASSERT().
  If ListHead was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and prior to insertion the number
  of nodes in ListHead, including the ListHead node, is greater than or
  equal to PcdMaximumLinkedListLength, then ASSERT().

  @param  ListHead  A pointer to the head node of a doubly linked list.
  @param  Entry     A pointer to a node that is to be added at the end of the
                    doubly linked list.

  @return ListHead

**/
LIST_ENTRY *
EFIAPI
InsertTailList (
  IN OUT  LIST_ENTRY                *ListHead,
  IN OUT  LIST_ENTRY                *Entry
  );


/**
  Retrieves the first node of a doubly linked list.

  Returns the first node of a doubly linked list.  List must have been
  initialized with INTIALIZE_LIST_HEAD_VARIABLE() or InitializeListHead().
  If List is empty, then List is returned.

  If List is NULL, then ASSERT().
  If List was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes
  in List, including the List node, is greater than or equal to
  PcdMaximumLinkedListLength, then ASSERT().

  @param  List  A pointer to the head node of a doubly linked list.

  @return The first node of a doubly linked list.
  @retval List  The list is empty.

**/
LIST_ENTRY *
EFIAPI
GetFirstNode (
  IN      CONST LIST_ENTRY          *List
  );


/**
  Retrieves the next node of a doubly linked list.

  Returns the node of a doubly linked list that follows Node.
  List must have been initialized with INTIALIZE_LIST_HEAD_VARIABLE()
  or InitializeListHead().  If List is empty, then List is returned.

  If List is NULL, then ASSERT().
  If Node is NULL, then ASSERT().
  If List was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and List contains more than
  PcdMaximumLinkedListLength nodes, then ASSERT().
  If PcdVerifyNodeInList is TRUE and Node is not a node in List, then ASSERT().

  @param  List  A pointer to the head node of a doubly linked list.
  @param  Node  A pointer to a node in the doubly linked list.

  @return The pointer to the next node if one exists. Otherwise List is returned.

**/
LIST_ENTRY *
EFIAPI
GetNextNode (
  IN      CONST LIST_ENTRY          *List,
  IN      CONST LIST_ENTRY          *Node
  );


/**
  Retrieves the previous node of a doubly linked list.

  Returns the node of a doubly linked list that precedes Node.
  List must have been initialized with INTIALIZE_LIST_HEAD_VARIABLE()
  or InitializeListHead().  If List is empty, then List is returned.

  If List is NULL, then ASSERT().
  If Node is NULL, then ASSERT().
  If List was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and List contains more than
  PcdMaximumLinkedListLength nodes, then ASSERT().
  If PcdVerifyNodeInList is TRUE and Node is not a node in List, then ASSERT().

  @param  List  A pointer to the head node of a doubly linked list.
  @param  Node  A pointer to a node in the doubly linked list.

  @return The pointer to the previous node if one exists. Otherwise List is returned.

**/
LIST_ENTRY *
EFIAPI
GetPreviousNode (
  IN      CONST LIST_ENTRY          *List,
  IN      CONST LIST_ENTRY          *Node
  );


/**
  Checks to see if a doubly linked list is empty or not.

  Checks to see if the doubly linked list is empty. If the linked list contains
  zero nodes, this function returns TRUE. Otherwise, it returns FALSE.

  If ListHead is NULL, then ASSERT().
  If ListHead was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes
  in List, including the List node, is greater than or equal to
  PcdMaximumLinkedListLength, then ASSERT().

  @param  ListHead  A pointer to the head node of a doubly linked list.

  @retval TRUE  The linked list is empty.
  @retval FALSE The linked list is not empty.

**/
BOOLEAN
EFIAPI
IsListEmpty (
  IN      CONST LIST_ENTRY          *ListHead
  );


/**
  Determines if a node in a doubly linked list is the head node of a the same
  doubly linked list.  This function is typically used to terminate a loop that
  traverses all the nodes in a doubly linked list starting with the head node.

  Returns TRUE if Node is equal to List.  Returns FALSE if Node is one of the
  nodes in the doubly linked list specified by List.  List must have been
  initialized with INTIALIZE_LIST_HEAD_VARIABLE() or InitializeListHead().

  If List is NULL, then ASSERT().
  If Node is NULL, then ASSERT().
  If List was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or InitializeListHead(),
  then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes
  in List, including the List node, is greater than or equal to
  PcdMaximumLinkedListLength, then ASSERT().
  If PcdVerifyNodeInList is TRUE and Node is not a node in List the and Node is not equal
  to List, then ASSERT().

  @param  List  A pointer to the head node of a doubly linked list.
  @param  Node  A pointer to a node in the doubly linked list.

  @retval TRUE  Node is the head of the doubly-linked list pointed by List.
  @retval FALSE Node is not the head of the doubly-linked list pointed by List.

**/
BOOLEAN
EFIAPI
IsNull (
  IN      CONST LIST_ENTRY          *List,
  IN      CONST LIST_ENTRY          *Node
  );


/**
  Determines if a node the last node in a doubly linked list.

  Returns TRUE if Node is the last node in the doubly linked list specified by
  List. Otherwise, FALSE is returned. List must have been initialized with
  INTIALIZE_LIST_HEAD_VARIABLE() or InitializeListHead().

  If List is NULL, then ASSERT().
  If Node is NULL, then ASSERT().
  If List was not initialized with INTIALIZE_LIST_HEAD_VARIABLE() or
  InitializeListHead(), then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes
  in List, including the List node, is greater than or equal to
  PcdMaximumLinkedListLength, then ASSERT().
  If PcdVerifyNodeInList is TRUE and Node is not a node in List, then ASSERT().

  @param  List  A pointer to the head node of a doubly linked list.
  @param  Node  A pointer to a node in the doubly linked list.

  @retval TRUE  Node is the last node in the linked list.
  @retval FALSE Node is not the last node in the linked list.

**/
BOOLEAN
EFIAPI
IsNodeAtEnd (
  IN      CONST LIST_ENTRY          *List,
  IN      CONST LIST_ENTRY          *Node
  );


/**
  Swaps the location of two nodes in a doubly linked list, and returns the
  first node after the swap.

  If FirstEntry is identical to SecondEntry, then SecondEntry is returned.
  Otherwise, the location of the FirstEntry node is swapped with the location
  of the SecondEntry node in a doubly linked list. SecondEntry must be in the
  same double linked list as FirstEntry and that double linked list must have
  been initialized with INTIALIZE_LIST_HEAD_VARIABLE() or InitializeListHead().
  SecondEntry is returned after the nodes are swapped.

  If FirstEntry is NULL, then ASSERT().
  If SecondEntry is NULL, then ASSERT().
  If PcdVerifyNodeInList is TRUE and SecondEntry and FirstEntry are not in the
  same linked list, then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes in the
  linked list containing the FirstEntry and SecondEntry nodes, including
  the FirstEntry and SecondEntry nodes, is greater than or equal to
  PcdMaximumLinkedListLength, then ASSERT().

  @param  FirstEntry  A pointer to a node in a linked list.
  @param  SecondEntry A pointer to another node in the same linked list.

  @return SecondEntry.

**/
LIST_ENTRY *
EFIAPI
SwapListEntries (
  IN OUT  LIST_ENTRY                *FirstEntry,
  IN OUT  LIST_ENTRY                *SecondEntry
  );


/**
  Removes a node from a doubly linked list, and returns the node that follows
  the removed node.

  Removes the node Entry from a doubly linked list. It is up to the caller of
  this function to release the memory used by this node if that is required. On
  exit, the node following Entry in the doubly linked list is returned. If
  Entry is the only node in the linked list, then the head node of the linked
  list is returned.

  If Entry is NULL, then ASSERT().
  If Entry is the head node of an empty list, then ASSERT().
  If PcdMaximumLinkedListLength is not zero, and the number of nodes in the
  linked list containing Entry, including the Entry node, is greater than
  or equal to PcdMaximumLinkedListLength, then ASSERT().

  @param  Entry A pointer to a node in a linked list.

  @return Entry.

**/
LIST_ENTRY *
EFIAPI
RemoveEntryList (
  IN      CONST LIST_ENTRY          *Entry
  );

//
// Math Services
//

/**
  Shifts a 64-bit integer left between 0 and 63 bits. The low bits are filled
  with zeros. The shifted value is returned.

  This function shifts the 64-bit value Operand to the left by Count bits. The
  low Count bits are set to zero. The shifted value is returned.

  If Count is greater than 63, then ASSERT().

  @param  Operand The 64-bit operand to shift left.
  @param  Count   The number of bits to shift left.

  @return Operand << Count.

**/
UINT64
EFIAPI
LShiftU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  );


/**
  Shifts a 64-bit integer right between 0 and 63 bits. This high bits are
  filled with zeros. The shifted value is returned.

  This function shifts the 64-bit value Operand to the right by Count bits. The
  high Count bits are set to zero. The shifted value is returned.

  If Count is greater than 63, then ASSERT().

  @param  Operand The 64-bit operand to shift right.
  @param  Count   The number of bits to shift right.

  @return Operand >> Count

**/
UINT64
EFIAPI
RShiftU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  );


/**
  Shifts a 64-bit integer right between 0 and 63 bits. The high bits are filled
  with original integer's bit 63. The shifted value is returned.

  This function shifts the 64-bit value Operand to the right by Count bits. The
  high Count bits are set to bit 63 of Operand.  The shifted value is returned.

  If Count is greater than 63, then ASSERT().

  @param  Operand The 64-bit operand to shift right.
  @param  Count   The number of bits to shift right.

  @return Operand >> Count

**/
UINT64
EFIAPI
ARShiftU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  );


/**
  Rotates a 32-bit integer left between 0 and 31 bits, filling the low bits
  with the high bits that were rotated.

  This function rotates the 32-bit value Operand to the left by Count bits. The
  low Count bits are fill with the high Count bits of Operand. The rotated
  value is returned.

  If Count is greater than 31, then ASSERT().

  @param  Operand The 32-bit operand to rotate left.
  @param  Count   The number of bits to rotate left.

  @return Operand << Count

**/
UINT32
EFIAPI
LRotU32 (
  IN      UINT32                    Operand,
  IN      UINTN                     Count
  );


/**
  Rotates a 32-bit integer right between 0 and 31 bits, filling the high bits
  with the low bits that were rotated.

  This function rotates the 32-bit value Operand to the right by Count bits.
  The high Count bits are fill with the low Count bits of Operand. The rotated
  value is returned.

  If Count is greater than 31, then ASSERT().

  @param  Operand The 32-bit operand to rotate right.
  @param  Count   The number of bits to rotate right.

  @return Operand >> Count

**/
UINT32
EFIAPI
RRotU32 (
  IN      UINT32                    Operand,
  IN      UINTN                     Count
  );


/**
  Rotates a 64-bit integer left between 0 and 63 bits, filling the low bits
  with the high bits that were rotated.

  This function rotates the 64-bit value Operand to the left by Count bits. The
  low Count bits are fill with the high Count bits of Operand. The rotated
  value is returned.

  If Count is greater than 63, then ASSERT().

  @param  Operand The 64-bit operand to rotate left.
  @param  Count   The number of bits to rotate left.

  @return Operand << Count

**/
UINT64
EFIAPI
LRotU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  );


/**
  Rotates a 64-bit integer right between 0 and 63 bits, filling the high bits
  with the high low bits that were rotated.

  This function rotates the 64-bit value Operand to the right by Count bits.
  The high Count bits are fill with the low Count bits of Operand. The rotated
  value is returned.

  If Count is greater than 63, then ASSERT().

  @param  Operand The 64-bit operand to rotate right.
  @param  Count   The number of bits to rotate right.

  @return Operand >> Count

**/
UINT64
EFIAPI
RRotU64 (
  IN      UINT64                    Operand,
  IN      UINTN                     Count
  );


/**
  Returns the bit position of the lowest bit set in a 32-bit value.

  This function computes the bit position of the lowest bit set in the 32-bit
  value specified by Operand. If Operand is zero, then -1 is returned.
  Otherwise, a value between 0 and 31 is returned.

  @param  Operand The 32-bit operand to evaluate.

  @retval 0..31  The lowest bit set in Operand was found.
  @retval -1    Operand is zero.

**/
INTN
EFIAPI
LowBitSet32 (
  IN      UINT32                    Operand
  );


/**
  Returns the bit position of the lowest bit set in a 64-bit value.

  This function computes the bit position of the lowest bit set in the 64-bit
  value specified by Operand. If Operand is zero, then -1 is returned.
  Otherwise, a value between 0 and 63 is returned.

  @param  Operand The 64-bit operand to evaluate.

  @retval 0..63  The lowest bit set in Operand was found.
  @retval -1    Operand is zero.


**/
INTN
EFIAPI
LowBitSet64 (
  IN      UINT64                    Operand
  );


/**
  Returns the bit position of the highest bit set in a 32-bit value. Equivalent
  to log2(x).

  This function computes the bit position of the highest bit set in the 32-bit
  value specified by Operand. If Operand is zero, then -1 is returned.
  Otherwise, a value between 0 and 31 is returned.

  @param  Operand The 32-bit operand to evaluate.

  @retval 0..31  Position of the highest bit set in Operand if found.
  @retval -1    Operand is zero.

**/
INTN
EFIAPI
HighBitSet32 (
  IN      UINT32                    Operand
  );


/**
  Returns the bit position of the highest bit set in a 64-bit value. Equivalent
  to log2(x).

  This function computes the bit position of the highest bit set in the 64-bit
  value specified by Operand. If Operand is zero, then -1 is returned.
  Otherwise, a value between 0 and 63 is returned.

  @param  Operand The 64-bit operand to evaluate.

  @retval 0..63   Position of the highest bit set in Operand if found.
  @retval -1     Operand is zero.

**/
INTN
EFIAPI
HighBitSet64 (
  IN      UINT64                    Operand
  );


/**
  Returns the value of the highest bit set in a 32-bit value. Equivalent to
  1 << log2(x).

  This function computes the value of the highest bit set in the 32-bit value
  specified by Operand. If Operand is zero, then zero is returned.

  @param  Operand The 32-bit operand to evaluate.

  @return 1 << HighBitSet32(Operand)
  @retval 0 Operand is zero.

**/
UINT32
EFIAPI
GetPowerOfTwo32 (
  IN      UINT32                    Operand
  );


/**
  Returns the value of the highest bit set in a 64-bit value. Equivalent to
  1 << log2(x).

  This function computes the value of the highest bit set in the 64-bit value
  specified by Operand. If Operand is zero, then zero is returned.

  @param  Operand The 64-bit operand to evaluate.

  @return 1 << HighBitSet64(Operand)
  @retval 0 Operand is zero.

**/
UINT64
EFIAPI
GetPowerOfTwo64 (
  IN      UINT64                    Operand
  );


/**
  Switches the endianness of a 16-bit integer.

  This function swaps the bytes in a 16-bit unsigned value to switch the value
  from little endian to big endian or vice versa. The byte swapped value is
  returned.

  @param  Value A 16-bit unsigned value.

  @return The byte swapped Value.

**/
UINT16
EFIAPI
SwapBytes16 (
  IN      UINT16                    Value
  );


/**
  Switches the endianness of a 32-bit integer.

  This function swaps the bytes in a 32-bit unsigned value to switch the value
  from little endian to big endian or vice versa. The byte swapped value is
  returned.

  @param  Value A 32-bit unsigned value.

  @return The byte swapped Value.

**/
UINT32
EFIAPI
SwapBytes32 (
  IN      UINT32                    Value
  );


/**
  Switches the endianness of a 64-bit integer.

  This function swaps the bytes in a 64-bit unsigned value to switch the value
  from little endian to big endian or vice versa. The byte swapped value is
  returned.

  @param  Value A 64-bit unsigned value.

  @return The byte swapped Value.

**/
UINT64
EFIAPI
SwapBytes64 (
  IN      UINT64                    Value
  );


/**
  Multiples a 64-bit unsigned integer by a 32-bit unsigned integer and
  generates a 64-bit unsigned result.

  This function multiples the 64-bit unsigned value Multiplicand by the 32-bit
  unsigned value Multiplier and generates a 64-bit unsigned result. This 64-
  bit unsigned result is returned.

  @param  Multiplicand  A 64-bit unsigned value.
  @param  Multiplier    A 32-bit unsigned value.

  @return Multiplicand * Multiplier

**/
UINT64
EFIAPI
MultU64x32 (
  IN      UINT64                    Multiplicand,
  IN      UINT32                    Multiplier
  );


/**
  Multiples a 64-bit unsigned integer by a 64-bit unsigned integer and
  generates a 64-bit unsigned result.

  This function multiples the 64-bit unsigned value Multiplicand by the 64-bit
  unsigned value Multiplier and generates a 64-bit unsigned result. This 64-
  bit unsigned result is returned.

  @param  Multiplicand  A 64-bit unsigned value.
  @param  Multiplier    A 64-bit unsigned value.

  @return Multiplicand * Multiplier.

**/
UINT64
EFIAPI
MultU64x64 (
  IN      UINT64                    Multiplicand,
  IN      UINT64                    Multiplier
  );


/**
  Multiples a 64-bit signed integer by a 64-bit signed integer and generates a
  64-bit signed result.

  This function multiples the 64-bit signed value Multiplicand by the 64-bit
  signed value Multiplier and generates a 64-bit signed result. This 64-bit
  signed result is returned.

  @param  Multiplicand  A 64-bit signed value.
  @param  Multiplier    A 64-bit signed value.

  @return Multiplicand * Multiplier

**/
INT64
EFIAPI
MultS64x64 (
  IN      INT64                     Multiplicand,
  IN      INT64                     Multiplier
  );


/**
  Divides a 64-bit unsigned integer by a 32-bit unsigned integer and generates
  a 64-bit unsigned result.

  This function divides the 64-bit unsigned value Dividend by the 32-bit
  unsigned value Divisor and generates a 64-bit unsigned quotient. This
  function returns the 64-bit unsigned quotient.

  If Divisor is 0, then ASSERT().

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 32-bit unsigned value.

  @return Dividend / Divisor.

**/
UINT64
EFIAPI
DivU64x32 (
  IN      UINT64                    Dividend,
  IN      UINT32                    Divisor
  );


/**
  Divides a 64-bit unsigned integer by a 32-bit unsigned integer and generates
  a 32-bit unsigned remainder.

  This function divides the 64-bit unsigned value Dividend by the 32-bit
  unsigned value Divisor and generates a 32-bit remainder. This function
  returns the 32-bit unsigned remainder.

  If Divisor is 0, then ASSERT().

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 32-bit unsigned value.

  @return Dividend % Divisor.

**/
UINT32
EFIAPI
ModU64x32 (
  IN      UINT64                    Dividend,
  IN      UINT32                    Divisor
  );


/**
  Divides a 64-bit unsigned integer by a 32-bit unsigned integer and generates
  a 64-bit unsigned result and an optional 32-bit unsigned remainder.

  This function divides the 64-bit unsigned value Dividend by the 32-bit
  unsigned value Divisor and generates a 64-bit unsigned quotient. If Remainder
  is not NULL, then the 32-bit unsigned remainder is returned in Remainder.
  This function returns the 64-bit unsigned quotient.

  If Divisor is 0, then ASSERT().

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 32-bit unsigned value.
  @param  Remainder A pointer to a 32-bit unsigned value. This parameter is
                    optional and may be NULL.

  @return Dividend / Divisor.

**/
UINT64
EFIAPI
DivU64x32Remainder (
  IN      UINT64                    Dividend,
  IN      UINT32                    Divisor,
  OUT     UINT32                    *Remainder  OPTIONAL
  );


/**
  Divides a 64-bit unsigned integer by a 64-bit unsigned integer and generates
  a 64-bit unsigned result and an optional 64-bit unsigned remainder.

  This function divides the 64-bit unsigned value Dividend by the 64-bit
  unsigned value Divisor and generates a 64-bit unsigned quotient. If Remainder
  is not NULL, then the 64-bit unsigned remainder is returned in Remainder.
  This function returns the 64-bit unsigned quotient.

  If Divisor is 0, then ASSERT().

  @param  Dividend  A 64-bit unsigned value.
  @param  Divisor   A 64-bit unsigned value.
  @param  Remainder A pointer to a 64-bit unsigned value. This parameter is
                    optional and may be NULL.

  @return Dividend / Divisor.

**/
UINT64
EFIAPI
DivU64x64Remainder (
  IN      UINT64                    Dividend,
  IN      UINT64                    Divisor,
  OUT     UINT64                    *Remainder  OPTIONAL
  );


/**
  Divides a 64-bit signed integer by a 64-bit signed integer and generates a
  64-bit signed result and a optional 64-bit signed remainder.

  This function divides the 64-bit signed value Dividend by the 64-bit signed
  value Divisor and generates a 64-bit signed quotient. If Remainder is not
  NULL, then the 64-bit signed remainder is returned in Remainder. This
  function returns the 64-bit signed quotient.

  It is the caller's responsibility to not call this function with a Divisor of 0.
  If Divisor is 0, then the quotient and remainder should be assumed to be
  the largest negative integer.

  If Divisor is 0, then ASSERT().

  @param  Dividend  A 64-bit signed value.
  @param  Divisor   A 64-bit signed value.
  @param  Remainder A pointer to a 64-bit signed value. This parameter is
                    optional and may be NULL.

  @return Dividend / Divisor.

**/
INT64
EFIAPI
DivS64x64Remainder (
  IN      INT64                     Dividend,
  IN      INT64                     Divisor,
  OUT     INT64                     *Remainder  OPTIONAL
  );


/**
  Reads a 16-bit value from memory that may be unaligned.

  This function returns the 16-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 16-bit value that may be unaligned.

  @return The 16-bit value read from Buffer.

**/
UINT16
EFIAPI
ReadUnaligned16 (
  IN CONST UINT16              *Buffer
  );


/**
  Writes a 16-bit value to memory that may be unaligned.

  This function writes the 16-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 16-bit value that may be unaligned.
  @param  Value   16-bit value to write to Buffer.

  @return The 16-bit value to write to Buffer.

**/
UINT16
EFIAPI
WriteUnaligned16 (
  OUT UINT16                    *Buffer,
  IN  UINT16                    Value
  );


/**
  Reads a 24-bit value from memory that may be unaligned.

  This function returns the 24-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 24-bit value that may be unaligned.

  @return The 24-bit value read from Buffer.

**/
UINT32
EFIAPI
ReadUnaligned24 (
  IN CONST UINT32              *Buffer
  );


/**
  Writes a 24-bit value to memory that may be unaligned.

  This function writes the 24-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 24-bit value that may be unaligned.
  @param  Value   24-bit value to write to Buffer.

  @return The 24-bit value to write to Buffer.

**/
UINT32
EFIAPI
WriteUnaligned24 (
  OUT UINT32                    *Buffer,
  IN  UINT32                    Value
  );


/**
  Reads a 32-bit value from memory that may be unaligned.

  This function returns the 32-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 32-bit value that may be unaligned.

  @return The 32-bit value read from Buffer.

**/
UINT32
EFIAPI
ReadUnaligned32 (
  IN CONST UINT32              *Buffer
  );


/**
  Writes a 32-bit value to memory that may be unaligned.

  This function writes the 32-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 32-bit value that may be unaligned.
  @param  Value   32-bit value to write to Buffer.

  @return The 32-bit value to write to Buffer.

**/
UINT32
EFIAPI
WriteUnaligned32 (
  OUT UINT32                    *Buffer,
  IN  UINT32                    Value
  );


/**
  Reads a 64-bit value from memory that may be unaligned.

  This function returns the 64-bit value pointed to by Buffer. The function
  guarantees that the read operation does not produce an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 64-bit value that may be unaligned.

  @return The 64-bit value read from Buffer.

**/
UINT64
EFIAPI
ReadUnaligned64 (
  IN CONST UINT64              *Buffer
  );


/**
  Writes a 64-bit value to memory that may be unaligned.

  This function writes the 64-bit value specified by Value to Buffer. Value is
  returned. The function guarantees that the write operation does not produce
  an alignment fault.

  If the Buffer is NULL, then ASSERT().

  @param  Buffer  The pointer to a 64-bit value that may be unaligned.
  @param  Value   64-bit value to write to Buffer.

  @return The 64-bit value to write to Buffer.

**/
UINT64
EFIAPI
WriteUnaligned64 (
  OUT UINT64                    *Buffer,
  IN  UINT64                    Value
  );


//
// Bit Field Functions
//

/**
  Returns a bit field from an 8-bit value.

  Returns the bitfield specified by the StartBit and the EndBit from Operand.

  If 8-bit operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.

  @return The bit field read.

**/
UINT8
EFIAPI
BitFieldRead8 (
  IN      UINT8                     Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit
  );


/**
  Writes a bit field to an 8-bit value, and returns the result.

  Writes Value to the bit field specified by the StartBit and the EndBit in
  Operand. All other bits in Operand are preserved. The new 8-bit value is
  returned.

  If 8-bit operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  Value     New value of the bit field.

  @return The new 8-bit value.

**/
UINT8
EFIAPI
BitFieldWrite8 (
  IN      UINT8                     Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT8                     Value
  );


/**
  Reads a bit field from an 8-bit value, performs a bitwise OR, and returns the
  result.

  Performs a bitwise OR between the bit field specified by StartBit
  and EndBit in Operand and the value specified by OrData. All other bits in
  Operand are preserved. The new 8-bit value is returned.

  If 8-bit operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  OrData    The value to OR with the read value from the value

  @return The new 8-bit value.

**/
UINT8
EFIAPI
BitFieldOr8 (
  IN      UINT8                     Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT8                     OrData
  );


/**
  Reads a bit field from an 8-bit value, performs a bitwise AND, and returns
  the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new 8-bit value is returned.

  If 8-bit operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with the read value from the value.

  @return The new 8-bit value.

**/
UINT8
EFIAPI
BitFieldAnd8 (
  IN      UINT8                     Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT8                     AndData
  );


/**
  Reads a bit field from an 8-bit value, performs a bitwise AND followed by a
  bitwise OR, and returns the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData, followed by a bitwise
  OR with value specified by OrData. All other bits in Operand are
  preserved. The new 8-bit value is returned.

  If 8-bit operations are not supported, then ASSERT().
  If StartBit is greater than 7, then ASSERT().
  If EndBit is greater than 7, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..7.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..7.
  @param  AndData   The value to AND with the read value from the value.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The new 8-bit value.

**/
UINT8
EFIAPI
BitFieldAndThenOr8 (
  IN      UINT8                     Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT8                     AndData,
  IN      UINT8                     OrData
  );


/**
  Returns a bit field from a 16-bit value.

  Returns the bitfield specified by the StartBit and the EndBit from Operand.

  If 16-bit operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.

  @return The bit field read.

**/
UINT16
EFIAPI
BitFieldRead16 (
  IN      UINT16                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit
  );


/**
  Writes a bit field to a 16-bit value, and returns the result.

  Writes Value to the bit field specified by the StartBit and the EndBit in
  Operand. All other bits in Operand are preserved. The new 16-bit value is
  returned.

  If 16-bit operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  Value     New value of the bit field.

  @return The new 16-bit value.

**/
UINT16
EFIAPI
BitFieldWrite16 (
  IN      UINT16                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT16                    Value
  );


/**
  Reads a bit field from a 16-bit value, performs a bitwise OR, and returns the
  result.

  Performs a bitwise OR between the bit field specified by StartBit
  and EndBit in Operand and the value specified by OrData. All other bits in
  Operand are preserved. The new 16-bit value is returned.

  If 16-bit operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  OrData    The value to OR with the read value from the value

  @return The new 16-bit value.

**/
UINT16
EFIAPI
BitFieldOr16 (
  IN      UINT16                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT16                    OrData
  );


/**
  Reads a bit field from a 16-bit value, performs a bitwise AND, and returns
  the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new 16-bit value is returned.

  If 16-bit operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with the read value from the value

  @return The new 16-bit value.

**/
UINT16
EFIAPI
BitFieldAnd16 (
  IN      UINT16                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT16                    AndData
  );


/**
  Reads a bit field from a 16-bit value, performs a bitwise AND followed by a
  bitwise OR, and returns the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData, followed by a bitwise
  OR with value specified by OrData. All other bits in Operand are
  preserved. The new 16-bit value is returned.

  If 16-bit operations are not supported, then ASSERT().
  If StartBit is greater than 15, then ASSERT().
  If EndBit is greater than 15, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..15.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..15.
  @param  AndData   The value to AND with the read value from the value.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The new 16-bit value.

**/
UINT16
EFIAPI
BitFieldAndThenOr16 (
  IN      UINT16                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT16                    AndData,
  IN      UINT16                    OrData
  );


/**
  Returns a bit field from a 32-bit value.

  Returns the bitfield specified by the StartBit and the EndBit from Operand.

  If 32-bit operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.

  @return The bit field read.

**/
UINT32
EFIAPI
BitFieldRead32 (
  IN      UINT32                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit
  );


/**
  Writes a bit field to a 32-bit value, and returns the result.

  Writes Value to the bit field specified by the StartBit and the EndBit in
  Operand. All other bits in Operand are preserved. The new 32-bit value is
  returned.

  If 32-bit operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  Value     New value of the bit field.

  @return The new 32-bit value.

**/
UINT32
EFIAPI
BitFieldWrite32 (
  IN      UINT32                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    Value
  );


/**
  Reads a bit field from a 32-bit value, performs a bitwise OR, and returns the
  result.

  Performs a bitwise OR between the bit field specified by StartBit
  and EndBit in Operand and the value specified by OrData. All other bits in
  Operand are preserved. The new 32-bit value is returned.

  If 32-bit operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  OrData    The value to OR with the read value from the value.

  @return The new 32-bit value.

**/
UINT32
EFIAPI
BitFieldOr32 (
  IN      UINT32                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    OrData
  );


/**
  Reads a bit field from a 32-bit value, performs a bitwise AND, and returns
  the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new 32-bit value is returned.

  If 32-bit operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the read value from the value

  @return The new 32-bit value.

**/
UINT32
EFIAPI
BitFieldAnd32 (
  IN      UINT32                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    AndData
  );


/**
  Reads a bit field from a 32-bit value, performs a bitwise AND followed by a
  bitwise OR, and returns the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData, followed by a bitwise
  OR with value specified by OrData. All other bits in Operand are
  preserved. The new 32-bit value is returned.

  If 32-bit operations are not supported, then ASSERT().
  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.
  @param  AndData   The value to AND with the read value from the value.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The new 32-bit value.

**/
UINT32
EFIAPI
BitFieldAndThenOr32 (
  IN      UINT32                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    AndData,
  IN      UINT32                    OrData
  );


/**
  Returns a bit field from a 64-bit value.

  Returns the bitfield specified by the StartBit and the EndBit from Operand.

  If 64-bit operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.

  @return The bit field read.

**/
UINT64
EFIAPI
BitFieldRead64 (
  IN      UINT64                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit
  );


/**
  Writes a bit field to a 64-bit value, and returns the result.

  Writes Value to the bit field specified by the StartBit and the EndBit in
  Operand. All other bits in Operand are preserved. The new 64-bit value is
  returned.

  If 64-bit operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If Value is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  Value     New value of the bit field.

  @return The new 64-bit value.

**/
UINT64
EFIAPI
BitFieldWrite64 (
  IN      UINT64                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT64                    Value
  );


/**
  Reads a bit field from a 64-bit value, performs a bitwise OR, and returns the
  result.

  Performs a bitwise OR between the bit field specified by StartBit
  and EndBit in Operand and the value specified by OrData. All other bits in
  Operand are preserved. The new 64-bit value is returned.

  If 64-bit operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  OrData    The value to OR with the read value from the value

  @return The new 64-bit value.

**/
UINT64
EFIAPI
BitFieldOr64 (
  IN      UINT64                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT64                    OrData
  );


/**
  Reads a bit field from a 64-bit value, performs a bitwise AND, and returns
  the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData. All other bits in Operand are
  preserved. The new 64-bit value is returned.

  If 64-bit operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  AndData   The value to AND with the read value from the value

  @return The new 64-bit value.

**/
UINT64
EFIAPI
BitFieldAnd64 (
  IN      UINT64                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT64                    AndData
  );


/**
  Reads a bit field from a 64-bit value, performs a bitwise AND followed by a
  bitwise OR, and returns the result.

  Performs a bitwise AND between the bit field specified by StartBit and EndBit
  in Operand and the value specified by AndData, followed by a bitwise
  OR with value specified by OrData. All other bits in Operand are
  preserved. The new 64-bit value is returned.

  If 64-bit operations are not supported, then ASSERT().
  If StartBit is greater than 63, then ASSERT().
  If EndBit is greater than 63, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().
  If AndData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().
  If OrData is larger than the bitmask value range specified by StartBit and EndBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..63.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..63.
  @param  AndData   The value to AND with the read value from the value.
  @param  OrData    The value to OR with the result of the AND operation.

  @return The new 64-bit value.

**/
UINT64
EFIAPI
BitFieldAndThenOr64 (
  IN      UINT64                    Operand,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT64                    AndData,
  IN      UINT64                    OrData
  );

/**
  Reads a bit field from a 32-bit value, counts and returns
  the number of set bits.

  Counts the number of set bits in the  bit field specified by
  StartBit and EndBit in Operand. The count is returned.

  If StartBit is greater than 31, then ASSERT().
  If EndBit is greater than 31, then ASSERT().
  If EndBit is less than StartBit, then ASSERT().

  @param  Operand   Operand on which to perform the bitfield operation.
  @param  StartBit  The ordinal of the least significant bit in the bit field.
                    Range 0..31.
  @param  EndBit    The ordinal of the most significant bit in the bit field.
                    Range 0..31.

  @return The number of bits set between StartBit and EndBit.

**/
UINT8
EFIAPI
BitFieldCountOnes32 (
  IN       UINT32                   Operand,
  IN       UINTN                    StartBit,
  IN       UINTN                    EndBit
  );

/**
   Reads a bit field from a 64-bit value, counts and returns
   the number of set bits.

   Counts the number of set bits in the  bit field specified by
   StartBit and EndBit in Operand. The count is returned.

   If StartBit is greater than 63, then ASSERT().
   If EndBit is greater than 63, then ASSERT().
   If EndBit is less than StartBit, then ASSERT().

   @param  Operand   Operand on which to perform the bitfield operation.
   @param  StartBit  The ordinal of the least significant bit in the bit field.
   Range 0..63.
   @param  EndBit    The ordinal of the most significant bit in the bit field.
   Range 0..63.

   @return The number of bits set between StartBit and EndBit.

**/
UINT8
EFIAPI
BitFieldCountOnes64 (
  IN       UINT64                   Operand,
  IN       UINTN                    StartBit,
  IN       UINTN                    EndBit
  );

//
// Base Library Checksum Functions
//

/**
  Returns the sum of all elements in a buffer in unit of UINT8.
  During calculation, the carry bits are dropped.

  This function calculates the sum of all elements in a buffer
  in unit of UINT8. The carry bits in result of addition are dropped.
  The result is returned as UINT8. If Length is Zero, then Zero is
  returned.

  If Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT8
EFIAPI
CalculateSum8 (
  IN      CONST UINT8              *Buffer,
  IN      UINTN                     Length
  );


/**
  Returns the two's complement checksum of all elements in a buffer
  of 8-bit values.

  This function first calculates the sum of the 8-bit values in the
  buffer specified by Buffer and Length.  The carry bits in the result
  of addition are dropped. Then, the two's complement of the sum is
  returned.  If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The two's complement checksum of Buffer.

**/
UINT8
EFIAPI
CalculateCheckSum8 (
  IN      CONST UINT8              *Buffer,
  IN      UINTN                     Length
  );


/**
  Returns the sum of all elements in a buffer of 16-bit values.  During
  calculation, the carry bits are dropped.

  This function calculates the sum of the 16-bit values in the buffer
  specified by Buffer and Length. The carry bits in result of addition are dropped.
  The 16-bit result is returned.  If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-bit boundary, then ASSERT().
  If Length is not aligned on a 16-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT16
EFIAPI
CalculateSum16 (
  IN      CONST UINT16             *Buffer,
  IN      UINTN                     Length
  );


/**
  Returns the two's complement checksum of all elements in a buffer of
  16-bit values.

  This function first calculates the sum of the 16-bit values in the buffer
  specified by Buffer and Length.  The carry bits in the result of addition
  are dropped. Then, the two's complement of the sum is returned.  If Length
  is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-bit boundary, then ASSERT().
  If Length is not aligned on a 16-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The two's complement checksum of Buffer.

**/
UINT16
EFIAPI
CalculateCheckSum16 (
  IN      CONST UINT16             *Buffer,
  IN      UINTN                     Length
  );


/**
  Returns the sum of all elements in a buffer of 32-bit values. During
  calculation, the carry bits are dropped.

  This function calculates the sum of the 32-bit values in the buffer
  specified by Buffer and Length. The carry bits in result of addition are dropped.
  The 32-bit result is returned. If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 32-bit boundary, then ASSERT().
  If Length is not aligned on a 32-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT32
EFIAPI
CalculateSum32 (
  IN      CONST UINT32             *Buffer,
  IN      UINTN                     Length
  );


/**
  Returns the two's complement checksum of all elements in a buffer of
  32-bit values.

  This function first calculates the sum of the 32-bit values in the buffer
  specified by Buffer and Length.  The carry bits in the result of addition
  are dropped. Then, the two's complement of the sum is returned.  If Length
  is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 32-bit boundary, then ASSERT().
  If Length is not aligned on a 32-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The two's complement checksum of Buffer.

**/
UINT32
EFIAPI
CalculateCheckSum32 (
  IN      CONST UINT32             *Buffer,
  IN      UINTN                     Length
  );


/**
  Returns the sum of all elements in a buffer of 64-bit values.  During
  calculation, the carry bits are dropped.

  This function calculates the sum of the 64-bit values in the buffer
  specified by Buffer and Length. The carry bits in result of addition are dropped.
  The 64-bit result is returned.  If Length is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 64-bit boundary, then ASSERT().
  If Length is not aligned on a 64-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the sum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Sum         The sum of Buffer with carry bits dropped during additions.

**/
UINT64
EFIAPI
CalculateSum64 (
  IN      CONST UINT64             *Buffer,
  IN      UINTN                     Length
  );


/**
  Returns the two's complement checksum of all elements in a buffer of
  64-bit values.

  This function first calculates the sum of the 64-bit values in the buffer
  specified by Buffer and Length.  The carry bits in the result of addition
  are dropped. Then, the two's complement of the sum is returned.  If Length
  is 0, then 0 is returned.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 64-bit boundary, then ASSERT().
  If Length is not aligned on a 64-bit boundary, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param  Buffer      The pointer to the buffer to carry out the checksum operation.
  @param  Length      The size, in bytes, of Buffer.

  @return Checksum    The two's complement checksum of Buffer.

**/
UINT64
EFIAPI
CalculateCheckSum64 (
  IN      CONST UINT64             *Buffer,
  IN      UINTN                     Length
  );

/**
  Computes and returns a 32-bit CRC for a data buffer.
  CRC32 value bases on ITU-T V.42.

  If Buffer is NULL, then ASSERT().
  If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

  @param[in]  Buffer       A pointer to the buffer on which the 32-bit CRC is to be computed.
  @param[in]  Length       The number of bytes in the buffer Data.

  @retval Crc32            The 32-bit CRC was computed for the data buffer.

**/
UINT32
EFIAPI
CalculateCrc32(
  IN  VOID                         *Buffer,
  IN  UINTN                        Length
  );

//
// Base Library CPU Functions
//

/**
  Function entry point used when a stack switch is requested with SwitchStack()

  @param  Context1        Context1 parameter passed into SwitchStack().
  @param  Context2        Context2 parameter passed into SwitchStack().

**/
typedef
VOID
(EFIAPI *SWITCH_STACK_ENTRY_POINT)(
  IN      VOID                      *Context1,  OPTIONAL
  IN      VOID                      *Context2   OPTIONAL
  );


/**
  Used to serialize load and store operations.

  All loads and stores that proceed calls to this function are guaranteed to be
  globally visible when this function returns.

**/
VOID
EFIAPI
MemoryFence (
  VOID
  );


/**
  Saves the current CPU context that can be restored with a call to LongJump()
  and returns 0.

  Saves the current CPU context in the buffer specified by JumpBuffer and
  returns 0. The initial call to SetJump() must always return 0. Subsequent
  calls to LongJump() cause a non-zero value to be returned by SetJump().

  If JumpBuffer is NULL, then ASSERT().
  For Itanium processors, if JumpBuffer is not aligned on a 16-byte boundary, then ASSERT().

  NOTE: The structure BASE_LIBRARY_JUMP_BUFFER is CPU architecture specific.
  The same structure must never be used for more than one CPU architecture context.
  For example, a BASE_LIBRARY_JUMP_BUFFER allocated by an IA-32 module must never be used from an x64 module.
  SetJump()/LongJump() is not currently supported for the EBC processor type.

  @param  JumpBuffer  A pointer to CPU context buffer.

  @retval 0 Indicates a return from SetJump().

**/
RETURNS_TWICE
UINTN
EFIAPI
SetJump (
  OUT     BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
  );


/**
  Restores the CPU context that was saved with SetJump().

  Restores the CPU context from the buffer specified by JumpBuffer. This
  function never returns to the caller. Instead is resumes execution based on
  the state of JumpBuffer.

  If JumpBuffer is NULL, then ASSERT().
  For Itanium processors, if JumpBuffer is not aligned on a 16-byte boundary, then ASSERT().
  If Value is 0, then ASSERT().

  @param  JumpBuffer  A pointer to CPU context buffer.
  @param  Value       The value to return when the SetJump() context is
                      restored and must be non-zero.

**/
VOID
EFIAPI
LongJump (
  IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer,
  IN      UINTN                     Value
  );


/**
  Enables CPU interrupts.

**/
VOID
EFIAPI
EnableInterrupts (
  VOID
  );


/**
  Disables CPU interrupts.

**/
VOID
EFIAPI
DisableInterrupts (
  VOID
  );


/**
  Disables CPU interrupts and returns the interrupt state prior to the disable
  operation.

  @retval TRUE  CPU interrupts were enabled on entry to this call.
  @retval FALSE CPU interrupts were disabled on entry to this call.

**/
BOOLEAN
EFIAPI
SaveAndDisableInterrupts (
  VOID
  );


/**
  Enables CPU interrupts for the smallest window required to capture any
  pending interrupts.

**/
VOID
EFIAPI
EnableDisableInterrupts (
  VOID
  );


/**
  Retrieves the current CPU interrupt state.

  Returns TRUE if interrupts are currently enabled. Otherwise
  returns FALSE.

  @retval TRUE  CPU interrupts are enabled.
  @retval FALSE CPU interrupts are disabled.

**/
BOOLEAN
EFIAPI
GetInterruptState (
  VOID
  );


/**
  Set the current CPU interrupt state.

  Sets the current CPU interrupt state to the state specified by
  InterruptState. If InterruptState is TRUE, then interrupts are enabled. If
  InterruptState is FALSE, then interrupts are disabled. InterruptState is
  returned.

  @param  InterruptState  TRUE if interrupts should enabled. FALSE if
                          interrupts should be disabled.

  @return InterruptState

**/
BOOLEAN
EFIAPI
SetInterruptState (
  IN      BOOLEAN                   InterruptState
  );


/**
  Requests CPU to pause for a short period of time.

  Requests CPU to pause for a short period of time. Typically used in MP
  systems to prevent memory starvation while waiting for a spin lock.

**/
VOID
EFIAPI
CpuPause (
  VOID
  );


/**
  Transfers control to a function starting with a new stack.

  Transfers control to the function specified by EntryPoint using the
  new stack specified by NewStack and passing in the parameters specified
  by Context1 and Context2.  Context1 and Context2 are optional and may
  be NULL.  The function EntryPoint must never return.  This function
  supports a variable number of arguments following the NewStack parameter.
  These additional arguments are ignored on IA-32, x64, and EBC architectures.
  Itanium processors expect one additional parameter of type VOID * that specifies
  the new backing store pointer.

  If EntryPoint is NULL, then ASSERT().
  If NewStack is NULL, then ASSERT().

  @param  EntryPoint  A pointer to function to call with the new stack.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function.
  @param  ...         This variable argument list is ignored for IA-32, x64, and
                      EBC architectures.  For Itanium processors, this variable
                      argument list is expected to contain a single parameter of
                      type VOID * that specifies the new backing store pointer.


**/
VOID
EFIAPI
SwitchStack (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1,  OPTIONAL
  IN      VOID                      *Context2,  OPTIONAL
  IN      VOID                      *NewStack,
  ...
  );


/**
  Generates a breakpoint on the CPU.

  Generates a breakpoint on the CPU. The breakpoint must be implemented such
  that code can resume normal execution after the breakpoint.

**/
VOID
EFIAPI
CpuBreakpoint (
  VOID
  );


/**
  Executes an infinite loop.

  Forces the CPU to execute an infinite loop. A debugger may be used to skip
  past the loop and the code that follows the loop must execute properly. This
  implies that the infinite loop must not cause the code that follow it to be
  optimized away.

**/
VOID
EFIAPI
CpuDeadLoop (
  VOID
  );


/**
  Uses as a barrier to stop speculative execution.

  Ensures that no later instruction will execute speculatively, until all prior
  instructions have completed.

**/
VOID
EFIAPI
SpeculationBarrier (
  VOID
  );


#if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64)
///
/// IA32 and x64 Specific Functions.
/// Byte packed structure for 16-bit Real Mode EFLAGS.
///
typedef union {
  struct {
    UINT32  CF:1;           ///< Carry Flag.
    UINT32  Reserved_0:1;   ///< Reserved.
    UINT32  PF:1;           ///< Parity Flag.
    UINT32  Reserved_1:1;   ///< Reserved.
    UINT32  AF:1;           ///< Auxiliary Carry Flag.
    UINT32  Reserved_2:1;   ///< Reserved.
    UINT32  ZF:1;           ///< Zero Flag.
    UINT32  SF:1;           ///< Sign Flag.
    UINT32  TF:1;           ///< Trap Flag.
    UINT32  IF:1;           ///< Interrupt Enable Flag.
    UINT32  DF:1;           ///< Direction Flag.
    UINT32  OF:1;           ///< Overflow Flag.
    UINT32  IOPL:2;         ///< I/O Privilege Level.
    UINT32  NT:1;           ///< Nested Task.
    UINT32  Reserved_3:1;   ///< Reserved.
  } Bits;
  UINT16    Uint16;
} IA32_FLAGS16;

///
/// Byte packed structure for EFLAGS/RFLAGS.
/// 32-bits on IA-32.
/// 64-bits on x64.  The upper 32-bits on x64 are reserved.
///
typedef union {
  struct {
    UINT32  CF:1;           ///< Carry Flag.
    UINT32  Reserved_0:1;   ///< Reserved.
    UINT32  PF:1;           ///< Parity Flag.
    UINT32  Reserved_1:1;   ///< Reserved.
    UINT32  AF:1;           ///< Auxiliary Carry Flag.
    UINT32  Reserved_2:1;   ///< Reserved.
    UINT32  ZF:1;           ///< Zero Flag.
    UINT32  SF:1;           ///< Sign Flag.
    UINT32  TF:1;           ///< Trap Flag.
    UINT32  IF:1;           ///< Interrupt Enable Flag.
    UINT32  DF:1;           ///< Direction Flag.
    UINT32  OF:1;           ///< Overflow Flag.
    UINT32  IOPL:2;         ///< I/O Privilege Level.
    UINT32  NT:1;           ///< Nested Task.
    UINT32  Reserved_3:1;   ///< Reserved.
    UINT32  RF:1;           ///< Resume Flag.
    UINT32  VM:1;           ///< Virtual 8086 Mode.
    UINT32  AC:1;           ///< Alignment Check.
    UINT32  VIF:1;          ///< Virtual Interrupt Flag.
    UINT32  VIP:1;          ///< Virtual Interrupt Pending.
    UINT32  ID:1;           ///< ID Flag.
    UINT32  Reserved_4:10;  ///< Reserved.
  } Bits;
  UINTN     UintN;
} IA32_EFLAGS32;

///
/// Byte packed structure for Control Register 0 (CR0).
/// 32-bits on IA-32.
/// 64-bits on x64.  The upper 32-bits on x64 are reserved.
///
typedef union {
  struct {
    UINT32  PE:1;           ///< Protection Enable.
    UINT32  MP:1;           ///< Monitor Coprocessor.
    UINT32  EM:1;           ///< Emulation.
    UINT32  TS:1;           ///< Task Switched.
    UINT32  ET:1;           ///< Extension Type.
    UINT32  NE:1;           ///< Numeric Error.
    UINT32  Reserved_0:10;  ///< Reserved.
    UINT32  WP:1;           ///< Write Protect.
    UINT32  Reserved_1:1;   ///< Reserved.
    UINT32  AM:1;           ///< Alignment Mask.
    UINT32  Reserved_2:10;  ///< Reserved.
    UINT32  NW:1;           ///< Mot Write-through.
    UINT32  CD:1;           ///< Cache Disable.
    UINT32  PG:1;           ///< Paging.
  } Bits;
  UINTN     UintN;
} IA32_CR0;

///
/// Byte packed structure for Control Register 4 (CR4).
/// 32-bits on IA-32.
/// 64-bits on x64.  The upper 32-bits on x64 are reserved.
///
typedef union {
  struct {
    UINT32  VME:1;          ///< Virtual-8086 Mode Extensions.
    UINT32  PVI:1;          ///< Protected-Mode Virtual Interrupts.
    UINT32  TSD:1;          ///< Time Stamp Disable.
    UINT32  DE:1;           ///< Debugging Extensions.
    UINT32  PSE:1;          ///< Page Size Extensions.
    UINT32  PAE:1;          ///< Physical Address Extension.
    UINT32  MCE:1;          ///< Machine Check Enable.
    UINT32  PGE:1;          ///< Page Global Enable.
    UINT32  PCE:1;          ///< Performance Monitoring Counter
                            ///< Enable.
    UINT32  OSFXSR:1;       ///< Operating System Support for
                            ///< FXSAVE and FXRSTOR instructions
    UINT32  OSXMMEXCPT:1;   ///< Operating System Support for
                            ///< Unmasked SIMD Floating Point
                            ///< Exceptions.
    UINT32  UMIP:1;         ///< User-Mode Instruction Prevention.
    UINT32  LA57:1;         ///< Linear Address 57bit.
    UINT32  VMXE:1;         ///< VMX Enable.
    UINT32  SMXE:1;         ///< SMX Enable.
    UINT32  Reserved_3:1;   ///< Reserved.
    UINT32  FSGSBASE:1;     ///< FSGSBASE Enable.
    UINT32  PCIDE:1;        ///< PCID Enable.
    UINT32  OSXSAVE:1;      ///< XSAVE and Processor Extended States Enable.
    UINT32  Reserved_4:1;   ///< Reserved.
    UINT32  SMEP:1;         ///< SMEP Enable.
    UINT32  SMAP:1;         ///< SMAP Enable.
    UINT32  PKE:1;          ///< Protection-Key Enable.
    UINT32  Reserved_5:9;   ///< Reserved.
  } Bits;
  UINTN     UintN;
} IA32_CR4;

///
/// Byte packed structure for a segment descriptor in a GDT/LDT.
///
typedef union {
  struct {
    UINT32  LimitLow:16;
    UINT32  BaseLow:16;
    UINT32  BaseMid:8;
    UINT32  Type:4;
    UINT32  S:1;
    UINT32  DPL:2;
    UINT32  P:1;
    UINT32  LimitHigh:4;
    UINT32  AVL:1;
    UINT32  L:1;
    UINT32  DB:1;
    UINT32  G:1;
    UINT32  BaseHigh:8;
  } Bits;
  UINT64  Uint64;
} IA32_SEGMENT_DESCRIPTOR;

///
/// Byte packed structure for an IDTR, GDTR, LDTR descriptor.
///
#pragma pack (1)
typedef struct {
  UINT16  Limit;
  UINTN   Base;
} IA32_DESCRIPTOR;
#pragma pack ()

#define IA32_IDT_GATE_TYPE_TASK          0x85
#define IA32_IDT_GATE_TYPE_INTERRUPT_16  0x86
#define IA32_IDT_GATE_TYPE_TRAP_16       0x87
#define IA32_IDT_GATE_TYPE_INTERRUPT_32  0x8E
#define IA32_IDT_GATE_TYPE_TRAP_32       0x8F

#define IA32_GDT_TYPE_TSS               0x9
#define IA32_GDT_ALIGNMENT              8

#if defined (MDE_CPU_IA32)
///
/// Byte packed structure for an IA-32 Interrupt Gate Descriptor.
///
typedef union {
  struct {
    UINT32  OffsetLow:16;   ///< Offset bits 15..0.
    UINT32  Selector:16;    ///< Selector.
    UINT32  Reserved_0:8;   ///< Reserved.
    UINT32  GateType:8;     ///< Gate Type.  See #defines above.
    UINT32  OffsetHigh:16;  ///< Offset bits 31..16.
  } Bits;
  UINT64  Uint64;
} IA32_IDT_GATE_DESCRIPTOR;

#pragma pack (1)
//
// IA32 Task-State Segment Definition
//
typedef struct {
  UINT16    PreviousTaskLink;
  UINT16    Reserved_2;
  UINT32    ESP0;
  UINT16    SS0;
  UINT16    Reserved_10;
  UINT32    ESP1;
  UINT16    SS1;
  UINT16    Reserved_18;
  UINT32    ESP2;
  UINT16    SS2;
  UINT16    Reserved_26;
  UINT32    CR3;
  UINT32    EIP;
  UINT32    EFLAGS;
  UINT32    EAX;
  UINT32    ECX;
  UINT32    EDX;
  UINT32    EBX;
  UINT32    ESP;
  UINT32    EBP;
  UINT32    ESI;
  UINT32    EDI;
  UINT16    ES;
  UINT16    Reserved_74;
  UINT16    CS;
  UINT16    Reserved_78;
  UINT16    SS;
  UINT16    Reserved_82;
  UINT16    DS;
  UINT16    Reserved_86;
  UINT16    FS;
  UINT16    Reserved_90;
  UINT16    GS;
  UINT16    Reserved_94;
  UINT16    LDTSegmentSelector;
  UINT16    Reserved_98;
  UINT16    T;
  UINT16    IOMapBaseAddress;
} IA32_TASK_STATE_SEGMENT;

typedef union {
  struct {
    UINT32  LimitLow:16;    ///< Segment Limit 15..00
    UINT32  BaseLow:16;     ///< Base Address  15..00
    UINT32  BaseMid:8;      ///< Base Address  23..16
    UINT32  Type:4;         ///< Type (1 0 B 1)
    UINT32  Reserved_43:1;  ///< 0
    UINT32  DPL:2;          ///< Descriptor Privilege Level
    UINT32  P:1;            ///< Segment Present
    UINT32  LimitHigh:4;    ///< Segment Limit 19..16
    UINT32  AVL:1;          ///< Available for use by system software
    UINT32  Reserved_52:2;  ///< 0 0
    UINT32  G:1;            ///< Granularity
    UINT32  BaseHigh:8;     ///< Base Address 31..24
  } Bits;
  UINT64  Uint64;
} IA32_TSS_DESCRIPTOR;
#pragma pack ()

#endif // defined (MDE_CPU_IA32)

#if defined (MDE_CPU_X64)
///
/// Byte packed structure for an x64 Interrupt Gate Descriptor.
///
typedef union {
  struct {
    UINT32  OffsetLow:16;   ///< Offset bits 15..0.
    UINT32  Selector:16;    ///< Selector.
    UINT32  Reserved_0:8;   ///< Reserved.
    UINT32  GateType:8;     ///< Gate Type.  See #defines above.
    UINT32  OffsetHigh:16;  ///< Offset bits 31..16.
    UINT32  OffsetUpper:32; ///< Offset bits 63..32.
    UINT32  Reserved_1:32;  ///< Reserved.
  } Bits;
  struct {
    UINT64  Uint64;
    UINT64  Uint64_1;
  } Uint128;
} IA32_IDT_GATE_DESCRIPTOR;

#pragma pack (1)
//
// IA32 Task-State Segment Definition
//
typedef struct {
  UINT32    Reserved_0;
  UINT64    RSP0;
  UINT64    RSP1;
  UINT64    RSP2;
  UINT64    Reserved_28;
  UINT64    IST[7];
  UINT64    Reserved_92;
  UINT16    Reserved_100;
  UINT16    IOMapBaseAddress;
} IA32_TASK_STATE_SEGMENT;

typedef union {
  struct {
    UINT32  LimitLow:16;    ///< Segment Limit 15..00
    UINT32  BaseLow:16;     ///< Base Address  15..00
    UINT32  BaseMidl:8;     ///< Base Address  23..16
    UINT32  Type:4;         ///< Type (1 0 B 1)
    UINT32  Reserved_43:1;  ///< 0
    UINT32  DPL:2;          ///< Descriptor Privilege Level
    UINT32  P:1;            ///< Segment Present
    UINT32  LimitHigh:4;    ///< Segment Limit 19..16
    UINT32  AVL:1;          ///< Available for use by system software
    UINT32  Reserved_52:2;  ///< 0 0
    UINT32  G:1;            ///< Granularity
    UINT32  BaseMidh:8;     ///< Base Address  31..24
    UINT32  BaseHigh:32;    ///< Base Address  63..32
    UINT32  Reserved_96:32; ///< Reserved
  } Bits;
  struct {
    UINT64  Uint64;
    UINT64  Uint64_1;
  } Uint128;
} IA32_TSS_DESCRIPTOR;
#pragma pack ()

#endif // defined (MDE_CPU_X64)

///
/// Byte packed structure for an FP/SSE/SSE2 context.
///
typedef struct {
  UINT8  Buffer[512];
} IA32_FX_BUFFER;

///
/// Structures for the 16-bit real mode thunks.
///
typedef struct {
  UINT32                            Reserved1;
  UINT32                            Reserved2;
  UINT32                            Reserved3;
  UINT32                            Reserved4;
  UINT8                             BL;
  UINT8                             BH;
  UINT16                            Reserved5;
  UINT8                             DL;
  UINT8                             DH;
  UINT16                            Reserved6;
  UINT8                             CL;
  UINT8                             CH;
  UINT16                            Reserved7;
  UINT8                             AL;
  UINT8                             AH;
  UINT16                            Reserved8;
} IA32_BYTE_REGS;

typedef struct {
  UINT16                            DI;
  UINT16                            Reserved1;
  UINT16                            SI;
  UINT16                            Reserved2;
  UINT16                            BP;
  UINT16                            Reserved3;
  UINT16                            SP;
  UINT16                            Reserved4;
  UINT16                            BX;
  UINT16                            Reserved5;
  UINT16                            DX;
  UINT16                            Reserved6;
  UINT16                            CX;
  UINT16                            Reserved7;
  UINT16                            AX;
  UINT16                            Reserved8;
} IA32_WORD_REGS;

typedef struct {
  UINT32                            EDI;
  UINT32                            ESI;
  UINT32                            EBP;
  UINT32                            ESP;
  UINT32                            EBX;
  UINT32                            EDX;
  UINT32                            ECX;
  UINT32                            EAX;
  UINT16                            DS;
  UINT16                            ES;
  UINT16                            FS;
  UINT16                            GS;
  IA32_EFLAGS32                     EFLAGS;
  UINT32                            Eip;
  UINT16                            CS;
  UINT16                            SS;
} IA32_DWORD_REGS;

typedef union {
  IA32_DWORD_REGS                   E;
  IA32_WORD_REGS                    X;
  IA32_BYTE_REGS                    H;
} IA32_REGISTER_SET;

///
/// Byte packed structure for an 16-bit real mode thunks.
///
typedef struct {
  IA32_REGISTER_SET                 *RealModeState;
  VOID                              *RealModeBuffer;
  UINT32                            RealModeBufferSize;
  UINT32                            ThunkAttributes;
} THUNK_CONTEXT;

#define THUNK_ATTRIBUTE_BIG_REAL_MODE             0x00000001
#define THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15   0x00000002
#define THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL 0x00000004

///
/// Type definition for representing labels in NASM source code that allow for
/// the patching of immediate operands of IA32 and X64 instructions.
///
/// While the type is technically defined as a function type (note: not a
/// pointer-to-function type), such labels in NASM source code never stand for
/// actual functions, and identifiers declared with this function type should
/// never be called. This is also why the EFIAPI calling convention specifier
/// is missing from the typedef, and why the typedef does not follow the usual
/// edk2 coding style for function (or pointer-to-function) typedefs. The VOID
/// return type and the VOID argument list are merely artifacts.
///
typedef VOID (X86_ASSEMBLY_PATCH_LABEL) (VOID);

/**
  Retrieves CPUID information.

  Executes the CPUID instruction with EAX set to the value specified by Index.
  This function always returns Index.
  If Eax is not NULL, then the value of EAX after CPUID is returned in Eax.
  If Ebx is not NULL, then the value of EBX after CPUID is returned in Ebx.
  If Ecx is not NULL, then the value of ECX after CPUID is returned in Ecx.
  If Edx is not NULL, then the value of EDX after CPUID is returned in Edx.
  This function is only available on IA-32 and x64.

  @param  Index The 32-bit value to load into EAX prior to invoking the CPUID
                instruction.
  @param  Eax   The pointer to the 32-bit EAX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.
  @param  Ebx   The pointer to the 32-bit EBX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.
  @param  Ecx   The pointer to the 32-bit ECX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.
  @param  Edx   The pointer to the 32-bit EDX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.

  @return Index.

**/
UINT32
EFIAPI
AsmCpuid (
  IN      UINT32                    Index,
  OUT     UINT32                    *Eax,  OPTIONAL
  OUT     UINT32                    *Ebx,  OPTIONAL
  OUT     UINT32                    *Ecx,  OPTIONAL
  OUT     UINT32                    *Edx   OPTIONAL
  );


/**
  Retrieves CPUID information using an extended leaf identifier.

  Executes the CPUID instruction with EAX set to the value specified by Index
  and ECX set to the value specified by SubIndex. This function always returns
  Index. This function is only available on IA-32 and x64.

  If Eax is not NULL, then the value of EAX after CPUID is returned in Eax.
  If Ebx is not NULL, then the value of EBX after CPUID is returned in Ebx.
  If Ecx is not NULL, then the value of ECX after CPUID is returned in Ecx.
  If Edx is not NULL, then the value of EDX after CPUID is returned in Edx.

  @param  Index     The 32-bit value to load into EAX prior to invoking the
                    CPUID instruction.
  @param  SubIndex  The 32-bit value to load into ECX prior to invoking the
                    CPUID instruction.
  @param  Eax       The pointer to the 32-bit EAX value returned by the CPUID
                    instruction. This is an optional parameter that may be
                    NULL.
  @param  Ebx       The pointer to the 32-bit EBX value returned by the CPUID
                    instruction. This is an optional parameter that may be
                    NULL.
  @param  Ecx       The pointer to the 32-bit ECX value returned by the CPUID
                    instruction. This is an optional parameter that may be
                    NULL.
  @param  Edx       The pointer to the 32-bit EDX value returned by the CPUID
                    instruction. This is an optional parameter that may be
                    NULL.

  @return Index.

**/
UINT32
EFIAPI
AsmCpuidEx (
  IN      UINT32                    Index,
  IN      UINT32                    SubIndex,
  OUT     UINT32                    *Eax,  OPTIONAL
  OUT     UINT32                    *Ebx,  OPTIONAL
  OUT     UINT32                    *Ecx,  OPTIONAL
  OUT     UINT32                    *Edx   OPTIONAL
  );


/**
  Set CD bit and clear NW bit of CR0 followed by a WBINVD.

  Disables the caches by setting the CD bit of CR0 to 1, clearing the NW bit of CR0 to 0,
  and executing a WBINVD instruction.  This function is only available on IA-32 and x64.

**/
VOID
EFIAPI
AsmDisableCache (
  VOID
  );


/**
  Perform a WBINVD and clear both the CD and NW bits of CR0.

  Enables the caches by executing a WBINVD instruction and then clear both the CD and NW
  bits of CR0 to 0.  This function is only available on IA-32 and x64.

**/
VOID
EFIAPI
AsmEnableCache (
  VOID
  );


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
  IN      UINT32                    Index
  );


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
  IN      UINT32                    Index,
  IN      UINT32                    Value
  );


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
  IN      UINT32                    Index,
  IN      UINT32                    OrData
  );


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
  IN      UINT32                    Index,
  IN      UINT32                    AndData
  );


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
  IN      UINT32                    Index,
  IN      UINT32                    AndData,
  IN      UINT32                    OrData
  );


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
  IN      UINT32                    Index,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit
  );


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
  @param  Value     New value of the bit field.

  @return The lower 32-bit of the value written to the MSR.

**/
UINT32
EFIAPI
AsmMsrBitFieldWrite32 (
  IN      UINT32                    Index,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    Value
  );


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
  IN      UINT32                    Index,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    OrData
  );


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
  IN      UINT32                    Index,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    AndData
  );


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
  IN      UINT32                    Index,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT32                    AndData,
  IN      UINT32                    OrData
  );


/**
  Returns a 64-bit Machine Specific Register(MSR).

  Reads and returns the 64-bit MSR specified by Index. No parameter checking is
  performed on Index, and some Index values may cause CPU exceptions. The
  caller must either guarantee that Index is valid, or the caller must set up
  exception handlers to catch the exceptions. This function is only available
  on IA-32 and x64.

  @param  Index The 32-bit MSR index to read.

  @return The value of the MSR identified by Index.

**/
UINT64
EFIAPI
AsmReadMsr64 (
  IN      UINT32                    Index
  );


/**
  Writes a 64-bit value to a Machine Specific Register(MSR), and returns the
  value.

  Writes the 64-bit value specified by Value to the MSR specified by Index. The
  64-bit value written to the MSR is returned. No parameter checking is
  performed on Index or Value, and some of these may cause CPU exceptions. The
  caller must either guarantee that Index and Value are valid, or the caller
  must establish proper exception handlers. This function is only available on
  IA-32 and x64.

  @param  Index The 32-bit MSR index to write.
  @param  Value The 64-bit value to write to the MSR.

  @return Value

**/
UINT64
EFIAPI
AsmWriteMsr64 (
  IN      UINT32                    Index,
  IN      UINT64                    Value
  );


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
  IN      UINT32                    Index,
  IN      UINT64                    OrData
  );


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
  IN      UINT32                    Index,
  IN      UINT64                    AndData
  );


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
  IN      UINT32                    Index,
  IN      UINT64                    AndData,
  IN      UINT64                    OrData
  );


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
  IN      UINT32                    Index,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit
  );


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
  @param  Value     New value of the bit field.

  @return The value written back to the MSR.

**/
UINT64
EFIAPI
AsmMsrBitFieldWrite64 (
  IN      UINT32                    Index,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT64                    Value
  );


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
  IN      UINT32                    Index,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT64                    OrData
  );


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
  IN      UINT32                    Index,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT64                    AndData
  );


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
  IN      UINT32                    Index,
  IN      UINTN                     StartBit,
  IN      UINTN                     EndBit,
  IN      UINT64                    AndData,
  IN      UINT64                    OrData
  );


/**
  Reads the current value of the EFLAGS register.

  Reads and returns the current value of the EFLAGS register. This function is
  only available on IA-32 and x64. This returns a 32-bit value on IA-32 and a
  64-bit value on x64.

  @return EFLAGS on IA-32 or RFLAGS on x64.

**/
UINTN
EFIAPI
AsmReadEflags (
  VOID
  );


/**
  Reads the current value of the Control Register 0 (CR0).

  Reads and returns the current value of CR0. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 0 (CR0).

**/
UINTN
EFIAPI
AsmReadCr0 (
  VOID
  );


/**
  Reads the current value of the Control Register 2 (CR2).

  Reads and returns the current value of CR2. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 2 (CR2).

**/
UINTN
EFIAPI
AsmReadCr2 (
  VOID
  );


/**
  Reads the current value of the Control Register 3 (CR3).

  Reads and returns the current value of CR3. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 3 (CR3).

**/
UINTN
EFIAPI
AsmReadCr3 (
  VOID
  );


/**
  Reads the current value of the Control Register 4 (CR4).

  Reads and returns the current value of CR4. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 4 (CR4).

**/
UINTN
EFIAPI
AsmReadCr4 (
  VOID
  );


/**
  Writes a value to Control Register 0 (CR0).

  Writes and returns a new value to CR0. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr0 The value to write to CR0.

  @return The value written to CR0.

**/
UINTN
EFIAPI
AsmWriteCr0 (
  UINTN  Cr0
  );


/**
  Writes a value to Control Register 2 (CR2).

  Writes and returns a new value to CR2. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr2 The value to write to CR2.

  @return The value written to CR2.

**/
UINTN
EFIAPI
AsmWriteCr2 (
  UINTN  Cr2
  );


/**
  Writes a value to Control Register 3 (CR3).

  Writes and returns a new value to CR3. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr3 The value to write to CR3.

  @return The value written to CR3.

**/
UINTN
EFIAPI
AsmWriteCr3 (
  UINTN  Cr3
  );


/**
  Writes a value to Control Register 4 (CR4).

  Writes and returns a new value to CR4. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr4 The value to write to CR4.

  @return The value written to CR4.

**/
UINTN
EFIAPI
AsmWriteCr4 (
  UINTN  Cr4
  );


/**
  Reads the current value of Debug Register 0 (DR0).

  Reads and returns the current value of DR0. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 0 (DR0).

**/
UINTN
EFIAPI
AsmReadDr0 (
  VOID
  );


/**
  Reads the current value of Debug Register 1 (DR1).

  Reads and returns the current value of DR1. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 1 (DR1).

**/
UINTN
EFIAPI
AsmReadDr1 (
  VOID
  );


/**
  Reads the current value of Debug Register 2 (DR2).

  Reads and returns the current value of DR2. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 2 (DR2).

**/
UINTN
EFIAPI
AsmReadDr2 (
  VOID
  );


/**
  Reads the current value of Debug Register 3 (DR3).

  Reads and returns the current value of DR3. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 3 (DR3).

**/
UINTN
EFIAPI
AsmReadDr3 (
  VOID
  );


/**
  Reads the current value of Debug Register 4 (DR4).

  Reads and returns the current value of DR4. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 4 (DR4).

**/
UINTN
EFIAPI
AsmReadDr4 (
  VOID
  );


/**
  Reads the current value of Debug Register 5 (DR5).

  Reads and returns the current value of DR5. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 5 (DR5).

**/
UINTN
EFIAPI
AsmReadDr5 (
  VOID
  );


/**
  Reads the current value of Debug Register 6 (DR6).

  Reads and returns the current value of DR6. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 6 (DR6).

**/
UINTN
EFIAPI
AsmReadDr6 (
  VOID
  );


/**
  Reads the current value of Debug Register 7 (DR7).

  Reads and returns the current value of DR7. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 7 (DR7).

**/
UINTN
EFIAPI
AsmReadDr7 (
  VOID
  );


/**
  Writes a value to Debug Register 0 (DR0).

  Writes and returns a new value to DR0. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr0 The value to write to Dr0.

  @return The value written to Debug Register 0 (DR0).

**/
UINTN
EFIAPI
AsmWriteDr0 (
  UINTN  Dr0
  );


/**
  Writes a value to Debug Register 1 (DR1).

  Writes and returns a new value to DR1. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr1 The value to write to Dr1.

  @return The value written to Debug Register 1 (DR1).

**/
UINTN
EFIAPI
AsmWriteDr1 (
  UINTN  Dr1
  );


/**
  Writes a value to Debug Register 2 (DR2).

  Writes and returns a new value to DR2. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr2 The value to write to Dr2.

  @return The value written to Debug Register 2 (DR2).

**/
UINTN
EFIAPI
AsmWriteDr2 (
  UINTN  Dr2
  );


/**
  Writes a value to Debug Register 3 (DR3).

  Writes and returns a new value to DR3. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr3 The value to write to Dr3.

  @return The value written to Debug Register 3 (DR3).

**/
UINTN
EFIAPI
AsmWriteDr3 (
  UINTN  Dr3
  );


/**
  Writes a value to Debug Register 4 (DR4).

  Writes and returns a new value to DR4. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr4 The value to write to Dr4.

  @return The value written to Debug Register 4 (DR4).

**/
UINTN
EFIAPI
AsmWriteDr4 (
  UINTN  Dr4
  );


/**
  Writes a value to Debug Register 5 (DR5).

  Writes and returns a new value to DR5. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr5 The value to write to Dr5.

  @return The value written to Debug Register 5 (DR5).

**/
UINTN
EFIAPI
AsmWriteDr5 (
  UINTN  Dr5
  );


/**
  Writes a value to Debug Register 6 (DR6).

  Writes and returns a new value to DR6. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr6 The value to write to Dr6.

  @return The value written to Debug Register 6 (DR6).

**/
UINTN
EFIAPI
AsmWriteDr6 (
  UINTN  Dr6
  );


/**
  Writes a value to Debug Register 7 (DR7).

  Writes and returns a new value to DR7. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr7 The value to write to Dr7.

  @return The value written to Debug Register 7 (DR7).

**/
UINTN
EFIAPI
AsmWriteDr7 (
  UINTN  Dr7
  );


/**
  Reads the current value of Code Segment Register (CS).

  Reads and returns the current value of CS. This function is only available on
  IA-32 and x64.

  @return The current value of CS.

**/
UINT16
EFIAPI
AsmReadCs (
  VOID
  );


/**
  Reads the current value of Data Segment Register (DS).

  Reads and returns the current value of DS. This function is only available on
  IA-32 and x64.

  @return The current value of DS.

**/
UINT16
EFIAPI
AsmReadDs (
  VOID
  );


/**
  Reads the current value of Extra Segment Register (ES).

  Reads and returns the current value of ES. This function is only available on
  IA-32 and x64.

  @return The current value of ES.

**/
UINT16
EFIAPI
AsmReadEs (
  VOID
  );


/**
  Reads the current value of FS Data Segment Register (FS).

  Reads and returns the current value of FS. This function is only available on
  IA-32 and x64.

  @return The current value of FS.

**/
UINT16
EFIAPI
AsmReadFs (
  VOID
  );


/**
  Reads the current value of GS Data Segment Register (GS).

  Reads and returns the current value of GS. This function is only available on
  IA-32 and x64.

  @return The current value of GS.

**/
UINT16
EFIAPI
AsmReadGs (
  VOID
  );


/**
  Reads the current value of Stack Segment Register (SS).

  Reads and returns the current value of SS. This function is only available on
  IA-32 and x64.

  @return The current value of SS.

**/
UINT16
EFIAPI
AsmReadSs (
  VOID
  );


/**
  Reads the current value of Task Register (TR).

  Reads and returns the current value of TR. This function is only available on
  IA-32 and x64.

  @return The current value of TR.

**/
UINT16
EFIAPI
AsmReadTr (
  VOID
  );


/**
  Reads the current Global Descriptor Table Register(GDTR) descriptor.

  Reads and returns the current GDTR descriptor and returns it in Gdtr. This
  function is only available on IA-32 and x64.

  If Gdtr is NULL, then ASSERT().

  @param  Gdtr  The pointer to a GDTR descriptor.

**/
VOID
EFIAPI
AsmReadGdtr (
  OUT     IA32_DESCRIPTOR           *Gdtr
  );


/**
  Writes the current Global Descriptor Table Register (GDTR) descriptor.

  Writes and the current GDTR descriptor specified by Gdtr. This function is
  only available on IA-32 and x64.

  If Gdtr is NULL, then ASSERT().

  @param  Gdtr  The pointer to a GDTR descriptor.

**/
VOID
EFIAPI
AsmWriteGdtr (
  IN      CONST IA32_DESCRIPTOR     *Gdtr
  );


/**
  Reads the current Interrupt Descriptor Table Register(IDTR) descriptor.

  Reads and returns the current IDTR descriptor and returns it in Idtr. This
  function is only available on IA-32 and x64.

  If Idtr is NULL, then ASSERT().

  @param  Idtr  The pointer to a IDTR descriptor.

**/
VOID
EFIAPI
AsmReadIdtr (
  OUT     IA32_DESCRIPTOR           *Idtr
  );


/**
  Writes the current Interrupt Descriptor Table Register(IDTR) descriptor.

  Writes the current IDTR descriptor and returns it in Idtr. This function is
  only available on IA-32 and x64.

  If Idtr is NULL, then ASSERT().

  @param  Idtr  The pointer to a IDTR descriptor.

**/
VOID
EFIAPI
AsmWriteIdtr (
  IN      CONST IA32_DESCRIPTOR     *Idtr
  );


/**
  Reads the current Local Descriptor Table Register(LDTR) selector.

  Reads and returns the current 16-bit LDTR descriptor value. This function is
  only available on IA-32 and x64.

  @return The current selector of LDT.

**/
UINT16
EFIAPI
AsmReadLdtr (
  VOID
  );


/**
  Writes the current Local Descriptor Table Register (LDTR) selector.

  Writes and the current LDTR descriptor specified by Ldtr. This function is
  only available on IA-32 and x64.

  @param  Ldtr  16-bit LDTR selector value.

**/
VOID
EFIAPI
AsmWriteLdtr (
  IN      UINT16                    Ldtr
  );


/**
  Save the current floating point/SSE/SSE2 context to a buffer.

  Saves the current floating point/SSE/SSE2 state to the buffer specified by
  Buffer. Buffer must be aligned on a 16-byte boundary. This function is only
  available on IA-32 and x64.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-byte boundary, then ASSERT().

  @param  Buffer  The pointer to a buffer to save the floating point/SSE/SSE2 context.

**/
VOID
EFIAPI
AsmFxSave (
  OUT     IA32_FX_BUFFER            *Buffer
  );


/**
  Restores the current floating point/SSE/SSE2 context from a buffer.

  Restores the current floating point/SSE/SSE2 state from the buffer specified
  by Buffer. Buffer must be aligned on a 16-byte boundary. This function is
  only available on IA-32 and x64.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-byte boundary, then ASSERT().
  If Buffer was not saved with AsmFxSave(), then ASSERT().

  @param  Buffer  The pointer to a buffer to save the floating point/SSE/SSE2 context.

**/
VOID
EFIAPI
AsmFxRestore (
  IN      CONST IA32_FX_BUFFER      *Buffer
  );


/**
  Reads the current value of 64-bit MMX Register #0 (MM0).

  Reads and returns the current value of MM0. This function is only available
  on IA-32 and x64.

  @return The current value of MM0.

**/
UINT64
EFIAPI
AsmReadMm0 (
  VOID
  );


/**
  Reads the current value of 64-bit MMX Register #1 (MM1).

  Reads and returns the current value of MM1. This function is only available
  on IA-32 and x64.

  @return The current value of MM1.

**/
UINT64
EFIAPI
AsmReadMm1 (
  VOID
  );


/**
  Reads the current value of 64-bit MMX Register #2 (MM2).

  Reads and returns the current value of MM2. This function is only available
  on IA-32 and x64.

  @return The current value of MM2.

**/
UINT64
EFIAPI
AsmReadMm2 (
  VOID
  );


/**
  Reads the current value of 64-bit MMX Register #3 (MM3).

  Reads and returns the current value of MM3. This function is only available
  on IA-32 and x64.

  @return The current value of MM3.

**/
UINT64
EFIAPI
AsmReadMm3 (
  VOID
  );


/**
  Reads the current value of 64-bit MMX Register #4 (MM4).

  Reads and returns the current value of MM4. This function is only available
  on IA-32 and x64.

  @return The current value of MM4.

**/
UINT64
EFIAPI
AsmReadMm4 (
  VOID
  );


/**
  Reads the current value of 64-bit MMX Register #5 (MM5).

  Reads and returns the current value of MM5. This function is only available
  on IA-32 and x64.

  @return The current value of MM5.

**/
UINT64
EFIAPI
AsmReadMm5 (
  VOID
  );


/**
  Reads the current value of 64-bit MMX Register #6 (MM6).

  Reads and returns the current value of MM6. This function is only available
  on IA-32 and x64.

  @return The current value of MM6.

**/
UINT64
EFIAPI
AsmReadMm6 (
  VOID
  );


/**
  Reads the current value of 64-bit MMX Register #7 (MM7).

  Reads and returns the current value of MM7. This function is only available
  on IA-32 and x64.

  @return The current value of MM7.

**/
UINT64
EFIAPI
AsmReadMm7 (
  VOID
  );


/**
  Writes the current value of 64-bit MMX Register #0 (MM0).

  Writes the current value of MM0. This function is only available on IA32 and
  x64.

  @param  Value The 64-bit value to write to MM0.

**/
VOID
EFIAPI
AsmWriteMm0 (
  IN      UINT64                    Value
  );


/**
  Writes the current value of 64-bit MMX Register #1 (MM1).

  Writes the current value of MM1. This function is only available on IA32 and
  x64.

  @param  Value The 64-bit value to write to MM1.

**/
VOID
EFIAPI
AsmWriteMm1 (
  IN      UINT64                    Value
  );


/**
  Writes the current value of 64-bit MMX Register #2 (MM2).

  Writes the current value of MM2. This function is only available on IA32 and
  x64.

  @param  Value The 64-bit value to write to MM2.

**/
VOID
EFIAPI
AsmWriteMm2 (
  IN      UINT64                    Value
  );


/**
  Writes the current value of 64-bit MMX Register #3 (MM3).

  Writes the current value of MM3. This function is only available on IA32 and
  x64.

  @param  Value The 64-bit value to write to MM3.

**/
VOID
EFIAPI
AsmWriteMm3 (
  IN      UINT64                    Value
  );


/**
  Writes the current value of 64-bit MMX Register #4 (MM4).

  Writes the current value of MM4. This function is only available on IA32 and
  x64.

  @param  Value The 64-bit value to write to MM4.

**/
VOID
EFIAPI
AsmWriteMm4 (
  IN      UINT64                    Value
  );


/**
  Writes the current value of 64-bit MMX Register #5 (MM5).

  Writes the current value of MM5. This function is only available on IA32 and
  x64.

  @param  Value The 64-bit value to write to MM5.

**/
VOID
EFIAPI
AsmWriteMm5 (
  IN      UINT64                    Value
  );


/**
  Writes the current value of 64-bit MMX Register #6 (MM6).

  Writes the current value of MM6. This function is only available on IA32 and
  x64.

  @param  Value The 64-bit value to write to MM6.

**/
VOID
EFIAPI
AsmWriteMm6 (
  IN      UINT64                    Value
  );


/**
  Writes the current value of 64-bit MMX Register #7 (MM7).

  Writes the current value of MM7. This function is only available on IA32 and
  x64.

  @param  Value The 64-bit value to write to MM7.

**/
VOID
EFIAPI
AsmWriteMm7 (
  IN      UINT64                    Value
  );


/**
  Reads the current value of Time Stamp Counter (TSC).

  Reads and returns the current value of TSC. This function is only available
  on IA-32 and x64.

  @return The current value of TSC

**/
UINT64
EFIAPI
AsmReadTsc (
  VOID
  );


/**
  Reads the current value of a Performance Counter (PMC).

  Reads and returns the current value of performance counter specified by
  Index. This function is only available on IA-32 and x64.

  @param  Index The 32-bit Performance Counter index to read.

  @return The value of the PMC specified by Index.

**/
UINT64
EFIAPI
AsmReadPmc (
  IN      UINT32                    Index
  );


/**
  Sets up a monitor buffer that is used by AsmMwait().

  Executes a MONITOR instruction with the register state specified by Eax, Ecx
  and Edx. Returns Eax. This function is only available on IA-32 and x64.

  @param  Eax The value to load into EAX or RAX before executing the MONITOR
              instruction.
  @param  Ecx The value to load into ECX or RCX before executing the MONITOR
              instruction.
  @param  Edx The value to load into EDX or RDX before executing the MONITOR
              instruction.

  @return Eax

**/
UINTN
EFIAPI
AsmMonitor (
  IN      UINTN                     Eax,
  IN      UINTN                     Ecx,
  IN      UINTN                     Edx
  );


/**
  Executes an MWAIT instruction.

  Executes an MWAIT instruction with the register state specified by Eax and
  Ecx. Returns Eax. This function is only available on IA-32 and x64.

  @param  Eax The value to load into EAX or RAX before executing the MONITOR
              instruction.
  @param  Ecx The value to load into ECX or RCX before executing the MONITOR
              instruction.

  @return Eax

**/
UINTN
EFIAPI
AsmMwait (
  IN      UINTN                     Eax,
  IN      UINTN                     Ecx
  );


/**
  Executes a WBINVD instruction.

  Executes a WBINVD instruction. This function is only available on IA-32 and
  x64.

**/
VOID
EFIAPI
AsmWbinvd (
  VOID
  );


/**
  Executes a INVD instruction.

  Executes a INVD instruction. This function is only available on IA-32 and
  x64.

**/
VOID
EFIAPI
AsmInvd (
  VOID
  );


/**
  Flushes a cache line from all the instruction and data caches within the
  coherency domain of the CPU.

  Flushed the cache line specified by LinearAddress, and returns LinearAddress.
  This function is only available on IA-32 and x64.

  @param  LinearAddress The address of the cache line to flush. If the CPU is
                        in a physical addressing mode, then LinearAddress is a
                        physical address. If the CPU is in a virtual
                        addressing mode, then LinearAddress is a virtual
                        address.

  @return LinearAddress.
**/
VOID *
EFIAPI
AsmFlushCacheLine (
  IN      VOID                      *LinearAddress
  );


/**
  Enables the 32-bit paging mode on the CPU.

  Enables the 32-bit paging mode on the CPU. CR0, CR3, CR4, and the page tables
  must be properly initialized prior to calling this service. This function
  assumes the current execution mode is 32-bit protected mode. This function is
  only available on IA-32. After the 32-bit paging mode is enabled, control is
  transferred to the function specified by EntryPoint using the new stack
  specified by NewStack and passing in the parameters specified by Context1 and
  Context2. Context1 and Context2 are optional and may be NULL. The function
  EntryPoint must never return.

  If the current execution mode is not 32-bit protected mode, then ASSERT().
  If EntryPoint is NULL, then ASSERT().
  If NewStack is NULL, then ASSERT().

  There are a number of constraints that must be followed before calling this
  function:
  1)  Interrupts must be disabled.
  2)  The caller must be in 32-bit protected mode with flat descriptors. This
      means all descriptors must have a base of 0 and a limit of 4GB.
  3)  CR0 and CR4 must be compatible with 32-bit protected mode with flat
      descriptors.
  4)  CR3 must point to valid page tables that will be used once the transition
      is complete, and those page tables must guarantee that the pages for this
      function and the stack are identity mapped.

  @param  EntryPoint  A pointer to function to call with the new stack after
                      paging is enabled.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function as the first parameter after paging is enabled.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function as the second parameter after paging is enabled.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function after paging is enabled.

**/
VOID
EFIAPI
AsmEnablePaging32 (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1,  OPTIONAL
  IN      VOID                      *Context2,  OPTIONAL
  IN      VOID                      *NewStack
  );


/**
  Disables the 32-bit paging mode on the CPU.

  Disables the 32-bit paging mode on the CPU and returns to 32-bit protected
  mode. This function assumes the current execution mode is 32-paged protected
  mode. This function is only available on IA-32. After the 32-bit paging mode
  is disabled, control is transferred to the function specified by EntryPoint
  using the new stack specified by NewStack and passing in the parameters
  specified by Context1 and Context2. Context1 and Context2 are optional and
  may be NULL. The function EntryPoint must never return.

  If the current execution mode is not 32-bit paged mode, then ASSERT().
  If EntryPoint is NULL, then ASSERT().
  If NewStack is NULL, then ASSERT().

  There are a number of constraints that must be followed before calling this
  function:
  1)  Interrupts must be disabled.
  2)  The caller must be in 32-bit paged mode.
  3)  CR0, CR3, and CR4 must be compatible with 32-bit paged mode.
  4)  CR3 must point to valid page tables that guarantee that the pages for
      this function and the stack are identity mapped.

  @param  EntryPoint  A pointer to function to call with the new stack after
                      paging is disabled.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function as the first parameter after paging is disabled.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function as the second parameter after paging is
                      disabled.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function after paging is disabled.

**/
VOID
EFIAPI
AsmDisablePaging32 (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1,  OPTIONAL
  IN      VOID                      *Context2,  OPTIONAL
  IN      VOID                      *NewStack
  );


/**
  Enables the 64-bit paging mode on the CPU.

  Enables the 64-bit paging mode on the CPU. CR0, CR3, CR4, and the page tables
  must be properly initialized prior to calling this service. This function
  assumes the current execution mode is 32-bit protected mode with flat
  descriptors. This function is only available on IA-32. After the 64-bit
  paging mode is enabled, control is transferred to the function specified by
  EntryPoint using the new stack specified by NewStack and passing in the
  parameters specified by Context1 and Context2. Context1 and Context2 are
  optional and may be 0. The function EntryPoint must never return.

  If the current execution mode is not 32-bit protected mode with flat
  descriptors, then ASSERT().
  If EntryPoint is 0, then ASSERT().
  If NewStack is 0, then ASSERT().

  @param  Cs          The 16-bit selector to load in the CS before EntryPoint
                      is called. The descriptor in the GDT that this selector
                      references must be setup for long mode.
  @param  EntryPoint  The 64-bit virtual address of the function to call with
                      the new stack after paging is enabled.
  @param  Context1    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the first parameter after
                      paging is enabled.
  @param  Context2    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the second parameter after
                      paging is enabled.
  @param  NewStack    The 64-bit virtual address of the new stack to use for
                      the EntryPoint function after paging is enabled.

**/
VOID
EFIAPI
AsmEnablePaging64 (
  IN      UINT16                    Cs,
  IN      UINT64                    EntryPoint,
  IN      UINT64                    Context1,  OPTIONAL
  IN      UINT64                    Context2,  OPTIONAL
  IN      UINT64                    NewStack
  );


/**
  Disables the 64-bit paging mode on the CPU.

  Disables the 64-bit paging mode on the CPU and returns to 32-bit protected
  mode. This function assumes the current execution mode is 64-paging mode.
  This function is only available on x64. After the 64-bit paging mode is
  disabled, control is transferred to the function specified by EntryPoint
  using the new stack specified by NewStack and passing in the parameters
  specified by Context1 and Context2. Context1 and Context2 are optional and
  may be 0. The function EntryPoint must never return.

  If the current execution mode is not 64-bit paged mode, then ASSERT().
  If EntryPoint is 0, then ASSERT().
  If NewStack is 0, then ASSERT().

  @param  Cs          The 16-bit selector to load in the CS before EntryPoint
                      is called. The descriptor in the GDT that this selector
                      references must be setup for 32-bit protected mode.
  @param  EntryPoint  The 64-bit virtual address of the function to call with
                      the new stack after paging is disabled.
  @param  Context1    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the first parameter after
                      paging is disabled.
  @param  Context2    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the second parameter after
                      paging is disabled.
  @param  NewStack    The 64-bit virtual address of the new stack to use for
                      the EntryPoint function after paging is disabled.

**/
VOID
EFIAPI
AsmDisablePaging64 (
  IN      UINT16                    Cs,
  IN      UINT32                    EntryPoint,
  IN      UINT32                    Context1,  OPTIONAL
  IN      UINT32                    Context2,  OPTIONAL
  IN      UINT32                    NewStack
  );


//
// 16-bit thunking services
//

/**
  Retrieves the properties for 16-bit thunk functions.

  Computes the size of the buffer and stack below 1MB required to use the
  AsmPrepareThunk16(), AsmThunk16() and AsmPrepareAndThunk16() functions. This
  buffer size is returned in RealModeBufferSize, and the stack size is returned
  in ExtraStackSize. If parameters are passed to the 16-bit real mode code,
  then the actual minimum stack size is ExtraStackSize plus the maximum number
  of bytes that need to be passed to the 16-bit real mode code.

  If RealModeBufferSize is NULL, then ASSERT().
  If ExtraStackSize is NULL, then ASSERT().

  @param  RealModeBufferSize  A pointer to the size of the buffer below 1MB
                              required to use the 16-bit thunk functions.
  @param  ExtraStackSize      A pointer to the extra size of stack below 1MB
                              that the 16-bit thunk functions require for
                              temporary storage in the transition to and from
                              16-bit real mode.

**/
VOID
EFIAPI
AsmGetThunk16Properties (
  OUT     UINT32                    *RealModeBufferSize,
  OUT     UINT32                    *ExtraStackSize
  );


/**
  Prepares all structures a code required to use AsmThunk16().

  Prepares all structures and code required to use AsmThunk16().

  This interface is limited to be used in either physical mode or virtual modes with paging enabled where the
  virtual to physical mappings for ThunkContext.RealModeBuffer is mapped 1:1.

  If ThunkContext is NULL, then ASSERT().

  @param  ThunkContext  A pointer to the context structure that describes the
                        16-bit real mode code to call.

**/
VOID
EFIAPI
AsmPrepareThunk16 (
  IN OUT  THUNK_CONTEXT             *ThunkContext
  );


/**
  Transfers control to a 16-bit real mode entry point and returns the results.

  Transfers control to a 16-bit real mode entry point and returns the results.
  AsmPrepareThunk16() must be called with ThunkContext before this function is used.
  This function must be called with interrupts disabled.

  The register state from the RealModeState field of ThunkContext is restored just prior
  to calling the 16-bit real mode entry point.  This includes the EFLAGS field of RealModeState,
  which is used to set the interrupt state when a 16-bit real mode entry point is called.
  Control is transferred to the 16-bit real mode entry point specified by the CS and Eip fields of RealModeState.
  The stack is initialized to the SS and ESP fields of RealModeState.  Any parameters passed to
  the 16-bit real mode code must be populated by the caller at SS:ESP prior to calling this function.
  The 16-bit real mode entry point is invoked with a 16-bit CALL FAR instruction,
  so when accessing stack contents, the 16-bit real mode code must account for the 16-bit segment
  and 16-bit offset of the return address that were pushed onto the stack. The 16-bit real mode entry
  point must exit with a RETF instruction. The register state is captured into RealModeState immediately
  after the RETF instruction is executed.

  If EFLAGS specifies interrupts enabled, or any of the 16-bit real mode code enables interrupts,
  or any of the 16-bit real mode code makes a SW interrupt, then the caller is responsible for making sure
  the IDT at address 0 is initialized to handle any HW or SW interrupts that may occur while in 16-bit real mode.

  If EFLAGS specifies interrupts enabled, or any of the 16-bit real mode code enables interrupts,
  then the caller is responsible for making sure the 8259 PIC is in a state compatible with 16-bit real mode.
  This includes the base vectors, the interrupt masks, and the edge/level trigger mode.

  If THUNK_ATTRIBUTE_BIG_REAL_MODE is set in the ThunkAttributes field of ThunkContext, then the user code
  is invoked in big real mode.  Otherwise, the user code is invoked in 16-bit real mode with 64KB segment limits.

  If neither THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15 nor THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL are set in
  ThunkAttributes, then it is assumed that the user code did not enable the A20 mask, and no attempt is made to
  disable the A20 mask.

  If THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15 is set and THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL is clear in
  ThunkAttributes, then attempt to use the INT 15 service to disable the A20 mask.  If this INT 15 call fails,
  then attempt to disable the A20 mask by directly accessing the 8042 keyboard controller I/O ports.

  If THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15 is clear and THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL is set in
  ThunkAttributes, then attempt to disable the A20 mask by directly accessing the 8042 keyboard controller I/O ports.

  If ThunkContext is NULL, then ASSERT().
  If AsmPrepareThunk16() was not previously called with ThunkContext, then ASSERT().
  If both THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15 and THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL are set in
  ThunkAttributes, then ASSERT().

  This interface is limited to be used in either physical mode or virtual modes with paging enabled where the
  virtual to physical mappings for ThunkContext.RealModeBuffer are mapped 1:1.

  @param  ThunkContext  A pointer to the context structure that describes the
                        16-bit real mode code to call.

**/
VOID
EFIAPI
AsmThunk16 (
  IN OUT  THUNK_CONTEXT             *ThunkContext
  );


/**
  Prepares all structures and code for a 16-bit real mode thunk, transfers
  control to a 16-bit real mode entry point, and returns the results.

  Prepares all structures and code for a 16-bit real mode thunk, transfers
  control to a 16-bit real mode entry point, and returns the results. If the
  caller only need to perform a single 16-bit real mode thunk, then this
  service should be used. If the caller intends to make more than one 16-bit
  real mode thunk, then it is more efficient if AsmPrepareThunk16() is called
  once and AsmThunk16() can be called for each 16-bit real mode thunk.

  This interface is limited to be used in either physical mode or virtual modes with paging enabled where the
  virtual to physical mappings for ThunkContext.RealModeBuffer is mapped 1:1.

  See AsmPrepareThunk16() and AsmThunk16() for the detailed description and ASSERT() conditions.

  @param  ThunkContext  A pointer to the context structure that describes the
                        16-bit real mode code to call.

**/
VOID
EFIAPI
AsmPrepareAndThunk16 (
  IN OUT  THUNK_CONTEXT             *ThunkContext
  );

/**
  Generates a 16-bit random number through RDRAND instruction.

  if Rand is NULL, then ASSERT().

  @param[out]  Rand     Buffer pointer to store the random result.

  @retval TRUE          RDRAND call was successful.
  @retval FALSE         Failed attempts to call RDRAND.

 **/
BOOLEAN
EFIAPI
AsmRdRand16 (
  OUT     UINT16                    *Rand
  );

/**
  Generates a 32-bit random number through RDRAND instruction.

  if Rand is NULL, then ASSERT().

  @param[out]  Rand     Buffer pointer to store the random result.

  @retval TRUE          RDRAND call was successful.
  @retval FALSE         Failed attempts to call RDRAND.

**/
BOOLEAN
EFIAPI
AsmRdRand32 (
  OUT     UINT32                    *Rand
  );

/**
  Generates a 64-bit random number through RDRAND instruction.

  if Rand is NULL, then ASSERT().

  @param[out]  Rand     Buffer pointer to store the random result.

  @retval TRUE          RDRAND call was successful.
  @retval FALSE         Failed attempts to call RDRAND.

**/
BOOLEAN
EFIAPI
AsmRdRand64  (
  OUT     UINT64                    *Rand
  );

/**
  Load given selector into TR register.

  @param[in] Selector     Task segment selector
**/
VOID
EFIAPI
AsmWriteTr (
  IN UINT16 Selector
  );

/**
  Performs a serializing operation on all load-from-memory instructions that
  were issued prior the AsmLfence function.

  Executes a LFENCE instruction. This function is only available on IA-32 and x64.

**/
VOID
EFIAPI
AsmLfence (
  VOID
  );

/**
  Patch the immediate operand of an IA32 or X64 instruction such that the byte,
  word, dword or qword operand is encoded at the end of the instruction's
  binary representation.

  This function should be used to update object code that was compiled with
  NASM from assembly source code. Example:

  NASM source code:

        mov     eax, strict dword 0 ; the imm32 zero operand will be patched
    ASM_PFX(gPatchCr3):
        mov     cr3, eax

  C source code:

    X86_ASSEMBLY_PATCH_LABEL gPatchCr3;
    PatchInstructionX86 (gPatchCr3, AsmReadCr3 (), 4);

  @param[out] InstructionEnd  Pointer right past the instruction to patch. The
                              immediate operand to patch is expected to
                              comprise the trailing bytes of the instruction.
                              If InstructionEnd is closer to address 0 than
                              ValueSize permits, then ASSERT().

  @param[in] PatchValue       The constant to write to the immediate operand.
                              The caller is responsible for ensuring that
                              PatchValue can be represented in the byte, word,
                              dword or qword operand (as indicated through
                              ValueSize); otherwise ASSERT().

  @param[in] ValueSize        The size of the operand in bytes; must be 1, 2,
                              4, or 8. ASSERT() otherwise.
**/
VOID
EFIAPI
PatchInstructionX86 (
  OUT X86_ASSEMBLY_PATCH_LABEL *InstructionEnd,
  IN  UINT64                   PatchValue,
  IN  UINTN                    ValueSize
  );

#endif // defined (MDE_CPU_IA32) || defined (MDE_CPU_X64)
#endif // !defined (__BASE_LIB__)
