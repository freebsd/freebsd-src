/** @file
  InterlockedCompareExchange32 function

  Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




/**
  Performs an atomic compare exchange operation on a 32-bit unsigned integer.

  Performs an atomic compare exchange operation on the 32-bit unsigned integer
  specified by Value.  If Value is equal to CompareValue, then Value is set to
  ExchangeValue and CompareValue is returned.  If Value is not equal to CompareValue,
  then Value is returned.  The compare exchange operation must be performed using
  MP safe mechanisms.

  @param  Value         A pointer to the 32-bit value for the compare exchange
                        operation.
  @param  CompareValue  32-bit value used in compare operation.
  @param  ExchangeValue 32-bit value used in exchange operation.

  @return The original *Value before exchange.

**/
UINT32
EFIAPI
InternalSyncCompareExchange32 (
  IN      volatile UINT32           *Value,
  IN      UINT32                    CompareValue,
  IN      UINT32                    ExchangeValue
  )
{
  _asm {
    mov     ecx, Value
    mov     eax, CompareValue
    mov     edx, ExchangeValue
    lock    cmpxchg [ecx], edx
  }
}

