;------------------------------------------------------------------------------
;
; CpuSleep() for ARMv7
;
; ARMv6 versions was:
;    MOV r0,#0
;    MCR p15,0,r0,c7,c0,4   ;Wait for Interrupt instruction
;
; But this is a no-op on ARMv7
;
; Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2008 - 2011, Apple Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

  EXPORT CpuSleep
  AREA cpu_sleep, CODE, READONLY

;/**
;  Places the CPU in a sleep state until an interrupt is received.
;
;  Places the CPU in a sleep state until an interrupt is received. If interrupts
;  are disabled prior to calling this function, then the CPU will be placed in a
;  sleep state indefinitely.
;
;**/
;VOID
;EFIAPI
;CpuSleep (
;  VOID
;  );
;
CpuSleep
    WFI
    BX LR

  END
