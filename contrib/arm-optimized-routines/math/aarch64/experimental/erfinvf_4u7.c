/*
 * Single-precision inverse error function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "poly_scalar_f32.h"
#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"

const static struct
{
  /*  We use P_N and Q_N to refer to arrays of coefficients, where P_N is the
      coeffs of the numerator in table N of Blair et al, and Q_N is the coeffs
      of the denominator.  */
  float P_10[3], Q_10[4], P_29[4], Q_29[4], P_50[6], Q_50[3];
} data = { .P_10 = { -0x1.a31268p+3, 0x1.ac9048p+4, -0x1.293ff6p+3 },
	   .Q_10 = { -0x1.8265eep+3, 0x1.ef5eaep+4, -0x1.12665p+4, 0x1p+0 },
	   .P_29
	   = { -0x1.fc0252p-4, 0x1.119d44p+0, -0x1.f59ee2p+0, 0x1.b13626p-2 },
	   .Q_29 = { -0x1.69952p-4, 0x1.c7b7d2p-1, -0x1.167d7p+1, 0x1p+0 },
	   .P_50 = { 0x1.3d8948p-3, 0x1.61f9eap+0, 0x1.61c6bcp-1,
		     -0x1.20c9f2p+0, 0x1.5c704cp-1, -0x1.50c6bep-3 },
	   .Q_50 = { 0x1.3d7dacp-3, 0x1.629e5p+0, 0x1p+0 } };

/* Inverse error function approximation, based on rational approximation as
   described in
   J. M. Blair, C. A. Edwards, and J. H. Johnson,
   "Rational Chebyshev approximations for the inverse of the error function",
   Math. Comp. 30, pp. 827--830 (1976).
   https://doi.org/10.1090/S0025-5718-1976-0421040-7
   Largest error is 4.71 ULP, in the tail region:
   erfinvf(0x1.f84e9ap-1) got 0x1.b8326ap+0
			 want 0x1.b83274p+0.  */
float
erfinvf (float x)
{
  if (x == 1.0f)
    return __math_oflowf (0);
  if (x == -1.0f)
    return __math_oflowf (1);

  float a = fabsf (x);
  if (a > 1.0f)
    return __math_invalidf (x);

  if (a <= 0.75f)
    {
      /* Greatest error in this region is 4.60 ULP:
	 erfinvf(0x1.0a98bap-5) got 0x1.d8a93ep-6
			       want 0x1.d8a948p-6.  */
      float t = x * x - 0.5625f;
      return x * horner_2_f32 (t, data.P_10) / horner_3_f32 (t, data.Q_10);
    }
  if (a < 0.9375f)
    {
      /* Greatest error in this region is 3.79 ULP:
	 erfinvf(0x1.ac82d6p-1) got 0x1.f8fc54p-1
			       want 0x1.f8fc5cp-1.  */
      float t = x * x - 0.87890625f;
      return x * horner_3_f32 (t, data.P_29) / horner_3_f32 (t, data.Q_29);
    }

  /* Tail region, where error is greatest (and sensitive to sqrt and log1p
     implementations.  */
  float t = 1.0 / sqrtf (-log1pf (-a));
  return horner_5_f32 (t, data.P_50)
	 / (copysignf (t, x) * horner_2_f32 (t, data.Q_50));
}

#if USE_MPFR
# warning Not generating tests for erfinvf, as MPFR has no suitable reference
#else
TEST_SIG (S, F, 1, erfinv, -0.99, 0.99)
TEST_ULP (erfinvf, 4.09)
TEST_SYM_INTERVAL (erfinvf, 0, 1, 40000)
#endif
