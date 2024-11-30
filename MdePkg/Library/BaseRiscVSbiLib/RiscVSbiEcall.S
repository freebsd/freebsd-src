//------------------------------------------------------------------------------
//
// Make ECALL to SBI
//
// Copyright (c) 2023, Ventana Micro Systems Inc. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------

#include <Register/RiscV64/RiscVImpl.h>

.data
.align 3
.section .text

//
// Make ECALL to SBI
// ecall updates the same a0 and a1 registers with
// return values. Hence, the C function which calls
// this should pass the address of Arg0 and Arg1.
// This routine saves the address and updates it
// with a0 and a1 once ecall returns.
//
// @param a0 : Pointer to Arg0
// @param a1 : Pointer to Arg1
// @param a2 : Arg2
// @param a3 : Arg3
// @param a4 : Arg4
// @param a5 : Arg5
// @param a6 : FunctionID
// @param a7 : ExtensionId
//
ASM_FUNC (RiscVSbiEcall)
  mv t0, a0
  mv t1, a1
  ld a0, 0(a0)
  ld a1, 0(a1)
  ecall
  sd a0, 0(t0)
  sd a1, 0(t1)
  ret
