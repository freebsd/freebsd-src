/** @file
  Random number generator services that uses RdRand instruction access
  to provide high-quality random numbers.

Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.<BR>
Copyright (c) 2023, Arm Limited. All rights reserved.<BR>
Copyright (c) 2022, Pedro Falcato. All rights reserved.<BR>
Copyright (c) 2021, NUVIA Inc. All rights reserved.<BR>
Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

#include "BaseRngLibInternals.h"

//
// Bit mask used to determine if RdRand instruction is supported.
//
#define RDRAND_MASK  BIT30

//
// Intel SDM says 10 tries is good enough for reliable RDRAND usage.
//
#define RDRAND_RETRIES  10

#define RDRAND_TEST_SAMPLES  8

#define RDRAND_MIN_CHANGE  5

//
// Add a define for native-word RDRAND, just for the test.
//
#ifdef MDE_CPU_X64
#define ASM_RDRAND  AsmRdRand64
#else
#define ASM_RDRAND  AsmRdRand32
#endif

/**
  Tests RDRAND for broken implementations.

  @retval TRUE         RDRAND is reliable (and hopefully safe).
  @retval FALSE        RDRAND is unreliable and should be disabled, despite CPUID.

**/
STATIC
BOOLEAN
TestRdRand (
  VOID
  )
{
  //
  // Test for notoriously broken rdrand implementations that always return the same
  // value, like the Zen 3 uarch (all-1s) or other several AMD families on suspend/resume (also all-1s).
  // Note that this should be expanded to extensively test for other sorts of possible errata.
  //

  //
  // Our algorithm samples rdrand $RDRAND_TEST_SAMPLES times and expects
  // a different result $RDRAND_MIN_CHANGE times for reliable RDRAND usage.
  //
  UINTN   Prev;
  UINT8   Idx;
  UINT8   TestIteration;
  UINT32  Changed;

  Changed = 0;

  for (TestIteration = 0; TestIteration < RDRAND_TEST_SAMPLES; TestIteration++) {
    UINTN  Sample;
    //
    // Note: We use a retry loop for rdrand. Normal users get this in BaseRng.c
    // Any failure to get a random number will assume RDRAND does not work.
    //
    for (Idx = 0; Idx < RDRAND_RETRIES; Idx++) {
      if (ASM_RDRAND (&Sample)) {
        break;
      }
    }

    if (Idx == RDRAND_RETRIES) {
      DEBUG ((DEBUG_ERROR, "BaseRngLib/x86: CPU BUG: Failed to get an RDRAND random number - disabling\n"));
      return FALSE;
    }

    if (TestIteration != 0) {
      Changed += Sample != Prev;
    }

    Prev = Sample;
  }

  if (Changed < RDRAND_MIN_CHANGE) {
    DEBUG ((DEBUG_ERROR, "BaseRngLib/x86: CPU BUG: RDRAND not reliable - disabling\n"));
    return FALSE;
  }

  return TRUE;
}

#undef ASM_RDRAND

/**
  The constructor function checks whether or not RDRAND instruction is supported
  by the host hardware.

  The constructor function checks whether or not RDRAND instruction is supported.
  It will ASSERT() if RDRAND instruction is not supported.
  It will always return EFI_SUCCESS.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
BaseRngLibConstructor (
  VOID
  )
{
  return EFI_SUCCESS;
}

/**
  Generates a 16-bit random number.

  @param[out] Rand     Buffer pointer to store the 16-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
ArchGetRandomNumber16 (
  OUT     UINT16  *Rand
  )
{
  return AsmRdRand16 (Rand);
}

/**
  Generates a 32-bit random number.

  @param[out] Rand     Buffer pointer to store the 32-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
ArchGetRandomNumber32 (
  OUT     UINT32  *Rand
  )
{
  return AsmRdRand32 (Rand);
}

/**
  Generates a 64-bit random number.

  @param[out] Rand     Buffer pointer to store the 64-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
ArchGetRandomNumber64 (
  OUT     UINT64  *Rand
  )
{
  return AsmRdRand64 (Rand);
}

/**
  Checks whether RDRAND is supported.

  @retval TRUE         RDRAND is supported.
  @retval FALSE        RDRAND is not supported.

**/
BOOLEAN
EFIAPI
ArchIsRngSupported (
  VOID
  )
{
  BOOLEAN  RdRandSupported;
  UINT32   RegEcx;

  //
  // Determine RDRAND support by examining bit 30 of the ECX register returned by
  // CPUID. A value of 1 indicates that processor support RDRAND instruction.
  //
  AsmCpuid (1, 0, 0, &RegEcx, 0);

  RdRandSupported = ((RegEcx & RDRAND_MASK) == RDRAND_MASK);

  if (RdRandSupported) {
    RdRandSupported = TestRdRand ();
  }

  return RdRandSupported;
}

/**
  Get a GUID identifying the RNG algorithm implementation.

  @param [out] RngGuid  If success, contains the GUID identifying
                        the RNG algorithm implementation.

  @retval EFI_SUCCESS             Success.
  @retval EFI_UNSUPPORTED         Not supported.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
EFI_STATUS
EFIAPI
GetRngGuid (
  GUID  *RngGuid
  )
{
  if (RngGuid == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (RngGuid, &gEfiRngAlgorithmSp80090Ctr256Guid, sizeof (*RngGuid));
  return EFI_SUCCESS;
}
