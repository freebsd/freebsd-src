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

ENTRY(__divdf3, 0)
{ .mfi
  // a is in f8
  // b is in f9

  // predicate registers used: p6
  // floating-point registers used: f6, f7, f8, f9, f10, f11

  // load a, the first argument, in f6
  nop.m 0
  mov f6=f8
  nop.i 0
} { .mfi
  // load b, the second argument, in f7
  nop.m 0
  mov f7=f9
  nop.i 0;;
} { .mfi

  // BEGIN DOUBLE PRECISION LATENCY-OPTIMIZED DIVIDE ALGORITHM

  nop.m 0
  // Step (1)
  // y0 = 1 / b in f8
  frcpa.s0 f8,p6=f6,f7
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (2)
  // q0 = a * y0 in f9
  (p6) fma.s1 f9=f6,f8,f0
  nop.i 0
} { .mfi
  nop.m 0
  // Step (3)
  // e0 = 1 - b * y0 in f10
  (p6) fnma.s1 f10=f7,f8,f1
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (4)
  // q1 = q0 + e0 * q0 in f9
  (p6) fma.s1 f9=f10,f9,f9
  nop.i 0
} { .mfi
  nop.m 0
  // Step (5)
  // e1 = e0 * e0 in f11
  (p6) fma.s1 f11=f10,f10,f0
  nop.i 0
} { .mfi
  nop.m 0
  // Step (6)
  // y1 = y0 + e0 * y0 in f8
  (p6) fma.s1 f8=f10,f8,f8
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (7)
  // q2 = q1 + e1 * q1 in f9
  (p6) fma.s1 f9=f11,f9,f9
  nop.i 0
} { .mfi
  nop.m 0
  // Step (8)
  // e2 = e1 * e1 in f10
  (p6) fma.s1 f10=f11,f11,f0
  nop.i 0
} { .mfi
  nop.m 0
  // Step (9)
  // y2 = y1 + e1 * y1 in f8
  (p6) fma.s1 f8=f11,f8,f8
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (10)
  // q3 = q2 + e2 * q2 in f9
  (p6) fma.d.s1 f9=f10,f9,f9
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (11)
  // y3 = y2 + e2 * y2 in f8
  (p6) fma.s1 f8=f10,f8,f8
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (12)
  // r0 = a - b * q3 in f6
  (p6) fnma.d.s1 f6=f7,f9,f6
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (13)
  // q4 = q3 + r0 * y3 in f8
  (p6) fma.d.s0 f8=f6,f8,f9
  nop.i 0;;

  // END DOUBLE PRECISION LATENCY-OPTIMIZED DIVIDE ALGORITHM

} { .mib
  nop.m 0
  nop.i 0
  // return
  br.ret.sptk b0;;
}

END(__divdf3)

