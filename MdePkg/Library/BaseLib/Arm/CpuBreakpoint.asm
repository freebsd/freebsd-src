;------------------------------------------------------------------------------
;
; CpuBreakpoint() for ARM
;
; Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

    EXPORT CpuBreakpoint

; Force ARM mode for this section, as MSFT assembler defaults to THUMB
  AREA Cpu_Breakpoint, CODE, READONLY, ARM

  ARM

;/**
;  Generates a breakpoint on the CPU.
;
;  Generates a breakpoint on the CPU. The breakpoint must be implemented such
;  that code can resume normal execution after the breakpoint.
;
;**/
;VOID
;EFIAPI
;CpuBreakpoint (
;  VOID
;  );
;
CpuBreakpoint
    swi   0xdbdbdb
    bx    lr

  END
