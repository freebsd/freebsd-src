/// @file
///  IPF specific Machine Specific Registers accessing functions.
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
///
///


//---------------------------------------------------------------------------------
//++
// AsmReadMsr
//
// Reads the current value of a Machine Specific Register (MSR).
//
// Reads and returns the current value of the Machine Specific Register specified by Index.  No
// parameter checking is performed on Index, and if the Index value is beyond the implemented MSR
// register range, a Reserved Register/Field fault may occur.  The caller must either guarantee that
// Index is valid, or the caller must set up fault handlers to catch the faults.  This function is
// only available on IPF.
//
// Arguments :
//
// On Entry : The 8-bit Machine Specific Register index to read.
//
// Return Value: The current value of the Machine Specific Register specified by Index.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadMsr, @function
.proc   AsmReadMsr
.regstk 1, 0, 0, 0

AsmReadMsr::
  mov    r8=msr[in0];;
  br.ret.sptk  b0;;
.endp   AsmReadMsr

//---------------------------------------------------------------------------------
//++
// AsmWriteMsr
//
// Writes the current value of a Machine Specific Register (MSR).
//
// Writes Value to the Machine Specific Register specified by Index.  Value is returned.  No
// parameter checking is performed on Index, and if the Index value is beyond the implemented MSR
// register range, a Reserved Register/Field fault may occur.  The caller must either guarantee that
// Index is valid, or the caller must set up fault handlers to catch the faults.  This function is
// only available on IPF.
//
// Arguments :
//
// On Entry : The 8-bit Machine Specific Register index to write.
//            The 64-bit value to write to the Machine Specific Register.
//
// Return Value: The 64-bit value to write to the Machine Specific Register.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteMsr, @function
.proc   AsmWriteMsr
.regstk 2, 0, 0, 0

AsmWriteMsr::
  mov             msr[in0] = in1
  mov             r8 = in1;;
  srlz.d;;
  br.ret.sptk     b0;;
.endp   AsmWriteMsr

