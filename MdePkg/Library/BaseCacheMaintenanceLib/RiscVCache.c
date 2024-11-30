/** @file
  RISC-V specific functionality for cache.

  Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
  Copyright (c) 2023, Rivos Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

//
// TODO: Grab cache block size and make Cache Management Operation
// enabling decision based on RISC-V CPU HOB in
// future when it is available and convert PcdRiscVFeatureOverride
// PCD to a pointer that contains pointer to bitmap structure
// which can be operated more elegantly.
//
#define RISCV_CACHE_BLOCK_SIZE         64
#define RISCV_CPU_FEATURE_CMO_BITMASK  0x1

typedef enum {
  CacheOpClean,
  CacheOpFlush,
  CacheOpInvld,
} CACHE_OP;

/**
Verify CBOs are supported by this HW
TODO: Use RISC-V CPU HOB once available.

**/
STATIC
BOOLEAN
RiscVIsCMOEnabled (
  VOID
  )
{
  // If CMO is disabled in HW, skip Override check
  // Otherwise this PCD can override settings
  return ((PcdGet64 (PcdRiscVFeatureOverride) & RISCV_CPU_FEATURE_CMO_BITMASK) != 0);
}

/**
  Performs required opeartion on cache lines in the cache coherency domain
  of the calling CPU. If Address is not aligned on a cache line boundary,
  then entire cache line containing Address is operated. If Address + Length
  is not aligned on a cache line boundary, then the entire cache line
  containing Address + Length -1 is operated.
  If Length is greater than (MAX_ADDRESS - Address + 1), then ASSERT().
  @param  Address The base address of the cache lines to
          invalidate.
  @param  Length  The number of bytes to invalidate from the instruction
          cache.
  @param  Op  Type of CMO operation to be performed
  @return Address.

**/
STATIC
VOID
CacheOpCacheRange (
  IN VOID      *Address,
  IN UINTN     Length,
  IN CACHE_OP  Op
  )
{
  UINTN  CacheLineSize;
  UINTN  Start;
  UINTN  End;

  if (Length == 0) {
    return;
  }

  if ((Op != CacheOpInvld) && (Op != CacheOpFlush) && (Op != CacheOpClean)) {
    return;
  }

  ASSERT ((Length - 1) <= (MAX_ADDRESS - (UINTN)Address));

  CacheLineSize = RISCV_CACHE_BLOCK_SIZE;

  Start = (UINTN)Address;
  //
  // Calculate the cache line alignment
  //
  End    = (Start + Length + (CacheLineSize - 1)) & ~(CacheLineSize - 1);
  Start &= ~((UINTN)CacheLineSize - 1);

  DEBUG (
    (DEBUG_VERBOSE,
     "CacheOpCacheRange: Performing Cache Management Operation %d \n", Op)
    );

  do {
    switch (Op) {
      case CacheOpInvld:
        RiscVCpuCacheInvalCmoAsm (Start);
        break;
      case CacheOpFlush:
        RiscVCpuCacheFlushCmoAsm (Start);
        break;
      case CacheOpClean:
        RiscVCpuCacheCleanCmoAsm (Start);
        break;
      default:
        break;
    }

    Start = Start + CacheLineSize;
  } while (Start != End);
}

/**
  Invalidates the entire instruction cache in cache coherency domain of the
  calling CPU. Risc-V does not have currently an CBO implementation which can
  invalidate the entire I-cache. Hence using Fence instruction for now. P.S.
  Fence instruction may or may not implement full I-cache invd functionality
  on all implementations.

**/
VOID
EFIAPI
InvalidateInstructionCache (
  VOID
  )
{
  RiscVInvalidateInstCacheFenceAsm ();
}

/**
  Invalidates a range of instruction cache lines in the cache coherency domain
  of the calling CPU.

  An operation from a CMO instruction is defined to operate only on the copies
  of a cache block that are cached in the caches accessible by the explicit
  memory accesses performed by the set of coherent agents.In other words CMO
  operations are not applicable to instruction cache. Use fence.i instruction
  instead to achieve the same purpose.
  @param  Address The base address of the instruction cache lines to
                  invalidate. If the CPU is in a physical addressing mode, then
                  Address is a physical address. If the CPU is in a virtual
                  addressing mode, then Address is a virtual address.

  @param  Length  The number of bytes to invalidate from the instruction cache.

  @return Address.

**/
VOID *
EFIAPI
InvalidateInstructionCacheRange (
  IN VOID   *Address,
  IN UINTN  Length
  )
{
  DEBUG (
    (DEBUG_VERBOSE,
     "InvalidateInstructionCacheRange: RISC-V unsupported function.\n"
     "Invalidating the whole instruction cache instead.\n"
    )
    );
  InvalidateInstructionCache ();
  return Address;
}

/**
  Writes back and invalidates the entire data cache in cache coherency domain
  of the calling CPU.

  Writes back and invalidates the entire data cache in cache coherency domain
  of the calling CPU. This function guarantees that all dirty cache lines are
  written back to system memory, and also invalidates all the data cache lines
  in the cache coherency domain of the calling CPU.

**/
VOID
EFIAPI
WriteBackInvalidateDataCache (
  VOID
  )
{
  ASSERT (FALSE);
  DEBUG ((
    DEBUG_ERROR,
    "WriteBackInvalidateDataCache: RISC-V unsupported function.\n"
    ));
}

/**
  Writes back and invalidates a range of data cache lines in the cache
  coherency domain of the calling CPU.

  Writes back and invalidates the data cache lines specified by Address and
  Length. If Address is not aligned on a cache line boundary, then entire data
  cache line containing Address is written back and invalidated. If Address +
  Length is not aligned on a cache line boundary, then the entire data cache
  line containing Address + Length -1 is written back and invalidated. This
  function may choose to write back and invalidate the entire data cache if
  that is more efficient than writing back and invalidating the specified
  range. If Length is 0, then no data cache lines are written back and
  invalidated. Address is returned.

  If Length is greater than (MAX_ADDRESS - Address + 1), then ASSERT().

  @param  Address The base address of the data cache lines to write back and
                  invalidate. If the CPU is in a physical addressing mode, then
                  Address is a physical address. If the CPU is in a virtual
                  addressing mode, then Address is a virtual address.
  @param  Length  The number of bytes to write back and invalidate from the
                  data cache.

  @return Address of cache invalidation.

**/
VOID *
EFIAPI
WriteBackInvalidateDataCacheRange (
  IN      VOID   *Address,
  IN      UINTN  Length
  )
{
  if (RiscVIsCMOEnabled ()) {
    CacheOpCacheRange (Address, Length, CacheOpFlush);
  } else {
    ASSERT (FALSE);
  }

  return Address;
}

/**
  Writes back the entire data cache in cache coherency domain of the calling
  CPU.

  Writes back the entire data cache in cache coherency domain of the calling
  CPU. This function guarantees that all dirty cache lines are written back to
  system memory. This function may also invalidate all the data cache lines in
  the cache coherency domain of the calling CPU.

**/
VOID
EFIAPI
WriteBackDataCache (
  VOID
  )
{
  ASSERT (FALSE);
}

/**
  Writes back a range of data cache lines in the cache coherency domain of the
  calling CPU.

  Writes back the data cache lines specified by Address and Length. If Address
  is not aligned on a cache line boundary, then entire data cache line
  containing Address is written back. If Address + Length is not aligned on a
  cache line boundary, then the entire data cache line containing Address +
  Length -1 is written back. This function may choose to write back the entire
  data cache if that is more efficient than writing back the specified range.
  If Length is 0, then no data cache lines are written back. This function may
  also invalidate all the data cache lines in the specified range of the cache
  coherency domain of the calling CPU. Address is returned.

  If Length is greater than (MAX_ADDRESS - Address + 1), then ASSERT().

  @param  Address The base address of the data cache lines to write back.
  @param  Length  The number of bytes to write back from the data cache.

  @return Address of cache written in main memory.

**/
VOID *
EFIAPI
WriteBackDataCacheRange (
  IN      VOID   *Address,
  IN      UINTN  Length
  )
{
  if (RiscVIsCMOEnabled ()) {
    CacheOpCacheRange (Address, Length, CacheOpClean);
  } else {
    ASSERT (FALSE);
  }

  return Address;
}

/**
  Invalidates the entire data cache in cache coherency domain of the calling
  CPU.

  Invalidates the entire data cache in cache coherency domain of the calling
  CPU. This function must be used with care because dirty cache lines are not
  written back to system memory. It is typically used for cache diagnostics. If
  the CPU does not support invalidation of the entire data cache, then a write
  back and invalidate operation should be performed on the entire data cache.

**/
VOID
EFIAPI
InvalidateDataCache (
  VOID
  )
{
  RiscVInvalidateDataCacheFenceAsm ();
}

/**
  Invalidates a range of data cache lines in the cache coherency domain of the
  calling CPU.

  Invalidates the data cache lines specified by Address and Length. If Address
  is not aligned on a cache line boundary, then entire data cache line
  containing Address is invalidated. If Address + Length is not aligned on a
  cache line boundary, then the entire data cache line containing Address +
  Length -1 is invalidated. This function must never invalidate any cache lines
  outside the specified range. If Length is 0, then no data cache lines are
  invalidated. Address is returned. This function must be used with care
  because dirty cache lines are not written back to system memory. It is
  typically used for cache diagnostics. If the CPU does not support
  invalidation of a data cache range, then a write back and invalidate
  operation should be performed on the data cache range.

  If Length is greater than (MAX_ADDRESS - Address + 1), then ASSERT().

  @param  Address The base address of the data cache lines to invalidate.
  @param  Length  The number of bytes to invalidate from the data cache.

  @return Address.

**/
VOID *
EFIAPI
InvalidateDataCacheRange (
  IN      VOID   *Address,
  IN      UINTN  Length
  )
{
  if (RiscVIsCMOEnabled ()) {
    CacheOpCacheRange (Address, Length, CacheOpInvld);
  } else {
    DEBUG (
      (DEBUG_VERBOSE,
       "InvalidateDataCacheRange: Zicbom not supported.\n"
       "Invalidating the whole Data cache instead.\n")
      );
    InvalidateDataCache ();
  }

  return Address;
}
