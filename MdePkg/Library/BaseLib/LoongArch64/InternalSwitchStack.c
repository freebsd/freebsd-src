/** @file
  SwitchStack() function for LoongArch.

  Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "BaseLibInternals.h"

UINTN
EFIAPI
InternalSwitchStackAsm (
  IN     BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
  );

/**
  Transfers control to a function starting with a new stack.

  Transfers control to the function specified by EntryPoint using the
  new stack specified by NewStack and passing in the parameters specified
  by Context1 and Context2.  Context1 and Context2 are optional and may
  be NULL.  The function EntryPoint must never return.

  If EntryPoint is NULL, then ASSERT().
  If NewStack is NULL, then ASSERT().

  @param[in]  EntryPoint  A pointer to function to call with the new stack.
  @param[in]  Context1    A pointer to the context to pass into the EntryPoint
                      function.
  @param[in]  Context2    A pointer to the context to pass into the EntryPoint
                      function.
  @param[in]  NewStack    A pointer to the new stack to use for the EntryPoint
                      function.
  @param[in]  Marker      VA_LIST marker for the variable argument list.

**/
VOID
EFIAPI
InternalSwitchStack (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1   OPTIONAL,
  IN      VOID                      *Context2   OPTIONAL,
  IN      VOID                      *NewStack,
  IN      VA_LIST                   Marker
  )

{
  BASE_LIBRARY_JUMP_BUFFER  JumpBuffer;

  JumpBuffer.RA                      = (UINTN)EntryPoint;
  JumpBuffer.SP                      = (UINTN)NewStack - sizeof (VOID *);
  JumpBuffer.SP                     -= sizeof (Context1) + sizeof (Context2);
  ((VOID **)(UINTN)JumpBuffer.SP)[0] = Context1;
  ((VOID **)(UINTN)JumpBuffer.SP)[1] = Context2;

  InternalSwitchStackAsm (&JumpBuffer);
}
