/** @file
  AsmReadPmc function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

/**
  Reads the current value of a Performance Counter (PMC).

  Reads and returns the current value of performance counter specified by
  Index. This function is only available on IA-32 and x64.

  @param  Index The 32-bit Performance Counter index to read.

  @return The value of the PMC specified by Index.

**/
UINT64
EFIAPI
AsmReadPmc (
  IN UINT32   Index
  )
{
  _asm {
    mov     ecx, Index
    rdpmc
  }
}

