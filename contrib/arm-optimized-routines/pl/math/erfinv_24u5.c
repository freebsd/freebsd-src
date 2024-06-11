/*
 * Double-precision inverse error function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "math_config.h"
#include "poly_scalar_f64.h"
#include "pl_sig.h"
#define IGNORE_SCALAR_FENV
#include "pl_test.h"

const static struct
{
  /*  We use P_N and Q_N to refer to arrays of coefficients, where P_N is the
      coeffs of the numerator in table N of Blair et al, and Q_N is the coeffs
      of the denominator.  */
  double P_17[7], Q_17[7], P_37[8], Q_37[8], P_57[9], Q_57[10];
} data = {
  .P_17 = { 0x1.007ce8f01b2e8p+4, -0x1.6b23cc5c6c6d7p+6, 0x1.74e5f6ceb3548p+7,
	    -0x1.5200bb15cc6bbp+7, 0x1.05d193233a849p+6, -0x1.148c5474ee5e1p+3,
	    0x1.689181bbafd0cp-3 },
  .Q_17 = { 0x1.d8fb0f913bd7bp+3, -0x1.6d7f25a3f1c24p+6, 0x1.a450d8e7f4cbbp+7,
	    -0x1.bc3480485857p+7, 0x1.ae6b0c504ee02p+6, -0x1.499dfec1a7f5fp+4,
	    0x1p+0 },
  .P_37 = { -0x1.f3596123109edp-7, 0x1.60b8fe375999ep-2, -0x1.779bb9bef7c0fp+1,
	    0x1.786ea384470a2p+3, -0x1.6a7c1453c85d3p+4, 0x1.31f0fc5613142p+4,
	    -0x1.5ea6c007d4dbbp+2, 0x1.e66f265ce9e5p-3 },
  .Q_37 = { -0x1.636b2dcf4edbep-7, 0x1.0b5411e2acf29p-2, -0x1.3413109467a0bp+1,
	    0x1.563e8136c554ap+3, -0x1.7b77aab1dcafbp+4, 0x1.8a3e174e05ddcp+4,
	    -0x1.4075c56404eecp+3, 0x1p+0 },
  .P_57 = { 0x1.b874f9516f7f1p-14, 0x1.5921f2916c1c4p-7, 0x1.145ae7d5b8fa4p-2,
	    0x1.29d6dcc3b2fb7p+1, 0x1.cabe2209a7985p+2, 0x1.11859f0745c4p+3,
	    0x1.b7ec7bc6a2ce5p+2, 0x1.d0419e0bb42aep+1, 0x1.c5aa03eef7258p-1 },
  .Q_57 = { 0x1.b8747e12691f1p-14, 0x1.59240d8ed1e0ap-7, 0x1.14aef2b181e2p-2,
	    0x1.2cd181bcea52p+1, 0x1.e6e63e0b7aa4cp+2, 0x1.65cf8da94aa3ap+3,
	    0x1.7e5c787b10a36p+3, 0x1.0626d68b6cea3p+3, 0x1.065c5f193abf6p+2,
	    0x1p+0 }
};

/* Inverse error function approximation, based on rational approximation as
   described in
   J. M. Blair, C. A. Edwards, and J. H. Johnson,
   "Rational Chebyshev approximations for the inverse of the error function",
   Math. Comp. 30, pp. 827--830 (1976).
   https://doi.org/10.1090/S0025-5718-1976-0421040-7
   Largest observed error is 24.46 ULP, in the extreme tail:
   erfinv(0x1.fd9504351b757p-1) got 0x1.ff72c1092917p+0
			       want 0x1.ff72c10929158p+0.  */
double
erfinv (double x)
{
  double a = fabs (x);

  if (a <= 0.75)
    {
      /* Largest observed error in this region is 6.06 ULP:
	 erfinv(0x1.1884650fd2d41p-2) got 0x1.fb65998cbd3fep-3
				     want 0x1.fb65998cbd404p-3.  */
      double t = x * x - 0.5625;
      return x * horner_6_f64 (t, data.P_17) / horner_6_f64 (t, data.Q_17);
    }

  if (a <= 0.9375)
    {
      /* Largest observed error in this region is 6.95 ULP:
	 erfinv(0x1.a8d65b94d8c6p-1) got 0x1.f08325591b54p-1
				    want 0x1.f08325591b547p-1.  */
      double t = x * x - 0.87890625;
      return x * horner_7_f64 (t, data.P_37) / horner_7_f64 (t, data.Q_37);
    }

  double t = 1.0 / (sqrt (-log (1 - a)));
  return horner_8_f64 (t, data.P_57)
	 / (copysign (t, x) * horner_9_f64 (t, data.Q_57));
}

PL_SIG (S, D, 1, erfinv, -0.99, 0.99)
PL_TEST_ULP (erfinv, 24.0)
PL_TEST_INTERVAL (erfinv, 0, 1, 40000)
PL_TEST_INTERVAL (erfinv, -0x1p-1022, -1, 40000)
