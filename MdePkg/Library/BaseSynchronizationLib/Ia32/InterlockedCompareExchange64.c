/** @file
  InterlockedCompareExchange64 function

  Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/




/**
  Performs an atomic compare exchange operation on a 64-bit unsigned integer.

  Performs an atomic compare exchange operation on the 64-bit unsigned integer specified
  by Value.  If Value is equal to CompareValue, then Value is set to ExchangeValue and
  CompareValue is returned.  If Value is not equal to CompareValue, then Value is returned.
  The compare exchange operation must be performed using MP safe mechanisms.

  @param  Value         A pointer to the 64-bit value for the compare exchange
                        operation.
  @param  CompareValue  A 64-bit value used in a compare operation.
  @param  ExchangeValue A 64-bit value used in an exchange operation.

  @return The original *Value before exchange.

**/
UINT64
EFIAPI
InternalSyncCompareExchange64 (
  IN      volatile UINT64           *Value,
  IN      UINT64                    CompareValue,
  IN      UINT64                    ExchangeValue
  )
{
  _asm {
    mov     esi, Value
    mov     eax, dword ptr [CompareValue + 0]
    mov     edx, dword ptr [CompareValue + 4]
    mov     ebx, dword ptr [ExchangeValue + 0]
    mov     ecx, dword ptr [ExchangeValue + 4]
    lock    cmpxchg8b   qword ptr [esi]
  }
}
