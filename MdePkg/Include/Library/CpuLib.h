/** @file
  Provides CPU architecture specific functions that can not be defined
  in the Base Library due to dependencies on the PAL Library

  The CPU Library provides services to flush CPU TLBs and place the CPU in a sleep state.
  The implementation of these services on Itanium processors requires the use of PAL Calls.
  PAL Calls require PEI and DXE specific mechanisms to look up PAL Entry Point.
  As a result, these services could not be defined in the Base Library.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
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


#endif
