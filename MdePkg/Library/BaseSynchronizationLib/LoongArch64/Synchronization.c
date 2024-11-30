/** @file
  LoongArch synchronization functions.

  Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>

UINT32
EFIAPI
AsmInternalSyncCompareExchange16 (
  IN volatile UINT32 *,
  IN UINT64,
  IN UINT64,
  IN UINT64
  );

UINT32
EFIAPI
AsmInternalSyncCompareExchange32 (
  IN volatile UINT32 *,
  IN UINT64,
  IN UINT64
  );

UINT64
EFIAPI
AsmInternalSyncCompareExchange64 (
  IN volatile UINT64 *,
  IN UINT64,
  IN UINT64
  );

UINT32
EFIAPI
AsmInternalSyncIncrement (
  IN      volatile UINT32 *
  );

UINT32
EFIAPI
AsmInternalSyncDecrement (
  IN      volatile UINT32 *
  );

/**
  Performs an atomic compare exchange operation on a 16-bit
  unsigned integer.

  Performs an atomic compare exchange operation on the 16-bit
  unsigned integer specified by Value.  If Value is equal to
  CompareValue, then Value is set to ExchangeValue and
  CompareValue is returned.  If Value is not equal to
  CompareValue, then Value is returned. The compare exchange
  operation must be performed using MP safe mechanisms.

  @param[in]  Value         A pointer to the 16-bit value for the
                        compare exchange operation.
  @param[in]  CompareValue  16-bit value used in compare operation.
  @param[in]  ExchangeValue 16-bit value used in exchange operation.

  @return The original *Value before exchange.

**/
UINT16
EFIAPI
InternalSyncCompareExchange16 (
  IN      volatile UINT16  *Value,
  IN      UINT16           CompareValue,
  IN      UINT16           ExchangeValue
  )
{
  UINT32           RetValue;
  UINT32           Shift;
  UINT64           Mask;
  UINT64           LocalCompareValue;
  UINT64           LocalExchangeValue;
  volatile UINT32  *Ptr32;

  /* Check that ptr is naturally aligned */
  ASSERT (!((UINT64)Value & (sizeof (UINT16) - 1)));

  /* Mask inputs to the correct size. */
  Mask               = (((~0UL) - (1UL << (0)) + 1) & (~0UL >> (64 - 1 - ((sizeof (UINT16) * 8) - 1))));
  LocalCompareValue  = ((UINT64)CompareValue) & Mask;
  LocalExchangeValue = ((UINT64)ExchangeValue) & Mask;

  /*
   * Calculate a shift & mask that correspond to the value we wish to
   * compare & exchange within the naturally aligned 4 byte integer
   * that includes it.
   */
  Shift                = (UINT64)Value & 0x3;
  Shift               *= 8; /* BITS_PER_BYTE */
  LocalCompareValue  <<= Shift;
  LocalExchangeValue <<= Shift;
  Mask               <<= Shift;

  /*
   * Calculate a pointer to the naturally aligned 4 byte integer that
   * includes our byte of interest, and load its value.
   */
  Ptr32 = (UINT32 *)((UINT64)Value & ~0x3);

  RetValue = AsmInternalSyncCompareExchange16 (
               Ptr32,
               Mask,
               LocalCompareValue,
               LocalExchangeValue
               );

  return (RetValue & Mask) >> Shift;
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

  @param[in]  Value         A pointer to the 32-bit value for the
                        compare exchange operation.
  @param[in]  CompareValue  32-bit value used in compare operation.
  @param[in]  ExchangeValue 32-bit value used in exchange operation.

  @return The original *Value before exchange.

**/
UINT32
EFIAPI
InternalSyncCompareExchange32 (
  IN      volatile UINT32  *Value,
  IN      UINT32           CompareValue,
  IN      UINT32           ExchangeValue
  )
{
  UINT32  RetValue;

  RetValue = AsmInternalSyncCompareExchange32 (
               Value,
               CompareValue,
               ExchangeValue
               );

  return RetValue;
}

/**
  Performs an atomic compare exchange operation on a 64-bit unsigned integer.

  Performs an atomic compare exchange operation on the 64-bit unsigned integer specified
  by Value.  If Value is equal to CompareValue, then Value is set to ExchangeValue and
  CompareValue is returned.  If Value is not equal to CompareValue, then Value is returned.
  The compare exchange operation must be performed using MP safe mechanisms.

  @param[in]  Value         A pointer to the 64-bit value for the compare exchange
                        operation.
  @param[in]  CompareValue  64-bit value used in compare operation.
  @param[in]  ExchangeValue 64-bit value used in exchange operation.

  @return The original *Value before exchange.

**/
UINT64
EFIAPI
InternalSyncCompareExchange64 (
  IN      volatile UINT64  *Value,
  IN      UINT64           CompareValue,
  IN      UINT64           ExchangeValue
  )
{
  UINT64  RetValue;

  RetValue = AsmInternalSyncCompareExchange64 (
               Value,
               CompareValue,
               ExchangeValue
               );

  return RetValue;
}

/**
  Performs an atomic increment of an 32-bit unsigned integer.

  Performs an atomic increment of the 32-bit unsigned integer specified by
  Value and returns the incremented value. The increment operation must be
  performed using MP safe mechanisms. The state of the return value is not
  guaranteed to be MP safe.

  @param[in]  Value A pointer to the 32-bit value to increment.

  @return The incremented value.

**/
UINT32
EFIAPI
InternalSyncIncrement (
  IN      volatile UINT32  *Value
  )
{
  return AsmInternalSyncIncrement (Value);
}

/**
  Performs an atomic decrement of an 32-bit unsigned integer.

  Performs an atomic decrement of the 32-bit unsigned integer specified by
  Value and returns the decrement value. The decrement operation must be
  performed using MP safe mechanisms. The state of the return value is not
  guaranteed to be MP safe.

  @param[in]  Value A pointer to the 32-bit value to decrement.

  @return The decrement value.

**/
UINT32
EFIAPI
InternalSyncDecrement (
  IN      volatile UINT32  *Value
  )
{
  return AsmInternalSyncDecrement (Value);
}
