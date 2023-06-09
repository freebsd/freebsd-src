/*
 * Data used in double-precision erfc(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Polynomial coefficients for approximating erfc(x)*exp(x*x) in double
   precision. Generated using the Remez algorithm on each interval separately
   (see erfc.sollya for more detail).  */
const struct erfc_data __erfc_data = {

/* Bounds for 20 intervals spanning [0x1.0p-50., 31.]. Interval bounds are a
   logarithmic scale, i.e. interval n has lower bound 2^(n/4) - 1, with the
   exception of the first interval.  */
.interval_bounds = {
  0x1.0p-50,		/* Tiny boundary.  */
  0x1.837f05c490126p-3, /* 0.189.  */
  0x1.a827997709f7ap-2, /* 0.414.  */
  0x1.5d13f326fe9c8p-1, /* 0.682.  */
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

/* Coefficients for each order 12 polynomial on each of the 20 intervals.  */
.poly = {
  {0x1.ffffffffffff6p-1, -0x1.20dd750429b66p0, 0x1.fffffffffffdcp-1,
   -0x1.812746b03713ap-1, 0x1.ffffffffbe94cp-2, -0x1.341f6bb6ec9a6p-2,
   0x1.555553a70ec2ep-3, -0x1.6023b4617a388p-4, 0x1.5550f0e40bfbap-5,
   -0x1.38c290c0c8de8p-6, 0x1.0e84002c6274ep-7, -0x1.a599eb0ac5d04p-9,
   0x1.c9bfafa73899cp-11},
  {0x1.a2b43dbd503c8p-1, -0x1.a3495b7c9e6a4p-1, 0x1.535f3fb8cb92ap-1,
   -0x1.d96ee9c714f44p-2, 0x1.26956676d2c64p-2, -0x1.4e2820da90c08p-3,
   0x1.5ea0cffac775ap-4, -0x1.57fb82ca373e8p-5, 0x1.3e0e8f48ba0f8p-6,
   -0x1.16a695af1bbd4p-7, 0x1.cc836241a87d4p-9, -0x1.531de41264fdap-10,
   0x1.526a8a14e9bfcp-12},
  {0x1.532e75821ed48p-1, -0x1.28be350460782p-1, 0x1.b08873adbf108p-2,
   -0x1.14377569249e2p-2, 0x1.3e1ece8cd10dap-3, -0x1.5087e2e6dc2e8p-4,
   0x1.4b3adb3bb335ap-5, -0x1.32342d711a4f4p-6, 0x1.0bc4f6ce2b656p-7,
   -0x1.bcdaa331f2144p-9, 0x1.5c21c9e0ca954p-10, -0x1.dfdc9b3b5c402p-12,
   0x1.b451af7dd52fep-14},
  {0x1.10f9745a4f44ap-1, -0x1.9b03213e6963ap-2, 0x1.09b942bc8de66p-2,
   -0x1.32755394481e4p-3, 0x1.42819b18af0e4p-4, -0x1.3a6d643aaa572p-5,
   0x1.1f17897603eaep-6, -0x1.eefb8d3f89d42p-8, 0x1.95559544f2fbp-9,
   -0x1.3c2a67c33338p-10, 0x1.cffa784efe6cp-12, -0x1.282646774689cp-13,
   0x1.e654e67532b44p-16},
  {0x1.b5d8780f956b2p-2, -0x1.17c4e3f17c04dp-2, 0x1.3c27283c328dbp-3,
   -0x1.44837f88ea4bdp-4, 0x1.33cad0e887482p-5, -0x1.10fcf0bc8963cp-6,
   0x1.c8cb68153ec42p-8, -0x1.6aef9a9842c54p-9, 0x1.1334345d6467cp-10,
   -0x1.8ebe8763a2a8cp-12, 0x1.0f457219dec0dp-13, -0x1.3d2501dcd2a0fp-15,
   0x1.d213a128a75c9p-18},
  {0x1.5ee444130b7dbp-2, -0x1.78396ab208478p-3, 0x1.6e617ec5c0cc3p-4,
   -0x1.49e60f63656b5p-5, 0x1.16064fddbbcb9p-6, -0x1.ba80af6a31018p-8,
   0x1.4ec374269d4ecp-9, -0x1.e40be960703a4p-11, 0x1.4fb029f35a144p-12,
   -0x1.be45fd71a60eap-14, 0x1.161235cd2a3e7p-15, -0x1.264890eb1b5ebp-17,
   0x1.7f90154bde15dp-20},
  {0x1.19a22c064d4eap-2, -0x1.f645498cae217p-4, 0x1.a0565950e3f08p-5,
   -0x1.446605c21c178p-6, 0x1.df1231d75622fp-8, -0x1.515167553de25p-9,
   0x1.c72c1b4a2a57fp-11, -0x1.276ae9394ecf1p-12, 0x1.71d2696d6c8c3p-14,
   -0x1.bd4152984ce1dp-16, 0x1.f5afd2b450df7p-18, -0x1.dafdaddc7f943p-20,
   0x1.1020f4741f79ep-22},
  {0x1.c57f0542a7637p-3, -0x1.4e5535c17afc8p-4, 0x1.d312725242824p-6,
   -0x1.3727cbc12a4bbp-7, 0x1.8d6730fc45b6bp-9, -0x1.e8855055c9b53p-11,
   0x1.21f73b70cc792p-12, -0x1.4d4fe06f13831p-14, 0x1.73867a82f7484p-16,
   -0x1.8fab204d1d75ep-18, 0x1.91d9ba10367f4p-20, -0x1.5077ce4b334ddp-22,
   0x1.501716d098f14p-25},
  {0x1.6e9827d229d2dp-3, -0x1.bd6ae4d14b135p-5, 0x1.043fe1a989f11p-6,
   -0x1.259061b98cf96p-8, 0x1.409cc2b1c4fc2p-10, -0x1.53dec152f6abfp-12,
   0x1.5e72cb4cc919fp-14, -0x1.6018b68100642p-16, 0x1.58d859380fb24p-18,
   -0x1.471723286dad5p-20, 0x1.21c1a0f7a6593p-22, -0x1.a872678d91154p-25,
   0x1.6eb74e2e99662p-28},
  {0x1.29a8a4e95063ep-3, -0x1.29a8a316d3318p-5, 0x1.21876b3fe4f84p-7,
   -0x1.1276f2d8ee36cp-9, 0x1.fbff52181a454p-12, -0x1.cb9ce9bde195ep-14,
   0x1.9710786fa90c5p-16, -0x1.6145ad5b471dcp-18, 0x1.2c52fac57009cp-20,
   -0x1.f02a8711f07cfp-23, 0x1.7eb574960398cp-25, -0x1.e58ce325343aap-28,
   0x1.68510d1c32842p-31},
  {0x1.e583024e2bc8p-4, -0x1.8fb458acb5b0fp-6, 0x1.42b9dffac2531p-8,
   -0x1.ff9fe9a553dddp-11, 0x1.8e7e86883ba0bp-13, -0x1.313af0bb12375p-15,
   0x1.cc29ccb17372ep-18, -0x1.55895fbb1ae42p-20, 0x1.f2bd2d6c7fd07p-23,
   -0x1.62ec031844613p-25, 0x1.d7d69ce7c1847p-28, -0x1.0106b95e4db03p-30,
   0x1.45aabbe505f6ap-34},
  {0x1.8d9cbafa30408p-4, -0x1.0dd14614ed20fp-6, 0x1.6943976ea9dcap-9,
   -0x1.dd6f05f4d7ce8p-12, 0x1.37891334aa621p-14, -0x1.91a8207766e1ep-17,
   0x1.ffcb0c613d75cp-20, -0x1.425116a6c88dfp-22, 0x1.90cb7c902d428p-25,
   -0x1.e70fc740c3b6dp-28, 0x1.14a09ae5851ep-30, -0x1.00f9e03eae993p-33,
   0x1.14989aac741c2p-37},
  {0x1.46dc6bf900f68p-4, -0x1.6e4b45246f8dp-7, 0x1.96a3de47cfdb5p-10,
   -0x1.bf5070eb6823bp-13, 0x1.e7af6e4aa8ef8p-16, -0x1.078bf26142831p-18,
   0x1.1a6e547aa40bep-21, -0x1.2c1c68f62f614p-24, 0x1.3bb8b473dd9e7p-27,
   -0x1.45576cacb45a1p-30, 0x1.39ab71899b44ep-33, -0x1.ee307d46e2866p-37,
   0x1.c21ba1b404f5ap-41},
  {0x1.0d9a17e032288p-4, -0x1.f3e942ff4e097p-8, 0x1.cc77f09db5af8p-11,
   -0x1.a56e8bffaab5cp-14, 0x1.7f49e36974e03p-17, -0x1.5a73fc0025d2fp-20,
   0x1.3742ae06a8be6p-23, -0x1.15ecf5317789bp-26, 0x1.ec74dd2b109fp-30,
   -0x1.ac28325f88dc1p-33, 0x1.5ca9e8d7841b2p-36, -0x1.cfef04667185fp-40,
   0x1.6487c50052867p-44},
  {0x1.be0c73cc19eddp-5, -0x1.56ce6f6c0cb33p-8, 0x1.0645980ec8568p-11,
   -0x1.8f86f88695a8cp-15, 0x1.2ef80cb1dca7cp-18, -0x1.c97ff7c599a6dp-22,
   0x1.57f0ac907d436p-25, -0x1.016be8d812c69p-28, 0x1.7ef6d33c73b75p-32,
   -0x1.17f9784eda0d4p-35, 0x1.7fd8662b486f1p-39, -0x1.ae21758156d89p-43,
   0x1.165732f1ae138p-47},
  {0x1.71eafbd9f5877p-5, -0x1.d83714d904525p-9, 0x1.2c74dbaccea28p-12,
   -0x1.7d27f3cdea565p-16, 0x1.e20b13581fcf8p-20, -0x1.2fe336f089679p-23,
   0x1.7dfce36129db3p-27, -0x1.dea026ee03f14p-31, 0x1.2a6019f7c64b1p-34,
   -0x1.6e0eeb9f98eeap-38, 0x1.a58b4ed07d741p-42, -0x1.8d12c77071e4cp-46,
   0x1.b0241c6d5b761p-51},
  {0x1.33714a024097ep-5, -0x1.467f441a50cbdp-9, 0x1.59fa2994d0e65p-13,
   -0x1.6dd369d9306cap-17, 0x1.81fb2b2af9413p-21, -0x1.96604d3c1bb6ep-25,
   0x1.aaef2da14243p-29, -0x1.bf7f1b935d3ebp-33, 0x1.d3261ebcd2061p-37,
   -0x1.e04c803bbd875p-41, 0x1.cff98a43bacdep-45, -0x1.6ef39a63cf675p-49,
   0x1.4f8abb4398a0dp-54},
  {0x1.fff97acd75487p-6, -0x1.c502e8e46ec0cp-10, 0x1.903b0650672eap-14,
   -0x1.6110aa5fb096fp-18, 0x1.36fd4c3e4040cp-22, -0x1.118489fe28728p-26,
   0x1.e06601208ac47p-31, -0x1.a52b90c21650ap-35, 0x1.6ffc42c05429bp-39,
   -0x1.3ce3322a6972ep-43, 0x1.009d8ef37ff8cp-47, -0x1.5498d2cc51c99p-52,
   0x1.058cd4ea9bf04p-57},
  {0x1.aaf347fc8c45bp-6, -0x1.3b2fd709cf97dp-10, 0x1.d0ddfb8593f4p-15,
   -0x1.5673f4aa86542p-19, 0x1.f8048954325f6p-24, -0x1.72839959ab3e9p-28,
   0x1.101597113be2ap-32, -0x1.8f1cf0ff4adeep-37, 0x1.23dca407fd66p-41,
   -0x1.a4f387e57a6a5p-46, 0x1.1dafd753f65e9p-50, -0x1.3e15343c973d6p-55,
   0x1.9a2af47d77e44p-61},
  {0x1.64839d636f92bp-6, -0x1.b7adf7536232dp-11, 0x1.0eec0b6357148p-15,
   -0x1.4da09b7f2c52bp-20, 0x1.9a8b146de838ep-25, -0x1.f8d1f145e7b6fp-30,
   0x1.3624435b3ba11p-34, -0x1.7cba19b4af977p-39, 0x1.d2282481ba91ep-44,
   -0x1.198c1e91f9564p-48, 0x1.4046224f8ccp-53, -0x1.2b1dc676c096fp-58,
   0x1.43d3358c64dafp-64}
}
};
