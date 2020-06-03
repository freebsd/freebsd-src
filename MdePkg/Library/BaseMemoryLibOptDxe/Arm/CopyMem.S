#------------------------------------------------------------------------------
#
# CopyMem() worker for ARM
#
# This file started out as C code that did 64 bit moves if the buffer was
# 32-bit aligned, else it does a byte copy. It also does a byte copy for
# any trailing bytes. It was updated to do 32-byte copies using stm/ldm.
#
# Copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
# Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

    .text
    .thumb
    .syntax unified

/**
  Copy Length bytes from Source to Destination. Overlap is OK.

  This implementation

  @param  Destination Target of copy
  @param  Source      Place to copy from
  @param  Length      Number of bytes to copy

  @return Destination


VOID *
EFIAPI
InternalMemCopyMem (
  OUT     VOID                      *DestinationBuffer,
  IN      CONST VOID                *SourceBuffer,
  IN      UINTN                     Length
  )
**/
    .type   ASM_PFX(InternalMemCopyMem), %function
ASM_GLOBAL ASM_PFX(InternalMemCopyMem)
ASM_PFX(InternalMemCopyMem):
    push    {r4-r11, lr}
    // Save the input parameters in extra registers (r11 = destination, r14 = source, r12 = length)
    mov     r11, r0
    mov     r10, r0
    mov     r12, r2
    mov     r14, r1

    cmp     r11, r1
    // If (dest < source)
    bcc     memcopy_check_optim_default

    // If (source + length < dest)
    rsb     r3, r1, r11
    cmp     r12, r3
    bcc     memcopy_check_optim_default
    b       memcopy_check_optim_overlap

memcopy_check_optim_default:
    // Check if we can use an optimized path ((length >= 32) && destination word-aligned && source word-aligned) for the memcopy (optimized path if r0 == 1)
    tst     r0, #0xF
    it      ne
    movne.n r0, #0
    bne     memcopy_default
    tst     r1, #0xF
    it      ne
    movne.n r3, #0
    it      eq
    moveq.n r3, #1
    cmp     r2, #31
    it      ls
    movls.n r0, #0
    bls     memcopy_default
    and     r0, r3, #1
    b       memcopy_default

memcopy_check_optim_overlap:
    // r10 = dest_end, r14 = source_end
    add     r10, r11, r12
    add     r14, r12, r1

    // Are we in the optimized case ((length >= 32) && dest_end word-aligned && source_end word-aligned)
    cmp     r2, #31
    it      ls
    movls.n r0, #0
    it      hi
    movhi.n r0, #1
    tst     r10, #0xF
    it      ne
    movne.n r0, #0
    tst     r14, #0xF
    it      ne
    movne.n r0, #0
    b       memcopy_overlapped

memcopy_overlapped_non_optim:
    // We read 1 byte from the end of the source buffer
    sub     r3, r14, #1
    sub     r12, r12, #1
    ldrb    r3, [r3, #0]
    sub     r2, r10, #1
    cmp     r12, #0
    // We write 1 byte at the end of the dest buffer
    sub     r10, r10, #1
    sub     r14, r14, #1
    strb    r3, [r2, #0]
    bne     memcopy_overlapped_non_optim
    b       memcopy_end

// r10 = dest_end, r14 = source_end
memcopy_overlapped:
    // Are we in the optimized case ?
    cmp     r0, #0
    beq     memcopy_overlapped_non_optim

    // Optimized Overlapped - Read 32 bytes
    sub     r14, r14, #32
    sub     r12, r12, #32
    cmp     r12, #31
    ldmia   r14, {r2-r9}

    // If length is less than 32 then disable optim
    it      ls
    movls.n r0, #0

    cmp     r12, #0

    // Optimized Overlapped - Write 32 bytes
    sub     r10, r10, #32
    stmia   r10, {r2-r9}

    // while (length != 0)
    bne     memcopy_overlapped
    b       memcopy_end

memcopy_default_non_optim:
    // Byte copy
    ldrb    r3, [r14], #1
    sub     r12, r12, #1
    strb    r3, [r10], #1

memcopy_default:
    cmp     r12, #0
    beq     memcopy_end

// r10 = dest, r14 = source
memcopy_default_loop:
    cmp     r0, #0
    beq     memcopy_default_non_optim

    // Optimized memcopy - Read 32 Bytes
    sub     r12, r12, #32
    cmp     r12, #31
    ldmia   r14!, {r2-r9}

    // If length is less than 32 then disable optim
    it      ls
    movls.n r0, #0

    cmp     r12, #0

    // Optimized memcopy - Write 32 Bytes
    stmia   r10!, {r2-r9}

    // while (length != 0)
    bne     memcopy_default_loop

memcopy_end:
    mov     r0, r11
    pop     {r4-r11, pc}
