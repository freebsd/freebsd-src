/** @file
  This file is empty to allow host applications
  to use the MSVC C runtime lib that provides
  stack cookie definitions without breaking the
  build.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

extern VOID  *__security_cookie;
