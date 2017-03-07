;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.<BR>
; Copyright (c) 2015, Linaro Ltd. All rights reserved.<BR>
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
;   InterlockedCompareExchange16.Asm
;
; Abstract:
;
;   InterlockedCompareExchange16 function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT16
; EFIAPI
; InternalSyncCompareExchange16 (
;   IN      volatile UINT16           *Value,
;   IN      UINT16                    CompareValue,
;   IN      UINT16                    ExchangeValue
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalSyncCompareExchange16)
ASM_PFX(InternalSyncCompareExchange16):
    mov     ecx, [esp + 4]
    mov     ax, [esp + 8]
    mov     dx, [esp + 12]
    lock    cmpxchg [ecx], dx
    ret

