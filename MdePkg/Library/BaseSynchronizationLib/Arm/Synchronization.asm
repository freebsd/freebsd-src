//  Implementation of synchronization functions for ARM architecture
//
//  Copyright (c) 2012-2015, ARM Limited. All rights reserved.
//  Copyright (c) 2015, Linaro Limited. All rights reserved.
//
//  SPDX-License-Identifier: BSD-2-Clause-Patent
//
//

    EXPORT  InternalSyncCompareExchange16
    EXPORT  InternalSyncCompareExchange32
    EXPORT  InternalSyncCompareExchange64
    EXPORT  InternalSyncIncrement
    EXPORT  InternalSyncDecrement

    AREA   ArmSynchronization, CODE, READONLY

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
//UINT16
//EFIAPI
//InternalSyncCompareExchange16 (
//  IN      volatile UINT16           *Value,
//  IN      UINT16                    CompareValue,
//  IN      UINT16                    ExchangeValue
//  )
InternalSyncCompareExchange16
  dmb

InternalSyncCompareExchange16Again
  ldrexh  r3, [r0]
  cmp     r3, r1
  bne     InternalSyncCompareExchange16Fail

InternalSyncCompareExchange16Exchange
  strexh  ip, r2, [r0]
  cmp     ip, #0
  bne     InternalSyncCompareExchange16Again

InternalSyncCompareExchange16Fail
  dmb
  mov     r0, r3
  bx      lr

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
//UINT32
//EFIAPI
//InternalSyncCompareExchange32 (
//  IN      volatile UINT32           *Value,
//  IN      UINT32                    CompareValue,
//  IN      UINT32                    ExchangeValue
//  )
InternalSyncCompareExchange32
  dmb

InternalSyncCompareExchange32Again
  ldrex   r3, [r0]
  cmp     r3, r1
  bne     InternalSyncCompareExchange32Fail

InternalSyncCompareExchange32Exchange
  strex   ip, r2, [r0]
  cmp     ip, #0
  bne     InternalSyncCompareExchange32Again

InternalSyncCompareExchange32Fail
  dmb
  mov     r0, r3
  bx      lr

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
//UINT64
//EFIAPI
//InternalSyncCompareExchange64 (
//  IN      volatile UINT64           *Value,         // r0
//  IN      UINT64                    CompareValue,   // r2-r3
//  IN      UINT64                    ExchangeValue   // stack
//  )
InternalSyncCompareExchange64
  push    { r4-r7 }
  ldrd    r4, r5, [sp, #16]
  dmb

InternalSyncCompareExchange64Again
  ldrexd  r6, r7, [r0]
  cmp     r6, r2
  cmpeq   r7, r3
  bne     InternalSyncCompareExchange64Fail

InternalSyncCompareExchange64Exchange
  strexd  ip, r4, r5, [r0]
  cmp     ip, #0
  bne     InternalSyncCompareExchange64Again

InternalSyncCompareExchange64Fail
  dmb
  mov     r0, r6
  mov     r1, r7
  pop     { r4-r7 }
  bx      lr

/**
  Performs an atomic increment of an 32-bit unsigned integer.

  Performs an atomic increment of the 32-bit unsigned integer specified by
  Value and returns the incremented value. The increment operation must be
  performed using MP safe mechanisms. The state of the return value is not
  guaranteed to be MP safe.

  @param  Value A pointer to the 32-bit value to increment.

  @return The incremented value.

**/
//UINT32
//EFIAPI
//InternalSyncIncrement (
//  IN      volatile UINT32           *Value
//  )
InternalSyncIncrement
  dmb
TryInternalSyncIncrement
  ldrex   r1, [r0]
  add     r1, r1, #1
  strex   r2, r1, [r0]
  cmp     r2, #0
  bne     TryInternalSyncIncrement
  dmb
  mov     r0, r1
  bx      lr

/**
  Performs an atomic decrement of an 32-bit unsigned integer.

  Performs an atomic decrement of the 32-bit unsigned integer specified by
  Value and returns the decrement value. The decrement operation must be
  performed using MP safe mechanisms. The state of the return value is not
  guaranteed to be MP safe.

  @param  Value A pointer to the 32-bit value to decrement.

  @return The decrement value.

**/
//UINT32
//EFIAPI
//InternalSyncDecrement (
//  IN      volatile UINT32           *Value
//  )
InternalSyncDecrement
  dmb
TryInternalSyncDecrement
  ldrex   r1, [r0]
  sub     r1, r1, #1
  strex   r2, r1, [r0]
  cmp     r2, #0
  bne     TryInternalSyncDecrement
  dmb
  mov     r0, r1
  bx      lr

  END
