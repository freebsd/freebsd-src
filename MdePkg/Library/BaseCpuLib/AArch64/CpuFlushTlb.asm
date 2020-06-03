;------------------------------------------------------------------------------
;
; CpuFlushTlb() for ARM
;
; Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
; Portions copyright (c) 2011 - 2013, ARM Ltd. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

  EXPORT CpuFlushTlb
  AREA BaseCpuLib_LowLevel, CODE, READONLY

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
  tlbi  vmalle1                 // Invalidate Inst TLB and Data TLB
  dsb   sy
  isb
  ret

  END
