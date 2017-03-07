/** @file
  AsmFlushCacheLine function

  Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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

