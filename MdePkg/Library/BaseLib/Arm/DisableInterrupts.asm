;------------------------------------------------------------------------------
;
; DisableInterrupts() for ARM
;
; Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

    EXPORT DisableInterrupts

    AREA Interrupt_disable, CODE, READONLY

;/**
;  Disables CPU interrupts.
;
;**/
;VOID
;EFIAPI
;DisableInterrupts (
;  VOID
;  );
;
DisableInterrupts
    MRS     R0,CPSR
    ORR     R0,R0,#0x80             ;Disable IRQ interrupts
    MSR     CPSR_c,R0
    BX      LR

    END
