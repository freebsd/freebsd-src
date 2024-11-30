/** @file
  Null instance of CPU Library for IA32/X64 specific services.

  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/CpuLib.h>

/**
  Initializes floating point units for requirement of UEFI specification.
  This function initializes floating-point control word to 0x027F (all exceptions
  masked,double-precision, round-to-nearest) and multimedia-extensions control word
  (if supported) to 0x1F80 (all exceptions masked, round-to-nearest, flush to zero
  for masked underflow).
**/
VOID
EFIAPI
InitializeFloatingPointUnits (
  VOID
  )
{
}

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
  return FALSE;
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
  return 0;
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
  return 0;
}
