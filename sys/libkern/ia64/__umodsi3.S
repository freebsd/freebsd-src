.file "__umodsi3.s"

// $FreeBSD$

//-
// Copyright (c) 2000, Intel Corporation
// All rights reserved.
//
// Contributed 2/15/2000 by Marius Cornea, John Harrison, Cristina Iordache, 
// Ted Kubaska, Bob Norin, and Shane Story of the Computational Software Lab, 
// Intel Corporation.
//
// WARRANTY DISCLAIMER
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR ITS 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Intel Corporation is the author of this code, and requests that all
// problem reports or change requests be submitted to it directly at
// http://developer.intel.com/opensource.
//

.section .text

// 32-bit unsigned integer remainder

.proc __umodsi3#
.align 32
.global __umodsi3#
.align 32

__umodsi3:

{ .mii
  alloc r31=ar.pfs,2,0,0,0
  nop.i 0
  nop.i 0;;
} { .mii
  nop.m 0

  // 32-BIT UNSIGNED INTEGER REMAINDER BEGINS HERE

  // general register used:
  //    r32 - 32-bit unsigned integer dividend
  //    r33 - 32-bit unsigned integer divisor
  //    r8 - 32-bit unsigned integer result
  //    r2 - scratch register
  // floating-point registers used: f6, f7, f8, f9, f10, f11
  // predicate registers used: p6

  zxt4 r32=r32
  zxt4 r33=r33;;
} { .mmb
  setf.sig f11=r32
  setf.sig f7=r33
  nop.b 0;;
} { .mfi
  nop.m 0
  fcvt.xf f6=f11
  nop.i 0
} { .mfi
  // get 2's complement of b
  sub r33=r0,r33
  fcvt.xf f7=f7
  mov r2 = 0x0ffdd;;
} { .mfi
  setf.exp f9 = r2
  // (1) y0
  frcpa.s1 f8,p6=f6,f7
  nop.i 0;;
}  { .mfi
  nop.m 0
  // (2) q0 = a * y0
  (p6) fma.s1 f10=f6,f8,f0
  nop.i 0
} { .mfi
  nop.m 0
  // (3) e0 = 1 - b * y0
  (p6) fnma.s1 f8=f7,f8,f1
  nop.i 0;;
} { .mfi
  nop.m 0
  // (4) q1 = q0 + e0 * q0
  (p6) fma.s1 f10=f8,f10,f10
  nop.i 0
} { .mfi
  // get 2's complement of b
  setf.sig f7=r33
  // (5) e1 = e0 * e0 + 2^-34
  (p6) fma.s1 f8=f8,f8,f9
  nop.i 0;;
} { .mfi
  nop.m 0
  // (6) q2 = q1 + e1 * q1
  (p6) fma.s1 f8=f8,f10,f10
  nop.i 0;;
} { .mfi
  nop.m 0
  // (7) q = trunc(q2)
  fcvt.fxu.trunc.s1 f8=f8
  nop.i 0;;
}  { .mfi
  nop.m 0
  // (8) r = a + (-b) * q
  xma.l f8=f8,f7,f11
  nop.i 0;;
}  { .mmi
  // remainder will be in the least significant 32 bits of r8 (if b != 0)
  getf.sig r8=f8
  nop.m 0
  nop.i 0;;
}

  // 32-BIT UNSIGNED INTEGER REMAINDER ENDS HERE

{ .mmb
  nop.m 0
  nop.m 0
  br.ret.sptk b0;;
}

.endp __umodsi3
