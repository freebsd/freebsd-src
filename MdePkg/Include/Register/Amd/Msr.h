/** @file
  MSR Definitions.

  Provides defines for Machine Specific Registers(MSR) indexes. Data structures
  are provided for MSRs that contain one or more bit fields.  If the MSR value
  returned is a single 32-bit or 64-bit value, then a data structure is not
  provided for that MSR.

  Copyright (c) 2017 - 2024, Advanced Micro Devices. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  AMD64 Architecture Programming Manual volume 2, March 2024

**/

#ifndef AMD_MSR_H_
#define AMD_MSR_H_

#include <Register/Intel/ArchitecturalMsr.h>
#include <Register/Amd/ArchitecturalMsr.h>
#include <Register/Amd/SevSnpMsr.h>
#include <Register/Amd/SvsmMsr.h>

#endif
