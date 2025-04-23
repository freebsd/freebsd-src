/** @file
  This library provides stack cookie checking functions for symbols inserted by the compiler. This header
  is not intended to be used directly by modules, but rather defines the expected interfaces to each supported
  compiler, so that if the compiler interface is updated it is easier to track.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef STACK_CHECK_LIB_H_
#define STACK_CHECK_LIB_H_

#include <Base.h>

#if defined (__GNUC__) || defined (__clang__)

// The __stack_chk_guard is a random value placed on the stack between the stack variables
// and the return address so that continuously writing past the stack variables will cause
// the stack cookie to be overwritten. Before the function returns, the stack cookie value
// will be checked and if there is a mismatch then StackCheckLib handles the failure.
extern VOID  *__stack_chk_guard;

/**
  Called when a stack cookie check fails. The return address is the failing address.

**/
VOID
EFIAPI
__stack_chk_fail (
  VOID
  );

#elif defined (_MSC_VER)

// The __security_cookie is a random value placed on the stack between the stack variables
// and the return address so that continuously writing past the stack variables will cause
// the stack cookie to be overwritten. Before the function returns, the stack cookie value
// will be checked and if there is a mismatch then StackCheckLib handles the failure.
extern VOID  *__security_cookie;

/**
  Called when a buffer check fails. This functionality is dependent on MSVC
  C runtime libraries and so is unsupported in UEFI.

**/
VOID
EFIAPI
__report_rangecheckfailure (
  VOID
  );

/**
   The GS handler is for checking the stack cookie during SEH or
   EH exceptions and is unsupported in UEFI.

**/
VOID
EFIAPI
__GSHandlerCheck (
  VOID
  );

/**
   Checks the stack cookie value against __security_cookie and calls the
   stack cookie failure handler if there is a mismatch.

   @param UINTN  CheckValue The value to check against __security_cookie

**/
VOID
EFIAPI
__security_check_cookie (
  UINTN  CheckValue
  );

#endif // Compiler type

#endif // STACK_CHECK_LIB_H_
