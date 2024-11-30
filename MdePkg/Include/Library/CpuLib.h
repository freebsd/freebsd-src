/** @file
  Provides CPU architecture specific functions that can not be defined
  in the Base Library due to dependencies on the PAL Library

  The CPU Library provides services to flush CPU TLBs and place the CPU in a sleep state.
  The implementation of these services on Itanium processors requires the use of PAL Calls.
  PAL Calls require PEI and DXE specific mechanisms to look up PAL Entry Point.
  As a result, these services could not be defined in the Base Library.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __CPU_LIB_H__
#define __CPU_LIB_H__

/**
  Places the CPU in a sleep state until an interrupt is received.

  Places the CPU in a sleep state until an interrupt is received. If interrupts
  are disabled prior to calling this function, then the CPU will be placed in a
  sleep state indefinitely.

**/
VOID
EFIAPI
CpuSleep (
  VOID
  );

/**
  Flushes all the Translation Lookaside Buffers(TLB) entries in a CPU.

  Flushes all the Translation Lookaside Buffers(TLB) entries in a CPU.

**/
VOID
EFIAPI
CpuFlushTlb (
  VOID
  );

#if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64) || defined (MDE_CPU_LOONGARCH64)

/**
  Initialize the CPU floating point units.

  Initializes floating point units for requirement of UEFI specification.
  For IA32 and X64, this function initializes floating-point control word to 0x027F
  (all exceptions masked,double-precision, round-to-nearest) and multimedia-extensions
  control word (if supported) to 0x1F80 (all exceptions masked, round-to-nearest,
  flush to zero for masked underflow).
**/
VOID
EFIAPI
InitializeFloatingPointUnits (
  VOID
  );

#endif

#if defined (MDE_CPU_IA32) || defined (MDE_CPU_X64)

/**
  Determine if the standard CPU signature is "AuthenticAMD".
  @retval TRUE  The CPU signature matches.
  @retval FALSE The CPU signature does not match.
**/
BOOLEAN
EFIAPI
StandardSignatureIsAuthenticAMD (
  VOID
  );

/**
  Return the 32bit CPU family and model value.
  @return CPUID[01h].EAX with Processor Type and Stepping ID cleared.
**/
UINT32
EFIAPI
GetCpuFamilyModel (
  VOID
  );

/**
  Return the CPU stepping ID.
  @return CPU stepping ID value in CPUID[01h].EAX.
**/
UINT8
EFIAPI
GetCpuSteppingId (
  VOID
  );

#endif

#if defined (MDE_CPU_LOONGARCH64)

/**
  Enable the CPU floating point units.

  Enable the CPU floating point units.
**/
VOID
EFIAPI
EnableFloatingPointUnits (
  VOID
  );

/**
  Disable the CPU floating point units.

  Disable the CPU floating point units.
**/
VOID
EFIAPI
DisableFloatingPointUnits (
  VOID
  );

#endif

#endif
