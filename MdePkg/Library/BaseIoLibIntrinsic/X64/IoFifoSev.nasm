;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
; Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>
;
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; Check whether we need to unroll the String I/O in SEV guest
;
; Return // eax   (1 - unroll, 0 - no unroll)
;------------------------------------------------------------------------------
global ASM_PFX(SevNoRepIo)
ASM_PFX(SevNoRepIo):

  ; CPUID clobbers ebx, ecx and edx
  push      rbx
  push      rcx
  push      rdx

  ; Check if we are runing under hypervisor
  ; CPUID(1).ECX Bit 31
  mov       eax, 1
  cpuid
  bt        ecx, 31
  jnc       @UseRepIo

  ; Check if we have Memory encryption CPUID leaf
  mov       eax, 0x80000000
  cpuid
  cmp       eax, 0x8000001f
  jl        @UseRepIo

  ; Check for memory encryption feature:
  ;  CPUID  Fn8000_001F[EAX] - Bit 1
  ;
  mov       eax,  0x8000001f
  cpuid
  bt        eax, 1
  jnc       @UseRepIo

  ; Check if memory encryption is enabled
  ;  MSR_0xC0010131 - Bit 0 (SEV enabled)
  ;  MSR_0xC0010131 - Bit 1 (SEV-ES enabled)
  mov       ecx, 0xc0010131
  rdmsr

  ; Check for (SevEsEnabled == 0 && SevEnabled == 1)
  and       eax, 3
  cmp       eax, 1
  je        @SevNoRepIo_Done

@UseRepIo:
  xor       eax, eax

@SevNoRepIo_Done:
  pop       rdx
  pop       rcx
  pop       rbx
  ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  SevIoReadFifo8 (
;    IN  UINTN                 Port,              // rcx
;    IN  UINTN                 Size,              // rdx
;    OUT VOID                  *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(SevIoReadFifo8)
ASM_PFX(SevIoReadFifo8):
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi

    ; Check if we need to unroll String I/O
    call    ASM_PFX(SevNoRepIo)
    test    eax, eax
    jnz     @IoReadFifo8_NoRep

    cld
    rep     insb
    jmp     @IoReadFifo8_Done

@IoReadFifo8_NoRep:
    jrcxz   @IoReadFifo8_Done

@IoReadFifo8_Loop:
    in      al, dx
    mov     byte [rdi], al
    inc     rdi
    loop    @IoReadFifo8_Loop

@IoReadFifo8_Done:
    mov     rdi, r8             ; restore rdi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  SevIoReadFifo16 (
;    IN  UINTN                 Port,              // rcx
;    IN  UINTN                 Size,              // rdx
;    OUT VOID                  *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(SevIoReadFifo16)
ASM_PFX(SevIoReadFifo16):
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi

    ; Check if we need to unroll String I/O
    call    ASM_PFX(SevNoRepIo)
    test    eax, eax
    jnz     @IoReadFifo16_NoRep

    cld
    rep     insw
    jmp     @IoReadFifo16_Done

@IoReadFifo16_NoRep:
    jrcxz   @IoReadFifo16_Done

@IoReadFifo16_Loop:
    in      ax, dx
    mov     word [rdi], ax
    add     rdi, 2
    loop    @IoReadFifo16_Loop

@IoReadFifo16_Done:
    mov     rdi, r8             ; restore rdi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  SevIoReadFifo32 (
;    IN  UINTN                 Port,              // rcx
;    IN  UINTN                 Size,              // rdx
;    OUT VOID                  *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(SevIoReadFifo32)
ASM_PFX(SevIoReadFifo32):
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi

    ; Check if we need to unroll String I/O
    call    ASM_PFX(SevNoRepIo)
    test    eax, eax
    jnz     @IoReadFifo32_NoRep

    cld
    rep     insd
    jmp     @IoReadFifo32_Done

@IoReadFifo32_NoRep:
    jrcxz   @IoReadFifo32_Done

@IoReadFifo32_Loop:
    in      eax, dx
    mov     dword [rdi], eax
    add     rdi, 4
    loop    @IoReadFifo32_Loop

@IoReadFifo32_Done:
    mov     rdi, r8             ; restore rdi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo8 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(SevIoWriteFifo8)
ASM_PFX(SevIoWriteFifo8):
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi

    ; Check if we need to unroll String I/O
    call    ASM_PFX(SevNoRepIo)
    test    eax, eax
    jnz     @IoWriteFifo8_NoRep

    cld
    rep     outsb
    jmp     @IoWriteFifo8_Done

@IoWriteFifo8_NoRep:
    jrcxz   @IoWriteFifo8_Done

@IoWriteFifo8_Loop:
    mov     al, byte [rsi]
    out     dx, al
    inc     rsi
    loop    @IoWriteFifo8_Loop

@IoWriteFifo8_Done:
    mov     rsi, r8             ; restore rsi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  SevIoWriteFifo16 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(SevIoWriteFifo16)
ASM_PFX(SevIoWriteFifo16):
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi

    ; Check if we need to unroll String I/O
    call    ASM_PFX(SevNoRepIo)
    test    eax, eax
    jnz     @IoWriteFifo16_NoRep

    cld
    rep     outsw
    jmp     @IoWriteFifo16_Done

@IoWriteFifo16_NoRep:
    jrcxz   @IoWriteFifo16_Done

@IoWriteFifo16_Loop:
    mov     ax, word [rsi]
    out     dx, ax
    add     rsi, 2
    loop    @IoWriteFifo16_Loop

@IoWriteFifo16_Done:
    mov     rsi, r8             ; restore rsi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  SevIoWriteFifo32 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(SevIoWriteFifo32)
ASM_PFX(SevIoWriteFifo32):
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi

    ; Check if we need to unroll String I/O
    call    ASM_PFX(SevNoRepIo)
    test    eax, eax
    jnz     @IoWriteFifo32_NoRep

    cld
    rep     outsd
    jmp     @IoWriteFifo32_Done

@IoWriteFifo32_NoRep:
    jrcxz   @IoWriteFifo32_Done

@IoWriteFifo32_Loop:
    mov     eax, dword [rsi]
    out     dx, eax
    add     rsi, 4
    loop    @IoWriteFifo32_Loop

@IoWriteFifo32_Done:
    mov     rsi, r8             ; restore rsi
    ret

