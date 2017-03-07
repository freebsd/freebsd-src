/// @file
///  IPF specific SwitchStack() function
///
/// Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
/// This program and the accompanying materials
/// are licensed and made available under the terms and conditions of the BSD License
/// which accompanies this distribution.  The full text of the license may be found at
/// http://opensource.org/licenses/bsd-license.php.
///
/// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
/// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
///
/// Module Name: SwitchStack.s
///
///

.auto
.text

.proc   AsmSwitchStackAndBackingStore
.type   AsmSwitchStackAndBackingStore, @function
.regstk 5, 0, 0, 0
AsmSwitchStackAndBackingStore::
        mov                 r14 = ar.rsc
        movl                r2  = ~((((1 << 14) - 1) << 16) | 3)

        mov                 r17 = in1
        mov                 r18 = in2
        and                 r2  = r14, r2
        
        flushrs
        
        mov                 ar.rsc = r2
        mov                 sp  = in3
        mov                 r19 = in4

        ld8.nt1             r16 = [in0], 8
        ld8.nta             gp  = [in0]
        mov                 r3  = -1

        loadrs
        mov                 ar.bspstore = r19
        mov                 b7  = r16

        alloc               r2  = ar.pfs, 0, 0, 2, 0
        mov                 out0 = r17
        mov                 out1 = r18

        mov                 ar.rnat = r3
        mov                 ar.rsc = r14
        br.call.sptk.many   b0  = b7
.endp   AsmSwitchStackAndBackingStore
