/** @file
  EFI Shell protocol as defined in the UEFI Shell 2.0 specification.

  Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EFI_SHELL_PARAMETERS_PROTOCOL_H__
#define __EFI_SHELL_PARAMETERS_PROTOCOL_H__

#include <Protocol/Shell.h>

#define EFI_SHELL_PARAMETERS_PROTOCOL_GUID \
  { \
  0x752f3136, 0x4e16, 0x4fdc, { 0xa2, 0x2a, 0xe5, 0xf4, 0x68, 0x12, 0xf4, 0xca } \
  }

typedef struct _EFI_SHELL_PARAMETERS_PROTOCOL {
  ///
  /// Points to an Argc-element array of points to NULL-terminated strings containing
  /// the command-line parameters. The first entry in the array is always the full file
  /// path of the executable. Any quotation marks that were used to preserve
  /// whitespace have been removed.
  ///
  CHAR16               **Argv;

  ///
  /// The number of elements in the Argv array.
  ///
  UINTN                Argc;

  ///
  /// The file handle for the standard input for this executable. This may be different
  /// from the ConInHandle in EFI_SYSTEM_TABLE.
  ///
  SHELL_FILE_HANDLE    StdIn;

  ///
  /// The file handle for the standard output for this executable. This may be different
  /// from the ConOutHandle in EFI_SYSTEM_TABLE.
  ///
  SHELL_FILE_HANDLE    StdOut;

  ///
  /// The file handle for the standard error output for this executable. This may be
  /// different from the StdErrHandle in EFI_SYSTEM_TABLE.
  ///
  SHELL_FILE_HANDLE    StdErr;
} EFI_SHELL_PARAMETERS_PROTOCOL;

extern EFI_GUID  gEfiShellParametersProtocolGuid;

#endif
