/// @file
///  IPF specific AsmReadCpuid()function
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
/// Module Name: ReadCpuid.s
///
///

//---------------------------------------------------------------------------------
//++
// AsmReadCpuid
//
// This routine is used to Reads the current value of Processor Identifier Register (CPUID).
//
// Arguments :
//
// On Entry : The 8-bit Processor Identifier Register index to read.
//
// Return Value: The current value of Processor Identifier Register specified by Index.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadCpuid, @function
.proc   AsmReadCpuid
.regstk 1, 0, 0, 0

AsmReadCpuid::
        mov             r8 = cpuid[in0];;
        br.ret.dpnt     b0;;
.endp    AsmReadCpuid

