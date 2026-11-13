.section .text

.globl ame_compat_memmove_asm
.type ame_compat_memmove_asm,@function
ame_compat_memmove_asm:
    movq %rdi, %rax
    testq %rdx, %rdx
    je .Lmemmove_done
    cmpq %rsi, %rdi
    jb .Lmemmove_forward
    leaq (%rsi,%rdx), %rcx
    cmpq %rcx, %rdi
    jae .Lmemmove_forward

    /* Overlap with destination above source: copy backwards. */
    leaq -1(%rdi,%rdx), %rdi
    leaq -1(%rsi,%rdx), %rsi
    std
    rep movsb
    cld
    ret

.Lmemmove_forward:
    cld
    movq %rdx, %rcx
    rep movsb
.Lmemmove_done:
    ret

.size ame_compat_memmove_asm, .-ame_compat_memmove_asm

.globl ame_compat_memset_asm
.type ame_compat_memset_asm,@function
ame_compat_memset_asm:
    cld
    movq %rdi, %r8
    movq %rdx, %rcx
    movb %sil, %al
    rep stosb
    movq %r8, %rax
    ret

.size ame_compat_memset_asm, .-ame_compat_memset_asm
