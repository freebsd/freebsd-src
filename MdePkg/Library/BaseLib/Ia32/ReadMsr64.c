/** @file
  AsmReadMsr64 function

  Copyright (c) 2006 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/RegisterFilterLib.h>

/**
  Returns a 64-bit Machine Specific Register(MSR).

  Reads and returns the 64-bit MSR specified by Index. No parameter checking is
  performed on Index, and some Index values may cause CPU exceptions. The
  caller must either guarantee that Index is valid, or the caller must set up
  exception handlers to catch the exceptions. This function is only available
  on IA-32 and x64.

  @param  Index The 32-bit MSR index to read.

  @return The value of the MSR identified by Index.

**/
UINT64
AsmReadMsr64Internal (
  IN UINT32  Index
  )
{
  _asm {
    mov     ecx, Index
    rdmsr
  }
}

/**
  Returns a 64-bit Machine Specific Register(MSR).

  Reads and returns the 64-bit MSR specified by Index. No parameter checking is
  performed on Index, and some Index values may cause CPU exceptions. The
  caller must either guarantee that Index is valid, or the caller must set up
  exception handlers to catch the exceptions. This function is only available
  on IA-32 and x64.

  @param  Index The 32-bit MSR index to read.

  @return The value of the MSR identified by Index.

**/
UINT64
EFIAPI
AsmReadMsr64 (
  IN UINT32  Index
  )
{
  UINT64   Value;
  BOOLEAN  Flag;

  Flag = FilterBeforeMsrRead (Index, &Value);
  if (Flag) {
    Value = AsmReadMsr64Internal (Index);
  }

  FilterAfterMsrRead (Index, &Value);

  return Value;
}
