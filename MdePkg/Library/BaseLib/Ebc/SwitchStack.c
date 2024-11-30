/** @file
  Switch Stack functions.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Transfers control to a function starting with a new stack.

  Transfers control to the function specified by EntryPoint using the
  new stack specified by NewStack and passing in the parameters specified
  by Context1 and Context2.  Context1 and Context2 are optional and may
  be NULL.  The function EntryPoint must never return.
  Marker will be ignored on IA-32, x64, and EBC.
  IPF CPUs expect one additional parameter of type VOID * that specifies
  the new backing store pointer.

  If EntryPoint is NULL, then ASSERT().
  If NewStack is NULL, then ASSERT().

  @param  EntryPoint  A pointer to function to call with the new stack.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function.
  @param  Marker      A VA_LIST marker for the variable argument list.

**/
VOID
EFIAPI
InternalSwitchStack (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1    OPTIONAL,
  IN      VOID                      *Context2    OPTIONAL,
  IN      VOID                      *NewStack,
  IN      VA_LIST                   Marker
  )

{
  //
  // This version of this function does not actually change the stack pointer
  // This is to support compilation of CPU types that do not support assemblers
  // such as EBC
  //
  EntryPoint (Context1, Context2);
}
