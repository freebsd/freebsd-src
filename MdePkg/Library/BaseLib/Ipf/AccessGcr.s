/// @file
///  IPF specific Global Control Registers accessing functions
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
/// Module Name: AccessGcr.s
///
///

//---------------------------------------------------------------------------------
//++
// AsmReadDcr
//
// This routine is used to Read the value of Default Control Register (DCR).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of DCR.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadDcr, @function
.proc   AsmReadDcr

AsmReadDcr::
         mov            r8 = cr.dcr;;
         br.ret.dpnt    b0;;
.endp    AsmReadDcr

//---------------------------------------------------------------------------------
//++
// AsmWriteDcr
//
// This routine is used to write the value to Default Control Register (DCR).
//
// Arguments :
//
// On Entry : The value need to be written to DCR
//
// Return Value: The value written to DCR.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteDcr, @function
.proc   AsmWriteDcr
.regstk 1, 0, 0, 0

AsmWriteDcr::
         mov            cr.dcr = in0
         mov            r8 = in0;;
         srlz.i;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmWriteDcr


//---------------------------------------------------------------------------------
//++
// AsmReadItc
//
// This routine is used to Read the value of Interval Timer Counter Register (ITC).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of ITC.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadItc, @function
.proc   AsmReadItc

AsmReadItc::
         mov            r8 = ar.itc;;
         br.ret.dpnt    b0;;
.endp    AsmReadItc

//---------------------------------------------------------------------------------
//++
// AsmWriteItc
//
// This routine is used to write the value to Interval Timer Counter Register (ITC).
//
// Arguments :
//
// On Entry : The value need to be written to the ITC
//
// Return Value: The value written to the ITC.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteItc, @function
.proc   AsmWriteItc
.regstk 1, 0, 0, 0

AsmWriteItc::
         mov            ar.itc = in0
         mov            r8 = in0;;
         br.ret.dpnt    b0;;
.endp    AsmWriteItc


//---------------------------------------------------------------------------------
//++
// AsmReadItm
//
// This routine is used to Read the value of Interval Timer Match Register (ITM).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of ITM.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadItm, @function
.proc   AsmReadItm

AsmReadItm::
         mov            r8 = cr.itm;;
         br.ret.dpnt    b0;;
.endp    AsmReadItm

//---------------------------------------------------------------------------------
//++
// AsmWriteItm
//
// This routine is used to write the value to Interval Timer Match Register (ITM).
//
// Arguments :
//
// On Entry : The value need to be written to ITM
//
// Return Value: The value written to ITM.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteItm, @function
.proc   AsmWriteItm
.regstk 1, 0, 0, 0

AsmWriteItm::
         mov            cr.itm = in0
         mov            r8 = in0;;
         srlz.d;
         br.ret.dpnt    b0;;
.endp    AsmWriteItm


//---------------------------------------------------------------------------------
//++
// AsmReadIva
//
// This routine is used to read the value of Interruption Vector Address Register (IVA).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of IVA.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadIva, @function
.proc   AsmReadIva

AsmReadIva::
         mov            r8 = cr.iva;;
         br.ret.dpnt    b0;;
.endp    AsmReadIva

//---------------------------------------------------------------------------------
//++
// AsmWriteIva
//
// This routine is used to write the value to Interruption Vector Address Register (IVA).
//
// Arguments :
//
// On Entry : The value need to be written to IVA
//
// Return Value: The value written to IVA.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWriteIva, @function
.proc   AsmWriteIva
.regstk 1, 3, 0, 0

AsmWriteIva::
        alloc loc1=ar.pfs,1,4,0,0 ;;

        mov         loc2 = psr
        rsm         0x6000                      // Make sure interrupts are masked

        mov            cr.iva = in0
        srlz.i;;
        mov         psr.l = loc2;;
        srlz.i;;
        srlz.d;;
        mov ar.pfs=loc1 ;;
        mov            r8 = in0;;
        br.ret.dpnt    b0;;
.endp   AsmWriteIva


//---------------------------------------------------------------------------------
//++
// AsmReadPta
//
// This routine is used to read the value of Page Table Address Register (PTA).
//
// Arguments :
//
// On Entry :
//
// Return Value: The current value of PTA.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadPta, @function
.proc   AsmReadPta

AsmReadPta::
         mov            r8 = cr.pta;;
         br.ret.dpnt    b0;;
.endp    AsmReadPta

//---------------------------------------------------------------------------------
//++
// AsmWritePta
//
// This routine is used to write the value to Page Table Address Register (PTA)).
//
// Arguments :
//
// On Entry : The value need to be written to PTA
//
// Return Value: The value written to PTA.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmWritePta, @function
.proc   AsmWritePta
.regstk 1, 0, 0, 0

AsmWritePta::
         mov            cr.pta = in0
         mov            r8 = in0;;
         srlz.i;;
         srlz.d;;
         br.ret.dpnt    b0;;
.endp    AsmWritePta
