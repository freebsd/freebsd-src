/** @file
  AsmCpuidEx function.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Retrieves CPUID information using an extended leaf identifier.

  Executes the CPUID instruction with EAX set to the value specified by Index
  and ECX set to the value specified by SubIndex. This function always returns
  Index. This function is only available on IA-32 and x64.

  If Eax is not NULL, then the value of EAX after CPUID is returned in Eax.
  If Ebx is not NULL, then the value of EBX after CPUID is returned in Ebx.
  If Ecx is not NULL, then the value of ECX after CPUID is returned in Ecx.
  If Edx is not NULL, then the value of EDX after CPUID is returned in Edx.

  @param  Index         The 32-bit value to load into EAX prior to invoking the
                        CPUID instruction.
  @param  SubIndex      The 32-bit value to load into ECX prior to invoking the
                        CPUID instruction.
  @param  RegisterEax   A pointer to the 32-bit EAX value returned by the CPUID
                        instruction. This is an optional parameter that may be
                        NULL.
  @param  RegisterEbx   A pointer to the 32-bit EBX value returned by the CPUID
                        instruction. This is an optional parameter that may be
                        NULL.
  @param  RegisterEcx   A pointer to the 32-bit ECX value returned by the CPUID
                        instruction. This is an optional parameter that may be
                        NULL.
  @param  RegisterEdx   A pointer to the 32-bit EDX value returned by the CPUID
                        instruction. This is an optional parameter that may be
                        NULL.

  @return Index.

**/
UINT32
EFIAPI
AsmCpuidEx (
  IN      UINT32  Index,
  IN      UINT32  SubIndex,
  OUT     UINT32  *RegisterEax   OPTIONAL,
  OUT     UINT32  *RegisterEbx   OPTIONAL,
  OUT     UINT32  *RegisterEcx   OPTIONAL,
  OUT     UINT32  *RegisterEdx   OPTIONAL
  )
{
  _asm {
    mov     eax, Index
    mov     ecx, SubIndex
    cpuid
    push    ecx
    mov     ecx, RegisterEax
    jecxz   SkipEax
    mov     [ecx], eax
SkipEax:
    mov     ecx, RegisterEbx
    jecxz   SkipEbx
    mov     [ecx], ebx
SkipEbx:
    pop     eax
    mov     ecx, RegisterEcx
    jecxz   SkipEcx
    mov     [ecx], eax
SkipEcx:
    mov     ecx, RegisterEdx
    jecxz   SkipEdx
    mov     [ecx], edx
SkipEdx:
    mov     eax, Index
  }
}
