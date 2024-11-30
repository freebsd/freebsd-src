/** @file
  Provides synchronization functions.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SYNCHRONIZATION_LIB__
#define __SYNCHRONIZATION_LIB__

///
/// Definitions for SPIN_LOCK
///
typedef volatile UINTN SPIN_LOCK;

/**
  Retrieves the architecture-specific spin lock alignment requirements for
  optimal spin lock performance.

  This function retrieves the spin lock alignment requirements for optimal
  performance on a given CPU architecture. The spin lock alignment is byte alignment.
  It must be a power of two and is returned by this function. If there are no alignment
  requirements, then 1 must be returned. The spin lock synchronization
  functions must function correctly if the spin lock size and alignment values
  returned by this function are not used at all. These values are hints to the
  consumers of the spin lock synchronization functions to obtain optimal spin
  lock performance.

  @return The architecture-specific spin lock alignment.

**/
UINTN
EFIAPI
GetSpinLockProperties (
  VOID
  );

/**
  Initializes a spin lock to the released state and returns the spin lock.

  This function initializes the spin lock specified by SpinLock to the released
  state, and returns SpinLock. Optimal performance can be achieved by calling
  GetSpinLockProperties() to determine the size and alignment requirements for
  SpinLock.

  If SpinLock is NULL, then ASSERT().

  @param  SpinLock  A pointer to the spin lock to initialize to the released
                    state.

  @return SpinLock in release state.

**/
SPIN_LOCK *
EFIAPI
InitializeSpinLock (
  OUT      SPIN_LOCK  *SpinLock
  );

/**
  Waits until a spin lock can be placed in the acquired state.

  This function checks the state of the spin lock specified by SpinLock. If
  SpinLock is in the released state, then this function places SpinLock in the
  acquired state and returns SpinLock. Otherwise, this function waits
  indefinitely for the spin lock to be released, and then places it in the
  acquired state and returns SpinLock. All state transitions of SpinLock must
  be performed using MP safe mechanisms.

  If SpinLock is NULL, then ASSERT().
  If SpinLock was not initialized with InitializeSpinLock(), then ASSERT().
  If PcdSpinLockTimeout is not zero, and SpinLock is can not be acquired in
  PcdSpinLockTimeout microseconds, then ASSERT().

  @param  SpinLock  A pointer to the spin lock to place in the acquired state.

  @return SpinLock acquired lock.

**/
SPIN_LOCK *
EFIAPI
AcquireSpinLock (
  IN OUT  SPIN_LOCK  *SpinLock
  );

/**
  Attempts to place a spin lock in the acquired state.

  This function checks the state of the spin lock specified by SpinLock. If
  SpinLock is in the released state, then this function places SpinLock in the
  acquired state and returns TRUE. Otherwise, FALSE is returned. All state
  transitions of SpinLock must be performed using MP safe mechanisms.

  If SpinLock is NULL, then ASSERT().
  If SpinLock was not initialized with InitializeSpinLock(), then ASSERT().

  @param  SpinLock  A pointer to the spin lock to place in the acquired state.

  @retval TRUE  SpinLock was placed in the acquired state.
  @retval FALSE SpinLock could not be acquired.

**/
BOOLEAN
EFIAPI
AcquireSpinLockOrFail (
  IN OUT  SPIN_LOCK  *SpinLock
  );

/**
  Releases a spin lock.

  This function places the spin lock specified by SpinLock in the release state
  and returns SpinLock.

  If SpinLock is NULL, then ASSERT().
  If SpinLock was not initialized with InitializeSpinLock(), then ASSERT().

  @param  SpinLock  A pointer to the spin lock to release.

  @return SpinLock released lock.

**/
SPIN_LOCK *
EFIAPI
ReleaseSpinLock (
  IN OUT  SPIN_LOCK  *SpinLock
  );

/**
  Performs an atomic increment of a 32-bit unsigned integer.

  Performs an atomic increment of the 32-bit unsigned integer specified by
  Value and returns the incremented value. The increment operation must be
  performed using MP safe mechanisms.

  If Value is NULL, then ASSERT().

  @param  Value A pointer to the 32-bit value to increment.

  @return The incremented value.

**/
UINT32
EFIAPI
InterlockedIncrement (
  IN      volatile UINT32  *Value
  );

/**
  Performs an atomic decrement of a 32-bit unsigned integer.

  Performs an atomic decrement of the 32-bit unsigned integer specified by
  Value and returns the decremented value. The decrement operation must be
  performed using MP safe mechanisms.

  If Value is NULL, then ASSERT().

  @param  Value A pointer to the 32-bit value to decrement.

  @return The decremented value.

**/
UINT32
EFIAPI
InterlockedDecrement (
  IN      volatile UINT32  *Value
  );

/**
  Performs an atomic compare exchange operation on a 16-bit unsigned integer.

  Performs an atomic compare exchange operation on the 16-bit unsigned integer
  specified by Value.  If Value is equal to CompareValue, then Value is set to
  ExchangeValue and CompareValue is returned.  If Value is not equal to CompareValue,
  then Value is returned.  The compare exchange operation must be performed using
  MP safe mechanisms.

  If Value is NULL, then ASSERT().

  @param  Value         A pointer to the 16-bit value for the compare exchange
                        operation.
  @param  CompareValue  16-bit value used in compare operation.
  @param  ExchangeValue 16-bit value used in exchange operation.

  @return The original *Value before exchange.
**/
UINT16
EFIAPI
InterlockedCompareExchange16 (
  IN OUT  volatile UINT16  *Value,
  IN      UINT16           CompareValue,
  IN      UINT16           ExchangeValue
  );

/**
  Performs an atomic compare exchange operation on a 32-bit unsigned integer.

  Performs an atomic compare exchange operation on the 32-bit unsigned integer
  specified by Value.  If Value is equal to CompareValue, then Value is set to
  ExchangeValue and CompareValue is returned.  If Value is not equal to CompareValue,
  then Value is returned.  The compare exchange operation must be performed using
  MP safe mechanisms.

  If Value is NULL, then ASSERT().

  @param  Value         A pointer to the 32-bit value for the compare exchange
                        operation.
  @param  CompareValue  32-bit value used in compare operation.
  @param  ExchangeValue 32-bit value used in exchange operation.

  @return The original *Value before exchange.

**/
UINT32
EFIAPI
InterlockedCompareExchange32 (
  IN OUT  volatile UINT32  *Value,
  IN      UINT32           CompareValue,
  IN      UINT32           ExchangeValue
  );

/**
  Performs an atomic compare exchange operation on a 64-bit unsigned integer.

  Performs an atomic compare exchange operation on the 64-bit unsigned integer specified
  by Value.  If Value is equal to CompareValue, then Value is set to ExchangeValue and
  CompareValue is returned.  If Value is not equal to CompareValue, then Value is returned.
  The compare exchange operation must be performed using MP safe mechanisms.

  If Value is NULL, then ASSERT().

  @param  Value         A pointer to the 64-bit value for the compare exchange
                        operation.
  @param  CompareValue  64-bit value used in compare operation.
  @param  ExchangeValue 64-bit value used in exchange operation.

  @return The original *Value before exchange.

**/
UINT64
EFIAPI
InterlockedCompareExchange64 (
  IN OUT  volatile UINT64  *Value,
  IN      UINT64           CompareValue,
  IN      UINT64           ExchangeValue
  );

/**
  Performs an atomic compare exchange operation on a pointer value.

  Performs an atomic compare exchange operation on the pointer value specified
  by Value. If Value is equal to CompareValue, then Value is set to
  ExchangeValue and CompareValue is returned. If Value is not equal to
  CompareValue, then Value is returned. The compare exchange operation must be
  performed using MP safe mechanisms.

  If Value is NULL, then ASSERT().

  @param  Value         A pointer to the pointer value for the compare exchange
                        operation.
  @param  CompareValue  Pointer value used in compare operation.
  @param  ExchangeValue Pointer value used in exchange operation.

  @return The original *Value before exchange.
**/
VOID *
EFIAPI
InterlockedCompareExchangePointer (
  IN OUT  VOID                      *volatile  *Value,
  IN      VOID                                 *CompareValue,
  IN      VOID                                 *ExchangeValue
  );

#endif
