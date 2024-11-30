/** @file
   Random number generator service that uses the SEED instruction
   to provide pseudorandom numbers.

   Copyright (c) 2024, Rivos, Inc.

   SPDX-License-Identifier: BSD-2-Clause-Patent
 **/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/RngLib.h>
#include <Register/RiscV64/RiscVEncoding.h>

#include "BaseRngLibInternals.h"
#define RISCV_CPU_FEATURE_ZKR_BITMASK  0x8

#define SEED_RETRY_LOOPS  100

// 64-bit Mersenne Twister implementation
// A widely used pseudo random number generator. It performs bit shifts etc to
// achieve the random number. It's output is determined by SEED value generated
// by RISC-V SEED CSR"

#define STATE_SIZE  312
#define MIDDLE      156
#define INIT_SHIFT  62
#define TWIST_MASK  0xb5026f5aa96619e9ULL
#define INIT_FACT   6364136223846793005ULL
#define SHIFT1      29
#define MASK1       0x5555555555555555ULL
#define SHIFT2      17
#define MASK2       0x71d67fffeda60000ULL
#define SHIFT3      37
#define MASK3       0xfff7eee000000000ULL
#define SHIFT4      43

#define LOWER_MASK  0x7fffffff
#define UPPER_MASK  (~(UINT64)LOWER_MASK)

static UINT64  mState[STATE_SIZE];
static UINTN   mIndex = STATE_SIZE + 1;

/**
   Initialize mState to defualt state.

   @param[in] S Input seed value
 **/
STATIC
VOID
SeedRng (
  IN UINT64  S
  )
{
  UINTN  I;

  mIndex    = STATE_SIZE;
  mState[0] = S;

  for (I = 1; I < STATE_SIZE; I++) {
    mState[I] = (INIT_FACT * (mState[I - 1] ^ (mState[I - 1] >> INIT_SHIFT))) + I;
  }
}

/**
   Initializes mState with entropy values. The initialization is based on the
   Seed value populated in mState[0] which then influences all the other values
   in the mState array. Later values are retrieved from the same array instead
   of calling trng instruction every time.

 **/
STATIC
VOID
TwistRng (
  VOID
  )
{
  UINTN   I;
  UINT64  X;

  for (I = 0; I < STATE_SIZE; I++) {
    X         = (mState[I] & UPPER_MASK) | (mState[(I + 1) % STATE_SIZE] & LOWER_MASK);
    X         = (X >> 1) ^ (X & 1 ? TWIST_MASK : 0);
    mState[I] = mState[(I + MIDDLE) % STATE_SIZE] ^ X;
  }

  mIndex = 0;
}

// Defined in Seed.S
extern UINT64
ReadSeed (
  VOID
  );

/**
   Gets seed value by executing trng instruction (CSR 0x15) amd returns
   the see to the caller 64bit value.

   @param[out] Out     Buffer pointer to store the 64-bit random value.
   @retval TRUE         Random number generated successfully.
   @retval FALSE        Failed to generate the random number.
 **/
STATIC
BOOLEAN
Get64BitSeed (
  OUT UINT64  *Out
  )
{
  UINT64  Seed;
  UINTN   Retry;
  UINTN   ValidSeeds;
  UINTN   NeededSeeds;
  UINT16  *Entropy;

  Retry       = SEED_RETRY_LOOPS;
  Entropy     = (UINT16 *)Out;
  NeededSeeds = sizeof (UINT64) / sizeof (UINT16);
  ValidSeeds  = 0;

  if (!ArchIsRngSupported ()) {
    DEBUG ((DEBUG_ERROR, "Get64BitSeed: HW not supported!\n"));
    return FALSE;
  }

  do {
    Seed = ReadSeed ();

    switch (Seed & SEED_OPST_MASK) {
      case SEED_OPST_ES16:
        Entropy[ValidSeeds++] = Seed & SEED_ENTROPY_MASK;
        if (ValidSeeds == NeededSeeds) {
          return TRUE;
        }

        break;

      case SEED_OPST_DEAD:
        DEBUG ((DEBUG_ERROR, "Get64BitSeed: Unrecoverable error!\n"));
        return FALSE;

      case SEED_OPST_BIST:           // fallthrough
      case SEED_OPST_WAIT:           // fallthrough
      default:
        continue;
    }
  } while (--Retry);

  return FALSE;
}

/**
   Constructor library which initializes Seeds and mStatus array.

   @retval EFI_SUCCESS  Intialization was successful.
   @retval EFI_UNSUPPORTED Feature not supported.

 **/
EFI_STATUS
EFIAPI
BaseRngLibConstructor (
  VOID
  )
{
  UINT64  Seed;

  if (Get64BitSeed (&Seed)) {
    SeedRng (Seed);
    return EFI_SUCCESS;
  } else {
    return EFI_UNSUPPORTED;
  }
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
  OUT UINT16  *Rand
  )
{
  UINT64  Rand64;

  if (ArchGetRandomNumber64 (&Rand64)) {
    *Rand = Rand64 & MAX_UINT16;
    return TRUE;
  }

  return FALSE;
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
  OUT UINT32  *Rand
  )
{
  UINT64  Rand64;

  if (ArchGetRandomNumber64 (&Rand64)) {
    *Rand = Rand64 & MAX_UINT32;
    return TRUE;
  }

  return FALSE;
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
  OUT UINT64  *Rand
  )
{
  UINT64  Y;

  // Never initialized.
  if (mIndex > STATE_SIZE) {
    return FALSE;
  }

  // Mersenne Twister
  if (mIndex == STATE_SIZE) {
    TwistRng ();
  }

  Y  = mState[mIndex];
  Y ^= (Y >> SHIFT1) & MASK1;
  Y ^= (Y << SHIFT2) & MASK2;
  Y ^= (Y << SHIFT3) & MASK3;
  Y ^= Y >> SHIFT4;

  mIndex++;

  *Rand = Y;
  return TRUE;
}

/**
   Checks whether SEED is supported.

   @retval TRUE         SEED is supported.
 **/
BOOLEAN
EFIAPI
ArchIsRngSupported (
  VOID
  )
{
  return ((PcdGet64 (PcdRiscVFeatureOverride) & RISCV_CPU_FEATURE_ZKR_BITMASK) != 0);
}
