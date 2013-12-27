.file "__moddi3.s"

//  
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

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

.section .text

// 64-bit signed integer remainder

.proc __moddi3#
.align 32
.global __moddi3#
.align 32

__moddi3:

{ .mii
  alloc r31=ar.pfs,3,0,0,0
  nop.i 0
  nop.i 0
} { .mmb

  // 64-BIT SIGNED INTEGER REMAINDER BEGINS HERE

  // general register used:
  //    r32 - 64-bit signed integer dividend
  //    r33 - 64-bit signed integer divisor
  //    r8 - 64-bit signed integer result
  //    r2 - scratch register
  // floating-point registers used: f6, f7, f8, f9, f10, f11, f12
  // predicate registers used: p6

  setf.sig f12=r32  // holds an in integer form
  setf.sig f7=r33
  nop.b 0
} { .mlx
  nop.m 0
  //movl r2=0x8000000000000000;;
  movl r2=0xffffffffffffffff;;
} { .mfi
  // get the 2's complement of b
  sub r33=r0,r33
  fcvt.xf f6=f12
  nop.i 0
} { .mfi
  nop.m 0
  fcvt.xf f7=f7
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (1)
  // y0 = 1 / b in f8
  frcpa.s1 f8,p6=f6,f7
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (2)
  // q0 = a * y0 in f10
  (p6) fma.s1 f10=f6,f8,f0
  nop.i 0
} { .mfi
  nop.m 0
  // Step (3)
  // e0 = 1 - b * y0 in f9
  (p6) fnma.s1 f9=f7,f8,f1
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (4)
  // q1 = q0 + e0 * q0 in f10
  (p6) fma.s1 f10=f9,f10,f10
  nop.i 0
} { .mfi
  nop.m 0
  // Step (5)
  // e1 = e0 * e0 in f11
  (p6) fma.s1 f11=f9,f9,f0
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (6)
  // y1 = y0 + e0 * y0 in f8
  (p6) fma.s1 f8=f9,f8,f8
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (7)
  // q2 = q1 + e1 * q1 in f9
  (p6) fma.s1 f9=f11,f10,f10
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (8)
  // y2 = y1 + e1 * y1 in f8
  (p6) fma.s1 f8=f11,f8,f8
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (9)
  // r2 = a - b * q2 in f10
  (p6) fnma.s1 f10=f7,f9,f6
  nop.i 0;;
} { .mfi
  setf.sig f7=r33
  // Step (10)
  // q3 = q2 + r2 * y2 in f8
  (p6) fma.s1 f8=f10,f8,f9
  nop.i 0;;
} { .mfi
  nop.m 0
  // (11) q = trunc(q3)
  fcvt.fx.trunc.s1 f8=f8
  nop.i 0;;
} { .mfi
  nop.m 0
  // (12) r = a + (-b) * q
  xma.l f8=f8,f7,f12
  nop.i 0;;
}  { .mib
  getf.sig r8=f8
  nop.i 0
  nop.b 0
} 

  // 64-BIT SIGNED INTEGER REMAINDER ENDS HERE

{ .mib
  nop.m 0
  nop.i 0
  br.ret.sptk b0;;
}

.endp __moddi3
