/** @file
  InterLockedIncrement function

  Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




/**
  Performs an atomic increment of an 32-bit unsigned integer.

  Performs an atomic increment of the 32-bit unsigned integer specified by
  Value and returns the incremented value. The increment operation must be
  performed using MP safe mechanisms. The state of the return value is not
  guaranteed to be MP safe.

  @param  Value A pointer to the 32-bit value to increment.

  @return The incremented value.

**/
UINT32
EFIAPI
InternalSyncIncrement (
  IN      volatile UINT32           *Value
  )
{
  _asm {
    mov     eax, Value
    lock    inc     dword ptr [eax]
    mov     eax, [eax]
  }
}

