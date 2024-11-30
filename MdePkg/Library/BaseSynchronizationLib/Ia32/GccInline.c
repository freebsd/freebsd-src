/** @file
  GCC inline implementation of BaseSynchronizationLib processor specific functions.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Performs an atomic increment of an 32-bit unsigned integer.

  Performs an atomic increment of the 32-bit unsigned integer specified by
  Value and returns the incremented value. The increment operation must be
  performed using MP safe mechanisms.

  @param  Value A pointer to the 32-bit value to increment.

  @return The incremented value.

**/
UINT32
EFIAPI
InternalSyncIncrement (
  IN      volatile UINT32  *Value
  )
{
  UINT32  Result;

  __asm__ __volatile__ (
    "movl    $1, %%eax  \n\t"
    "lock               \n\t"
    "xadd    %%eax, %1  \n\t"
    "inc     %%eax      \n\t"
    : "=&a" (Result),         // %0
      "+m" (*Value)           // %1
    :                         // no inputs that aren't also outputs
    : "memory",
      "cc"
  );

  return Result;
}

/**
  Performs an atomic decrement of an 32-bit unsigned integer.

  Performs an atomic decrement of the 32-bit unsigned integer specified by
  Value and returns the decremented value. The decrement operation must be
  performed using MP safe mechanisms.

  @param  Value A pointer to the 32-bit value to decrement.

  @return The decremented value.

**/
UINT32
EFIAPI
InternalSyncDecrement (
  IN      volatile UINT32  *Value
  )
{
  UINT32  Result;

  __asm__ __volatile__ (
    "movl    $-1, %%eax  \n\t"
    "lock                \n\t"
    "xadd    %%eax, %1   \n\t"
    "dec     %%eax       \n\t"
    : "=&a" (Result),          // %0
      "+m" (*Value)            // %1
    :                          // no inputs that aren't also outputs
    : "memory",
      "cc"
  );

  return Result;
}

/**
  Performs an atomic compare exchange operation on a 16-bit unsigned integer.

  Performs an atomic compare exchange operation on the 16-bit unsigned integer
  specified by Value.  If Value is equal to CompareValue, then Value is set to
  ExchangeValue and CompareValue is returned.  If Value is not equal to CompareValue,
  then Value is returned.  The compare exchange operation must be performed using
  MP safe mechanisms.


  @param  Value         A pointer to the 16-bit value for the compare exchange
                        operation.
  @param  CompareValue  16-bit value used in compare operation.
  @param  ExchangeValue 16-bit value used in exchange operation.

  @return The original *Value before exchange.

**/
UINT16
EFIAPI
InternalSyncCompareExchange16 (
  IN OUT volatile  UINT16  *Value,
  IN      UINT16           CompareValue,
  IN      UINT16           ExchangeValue
  )
{
  __asm__ __volatile__ (
    "lock                 \n\t"
    "cmpxchgw    %2, %1   \n\t"
    : "+a" (CompareValue),      // %0
      "+m" (*Value)             // %1
    : "q"  (ExchangeValue)      // %2
    : "memory",
      "cc"
  );

  return CompareValue;
}

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
  IN OUT volatile  UINT32  *Value,
  IN      UINT32           CompareValue,
  IN      UINT32           ExchangeValue
  )
{
  __asm__ __volatile__ (
    "lock                 \n\t"
    "cmpxchgl    %2, %1   \n\t"
    : "+a" (CompareValue),      // %0
      "+m" (*Value)             // %1
    : "q"  (ExchangeValue)      // %2
    : "memory",
      "cc"
  );

  return CompareValue;
}

/**
  Performs an atomic compare exchange operation on a 64-bit unsigned integer.

  Performs an atomic compare exchange operation on the 64-bit unsigned integer specified
  by Value.  If Value is equal to CompareValue, then Value is set to ExchangeValue and
  CompareValue is returned.  If Value is not equal to CompareValue, then Value is returned.
  The compare exchange operation must be performed using MP safe mechanisms.


  @param  Value         A pointer to the 64-bit value for the compare exchange
                        operation.
  @param  CompareValue  64-bit value used in compare operation.
  @param  ExchangeValue 64-bit value used in exchange operation.

  @return The original *Value before exchange.

**/
UINT64
EFIAPI
InternalSyncCompareExchange64 (
  IN OUT  volatile UINT64  *Value,
  IN      UINT64           CompareValue,
  IN      UINT64           ExchangeValue
  )
{
  __asm__ __volatile__ (
    "lock                   \n\t"
    "cmpxchg8b   (%1)       \n\t"
    : "+A"  (CompareValue)                    // %0
    : "S"   (Value),                          // %1
      "b"   ((UINT32) ExchangeValue),         // %2
      "c"   ((UINT32) (ExchangeValue >> 32))  // %3
    : "memory",
      "cc"
  );

  return CompareValue;
}
