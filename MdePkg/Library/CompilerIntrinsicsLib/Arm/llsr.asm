//------------------------------------------------------------------------------
//
// Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
// Copyright (c) 2018, Pete Batard. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------

  EXPORT  __aeabi_llsr
  EXPORT  __rt_srsh

  AREA  s___aeabi_llsr, CODE, READONLY, ARM

  ARM

;
;VOID
;EFIAPI
;__aeabi_llsr (
;  IN  UINT64  Value,
;  IN  UINT32  Shift
;)
;
__aeabi_llsr
__rt_srsh
    SUBS     r3,r2,#0x20
    BPL      __aeabi_llsr_label1
    RSB      r3,r2,#0x20
    LSR      r0,r0,r2
    ORR      r0,r0,r1,LSL r3
    LSR      r1,r1,r2
    BX       lr
__aeabi_llsr_label1
    LSR      r0,r1,r3
    MOV      r1,#0
    BX       lr

    END
