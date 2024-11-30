/** @file
  AsmMwait function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Executes an MWAIT instruction.

  Executes an MWAIT instruction with the register state specified by Eax and
  Ecx. Returns Eax. This function is only available on IA-32 and x64.

  @param  RegisterEax The value to load into EAX or RAX before executing the MONITOR
                      instruction.
  @param  RegisterEcx The value to load into ECX or RCX before executing the MONITOR
                      instruction.

  @return RegisterEax

**/
UINTN
EFIAPI
AsmMwait (
  IN      UINTN  RegisterEax,
  IN      UINTN  RegisterEcx
  )
{
  _asm {
    mov     eax, RegisterEax
    mov     ecx, RegisterEcx
    _emit   0x0f              // mwait
    _emit   0x01
    _emit   0xC9
  }
}
