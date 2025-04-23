/** @file
  Defines the stack cookie variable for GCC, Clang and MSVC compilers.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/StackCheckLib.h>

VOID  *__security_cookie = (VOID *)(UINTN)0x0;
