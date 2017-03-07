/// @file
///  IPF specific Debug Breakpoint Registers accessing functions
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
/// Module Name: AccessDbr.s
///
///

//---------------------------------------------------------------------------------
//++
// AsmReadDbr
//
// This routine is used to Reads the current value of Data Breakpoint Register (DBR).
//
// Arguments :
//
// On Entry : The 8-bit DBR index to read.
//
// Return Value: The current value of DBR by Index.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadDbr, @function
.proc   AsmReadDbr
.regstk 1, 0, 0, 0

AsmReadDbr::
        mov             r8 = dbr[in0];;
        br.ret.dpnt     b0;;
.endp   AsmReadDbr

//---------------------------------------------------------------------------------
//++
// AsmWriteDbr
//
// This routine is used to write the current value to Data Breakpoint Register (DBR).
//
// Arguments :
//
// On Entry : The 8-bit DBR index to read.
//            The value should be written to DBR
//
// Return Value: The value written to DBR.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteDbr, @function
.proc   AsmWriteDbr
.regstk 2, 0, 0, 0

AsmWriteDbr::
        mov             dbr[in0] = in1
        mov             r8 = in1;;
        srlz.d;;
        br.ret.dpnt     b0;;
.endp   AsmWriteDbr


//---------------------------------------------------------------------------------
//++
// AsmReadIbr
//
// This routine is used to Reads the current value of Instruction Breakpoint Register (IBR).
//
// Arguments :
//
// On Entry : The 8-bit IBR index.
//
// Return Value: The current value of IBR by Index.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadIbr, @function
.proc   AsmReadIbr
.regstk 1, 0, 0, 0

AsmReadIbr::
        mov             r8 = ibr[in0];;
        br.ret.dpnt     b0;;
.endp   AsmReadIbr

//---------------------------------------------------------------------------------
//++
// AsmWriteIbr
//
// This routine is used to write the current value to Instruction Breakpoint Register (IBR).
//
// Arguments :
//
// On Entry : The 8-bit IBR index.
//            The value should be written to IBR
//
// Return Value: The value written to IBR.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteIbr, @function
.proc   AsmWriteIbr
.regstk 2, 0, 0, 0

AsmWriteIbr::
        mov             ibr[in0] = in1
        mov             r8 = in1;;
        srlz.i;;
        br.ret.dpnt     b0;;
.endp   AsmWriteIbr
