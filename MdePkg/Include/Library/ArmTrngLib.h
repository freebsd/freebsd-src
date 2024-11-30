/** @file
  Arm TRNG interface library definitions (Cf. [1]).

  Copyright (c) 2021 - 2022, Arm Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Reference(s):
  - [1] Arm True Random Number Generator Firmware, Interface 1.0,
        Platform Design Document.
        (https://developer.arm.com/documentation/den0098/latest/)
  - [2] NIST Special Publication 800-90B, Recommendation for the Entropy
        Sources Used for Random Bit Generation.
        (https://csrc.nist.gov/publications/detail/sp/800-90b/final)

  @par Glossary:
    - TRNG - True Random Number Generator
**/

#ifndef ARM_TRNG_LIB_H_
#define ARM_TRNG_LIB_H_

/** Get the version of the Arm TRNG backend.

  A TRNG may be implemented by the system firmware, in which case this
  function shall return the version of the Arm TRNG backend.
  The implementation must return NOT_SUPPORTED if a Back end is not present.

  @param [out]  MajorRevision     Major revision.
  @param [out]  MinorRevision     Minor revision.

  @retval  RETURN_SUCCESS            The function completed successfully.
  @retval  RETURN_INVALID_PARAMETER  Invalid parameter.
  @retval  RETURN_UNSUPPORTED        Backend not present.
**/
RETURN_STATUS
EFIAPI
GetArmTrngVersion (
  OUT UINT16  *MajorRevision,
  OUT UINT16  *MinorRevision
  );

/** Get the UUID of the Arm TRNG backend.

  A TRNG may be implemented by the system firmware, in which case this
  function shall return the UUID of the TRNG backend.
  Returning the Arm TRNG UUID is optional and if not implemented,
  RETURN_UNSUPPORTED shall be returned.

  Note: The caller must not rely on the returned UUID as a trustworthy Arm TRNG
        Back end identity

  @param [out]  Guid              UUID of the Arm TRNG backend.

  @retval  RETURN_SUCCESS            The function completed successfully.
  @retval  RETURN_INVALID_PARAMETER  Invalid parameter.
  @retval  RETURN_UNSUPPORTED        Function not implemented.
**/
RETURN_STATUS
EFIAPI
GetArmTrngUuid (
  OUT GUID  *Guid
  );

/** Returns maximum number of entropy bits that can be returned in a single
    call.

  @return Returns the maximum number of Entropy bits that can be returned
          in a single call to GetArmTrngEntropy().
**/
UINTN
EFIAPI
GetArmTrngMaxSupportedEntropyBits (
  VOID
  );

/** Returns N bits of conditioned entropy.

  See [2] Section 2.3.1 GetEntropy: An Interface to the Entropy Source
    GetEntropy
      Input:
        bits_of_entropy: the requested amount of entropy
      Output:
        entropy_bitstring: The string that provides the requested entropy.
      status: A Boolean value that is TRUE if the request has been satisfied,
              and is FALSE otherwise.

  @param  [in]   EntropyBits  Number of entropy bits requested.
  @param  [in]   BufferSize   Size of the Buffer in bytes.
  @param  [out]  Buffer       Buffer to return the entropy bits.

  @retval  RETURN_SUCCESS            The function completed successfully.
  @retval  RETURN_INVALID_PARAMETER  Invalid parameter.
  @retval  RETURN_UNSUPPORTED        Function not implemented.
  @retval  RETURN_BAD_BUFFER_SIZE    Buffer size is too small.
  @retval  RETURN_NOT_READY          No Entropy available.
**/
RETURN_STATUS
EFIAPI
GetArmTrngEntropy (
  IN  UINTN  EntropyBits,
  IN  UINTN  BufferSize,
  OUT UINT8  *Buffer
  );

#endif // ARM_TRNG_LIB_H_
