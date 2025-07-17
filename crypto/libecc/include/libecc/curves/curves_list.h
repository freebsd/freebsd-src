/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#ifndef __CURVES_LIST_H__
#define __CURVES_LIST_H__

#include <libecc/lib_ecc_config.h>
#include <libecc/lib_ecc_types.h>
#include <libecc/words/words.h>
#include <libecc/curves/known/ec_params_brainpoolp192r1.h>
#include <libecc/curves/known/ec_params_brainpoolp224r1.h>
#include <libecc/curves/known/ec_params_brainpoolp256r1.h>
#include <libecc/curves/known/ec_params_brainpoolp384r1.h>
#include <libecc/curves/known/ec_params_brainpoolp512r1.h>
#include <libecc/curves/known/ec_params_secp192r1.h>
#include <libecc/curves/known/ec_params_secp224r1.h>
#include <libecc/curves/known/ec_params_secp256r1.h>
#include <libecc/curves/known/ec_params_secp384r1.h>
#include <libecc/curves/known/ec_params_secp521r1.h>
#include <libecc/curves/known/ec_params_frp256v1.h>
#include <libecc/curves/known/ec_params_gost256.h>
#include <libecc/curves/known/ec_params_gost512.h>
#include <libecc/curves/known/ec_params_sm2p192test.h>
#include <libecc/curves/known/ec_params_sm2p256test.h>
#include <libecc/curves/known/ec_params_sm2p256v1.h>
#include <libecc/curves/known/ec_params_wei25519.h>
#include <libecc/curves/known/ec_params_wei448.h>
#include <libecc/curves/known/ec_params_gost_R3410_2012_256_paramSetA.h>
#include <libecc/curves/known/ec_params_secp256k1.h>
#include <libecc/curves/known/ec_params_gost_R3410_2001_TestParamSet.h>
#include <libecc/curves/known/ec_params_gost_R3410_2001_CryptoPro_A_ParamSet.h>
#include <libecc/curves/known/ec_params_gost_R3410_2001_CryptoPro_B_ParamSet.h>
#include <libecc/curves/known/ec_params_gost_R3410_2001_CryptoPro_C_ParamSet.h>
#include <libecc/curves/known/ec_params_gost_R3410_2001_CryptoPro_XchA_ParamSet.h>
#include <libecc/curves/known/ec_params_gost_R3410_2001_CryptoPro_XchB_ParamSet.h>
#include <libecc/curves/known/ec_params_gost_R3410_2012_256_paramSetA.h>
#include <libecc/curves/known/ec_params_gost_R3410_2012_256_paramSetB.h>
#include <libecc/curves/known/ec_params_gost_R3410_2012_256_paramSetC.h>
#include <libecc/curves/known/ec_params_gost_R3410_2012_256_paramSetD.h>
#include <libecc/curves/known/ec_params_gost_R3410_2012_512_paramSetTest.h>
#include <libecc/curves/known/ec_params_gost_R3410_2012_512_paramSetA.h>
#include <libecc/curves/known/ec_params_gost_R3410_2012_512_paramSetB.h>
#include <libecc/curves/known/ec_params_gost_R3410_2012_512_paramSetC.h>
#include <libecc/curves/known/ec_params_secp192k1.h>
#include <libecc/curves/known/ec_params_secp224k1.h>
#include <libecc/curves/known/ec_params_brainpoolp192t1.h>
#include <libecc/curves/known/ec_params_brainpoolp224t1.h>
#include <libecc/curves/known/ec_params_brainpoolp256t1.h>
#include <libecc/curves/known/ec_params_brainpoolp320r1.h>
#include <libecc/curves/known/ec_params_brainpoolp320t1.h>
#include <libecc/curves/known/ec_params_brainpoolp384t1.h>
#include <libecc/curves/known/ec_params_brainpoolp512t1.h>
#include <libecc/curves/known/ec_params_bign256v1.h>
#include <libecc/curves/known/ec_params_bign384v1.h>
#include <libecc/curves/known/ec_params_bign512v1.h>
/* ADD curves header here */
/* XXX: Do not remove the comment above, as it is
 * used by external tools as a placeholder to add or
 * remove automatically generated code.
 */

#ifndef CURVES_MAX_P_BIT_LEN
#error "Max p bit length is 0; did you disable all curves in lib_ecc_config.h?"
#endif
#if (CURVES_MAX_P_BIT_LEN > 65535)
#error "Prime field length (in bytes) MUST fit on an u16!"
#endif

#ifndef CURVES_MAX_Q_BIT_LEN
#error "Max q bit length is 0; did you disable all curves in lib_ecc_config.h?"
#endif
#if (CURVES_MAX_Q_BIT_LEN > 65535)
#error "Generator order length (in bytes) MUST fit on an u16!"
#endif

#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#error "Max curve order bit length is 0; did you disable all curves in lib_ecc_config.h?"
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN > 65535)
#error "Curve order length (in bytes) MUST fit on an u16!"
#endif

typedef struct {
	ec_curve_type type;
	const ec_str_params *params;
} ec_mapping;

static const ec_mapping ec_maps[] = {
#ifdef WITH_CURVE_FRP256V1
	{.type = FRP256V1,.params = &frp256v1_str_params},
#endif /* WITH_CURVE_FRP256V1 */
#ifdef WITH_CURVE_SECP192R1
	{.type = SECP192R1,.params = &secp192r1_str_params},
#endif /* WITH_CURVE_SECP192R1 */
#ifdef WITH_CURVE_SECP224R1
	{.type = SECP224R1,.params = &secp224r1_str_params},
#endif /* WITH_CURVE_SECP224R1 */
#ifdef WITH_CURVE_SECP256R1
	{.type = SECP256R1,.params = &secp256r1_str_params},
#endif /* WITH_CURVE_SECP256R1 */
#ifdef WITH_CURVE_SECP384R1
	{.type = SECP384R1,.params = &secp384r1_str_params},
#endif /* WITH_CURVE_SECP384R1 */
#ifdef WITH_CURVE_SECP521R1
	{.type = SECP521R1,.params = &secp521r1_str_params},
#endif /* WITH_CURVE_SECP521R1 */
#ifdef WITH_CURVE_BRAINPOOLP192R1
	{.type = BRAINPOOLP192R1,.params = &brainpoolp192r1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP192R1 */
#ifdef WITH_CURVE_BRAINPOOLP224R1
	{.type = BRAINPOOLP224R1,.params = &brainpoolp224r1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP224R1 */
#ifdef WITH_CURVE_BRAINPOOLP256R1
	{.type = BRAINPOOLP256R1,.params = &brainpoolp256r1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP256R1 */
#ifdef WITH_CURVE_BRAINPOOLP384R1
	{.type = BRAINPOOLP384R1,.params = &brainpoolp384r1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP384R1 */
#ifdef WITH_CURVE_BRAINPOOLP512R1
	{.type = BRAINPOOLP512R1,.params = &brainpoolp512r1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP512R1 */
#ifdef WITH_CURVE_GOST256
	{.type = GOST256,.params = &GOST_256bits_curve_str_params},
#endif /* WITH_CURVE_GOST256 */
#ifdef WITH_CURVE_GOST512
	{.type = GOST512,.params = &GOST_512bits_curve_str_params},
#endif /* WITH_CURVE_GOST512 */
#ifdef WITH_CURVE_SM2P256TEST
	{.type = SM2P256TEST,.params = &sm2p256test_str_params},
#endif /* WITH_CURVE_SM2P256TEST */
#ifdef WITH_CURVE_SM2P256V1
	{.type = SM2P256V1,.params = &sm2p256v1_str_params},
#endif /* WITH_CURVE_SM2P256V1 */
#ifdef WITH_CURVE_WEI25519
	{.type = WEI25519,.params = &wei25519_str_params},
#endif /* WITH_CURVE_WEI25519 */
#ifdef WITH_CURVE_WEI448
	{.type = WEI448,.params = &wei448_str_params},
#endif /* WITH_CURVE_WEI448 */
#ifdef WITH_CURVE_GOST_R3410_2012_256_PARAMSETA
	{ .type = GOST_R3410_2012_256_PARAMSETA, .params = &gost_R3410_2012_256_paramSetA_str_params },
#endif /* WITH_CURVE_GOST_R3410_2012_256_PARAMSETA */
#ifdef WITH_CURVE_SECP256K1
	{.type = SECP256K1,.params = &secp256k1_str_params},
#endif /* WITH_CURVE_SECP256K1 */
#ifdef WITH_CURVE_GOST_R3410_2001_TESTPARAMSET
	{ .type = GOST_R3410_2001_TESTPARAMSET, .params = &gost_R3410_2001_TestParamSet_str_params },
#endif /* WITH_CURVE_GOST_R3410_2001_TESTPARAMSET */
#ifdef WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET
	{ .type = GOST_R3410_2001_CRYPTOPRO_A_PARAMSET, .params = &gost_R3410_2001_CryptoPro_A_ParamSet_str_params },
#endif /* WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET */
#ifdef WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET
	{ .type = GOST_R3410_2001_CRYPTOPRO_B_PARAMSET, .params = &gost_R3410_2001_CryptoPro_B_ParamSet_str_params },
#endif /* WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET */
#ifdef WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET
	{ .type = GOST_R3410_2001_CRYPTOPRO_C_PARAMSET, .params = &gost_R3410_2001_CryptoPro_C_ParamSet_str_params },
#endif /* WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET */
#ifdef WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_XCHA_PARAMSET
	{ .type = GOST_R3410_2001_CRYPTOPRO_XCHA_PARAMSET, .params = &gost_R3410_2001_CryptoPro_XchA_ParamSet_str_params },
#endif /* WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_XCHA_PARAMSET */
#ifdef WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_XCHB_PARAMSET
	{ .type = GOST_R3410_2001_CRYPTOPRO_XCHB_PARAMSET, .params = &gost_R3410_2001_CryptoPro_XchB_ParamSet_str_params },
#endif /* WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_XCHB_PARAMSET */
#ifdef WITH_CURVE_GOST_R3410_2012_256_PARAMSETA
	{ .type = GOST_R3410_2012_256_PARAMSETA, .params = &gost_R3410_2012_256_paramSetA_str_params },
#endif /* WITH_CURVE_GOST_R3410_2012_256_PARAMSETA */
#ifdef WITH_CURVE_GOST_R3410_2012_256_PARAMSETB
	{ .type = GOST_R3410_2012_256_PARAMSETB, .params = &gost_R3410_2012_256_paramSetB_str_params },
#endif /* WITH_CURVE_GOST_R3410_2012_256_PARAMSETB */
#ifdef WITH_CURVE_GOST_R3410_2012_256_PARAMSETC
	{ .type = GOST_R3410_2012_256_PARAMSETC, .params = &gost_R3410_2012_256_paramSetC_str_params },
#endif /* WITH_CURVE_GOST_R3410_2012_256_PARAMSETC */
#ifdef WITH_CURVE_GOST_R3410_2012_256_PARAMSETD
	{ .type = GOST_R3410_2012_256_PARAMSETD, .params = &gost_R3410_2012_256_paramSetD_str_params },
#endif /* WITH_CURVE_GOST_R3410_2012_256_PARAMSETD */
#ifdef WITH_CURVE_GOST_R3410_2012_512_PARAMSETTEST
	{ .type = GOST_R3410_2012_512_PARAMSETTEST, .params = &gost_R3410_2012_512_paramSetTest_str_params },
#endif /* WITH_CURVE_GOST_R3410_2012_512_PARAMSETTEST */
#ifdef WITH_CURVE_GOST_R3410_2012_512_PARAMSETA
	{ .type = GOST_R3410_2012_512_PARAMSETA, .params = &gost_R3410_2012_512_paramSetA_str_params },
#endif /* WITH_CURVE_GOST_R3410_2012_512_PARAMSETA */
#ifdef WITH_CURVE_GOST_R3410_2012_512_PARAMSETB
	{ .type = GOST_R3410_2012_512_PARAMSETB, .params = &gost_R3410_2012_512_paramSetB_str_params },
#endif /* WITH_CURVE_GOST_R3410_2012_512_PARAMSETB */
#ifdef WITH_CURVE_GOST_R3410_2012_512_PARAMSETC
	{ .type = GOST_R3410_2012_512_PARAMSETC, .params = &gost_R3410_2012_512_paramSetC_str_params },
#endif /* WITH_CURVE_GOST_R3410_2012_512_PARAMSETC */
#ifdef WITH_CURVE_SECP192K1
	{.type = SECP192K1,.params = &secp192k1_str_params},
#endif /* WITH_CURVE_SECP192K1 */
#ifdef WITH_CURVE_SECP224K1
	{.type = SECP224K1,.params = &secp224k1_str_params},
#endif /* WITH_CURVE_SECP224K1 */
#ifdef WITH_CURVE_BRAINPOOLP192T1
	{.type = BRAINPOOLP192T1,.params = &brainpoolp192t1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP192T1 */
#ifdef WITH_CURVE_BRAINPOOLP224T1
	{.type = BRAINPOOLP224T1,.params = &brainpoolp224t1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP224T1 */
#ifdef WITH_CURVE_BRAINPOOLP256T1
	{.type = BRAINPOOLP256T1,.params = &brainpoolp256t1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP256T1 */
#ifdef WITH_CURVE_BRAINPOOLP320R1
	{.type = BRAINPOOLP320R1,.params = &brainpoolp320r1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP320R1 */
#ifdef WITH_CURVE_BRAINPOOLP320T1
	{.type = BRAINPOOLP320T1,.params = &brainpoolp320t1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP320T1 */
#ifdef WITH_CURVE_BRAINPOOLP384T1
	{.type = BRAINPOOLP384T1,.params = &brainpoolp384t1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP192T1 */
#ifdef WITH_CURVE_BRAINPOOLP512T1
	{.type = BRAINPOOLP512T1,.params = &brainpoolp512t1_str_params},
#endif /* WITH_CURVE_BRAINPOOLP512T1 */
#ifdef WITH_CURVE_BIGN256V1
	{.type = BIGN256V1,.params = &bign256v1_str_params},
#endif /* WITH_CURVE_BIGN256V1 */
#ifdef WITH_CURVE_BIGN384V1
	{.type = BIGN384V1,.params = &bign384v1_str_params},
#endif /* WITH_CURVE_BIGN384V1 */
#ifdef WITH_CURVE_BIGN512V1
	{.type = BIGN512V1,.params = &bign512v1_str_params},
#endif /* WITH_CURVE_BIGN512V1 */
/* ADD curves mapping here */
/* XXX: Do not remove the comment above, as it is
 * used by external tools as a placeholder to add or
 * remove automatically generated code.
 */
};

/*
 * Number of cuvres supported by the lib, i.e. number of elements in
 * ec_maps array above.
 */
#define EC_CURVES_NUM (sizeof(ec_maps) / sizeof(ec_mapping))
#endif /* __CURVES_LIST_H__ */
