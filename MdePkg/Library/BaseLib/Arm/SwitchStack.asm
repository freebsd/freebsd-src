;------------------------------------------------------------------------------ 
;
; Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------
  
    EXPORT InternalSwitchStackAsm
            
    AREA   Switch_Stack, CODE, READONLY
  
;/**
;  This allows the caller to switch the stack and goes to the new entry point
;
; @param      EntryPoint   The pointer to the location to enter
; @param      Context      Parameter to pass in
; @param      Context2     Parameter2 to pass in
; @param      NewStack     New Location of the stack
;
; @return     Nothing. Goes to the Entry Point passing in the new parameters
;
;**/
;VOID
;EFIAPI
;InternalSwitchStackAsm (
;  SWITCH_STACK_ENTRY_POINT EntryPoint,
;  VOID  *Context,
;  VOID  *Context2,
;  VOID  *NewStack
;  );
;
InternalSwitchStackAsm
    MOV   LR, R0
    MOV   SP, R3
    MOV   R0, R1
    MOV   R1, R2
    BX    LR
    END
