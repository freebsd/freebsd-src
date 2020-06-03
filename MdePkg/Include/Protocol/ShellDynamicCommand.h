/** @file
  EFI Shell Dynamic Command registration protocol

  (C) Copyright 2012-2014 Hewlett-Packard Development Company, L.P.<BR>
  Copyright (c) 2016 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL_H__
#define __EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL_H__

#include <Protocol/Shell.h>
#include <Protocol/ShellParameters.h>

// {3C7200E9-005F-4EA4-87DE-A3DFAC8A27C3}
#define EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL_GUID \
  { \
  0x3c7200e9, 0x005f, 0x4ea4, { 0x87, 0xde, 0xa3, 0xdf, 0xac, 0x8a, 0x27, 0xc3 } \
  }


//
// Define for forward reference.
//
typedef struct _EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL;


/**
  This is the shell command handler function pointer callback type.  This
  function handles the command when it is invoked in the shell.

  @param[in] This                   The instance of the EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL.
  @param[in] SystemTable            The pointer to the system table.
  @param[in] ShellParameters        The parameters associated with the command.
  @param[in] Shell                  The instance of the shell protocol used in the context
                                    of processing this command.

  @return EFI_SUCCESS               the operation was sucessful
  @return other                     the operation failed.
**/
typedef
SHELL_STATUS
(EFIAPI * SHELL_COMMAND_HANDLER)(
  IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL    *This,
  IN EFI_SYSTEM_TABLE                      *SystemTable,
  IN EFI_SHELL_PARAMETERS_PROTOCOL         *ShellParameters,
  IN EFI_SHELL_PROTOCOL                    *Shell
  );

/**
  This is the command help handler function pointer callback type.  This
  function is responsible for displaying help information for the associated
  command.

  @param[in] This                   The instance of the EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL.
  @param[in] Language               The pointer to the language string to use.

  @return string                    Pool allocated help string, must be freed by caller
**/
typedef
CHAR16*
(EFIAPI * SHELL_COMMAND_GETHELP)(
  IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL    *This,
  IN CONST CHAR8                           *Language
  );

/// EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL protocol structure.
struct _EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL {

  CONST CHAR16           *CommandName;
  SHELL_COMMAND_HANDLER  Handler;
  SHELL_COMMAND_GETHELP  GetHelp;

};

extern EFI_GUID gEfiShellDynamicCommandProtocolGuid;

#endif
