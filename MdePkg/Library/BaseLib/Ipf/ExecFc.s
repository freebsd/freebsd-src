/// @file
///  IPF specific AsmFc() and AsmFci () functions
///
/// Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
/// This program and the accompanying materials
/// are licensed and made available under the terms and conditions of the BSD License
/// which accompanies this distribution.  The full text of the license may be found at
/// http://opensource.org/licenses/bsd-license.php.
///
/// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
/// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
///
/// Module Name: ExecFc.s
///
///

//---------------------------------------------------------------------------------
//++
// AsmFc
//
// This routine is used to execute a FC instruction on the specific address.
//
// Arguments :
//
// On Entry :  The specific address need to execute FC instruction.
//
// Return Value: The specific address have been execute FC instruction.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmFc, @function
.proc   AsmFc
.regstk 1, 0, 0, 0

AsmFc::
        fc              in0
        mov             r8 = in0;;
        br.ret.dpnt     b0;;
.endp   AsmFc


//---------------------------------------------------------------------------------
//++
// AsmFci
//
// This routine is used to execute a FC.i instruction on the specific address.
//
// Arguments :
//
// On Entry :  The specific address need to execute FC.i instruction.
//
// Return Value: The specific address have been execute FC.i instruction.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmFci, @function
.proc   AsmFci
.regstk 1, 0, 0, 0

AsmFci::
        fc.i            in0
        mov             r8 = in0;;
        br.ret.dpnt     b0;;
.endp   AsmFci
