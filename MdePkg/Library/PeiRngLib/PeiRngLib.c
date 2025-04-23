/** @file
  RNG library instance that uses the Random Number Generator (RNG) PPI to provide
  random numbers.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PeiServicesLib.h>
#include <Ppi/Rng.h>

/**
  Generates a random number via the NIST 800-9A algorithm. Refer to
  http://csrc.nist.gov/groups/STM/cavp/documents/drbg/DRBGVS.pdf for more information.

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
  EFI_STATUS  Status;
  RNG_PPI     *RngPpi;

  RngPpi = NULL;

  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Buffer == NULL.\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  Status = PeiServicesLocatePpi (&gEfiRngPpiGuid, 0, NULL, (VOID **)&RngPpi);
  if (EFI_ERROR (Status) || (RngPpi == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Could not locate RNG PPI, Status = %r\n", __func__, Status));
    return Status;
  }

  Status = RngPpi->GetRNG (RngPpi, &gEfiRngAlgorithmSp80090Ctr256Guid, BufferSize, Buffer);
  DEBUG ((DEBUG_INFO, "%a: GetRNG algorithm CTR-256 - Status = %r\n", __func__, Status));
  if (!EFI_ERROR (Status)) {
    return Status;
  }

  Status = RngPpi->GetRNG (RngPpi, &gEfiRngAlgorithmSp80090Hmac256Guid, BufferSize, Buffer);
  DEBUG ((DEBUG_INFO, "%a: GetRNG algorithm HMAC-256 - Status = %r\n", __func__, Status));
  if (!EFI_ERROR (Status)) {
    return Status;
  }

  Status = RngPpi->GetRNG (RngPpi, &gEfiRngAlgorithmSp80090Hash256Guid, BufferSize, Buffer);
  DEBUG ((DEBUG_INFO, "%a: GetRNG algorithm Hash-256 - Status = %r\n", __func__, Status));
  if (!EFI_ERROR (Status)) {
    return Status;
  }

  Status = RngPpi->GetRNG (RngPpi, &gEfiRngAlgorithmRaw, BufferSize, Buffer);
  DEBUG ((DEBUG_INFO, "%a: GetRNG algorithm Raw - Status = %r\n", __func__, Status));
  if (!EFI_ERROR (Status)) {
    return Status;
  }

  // If all the other methods have failed, use the default method from the RngPpi
  Status = RngPpi->GetRNG (RngPpi, NULL, BufferSize, Buffer);
  DEBUG ((DEBUG_INFO, "%a: GetRNG algorithm default - Status = %r\n", __func__, Status));
  if (!EFI_ERROR (Status)) {
    return Status;
  }

  // If we get to this point, we have failed
  DEBUG ((DEBUG_ERROR, "%a: GetRNG() failed, staus = %r\n", __func__, Status));

  return Status;
}

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
  // Similar to DxeRngLib, EFI_UNSUPPORTED is returned for this library instance since it is unknown
  // which exact algorithm may be used for a given request.

  return EFI_UNSUPPORTED;
}
