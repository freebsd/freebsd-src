/** @file
  AsmWriteMsr64 function

  Copyright (c) 2006 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/RegisterFilterLib.h>

/**
  Writes a 64-bit value to a Machine Specific Register(MSR), and returns the
  value.

  Writes the 64-bit value specified by Value to the MSR specified by Index. The
  64-bit value written to the MSR is returned. No parameter checking is
  performed on Index or Value, and some of these may cause CPU exceptions. The
  caller must either guarantee that Index and Value are valid, or the caller
  must establish proper exception handlers. This function is only available on
  IA-32 and x64.

  @param  Index The 32-bit MSR index to write.
  @param  Value The 64-bit value to write to the MSR.

  @return Value

**/
UINT64
EFIAPI
AsmWriteMsr64 (
  IN UINT32  Index,
  IN UINT64  Value
  )
{
  BOOLEAN  Flag;

  Flag = FilterBeforeMsrWrite (Index, &Value);
  if (Flag) {
    _asm {
      mov     edx, dword ptr [Value + 4]
      mov     eax, dword ptr [Value + 0]
      mov     ecx, Index
      wrmsr
    }
  }

  FilterAfterMsrWrite (Index, &Value);

  return Value;
}
