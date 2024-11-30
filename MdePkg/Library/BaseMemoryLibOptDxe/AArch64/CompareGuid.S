//
// Copyright (c) 2016, Linaro Limited
// All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause-Patent
//

    .text
    .align  5
ASM_GLOBAL ASM_PFX(InternalMemCompareGuid)
ASM_PFX(InternalMemCompareGuid):
    AARCH64_BTI(c)
    mov     x2, xzr
    ldp     x3, x4, [x0]
    cbz     x1, 0f
    ldp     x1, x2, [x1]
0:  cmp     x1, x3
    ccmp    x2, x4, #0, eq
    cset    w0, eq
    ret
