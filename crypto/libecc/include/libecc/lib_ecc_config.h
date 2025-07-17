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
#ifndef __LIB_ECC_CONFIG_H__
#define __LIB_ECC_CONFIG_H__

/*
 * This configuration file provides various knobs to configure
 * what will be built in the library (supported curves, hash
 * algorithms and signature/verification schemes).
 */

/* It is possible to override the LIBECC configuration by defining
 * the WITH_LIBECC_CONFIG_OVERRIDE preprocessing flag in the CFLAGS. When
 * this is done, it is expected that the user defines the curves,
 * hash algorithms and signature schemes in the compilation
 * command line (e.g. via the CFLAGS).
 * For instance, in order to only use FRP256V1, SHA-256 and ECDSA, add to the CFLAGS:
 *
 *   -DWITH_LIBECC_CONFIG_OVERRIDE -DWITH_CURVE_FRP256V1 -DWITH_HASH_SHA256 -DWITH_SIG_ECDSA
 *
 */
#ifndef WITH_LIBECC_CONFIG_OVERRIDE

/* Supported curves */
#define WITH_CURVE_FRP256V1
#define WITH_CURVE_SECP192R1
#define WITH_CURVE_SECP224R1
#define WITH_CURVE_SECP256R1
#define WITH_CURVE_SECP384R1
#define WITH_CURVE_SECP521R1
#define WITH_CURVE_BRAINPOOLP192R1
#define WITH_CURVE_BRAINPOOLP224R1
#define WITH_CURVE_BRAINPOOLP256R1
#define WITH_CURVE_BRAINPOOLP384R1
#define WITH_CURVE_BRAINPOOLP512R1
#define WITH_CURVE_GOST256
#define WITH_CURVE_GOST512
#define WITH_CURVE_SM2P256TEST
#define WITH_CURVE_SM2P256V1
#define WITH_CURVE_WEI25519
#define WITH_CURVE_WEI448
#define WITH_CURVE_GOST_R3410_2012_256_PARAMSETA
#define WITH_CURVE_SECP256K1
#define WITH_CURVE_GOST_R3410_2001_TESTPARAMSET
#define WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET
#define WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET
#define WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET
#define WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_XCHA_PARAMSET
#define WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_XCHB_PARAMSET
#define WITH_CURVE_GOST_R3410_2012_256_PARAMSETB
#define WITH_CURVE_GOST_R3410_2012_256_PARAMSETC
#define WITH_CURVE_GOST_R3410_2012_256_PARAMSETD
#define WITH_CURVE_GOST_R3410_2012_512_PARAMSETTEST
#define WITH_CURVE_GOST_R3410_2012_512_PARAMSETA
#define WITH_CURVE_GOST_R3410_2012_512_PARAMSETB
#define WITH_CURVE_GOST_R3410_2012_512_PARAMSETC
#define WITH_CURVE_SECP192K1
#define WITH_CURVE_SECP224K1
#define WITH_CURVE_BRAINPOOLP192T1
#define WITH_CURVE_BRAINPOOLP224T1
#define WITH_CURVE_BRAINPOOLP256T1
#define WITH_CURVE_BRAINPOOLP320R1
#define WITH_CURVE_BRAINPOOLP320T1
#define WITH_CURVE_BRAINPOOLP384T1
#define WITH_CURVE_BRAINPOOLP512T1
#define WITH_CURVE_BIGN256V1
#define WITH_CURVE_BIGN384V1
#define WITH_CURVE_BIGN512V1
/* ADD curves define here */
/* XXX: Do not remove the comment above, as it is
 * used by external tools as a placeholder to add or
 * remove automatically generated code.
 */

/* Supported hash algorithms */
#define WITH_HASH_SHA224
#define WITH_HASH_SHA256
#define WITH_HASH_SHA384
#define WITH_HASH_SHA512
#define WITH_HASH_SHA512_224
#define WITH_HASH_SHA512_256
#define WITH_HASH_SHA3_224
#define WITH_HASH_SHA3_256
#define WITH_HASH_SHA3_384
#define WITH_HASH_SHA3_512
#define WITH_HASH_SM3
#define WITH_HASH_SHAKE256
#define WITH_HASH_STREEBOG256
#define WITH_HASH_STREEBOG512
#define WITH_HASH_RIPEMD160
#define WITH_HASH_BELT_HASH
#define WITH_HASH_BASH224
#define WITH_HASH_BASH256
#define WITH_HASH_BASH384
#define WITH_HASH_BASH512
#define WITH_HMAC

/* Supported sig/verif schemes */
#define WITH_SIG_ECDSA
#define WITH_SIG_ECKCDSA
#define WITH_SIG_ECSDSA
#define WITH_SIG_ECOSDSA
#define WITH_SIG_ECFSDSA
#define WITH_SIG_ECGDSA
#define WITH_SIG_ECRDSA
#define WITH_SIG_SM2
#define WITH_SIG_EDDSA25519
#define WITH_SIG_EDDSA448
#define WITH_SIG_DECDSA
#define WITH_SIG_BIGN
#define WITH_SIG_DBIGN
#define WITH_SIG_BIP0340

/* Supported ECDH schemes */
#define WITH_ECCCDH
#define WITH_X25519
#define WITH_X448

#endif /* WITH_LIBECC_CONFIG_OVERRIDE */

#endif /* __LIB_ECC_CONFIG_H__ */
