/// @file
///   Contains an implementation of CallPalProcStacked on Itanium-based
///   architecture.
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
/// Module Name:  AsmPalCall.s
///
///


//-----------------------------------------------------------------------------
//++
//  AsmPalCall
//
//  Makes a PAL procedure call.
//  This is function to make a PAL procedure call.  Based on the Index
//  value this API will make static or stacked PAL call.  The following table
//  describes the usage of PAL Procedure Index Assignment. Architected procedures
//  may be designated as required or optional.  If a PAL procedure is specified
//  as optional, a unique return code of 0xFFFFFFFFFFFFFFFF is returned in the
//  Status field of the PAL_CALL_RETURN structure.
//  This indicates that the procedure is not present in this PAL implementation.
//  It is the caller's responsibility to check for this return code after calling
//  any optional PAL procedure.
//  No parameter checking is performed on the 5 input parameters, but there are
//  some common rules that the caller should follow when making a PAL call.  Any
//  address passed to PAL as buffers for return parameters must be 8-byte aligned.
//  Unaligned addresses may cause undefined results.  For those parameters defined
//  as reserved or some fields defined as reserved must be zero filled or the invalid
//  argument return value may be returned or undefined result may occur during the
//  execution of the procedure.  If the PalEntryPoint  does not point to a valid
//  PAL entry point then the system behavior is undefined.  This function is only
//  available on IPF.
//
//  On Entry :
//           in0:  PAL_PROC entrypoint
//           in1-in4 : PAL_PROC arguments
//
//  Return Value:
//
//  As per stacked calling conventions.
//
//--
//---------------------------------------------------------------------------

//
// PAL function calls
//
#define PAL_MC_CLEAR_LOG               0x0015
#define PAL_MC_DYNAMIC_STATE           0x0018
#define PAL_MC_ERROR_INFO              0x0019
#define PAL_MC_RESUME                  0x001a


.text
.proc AsmPalCall
.type AsmPalCall, @function

AsmPalCall::
         alloc          loc1 = ar.pfs,5,8,4,0
         mov            loc0 = b0
         mov            loc3 = b5
         mov            loc4 = r2
         mov            loc7 = r1
         mov            r2 = psr;;
         mov            r28 = in1
         mov            loc5 = r2;;

         movl           loc6 = 0x100;;
         cmp.ge         p6,p7 = r28,loc6;;

(p6)     movl           loc6 = 0x1FF;;
(p7)     br.dpnt.few PalCallStatic;;                  // 0 ~ 255 make a static Pal Call
(p6)     cmp.le         p6,p7 = r28,loc6;;
(p6)     br.dpnt.few PalCallStacked;;                 // 256 ~ 511 make a stacked Pal Call
(p7)     movl           loc6 = 0x300;;
(p7)     cmp.ge         p6,p7 = r28,loc6;;
(p7)     br.dpnt.few PalCallStatic;;                  // 512 ~ 767 make a static Pal Call
(p6)     movl           loc6 = 0x3FF;;
(p6)     cmp.le         p6,p7 = r28,loc6;;
(p6)     br.dpnt.few PalCallStacked;;                 // 768 ~ 1023 make a stacked Pal Call

(p7)     mov            r8 = 0xFFFFFFFFFFFFFFFF;;     // > 1024 return invalid
(p7)     br.dpnt.few    ComeBackFromPALCall;;

PalCallStatic:
         movl           loc6 = PAL_MC_CLEAR_LOG;;
         cmp.eq         p6,p7 = r28,loc6;;

(p7)     movl           loc6 = PAL_MC_DYNAMIC_STATE;;
(p7)     cmp.eq         p6,p7 = r28,loc6;;

(p7)     movl           loc6 = PAL_MC_ERROR_INFO;;
(p7)     cmp.eq         p6,p7 = r28,loc6;;

(p7)     movl           loc6 = PAL_MC_RESUME;;
(p7)     cmp.eq         p6,p7 = r28,loc6 ;;

         mov            loc6 = 0x1;;
(p7)     dep            r2 = loc6,r2,13,1;;           // psr.ic = 1

// p6 will be true, if it is one of the MCHK calls. There has been lots of debate
// on psr.ic for these values. For now, do not do any thing to psr.ic

         dep            r2 = r0,r2,14,1;;             // psr.i = 0

         mov            psr.l = r2
         srlz.d                                       // Needs data serailization.
         srlz.i                                       // Needs instruction serailization.

StaticGetPALLocalIP:
         mov            loc2 = ip;;
         add            loc2 = ComeBackFromPALCall - StaticGetPALLocalIP,loc2;;
         mov            b0 = loc2                     // return address after Pal call

         mov            r29 = in2
         mov            r30 = in3
         mov            r31 = in4
         mov            b5 = in0;;                    // get the PalProcEntrypt from input
         br.sptk        b5;;                          // Take the plunge.

PalCallStacked:
         dep            r2 = r0,r2,14,1;;             // psr.i = 0
         mov            psr.l = r2;;
         srlz.d                                       // Needs data serailization.
         srlz.i                                       // Needs instruction serailization.

StackedGetPALLocalIP:
         mov            out0 = in1
         mov            out1 = in2
         mov            out2 = in3
         mov            out3 = in4
         mov            b5 =  in0 ;;                  // get the PalProcEntrypt from input
         br.call.dpnt   b0 = b5 ;;                    // Take the plunge.

ComeBackFromPALCall:
         mov            psr.l = loc5 ;;
         srlz.d                                       // Needs data serailization.
         srlz.i                                       // Needs instruction serailization.

         mov            b5 = loc3
         mov            r2 = loc4
         mov            r1 = loc7

         mov            b0 = loc0
         mov            ar.pfs = loc1;;
         br.ret.dpnt    b0;;

.endp AsmPalCall

