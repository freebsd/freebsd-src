/*
 * Helper macros for pairwise Horner polynomial evaluation.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

// clang-format off
#define  PW_HORNER_1_(x, c,     i) FMA(x,  c(i + 1),                       c(i))
#define  PW_HORNER_3_(x, x2, c, i) FMA(x2, PW_HORNER_1_ (x,     c, i + 2), PW_HORNER_1_(x, c, i))
#define  PW_HORNER_5_(x, x2, c, i) FMA(x2, PW_HORNER_3_ (x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define  PW_HORNER_7_(x, x2, c, i) FMA(x2, PW_HORNER_5_ (x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define  PW_HORNER_9_(x, x2, c, i) FMA(x2, PW_HORNER_7_ (x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define PW_HORNER_11_(x, x2, c, i) FMA(x2, PW_HORNER_9_ (x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define PW_HORNER_13_(x, x2, c, i) FMA(x2, PW_HORNER_11_(x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define PW_HORNER_15_(x, x2, c, i) FMA(x2, PW_HORNER_13_(x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define PW_HORNER_17_(x, x2, c, i) FMA(x2, PW_HORNER_15_(x, x2, c, i + 2), PW_HORNER_1_(x, c, i))

#define  PAIRWISE_HORNER_1(x,     c) PW_HORNER_1_ (x, c, 0)
#define  PAIRWISE_HORNER_3(x, x2, c) PW_HORNER_3_ (x, x2, c, 0)
#define  PAIRWISE_HORNER_5(x, x2, c) PW_HORNER_5_ (x, x2, c, 0)
#define  PAIRWISE_HORNER_7(x, x2, c) PW_HORNER_7_ (x, x2, c, 0)
#define  PAIRWISE_HORNER_9(x, x2, c) PW_HORNER_9_ (x, x2, c, 0)
#define PAIRWISE_HORNER_11(x, x2, c) PW_HORNER_11_(x, x2, c, 0)
#define PAIRWISE_HORNER_13(x, x2, c) PW_HORNER_13_(x, x2, c, 0)
#define PAIRWISE_HORNER_15(x, x2, c) PW_HORNER_15_(x, x2, c, 0)
#define PAIRWISE_HORNER_17(x, x2, c) PW_HORNER_17_(x, x2, c, 0)

#define  PW_HORNER_2_(x, x2, c, i) FMA(x2, c(i + 2),                       PW_HORNER_1_(x, c, i))
#define  PW_HORNER_4_(x, x2, c, i) FMA(x2, PW_HORNER_2_ (x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define  PW_HORNER_6_(x, x2, c, i) FMA(x2, PW_HORNER_4_ (x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define  PW_HORNER_8_(x, x2, c, i) FMA(x2, PW_HORNER_6_ (x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define PW_HORNER_10_(x, x2, c, i) FMA(x2, PW_HORNER_8_ (x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define PW_HORNER_12_(x, x2, c, i) FMA(x2, PW_HORNER_10_(x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define PW_HORNER_14_(x, x2, c, i) FMA(x2, PW_HORNER_12_(x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define PW_HORNER_16_(x, x2, c, i) FMA(x2, PW_HORNER_14_(x, x2, c, i + 2), PW_HORNER_1_(x, c, i))
#define PW_HORNER_18_(x, x2, c, i) FMA(x2, PW_HORNER_16_(x, x2, c, i + 2), PW_HORNER_1_(x, c, i))

#define  PAIRWISE_HORNER_2(x, x2, c) PW_HORNER_2_ (x, x2, c, 0)
#define  PAIRWISE_HORNER_4(x, x2, c) PW_HORNER_4_ (x, x2, c, 0)
#define  PAIRWISE_HORNER_6(x, x2, c) PW_HORNER_6_ (x, x2, c, 0)
#define  PAIRWISE_HORNER_8(x, x2, c) PW_HORNER_8_(x, x2, c, 0)
#define PAIRWISE_HORNER_10(x, x2, c) PW_HORNER_10_(x, x2, c, 0)
#define PAIRWISE_HORNER_12(x, x2, c) PW_HORNER_12_(x, x2, c, 0)
#define PAIRWISE_HORNER_14(x, x2, c) PW_HORNER_14_(x, x2, c, 0)
#define PAIRWISE_HORNER_16(x, x2, c) PW_HORNER_16_(x, x2, c, 0)
#define PAIRWISE_HORNER_18(x, x2, c) PW_HORNER_18_(x, x2, c, 0)
// clang-format on
