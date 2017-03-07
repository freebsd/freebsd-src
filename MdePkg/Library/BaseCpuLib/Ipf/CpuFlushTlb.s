/// @file
///   CpuFlushTlb() function for Itanium-based architecture.
///
/// Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
/// This program and the accompanying materials
/// are licensed and made available under the terms and conditions of the BSD License
/// which accompanies this distribution.  The full text of the license may be found at
/// http://opensource.org/licenses/bsd-license.php.
///
/// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
/// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
///
/// Module Name:  CpuFlushTlb.s
///
///

.auto
.text

ASM_GLOBAL PalCall
.type   PalCall, @function

.proc   CpuFlushTlb
.type   CpuFlushTlb, @function
CpuFlushTlb::
        alloc               loc0 = ar.pfs, 0, 3, 5, 0
        mov                 out0 = 0
        mov                 out1 = 6
        mov                 out2 = 0
        mov                 out3 = 0
        mov                 loc1 = b0
        mov                 out4 = 0
        brl.call.sptk       b0  = PalCall
        mov                 loc2 = psr              // save PSR
        mov                 ar.pfs = loc0
        extr.u              r14 = r10, 32, 32       // r14 <- count1
        rsm                 1 << 14                 // Disable interrupts
        extr.u              r15 = r11, 32, 32       // r15 <- stride1
        extr.u              r10 = r10, 0, 32        // r10 <- count2
        add                 r10 = -1, r10
        extr.u              r11 = r11, 0, 32        // r11 <- stride2
        br.cond.sptk        LoopPredicate
LoopOuter:
        mov                 ar.lc = r10             // LC <- count2
        mov                 ar.ec = r0              // EC <- 0
Loop:
        ptc.e               r9
        add                 r9 = r11, r9            // r9 += stride2
        br.ctop.sptk        Loop
        add                 r9 = r15, r9            // r9 += stride1
LoopPredicate:
        cmp.ne              p6 = r0, r14            // count1 == 0?
        add                 r14 = -1, r14
(p6)    br.cond.sptk        LoopOuter
        mov                 psr.l = loc2
        mov                 b0  = loc1
        br.ret.sptk.many    b0
.endp
