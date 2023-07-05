/*
 * Function wrappers for mathbench.
 *
 * Copyright (c) 2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#if WANT_VMATH
#if __aarch64__

#ifdef __vpcs
__vpcs static v_float
xy__vn_powf (v_float x)
{
  return __vn_powf (x, x);
}

__vpcs static v_float
xy_Z_powf (v_float x)
{
  return _ZGVnN4vv_powf (x, x);
}

__vpcs static v_double
xy__vn_pow (v_double x)
{
  return __vn_pow (x, x);
}

__vpcs static v_double
xy_Z_pow (v_double x)
{
  return _ZGVnN2vv_pow (x, x);
}
#endif // __vpcs

static v_float
xy__v_powf (v_float x)
{
  return __v_powf (x, x);
}

static v_double
xy__v_pow (v_double x)
{
  return __v_pow (x, x);
}
#endif // __aarch64__

static float
xy__s_powf (float x)
{
  return __s_powf (x, x);
}

static double
xy__s_pow (double x)
{
  return __s_pow (x, x);
}
#endif // WANT_VMATH

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
