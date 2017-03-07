/// @file
///  IPF specific AsmReadKr7() and AsmWriteKr7()
///
/// Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
/// This program and the accompanying materials
/// are licensed and made available under the terms and conditions of the BSD License
/// which accompanies this distribution.  The full text of the license may be found at
/// http://opensource.org/licenses/bsd-license.php.
///
/// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
/// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
///
/// Module Name: AccessKr7.s
///
///

//---------------------------------------------------------------------------------
//++
// AsmReadKr7
//
// This routine is used to get KR7.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value store in KR7.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadKr7, @function
.proc   AsmReadKr7

AsmReadKr7::
        mov             r8 = ar.k7;;
        br.ret.dpnt     b0;;
.endp   AsmReadKr7

//---------------------------------------------------------------------------------
//++
// AsmWriteKr7
//
// This routine is used to write KR7.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value written to the KR7.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteKr7, @function
.proc   AsmWriteKr7
.regstk 1, 3, 0, 0

AsmWriteKr7::
        mov             ar.k7 = in0
        mov             r8 = in0;;
        br.ret.dpnt     b0;;
.endp   AsmWriteKr7
