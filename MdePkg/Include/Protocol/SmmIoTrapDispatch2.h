/** @file
  SMM IO Trap Dispatch2 Protocol as defined in PI 1.1 Specification
  Volume 4 System Management Mode Core Interface.

  This protocol provides a parent dispatch service for IO trap SMI sources.

  Copyright (c) 2009 - 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This protocol is from PI Version 1.1.

**/

#ifndef _SMM_IO_TRAP_DISPATCH2_H_
#define _SMM_IO_TRAP_DISPATCH2_H_

#include <Protocol/MmIoTrapDispatch.h>

#define EFI_SMM_IO_TRAP_DISPATCH2_PROTOCOL_GUID  EFI_MM_IO_TRAP_DISPATCH_PROTOCOL_GUID

///
/// IO Trap valid types
///
typedef EFI_MM_IO_TRAP_DISPATCH_TYPE EFI_SMM_IO_TRAP_DISPATCH_TYPE;

///
/// IO Trap context structure containing information about the
/// IO trap event that should invoke the handler
///
typedef EFI_MM_IO_TRAP_REGISTER_CONTEXT EFI_SMM_IO_TRAP_REGISTER_CONTEXT;

///
/// IO Trap context structure containing information about the IO trap that occurred
///
typedef EFI_MM_IO_TRAP_CONTEXT EFI_SMM_IO_TRAP_CONTEXT;

typedef EFI_MM_IO_TRAP_DISPATCH_PROTOCOL EFI_SMM_IO_TRAP_DISPATCH2_PROTOCOL;

typedef EFI_MM_IO_TRAP_DISPATCH_REGISTER   EFI_SMM_IO_TRAP_DISPATCH2_REGISTER;

typedef EFI_MM_IO_TRAP_DISPATCH_UNREGISTER EFI_SMM_IO_TRAP_DISPATCH2_UNREGISTER;

extern EFI_GUID gEfiSmmIoTrapDispatch2ProtocolGuid;

#endif

