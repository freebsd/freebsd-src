/** @file
  SMM Sx Dispatch Protocol as defined in PI 1.2 Specification
  Volume 4 System Management Mode Core Interface.

  Provides the parent dispatch service for a given Sx-state source generator.

  Copyright (c) 2009 - 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMM_SX_DISPATCH2_H_
#define _SMM_SX_DISPATCH2_H_

#include <Protocol/MmSxDispatch.h>

#define EFI_SMM_SX_DISPATCH2_PROTOCOL_GUID  EFI_MM_SX_DISPATCH_PROTOCOL_GUID

///
/// The dispatch function's context
///
typedef EFI_MM_SX_REGISTER_CONTEXT EFI_SMM_SX_REGISTER_CONTEXT;

typedef EFI_MM_SX_DISPATCH_PROTOCOL EFI_SMM_SX_DISPATCH2_PROTOCOL;

typedef EFI_MM_SX_REGISTER EFI_SMM_SX_REGISTER2;

typedef EFI_MM_SX_UNREGISTER EFI_SMM_SX_UNREGISTER2;

extern EFI_GUID  gEfiSmmSxDispatch2ProtocolGuid;

#endif
