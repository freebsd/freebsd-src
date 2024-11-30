/** @file
  CpuBreakpoint function.

  Copyright (c) 2006 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Microsoft Visual Studio 7.1 Function Prototypes for I/O Intrinsics.
**/

#include <Library/RegisterFilterLib.h>

unsigned __int64
__readmsr (
  int register
  );

#pragma intrinsic(__readmsr)

/**
  Read data to MSR.

  @param  Index                Register index of MSR.

  @return Value read from MSR.

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
    Value = __readmsr (Index);
  }

  FilterAfterMsrRead (Index, &Value);

  return Value;
}
