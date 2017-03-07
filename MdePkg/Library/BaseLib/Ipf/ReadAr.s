/// @file
///  IPF specific application register reading functions
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
// AsmReadApplicationRegister
//
// Reads a 64-bit application register.
//
// Reads and returns the application register specified by Index.
// If Index is invalid then 0xFFFFFFFFFFFFFFFF is returned.  This function is only available on IPF.
//
// Arguments :
//
// On Entry : The index of the application register to read.
//
// Return Value: The application register specified by Index.
//
//--
//----------------------------------------------------------------------------------
.text
.type   AsmReadApplicationRegister, @function
.proc   AsmReadApplicationRegister
.regstk 1, 0, 0, 0

AsmReadApplicationRegister::
  //
  // ARs are defined in the ranges 0-44 and 64-66 (with some holes).
  // Compact this list by subtracting 16 from the top range.
  // 0-44, 64-66 -> 0-44, 48-50
  //
  mov  r15=2
  mov  r14=pr                   // save predicates
  cmp.leu  p6,p7=64,in0         // p6 = AR# >= 64
  ;;
  (p7)  cmp.leu  p7,p0=48,in0   // p7 = 32 <= AR# < 64
  (p6)  add  in0=-16,in0        // if (AR >= 64) AR# -= 16
  ;;
  (p7)  mov  r15=0              // if bad range (48-63)
  ;;
  mov  ret0=-1                  // in case of illegal AR #
  shl  r15=r15,in0              // r15 = 0x2 << AR#
  ;;
  mov  pr=r15,-1
  ;;
  //
  // At this point the predicates contain a bit field of the
  // AR desired.  (The bit is the AR+1, since pr0 is always 1.)
  //
  .pred.rel "mutex",p1,p2,p3,p4,p5,p6,p7,p8,p17,p18,p19,p20,p22,p25,\
        p26,p27,p28,p29,p30,p31,p33,p37,p41,p45,p49,p50,p51
  (p1)  mov  ret0=ar.k0         // ar0
  (p2)  mov  ret0=ar.k1         // ar1
  (p3)  mov  ret0=ar.k2         // ar2
  (p4)  mov  ret0=ar.k3         // ar3
  (p5)  mov  ret0=ar.k4         // ar4
  (p6)  mov  ret0=ar.k5         // ar5
  (p7)  mov  ret0=ar.k6         // ar6
  (p8)  mov  ret0=ar.k7         // ar7

  (p17)  mov  ret0=ar.rsc       // ar16
  (p18)  mov  ret0=ar.bsp       // ar17
  (p19)  mov  ret0=ar.bspstore  // ar18
  (p20)  mov  ret0=ar.rnat      // ar19

  (p22)  mov  ret0=ar.fcr       // ar21 [iA32]

  (p25)  mov  ret0=ar.eflag     // ar24 [iA32]
  (p26)  mov  ret0=ar.csd       // ar25 [iA32]
  (p27)  mov  ret0=ar.ssd       // ar26 [iA32]
  (p28)  mov  ret0=ar.cflg      // ar27 [iA32]
  (p29)  mov  ret0=ar.fsr       // ar28 [iA32]
  (p30)  mov  ret0=ar.fir       // ar29 [iA32]
  (p31)  mov  ret0=ar.fdr       // ar30 [iA32]

  (p33)  mov  ret0=ar.ccv       // ar32

  (p37)  mov  ret0=ar.unat      // ar36

  (p41)  mov  ret0=ar.fpsr      // ar40

  (p45)  mov  ret0=ar.itc       // ar44

  //
  // This is the translated (-16) range.
  //
  (p49)  mov  ret0=ar.pfs       // ar64
  (p50)  mov  ret0=ar.lc        // ar65
  (p51)  mov  ret0=ar.ec        // ar66

  // Restore predicates and return.

  mov  pr=r14,-1
  br.ret.sptk  b0
  .endp
