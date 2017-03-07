;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
; Module Name:
;
;   FlushCacheLine.Asm
;
; Abstract:
;
;   AsmFlushCacheLine function
;
; Notes:
;
;------------------------------------------------------------------------------

    .586P
    .model  flat,C
    .xmm
    .code

;------------------------------------------------------------------------------
; VOID *
; EFIAPI
; AsmFlushCacheLine (
;   IN      VOID                      *LinearAddress
;   );
;------------------------------------------------------------------------------
AsmFlushCacheLine   PROC
    ;
    ; If the CPU does not support CLFLUSH instruction, 
    ; then promote flush range to flush entire cache.
    ;
    mov     eax, 1
    push    ebx
    cpuid
    pop     ebx
    mov     eax, [esp + 4]
    test    edx, BIT19
    jz      @F
    clflush [eax]
    ret
@@:
    wbinvd
    ret
AsmFlushCacheLine   ENDP

    END
