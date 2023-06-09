/*
 * Coefficients for single-precision SVE log function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

const float __sv_logf_poly[] = {
  /* Copied from coeffs for the Neon routine in math/.  */
  -0x1.3e737cp-3f, 0x1.5a9aa2p-3f, -0x1.4f9934p-3f, 0x1.961348p-3f,
  -0x1.00187cp-2f, 0x1.555d7cp-2f, -0x1.ffffc8p-2f,
};
