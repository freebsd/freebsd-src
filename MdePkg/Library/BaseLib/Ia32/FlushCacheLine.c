/** @file
  AsmFlushCacheLine function

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/




/**
  Flushes a cache line from all the instruction and data caches within the
  coherency domain of the CPU.

  Flushed the cache line specified by LinearAddress, and returns LinearAddress.
  This function is only available on IA-32 and x64.

  @param  LinearAddress The address of the cache line to flush. If the CPU is
                        in a physical addressing mode, then LinearAddress is a
                        physical address. If the CPU is in a virtual
                        addressing mode, then LinearAddress is a virtual
                        address.

  @return LinearAddress
**/
VOID *
EFIAPI
AsmFlushCacheLine (
  IN      VOID                      *LinearAddress
  )
{
  //
  // If the CPU does not support CLFLUSH instruction,
  // then promote flush range to flush entire cache.
  //
  _asm {
    mov     eax, 1
    cpuid
    test    edx, BIT19
    jz      NoClflush
    mov     eax, dword ptr [LinearAddress]
    clflush [eax]
    jmp     Done
NoClflush:
    wbinvd
Done:
  }

  return LinearAddress;
}

