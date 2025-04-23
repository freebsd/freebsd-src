/** @file
 Provides an implementation of the library class RngLib that uses the Rng protocol.

 Copyright (c) 2023 - 2024, Arm Limited. All rights reserved.
 Copyright (c) Microsoft Corporation. All rights reserved.
 SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/RngLib.h>
#include <Protocol/Rng.h>

STATIC EFI_RNG_PROTOCOL  *mRngProtocol;
STATIC UINTN             mFirstAlgo = MAX_UINTN;

typedef struct {
  /// Guid of the secure algorithm.
  EFI_GUID       *Guid;

  /// Algorithm name.
  CONST CHAR8    *Name;

  /// The algorithm is available for use.
  BOOLEAN        Available;
} SECURE_RNG_ALGO_ARRAY;

//
// These represent UEFI SPEC defined algorithms that should be supported by
// the RNG protocol and are generally considered secure.
//
static SECURE_RNG_ALGO_ARRAY  mSecureHashAlgorithms[] = {
 #ifdef MDE_CPU_AARCH64
  {
    &gEfiRngAlgorithmArmRndr, // unspecified SP800-90A DRBG (through RNDR instr.)
    "ARM-RNDR",
    FALSE,
  },
 #endif
  {
    &gEfiRngAlgorithmSp80090Ctr256Guid,  // SP800-90A DRBG CTR using AES-256
    "DRBG-CTR",
    FALSE,
  },
  {
    &gEfiRngAlgorithmSp80090Hmac256Guid, // SP800-90A DRBG HMAC using SHA-256
    "DRBG-HMAC",
    FALSE,
  },
  {
    &gEfiRngAlgorithmSp80090Hash256Guid, // SP800-90A DRBG Hash using SHA-256
    "DRBG-Hash",
    FALSE,
  },
  {
    &gEfiRngAlgorithmRaw, // Raw data from NRBG (or TRNG)
    "TRNG",
    FALSE,
  },
};

/**
  Constructor routine to probe the available secure Rng algorithms.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS           Success.
  @retval EFI_NOT_FOUND         Not found.
  @retval EFI_INVALID_PARAMETER Invalid parameter.
**/
EFI_STATUS
EFIAPI
DxeRngLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS         Status;
  UINTN              RngArraySize;
  UINTN              RngArrayCnt;
  UINT32             Index;
  UINT32             Index1;
  EFI_RNG_ALGORITHM  *RngArray;

  Status = gBS->LocateProtocol (&gEfiRngProtocolGuid, NULL, (VOID **)&mRngProtocol);
  if (EFI_ERROR (Status) || (mRngProtocol == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Could not locate RNG protocol, Status = %r\n", __func__, Status));
    return Status;
  }

  RngArraySize = 0;

  Status = mRngProtocol->GetInfo (mRngProtocol, &RngArraySize, NULL);
  if (EFI_ERROR (Status) && (Status != EFI_BUFFER_TOO_SMALL)) {
    return Status;
  } else if (RngArraySize == 0) {
    return EFI_NOT_FOUND;
  }

  RngArrayCnt = RngArraySize / sizeof (*RngArray);

  RngArray = AllocateZeroPool (RngArraySize);
  if (RngArray == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = mRngProtocol->GetInfo (mRngProtocol, &RngArraySize, RngArray);
  if (EFI_ERROR (Status)) {
    goto ExitHandler;
  }

  for (Index = 0; Index < RngArrayCnt; Index++) {
    for (Index1 = 0; Index1 < ARRAY_SIZE (mSecureHashAlgorithms); Index1++) {
      if (CompareGuid (&RngArray[Index], mSecureHashAlgorithms[Index1].Guid)) {
        mSecureHashAlgorithms[Index1].Available = TRUE;
        if (mFirstAlgo == MAX_UINTN) {
          mFirstAlgo = Index1;
        }

        break;
      }
    }
  }

ExitHandler:
  FreePool (RngArray);
  return Status;
}

/**
  Routine Description:

  Generates a random number via the NIST
  800-9A algorithm.  Refer to
  http://csrc.nist.gov/groups/STM/cavp/documents/drbg/DRBGVS.pdf
  for more information.

  @param[out] Buffer      Buffer to receive the random number.
  @param[in]  BufferSize  Number of bytes in Buffer.

  @retval EFI_SUCCESS or underlying failure code.
**/
STATIC
EFI_STATUS
GenerateRandomNumberViaNist800Algorithm (
  OUT UINT8  *Buffer,
  IN  UINTN  BufferSize
  )
{
  EFI_STATUS             Status;
  UINTN                  Index;
  SECURE_RNG_ALGO_ARRAY  *Algo;

  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Buffer == NULL.\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  if (mRngProtocol == NULL) {
    return EFI_NOT_FOUND;
  }

  // Try the first available algorithm.
  if (mFirstAlgo != MAX_UINTN) {
    Algo   = &mSecureHashAlgorithms[mFirstAlgo];
    Status = mRngProtocol->GetRNG (mRngProtocol, Algo->Guid, BufferSize, Buffer);
    DEBUG ((
      DEBUG_INFO,
      "%a: GetRNG algorithm %a - Status = %r\n",
      __func__,
      Algo->Name,
      Status
      ));
    if (!EFI_ERROR (Status)) {
      return Status;
    }

    Index = mFirstAlgo + 1;
  } else {
    Index = 0;
  }

  // Iterate over other available algorithms.
  for ( ; Index < ARRAY_SIZE (mSecureHashAlgorithms); Index++) {
    Algo = &mSecureHashAlgorithms[Index];
    if (!Algo->Available) {
      continue;
    }

    Status = mRngProtocol->GetRNG (mRngProtocol, Algo->Guid, BufferSize, Buffer);
    DEBUG ((
      DEBUG_INFO,
      "%a: GetRNG algorithm %a - Status = %r\n",
      __func__,
      Algo->Name,
      Status
      ));
    if (!EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (PcdGetBool (PcdEnforceSecureRngAlgorithms)) {
    // Platform does not permit the use of the default (insecure) algorithm.
    Status = EFI_SECURITY_VIOLATION;
  } else {
    // If all the other methods have failed, use the default method from the RngProtocol
    Status = mRngProtocol->GetRNG (mRngProtocol, NULL, BufferSize, Buffer);
    DEBUG ((DEBUG_INFO, "%a: GetRNG algorithm default - Status = %r\n", __func__, Status));
    if (!EFI_ERROR (Status)) {
      return Status;
    }
  }

  // If we get to this point, we have failed
  DEBUG ((DEBUG_ERROR, "%a: GetRNG() failed, Status = %r\n", __func__, Status));

  return Status;
}// GenerateRandomNumberViaNist800Algorithm()

/**
  Generates a 16-bit random number.

  if Rand is NULL, return FALSE.

  @param[out] Rand     Buffer pointer to store the 16-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber16 (
  OUT UINT16  *Rand
  )
{
  EFI_STATUS  Status;

  if (Rand == NULL) {
    return FALSE;
  }

  Status = GenerateRandomNumberViaNist800Algorithm ((UINT8 *)Rand, sizeof (UINT16));
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return TRUE;
}

/**
  Generates a 32-bit random number.

  if Rand is NULL, return FALSE.

  @param[out] Rand     Buffer pointer to store the 32-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber32 (
  OUT UINT32  *Rand
  )
{
  EFI_STATUS  Status;

  if (Rand == NULL) {
    return FALSE;
  }

  Status = GenerateRandomNumberViaNist800Algorithm ((UINT8 *)Rand, sizeof (UINT32));
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return TRUE;
}

/**
  Generates a 64-bit random number.

  if Rand is NULL, return FALSE.

  @param[out] Rand     Buffer pointer to store the 64-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber64 (
  OUT UINT64  *Rand
  )
{
  EFI_STATUS  Status;

  if (Rand == NULL) {
    return FALSE;
  }

  Status = GenerateRandomNumberViaNist800Algorithm ((UINT8 *)Rand, sizeof (UINT64));
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return TRUE;
}

/**
  Generates a 128-bit random number.

  if Rand is NULL, return FALSE.

  @param[out] Rand     Buffer pointer to store the 128-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
BOOLEAN
EFIAPI
GetRandomNumber128 (
  OUT UINT64  *Rand
  )
{
  EFI_STATUS  Status;

  if (Rand == NULL) {
    return FALSE;
  }

  Status = GenerateRandomNumberViaNist800Algorithm ((UINT8 *)Rand, 2 * sizeof (UINT64));
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  return TRUE;
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
  /* It is not possible to know beforehand which Rng algorithm will
   * be used by this library.
   * This API is mainly used by RngDxe. RngDxe relies on the RngLib.
   * The RngLib|DxeRngLib.inf implementation locates and uses an installed
   * EFI_RNG_PROTOCOL.
   * It is thus not possible to have both RngDxe and RngLib|DxeRngLib.inf.
   * and it is ok not to support this API.
   */
  return EFI_UNSUPPORTED;
}
