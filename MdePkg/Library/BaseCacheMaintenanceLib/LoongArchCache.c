/** @file
  Cache Maintenance Functions for LoongArch.
  LoongArch cache maintenance functions has not yet been completed, and will added in later.
  Functions are null functions now.

  Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

//
// Include common header file for this module.
//
#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

/**
  LoongArch data barrier operation.
**/
VOID
EFIAPI
AsmDataBarrierLoongArch (
  VOID
  );

/**
  LoongArch instruction barrier operation.
**/
VOID
EFIAPI
AsmInstructionBarrierLoongArch (
  VOID
  );

/**
  Invalidates the entire instruction cache in cache coherency domain of the
  calling CPU.

**/
VOID
EFIAPI
InvalidateInstructionCache (
  VOID
  )
{
  AsmInstructionBarrierLoongArch ();
}

/**
  Invalidates a range of instruction cache lines in the cache coherency domain
  of the calling CPU.

  Invalidates the instruction cache lines specified by Address and Length. If
  Address is not aligned on a cache line boundary, then entire instruction
  cache line containing Address is invalidated. If Address + Length is not
  aligned on a cache line boundary, then the entire instruction cache line
  containing Address + Length -1 is invalidated. This function may choose to
  invalidate the entire instruction cache if that is more efficient than
  invalidating the specified range. If Length is 0, the no instruction cache
  lines are invalidated. Address is returned.

  If Length is greater than (MAX_ADDRESS - Address + 1), then ASSERT().

  @param[in]  Address The base address of the instruction cache lines to
                  invalidate. If the CPU is in a physical addressing mode, then
                  Address is a physical address. If the CPU is in a virtual
                  addressing mode, then Address is a virtual address.

  @param[in]  Length  The number of bytes to invalidate from the instruction cache.

  @return Address.

**/
VOID *
EFIAPI
InvalidateInstructionCacheRange (
  IN       VOID   *Address,
  IN       UINTN  Length
  )
{
  AsmInstructionBarrierLoongArch ();
  return Address;
}

/**
  Writes Back and Invalidates the entire data cache in cache coherency domain
  of the calling CPU.

  Writes Back and Invalidates the entire data cache in cache coherency domain
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
  DEBUG ((DEBUG_ERROR, "%a: Not currently implemented on LoongArch.\n", __func__));
}

/**
  Writes Back and Invalidates a range of data cache lines in the cache
  coherency domain of the calling CPU.

  Writes Back and Invalidate the data cache lines specified by Address and
  Length. If Address is not aligned on a cache line boundary, then entire data
  cache line containing Address is written back and invalidated. If Address +
  Length is not aligned on a cache line boundary, then the entire data cache
  line containing Address + Length -1 is written back and invalidated. This
  function may choose to write back and invalidate the entire data cache if
  that is more efficient than writing back and invalidating the specified
  range. If Length is 0, the no data cache lines are written back and
  invalidated. Address is returned.

  If Length is greater than (MAX_ADDRESS - Address + 1), then ASSERT().

  @param[in]  Address The base address of the data cache lines to write back and
                  invalidate. If the CPU is in a physical addressing mode, then
                  Address is a physical address. If the CPU is in a virtual
                  addressing mode, then Address is a virtual address.
  @param[in]  Length  The number of bytes to write back and invalidate from the
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
  DEBUG ((DEBUG_ERROR, "%a: Not currently implemented on LoongArch.\n", __func__));
  return Address;
}

/**
  Writes Back the entire data cache in cache coherency domain of the calling
  CPU.

  Writes Back the entire data cache in cache coherency domain of the calling
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
  WriteBackInvalidateDataCache ();
}

/**
  Writes Back a range of data cache lines in the cache coherency domain of the
  calling CPU.

  Writes Back the data cache lines specified by Address and Length. If Address
  is not aligned on a cache line boundary, then entire data cache line
  containing Address is written back. If Address + Length is not aligned on a
  cache line boundary, then the entire data cache line containing Address +
  Length -1 is written back. This function may choose to write back the entire
  data cache if that is more efficient than writing back the specified range.
  If Length is 0, the no data cache lines are written back. This function may
  also invalidate all the data cache lines in the specified range of the cache
  coherency domain of the calling CPU. Address is returned.

  If Length is greater than (MAX_ADDRESS - Address + 1), then ASSERT().

  @param[in]  Address The base address of the data cache lines to write back. If
                  the CPU is in a physical addressing mode, then Address is a
                  physical address. If the CPU is in a virtual addressing
                  mode, then Address is a virtual address.
  @param[in]  Length  The number of bytes to write back from the data cache.

  @return Address of cache written in main memory.

**/
VOID *
EFIAPI
WriteBackDataCacheRange (
  IN      VOID   *Address,
  IN      UINTN  Length
  )
{
  DEBUG ((DEBUG_ERROR, "%a: Not currently implemented on LoongArch.\n", __func__));
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
  AsmDataBarrierLoongArch ();
}

/**
  Invalidates a range of data cache lines in the cache coherency domain of the
  calling CPU.

  Invalidates the data cache lines specified by Address and Length. If Address
  is not aligned on a cache line boundary, then entire data cache line
  containing Address is invalidated. If Address + Length is not aligned on a
  cache line boundary, then the entire data cache line containing Address +
  Length -1 is invalidated. This function must never invalidate any cache lines
  outside the specified range. If Length is 0, the no data cache lines are
  invalidated. Address is returned. This function must be used with care
  because dirty cache lines are not written back to system memory. It is
  typically used for cache diagnostics. If the CPU does not support
  invalidation of a data cache range, then a write back and invalidate
  operation should be performed on the data cache range.

  If Length is greater than (MAX_ADDRESS - Address + 1), then ASSERT().

  @param[in]  Address The base address of the data cache lines to invalidate. If
                  the CPU is in a physical addressing mode, then Address is a
                  physical address. If the CPU is in a virtual addressing mode,
                  then Address is a virtual address.
  @param[in]  Length  The number of bytes to invalidate from the data cache.

  @return Address.

**/
VOID *
EFIAPI
InvalidateDataCacheRange (
  IN      VOID   *Address,
  IN      UINTN  Length
  )
{
  AsmDataBarrierLoongArch ();
  return Address;
}
