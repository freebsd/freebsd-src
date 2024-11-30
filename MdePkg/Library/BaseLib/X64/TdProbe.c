/** @file

  Copyright (c) 2020-2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Register/Intel/Cpuid.h>

/**
  Probe if TD is enabled.

  @return TRUE    TD is enabled.
  @return FALSE   TD is not enabled.
**/
BOOLEAN
EFIAPI
TdIsEnabled (
  )
{
  UINT32                  Eax;
  UINT32                  Ebx;
  UINT32                  Ecx;
  UINT32                  Edx;
  UINT32                  LargestEax;
  BOOLEAN                 TdEnabled;
  CPUID_VERSION_INFO_ECX  CpuIdVersionInfoEcx;

  TdEnabled = FALSE;

  do {
    AsmCpuid (CPUID_SIGNATURE, &LargestEax, &Ebx, &Ecx, &Edx);

    if (  (Ebx != CPUID_SIGNATURE_GENUINE_INTEL_EBX)
       || (Edx != CPUID_SIGNATURE_GENUINE_INTEL_EDX)
       || (Ecx != CPUID_SIGNATURE_GENUINE_INTEL_ECX))
    {
      break;
    }

    AsmCpuid (CPUID_VERSION_INFO, NULL, NULL, &CpuIdVersionInfoEcx.Uint32, NULL);
    if (CpuIdVersionInfoEcx.Bits.ParaVirtualized == 0) {
      break;
    }

    if (LargestEax < CPUID_GUESTTD_RUNTIME_ENVIRONMENT) {
      break;
    }

    AsmCpuidEx (CPUID_GUESTTD_RUNTIME_ENVIRONMENT, 0, &Eax, &Ebx, &Ecx, &Edx);
    if (  (Ebx != CPUID_GUESTTD_SIGNATURE_GENUINE_INTEL_EBX)
       || (Edx != CPUID_GUESTTD_SIGNATURE_GENUINE_INTEL_EDX)
       || (Ecx != CPUID_GUESTTD_SIGNATURE_GENUINE_INTEL_ECX))
    {
      break;
    }

    TdEnabled = TRUE;
  } while (FALSE);

  return TdEnabled;
}
