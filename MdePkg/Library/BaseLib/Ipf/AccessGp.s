/// @file
///  IPF specific Global Pointer and Stack Pointer accessing functions
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
/// Module Name: AccessGp.s
///
///

//---------------------------------------------------------------------------------
//++
// AsmReadGp
//
// This routine is used to read the current value of 64-bit Global Pointer (GP).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current GP value.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadGp, @function
.proc   AsmReadGp

AsmReadGp::
        mov             r8 = gp;;
        br.ret.dpnt     b0;;
.endp   AsmReadGp

//---------------------------------------------------------------------------------
//++
// AsmWriteGp
//
// This routine is used to write the current value of 64-bit Global Pointer (GP).
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
.type   AsmWriteGp, @function
.proc   AsmWriteGp
.regstk 1, 0, 0, 0

AsmWriteGp::
        mov             gp = in0
        mov             r8 = in0;;
        br.ret.dpnt     b0;;
.endp   AsmWriteGp

//---------------------------------------------------------------------------------
//++
// AsmReadSp
//
// This routine is used to read the current value of 64-bit Stack Pointer (SP).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current SP value.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadSp, @function
.proc   AsmReadSp

AsmReadSp::
        mov             r8 = sp;;
        br.ret.dpnt     b0;;
.endp   AsmReadSp
