/*
 * Polynomial coefficients and other constants for tgamma128.c.
 *
 * Copyright (c) 2006-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/* The largest positive value for which 128-bit tgamma does not overflow. */
static const long double max_x =  0x1.b6e3180cd66a5c4206f128ba77f4p+10L;

/* Coefficients of the polynomial used in the tgamma_large() subroutine */
static const long double coeffs_large[] = {
     0x1.8535745aa79569579b9eec0f3bbcp+0L,
     0x1.0378f83c6fb8f0e51269f2b4a973p-3L,
     0x1.59f6a05094f69686c3380f4e2783p-8L,
    -0x1.0b291dee952a82764a4859b081a6p-8L,
    -0x1.6dd301b2205bf936b5a3eaad0dbbp-12L,
     0x1.387a8b5f38dd77e7f139b1021e86p-10L,
     0x1.bca46637f65b13750c728cc29e40p-14L,
    -0x1.d80401c00aef998c9e303151a51cp-11L,
    -0x1.49cb6bb09f935a2053ccc2cf3711p-14L,
     0x1.4e950204437dcaf2be77f73a6f45p-10L,
     0x1.cb711a2d65f188bf60110934d6bep-14L,
    -0x1.7d7ff4bc95dc7faefc5e767f70f1p-9L,
    -0x1.0305ab9760cddb0d833e73766836p-12L,
     0x1.3ef6c84bf1cd5c3f65ac2693bb5bp-7L,
     0x1.bb4144740ad9290123fdcea684aap-11L,
    -0x1.72ab4e88272a229bfafd192450f0p-5L,
     0x1.80c70ac6eb3b7a698983d25a62b8p-12L,
     0x1.e222791c6743ce3e3cae220fb236p-3L,
     0x1.1a2dca1c82a9326c52b465f7cb7ap-2L,
    -0x1.9d204fa235a42cd901b123d2ad47p+1L,
     0x1.55b56d1158f77ddb1c95fc44ab02p+0L,
     0x1.37f900a11dbd892abd7dde533e2dp+5L,
    -0x1.2da49f4188dd89cb958369ef2401p+7L,
     0x1.fdae5ec3ec6eb7dffc09edbe6c14p+7L,
    -0x1.61433cebe649098c9611c4c7774ap+7L,
};

/* Coefficients of the polynomial used in the tgamma_tiny() subroutine */
static const long double coeffs_tiny[] = {
     0x1.0000000000000000000000000000p+0L,
     0x1.2788cfc6fb618f49a37c7f0201fep-1L,
    -0x1.4fcf4026afa2dceb8490ade22796p-1L,
    -0x1.5815e8fa27047c8f42b5d9217244p-5L,
     0x1.5512320b43fbe5dfa771333518f7p-3L,
    -0x1.59af103c340927bffdd44f954bfcp-5L,
    -0x1.3b4af28483e210479657e5543366p-7L,
     0x1.d919c527f6070bfce9b29c2ace9cp-8L,
    -0x1.317112ce35337def3556a18aa178p-10L,
    -0x1.c364fe77a6f27677b985b1fa2e1dp-13L,
     0x1.0c8a7a19a3fd40fe1f7e867efe7bp-13L,
    -0x1.51cf9f090b5dc398ba86305e3634p-16L,
    -0x1.4e80f64c04a339740de06ca9fa4ap-20L,
     0x1.241ddc2aef2ec20e58b08f2fda17p-20L,
};

/* The location within the interval [1,2] where gamma has a minimum.
 * Specified as the sum of two 128-bit values, for extra precision. */
static const long double min_x_hi =  0x1.762d86356be3f6e1a9c8865e0a4fp+0L;
static const long double min_x_lo =  0x1.ac54d7d218de21303a7c60f08840p-118L;

/* The actual minimum value that gamma takes at that location.
 * Again specified as the sum of two 128-bit values. */
static const long double min_y_hi =  0x1.c56dc82a74aee8d8851566d40f32p-1L;
static const long double min_y_lo =  0x1.8ed98685742c353ce55e5794686fp-114L;

/* Coefficients of the polynomial used in the tgamma_central() subroutine
 * for computing gamma on the interval [1,min_x] */
static const long double coeffs_central_neg[] = {
     0x1.b6c53f7377b83839c8a292e43b69p-2L,
     0x1.0bae9f40c7d09ed76e732045850ap-3L,
     0x1.4981175e14d04c3530e51d01c5fep-3L,
     0x1.79f77aaf032c948af3a9edbd2061p-4L,
     0x1.1e97bd10821095a5b79fbfdfa1a3p-4L,
     0x1.8071ce0935e4dcf0b33b0fbec7c1p-5L,
     0x1.0b44c2f92982f887b55ec36dfdb0p-5L,
     0x1.6df1de1e178ef72ca7bd63d40870p-6L,
     0x1.f63f502bde27e81c0f5e13479b43p-7L,
     0x1.57fd67d901f40ea011353ad89a0ap-7L,
     0x1.d7151376eed187eb753e2273cafcp-8L,
     0x1.427162b5c6ff1d904c71ef53e37cp-8L,
     0x1.b954b8c3a56cf93e49ef6538928ap-9L,
     0x1.2dff2ec26a3ae5cd3aaccae7a09ep-9L,
     0x1.9d35250d9b9378d9b59df734537ap-10L,
     0x1.1b2c0c48b9855a28f6dbd6fdff3cp-10L,
     0x1.7e0db39bb99cdb52b028d9359380p-11L,
     0x1.2164b5e1d364a0b5eaf97c436aa7p-11L,
     0x1.27521cf5fd24dcdf43524e6add11p-13L,
     0x1.06461d62243bf9a826b42349672fp-10L,
    -0x1.2b852abead28209b4e0c756dc46ep-9L,
     0x1.be673c11a72c826115ec6d286c14p-8L,
    -0x1.fd9ce330c215c31fcd3cb53c42ebp-7L,
     0x1.fa362bd2dc68f41abef2d8600acdp-6L,
    -0x1.a21585b2f52f8b23855de8e452edp-5L,
     0x1.1f234431ed032052fc92e64e0493p-4L,
    -0x1.40d332476ca0199c60cdae3f9132p-4L,
     0x1.1d45dc665d86012eba2eea199cefp-4L,
    -0x1.8491016cdd08dc9be7ade9b5fef3p-5L,
     0x1.7e7e2fbc6d49ad484300d6add324p-6L,
    -0x1.e63fe3f874a37276a8d7d8b705ecp-8L,
     0x1.30a2a73944f8c84998314d69c23fp-10L,
};

/* Coefficients of the polynomial used in the tgamma_central() subroutine
 * for computing gamma on the interval [min_x,2] */
static const long double coeffs_central_pos[] = {
     0x1.b6c53f7377b83839c8a292e22aa2p-2L,
    -0x1.0bae9f40c7d09ed76e72e1c955dep-3L,
     0x1.4981175e14d04c3530ee5e1ecebcp-3L,
    -0x1.79f77aaf032c948ac983d77f3e07p-4L,
     0x1.1e97bd10821095ab7dc94936cc11p-4L,
    -0x1.8071ce0935e4d7edef8cbf2a1cf1p-5L,
     0x1.0b44c2f929837fafef7b5d9e80f1p-5L,
    -0x1.6df1de1e175fe2a51faa25cddbb4p-6L,
     0x1.f63f502be57d11aed2cfe90843ffp-7L,
    -0x1.57fd67d852f230015b9f64770273p-7L,
     0x1.d715138adc07e5fce81077070357p-8L,
    -0x1.4271618e9fda8992a667adb15f4fp-8L,
     0x1.b954d15d9eb772e80fdd760672d7p-9L,
    -0x1.2dfe391241d3cb79c8c15182843dp-9L,
     0x1.9d44396fcd48451c3ba924cee814p-10L,
    -0x1.1ac195fb99739e341589e39803e6p-10L,
     0x1.82e46127b68f002770826e25f146p-11L,
    -0x1.089dacd90d9f41493119ac178359p-11L,
     0x1.6993c007b20394a057d21f3d37f8p-12L,
    -0x1.ec43a709f4446560c099dec8e31bp-13L,
     0x1.4ba36322f4074e9add9450f003cap-13L,
    -0x1.b3f83a977965ca1b7937bf5b34cap-14L,
     0x1.10af346abc09cb25a6d9fe810b6ep-14L,
    -0x1.38d8ea1188f242f50203edc395bdp-15L,
     0x1.39add987a948ec56f62b721a4475p-16L,
    -0x1.02a4e141f286c8a967e2df9bc9adp-17L,
     0x1.433b50af22425f546e87113062d7p-19L,
    -0x1.0c7b73cb0013f00aafc103e8e382p-21L,
     0x1.b852de313ec38da2297f6deaa6b4p-25L,
};

/* 128-bit float value of pi, used by the sin_pi_x_over_pi subroutine
 */
static const long double pi =  0x1.921fb54442d18469898cc51701b8p+1L;
