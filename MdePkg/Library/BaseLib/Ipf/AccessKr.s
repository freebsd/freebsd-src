/// @file
///  IPF specific AsmReadKrX() and AsmWriteKrX() functions, 'X' is from '0' to '6'
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
/// Module Name: AccessKr.s
///
///

//---------------------------------------------------------------------------------
//++
// AsmReadKr0
//
// This routine is used to get KR0.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value store in KR0.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadKr0, @function
.proc   AsmReadKr0

AsmReadKr0::
        mov             r8 = ar.k0;;
        br.ret.dpnt     b0;;
.endp   AsmReadKr0

//---------------------------------------------------------------------------------
//++
// AsmWriteKr0
//
// This routine is used to Write KR0.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value written to the KR0.
//
//--
//----------------------------------------------------------------------------------

.text
.type   AsmWriteKr0, @function
.proc   AsmWriteKr0
.regstk 1, 3, 0, 0

AsmWriteKr0::
        alloc loc1=ar.pfs,1,4,0,0 ;;
        mov             loc2 = psr;;
        rsm             0x6000;;                      // Masking interrupts
        mov             ar.k0 = in0
        srlz.i;;
        mov             psr.l = loc2;;
        srlz.i;;
        srlz.d;;
        mov             r8 = in0;;
        mov ar.pfs=loc1 ;;
        br.ret.dpnt     b0;;
.endp   AsmWriteKr0


//---------------------------------------------------------------------------------
//++
// AsmReadKr1
//
// This routine is used to get KR1.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value store in KR1.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadKr1, @function
.proc   AsmReadKr1

AsmReadKr1::
        mov             r8 = ar.k1;;
        br.ret.dpnt     b0;;
.endp   AsmReadKr1

//---------------------------------------------------------------------------------
//++
// AsmWriteKr1
//
// This routine is used to Write KR1.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value written to the KR1.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteKr1, @function
.proc   AsmWriteKr1

AsmWriteKr1::
        mov             ar.k1 = in0
        mov             r8 = in0;;
        br.ret.dpnt     b0;;
.endp   AsmWriteKr1


//---------------------------------------------------------------------------------
//++
// AsmReadKr2
//
// This routine is used to get KR2.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value store in KR2.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadKr2, @function
.proc   AsmReadKr2

AsmReadKr2::
        mov             r8 = ar.k2;;
        br.ret.dpnt     b0;;
.endp   AsmReadKr2

//---------------------------------------------------------------------------------
//++
// AsmWriteKr2
//
// This routine is used to Write KR2.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value written to the KR2.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteKr2, @function
.proc   AsmWriteKr2

AsmWriteKr2::
        mov             ar.k2 = in0
        mov             r8 = in0;;
        br.ret.dpnt     b0;;
.endp   AsmWriteKr2


//---------------------------------------------------------------------------------
//++
// AsmReadKr3
//
// This routine is used to get KR3.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value store in KR3.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadKr3, @function
.proc   AsmReadKr3

AsmReadKr3::
        mov             r8 = ar.k3;;
        br.ret.dpnt     b0;;
.endp   AsmReadKr3

//---------------------------------------------------------------------------------
//++
// AsmWriteKr3
//
// This routine is used to Write KR3.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value written to the KR3.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteKr3, @function
.proc   AsmWriteKr3

AsmWriteKr3::
        mov             ar.k3 = in0
        mov             r8 = in0;;
        br.ret.dpnt     b0;;
.endp   AsmWriteKr3


//---------------------------------------------------------------------------------
//++
// AsmReadKr4
//
// This routine is used to get KR4.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value store in KR4.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadKr4, @function
.proc   AsmReadKr4

AsmReadKr4::
        mov             r8 = ar.k4;;
        br.ret.dpnt     b0;;
.endp   AsmReadKr4

//---------------------------------------------------------------------------------
//++
// AsmWriteKr4
//
// This routine is used to Write KR4.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value written to the KR4.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteKr4, @function
.proc   AsmWriteKr4

AsmWriteKr4::
        mov             ar.k4 = in0
        mov             r8 = in0;;
        br.ret.dpnt     b0;;
.endp   AsmWriteKr4


//---------------------------------------------------------------------------------
//++
// AsmReadKr5
//
// This routine is used to get KR5.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value store in KR5.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadKr5, @function
.proc   AsmReadKr5

AsmReadKr5::
        mov             r8 = ar.k5;;
        br.ret.dpnt     b0;;
.endp   AsmReadKr5

//---------------------------------------------------------------------------------
//++
// AsmWriteKr5
//
// This routine is used to Write KR5.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value written to the KR5.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteKr5, @function
.proc   AsmWriteKr5

AsmWriteKr5::
        mov             ar.k5 = in0
        mov             r8 = in0;;
        br.ret.dpnt     b0;;
.endp   AsmWriteKr5


//---------------------------------------------------------------------------------
//++
// AsmReadKr6
//
// This routine is used to get KR6.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value store in KR6.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadKr6, @function
.proc   AsmReadKr6

AsmReadKr6::
        mov             r8 = ar.k6;;
        br.ret.dpnt     b0;;
.endp   AsmReadKr6

//---------------------------------------------------------------------------------
//++
// AsmWriteKr6
//
// This routine is used to write KR6.
//
// Arguments :
//
// On Entry :  None.
//
// Return Value: The value written to the KR6.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteKr6, @function
.proc   AsmWriteKr6

AsmWriteKr6::
        mov             ar.k6 = in0
        mov             r8 = in0;;
        br.ret.dpnt     b0;;
.endp   AsmWriteKr6
