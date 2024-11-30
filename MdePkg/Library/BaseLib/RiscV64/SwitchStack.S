//------------------------------------------------------------------------------
//
// InternalSwitchStackAsm for RISC-V
//
// Copyright (c) 2023, Bosc Corporation. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------
.align 3

#/**
#
# This allows the caller to switch the stack and goes to the new entry point
#
# @param      Context      Parameter to pass in
# @param      Context2     Parameter2 to pass in
# @param      EntryPoint   The pointer to the location to enter
# @param      NewStack     New Location of the stack
#
# @return     Nothing. Goes to the Entry Point passing in the new parameters
#
#**/
#VOID
#EFIAPI
#InternalSwitchStackAsm (
#  VOID  *Context,
#  VOID  *Context2,
#  SWITCH_STACK_ENTRY_POINT EntryPoint,
#  VOID  *NewStack
#  );
#
    .globl InternalSwitchStackAsm
InternalSwitchStackAsm:
  mv ra, a2
  mv sp, a3
  ret
