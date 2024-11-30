#------------------------------------------------------------------------------
#
# LoongArch synchronization ASM functions.
#
# Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(AsmInternalSyncCompareExchange16)
ASM_GLOBAL ASM_PFX(AsmInternalSyncCompareExchange32)
ASM_GLOBAL ASM_PFX(AsmInternalSyncCompareExchange64)
ASM_GLOBAL ASM_PFX(AsmInternalSyncIncrement)
ASM_GLOBAL ASM_PFX(AsmInternalSyncDecrement)

/**
UINT32
EFIAPI
AsmInternalSyncCompareExchange16 (
  IN volatile UINT32 *Ptr32,
  IN UINT64          Mask,
  IN UINT64          LocalCompareValue,
  IN UINT64          LocalExchangeValue
  )
**/
ASM_PFX(AsmInternalSyncCompareExchange16):
1:
  ll.w  $t0, $a0, 0x0
  and   $t1, $t0, $a1
  bne   $t1, $a2, 2f
  andn  $t1, $t0, $a1
  or    $t1, $t1, $a3
  sc.w  $t1, $a0, 0x0
  beqz  $t1, 1b
  b     3f
2:
  dbar  0
3:
  move   $a0, $t0
  jirl   $zero, $ra, 0

/**
UINT32
EFIAPI
AsmInternalSyncCompareExchange32 (
  IN volatile UINT32 *Value,
  IN UINT64          CompareValue,
  IN UINT64          ExchangeValue
  )
**/
ASM_PFX(AsmInternalSyncCompareExchange32):
1:
  ll.w  $t0, $a0, 0x0
  bne   $t0, $a1, 2f
  move  $t1, $a2
  sc.w  $t1, $a0, 0x0
  beqz  $t1, 1b
  b     3f
2:
  dbar  0
3:
  move   $a0, $t0
  jirl   $zero, $ra, 0

/**
UINT64
EFIAPI
AsmInternalSyncCompareExchange64 (
  IN volatile UINT64 *Value,
  IN UINT64          CompareValue,
  IN UINT64          ExchangeValue
  )
**/
ASM_PFX(AsmInternalSyncCompareExchange64):
1:
  ll.d  $t0, $a0, 0x0
  bne   $t0, $a1, 2f
  move  $t1, $a2
  sc.d  $t1, $a0, 0x0
  beqz  $t1, 1b
  b     3f
2:
  dbar  0
3:
  move   $a0, $t0
  jirl   $zero, $ra, 0

/**
UINT32
EFIAPI
AsmInternalSyncIncrement (
  IN      volatile UINT32  *Value
  )
**/
ASM_PFX(AsmInternalSyncIncrement):
  li.w     $t0, 1
  amadd.w  $zero, $t0, $a0

  ld.w     $a0, $a0, 0
  jirl     $zero, $ra, 0

/**
UINT32
EFIAPI
AsmInternalSyncDecrement (
  IN      volatile UINT32  *Value
  )
**/
ASM_PFX(AsmInternalSyncDecrement):
  li.w     $t0, -1
  amadd.w  $zero, $t0, $a0

  ld.w     $a0, $a0, 0
  jirl     $zero, $ra, 0
.end
