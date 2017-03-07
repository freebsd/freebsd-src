/// @file
///  IPF specific Processor Status Register accessing functions
///
/// Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
/// This program and the accompanying materials
/// are licensed and made available under the terms and conditions of the BSD License
/// which accompanies this distribution.  The full text of the license may be found at
/// http://opensource.org/licenses/bsd-license.php.
///
/// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
/// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
///
/// Module Name: AccessPsr.s
///
///

#define CpuModeMask           0x0000001008020000

#define CpuInVirtualMode             0x1
#define CpuInPhysicalMode            0x0
#define CpuInMixMode                 (0x0 - 0x1)

//---------------------------------------------------------------------------------
//++
// AsmReadPsr
//
// This routine is used to read the current value of Processor Status Register (PSR).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current PSR value.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadPsr, @function
.proc   AsmReadPsr

AsmReadPsr::
        mov             r8 = psr;;
        br.ret.dpnt     b0;;
.endp   AsmReadPsr

//---------------------------------------------------------------------------------
//++
// AsmWritePsr
//
// This routine is used to write the value of Processor Status Register (PSR).
//
// Arguments :
//
// On Entry : The value need to be written.
//
// Return Value: The value have been written.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWritePsr, @function
.proc   AsmWritePsr
.regstk 1, 0, 0, 0

AsmWritePsr::
        mov             psr.l = in0
        mov             r8 = in0;;
        srlz.d;;
        srlz.i;;
        br.ret.dpnt     b0;;
.endp   AsmWritePsr

//---------------------------------------------------------------------------------
//++
// AsmCpuVirtual
//
// This routine is used to determines if the CPU is currently executing
// in virtual, physical, or mixed mode.
//
// If the CPU is in virtual mode(PSR.RT=1, PSR.DT=1, PSR.IT=1), then 1 is returned.
// If the CPU is in physical mode(PSR.RT=0, PSR.DT=0, PSR.IT=0), then 0 is returned.
// If the CPU is not in physical mode or virtual mode, then it is in mixed mode,
// and -1 is returned.
//
// Arguments:
//
// On Entry: None
//
// Return Value: The CPU mode flag
//               return  1  The CPU is in virtual mode.
//               return  0  The CPU is in physical mode.
//               return -1  The CPU is in mixed mode.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmCpuVirtual, @function
.proc   AsmCpuVirtual

AsmCpuVirtual::
        mov            r29 = psr
        movl           r30 = CpuModeMask;;
        and            r28 = r30, r29;;
        cmp.eq         p6, p7 = r30, r28;;
(p6)    mov            r8 = CpuInVirtualMode;;
(p6)    br.ret.dpnt    b0;;
(p7)    cmp.eq         p6, p7 = 0x0, r28;;
(p6)    mov            r8 = CpuInPhysicalMode;;
(p7)    mov            r8 = CpuInMixMode;;
        br.ret.dpnt    b0;;
.endp   AsmCpuVirtual
