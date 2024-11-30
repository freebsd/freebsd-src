/** @file
  The file provides the mechanism to set and get the values
  associated with a keyword exposed through a x-UEFI- prefixed
  configuration language namespace.

Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in UEFI Specification 2.5.


**/

#ifndef __EFI_CONFIG_KEYWORD_HANDLER_H__
#define __EFI_CONFIG_KEYWORD_HANDLER_H__

#define EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL_GUID \
{ \
  0x0a8badd5, 0x03b8, 0x4d19, {0xb1, 0x28, 0x7b, 0x8f, 0x0e, 0xda, 0xa5, 0x96 } \
}

// ***********************************************************
// Progress Errors
// ***********************************************************
#define KEYWORD_HANDLER_NO_ERROR                     0x00000000
#define KEYWORD_HANDLER_NAMESPACE_ID_NOT_FOUND       0x00000001
#define KEYWORD_HANDLER_MALFORMED_STRING             0x00000002
#define KEYWORD_HANDLER_KEYWORD_NOT_FOUND            0x00000004
#define KEYWORD_HANDLER_INCOMPATIBLE_VALUE_DETECTED  0x00000008
#define KEYWORD_HANDLER_ACCESS_NOT_PERMITTED         0x00000010
#define KEYWORD_HANDLER_UNDEFINED_PROCESSING_ERROR   0x80000000

typedef struct _EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL;

/**

  This function accepts a <MultiKeywordResp> formatted string, finds the associated
  keyword owners, creates a <MultiConfigResp> string from it and forwards it to the
  EFI_HII_ROUTING_PROTOCOL.RouteConfig function.

  If there is an issue in resolving the contents of the KeywordString, then the
  function returns an error and also sets the Progress and ProgressErr with the
  appropriate information about where the issue occurred and additional data about
  the nature of the issue.

  In the case when KeywordString containing multiple keywords, when an EFI_NOT_FOUND
  error is generated during processing the second or later keyword element, the system
  storage associated with earlier keywords is not modified. All elements of the
  KeywordString must successfully pass all tests for format and access prior to making
  any modifications to storage.

  In the case when EFI_DEVICE_ERROR is returned from the processing of a KeywordString
  containing multiple keywords, the state of storage associated with earlier keywords
  is undefined.


  @param This             Pointer to the EFI_KEYWORD_HANDLER _PROTOCOL instance.

  @param KeywordString    A null-terminated string in <MultiKeywordResp> format.

  @param Progress         On return, points to a character in the KeywordString.
                          Points to the string's NULL terminator if the request
                          was successful. Points to the most recent '&' before
                          the first failing name / value pair (or the beginning
                          of the string if the failure is in the first name / value
                          pair) if the request was not successful.

  @param ProgressErr      If during the processing of the KeywordString there was
                          a failure, this parameter gives additional information
                          about the possible source of the problem. The various
                          errors are defined in "Related Definitions" below.


  @retval EFI_SUCCESS             The specified action was completed successfully.

  @retval EFI_INVALID_PARAMETER   One or more of the following are TRUE:
                                  1. KeywordString is NULL.
                                  2. Parsing of the KeywordString resulted in an
                                     error. See Progress and ProgressErr for more data.

  @retval EFI_NOT_FOUND           An element of the KeywordString was not found.
                                  See ProgressErr for more data.

  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.
                                  See ProgressErr for more data.

  @retval EFI_ACCESS_DENIED       The action violated system policy. See ProgressErr
                                  for more data.

  @retval EFI_DEVICE_ERROR        An unexpected system error occurred. See ProgressErr
                                  for more data.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_CONFIG_KEYWORD_HANDLER_SET_DATA)(
  IN EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL *This,
  IN CONST EFI_STRING                    KeywordString,
  OUT EFI_STRING                         *Progress,
  OUT UINT32                             *ProgressErr
  );

/**

  This function accepts a <MultiKeywordRequest> formatted string, finds the underlying
  keyword owners, creates a <MultiConfigRequest> string from it and forwards it to the
  EFI_HII_ROUTING_PROTOCOL.ExtractConfig function.

  If there is an issue in resolving the contents of the KeywordString, then the function
  returns an EFI_INVALID_PARAMETER and also set the Progress and ProgressErr with the
  appropriate information about where the issue occurred and additional data about the
  nature of the issue.

  In the case when KeywordString is NULL, or contains multiple keywords, or when
  EFI_NOT_FOUND is generated while processing the keyword elements, the Results string
  contains values returned for all keywords processed prior to the keyword generating the
  error but no values for the keyword with error or any following keywords.


  @param This           Pointer to the EFI_KEYWORD_HANDLER _PROTOCOL instance.

  @param NameSpaceId    A null-terminated string containing the platform configuration
                        language to search through in the system. If a NULL is passed
                        in, then it is assumed that any platform configuration language
                        with the prefix of "x-UEFI-" are searched.

  @param KeywordString  A null-terminated string in <MultiKeywordRequest> format. If a
                        NULL is passed in the KeywordString field, all of the known
                        keywords in the system for the NameSpaceId specified are
                        returned in the Results field.

  @param Progress       On return, points to a character in the KeywordString. Points
                        to the string's NULL terminator if the request was successful.
                        Points to the most recent '&' before the first failing name / value
                        pair (or the beginning of the string if the failure is in the first
                        name / value pair) if the request was not successful.

  @param ProgressErr    If during the processing of the KeywordString there was a
                        failure, this parameter gives additional information about the
                        possible source of the problem. See the definitions in SetData()
                        for valid value definitions.

  @param Results        A null-terminated string in <MultiKeywordResp> format is returned
                        which has all the values filled in for the keywords in the
                        KeywordString. This is a callee-allocated field, and must be freed
                        by the caller after being used.

  @retval EFI_SUCCESS             The specified action was completed successfully.

  @retval EFI_INVALID_PARAMETER   One or more of the following are TRUE:
                                  1.Progress, ProgressErr, or Results is NULL.
                                  2.Parsing of the KeywordString resulted in an error. See
                                    Progress and ProgressErr for more data.


  @retval EFI_NOT_FOUND           An element of the KeywordString was not found. See
                                  ProgressErr for more data.

  @retval EFI_NOT_FOUND           The NamespaceId specified was not found.  See ProgressErr
                                  for more data.

  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.  See
                                  ProgressErr for more data.

  @retval EFI_ACCESS_DENIED       The action violated system policy.  See ProgressErr for
                                  more data.

  @retval EFI_DEVICE_ERROR        An unexpected system error occurred.  See ProgressErr
                                  for more data.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_CONFIG_KEYWORD_HANDLER_GET_DATA)(
  IN EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL  *This,
  IN CONST EFI_STRING                     NameSpaceId  OPTIONAL,
  IN CONST EFI_STRING                     KeywordString  OPTIONAL,
  OUT EFI_STRING                          *Progress,
  OUT UINT32                              *ProgressErr,
  OUT EFI_STRING                          *Results
  );

///
/// The EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL provides the mechanism
/// to set and get the values associated with a keyword exposed
/// through a x-UEFI- prefixed configuration language namespace
///

struct _EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL {
  EFI_CONFIG_KEYWORD_HANDLER_SET_DATA    SetData;
  EFI_CONFIG_KEYWORD_HANDLER_GET_DATA    GetData;
};

extern EFI_GUID  gEfiConfigKeywordHandlerProtocolGuid;

#endif
