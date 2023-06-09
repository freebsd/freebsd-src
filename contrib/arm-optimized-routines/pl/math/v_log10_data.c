/*
 * Lookup table for double-precision log10(x) vector function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

#define N (1 << V_LOG10_TABLE_BITS)

/* Algorithm:

	x = 2^k z
	log10(x) = k log10(2) + log10(c) + poly(z/c - 1) / log(10)

where z is in [a;2a) which is split into N subintervals (a=0x1.69009p-1,N=128)
and log(c) and 1/c for the ith subinterval comes from a lookup table:

	tab[i].invc = 1/c
	tab[i].log10c = (double)log10(c)

where c is near the center of the subinterval and is chosen by trying several
floating point invc candidates around 1/center and selecting one for which
the error in (double)log(c) is minimized (< 0x1p-74), except the subinterval
that contains 1 and the previous one got tweaked to avoid cancellation.
NB: invc should be optimized to minimize error in (double)log10(c) instead.  */
const struct v_log10_data __v_log10_data
  = {.tab = {{0x1.6a133d0dec120p+0, -0x1.345825f221684p-3},
	     {0x1.6815f2f3e42edp+0, -0x1.2f71a1f0c554ep-3},
	     {0x1.661e39be1ac9ep+0, -0x1.2a91fdb30b1f4p-3},
	     {0x1.642bfa30ac371p+0, -0x1.25b9260981a04p-3},
	     {0x1.623f1d916f323p+0, -0x1.20e7081762193p-3},
	     {0x1.60578da220f65p+0, -0x1.1c1b914aeefacp-3},
	     {0x1.5e75349dea571p+0, -0x1.1756af5de404dp-3},
	     {0x1.5c97fd387a75ap+0, -0x1.12985059c90bfp-3},
	     {0x1.5abfd2981f200p+0, -0x1.0de0628f63df4p-3},
	     {0x1.58eca051dc99cp+0, -0x1.092ed492e08eep-3},
	     {0x1.571e526d9df12p+0, -0x1.0483954caf1dfp-3},
	     {0x1.5554d555b3fcbp+0, -0x1.ffbd27a9adbcp-4},
	     {0x1.539015e2a20cdp+0, -0x1.f67f7f2e3d1ap-4},
	     {0x1.51d0014ee0164p+0, -0x1.ed4e1071ceebep-4},
	     {0x1.50148538cd9eep+0, -0x1.e428bb47413c4p-4},
	     {0x1.4e5d8f9f698a1p+0, -0x1.db0f6003028d6p-4},
	     {0x1.4cab0edca66bep+0, -0x1.d201df6749831p-4},
	     {0x1.4afcf1a9db874p+0, -0x1.c9001ac5c9672p-4},
	     {0x1.495327136e16fp+0, -0x1.c009f3c78c79p-4},
	     {0x1.47ad9e84af28fp+0, -0x1.b71f4cb642e53p-4},
	     {0x1.460c47b39ae15p+0, -0x1.ae400818526b2p-4},
	     {0x1.446f12b278001p+0, -0x1.a56c091954f87p-4},
	     {0x1.42d5efdd720ecp+0, -0x1.9ca3332f096eep-4},
	     {0x1.4140cfe001a0fp+0, -0x1.93e56a3f23e55p-4},
	     {0x1.3fafa3b421f69p+0, -0x1.8b3292a3903bp-4},
	     {0x1.3e225c9c8ece5p+0, -0x1.828a9112d9618p-4},
	     {0x1.3c98ec29a211ap+0, -0x1.79ed4ac35f5acp-4},
	     {0x1.3b13442a413fep+0, -0x1.715aa51ed28c4p-4},
	     {0x1.399156baa3c54p+0, -0x1.68d2861c999e9p-4},
	     {0x1.38131639b4cdbp+0, -0x1.6054d40ded21p-4},
	     {0x1.36987540fbf53p+0, -0x1.57e17576bc9a2p-4},
	     {0x1.352166b648f61p+0, -0x1.4f7851798bb0bp-4},
	     {0x1.33adddb3eb575p+0, -0x1.47194f5690ae3p-4},
	     {0x1.323dcd99fc1d3p+0, -0x1.3ec456d58ec47p-4},
	     {0x1.30d129fefc7d2p+0, -0x1.36794ff3e5f55p-4},
	     {0x1.2f67e6b72fe7dp+0, -0x1.2e382315725e4p-4},
	     {0x1.2e01f7cf8b187p+0, -0x1.2600b8ed82e91p-4},
	     {0x1.2c9f518ddc86ep+0, -0x1.1dd2fa85efc12p-4},
	     {0x1.2b3fe86e5f413p+0, -0x1.15aed136e3961p-4},
	     {0x1.29e3b1211b25cp+0, -0x1.0d94269d1a30dp-4},
	     {0x1.288aa08b373cfp+0, -0x1.0582e4a7659f5p-4},
	     {0x1.2734abcaa8467p+0, -0x1.faf5eb655742dp-5},
	     {0x1.25e1c82459b81p+0, -0x1.eaf888487e8eep-5},
	     {0x1.2491eb1ad59c5p+0, -0x1.db0d75ef25a82p-5},
	     {0x1.23450a54048b5p+0, -0x1.cb348a49e6431p-5},
	     {0x1.21fb1bb09e578p+0, -0x1.bb6d9c69acdd8p-5},
	     {0x1.20b415346d8f7p+0, -0x1.abb88368aa7ap-5},
	     {0x1.1f6fed179a1acp+0, -0x1.9c1517476af14p-5},
	     {0x1.1e2e99b93c7b3p+0, -0x1.8c833051bfa4dp-5},
	     {0x1.1cf011a7a882ap+0, -0x1.7d02a78e7fb31p-5},
	     {0x1.1bb44b97dba5ap+0, -0x1.6d93565e97c5fp-5},
	     {0x1.1a7b3e66cdd4fp+0, -0x1.5e351695db0c5p-5},
	     {0x1.1944e11dc56cdp+0, -0x1.4ee7c2ba67adcp-5},
	     {0x1.18112aebb1a6ep+0, -0x1.3fab35ba16c01p-5},
	     {0x1.16e013231b7e9p+0, -0x1.307f4ad854bc9p-5},
	     {0x1.15b1913f156cfp+0, -0x1.2163ddf4f988cp-5},
	     {0x1.14859cdedde13p+0, -0x1.1258cb5d19e22p-5},
	     {0x1.135c2dc68cfa4p+0, -0x1.035defdba3188p-5},
	     {0x1.12353bdb01684p+0, -0x1.e8e651191bce4p-6},
	     {0x1.1110bf25b85b4p+0, -0x1.cb30a62be444cp-6},
	     {0x1.0feeafd2f8577p+0, -0x1.ad9a9b3043823p-6},
	     {0x1.0ecf062c51c3bp+0, -0x1.9023ecda1ccdep-6},
	     {0x1.0db1baa076c8bp+0, -0x1.72cc592bd82dp-6},
	     {0x1.0c96c5bb3048ep+0, -0x1.55939eb1f9c6ep-6},
	     {0x1.0b7e20263e070p+0, -0x1.38797ca6cc5ap-6},
	     {0x1.0a67c2acd0ce3p+0, -0x1.1b7db35c2c072p-6},
	     {0x1.0953a6391e982p+0, -0x1.fd400812ee9a2p-7},
	     {0x1.0841c3caea380p+0, -0x1.c3c05fb4620f1p-7},
	     {0x1.07321489b13eap+0, -0x1.8a7bf3c40e2e3p-7},
	     {0x1.062491aee9904p+0, -0x1.517249c15a75cp-7},
	     {0x1.05193497a7cc5p+0, -0x1.18a2ea5330c91p-7},
	     {0x1.040ff6b5f5e9fp+0, -0x1.c01abc8cdc4e2p-8},
	     {0x1.0308d19aa6127p+0, -0x1.4f6261750dec9p-8},
	     {0x1.0203beedb0c67p+0, -0x1.be37b6612afa7p-9},
	     {0x1.010037d38bcc2p+0, -0x1.bc3a8398ac26p-10},
	     {1.0, 0.0},
	     {0x1.fc06d493cca10p-1, 0x1.bb796219f30a5p-9},
	     {0x1.f81e6ac3b918fp-1, 0x1.b984fdcba61cep-8},
	     {0x1.f44546ef18996p-1, 0x1.49cf12adf8e8cp-7},
	     {0x1.f07b10382c84bp-1, 0x1.b6075b5217083p-7},
	     {0x1.ecbf7070e59d4p-1, 0x1.10b7466fc30ddp-6},
	     {0x1.e91213f715939p-1, 0x1.4603e4db6a3a1p-6},
	     {0x1.e572a9a75f7b7p-1, 0x1.7aeb10e99e105p-6},
	     {0x1.e1e0e2c530207p-1, 0x1.af6e49b0f0e36p-6},
	     {0x1.de5c72d8a8be3p-1, 0x1.e38f064f41179p-6},
	     {0x1.dae50fa5658ccp-1, 0x1.0ba75abbb7623p-5},
	     {0x1.d77a71145a2dap-1, 0x1.25575ee2dba86p-5},
	     {0x1.d41c51166623ep-1, 0x1.3ed83f477f946p-5},
	     {0x1.d0ca6ba0bb29fp-1, 0x1.582aa79af60efp-5},
	     {0x1.cd847e8e59681p-1, 0x1.714f400fa83aep-5},
	     {0x1.ca4a499693e00p-1, 0x1.8a46ad3901cb9p-5},
	     {0x1.c71b8e399e821p-1, 0x1.a311903b6b87p-5},
	     {0x1.c3f80faf19077p-1, 0x1.bbb086f216911p-5},
	     {0x1.c0df92dc2b0ecp-1, 0x1.d4242bdda648ep-5},
	     {0x1.bdd1de3cbb542p-1, 0x1.ec6d167c2af1p-5},
	     {0x1.baceb9e1007a3p-1, 0x1.0245ed8221426p-4},
	     {0x1.b7d5ef543e55ep-1, 0x1.0e40856c74f64p-4},
	     {0x1.b4e749977d953p-1, 0x1.1a269a31120fep-4},
	     {0x1.b20295155478ep-1, 0x1.25f8718fc076cp-4},
	     {0x1.af279f8e82be2p-1, 0x1.31b64ffc95bfp-4},
	     {0x1.ac5638197fdf3p-1, 0x1.3d60787ca5063p-4},
	     {0x1.a98e2f102e087p-1, 0x1.48f72ccd187fdp-4},
	     {0x1.a6cf5606d05c1p-1, 0x1.547aad6602f1cp-4},
	     {0x1.a4197fc04d746p-1, 0x1.5feb3989d3acbp-4},
	     {0x1.a16c80293dc01p-1, 0x1.6b490f3978c79p-4},
	     {0x1.9ec82c4dc5bc9p-1, 0x1.76946b3f5e703p-4},
	     {0x1.9c2c5a491f534p-1, 0x1.81cd895717c83p-4},
	     {0x1.9998e1480b618p-1, 0x1.8cf4a4055c30ep-4},
	     {0x1.970d9977c6c2dp-1, 0x1.9809f4c48c0ebp-4},
	     {0x1.948a5c023d212p-1, 0x1.a30db3f9899efp-4},
	     {0x1.920f0303d6809p-1, 0x1.ae001905458fcp-4},
	     {0x1.8f9b698a98b45p-1, 0x1.b8e15a2e3a2cdp-4},
	     {0x1.8d2f6b81726f6p-1, 0x1.c3b1ace2b0996p-4},
	     {0x1.8acae5bb55badp-1, 0x1.ce71456edfa62p-4},
	     {0x1.886db5d9275b8p-1, 0x1.d9205759882c4p-4},
	     {0x1.8617ba567c13cp-1, 0x1.e3bf1513af0dfp-4},
	     {0x1.83c8d27487800p-1, 0x1.ee4db0412c414p-4},
	     {0x1.8180de3c5dbe7p-1, 0x1.f8cc5998de3a5p-4},
	     {0x1.7f3fbe71cdb71p-1, 0x1.019da085eaeb1p-3},
	     {0x1.7d055498071c1p-1, 0x1.06cd4acdb4e3dp-3},
	     {0x1.7ad182e54f65ap-1, 0x1.0bf542bef813fp-3},
	     {0x1.78a42c3c90125p-1, 0x1.11159f14da262p-3},
	     {0x1.767d342f76944p-1, 0x1.162e761c10d1cp-3},
	     {0x1.745c7ef26b00ap-1, 0x1.1b3fddc60d43ep-3},
	     {0x1.7241f15769d0fp-1, 0x1.2049ebac86aa6p-3},
	     {0x1.702d70d396e41p-1, 0x1.254cb4fb7836ap-3},
	     {0x1.6e1ee3700cd11p-1, 0x1.2a484e8d0d252p-3},
	     {0x1.6c162fc9cbe02p-1, 0x1.2f3ccce1c860bp-3}},

     /* Computed from log coeffs div by log(10) then rounded to double
	precision.  */
     .poly
     = {-0x1.bcb7b1526e506p-3, 0x1.287a7636be1d1p-3, -0x1.bcb7b158af938p-4,
	0x1.63c78734e6d07p-4, -0x1.287461742fee4p-4},

     .invln10 = 0x1.bcb7b1526e50ep-2,
     .log10_2 = 0x1.34413509f79ffp-2

};
