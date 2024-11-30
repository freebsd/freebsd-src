/** @file
  Defines file-path manipulation functions.

  Copyright (c) 2011 - 2017, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2018, Dell Technologies. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include  <Library/BaseMemoryLib.h>
#include  <Library/BaseLib.h>

/**
  Removes the last directory or file entry in a path. For a path which is
  like L"fs0:startup.nsh", it's converted to L"fs0:".

  @param[in,out] Path     A pointer to the path to modify.

  @retval FALSE     Nothing was found to remove.
  @retval TRUE      A directory or file was removed.
**/
BOOLEAN
EFIAPI
PathRemoveLastItem (
  IN OUT CHAR16  *Path
  )
{
  CHAR16  *Walker;
  CHAR16  *LastSlash;

  //
  // get directory name from path... ('chop' off extra)
  //
  for ( Walker = Path, LastSlash = NULL
        ; Walker != NULL && *Walker != CHAR_NULL
        ; Walker++
        )
  {
    if ((*Walker == L'\\') && (*(Walker + 1) != CHAR_NULL)) {
      LastSlash = Walker+1;
    } else if ((*Walker == L':') && (*(Walker + 1) != L'\\') && (*(Walker + 1) != CHAR_NULL)) {
      LastSlash = Walker+1;
    }
  }

  if (LastSlash != NULL) {
    *LastSlash = CHAR_NULL;
    return (TRUE);
  }

  return (FALSE);
}

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
CHAR16 *
EFIAPI
PathCleanUpDirectories (
  IN CHAR16  *Path
  )
{
  CHAR16  *TempString;

  if (Path == NULL) {
    return NULL;
  }

  //
  // Replace the '/' with '\'
  //
  for (TempString = Path; *TempString != CHAR_NULL; TempString++) {
    if (*TempString == L'/') {
      *TempString = L'\\';
    }
  }

  //
  // Replace the "\\" with "\"
  //
  while ((TempString = StrStr (Path, L"\\\\")) != NULL) {
    CopyMem (TempString, TempString + 1, StrSize (TempString + 1));
  }

  //
  // Remove all the "\.". E.g.: fs0:\abc\.\def\.
  //
  while ((TempString = StrStr (Path, L"\\.\\")) != NULL) {
    CopyMem (TempString, TempString + 2, StrSize (TempString + 2));
  }

  if ((StrLen (Path) >= 2) && (StrCmp (Path + StrLen (Path) - 2, L"\\.") == 0)) {
    Path[StrLen (Path) - 1] = CHAR_NULL;
  }

  //
  // Remove all the "\..". E.g.: fs0:\abc\..\def\..
  //
  while (((TempString = StrStr (Path, L"\\..")) != NULL) &&
         ((*(TempString + 3) == L'\\') || (*(TempString + 3) == CHAR_NULL))
         )
  {
    *(TempString + 1) = CHAR_NULL;
    PathRemoveLastItem (Path);
    if (*(TempString + 3) != CHAR_NULL) {
      CopyMem (Path + StrLen (Path), TempString + 4, StrSize (TempString + 4));
    }
  }

  return Path;
}
