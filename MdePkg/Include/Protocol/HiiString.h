/** @file
  The file provides services to manipulate string data.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in UEFI Specification 2.1.

**/

#ifndef __HII_STRING_H__
#define __HII_STRING_H__

#include <Protocol/HiiFont.h>

#define EFI_HII_STRING_PROTOCOL_GUID \
  { 0xfd96974, 0x23aa, 0x4cdc, { 0xb9, 0xcb, 0x98, 0xd1, 0x77, 0x50, 0x32, 0x2a } }

typedef struct _EFI_HII_STRING_PROTOCOL EFI_HII_STRING_PROTOCOL;

/**
  This function adds the string String to the group of strings owned by PackageList, with the
  specified font information StringFontInfo, and returns a new string id.
  The new string identifier is guaranteed to be unique within the package list.
  That new string identifier is reserved for all languages in the package list.

  @param  This                   A pointer to the EFI_HII_STRING_PROTOCOL instance.
  @param  PackageList            The handle of the package list where this string will
                                 be added.
  @param  StringId               On return, contains the new strings id, which is
                                 unique within PackageList.
  @param  Language               Points to the language for the new string.
  @param  LanguageName           Points to the printable language name to associate
                                 with the passed in  Language field.If LanguageName
                                 is not NULL and the string package header's
                                 LanguageName  associated with a given Language is
                                 not zero, the LanguageName being passed in will
                                 be ignored.
  @param  String                 Points to the new null-terminated string.
  @param  StringFontInfo         Points to the new string's font information or
                                 NULL if the string should have the default system
                                 font, size and style.

  @retval EFI_SUCCESS            The new string was added successfully.
  @retval EFI_NOT_FOUND          The specified PackageList could not be found in
                                 database.
  @retval EFI_OUT_OF_RESOURCES   Could not add the string due to lack of resources.
  @retval EFI_INVALID_PARAMETER  String is NULL, or StringId is NULL, or Language is NULL.
  @retval EFI_INVALID_PARAMETER  The specified StringFontInfo does not exist in
                                 current database.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_HII_NEW_STRING)(
  IN CONST  EFI_HII_STRING_PROTOCOL   *This,
  IN        EFI_HII_HANDLE            PackageList,
  OUT       EFI_STRING_ID             *StringId,
  IN CONST  CHAR8                     *Language,
  IN  CONST CHAR16                    *LanguageName, OPTIONAL
  IN CONST  EFI_STRING                String,
  IN CONST  EFI_FONT_INFO             *StringFontInfo OPTIONAL
);


/**
  This function retrieves the string specified by StringId which is associated
  with the specified PackageList in the language Language and copies it into
  the buffer specified by String.

  @param  This                   A pointer to the EFI_HII_STRING_PROTOCOL instance.
  @param  Language               Points to the language for the retrieved string.
  @param  PackageList            The package list in the HII database to search for
                                 the  specified string.
  @param  StringId               The string's id, which is unique within
                                 PackageList.
  @param  String                 Points to the new null-terminated string.
  @param  StringSize             On entry, points to the size of the buffer pointed
                                 to by  String, in bytes. On return, points to the
                                 length of the string, in bytes.
  @param  StringFontInfo         If not NULL, points to the string's font
                                 information.  It's caller's responsibility to free
                                 this buffer.

  @retval EFI_SUCCESS            The string was returned successfully.
  @retval EFI_NOT_FOUND          The string specified by StringId is not available.
                                 The specified PackageList is not in the database.
  @retval EFI_INVALID_LANGUAGE    The string specified by StringId is available but
                                  not in the specified language.
  @retval EFI_BUFFER_TOO_SMALL   The buffer specified by StringSize is too small to
                                 hold the string.
  @retval EFI_INVALID_PARAMETER   The Language or StringSize was NULL.
  @retval EFI_INVALID_PARAMETER   The value referenced by StringSize was not zero and
                                  String was NULL.
  @retval EFI_OUT_OF_RESOURCES    There were insufficient resources to complete the
                                  request.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_HII_GET_STRING)(
  IN CONST  EFI_HII_STRING_PROTOCOL *This,
  IN CONST  CHAR8                   *Language,
  IN        EFI_HII_HANDLE          PackageList,
  IN        EFI_STRING_ID           StringId,
  OUT       EFI_STRING              String,
  IN OUT    UINTN                   *StringSize,
  OUT       EFI_FONT_INFO           **StringFontInfo OPTIONAL
);

/**
  This function updates the string specified by StringId in the specified PackageList to the text
  specified by String and, optionally, the font information specified by StringFontInfo.

  @param  This                   A pointer to the EFI_HII_STRING_PROTOCOL instance.
  @param  PackageList            The package list containing the strings.
  @param  StringId               The string's id, which is unique within
                                 PackageList.
  @param  Language               Points to the language for the updated string.
  @param  String                 Points to the new null-terminated string.
  @param  StringFontInfo         Points to the string's font information or NULL if
                                 the  string font information is not changed.

  @retval EFI_SUCCESS            The string was updated successfully.
  @retval EFI_NOT_FOUND          The string specified by StringId is not in the
                                 database.
  @retval EFI_INVALID_PARAMETER  The String or Language was NULL.
  @retval EFI_INVALID_PARAMETER  The specified StringFontInfo does not exist in
                                 current database.
  @retval EFI_OUT_OF_RESOURCES   The system is out of resources to accomplish the
                                 task.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_HII_SET_STRING)(
  IN CONST  EFI_HII_STRING_PROTOCOL *This,
  IN        EFI_HII_HANDLE          PackageList,
  IN        EFI_STRING_ID           StringId,
  IN CONST  CHAR8                   *Language,
  IN        EFI_STRING              String,
  IN CONST  EFI_FONT_INFO           *StringFontInfo OPTIONAL
);


/**
  This function returns the list of supported languages.

  @param  This                   A pointer to the EFI_HII_STRING_PROTOCOL instance.
  @param  PackageList            The package list to examine.
  @param  Languages              Points to the buffer to hold the returned
                                 null-terminated ASCII string.
  @param  LanguagesSize          On entry, points to the size of the buffer pointed
                                 to by Languages, in bytes. On return, points to
                                 the length of Languages, in bytes.

  @retval EFI_SUCCESS            The languages were returned successfully.
  @retval EFI_INVALID_PARAMETER  The LanguagesSize was NULL.
  @retval EFI_INVALID_PARAMETER  The value referenced by LanguagesSize is not zero
                                 and Languages is NULL.
  @retval EFI_BUFFER_TOO_SMALL   The LanguagesSize is too small to hold the list of
                                 supported languages. LanguageSize is updated to
                                 contain the required size.
  @retval EFI_NOT_FOUND          Could not find string package in specified
                                 packagelist.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_HII_GET_LANGUAGES)(
  IN CONST  EFI_HII_STRING_PROTOCOL   *This,
  IN        EFI_HII_HANDLE            PackageList,
  IN OUT    CHAR8                     *Languages,
  IN OUT    UINTN                     *LanguagesSize
);


/**
  Each string package has associated with it a single primary language and zero
  or more secondary languages. This routine returns the secondary languages
  associated with a package list.

  @param  This                   A pointer to the EFI_HII_STRING_PROTOCOL instance.
  @param  PackageList            The package list to examine.
  @param  PrimaryLanguage        Points to the null-terminated ASCII string that specifies
                                 the primary language. Languages are specified in the
                                 format specified in Appendix M of the UEFI 2.0 specification.
  @param  SecondaryLanguages     Points to the buffer to hold the returned null-terminated
                                 ASCII string that describes the list of
                                 secondary languages for the specified
                                 PrimaryLanguage. If there are no secondary
                                 languages, the function returns successfully, but
                                 this is set to NULL.
  @param  SecondaryLanguagesSize On entry, points to the size of the buffer pointed
                                 to by SecondaryLanguages, in bytes. On return,
                                 points to the length of SecondaryLanguages in bytes.

  @retval EFI_SUCCESS            Secondary languages were correctly returned.
  @retval EFI_INVALID_PARAMETER  PrimaryLanguage or SecondaryLanguagesSize was NULL.
  @retval EFI_INVALID_PARAMETER  The value referenced by SecondaryLanguagesSize is not
                                 zero and SecondaryLanguages is NULL.
  @retval EFI_BUFFER_TOO_SMALL   The buffer specified by SecondaryLanguagesSize is
                                 too small to hold the returned information.
                                 SecondaryLanguageSize is updated to hold the size of
                                 the buffer required.
  @retval EFI_INVALID_LANGUAGE   The language specified by PrimaryLanguage is not
                                 present in the specified package list.
  @retval EFI_NOT_FOUND          The specified PackageList is not in the Database.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_HII_GET_2ND_LANGUAGES)(
  IN CONST  EFI_HII_STRING_PROTOCOL   *This,
  IN        EFI_HII_HANDLE            PackageList,
  IN CONST  CHAR8                     *PrimaryLanguage,
  IN OUT    CHAR8                     *SecondaryLanguages,
  IN OUT    UINTN                     *SecondaryLanguagesSize
);


///
/// Services to manipulate the string.
///
struct _EFI_HII_STRING_PROTOCOL {
  EFI_HII_NEW_STRING        NewString;
  EFI_HII_GET_STRING        GetString;
  EFI_HII_SET_STRING        SetString;
  EFI_HII_GET_LANGUAGES     GetLanguages;
  EFI_HII_GET_2ND_LANGUAGES GetSecondaryLanguages;
};


extern EFI_GUID gEfiHiiStringProtocolGuid;

#endif

