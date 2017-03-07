/** @file
  AsmReadLdtr function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




/**
  Reads the current Local Descriptor Table Register(LDTR) selector.

  Reads and returns the current 16-bit LDTR descriptor value. This function is
  only available on IA-32 and x64.

  @return The current selector of LDT.

**/
UINT16
EFIAPI
AsmReadLdtr (
  VOID
  )
{
  _asm {
    sldt    ax
  }
}

