/** @file
  BaseRng Library that uses the TimerLib to provide reasonably random numbers.
  Do not use this on a production system.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>

#define DEFAULT_DELAY_TIME_IN_MICROSECONDS  10

/**
  This implementation is to be replaced by its MdeModulePkg copy.
  The cause being that some GUIDs (gEdkiiRngAlgorithmUnSafe) cannot
  be defined in the MdePkg.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.
**/
RETURN_STATUS
EFIAPI
BaseRngLibTimerConstructor (
  VOID
  )
{
  DEBUG ((
    DEBUG_WARN,
    "Warning: This BaseRngTimerLib implementation will be deprecated. "
    "Please use the MdeModulePkg implementation equivalent.\n"
    ));

  return RETURN_SUCCESS;
}

/**
 Using the TimerLib GetPerformanceCounterProperties() we delay
 for enough time for the PerformanceCounter to increment.

 If the return value from GetPerformanceCounterProperties (TimerLib)
 is zero, this function will return 10 and attempt to assert.
 **/
STATIC
UINT32
CalculateMinimumDecentDelayInMicroseconds (
  VOID
  )
{
  UINT64  CounterHz;

  // Get the counter properties
  CounterHz = GetPerformanceCounterProperties (NULL, NULL);
  // Make sure we won't divide by zero
  if (CounterHz == 0) {
    ASSERT (CounterHz != 0); // Assert so the developer knows something is wrong
    return DEFAULT_DELAY_TIME_IN_MICROSECONDS;
  }

  // Calculate the minimum delay based on 1.5 microseconds divided by the hertz.
  // We calculate the length of a cycle (1/CounterHz) and multiply it by 1.5 microseconds
  // This ensures that the performance counter has increased by at least one
  return (UINT32)(MAX (DivU64x64Remainder (1500000, CounterHz, NULL), 1));
}

/**
  Generates a 16-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 16-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber16 (
  OUT     UINT16  *Rand
  )
{
  UINT32  Index;
  UINT8   *RandPtr;
  UINT32  DelayInMicroSeconds;

  ASSERT (Rand != NULL);

  if (Rand == NULL) {
    return FALSE;
  }

  DelayInMicroSeconds = CalculateMinimumDecentDelayInMicroseconds ();
  RandPtr             = (UINT8 *)Rand;
  // Get 2 bytes of random ish data
  for (Index = 0; Index < sizeof (UINT16); Index++) {
    *RandPtr = (UINT8)(GetPerformanceCounter () & 0xFF);
    // Delay to give the performance counter a chance to change
    MicroSecondDelay (DelayInMicroSeconds);
    RandPtr++;
  }

  return TRUE;
}

/**
  Generates a 32-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 32-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber32 (
  OUT     UINT32  *Rand
  )
{
  UINT32  Index;
  UINT8   *RandPtr;
  UINT32  DelayInMicroSeconds;

  ASSERT (Rand != NULL);

  if (NULL == Rand) {
    return FALSE;
  }

  RandPtr             = (UINT8 *)Rand;
  DelayInMicroSeconds = CalculateMinimumDecentDelayInMicroseconds ();
  // Get 4 bytes of random ish data
  for (Index = 0; Index < sizeof (UINT32); Index++) {
    *RandPtr = (UINT8)(GetPerformanceCounter () & 0xFF);
    // Delay to give the performance counter a chance to change
    MicroSecondDelay (DelayInMicroSeconds);
    RandPtr++;
  }

  return TRUE;
}

/**
  Generates a 64-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 64-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber64 (
  OUT     UINT64  *Rand
  )
{
  UINT32  Index;
  UINT8   *RandPtr;
  UINT32  DelayInMicroSeconds;

  ASSERT (Rand != NULL);

  if (NULL == Rand) {
    return FALSE;
  }

  RandPtr             = (UINT8 *)Rand;
  DelayInMicroSeconds = CalculateMinimumDecentDelayInMicroseconds ();
  // Get 8 bytes of random ish data
  for (Index = 0; Index < sizeof (UINT64); Index++) {
    *RandPtr = (UINT8)(GetPerformanceCounter () & 0xFF);
    // Delay to give the performance counter a chance to change
    MicroSecondDelay (DelayInMicroSeconds);
    RandPtr++;
  }

  return TRUE;
}

/**
  Generates a 128-bit random number.

  if Rand is NULL, then ASSERT().

  @param[out] Rand     Buffer pointer to store the 128-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber128 (
  OUT     UINT64  *Rand
  )
{
  ASSERT (Rand != NULL);
  // This should take around 80ms

  // Read first 64 bits
  if (!GetRandomNumber64 (Rand)) {
    return FALSE;
  }

  // Read second 64 bits
  return GetRandomNumber64 (++Rand);
}

/**
  Get a GUID identifying the RNG algorithm implementation.

  @param [out] RngGuid  If success, contains the GUID identifying
                        the RNG algorithm implementation.

  @retval EFI_SUCCESS             Success.
  @retval EFI_UNSUPPORTED         Not supported.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
RETURN_STATUS
EFIAPI
GetRngGuid (
  GUID  *RngGuid
  )
{
  /* This implementation is to be replaced by its MdeModulePkg copy.
   * The cause being that some GUIDs (gEdkiiRngAlgorithmUnSafe) cannot
   * be defined in the MdePkg.
   */
  return RETURN_UNSUPPORTED;
}
