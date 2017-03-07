/** @file
  CpuBreakpoint function.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

/**
  Microsoft Visual Studio 7.1 Function Prototypes for I/O Intrinsics.
**/

unsigned __int64 __readmsr (int register);

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
  return __readmsr (Index);
}

