// $FreeBSD$

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

ENTRY(__divsf3, 0)
{ .mfi
  // a is in f8
  // b is in f9

  // general registers used: r31, r32, r33, r34
  // predicate registers used: p6
  // floating-point registers used: f6, f7, f8

  nop.m 0
  // load a, the first argument, in f6
  mov f6=f8
  nop.i 0;;
} { .mfi
  nop.m 0
  // load b, the second argument, in f7
  mov f7=f9
  nop.i 0;;
} { .mfi

  // BEGIN SINGLE PRECISION LATENCY-OPTIMIZED DIVIDE ALGORITHM

  nop.m 0
  // Step (1)
  // y0 = 1 / b in f8
  frcpa.s0 f8,p6=f6,f7
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (2)
  // q0 = a * y0 in f6
  (p6) fma.s1 f6=f6,f8,f0
  nop.i 0
} { .mfi
  nop.m 0
  // Step (3)
  // e0 = 1 - b * y0 in f7
  (p6) fnma.s1 f7=f7,f8,f1
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (4)
  // q1 = q0 + e0 * q0 in f6
  (p6) fma.s1 f6=f7,f6,f6
  nop.i 0
} { .mfi
  nop.m 0
  // Step (5)
  // e1 = e0 * e0 in f7
  (p6) fma.s1 f7=f7,f7,f0
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (6)
  // q2 = q1 + e1 * q1 in f6
  (p6) fma.s1 f6=f7,f6,f6
  nop.i 0
} { .mfi
  nop.m 0
  // Step (7)
  // e2 = e1 * e1 in f7
  (p6) fma.s1 f7=f7,f7,f0
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (8)
  // q3 = q2 + e2 * q2 in f6
  (p6) fma.d.s1 f6=f7,f6,f6
  nop.i 0;;
} { .mfi
  nop.m 0
  // Step (9)
  // q3' = q3 in f8
  (p6) fma.s.s0 f8=f6,f1,f0
  nop.i 0;;

  // END SINGLE PRECISION LATENCY-OPTIMIZED DIVIDE ALGORITHM

} { .mmb
  nop.m 0
  nop.m 0
  // return
  br.ret.sptk b0;;
}

END(__divsf3)
