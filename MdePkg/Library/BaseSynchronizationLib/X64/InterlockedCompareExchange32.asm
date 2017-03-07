;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.<BR>
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
;   InterlockedCompareExchange32.Asm
;
; Abstract:
;
;   InterlockedCompareExchange32 function
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
; UINT32
; EFIAPI
; InternalSyncCompareExchange32 (
;   IN      volatile UINT32           *Value,
;   IN      UINT32                    CompareValue,
;   IN      UINT32                    ExchangeValue
;   );
;------------------------------------------------------------------------------
InternalSyncCompareExchange32   PROC
    mov     eax, edx
    lock    cmpxchg [rcx], r8d
    ret
InternalSyncCompareExchange32   ENDP

    END
