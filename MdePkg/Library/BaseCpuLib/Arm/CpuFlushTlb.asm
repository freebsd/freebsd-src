;------------------------------------------------------------------------------
;
; CpuFlushTlb() for ARM
;
; Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

  EXPORT CpuFlushTlb
  AREA cpu_flush_tlb, CODE, READONLY

;/**
;  Flushes all the Translation Lookaside Buffers(TLB) entries in a CPU.
;
;  Flushes all the Translation Lookaside Buffers(TLB) entries in a CPU.
;
;**/
;VOID
;EFIAPI
;CpuFlushTlb (
;  VOID
;  );
;
CpuFlushTlb
    MOV r0,#0
    MCR p15,0,r0,c8,c5,0        ;Invalidate all the unlocked entried in TLB
    BX LR

  END
