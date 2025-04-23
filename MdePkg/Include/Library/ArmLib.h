/** @file

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Copyright (c) 2011 - 2016, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2020 - 2021, NUVIA Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ARM_LIB_H_
#define ARM_LIB_H_

#include <Uefi/UefiBaseType.h>

#ifdef MDE_CPU_ARM
  #include <Arm/AArch32.h>
#elif defined (MDE_CPU_AARCH64)
  #include <AArch64/AArch64.h>
#else
  #error "Unknown chipset."
#endif

#define EFI_MEMORY_CACHETYPE_MASK  (EFI_MEMORY_UC | EFI_MEMORY_WC |  \
                                     EFI_MEMORY_WT | EFI_MEMORY_WB | \
                                     EFI_MEMORY_UCE)

typedef enum {
  ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED = 0,
  ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK,

  // On some platforms, memory mapped flash region is designed as not supporting
  // shareable attribute, so WRITE_BACK_NONSHAREABLE is added for such special
  // need.
  // Do NOT use below two attributes if you are not sure.
  ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK_NONSHAREABLE,

  // Special region types for memory that must be mapped with read-only or
  // non-execute permissions from the very start, e.g., to support the use
  // of the WXN virtual memory control.
  ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK_RO,
  ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK_XP,

  ARM_MEMORY_REGION_ATTRIBUTE_WRITE_THROUGH,
  ARM_MEMORY_REGION_ATTRIBUTE_DEVICE,
} ARM_MEMORY_REGION_ATTRIBUTES;

typedef struct {
  EFI_PHYSICAL_ADDRESS            PhysicalBase;
  EFI_VIRTUAL_ADDRESS             VirtualBase;
  UINT64                          Length;
  ARM_MEMORY_REGION_ATTRIBUTES    Attributes;
} ARM_MEMORY_REGION_DESCRIPTOR;

typedef VOID (*CACHE_OPERATION)(
  VOID
  );
typedef VOID (*LINE_OPERATION)(
  UINTN
  );

//
// ARM Processor Mode
//
typedef enum {
  ARM_PROCESSOR_MODE_USER       = 0x10,
  ARM_PROCESSOR_MODE_FIQ        = 0x11,
  ARM_PROCESSOR_MODE_IRQ        = 0x12,
  ARM_PROCESSOR_MODE_SUPERVISOR = 0x13,
  ARM_PROCESSOR_MODE_ABORT      = 0x17,
  ARM_PROCESSOR_MODE_HYP        = 0x1A,
  ARM_PROCESSOR_MODE_UNDEFINED  = 0x1B,
  ARM_PROCESSOR_MODE_SYSTEM     = 0x1F,
  ARM_PROCESSOR_MODE_MASK       = 0x1F
} ARM_PROCESSOR_MODE;

//
// ARM Cpu IDs
//
#define ARM_CPU_IMPLEMENTER_MASK      (0xFFU << 24)
#define ARM_CPU_IMPLEMENTER_ARMLTD    (0x41U << 24)
#define ARM_CPU_IMPLEMENTER_DEC       (0x44U << 24)
#define ARM_CPU_IMPLEMENTER_MOT       (0x4DU << 24)
#define ARM_CPU_IMPLEMENTER_QUALCOMM  (0x51U << 24)
#define ARM_CPU_IMPLEMENTER_MARVELL   (0x56U << 24)

#define ARM_CPU_PRIMARY_PART_MASK       (0xFFF << 4)
#define ARM_CPU_PRIMARY_PART_CORTEXA5   (0xC05 << 4)
#define ARM_CPU_PRIMARY_PART_CORTEXA7   (0xC07 << 4)
#define ARM_CPU_PRIMARY_PART_CORTEXA8   (0xC08 << 4)
#define ARM_CPU_PRIMARY_PART_CORTEXA9   (0xC09 << 4)
#define ARM_CPU_PRIMARY_PART_CORTEXA15  (0xC0F << 4)

//
// ARM MP Core IDs
//
#define ARM_CORE_AFF0  0xFF
#define ARM_CORE_AFF1  (0xFF << 8)
#define ARM_CORE_AFF2  (0xFF << 16)
#define ARM_CORE_AFF3  (0xFFULL << 32)

#define ARM_CORE_MASK     ARM_CORE_AFF0
#define ARM_CLUSTER_MASK  ARM_CORE_AFF1
#define GET_CORE_ID(MpId)              ((MpId) & ARM_CORE_MASK)
#define GET_CLUSTER_ID(MpId)           (((MpId) & ARM_CLUSTER_MASK) >> 8)
#define GET_MPID(ClusterId, CoreId)    (((ClusterId) << 8) | (CoreId))
#define GET_MPIDR_AFF0(MpId)           ((MpId) & ARM_CORE_AFF0)
#define GET_MPIDR_AFF1(MpId)           (((MpId) & ARM_CORE_AFF1) >> 8)
#define GET_MPIDR_AFF2(MpId)           (((MpId) & ARM_CORE_AFF2) >> 16)
#define GET_MPIDR_AFF3(MpId)           (((MpId) & ARM_CORE_AFF3) >> 32)
#define GET_MPIDR_AFFINITY_BITS(MpId)  ((MpId) & 0xFF00FFFFFF)
#define PRIMARY_CORE_ID  (PcdGet32(PcdArmPrimaryCore) & ARM_CORE_MASK)
#define MPIDR_MT_BIT     BIT24

/** Reads the CCSIDR register for the specified cache.

  @param CSSELR The CSSELR cache selection register value.

  @return The contents of the CCSIDR_EL1 register for the specified cache, when in AARCH64 mode.
          Returns the contents of the CCSIDR register in AARCH32 mode.
**/
UINTN
ReadCCSIDR (
  IN UINT32  CSSELR
  );

/** Reads the CCSIDR2 for the specified cache.

  @param CSSELR The CSSELR cache selection register value

  @return The contents of the CCSIDR2 register for the specified cache.
**/
UINT32
ReadCCSIDR2 (
  IN UINT32  CSSELR
  );

/** Reads the Cache Level ID (CLIDR) register.

  @return The contents of the CLIDR_EL1 register.
**/
UINT32
ReadCLIDR (
  VOID
  );

UINTN
EFIAPI
ArmDataCacheLineLength (
  VOID
  );

UINTN
EFIAPI
ArmInstructionCacheLineLength (
  VOID
  );

UINT32
EFIAPI
ArmCacheWritebackGranule (
  VOID
  );

UINTN
EFIAPI
ArmIsArchTimerImplemented (
  VOID
  );

UINTN
EFIAPI
ArmCacheInfo (
  VOID
  );

BOOLEAN
EFIAPI
ArmIsMpCore (
  VOID
  );

VOID
EFIAPI
ArmInvalidateInstructionCache (
  VOID
  );

VOID
EFIAPI
ArmInvalidateDataCacheEntryByMVA (
  IN  UINTN  Address
  );

VOID
EFIAPI
ArmCleanDataCacheEntryToPoUByMVA (
  IN  UINTN  Address
  );

VOID
EFIAPI
ArmInvalidateInstructionCacheEntryToPoUByMVA (
  IN  UINTN  Address
  );

VOID
EFIAPI
ArmCleanDataCacheEntryByMVA (
  IN  UINTN  Address
  );

VOID
EFIAPI
ArmCleanInvalidateDataCacheEntryByMVA (
  IN  UINTN  Address
  );

VOID
EFIAPI
ArmEnableDataCache (
  VOID
  );

VOID
EFIAPI
ArmDisableDataCache (
  VOID
  );

VOID
EFIAPI
ArmEnableInstructionCache (
  VOID
  );

VOID
EFIAPI
ArmDisableInstructionCache (
  VOID
  );

VOID
EFIAPI
ArmEnableMmu (
  VOID
  );

VOID
EFIAPI
ArmDisableMmu (
  VOID
  );

VOID
EFIAPI
ArmEnableCachesAndMmu (
  VOID
  );

VOID
EFIAPI
ArmDisableCachesAndMmu (
  VOID
  );

VOID
EFIAPI
ArmEnableInterrupts (
  VOID
  );

UINTN
EFIAPI
ArmDisableInterrupts (
  VOID
  );

BOOLEAN
EFIAPI
ArmGetInterruptState (
  VOID
  );

VOID
EFIAPI
ArmEnableAsynchronousAbort (
  VOID
  );

UINTN
EFIAPI
ArmDisableAsynchronousAbort (
  VOID
  );

VOID
EFIAPI
ArmEnableIrq (
  VOID
  );

UINTN
EFIAPI
ArmDisableIrq (
  VOID
  );

VOID
EFIAPI
ArmEnableFiq (
  VOID
  );

UINTN
EFIAPI
ArmDisableFiq (
  VOID
  );

BOOLEAN
EFIAPI
ArmGetFiqState (
  VOID
  );

/**
 * Invalidate Data and Instruction TLBs
 */
VOID
EFIAPI
ArmInvalidateTlb (
  VOID
  );

VOID
EFIAPI
ArmUpdateTranslationTableEntry (
  IN  VOID  *TranslationTableEntry,
  IN  VOID  *Mva
  );

VOID
EFIAPI
ArmSetDomainAccessControl (
  IN  UINT32  Domain
  );

VOID
EFIAPI
ArmSetTTBR0 (
  IN  VOID  *TranslationTableBase
  );

VOID
EFIAPI
ArmSetTTBCR (
  IN  UINT32  Bits
  );

VOID *
EFIAPI
ArmGetTTBR0BaseAddress (
  VOID
  );

BOOLEAN
EFIAPI
ArmMmuEnabled (
  VOID
  );

VOID
EFIAPI
ArmEnableBranchPrediction (
  VOID
  );

VOID
EFIAPI
ArmDisableBranchPrediction (
  VOID
  );

VOID
EFIAPI
ArmSetLowVectors (
  VOID
  );

VOID
EFIAPI
ArmSetHighVectors (
  VOID
  );

VOID
EFIAPI
ArmDataMemoryBarrier (
  VOID
  );

VOID
EFIAPI
ArmDataSynchronizationBarrier (
  VOID
  );

VOID
EFIAPI
ArmInstructionSynchronizationBarrier (
  VOID
  );

VOID
EFIAPI
ArmWriteVBar (
  IN  UINTN  VectorBase
  );

UINTN
EFIAPI
ArmReadVBar (
  VOID
  );

VOID
EFIAPI
ArmWriteAuxCr (
  IN  UINT32  Bit
  );

UINT32
EFIAPI
ArmReadAuxCr (
  VOID
  );

VOID
EFIAPI
ArmSetAuxCrBit (
  IN  UINT32  Bits
  );

VOID
EFIAPI
ArmUnsetAuxCrBit (
  IN  UINT32  Bits
  );

VOID
EFIAPI
ArmCallSEV (
  VOID
  );

VOID
EFIAPI
ArmCallWFE (
  VOID
  );

VOID
EFIAPI
ArmCallWFI (

  VOID
  );

UINTN
EFIAPI
ArmReadMpidr (
  VOID
  );

UINTN
EFIAPI
ArmReadMidr (
  VOID
  );

UINT32
EFIAPI
ArmReadCpacr (
  VOID
  );

VOID
EFIAPI
ArmWriteCpacr (
  IN  UINT32  Access
  );

VOID
EFIAPI
ArmEnableVFP (
  VOID
  );

UINT32
EFIAPI
ArmReadSctlr (
  VOID
  );

VOID
EFIAPI
ArmWriteSctlr (
  IN  UINT32  Value
  );

UINTN
EFIAPI
ArmReadHVBar (
  VOID
  );

VOID
EFIAPI
ArmWriteHVBar (
  IN  UINTN  HypModeVectorBase
  );

//
// Helper functions for accessing CPU ACTLR
//

UINTN
EFIAPI
ArmReadCpuActlr (
  VOID
  );

VOID
EFIAPI
ArmWriteCpuActlr (
  IN  UINTN  Val
  );

VOID
EFIAPI
ArmSetCpuActlrBit (
  IN  UINTN  Bits
  );

VOID
EFIAPI
ArmUnsetCpuActlrBit (
  IN  UINTN  Bits
  );

//
// Accessors for the architected generic timer registers
//

#define ARM_ARCH_TIMER_ENABLE   (1 << 0)
#define ARM_ARCH_TIMER_IMASK    (1 << 1)
#define ARM_ARCH_TIMER_ISTATUS  (1 << 2)

UINTN
EFIAPI
ArmReadCntFrq (
  VOID
  );

VOID
EFIAPI
ArmWriteCntFrq (
  UINTN  FreqInHz
  );

UINT64
EFIAPI
ArmReadCntPct (
  VOID
  );

UINTN
EFIAPI
ArmReadCntkCtl (
  VOID
  );

VOID
EFIAPI
ArmWriteCntkCtl (
  UINTN  Val
  );

UINTN
EFIAPI
ArmReadCntpTval (
  VOID
  );

VOID
EFIAPI
ArmWriteCntpTval (
  UINTN  Val
  );

UINTN
EFIAPI
ArmReadCntpCtl (
  VOID
  );

VOID
EFIAPI
ArmWriteCntpCtl (
  UINTN  Val
  );

UINTN
EFIAPI
ArmReadCntvTval (
  VOID
  );

VOID
EFIAPI
ArmWriteCntvTval (
  UINTN  Val
  );

UINTN
EFIAPI
ArmReadCntvCtl (
  VOID
  );

VOID
EFIAPI
ArmWriteCntvCtl (
  UINTN  Val
  );

UINT64
EFIAPI
ArmReadCntvCt (
  VOID
  );

UINT64
EFIAPI
ArmReadCntpCval (
  VOID
  );

VOID
EFIAPI
ArmWriteCntpCval (
  UINT64  Val
  );

UINT64
EFIAPI
ArmReadCntvCval (
  VOID
  );

VOID
EFIAPI
ArmWriteCntvCval (
  UINT64  Val
  );

UINT64
EFIAPI
ArmReadCntvOff (
  VOID
  );

VOID
EFIAPI
ArmWriteCntvOff (
  UINT64  Val
  );

UINTN
EFIAPI
ArmGetPhysicalAddressBits (
  VOID
  );

///
///  ID Register Helper functions
///

/**
  Check whether the CPU supports the GIC system register interface (any version)

  @return   Whether GIC System Register Interface is supported

**/
BOOLEAN
EFIAPI
ArmHasGicSystemRegisters (
  VOID
  );

/** Checks if CCIDX is implemented.

   @retval TRUE  CCIDX is implemented.
   @retval FALSE CCIDX is not implemented.
**/
BOOLEAN
EFIAPI
ArmHasCcidx (
  VOID
  );

#ifdef MDE_CPU_AARCH64
///
/// AArch64-only ID Register Helper functions
///

/**
  Checks whether the CPU implements the Virtualization Host Extensions.

  @retval TRUE  FEAT_VHE is implemented.
  @retval FALSE FEAT_VHE is not mplemented.
**/
BOOLEAN
EFIAPI
ArmHasVhe (
  VOID
  );

/**
  Checks whether the CPU implements the Trace Buffer Extension.

  @retval TRUE  FEAT_TRBE is implemented.
  @retval FALSE FEAT_TRBE is not mplemented.
**/
BOOLEAN
EFIAPI
ArmHasTrbe (
  VOID
  );

/**
  Checks whether the CPU implements the Embedded Trace Extension.

  @retval TRUE  FEAT_ETE is implemented.
  @retval FALSE FEAT_ETE is not mplemented.
**/
BOOLEAN
EFIAPI
ArmHasEte (
  VOID
  );

#endif // MDE_CPU_AARCH64

#ifdef MDE_CPU_ARM
///
/// AArch32-only ID Register Helper functions
///

/**
  Check whether the CPU supports the Security extensions

  @return   Whether the Security extensions are implemented

**/
BOOLEAN
EFIAPI
ArmHasSecurityExtensions (
  VOID
  );

#endif // MDE_CPU_ARM

#endif // ARM_LIB_H_
