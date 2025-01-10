/*
 * Data used in single-precision tan(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

const struct tanf_poly_data __tanf_poly_data = {
.poly_tan = {
/* Coefficients generated using:
   poly = fpminimax((tan(sqrt(x))-sqrt(x))/x^(3/2), deg, [|single ...|], [a*a;b*b]);
   optimize relative error
   final prec : 23 bits
   deg : 5
   a : 0x1p-126 ^ 2
   b : ((pi) / 0x1p2) ^ 2
   dirty rel error: 0x1.f7c2e4p-25
   dirty abs error: 0x1.f7c2ecp-25.  */
0x1.55555p-2,
0x1.11166p-3,
0x1.b88a78p-5,
0x1.7b5756p-6,
0x1.4ef4cep-8,
0x1.0e1e74p-7
},
.poly_cotan = {
/* Coefficients generated using:
   fpminimax(f(x) = (0x1p0 / tan(sqrt(x)) - 0x1p0 / sqrt(x)) / sqrt(x), deg, [|dtype ...|], [a;b])
   optimize a single polynomial
   optimize absolute error
   final prec : 23 bits
   working prec : 128 bits
   deg : 3
   a : 0x1p-126
   b : (pi) / 0x1p2
   dirty rel error : 0x1.81298cp-25
   dirty abs error : 0x1.a8acf4p-25.  */
-0x1.55555p-2, /* -0.33333325.  */
-0x1.6c23e4p-6, /* -2.2225354e-2.  */
-0x1.12dbap-9, /* -2.0969994e-3.  */
-0x1.05a1c2p-12, /* -2.495116e-4.  */
}
};
