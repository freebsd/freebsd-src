/// @file
///  IPF specific Performance Monitor Configuration/Data Registers accessing functions
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
/// Module Name: AccessPmr.s
///
///

//---------------------------------------------------------------------------------
//++
// AsmReadPmc
//
// This routine is used to Reads the current value of Performance Monitor Configuration Register (PMC).
//
// Arguments :
//
// On Entry : The 8-bit PMC index.
//
// Return Value: The current value of PMC by Index.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadPmc, @function
.proc   AsmReadPmc
.regstk 1, 0, 0, 0

AsmReadPmc::
        srlz.i;;
        srlz.d;;
        mov             r8 = pmc[in0];;
        br.ret.dpnt     b0;;
.endp   AsmReadPmc

//---------------------------------------------------------------------------------
//++
// AsmWritePmc
//
// This routine is used to write the current value to a Performance Monitor Configuration Register (PMC).
//
// Arguments :
//
// On Entry : The 8-bit PMC index.
//            The value should be written to PMC
//
// Return Value: The value written to PMC.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWritePmc, @function
.proc   AsmWritePmc
.regstk 2, 0, 0, 0

AsmWritePmc::
        mov             pmc[in0] = in1
        mov             r8 = in1;;
        srlz.i;;
        srlz.d;;
        br.ret.dpnt     b0;;
.endp   AsmWritePmc


//---------------------------------------------------------------------------------
//++
// AsmReadPmd
//
// This routine is used to Reads the current value of Performance Monitor Data Register (PMD).
//
// Arguments :
//
// On Entry : The 8-bit PMD index.
//
// Return Value: The current value of PMD by Index.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadPmd, @function
.proc   AsmReadPmd
.regstk 1, 0, 0, 0

AsmReadPmd::
        srlz.i;;
        srlz.d;;
        mov             r8 = pmd[in0];;
        br.ret.dpnt     b0;;
.endp   AsmReadPmd

//---------------------------------------------------------------------------------
//++
// AsmWritePmd
//
// This routine is used to write the current value to Performance Monitor Data Register (PMD).
//
// Arguments :
//
// On Entry : The 8-bit PMD index.
//            The value should be written to PMD
//
// Return Value: The value written to PMD.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWritePmd, @function
.proc   AsmWritePmd
.regstk 2, 0, 0, 0

AsmWritePmd::
        mov             pmd[in0] = in1
        mov             r8 = in1;;
        srlz.i;;
        srlz.d;;
        br.ret.dpnt     b0;;
.endp   AsmWritePmd
