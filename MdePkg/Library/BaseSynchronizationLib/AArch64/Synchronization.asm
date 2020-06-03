;  Implementation of synchronization functions for ARM architecture (AArch64)
;
;  Copyright (c) 2012-2015, ARM Limited. All rights reserved.
;  Copyright (c) 2015, Linaro Limited. All rights reserved.
;
;  SPDX-License-Identifier: BSD-2-Clause-Patent
;
;

  EXPORT InternalSyncCompareExchange16
  EXPORT InternalSyncCompareExchange32
  EXPORT InternalSyncCompareExchange64
  EXPORT InternalSyncIncrement
  EXPORT InternalSyncDecrement
  AREA BaseSynchronizationLib_LowLevel, CODE, READONLY

;/**
;  Performs an atomic compare exchange operation on a 16-bit unsigned integer.
;
;  Performs an atomic compare exchange operation on the 16-bit unsigned integer
;  specified by Value.  If Value is equal to CompareValue, then Value is set to
;  ExchangeValue and CompareValue is returned.  If Value is not equal to CompareValue,
;  then Value is returned.  The compare exchange operation must be performed using
;  MP safe mechanisms.
;
;  @param  Value         A pointer to the 16-bit value for the compare exchange
;                        operation.
;  @param  CompareValue  16-bit value used in compare operation.
;  @param  ExchangeValue 16-bit value used in exchange operation.
;
;  @return The original *Value before exchange.
;
;**/
;UINT16
;EFIAPI
;InternalSyncCompareExchange16 (
;  IN      volatile UINT16           *Value,
;  IN      UINT16                    CompareValue,
;  IN      UINT16                    ExchangeValue
;  )
InternalSyncCompareExchange16
  uxth    w1, w1
  uxth    w2, w2
  dmb     sy

InternalSyncCompareExchange16Again
  ldxrh   w3, [x0]
  cmp     w3, w1
  bne     InternalSyncCompareExchange16Fail

InternalSyncCompareExchange16Exchange
  stxrh   w4, w2, [x0]
  cbnz    w4, InternalSyncCompareExchange16Again

InternalSyncCompareExchange16Fail
  dmb     sy
  mov     w0, w3
  ret

;/**
;  Performs an atomic compare exchange operation on a 32-bit unsigned integer.
;
;  Performs an atomic compare exchange operation on the 32-bit unsigned integer
;  specified by Value.  If Value is equal to CompareValue, then Value is set to
;  ExchangeValue and CompareValue is returned.  If Value is not equal to CompareValue,
;  then Value is returned.  The compare exchange operation must be performed using
;  MP safe mechanisms.
;
;  @param  Value         A pointer to the 32-bit value for the compare exchange
;                        operation.
;  @param  CompareValue  32-bit value used in compare operation.
;  @param  ExchangeValue 32-bit value used in exchange operation.
;
;  @return The original *Value before exchange.
;
;**/
;UINT32
;EFIAPI
;InternalSyncCompareExchange32 (
;  IN      volatile UINT32           *Value,
;  IN      UINT32                    CompareValue,
;  IN      UINT32                    ExchangeValue
;  )
InternalSyncCompareExchange32
  dmb     sy

InternalSyncCompareExchange32Again
  ldxr    w3, [x0]
  cmp     w3, w1
  bne     InternalSyncCompareExchange32Fail

InternalSyncCompareExchange32Exchange
  stxr    w4, w2, [x0]
  cbnz    w4, InternalSyncCompareExchange32Again

InternalSyncCompareExchange32Fail
  dmb     sy
  mov     w0, w3
  ret

;/**
;  Performs an atomic compare exchange operation on a 64-bit unsigned integer.
;
;  Performs an atomic compare exchange operation on the 64-bit unsigned integer specified
;  by Value.  If Value is equal to CompareValue, then Value is set to ExchangeValue and
;  CompareValue is returned.  If Value is not equal to CompareValue, then Value is returned.
;  The compare exchange operation must be performed using MP safe mechanisms.
;
;  @param  Value         A pointer to the 64-bit value for the compare exchange
;                        operation.
;  @param  CompareValue  64-bit value used in compare operation.
;  @param  ExchangeValue 64-bit value used in exchange operation.
;
;  @return The original *Value before exchange.
;
;**/
;UINT64
;EFIAPI
;InternalSyncCompareExchange64 (
;  IN      volatile UINT64           *Value,
;  IN      UINT64                    CompareValue,
;  IN      UINT64                    ExchangeValue
;  )
InternalSyncCompareExchange64
  dmb     sy

InternalSyncCompareExchange64Again
  ldxr    x3, [x0]
  cmp     x3, x1
  bne     InternalSyncCompareExchange64Fail

InternalSyncCompareExchange64Exchange
  stxr    w4, x2, [x0]
  cbnz    w4, InternalSyncCompareExchange64Again

InternalSyncCompareExchange64Fail
  dmb     sy
  mov     x0, x3
  ret

;/**
;  Performs an atomic increment of an 32-bit unsigned integer.
;
;  Performs an atomic increment of the 32-bit unsigned integer specified by
;  Value and returns the incremented value. The increment operation must be
;  performed using MP safe mechanisms. The state of the return value is not
;  guaranteed to be MP safe.
;
;  @param  Value A pointer to the 32-bit value to increment.
;
;  @return The incremented value.
;
;**/
;UINT32
;EFIAPI
;InternalSyncIncrement (
;  IN      volatile UINT32           *Value
;  )
InternalSyncIncrement
  dmb     sy
TryInternalSyncIncrement
  ldxr    w1, [x0]
  add     w1, w1, #1
  stxr    w2, w1, [x0]
  cbnz    w2, TryInternalSyncIncrement
  mov     w0, w1
  dmb     sy
  ret

;/**
;  Performs an atomic decrement of an 32-bit unsigned integer.
;
;  Performs an atomic decrement of the 32-bit unsigned integer specified by
;  Value and returns the decrement value. The decrement operation must be
;  performed using MP safe mechanisms. The state of the return value is not
;  guaranteed to be MP safe.
;
;  @param  Value A pointer to the 32-bit value to decrement.
;
;  @return The decrement value.
;
;**/
;UINT32
;EFIAPI
;InternalSyncDecrement (
;  IN      volatile UINT32           *Value
;  )
InternalSyncDecrement
  dmb     sy
TryInternalSyncDecrement
  ldxr    w1, [x0]
  sub     w1, w1, #1
  stxr    w2, w1, [x0]
  cbnz    w2, TryInternalSyncDecrement
  mov     w0, w1
  dmb     sy
  ret

  END
