//------------------------------------------------------------------------------
//
// RISC-V synchronization functions.
//
// Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------
#include <Base.h>

.data

.text
.align 3

.global ASM_PFX(InternalSyncCompareExchange32)
.global ASM_PFX(InternalSyncCompareExchange64)
.global ASM_PFX(InternalSyncIncrement)
.global ASM_PFX(InternalSyncDecrement)

//
// ompare and xchange a 32-bit value.
//
// @param a0 : Pointer to 32-bit value.
// @param a1 : Compare value.
// @param a2 : Exchange value.
//
ASM_PFX (InternalSyncCompareExchange32):
    lr.w  a3, (a0)        // Load the value from a0 and make
                          // the reservation of address.
    bne   a3, a1, exit
    sc.w  a3, a2, (a0)    // Write the value back to the address.
    mv    a3, a1
exit:
    mv    a0, a3
    ret

//
// Compare and xchange a 64-bit value.
//
// @param a0 : Pointer to 64-bit value.
// @param a1 : Compare value.
// @param a2 : Exchange value.
//
ASM_PFX (InternalSyncCompareExchange64):
    lr.d  a3, (a0)       // Load the value from a0 and make
                         // the reservation of address.
    bne   a3, a1, exit
    sc.d  a3, a2, (a0)   // Write the value back to the address.
    mv    a3, a1
exit2:
    mv    a0, a3
    ret

//
// Performs an atomic increment of an 32-bit unsigned integer.
//
// @param a0 : Pointer to 32-bit value.
//
ASM_PFX (InternalSyncIncrement):
    li  a1, 1
    amoadd.w  a2, a1, (a0)
    mv  a0, a2
    ret

//
// Performs an atomic decrement of an 32-bit unsigned integer.
//
// @param a0 : Pointer to 32-bit value.
//
ASM_PFX (InternalSyncDecrement):
    li  a1, -1
    amoadd.w  a2, a1, (a0)
    mv  a0, a2
    ret
