/** @file
  GCC inline implementation of BaseSynchronizationLib processor specific functions.
  
  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR> 
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
  IN      volatile UINT32    *Value
  )
{
  UINT32  Result;

  __asm__ __volatile__ (
    "lock               \n\t"
    "incl    %2         \n\t"
    "mov     %2, %%eax      "
    : "=a" (Result),          // %0
      "=m" (*Value)           // %1
    : "m"  (*Value)           // %2 
    : "memory",
      "cc"
    );
    
  return Result;    
}


/**
  Performs an atomic decrement of an 32-bit unsigned integer.

  Performs an atomic decrement of the 32-bit unsigned integer specified by
  Value and returns the decremented value. The decrement operation must be
  performed using MP safe mechanisms. The state of the return value is not
  guaranteed to be MP safe.

  @param  Value A pointer to the 32-bit value to decrement.

  @return The decremented value.

**/
UINT32
EFIAPI
InternalSyncDecrement (
  IN      volatile UINT32       *Value
  )
{
   UINT32  Result;
  
  __asm__ __volatile__ (
    "lock               \n\t"
    "decl    %2         \n\t"
    "mov     %2, %%eax      "
    : "=a" (Result),          // %0
      "=m" (*Value)           // %1
    : "m"  (*Value)           // %2 
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
  IN OUT volatile  UINT16           *Value,
  IN      UINT16                    CompareValue,
  IN      UINT16                    ExchangeValue
  )
{


  __asm__ __volatile__ (
    "lock                 \n\t"
    "cmpxchgw    %3, %1       "
    : "=a" (CompareValue),
      "=m" (*Value)
    : "a"  (CompareValue),
      "r"  (ExchangeValue),
      "m"  (*Value)
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
  IN OUT volatile  UINT32           *Value,
  IN      UINT32                    CompareValue,
  IN      UINT32                    ExchangeValue
  )
{


  __asm__ __volatile__ (
    "lock                 \n\t"
    "cmpxchgl    %3, %1       "
    : "=a" (CompareValue),    // %0
      "=m" (*Value)           // %1
    : "a"  (CompareValue),    // %2
      "r"  (ExchangeValue),   // %3 
      "m"  (*Value)
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
  IN OUT  volatile UINT64           *Value,
  IN      UINT64                    CompareValue,
  IN      UINT64                    ExchangeValue
  )
{

  __asm__ __volatile__ (
    "lock                 \n\t"
    "cmpxchgq    %3, %1       "
    : "=a" (CompareValue),    // %0
      "=m" (*Value)           // %1
    : "a"  (CompareValue),    // %2
      "r"  (ExchangeValue),   // %3 
      "m"  (*Value)
    : "memory",
      "cc"
    );
  
  return CompareValue;
}


