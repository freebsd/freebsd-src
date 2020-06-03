/** @file
  CpuBreakpoint function.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/




/**
  Microsoft Visual Studio 7.1 Function Prototypes for I/O Intrinsics.
**/

void __debugbreak (VOID);

#pragma intrinsic(__debugbreak)

/**
  Generates a breakpoint on the CPU.

  Generates a breakpoint on the CPU. The breakpoint must be implemented such
  that code can resume normal execution after the breakpoint.

**/
VOID
EFIAPI
CpuBreakpoint (
  VOID
  )
{
  __debugbreak ();
}

