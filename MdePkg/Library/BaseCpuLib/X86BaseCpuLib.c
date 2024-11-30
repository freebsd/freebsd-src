/** @file
  This library defines some routines that are generic for IA32 family CPU.

  The library routines are UEFI specification compliant.

  Copyright (c) 2020, AMD Inc. All rights reserved.<BR>
  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Register/Intel/Cpuid.h>
#include <Register/Amd/Cpuid.h>

#include <Library/BaseLib.h>
#include <Library/CpuLib.h>

/**
  Determine if the standard CPU signature is "AuthenticAMD".

  @retval TRUE  The CPU signature matches.
  @retval FALSE The CPU signature does not match.

**/
BOOLEAN
EFIAPI
StandardSignatureIsAuthenticAMD (
  VOID
  )
{
  UINT32  RegEbx;
  UINT32  RegEcx;
  UINT32  RegEdx;

  AsmCpuid (CPUID_SIGNATURE, NULL, &RegEbx, &RegEcx, &RegEdx);
  return (RegEbx == CPUID_SIGNATURE_AUTHENTIC_AMD_EBX &&
          RegEcx == CPUID_SIGNATURE_AUTHENTIC_AMD_ECX &&
          RegEdx == CPUID_SIGNATURE_AUTHENTIC_AMD_EDX);
}

/**
  Return the 32bit CPU family and model value.

  @return CPUID[01h].EAX with Processor Type and Stepping ID cleared.
**/
UINT32
EFIAPI
GetCpuFamilyModel (
  VOID
  )
{
  CPUID_VERSION_INFO_EAX  Eax;

  AsmCpuid (CPUID_VERSION_INFO, &Eax.Uint32, NULL, NULL, NULL);

  //
  // Mask other fields than Family and Model.
  //
  Eax.Bits.SteppingId    = 0;
  Eax.Bits.ProcessorType = 0;
  Eax.Bits.Reserved1     = 0;
  Eax.Bits.Reserved2     = 0;
  return Eax.Uint32;
}

/**
  Return the CPU stepping ID.
  @return CPU stepping ID value in CPUID[01h].EAX.
**/
UINT8
EFIAPI
GetCpuSteppingId (
  VOID
  )
{
  CPUID_VERSION_INFO_EAX  Eax;

  AsmCpuid (CPUID_VERSION_INFO, &Eax.Uint32, NULL, NULL, NULL);

  return (UINT8)Eax.Bits.SteppingId;
}
