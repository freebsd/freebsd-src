;------------------------------------------------------------------------------
;
; DisableInterrupts() for AArch64
;
; Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
; Portions copyright (c) 2011 - 2013, ARM Ltd. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

  EXPORT DisableInterrupts
  AREA BaseLib_LowLevel, CODE, READONLY

DAIF_WR_IRQ_BIT     EQU     (1 << 1)

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
    msr  daifset, #DAIF_WR_IRQ_BIT
    ret

  END
