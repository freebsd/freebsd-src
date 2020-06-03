;------------------------------------------------------------------------------
;
; EnableInterrupts() for ARM
;
; Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

    EXPORT EnableInterrupts

    AREA Interrupt_enable, CODE, READONLY

;/**
;  Enables CPU interrupts.
;
;**/
;VOID
;EFIAPI
;EnableInterrupts (
;  VOID
;  );
;
EnableInterrupts
    MRS     R0,CPSR
    BIC     R0,R0,#0x80             ;Enable IRQ interrupts
    MSR     CPSR_c,R0
    BX      LR

    END
