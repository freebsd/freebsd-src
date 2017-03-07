/** @file
  Base Library CPU Functions for all architectures.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/



#include <Base.h>
#include <Library/BaseLib.h>

/**
  Executes an infinite loop.

  Forces the CPU to execute an infinite loop. A debugger may be used to skip
  past the loop and the code that follows the loop must execute properly. This
  implies that the infinite loop must not cause the code that follow it to be
  optimized away.

**/
VOID
EFIAPI
CpuDeadLoop (
  VOID
  )
{
  volatile UINTN  Index;

  for (Index = 0; Index == 0;);
}
