#------------------------------------------------------------------------------
#
# LoongArch ASM CSR operation functions
#
# Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <Register/LoongArch64/Csr.h>

ASM_GLOBAL ASM_PFX (AsmCsrRead)
ASM_GLOBAL ASM_PFX (AsmCsrWrite)
ASM_GLOBAL ASM_PFX (AsmCsrXChg)

.macro AsmCsrRd Sel
  csrrd   $a0, \Sel
  jirl    $zero, $ra, 0
.endm

.macro AsmCsrWr Sel
  csrwr   $a0, \Sel
  jirl    $zero, $ra, 0
.endm

.macro AsmCsrXChange Sel
  csrxchg $a0, $a1, \Sel
  jirl    $zero, $ra, 0
.endm

ASM_PFX(AsmCsrRead):
  blt      $a0, $zero, ReadSelNumErr
  li.w     $t0, LOONGARCH_CSR_EBASE
  bltu     $t0, $a0, TlbCsrRd

BasicCsrRd:
  la.pcrel $t0, BasicCsrRead
  alsl.d   $t0, $a0, $t0, 3
  jirl     $zero, $t0, 0

TlbCsrRd:
  li.w     $t0, LOONGARCH_CSR_TLBIDX
  bltu     $a0, $t0, ReadSelNumErr
  li.w     $t0, LOONGARCH_CSR_RVACFG
  bltu     $t0, $a0, CfgCsrRd
  la.pcrel $t0, TlbCsrRead
  addi.w   $t1, $a0, -LOONGARCH_CSR_TLBIDX
  alsl.d   $t0, $t1, $t0, 3
  jirl     $zero, $t0, 0

CfgCsrRd:
  li.w     $t0, LOONGARCH_CSR_CPUID
  bltu     $a0, $t0, ReadSelNumErr
  li.w     $t0, LOONGARCH_CSR_PRCFG3
  bltu     $t0, $a0, KcsCsrRd
  la.pcrel $t0, CfgCsrRead
  addi.w   $t1, $a0, -LOONGARCH_CSR_CPUID
  alsl.d   $t0, $t1, $t0, 3
  jirl     $zero, $t0, 0

KcsCsrRd:
  li.w     $t0, LOONGARCH_CSR_KS0
  bltu     $a0, $t0, ReadSelNumErr
  li.w     $t0, LOONGARCH_CSR_KS8
  bltu     $t0, $a0, StableTimerCsrRd
  la.pcrel $t0, KcsCsrRead
  addi.w   $t1, $a0, -LOONGARCH_CSR_KS0
  alsl.d   $t0, $t1, $t0, 3
  jirl     $zero, $t0, 0

StableTimerCsrRd:
  li.w     $t0, LOONGARCH_CSR_TMID
  bltu     $a0, $t0, ReadSelNumErr
  li.w     $t0, LOONGARCH_CSR_TINTCLR
  bltu     $t0, $a0, TlbRefillCsrRd
  la.pcrel $t0, StableTimerCsrRead
  addi.w   $t1, $a0, -LOONGARCH_CSR_TMID
  alsl.d   $t0, $t1, $t0, 3
  jirl     $zero, $t0, 0

TlbRefillCsrRd:
  li.w     $t0, LOONGARCH_CSR_TLBREBASE
  bltu     $a0, $t0, ReadSelNumErr
  li.w     $t0, LOONGARCH_CSR_TLBREHI
  bltu     $t0, $a0, DirMapCsrRd
  la.pcrel $t0, TlbRefillCsrRead
  addi.w   $t1, $a0, -LOONGARCH_CSR_TLBREBASE
  alsl.d   $t0, $t1, $t0, 3
  jirl     $zero, $t0, 0

DirMapCsrRd:
  li.w     $t0, LOONGARCH_CSR_DMWIN0
  bltu     $a0, $t0, ReadSelNumErr
  li.w     $t0, LOONGARCH_CSR_DMWIN3
  bltu     $t0, $a0, ReadSelNumErr
  la.pcrel $t0, DirMapCsrRead
  addi.w   $t1, $a0, -LOONGARCH_CSR_DMWIN0
  alsl.d   $t0, $t1, $t0, 3
  jirl     $zero, $t0, 0

ReadSelNumErr:
  break    0

BasicCsrRead:
  CsrSel = LOONGARCH_CSR_CRMD
  .rept LOONGARCH_CSR_EBASE - LOONGARCH_CSR_CRMD + 1
    AsmCsrRd CsrSel
    CsrSel = CsrSel + 1
  .endr

TlbCsrRead:
  CsrSel = LOONGARCH_CSR_TLBIDX
  .rept LOONGARCH_CSR_RVACFG - LOONGARCH_CSR_TLBIDX + 1
    AsmCsrRd CsrSel
    CsrSel = CsrSel + 1
  .endr

CfgCsrRead:
  CsrSel = LOONGARCH_CSR_CPUID
  .rept LOONGARCH_CSR_PRCFG3 - LOONGARCH_CSR_CPUID + 1
    AsmCsrRd CsrSel
    CsrSel = CsrSel + 1
  .endr

KcsCsrRead:
  CsrSel = LOONGARCH_CSR_KS0
  .rept LOONGARCH_CSR_KS8 - LOONGARCH_CSR_KS0 + 1
    AsmCsrRd CsrSel
    CsrSel = CsrSel + 1
  .endr

StableTimerCsrRead:
  CsrSel = LOONGARCH_CSR_TMID
  .rept LOONGARCH_CSR_TINTCLR - LOONGARCH_CSR_TMID + 1
    AsmCsrRd CsrSel
    CsrSel = CsrSel + 1
  .endr

TlbRefillCsrRead:
  CsrSel = LOONGARCH_CSR_TLBREBASE
  .rept LOONGARCH_CSR_TLBREHI - LOONGARCH_CSR_TLBREBASE + 1
    AsmCsrRd CsrSel
    CsrSel = CsrSel + 1
  .endr

DirMapCsrRead:
  CsrSel = LOONGARCH_CSR_DMWIN0
  .rept LOONGARCH_CSR_DMWIN3 - LOONGARCH_CSR_DMWIN0 + 1
    AsmCsrRd CsrSel
    CsrSel = CsrSel + 1
  .endr

ASM_PFX(AsmCsrWrite):
  blt      $a0, $zero, WriteSelNumErr
  li.w     $t0, LOONGARCH_CSR_EBASE
  bltu     $t0, $a0, TlbCsrWr

BasicCsrWr:
  la.pcrel $t0, BasicCsrWrite
  alsl.d   $t0, $a0, $t0, 3
  move     $a0, $a1
  jirl     $zero, $t0, 0

TlbCsrWr:
  li.w     $t0, LOONGARCH_CSR_TLBIDX
  bltu     $a0, $t0, WriteSelNumErr
  li.w     $t0, LOONGARCH_CSR_RVACFG
  bltu     $t0, $a0, CfgCsrWr
  la.pcrel $t0, TlbCsrWrite
  addi.w   $t1, $a0, -LOONGARCH_CSR_TLBIDX
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  jirl     $zero, $t0, 0

CfgCsrWr:
  li.w     $t0, LOONGARCH_CSR_CPUID
  bltu     $a0, $t0, WriteSelNumErr
  li.w     $t0, LOONGARCH_CSR_PRCFG3
  bltu     $t0, $a0, KcsCsrWr
  la.pcrel $t0, CfgCsrWrite
  addi.w   $t1, $a0, -LOONGARCH_CSR_CPUID
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  jirl     $zero, $t0, 0

KcsCsrWr:
  li.w     $t0, LOONGARCH_CSR_KS0
  bltu     $a0, $t0, WriteSelNumErr
  li.w     $t0, LOONGARCH_CSR_KS8
  bltu     $t0, $a0, StableTimerCsrWr
  la.pcrel $t0, KcsCsrWrite
  addi.w   $t1, $a0, -LOONGARCH_CSR_KS0
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  jirl     $zero, $t0, 0

StableTimerCsrWr:
  li.w     $t0, LOONGARCH_CSR_TMID
  bltu     $a0, $t0, WriteSelNumErr
  li.w     $t0, LOONGARCH_CSR_TINTCLR
  bltu     $t0, $a0, TlbRefillCsrWr
  la.pcrel $t0, StableTimerCsrWrite
  addi.w   $t1, $a0, -LOONGARCH_CSR_TMID
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  jirl     $zero, $t0, 0

TlbRefillCsrWr:
  li.w     $t0, LOONGARCH_CSR_TLBREBASE
  bltu     $a0, $t0, WriteSelNumErr
  li.w     $t0, LOONGARCH_CSR_TLBREHI
  bltu     $t0, $a0, DirMapCsrWr
  la.pcrel $t0, TlbRefillCsrWrite
  addi.w   $t1, $a0, -LOONGARCH_CSR_TLBREBASE
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  jirl     $zero, $t0, 0

DirMapCsrWr:
  li.w     $t0, LOONGARCH_CSR_DMWIN0
  bltu     $a0, $t0, WriteSelNumErr
  li.w     $t0, LOONGARCH_CSR_DMWIN3
  bltu     $t0, $a0, WriteSelNumErr
  la.pcrel $t0, DirMapCsrWrite
  addi.w   $t1, $a0, -LOONGARCH_CSR_DMWIN0
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  jirl     $zero, $t0, 0

WriteSelNumErr:
  break    0

BasicCsrWrite:
  CsrSel = LOONGARCH_CSR_CRMD
  .rept LOONGARCH_CSR_EBASE - LOONGARCH_CSR_CRMD + 1
    AsmCsrWr CsrSel
    CsrSel = CsrSel + 1
  .endr

TlbCsrWrite:
  CsrSel = LOONGARCH_CSR_TLBIDX
  .rept LOONGARCH_CSR_RVACFG - LOONGARCH_CSR_TLBIDX + 1
    AsmCsrWr CsrSel
    CsrSel = CsrSel + 1
  .endr

CfgCsrWrite:
  CsrSel = LOONGARCH_CSR_CPUID
  .rept LOONGARCH_CSR_PRCFG3 - LOONGARCH_CSR_CPUID + 1
    AsmCsrWr CsrSel
    CsrSel = CsrSel + 1
  .endr

KcsCsrWrite:
  CsrSel = LOONGARCH_CSR_KS0
  .rept LOONGARCH_CSR_KS8 - LOONGARCH_CSR_KS0 + 1
    AsmCsrWr CsrSel
    CsrSel = CsrSel + 1
  .endr

StableTimerCsrWrite:
  CsrSel = LOONGARCH_CSR_TMID
  .rept LOONGARCH_CSR_TINTCLR - LOONGARCH_CSR_TMID + 1
    AsmCsrWr CsrSel
    CsrSel = CsrSel + 1
  .endr

TlbRefillCsrWrite:
  CsrSel = LOONGARCH_CSR_TLBREBASE
  .rept LOONGARCH_CSR_TLBREHI - LOONGARCH_CSR_TLBREBASE + 1
    AsmCsrWr CsrSel
    CsrSel = CsrSel + 1
  .endr

DirMapCsrWrite:
  CsrSel = LOONGARCH_CSR_DMWIN0
  .rept LOONGARCH_CSR_DMWIN3 - LOONGARCH_CSR_DMWIN0 + 1
    AsmCsrWr CsrSel
    CsrSel = CsrSel + 1
  .endr


ASM_PFX(AsmCsrXChg):
  blt      $a0, $zero, XchgSelNumErr
  li.w     $t0, LOONGARCH_CSR_EBASE
  bltu     $t0, $a0, TlbCsrXchg

BasicCsrXchg:
  la.pcrel $t0, BasicCsrXchange
  alsl.d   $t0, $a0, $t0, 3
  move     $a0, $a1
  move     $a1, $a2
  jirl     $zero, $t0, 0

TlbCsrXchg:
  li.w     $t0, LOONGARCH_CSR_TLBIDX
  bltu     $a0, $t0, XchgSelNumErr
  li.w     $t0, LOONGARCH_CSR_RVACFG
  bltu     $t0, $a0, CfgCsrXchg
  la.pcrel $t0, TlbCsrXchange
  addi.w   $t1, $a0, -LOONGARCH_CSR_TLBIDX
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  move     $a1, $a2
  jirl     $zero, $t0, 0

CfgCsrXchg:
  li.w     $t0, LOONGARCH_CSR_CPUID
  bltu     $a0, $t0, XchgSelNumErr
  li.w     $t0, LOONGARCH_CSR_PRCFG3
  bltu     $t0, $a0, KcsCsrXchg
  la.pcrel $t0, CfgCsrXchange
  addi.w   $t1, $a0, -LOONGARCH_CSR_CPUID
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  move     $a1, $a2
  jirl     $zero, $t0, 0

KcsCsrXchg:
  li.w     $t0, LOONGARCH_CSR_KS0
  bltu     $a0, $t0, XchgSelNumErr
  li.w     $t0, LOONGARCH_CSR_KS8
  bltu     $t0, $a0, StableTimerCsrXchg
  la.pcrel $t0, KcsCsrXchange
  addi.w   $t1, $a0, -LOONGARCH_CSR_KS0
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  move     $a1, $a2
  jirl     $zero, $t0, 0

StableTimerCsrXchg:
  li.w     $t0, LOONGARCH_CSR_TMID
  bltu     $a0, $t0, XchgSelNumErr
  li.w     $t0, LOONGARCH_CSR_TINTCLR
  bltu     $t0, $a0, TlbRefillCsrXchg
  la.pcrel $t0, StableTimerCsrXchange
  addi.w   $t1, $a0, -LOONGARCH_CSR_TMID
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  move     $a1, $a2
  jirl     $zero, $t0, 0

TlbRefillCsrXchg:
  li.w     $t0, LOONGARCH_CSR_TLBREBASE
  bltu     $a0, $t0, XchgSelNumErr
  li.w     $t0, LOONGARCH_CSR_TLBREHI
  bltu     $t0, $a0, DirMapCsrXchg
  la.pcrel $t0, TlbRefillCsrXchange
  addi.w   $t1, $a0, -LOONGARCH_CSR_TLBREBASE
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  move     $a1, $a2
  jirl     $zero, $t0, 0

DirMapCsrXchg:
  li.w     $t0, LOONGARCH_CSR_DMWIN0
  bltu     $a0, $t0, XchgSelNumErr
  li.w     $t0, LOONGARCH_CSR_DMWIN3
  bltu     $t0, $a0, XchgSelNumErr
  la.pcrel $t0, DirMapCsrXchange
  addi.w   $t1, $a0, -LOONGARCH_CSR_DMWIN0
  alsl.d   $t0, $t1, $t0, 3
  move     $a0, $a1
  move     $a1, $a2
  jirl     $zero, $t0, 0

XchgSelNumErr:
  break    0

BasicCsrXchange:
  CsrSel = LOONGARCH_CSR_CRMD
  .rept LOONGARCH_CSR_EBASE - LOONGARCH_CSR_CRMD + 1
    AsmCsrXChange CsrSel
    CsrSel = CsrSel + 1
  .endr

TlbCsrXchange:
  CsrSel = LOONGARCH_CSR_TLBIDX
  .rept LOONGARCH_CSR_RVACFG - LOONGARCH_CSR_TLBIDX + 1
    AsmCsrXChange CsrSel
    CsrSel = CsrSel + 1
  .endr

CfgCsrXchange:
  CsrSel = LOONGARCH_CSR_CPUID
  .rept LOONGARCH_CSR_PRCFG3 - LOONGARCH_CSR_CPUID + 1
    AsmCsrXChange CsrSel
    CsrSel = CsrSel + 1
  .endr

KcsCsrXchange:
  CsrSel = LOONGARCH_CSR_KS0
  .rept LOONGARCH_CSR_KS8 - LOONGARCH_CSR_KS0 + 1
    AsmCsrXChange CsrSel
    CsrSel = CsrSel + 1
  .endr

StableTimerCsrXchange:
  CsrSel = LOONGARCH_CSR_TMID
  .rept LOONGARCH_CSR_TINTCLR - LOONGARCH_CSR_TMID + 1
    AsmCsrXChange CsrSel
    CsrSel = CsrSel + 1
  .endr

TlbRefillCsrXchange:
  CsrSel = LOONGARCH_CSR_TLBREBASE
  .rept LOONGARCH_CSR_TLBREHI - LOONGARCH_CSR_TLBREBASE + 1
    AsmCsrXChange CsrSel
    CsrSel = CsrSel + 1
  .endr

DirMapCsrXchange:
  CsrSel = LOONGARCH_CSR_DMWIN0
  .rept LOONGARCH_CSR_DMWIN3 - LOONGARCH_CSR_DMWIN0 + 1
    AsmCsrXChange CsrSel
    CsrSel = CsrSel + 1
  .endr
.end
