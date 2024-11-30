;------------------------------------------------------------------------------
; AArch64/StackCookieInterrupt.asm
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
    SVC     FixedPcdGet8 (PcdStackCookieExceptionVector)
    RET
TriggerStackCookieInterrupt ENDP

    END
