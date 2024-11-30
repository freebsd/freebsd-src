/** @file
  AsmMonitor function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Sets up a monitor buffer that is used by AsmMwait().

  Executes a MONITOR instruction with the register state specified by Eax, Ecx
  and Edx. Returns Eax. This function is only available on IA-32 and x64.

  @param  RegisterEax The value to load into EAX or RAX before executing the MONITOR
                      instruction.
  @param  RegisterEcx The value to load into ECX or RCX before executing the MONITOR
                      instruction.
  @param  RegisterEdx The value to load into EDX or RDX before executing the MONITOR
                      instruction.

  @return RegisterEax

**/
UINTN
EFIAPI
AsmMonitor (
  IN      UINTN  RegisterEax,
  IN      UINTN  RegisterEcx,
  IN      UINTN  RegisterEdx
  )
{
  _asm {
    mov     eax, RegisterEax
    mov     ecx, RegisterEcx
    mov     edx, RegisterEdx
    _emit   0x0f             // monitor
    _emit   0x01
    _emit   0xc8
  }
}
