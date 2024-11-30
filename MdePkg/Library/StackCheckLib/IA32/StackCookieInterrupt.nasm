;------------------------------------------------------------------------------
; IA32/StackCookieInterrupt.nasm
;
; Copyright (c) Microsoft Corporation.
; SPDX-License-Identifier: BSD-2-Clause-Patent
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; Checks the stack cookie value against __security_cookie and calls the
; stack cookie failure handler if there is a mismatch.
;
; VOID
; TriggerStackCookieInterrupt (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(TriggerStackCookieInterrupt)
ASM_PFX(TriggerStackCookieInterrupt):
    int     FixedPcdGet8 (PcdStackCookieExceptionVector)
    ret
