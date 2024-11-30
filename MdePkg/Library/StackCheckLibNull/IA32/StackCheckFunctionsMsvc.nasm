;------------------------------------------------------------------------------
; IA32/StackCheckFunctionsMsvc.nasm
;
; Copyright (c) Microsoft Corporation.
; SPDX-License-Identifier: BSD-2-Clause-Patent
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

global ASM_PFX(__report_rangecheckfailure)
ASM_PFX(__report_rangecheckfailure):
    ret

global ASM_PFX(__GSHandlerCheck)
ASM_PFX(__GSHandlerCheck):
    ret

global @__security_check_cookie@4
@__security_check_cookie@4:
    ret
