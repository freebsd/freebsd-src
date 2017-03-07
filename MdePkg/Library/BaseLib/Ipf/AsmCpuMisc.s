/// @file
///   Contains an implementation of CallPalProcStacked on Itanium-based
///   architecture.
///
/// Copyright (c) 2008, Intel Corporation. All rights reserved.<BR>
/// This program and the accompanying materials
/// are licensed and made available under the terms and conditions of the BSD License
/// which accompanies this distribution.  The full text of the license may be found at
/// http://opensource.org/licenses/bsd-license.php.
///
/// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
/// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
///
/// Module Name:  AsmCpuMisc.s
///
///


.text
.proc CpuBreakpoint
.type CpuBreakpoint, @function

CpuBreakpoint::
        break.i 0;;
        br.ret.dpnt    b0;;

.endp CpuBreakpoint

.proc MemoryFence
.type MemoryFence, @function

MemoryFence::
        mf;;    // memory access ordering

        // do we need the mf.a also here?
        mf.a    // wait for any IO to complete?
        
        // not sure if we need serialization here, just put it, in case...
        
        srlz.d;;
        srlz.i;;
        
        br.ret.dpnt    b0;;
.endp MemoryFence

.proc DisableInterrupts
.type DisableInterrupts, @function

DisableInterrupts::
         rsm      0x4000
         srlz.d;;
         br.ret.dpnt    b0;;

.endp DisableInterrupts

.proc EnableInterrupts
.type EnableInterrupts, @function

EnableInterrupts::
      ssm     0x4000
      srlz.d;;
      br.ret.dpnt    b0;;

.endp EnableInterrupts

.proc EnableDisableInterrupts
.type EnableDisableInterrupts, @function

EnableDisableInterrupts::
         ssm      0x4000
         srlz.d;;
         srlz.i;;
         rsm      0x4000
         srlz.d;;

         br.ret.dpnt    b0;;

.endp EnableDisableInterrupts

