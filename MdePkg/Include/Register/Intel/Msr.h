/** @file
  MSR Definitions.

  Provides defines for Machine Specific Registers(MSR) indexes. Data structures
  are provided for MSRs that contain one or more bit fields.  If the MSR value
  returned is a single 32-bit or 64-bit value, then a data structure is not
  provided for that MSR.

  Copyright (c) 2016 ~ 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  Intel(R) 64 and IA-32 Architectures Software Developer's Manual, Volume 4,
  May 2018, Volume 4: Model-Specific-Registers (MSR)

**/

#ifndef __INTEL_MSR_H__
#define __INTEL_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>
#include <Register/Intel/Msr/Core2Msr.h>
#include <Register/Intel/Msr/AtomMsr.h>
#include <Register/Intel/Msr/SilvermontMsr.h>
#include <Register/Intel/Msr/GoldmontMsr.h>
#include <Register/Intel/Msr/GoldmontPlusMsr.h>
#include <Register/Intel/Msr/NehalemMsr.h>
#include <Register/Intel/Msr/Xeon5600Msr.h>
#include <Register/Intel/Msr/XeonE7Msr.h>
#include <Register/Intel/Msr/SandyBridgeMsr.h>
#include <Register/Intel/Msr/IvyBridgeMsr.h>
#include <Register/Intel/Msr/HaswellMsr.h>
#include <Register/Intel/Msr/HaswellEMsr.h>
#include <Register/Intel/Msr/BroadwellMsr.h>
#include <Register/Intel/Msr/XeonDMsr.h>
#include <Register/Intel/Msr/SkylakeMsr.h>
#include <Register/Intel/Msr/XeonPhiMsr.h>
#include <Register/Intel/Msr/Pentium4Msr.h>
#include <Register/Intel/Msr/CoreMsr.h>
#include <Register/Intel/Msr/PentiumMMsr.h>
#include <Register/Intel/Msr/P6Msr.h>
#include <Register/Intel/Msr/PentiumMsr.h>

#endif
