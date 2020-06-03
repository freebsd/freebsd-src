/** @file
  Implementation of synchronization functions on EBC.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/**
  Performs an atomic compare exchange operation on a 16-bit
  unsigned integer.

  Performs an atomic compare exchange operation on the 16-bit
  unsigned integer specified by Value.  If Value is equal to
  CompareValue, then Value is set to ExchangeValue and
  CompareValue is returned.  If Value is not equal to
  CompareValue, then Value is returned. The compare exchange
  operation must be performed using MP safe mechanisms.

  @param  Value         A pointer to the 16-bit value for the
                        compare exchange operation.
  @param  CompareValue  16-bit value used in compare operation.
  @param  ExchangeValue 16-bit value used in exchange operation.

  @return The original *Value before exchange.

**/
UINT16
EFIAPI
InternalSyncCompareExchange16 (
  IN      volatile UINT16           *Value,
  IN      UINT16                    CompareValue,
  IN      UINT16                    ExchangeValue
  )
{
  return *Value != CompareValue ? *Value :
           ((*Value = ExchangeValue), CompareValue);
}

/**
  Performs an atomic compare exchange operation on a 32-bit
  unsigned integer.

  Performs an atomic compare exchange operation on the 32-bit
  unsigned integer specified by Value.  If Value is equal to
  CompareValue, then Value is set to ExchangeValue and
  CompareValue is returned.  If Value is not equal to
  CompareValue, then Value is returned. The compare exchange
  operation must be performed using MP safe mechanisms.

  @param  Value         A pointer to the 32-bit value for the
                        compare exchange operation.
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
  return *Value != CompareValue ? *Value :
           ((*Value = ExchangeValue), CompareValue);
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
  IN      volatile UINT64           *Value,
  IN      UINT64                    CompareValue,
  IN      UINT64                    ExchangeValue
  )
{
  return *Value != CompareValue ? *Value :
           ((*Value = ExchangeValue), CompareValue);
}

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
  return ++*Value;
}

/**
  Performs an atomic decrement of an 32-bit unsigned integer.

  Performs an atomic decrement of the 32-bit unsigned integer specified by
  Value and returns the decrement value. The decrement operation must be
  performed using MP safe mechanisms. The state of the return value is not
  guaranteed to be MP safe.

  @param  Value A pointer to the 32-bit value to decrement.

  @return The decrement value.

**/
UINT32
EFIAPI
InternalSyncDecrement (
  IN      volatile UINT32           *Value
  )
{
  return --*Value;
}
