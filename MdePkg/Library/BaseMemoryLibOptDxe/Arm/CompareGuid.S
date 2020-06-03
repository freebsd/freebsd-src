//
// Copyright (c) 2016, Linaro Limited
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the Linaro nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

    .text
    .thumb
    .syntax unified
    .align  5
    .type   ASM_PFX(InternalMemCompareGuid), %function
ASM_GLOBAL ASM_PFX(InternalMemCompareGuid)
ASM_PFX(InternalMemCompareGuid):
    push    {r4, lr}
    ldr     r2, [r0]
    ldr     r3, [r0, #4]
    ldr     r4, [r0, #8]
    ldr     r0, [r0, #12]
    cbz     r1, 1f
    ldr     ip, [r1]
    ldr     lr, [r1, #4]
    cmp     r2, ip
    it      eq
    cmpeq.n r3, lr
    beq     0f
    movs    r0, #0
    pop     {r4, pc}

0:  ldr     r2, [r1, #8]
    ldr     r3, [r1, #12]
    cmp     r4, r2
    it      eq
    cmpeq.n r0, r3
    bne     2f
    movs    r0, #1
    pop     {r4, pc}

1:  orrs    r2, r2, r3
    orrs    r4, r4, r0
    movs    r0, #1
    orrs    r2, r2, r4
2:  it      ne
    movne.n r0, #0
    pop     {r4, pc}
