/** @file
 Base Stack Check library for GCC/clang.

 Use -fstack-protector-all compiler flag to make the compiler insert the
 __stack_chk_guard "canary" value into the stack and check the value prior
 to exiting the function. If the "canary" is overwritten __stack_chk_fail()
 is called. This is GCC specific code.

 Copyright (c) 2012, Apple Inc. All rights reserved.<BR>
 SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

/// "canary" value that is inserted by the compiler into the stack frame.
VOID *__stack_chk_guard = (VOID*)0x0AFF;

// If ASLR was enabled we could use
//void (*__stack_chk_guard)(void) = __stack_chk_fail;

/**
 Error path for compiler generated stack "canary" value check code. If the
 stack canary has been overwritten this function gets called on exit of the
 function.
**/
VOID
__stack_chk_fail (
 VOID
 )
{
  UINT8 DebugPropertyMask;

  DEBUG ((DEBUG_ERROR, "STACK FAULT: Buffer Overflow in function %a.\n", __builtin_return_address(0)));

  //
  // Generate a Breakpoint, DeadLoop, or NOP based on PCD settings even if
  // BaseDebugLibNull is in use.
  //
  DebugPropertyMask = PcdGet8 (PcdDebugPropertyMask);
  if ((DebugPropertyMask & DEBUG_PROPERTY_ASSERT_BREAKPOINT_ENABLED) != 0) {
    CpuBreakpoint ();
  } else if ((DebugPropertyMask & DEBUG_PROPERTY_ASSERT_DEADLOOP_ENABLED) != 0) {
   CpuDeadLoop ();
  }
}
