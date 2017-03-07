/// @file
///  IPF specific External Interrupt Control Registers accessing functions
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
/// Module Name: AccessEicr.s
///
///

//---------------------------------------------------------------------------------
//++
// AsmReadLid
//
// This routine is used to read the value of Local Interrupt ID Register (LID).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of LID.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadLid, @function
.proc   AsmReadLid

AsmReadLid::
         mov            r8 = cr.lid;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmReadLid

//---------------------------------------------------------------------------------
//++
// AsmWriteLid
//
// This routine is used to write the value to Local Interrupt ID Register (LID).
//
// Arguments :
//
// On Entry :  The value need to be written to LID.
//
// Return Value: The value written to LID.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteLid, @function
.proc   AsmWriteLid
.regstk 1, 0, 0, 0

AsmWriteLid::
         mov            cr.lid = in0
         mov            r8 = in0;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmWriteLid


//---------------------------------------------------------------------------------
//++
// AsmReadIvr
//
// This routine is used to read the value of External Interrupt Vector Register (IVR).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of IVR.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadIvr, @function
.proc   AsmReadIvr

AsmReadIvr::
         mov            r8 = cr.ivr;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmReadIvr


//---------------------------------------------------------------------------------
//++
// AsmReadTpr
//
// This routine is used to read the value of Task Priority Register (TPR).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of TPR.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadTpr, @function
.proc   AsmReadTpr

AsmReadTpr::
         mov            r8 = cr.tpr;;
         br.ret.dpnt    b0;;
.endp    AsmReadTpr

//---------------------------------------------------------------------------------
//++
// AsmWriteTpr
//
// This routine is used to write the value to Task Priority Register (TPR).
//
// Arguments :
//
// On Entry :  The value need to be written to TPR.
//
// Return Value: The value written to TPR.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteTpr, @function
.proc   AsmWriteTpr
.regstk 1, 0, 0, 0

AsmWriteTpr::
         mov            cr.tpr = in0
         mov            r8 = in0;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmWriteTpr


//---------------------------------------------------------------------------------
//++
// AsmWriteEoi
//
// This routine is used to write the value to End of External Interrupt Register (EOI).
//
// Arguments :
//
// On Entry :  The value need to be written to EOI.
//
// Return Value: The value written to EOI.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteEoi, @function
.proc   AsmWriteEoi

AsmWriteEoi::
         mov            cr.eoi = r0;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmWriteEoi


//---------------------------------------------------------------------------------
//++
// AsmReadIrr0
//
// This routine is used to Read the value of External Interrupt Request Register 0 (IRR0).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of IRR0.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadIrr0, @function
.proc   AsmReadIrr0

AsmReadIrr0::
         mov            r8 = cr.irr0;;
         br.ret.dpnt    b0;;
.endp    AsmReadIrr0


//---------------------------------------------------------------------------------
//++
// AsmReadIrr1
//
// This routine is used to Read the value of External Interrupt Request Register 1 (IRR1).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of IRR1.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadIrr1, @function
.proc   AsmReadIrr1

AsmReadIrr1::
         mov            r8 = cr.irr1;;
         br.ret.dpnt    b0;;
.endp    AsmReadIrr1


//---------------------------------------------------------------------------------
//++
// AsmReadIrr2
//
// This routine is used to Read the value of External Interrupt Request Register 2 (IRR2).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of IRR2.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadIrr2, @function
.proc   AsmReadIrr2

AsmReadIrr2::
         mov            r8 = cr.irr2;;
         br.ret.dpnt    b0;;
.endp    AsmReadIrr2


//---------------------------------------------------------------------------------
//++
// AsmReadIrr3
//
// This routine is used to Read the value of External Interrupt Request Register 3 (IRR3).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of IRR3.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadIrr3, @function
.proc   AsmReadIrr3

AsmReadIrr3::
         mov            r8 = cr.irr3;;
         br.ret.dpnt    b0;;
.endp    AsmReadIrr3


//---------------------------------------------------------------------------------
//++
// AsmReadItv
//
// This routine is used to Read the value of Interval Timer Vector Register (ITV).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of ITV.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadItv, @function
.proc   AsmReadItv

AsmReadItv::
         mov            r8 = cr.itv;;
         br.ret.dpnt    b0;;
.endp    AsmReadItv

//---------------------------------------------------------------------------------
//++
// AsmWriteItv
//
// This routine is used to write the value to Interval Timer Vector Register (ITV).
//
// Arguments :
//
// On Entry : The value need to be written to ITV
//
// Return Value: The value written to ITV.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteItv, @function
.proc   AsmWriteItv
.regstk 1, 0, 0, 0

AsmWriteItv::
         mov            cr.itv = in0
         mov            r8 = in0;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmWriteItv


//---------------------------------------------------------------------------------
//++
// AsmReadPmv
//
// This routine is used to Read the value of Performance Monitoring Vector Register (PMV).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of PMV.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadPmv, @function
.proc   AsmReadPmv

AsmReadPmv::
         mov            r8 = cr.pmv;;
         br.ret.dpnt    b0;;
.endp    AsmReadPmv

//---------------------------------------------------------------------------------
//++
// AsmWritePmv
//
// This routine is used to write the value to Performance Monitoring Vector Register (PMV).
//
// Arguments :
//
// On Entry : The value need to be written to PMV
//
// Return Value: The value written to PMV.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWritePmv, @function
.proc   AsmWritePmv
.regstk 1, 0, 0, 0

AsmWritePmv::
         mov            cr.pmv = in0
         mov            r8 = in0;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmWritePmv


//---------------------------------------------------------------------------------
//++
// AsmReadCmcv
//
// This routine is used to Read the value of Corrected Machine Check Vector Register (CMCV).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of CMCV.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadCmcv, @function
.proc   AsmReadCmcv

AsmReadCmcv::
         mov            r8 = cr.cmcv;;
         br.ret.dpnt    b0;;
.endp    AsmReadCmcv

//---------------------------------------------------------------------------------
//++
// AsmWriteCmcv
//
// This routine is used to write the value to Corrected Machine Check Vector Register (CMCV).
//
// Arguments :
//
// On Entry : The value need to be written to CMCV
//
// Return Value: The value written to CMCV.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteCmcv, @function
.proc   AsmWriteCmcv
.regstk 1, 0, 0, 0

AsmWriteCmcv::
         mov            cr.cmcv = in0
         mov            r8 = in0;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmWriteCmcv


//---------------------------------------------------------------------------------
//++
// AsmReadLrr0
//
// This routine is used to read the value of Local Redirection Register 0 (LRR0).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of LRR0.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadLrr0, @function
.proc   AsmReadLrr0

AsmReadLrr0::
         mov            r8 = cr.lrr0;;
         br.ret.dpnt    b0;;
.endp    AsmReadLrr0

//---------------------------------------------------------------------------------
//++
// AsmWriteLrr0
//
// This routine is used to write the value to Local Redirection Register 0 (LRR0).
//
// Arguments :
//
// On Entry :  The value need to be written to LRR0.
//
// Return Value: The value written to LRR0.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteLrr0, @function
.proc   AsmWriteLrr0
.regstk 1, 0, 0, 0

AsmWriteLrr0::
         mov            cr.lrr0 = in0
         mov            r8 = in0;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmWriteLrr0


//---------------------------------------------------------------------------------
//++
// AsmReadLrr1
//
// This routine is used to read the value of Local Redirection Register 1 (LRR1).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of LRR1.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadLrr1, @function
.proc   AsmReadLrr1

AsmReadLrr1::
         mov            r8 = cr.lrr1;;
         br.ret.dpnt    b0;;
.endp    AsmReadLrr1

//---------------------------------------------------------------------------------
//++
// AsmWriteLrr1
//
// This routine is used to write the value to Local Redirection Register 1 (LRR1).
//
// Arguments :
//
// On Entry :  The value need to be written to LRR1.
//
// Return Value: The value written to LRR1.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteLrr1, @function
.proc   AsmWriteLrr1
.regstk 1, 0, 0, 0

AsmWriteLrr1::
         mov            cr.lrr1 = in0
         mov            r8 = in0;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmWriteLrr1

