/** @file
  This section defines the Regular Expression Protocol. This protocol isused to match
  Unicode strings against Regular Expression patterns.

Copyright (c) 2015-2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in UEFI Specification 2.5.

**/

#ifndef __REGULAR_EXPRESSION_PROTOCOL_H__
#define __REGULAR_EXPRESSION_PROTOCOL_H__

#define EFI_REGULAR_EXPRESSION_PROTOCOL_GUID \
  { \
    0xB3F79D9A, 0x436C, 0xDC11, {0xB0, 0x52, 0xCD, 0x85, 0xDF, 0x52, 0x4C, 0xE6 } \
  }

#define EFI_REGEX_SYNTAX_TYPE_POSIX_EXTENDED_GUID \
  { \
    0x5F05B20F, 0x4A56, 0xC231, {0xFA, 0x0B, 0xA7, 0xB1, 0xF1, 0x10, 0x04, 0x1D } \
  }

#define EFI_REGEX_SYNTAX_TYPE_PERL_GUID \
  { \
    0x63E60A51, 0x497D, 0xD427, {0xC4, 0xA5, 0xB8, 0xAB, 0xDC, 0x3A, 0xAE, 0xB6 } \
  }

#define EFI_REGEX_SYNTAX_TYPE_ECMA_262_GUID \
  { \
    0x9A473A4A, 0x4CEB, 0xB95A, {0x41, 0x5E, 0x5B, 0xA0, 0xBC, 0x63, 0x9B, 0x2E } \
  }

typedef struct _EFI_REGULAR_EXPRESSION_PROTOCOL EFI_REGULAR_EXPRESSION_PROTOCOL;

typedef struct {
  CONST CHAR16    *CapturePtr; // Pointer to the start of the captured sub-expression
                               // within matched String.

  UINTN           Length;   // Length of captured sub-expression.
} EFI_REGEX_CAPTURE;

typedef EFI_GUID EFI_REGEX_SYNTAX_TYPE;

//
// Protocol member functions
//

/**
  Returns information about the regular expression syntax types supported
  by the implementation.

  This                     A pointer to the EFI_REGULAR_EXPRESSION_PROTOCOL
                           instance.

  RegExSyntaxTypeListSize  On input, the size in bytes of RegExSyntaxTypeList.
                           On output with a return code of EFI_SUCCESS, the
                           size in bytes of the data returned in
                           RegExSyntaxTypeList. On output with a return code
                           of EFI_BUFFER_TOO_SMALL, the size of
                           RegExSyntaxTypeListrequired to obtain the list.

  RegExSyntaxTypeList      A caller-allocated memory buffer filled by the
                           driver with one EFI_REGEX_SYNTAX_TYPEelement
                           for each supported Regular expression syntax
                           type. The list must not change across multiple
                           calls to the same driver. The first syntax
                           type in the list is the default type for the
                           driver.

  @retval EFI_SUCCESS            The regular expression syntax types list
                                 was returned successfully.
  @retval EFI_UNSUPPORTED        The service is not supported by this driver.
  @retval EFI_DEVICE_ERROR       The list of syntax types could not be
                                 retrieved due to a hardware or firmware error.
  @retval EFI_BUFFER_TOO_SMALL   The buffer RegExSyntaxTypeList is too small
                                 to hold the result.
  @retval EFI_INVALID_PARAMETER  RegExSyntaxTypeListSize is NULL

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REGULAR_EXPRESSION_GET_INFO)(
  IN     EFI_REGULAR_EXPRESSION_PROTOCOL *This,
  IN OUT UINTN                           *RegExSyntaxTypeListSize,
  OUT    EFI_REGEX_SYNTAX_TYPE           *RegExSyntaxTypeList
  );

/**
  Checks if the input string matches to the regular expression pattern.

  This          A pointer to the EFI_REGULAR_EXPRESSION_PROTOCOL instance.
                Type EFI_REGULAR_EXPRESSION_PROTOCOL is defined in Section
                XYZ.

  String        A pointer to a NULL terminated string to match against the
                regular expression string specified by Pattern.

  Pattern       A pointer to a NULL terminated string that represents the
                regular expression.

  SyntaxType    A pointer to the EFI_REGEX_SYNTAX_TYPE that identifies the
                regular expression syntax type to use. May be NULL in which
                case the function will use its default regular expression
                syntax type.

  Result        On return, points to TRUE if String fully matches against
                the regular expression Pattern using the regular expression
                SyntaxType. Otherwise, points to FALSE.

  Captures      A Pointer to an array of EFI_REGEX_CAPTURE objects to receive
                the captured groups in the event of a match. The full
                sub-string match is put in Captures[0], and the results of N
                capturing groups are put in Captures[1:N]. If Captures is
                NULL, then this function doesn't allocate the memory for the
                array and does not build up the elements. It only returns the
                number of matching patterns in CapturesCount. If Captures is
                not NULL, this function returns a pointer to an array and
                builds up the elements in the array. CapturesCount is also
                updated to the number of matching patterns found. It is the
                caller's responsibility to free the memory pool in Captures
                and in each CapturePtr in the array elements.

  CapturesCount On output, CapturesCount is the number of matching patterns
                found in String. Zero means no matching patterns were found
                in the string.

  @retval EFI_SUCCESS            The regular expression string matching
                                 completed successfully.
  @retval EFI_UNSUPPORTED        The regular expression syntax specified by
                                 SyntaxTypeis not supported by this driver.
  @retval EFI_DEVICE_ERROR       The regular expression string matching
                                 failed due to a hardware or firmware error.
  @retval EFI_INVALID_PARAMETER  String, Pattern, Result, or CapturesCountis
                                 NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REGULAR_EXPRESSION_MATCH)(
  IN  EFI_REGULAR_EXPRESSION_PROTOCOL *This,
  IN  CHAR16                          *String,
  IN  CHAR16                          *Pattern,
  IN  EFI_REGEX_SYNTAX_TYPE           *SyntaxType  OPTIONAL,
  OUT BOOLEAN                         *Result,
  OUT EFI_REGEX_CAPTURE               **Captures  OPTIONAL,
  OUT UINTN                           *CapturesCount
  );

struct _EFI_REGULAR_EXPRESSION_PROTOCOL {
  EFI_REGULAR_EXPRESSION_MATCH       MatchString;
  EFI_REGULAR_EXPRESSION_GET_INFO    GetInfo;
};

extern EFI_GUID  gEfiRegularExpressionProtocolGuid;

//
// For regular expression rules specified in the POSIX Extended Regular
// Expression (ERE) Syntax:
//
extern EFI_GUID  gEfiRegexSyntaxTypePosixExtendedGuid;

//
// For regular expression rules specifiedin the ECMA 262 Specification
//
extern EFI_GUID  gEfiRegexSyntaxTypeEcma262Guid;

//
// For regular expression rules specified in the Perl standard:
//
extern EFI_GUID  gEfiRegexSyntaxTypePerlGuid;

#endif
