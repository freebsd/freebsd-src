/*
 * Function wrappers for mathbench.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifdef __vpcs

__vpcs static v_float
xy_Z_powf (v_float x)
{
  return _ZGVnN4vv_powf (x, x);
}

__vpcs static v_double
xy_Z_pow (v_double x)
{
  return _ZGVnN2vv_pow (x, x);
}

#endif

static double
xypow (double x)
{
  return pow (x, x);
}

static float
xypowf (float x)
{
  return powf (x, x);
}

static double
xpow (double x)
{
  return pow (x, 23.4);
}

static float
xpowf (float x)
{
  return powf (x, 23.4f);
}

static double
ypow (double x)
{
  return pow (2.34, x);
}

static float
ypowf (float x)
{
  return powf (2.34f, x);
}

static float
sincosf_wrap (float x)
{
  float s, c;
  sincosf (x, &s, &c);
  return s + c;
}
