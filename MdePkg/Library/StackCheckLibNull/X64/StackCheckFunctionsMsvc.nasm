;------------------------------------------------------------------------------
; X64/StackCheckFunctionsMsvc.nasm
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

global ASM_PFX(__security_check_cookie)
ASM_PFX(__security_check_cookie):
    ret
