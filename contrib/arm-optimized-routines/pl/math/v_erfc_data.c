/*
 * Polynomial coefficients for double-precision erfc(x) vector function.
 *
 * Copyright (c) 2020-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Coefficients for 20 order-12 polynomials used in v_erfc. The intervals have
   the same bounds as the scalar algorithm, with the exception of the lower
   bound of the first interval which is larger. This is because the vector
   variants fall back to the scalar for tiny arguments, meaning that we can use
   a slightly different approach which is more precise for larger inputs but
   unacceptably imprecise for tiny inputs.  */

const struct v_erfc_data __v_erfc_data = {

/* Bounds for 20 intervals spanning [0x1.0p-28., 31.]. Interval bounds are a
   logarithmic scale, i.e. interval n has lower bound 2^(n/4) - 1, with the
   exception of the first interval.  */
.interval_bounds = {
  0x1p-28,		/* If xmin=2^-28, 0 otherwise.  */
  0x1.837f0518db8a9p-3, /* 0.189.  */
  0x1.a827999fcef32p-2, /* 0.414.  */
  0x1.5d13f32b5a75bp-1, /* 0.682.  */
  0x1.0p0,		/* 1.000.  */
  0x1.60dfc14636e2ap0,	/* 1.378.  */
  0x1.d413cccfe779ap0,	/* 1.828.  */
  0x1.2e89f995ad3adp1,	/* 2.364.  */
  0x1.8p1,		/* 3.000.  */
  0x1.e0dfc14636e2ap1,	/* 3.757.  */
  0x1.2a09e667f3bcdp2,	/* 4.657.  */
  0x1.6e89f995ad3adp2,	/* 5.727.  */
  0x1.cp2,		/* 7.000.  */
  0x1.106fe0a31b715p3,	/* 8.514.  */
  0x1.4a09e667f3bcdp3,	/* 10.31.  */
  0x1.8e89f995ad3adp3,	/* 12.45.  */
  0x1.ep3,		/* 15.00.  */
  0x1.206fe0a31b715p4,	/* 18.03.  */
  0x1.5a09e667f3bcdp4,	/* 21.63.  */
  0x1.9e89f995ad3adp4,	/* 25.91.  */
  0x1.fp4		/* 31.00.  */
},

/* Generated using fpminimax algorithm on each interval separately. The
   polynomial approximates erfc(x + a) * exp((x + a) ^ 2) in the interval
   [0;b-a], where [a;b] is the interval in which the input lies. Note this is
   slightly different from the scalar polynomial, which approximates
   erfc(x + a) * exp(x ^ 2). See v_erfc.sollya for more details.  */
.poly = {
/* 3.725290298461914e-9 < x < 0.18920711500272103.  */
{0x1.ffffffdbe4516p-1, -0x1.20dd74e429b54p0, 0x1.ffffffb7c6a67p-1, -0x1.8127466fa2ec9p-1, 0x1.ffffff6eeff5ap-2, -0x1.341f668c90dccp-2, 0x1.5554aca74e5d6p-3, -0x1.6014d9d3fed0dp-4, 0x1.546b5f2c85127p-5, -0x1.2f7ec79acc129p-6, 0x1.a27e53703b7abp-8, 0x1.7b18bce311fa3p-12, -0x1.1897cda04df3ap-9},
/* 0.18920711500272103 < x < 0.41421356237309515.  */
{0x1.a2b43de077724p-1, -0x1.a3495bb58664cp-1, 0x1.535f3ff4547e6p-1, -0x1.d96eea2951a7cp-2, 0x1.269566a956371p-2, -0x1.4e281de026b47p-3, 0x1.5ea071b652a2fp-4, -0x1.57f46cfca7024p-5, 0x1.3db28243f06abp-6, -0x1.138745eef6f26p-7, 0x1.a9cd70bad344p-9, -0x1.c6e4fda8920c4p-11, 0x1.624709ca2bc71p-16},
/* 0.41421356237309515 < x < 0.681792830507429.  */
{0x1.532e75764e513p-1, -0x1.28be34f327f9dp-1, 0x1.b088738cca84cp-2, -0x1.14377551bd5c8p-2, 0x1.3e1ecedd64246p-3, -0x1.5087f3110eb57p-4, 0x1.4b3c61efcb562p-5, -0x1.324cc70a4f459p-6, 0x1.0cd19a96af21bp-7, -0x1.cc2ccc725d07p-9, 0x1.a3ba67a7d02b4p-10, -0x1.b1943295882abp-11, 0x1.53a1c5fdf8e67p-12},
/* 0.681792830507429 < x < 1.  */
{0x1.10f974588f63dp-1, -0x1.9b032139e3367p-2, 0x1.09b942b8a951dp-2, -0x1.327553909cb88p-3, 0x1.42819b6c9a14p-4, -0x1.3a6d6f1924825p-5, 0x1.1f1864dd6f28fp-6, -0x1.ef12c5e9f3232p-8, 0x1.962ac63d55aa1p-9, -0x1.4146d9206419cp-10, 0x1.f823f62268229p-12, -0x1.837ab488d5ed8p-13, 0x1.aa021ae16edfep-15},
/* 1 < x < 1.378414230005442.  */
{0x1.b5d8780f956b2p-2, -0x1.17c4e3f17c034p-2, 0x1.3c27283c31939p-3, -0x1.44837f88a0ecdp-4, 0x1.33cad0dc779c8p-5, -0x1.10fcef8294e8dp-6, 0x1.c8cb3e5a6a5a6p-8, -0x1.6aedbd3a05f1cp-9, 0x1.1325c0bf9a0cap-10, -0x1.8e28d61a0f646p-12, 0x1.0d554e2ab3652p-13, -0x1.35b5f9ac296ebp-15, 0x1.b8faf07e2527dp-18},
/* 1.378414230005442 < x < 1.8284271247461903.  */
{0x1.5ee444130b7dbp-2, -0x1.78396ab2083e8p-3, 0x1.6e617ec5bc039p-4, -0x1.49e60f6238765p-5, 0x1.16064fb4428c9p-6, -0x1.ba80a8575a434p-8, 0x1.4ec30f2efeb8p-9, -0x1.e40456c735f09p-11, 0x1.4f7ee6b7885b7p-12, -0x1.bc9997995fdecp-14, 0x1.1169f7327ff2p-15, -0x1.174826d000852p-17, 0x1.5506a7433e925p-20},
/* 1.8284271247461903 < x < 2.363585661014858.  */
{0x1.19a22c064d4eap-2, -0x1.f645498cae1b3p-4, 0x1.a0565950e1256p-5, -0x1.446605c186f6dp-6, 0x1.df1231b47ff04p-8, -0x1.515164d13dfafp-9, 0x1.c72bde869ad61p-11, -0x1.2768fbf9b1d6ep-12, 0x1.71bd3a1b851e9p-14, -0x1.bca5b5942017cp-16, 0x1.f2d480b3a2e63p-18, -0x1.d339662d53467p-20, 0x1.06d67ebf792bp-22},
/* 2.363585661014858 < x < 3.  */
{0x1.c57f0542a7637p-3, -0x1.4e5535c17af25p-4, 0x1.d31272523acfep-6, -0x1.3727cbbfd1bfcp-7, 0x1.8d6730b8c5a4cp-9, -0x1.e88548286036fp-11, 0x1.21f6e89456853p-12, -0x1.4d4b7787bd3c2p-14, 0x1.735dc84e7ff16p-16, -0x1.8eb02db832048p-18, 0x1.8dfb8add3b86ep-20, -0x1.47a340d76c72bp-22, 0x1.3e5925ffebe6bp-25},
/* 3 < x < 3.756828460010884.  */
{0x1.6e9827d229d2dp-3, -0x1.bd6ae4d14b1adp-5, 0x1.043fe1a98c3b9p-6, -0x1.259061ba34453p-8, 0x1.409cc2cc96bedp-10, -0x1.53dec3fd6c443p-12, 0x1.5e72f7baf3554p-14, -0x1.601aa94bf21eep-16, 0x1.58e730ceaa91dp-18, -0x1.4762cbd256163p-20, 0x1.22b8bea5d4a5ap-22, -0x1.ac197af37fcadp-25, 0x1.74cdf138a0b73p-28},
/* 3.756828460010884 < x < 4.656854249492381.  */
{0x1.29a8a4e95063ep-3, -0x1.29a8a316d331dp-5, 0x1.21876b3fe50cfp-7, -0x1.1276f2d8eefd9p-9, 0x1.fbff521741e5cp-12, -0x1.cb9ce996b9601p-14, 0x1.971075371ef81p-16, -0x1.61458571e4738p-18, 0x1.2c51c21b7ab9ep-20, -0x1.f01e444a666c3p-23, 0x1.7e8f2979b67f1p-25, -0x1.e505367843027p-28, 0x1.67809d68de49cp-31},
/* 4.656854249492381 < x < 5.727171322029716.  */
{0x1.e583024e2bc7fp-4, -0x1.8fb458acb5acep-6, 0x1.42b9dffac075cp-8, -0x1.ff9fe9a48522p-11, 0x1.8e7e866f4f073p-13, -0x1.313aeee1c2d45p-15, 0x1.cc299efd7374cp-18, -0x1.5587e53442d66p-20, 0x1.f2aca160f159bp-23, -0x1.62ae4834dcda7p-25, 0x1.d6b070147cb37p-28, -0x1.fee399e7be1bfp-31, 0x1.41d6f9fbc9515p-34},
/* 5.727171322029716 < x < 7.  */
{0x1.8d9cbafa30408p-4, -0x1.0dd14614ed1cfp-6, 0x1.6943976ea6bf4p-9, -0x1.dd6f05f3b914cp-12, 0x1.37891317e7bcfp-14, -0x1.91a81ce9014a2p-17, 0x1.ffcac303208b9p-20, -0x1.424f1af78feb3p-22, 0x1.90b8edbca12a5p-25, -0x1.e69bea0338c7fp-28, 0x1.13b974a710373p-30, -0x1.fdc9aa9359794p-34, 0x1.105fc772b5a66p-37},
/* 7 < x < 8.513656920021768.  */
{0x1.46dc6bf900f68p-4, -0x1.6e4b45246f95p-7, 0x1.96a3de47d4bd7p-10, -0x1.bf5070eccb409p-13, 0x1.e7af6e83607a2p-16, -0x1.078bf5306f9eep-18, 0x1.1a6e8327243adp-21, -0x1.2c1e7368c7809p-24, 0x1.3bc83557dac43p-27, -0x1.45a6405b2e649p-30, 0x1.3aac4888689ebp-33, -0x1.f1fa23448a168p-37, 0x1.c868668755778p-41},
/* 8.513656920021768 < x < 10.313708498984761.  */
{0x1.0d9a17e032288p-4, -0x1.f3e942ff4df7p-8, 0x1.cc77f09dabc5cp-11, -0x1.a56e8bfd32da8p-14, 0x1.7f49e31164409p-17, -0x1.5a73f46a6afc9p-20, 0x1.374240ce973d2p-23, -0x1.15e8d473b728cp-26, 0x1.ec3ec79699378p-30, -0x1.ab3b8aba63362p-33, 0x1.5a1381cfe2866p-36, -0x1.c78e252ce77ccp-40, 0x1.589857ceaaaeep-44},
/* 10.313708498984761 < x < 12.454342644059432.  */
{0x1.be0c73cc19eddp-5, -0x1.56ce6f6c0cbb1p-8, 0x1.0645980ecbbfcp-11, -0x1.8f86f887f6598p-15, 0x1.2ef80cd9e00b1p-18, -0x1.c97ffd66720e4p-22, 0x1.57f0eeecf030ap-25, -0x1.016df7d5e28d9p-28, 0x1.7f0d022922f1dp-32, -0x1.1849731f004aep-35, 0x1.8149e7ca0fb3cp-39, -0x1.b1fe4abe62d81p-43, 0x1.1ae4d60247651p-47},
/* 12.454342644059432 < x < 15.  */
{0x1.71eafbd9f5877p-5, -0x1.d83714d90461fp-9, 0x1.2c74dbacd45fdp-12, -0x1.7d27f3cfe160ep-16, 0x1.e20b13b8d32e3p-20, -0x1.2fe33cb2bce33p-23, 0x1.7dfd564d69a07p-27, -0x1.dea62ef0f7d7ep-31, 0x1.2a7b946273ea5p-34, -0x1.6eb665bad5b72p-38, 0x1.a8191750e8bf9p-42, -0x1.92d8a86cbd0fcp-46, 0x1.bba272feef841p-51},
/* 15 < x < 18.027313840043536.  */
{0x1.33714a024097ep-5, -0x1.467f441a50bc3p-9, 0x1.59fa2994c6f7ap-13, -0x1.6dd369d642b7dp-17, 0x1.81fb2aaf2e37p-21, -0x1.966040990b623p-25, 0x1.aaee55e15a079p-29, -0x1.bf756fc8ef04p-33, 0x1.d2daf554e0157p-37, -0x1.dec63e10d317p-41, 0x1.cae915bab7704p-45, -0x1.6537fbb62a8edp-49, 0x1.3f14bd5531da8p-54},
/* 18.027313840043536 < x < 21.627416997969522.  */
{0x1.fff97acd75487p-6, -0x1.c502e8e46eb81p-10, 0x1.903b065062756p-14, -0x1.6110aa5e81885p-18, 0x1.36fd4c13c4f1fp-22, -0x1.11848650be987p-26, 0x1.e06596bf6a27p-31, -0x1.a527876771d55p-35, 0x1.6fe1b92a40eb8p-39, -0x1.3c6eb50b23bc6p-43, 0x1.fead2230125dp-48, -0x1.5073427c5207dp-52, 0x1.ff420973fa51dp-58},
/* 21.627416997969522 < x < 25.908685288118864.  */
{0x1.aaf347fc8c45bp-6, -0x1.3b2fd709cf8e5p-10, 0x1.d0ddfb858b60ap-15, -0x1.5673f4a8bb08ep-19, 0x1.f80488e89ddb9p-24, -0x1.728391905fcf3p-28, 0x1.101538d7e30bap-32, -0x1.8f16f49d0fa3bp-37, 0x1.23bbaea534034p-41, -0x1.a40119533ee1p-46, 0x1.1b75770e435fdp-50, -0x1.3804bdeb33efdp-55, 0x1.8ba4e7838a4dp-61},
/* 25.908685288118864 < x < 31.  */
{0x1.64839d636f92bp-6, -0x1.b7adf753623afp-11, 0x1.0eec0b635a0c4p-15, -0x1.4da09b802ef48p-20, 0x1.9a8b149f5ddf1p-25, -0x1.f8d1f722c65bap-30, 0x1.36247d9a20e19p-34, -0x1.7cbd25180c1d3p-39, 0x1.d243c7a5c8331p-44, -0x1.19e00cc6b1e08p-48, 0x1.418cb6823f2d9p-53, -0x1.2dfdc526c43acp-58, 0x1.49885a987486fp-64},
/* Dummy interval for x>31 */
{0x0p0, 0x0p0, 0x0p0, 0x0p0, 0x0p0, 0x0p0, 0x0p0, 0x0p0, 0x0p0, 0x0p0,
 0x0p0, 0x0p0, 0x0p0}
}
};
