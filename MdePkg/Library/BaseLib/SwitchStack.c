/** @file
  Switch Stack functions.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Transfers control to a function starting with a new stack.

  Transfers control to the function specified by EntryPoint using the
  new stack specified by NewStack and passing in the parameters specified
  by Context1 and Context2.  Context1 and Context2 are optional and may
  be NULL.  The function EntryPoint must never return.  This function
  supports a variable number of arguments following the NewStack parameter.
  These additional arguments are ignored on IA-32, x64, and EBC.
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
  @param  ...         This variable argument list is ignored for IA32, x64, and EBC.
                      For IPF, this variable argument list is expected to contain
                      a single parameter of type VOID * that specifies the new backing
                      store pointer.


**/
VOID
EFIAPI
SwitchStack (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1   OPTIONAL,
  IN      VOID                      *Context2   OPTIONAL,
  IN      VOID                      *NewStack,
  ...
  )
{
  VA_LIST  Marker;

  ASSERT (EntryPoint != NULL);
  ASSERT (NewStack != NULL);

  //
  // New stack must be aligned with CPU_STACK_ALIGNMENT
  //
  ASSERT (((UINTN)NewStack & (CPU_STACK_ALIGNMENT - 1)) == 0);

  VA_START (Marker, NewStack);

  InternalSwitchStack (EntryPoint, Context1, Context2, NewStack, Marker);

  VA_END (Marker);

  //
  // InternalSwitchStack () will never return
  //
  ASSERT (FALSE);
}
