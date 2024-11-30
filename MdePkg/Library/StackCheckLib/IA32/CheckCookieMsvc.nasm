;------------------------------------------------------------------------------
; IA32/CheckCookieMsvc.nasm
;
; Copyright (c) Microsoft Corporation.
; SPDX-License-Identifier: BSD-2-Clause-Patent
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

extern ASM_PFX(StackCheckFailure)
extern ASM_PFX(__security_cookie)
extern ASM_PFX(CpuDeadLoop)

; Called when a buffer check fails. This functionality is dependent on MSVC
; C runtime libraries and so is unsupported in UEFI.
global ASM_PFX(__report_rangecheckfailure)
ASM_PFX(__report_rangecheckfailure):
    jmp ASM_PFX(CpuDeadLoop)
    ret

; The GS handler is for checking the stack cookie during SEH or
; EH exceptions and is unsupported in UEFI.
global ASM_PFX(__GSHandlerCheck)
ASM_PFX(__GSHandlerCheck):
    jmp ASM_PFX(CpuDeadLoop)
    ret

;------------------------------------------------------------------------------
; Checks the stack cookie value against __security_cookie and calls the
; stack cookie failure handler if there is a mismatch.
;
; VOID
; EFIAPI
; __security_check_cookie (
;   IN UINTN CheckValue
;   );
;------------------------------------------------------------------------------
global @__security_check_cookie@4
@__security_check_cookie@4:
    cmp    ecx, [ASM_PFX(__security_cookie)]
    jne    ASM_PFX(StackCheckFailure)
    ret
