/// @file
///  IPF specific control register reading functions
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
// AsmReadControlRegister
//
// Reads a 64-bit control register.
//
// Reads and returns the control register specified by Index.
// If Index is invalid then 0xFFFFFFFFFFFFFFFF is returned.  This function is only available on IPF.
//
// Arguments :
//
// On Entry : The index of the control register to read.
//
// Return Value: The control register specified by Index.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadControlRegister, @function
.proc   AsmReadControlRegister
.regstk 1, 0, 0, 0

AsmReadControlRegister::
  //
  // CRs are defined in the ranges 0-25 and 64-81 (with some holes).
  // Compact this list by subtracting 32 from the top range.
  // 0-25, 64-81 -> 0-25, 32-49
  //
  mov  r15=2
  mov  r14=pr                    // save predicates
  cmp.leu  p6,p7=64,in0          // p6 = CR# >= 64
  ;;
  (p7)  cmp.leu  p7,p0=32,in0    // p7 = 32 <= CR# < 64
  (p6)  add  in0=-32,in0         // if (CR >= 64) CR# -= 32
  ;;
  (p7)  mov  r15=0               // if bad range (32-63)
  ;;
  mov  ret0=-1                   // in case of illegal CR #
  shl  r15=r15,in0               // r15 = 0x2 << CR#
  ;;
  mov  pr=r15,-1
  ;;

  //
  // At this point the predicates contain a bit field of the
  // CR desired.  (The bit is the CR+1, since pr0 is always 1.)
  //
  .pred.rel "mutex",p1,p2,p3,p9,p17,p18,p20,p21,p22,p23,p24,p25,p26,\
    p33,p34,p35,p36,p37,p38,p39,p40,p41,p42,p43,p49,p50
  (p1)  mov  ret0=cr.dcr        // cr0
  (p2)  mov  ret0=cr.itm        // cr1
  (p3)  mov  ret0=cr.iva        // cr2
  (p9)  mov  ret0=cr.pta        // cr8
  (p17)  mov  ret0=cr.ipsr      // cr16
  (p18)  mov  ret0=cr.isr       // cr17
  (p20)  mov  ret0=cr.iip       // cr19
  (p21)  mov  ret0=cr.ifa       // cr20
  (p22)  mov  ret0=cr.itir      // cr21
  (p23)  mov  ret0=cr.iipa      // cr22
  (p24)  mov  ret0=cr.ifs       // cr23
  (p25)  mov  ret0=cr.iim       // cr24
  (p26)  mov  ret0=cr.iha       // cr25

  // This is the translated (-32) range.

  (p33)  mov  ret0=cr.lid       // cr64
  (p34)  mov  ret0=cr.ivr       // cr65
  (p35)  mov  ret0=cr.tpr       // cr66
  (p36)  mov  ret0=cr.eoi       // cr67
  (p37)  mov  ret0=cr.irr0      // cr68
  (p38)  mov  ret0=cr.irr1      // cr69
  (p39)  mov  ret0=cr.irr2      // cr70
  (p40)  mov  ret0=cr.irr3      // cr71
  (p41)  mov  ret0=cr.itv       // cr72
  (p42)  mov  ret0=cr.pmv       // cr73
  (p43)  mov  ret0=cr.cmcv      // cr74
  (p49)  mov  ret0=cr.lrr0      // cr80
  (p50)  mov  ret0=cr.lrr1      // cr81
  
  //
  // Restore predicates and return.
  //
  mov  pr=r14,-1
  br.ret.sptk  b0
  .endp
