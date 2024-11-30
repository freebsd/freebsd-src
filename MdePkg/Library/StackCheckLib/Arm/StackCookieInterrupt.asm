;------------------------------------------------------------------------------
; Arm/StackCookieInterrupt.asm
;
; Copyright (c) Microsoft Corporation.
; SPDX-License-Identifier: BSD-2-Clause-Patent
;------------------------------------------------------------------------------

    EXPORT TriggerStackCookieInterrupt

    AREA |.text|, CODE, READONLY

;------------------------------------------------------------------------------
; Calls an interrupt using the vector specified by PcdStackCookieExceptionVector
;
; VOID
; TriggerStackCookieInterrupt (
;   VOID
;   );
;------------------------------------------------------------------------------
TriggerStackCookieInterrupt PROC
    SWI     FixedPcdGet8 (PcdStackCookieExceptionVector)
    BX      LR
TriggerStackCookieInterrupt ENDP

    END
