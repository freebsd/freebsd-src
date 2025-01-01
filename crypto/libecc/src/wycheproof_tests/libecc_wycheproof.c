/*
 *  Copyright (C) 2021 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */

/*
 * Source code for handling tests imported from the wycheproof project:
 *     https://github.com/google/wycheproof
 *
 * As this project primarily targets java cryptographic libraries, the
 * json test files have been parsed to generate libecc friendly test cases.
 *
 * NOTE: we skip here all the tests related to ASN.1 format errors as libecc
 * does not handle ASN.1 parsing at all. This explains the "skipped" tests from
 * the wycheproof project.
 *
 */
#include "libecc_wycheproof.h"

/* Parallelize self tests? */
#ifdef WITH_OPENMP_SELF_TESTS
/* No openmp without stdlib ... */
#ifndef WITH_STDLIB
#error "Sorry: no possible self tests parallelization (OpenMP) without stdlib! Please use WITH_STDLIB"
#endif
#include <omp.h>
#include <stdlib.h>
static omp_lock_t global_lock;
static volatile u8 global_lock_initialized = 0;
#define OPENMP_LOCK() do {                                       	\
        if(!global_lock_initialized){                             	\
                omp_init_lock(&global_lock);                      	\
                global_lock_initialized = 1;                      	\
        }                                                         	\
        omp_set_lock(&global_lock);                               	\
} while(0)
#define OPENMP_EG(ret, err) do {				  				\
	if(ret){						  				\
		ext_printf("OpenMP abort following error ... %s:%d\n", __FILE__, __LINE__); 	\
		exit(-1);					 				\
	}							 				\
} while(0)
#define OPENMP_MUST_HAVE(cnd, ret, err) do {			 	\
	ret = !!(cnd);							\
	ret = -((~ret) & 1);						\
	OPENMP_EG(ret, err);					     	\
} while(0)
#define OPENMP_UNLOCK() do {                                     	\
        omp_unset_lock(&global_lock);                             	\
} while(0)
#else
#define OPENMP_LOCK()
#define OPENMP_UNLOCK()
#define OPENMP_EG(ret, err) do {				 	\
	EG(ret, err);					     		\
} while(0)
#define OPENMP_MUST_HAVE(cnd, ret, err) do {			 	\
	MUST_HAVE(cnd, ret, err);					\
} while(0)
#endif

#include "libecc_wycheproof_tests.h"

/* Check all ECDSA test vectors */
static unsigned int ecdsa_acceptable_invalid = 0;
static unsigned int ecdsa_acceptable_valid = 0;
static unsigned int ecdsa_all_performed = 0;
static int check_wycheproof_ecdsa(void)
{
#if defined(WITH_SIG_ECDSA)
	int ret;
	unsigned int i;

#ifdef WITH_OPENMP_SELF_TESTS
        #pragma omp parallel
        #pragma omp for schedule(static, 1) nowait
#endif
	for(i = 0; i < NUM_WYCHEPROOF_ECDSA_TESTS; i++){
		const wycheproof_ecdsa_test *t = wycheproof_ecdsa_all_tests[i];
		ec_pub_key pub_key;
		ec_params params;

		if (t == NULL){
			continue;
		}

		ecdsa_all_performed++;
		ret = local_memset(&pub_key, 0, sizeof(pub_key)); OPENMP_EG(ret, err);
		ret = local_memset(&params, 0, sizeof(params)); OPENMP_EG(ret, err);

		/* Import EC params from test case */
		ret = import_params(&params, t->curve);
		if (ret) {
			ext_printf("Error: ECDSA tests error importing params\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}

		/* Import the public key */
		ret = ec_pub_key_import_from_aff_buf(&pub_key, &params, t->pubkey, (u8)(t->pubkeylen), t->sig_alg);
		if (ret) {
			ext_printf("Error: ECDSA tests error importing public key\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}


		ret = ec_verify(t->sig, (u8)(t->siglen), &pub_key, t->msg, t->msglen, t->sig_alg, t->hash, NULL, 0);
		/* Valid result */
		if ((t->result == 1) && ret) {
			ext_printf("[-] Error when verifying ECDSA test %d / %s (verification NOK while must be valid)\n", i, t->name);
			ext_printf("    (comment = %s)\n", t->comment);
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Invalid result */
		if ((t->result == -1) && !ret) {
			ext_printf("[-] Error when verifying ECDSA test %d / %s (verification OK while must be invalid)\n", i, t->name);
			ext_printf("    (comment = %s)\n", t->comment);
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Acceptable result: only trigger an informational warning */
		if (t->result == 0) {
			if(ret){
				ecdsa_acceptable_valid++;
			}
			else{
				ecdsa_acceptable_invalid++;
			}
#ifdef VERBOSE_ACCEPTABLE
			ext_printf("\t[~] ECDSA test %d / %s (verification %d while acceptable)\n", i, t->name, ret);
			ext_printf("\t    (comment = %s)\n", t->comment);
#endif
		}
	}

	ret = 0;
#ifndef WITH_OPENMP_SELF_TESTS
err:
#endif
	return ret;
#else
	return 0;
#endif
}

/* Check all EDDSA test vectors */
static unsigned int eddsa_acceptable_invalid = 0;
static unsigned int eddsa_acceptable_valid = 0;
static unsigned int eddsa_all_performed = 0;
static int check_wycheproof_eddsa(void)
{
#if defined(WITH_SIG_EDDSA25519) || defined(WITH_SIG_EDDSA448)
	int ret;
	unsigned int i;

#ifdef WITH_OPENMP_SELF_TESTS
        #pragma omp parallel
        #pragma omp for schedule(static, 1) nowait
#endif
	for(i = 0; i < NUM_WYCHEPROOF_EDDSA_TESTS; i++){
		const wycheproof_eddsa_test *t = wycheproof_eddsa_all_tests[i];
		ec_pub_key pub_key;
		ec_pub_key pub_key_check;
		ec_priv_key priv_key;
		ec_params params;
		int check;
		u8 exported_pub_key[EDDSA_MAX_PUB_KEY_ENCODED_LEN];

		if (t == NULL){
			continue;
		}

		OPENMP_LOCK();
		eddsa_all_performed++;
		OPENMP_UNLOCK();
		ret = local_memset(&pub_key, 0, sizeof(pub_key)); OPENMP_EG(ret, err);
		ret = local_memset(&priv_key, 0, sizeof(priv_key)); OPENMP_EG(ret, err);
		ret = local_memset(&params, 0, sizeof(params)); OPENMP_EG(ret, err);

		/* Import EC params from test case */
		ret = import_params(&params, t->curve);
		if (ret) {
			ext_printf("Error: EDDSA tests error importing params\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}

		/* Import the public key */
		ret = eddsa_import_pub_key(&pub_key, t->pubkey, (u8)(t->pubkeylen), &params, t->sig_alg);
		if (ret) {
			ext_printf("Error: EDDSA tests error importing public key\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Import the private key */
		ret = eddsa_import_priv_key(&priv_key, t->privkey, (u8)(t->privkeylen), &params, t->sig_alg);
 		if (ret) {
			ext_printf("Error: EDDSA tests error importing private key\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Derive private to public */
		ret = eddsa_init_pub_key(&pub_key_check, &priv_key);
		if (ret) {
			ext_printf("Error: EDDSA tests error deriving private to public key\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Check */
		ret = eddsa_export_pub_key(&pub_key, exported_pub_key, (u8)(t->pubkeylen));
		if(ret){
			ext_printf("Error: EDDSA tests error when exporting public key\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* */
		ret = are_equal(t->pubkey, &exported_pub_key, (u8)(t->pubkeylen), &check); OPENMP_EG(ret, err);
		if(!check){
			ext_printf("Error: EDDSA tests error when checking public key from private\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}

		ret = ec_verify(t->sig, (u8)(t->siglen), &pub_key, t->msg, t->msglen, t->sig_alg, t->hash, NULL, 0);
		/* Valid result */
		if ((t->result == 1) && ret) {
			ext_printf("[-] Error when verifying EDDSA test %d / %s (verification NOK while must be valid)\n", i, t->name);
			ext_printf("    (comment = %s)\n", t->comment);
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Invalid result */
		if ((t->result == -1) && !ret) {
			ext_printf("[-] Error when verifying EDDSA test %d / %s (verification OK while must be invalid)\n", i, t->name);
			ext_printf("    (comment = %s)\n", t->comment);
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Acceptable result: only trigger an informational warning */
		if (t->result == 0) {
			OPENMP_LOCK();
			if(ret){
				eddsa_acceptable_valid++;
			}
			else{
				eddsa_acceptable_invalid++;
			}
#ifdef VERBOSE_ACCEPTABLE
			ext_printf("\t[~] EDDSA test %d / %s (verification %d while acceptable)\n", i, t->name, ret);
			ext_printf("\t    (comment = %s)\n", t->comment);
#endif
			OPENMP_UNLOCK();
		}
	}

	ret = 0;
#ifndef WITH_OPENMP_SELF_TESTS
err:
#endif
	return ret;
#else
	return 0;
#endif
}

/* Check all XDH test vectors */
static unsigned int xdh_acceptable_invalid = 0;
static unsigned int xdh_acceptable_valid = 0;
static unsigned int xdh_all_performed = 0;
static int check_wycheproof_xdh(void)
{
#if defined(WITH_X25519) || defined(WITH_X448)
	int ret;
	unsigned int i;

#ifdef WITH_OPENMP_SELF_TESTS
        #pragma omp parallel
        #pragma omp for schedule(static, 1) nowait
#endif
	for(i = 0; i < NUM_WYCHEPROOF_XDH_TESTS; i++){
		int check;
		const wycheproof_xdh_test *t = wycheproof_xdh_all_tests[i];
		unsigned int alglen = 0;
		/* Max size buffer */
		u8 pubkey_check[X448_SIZE];
		u8 sharedsecret_check[X448_SIZE];

		if (t == NULL){
			continue;
		}

		OPENMP_LOCK();
		xdh_all_performed++;
		OPENMP_UNLOCK();

#if defined(WITH_X25519)
		if(t->xdh_alg == X25519){
			OPENMP_MUST_HAVE(((t->curve) == &wei25519_str_params), ret, err);
			alglen = X25519_SIZE;
		}
#endif
#if defined(WITH_X448)
		if(t->xdh_alg == X448){
			OPENMP_MUST_HAVE(((t->curve) == &wei448_str_params), ret, err);
			alglen = X448_SIZE;
		}
#endif
		if(alglen == 0){
			ext_printf("Error: XDH tests error, unkown algorithm\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Reject bad lengths */
		if(t->privkeylen != alglen){
			if(t->result != -1){
				ext_printf("[-] Error: XDH tests error, unkown private key length %d with valid result\n", t->privkeylen);
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
			else{
				continue;
			}
		}
		if(t->peerpubkeylen != alglen){
			if(t->result != -1){
				ext_printf("[-] Error: XDH tests error, unkown peer public key length %d with valid result\n", t->peerpubkeylen);
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
			else{
				continue;
			}
		}
		if(t->sharedsecretlen != alglen){
			if(t->result != -1){
				ext_printf("[-] Error: XDH tests error, unkown shared secret length %d with valid result\n", t->sharedsecretlen);
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
			else{
				continue;
			}
		}
		if((t->ourpubkeylen != 0) && (t->ourpubkeylen != alglen)){
			if(t->result != -1){
				ext_printf("[-] Error: XDH tests error, unkown our public key length %d with valid result\n", t->ourpubkeylen);
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
			else{
				continue;
			}
		}
#if defined(WITH_X25519)
		if(t->xdh_alg == X25519){
			/* Derive our public key */
			ret = x25519_init_pub_key(t->privkey, pubkey_check);
			if(ret){
				ext_printf("[-] Error: XDH tests error when deriving private key to public\n");
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
			if(t->ourpubkeylen != 0){
				/* Check public key against the test one */
				ret = are_equal(t->ourpubkey, pubkey_check, alglen, &check); OPENMP_EG(ret, err);
				if(!check){
					ext_printf("[-] Error: XDH tests error when checking our public key\n");
					ext_printf("    (comment = %s)\n", t->comment);
					ret = -1;
					OPENMP_EG(ret, err);
				}
			}
			/* Derive the shared secret */
			ret = x25519_derive_secret(t->privkey, t->peerpubkey, sharedsecret_check);
			if(ret){
				/* Handle "acceptable" results here (e.g. public key on twist) */
				if(t->result == 0){
					OPENMP_LOCK();
					xdh_acceptable_invalid++;
#ifdef VERBOSE_ACCEPTABLE
					ext_printf("\t[~] XDH test %d / %s (shared secret derivation NOK while acceptable)\n", i, t->name);
					ext_printf("\t    (comment = %s)\n", t->comment);
#endif
					OPENMP_UNLOCK();
					continue;
				}
				ext_printf("[-] Error: XDH tests error when deriving shared secret\n");
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
			if(t->result == -1){
				ext_printf("[-] Error: XDH tests is OK while invalid\n");
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
			/* Check the shared secret */
			ret = are_equal(t->sharedsecret, sharedsecret_check, alglen, &check); OPENMP_EG(ret, err);
			if(!check){
				ext_printf("[-] Error: XDH tests error when checking shared secret\n");
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
		}
#endif
#if defined(WITH_X448)
		if(t->xdh_alg == X448){
			/* Derive our public key */
			ret = x448_init_pub_key(t->privkey, pubkey_check);
			if(ret){
				ext_printf("[-] Error: XDH tests error when deriving private key to public\n");
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
			if(t->ourpubkeylen != 0){
				/* Check public key against the test one */
				ret = are_equal(t->ourpubkey, pubkey_check, alglen, &check); OPENMP_EG(ret, err);
				if(!check){
					ext_printf("[-] Error: XDH tests error when checking our public key\n");
					ext_printf("    (comment = %s)\n", t->comment);
					ret = -1;
					OPENMP_EG(ret, err);
				}
			}
			/* Derive the shared secret */
			ret = x448_derive_secret(t->privkey, t->peerpubkey, sharedsecret_check);
			if(ret){
				/* Handle "acceptable" results here (e.g. public key on twist) */
				if(t->result == 0){
					OPENMP_LOCK();
					xdh_acceptable_invalid++;
#ifdef VERBOSE_ACCEPTABLE
					ext_printf("\t[~] XDH test %d / %s (shared secret derivation NOK while acceptable)\n", i, t->name);
					ext_printf("\t    (comment = %s)\n", t->comment);
#endif
					OPENMP_UNLOCK();
					continue;
				}
				ext_printf("[-] Error: XDH tests error when deriving shared secret\n");
				ext_printf("    (comment = %s)\n", t->comment);
				OPENMP_EG(ret, err);
			}
			if(t->result == -1){
				ext_printf("[-] Error: XDH tests is OK while invalid\n");
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
			/* Check the shared secret */
			ret = are_equal(t->sharedsecret, sharedsecret_check, alglen, &check); OPENMP_EG(ret, err);
			if(!check){
				ext_printf("[-] Error: XDH tests error when checking shared secret\n");
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}

		}
#endif
		/* Log the acceptable results */
		if (t->result == 0) {
			OPENMP_LOCK();
			xdh_acceptable_valid++;
#ifdef VERBOSE_ACCEPTABLE
			ext_printf("\t[~] XDH test %d / %s (shared secret OK while acceptable)\n", i, t->name);
			ext_printf("\t    (comment = %s)\n", t->comment);
#endif
			OPENMP_UNLOCK();
		}
	}
	ret = 0;
#ifndef WITH_OPENMP_SELF_TESTS
err:
#endif
	return ret;
#else
	return 0;
#endif
}

/* Point decompression routine */
static int uncompress_ecc_point(const ec_params *params, const u8 *peerpubkey, u8 peerpubkeylen, u8 *serialized_pub_key, u8 serialized_pub_key_size, int compression)
{
	int ret, sign, check;
	fp x, tmp;
	fp_t y;
	x.magic = tmp.magic = 0;

	MUST_HAVE((params != NULL) && (peerpubkey != NULL) && (serialized_pub_key != NULL), ret, err);

	/* Uncompressed point size should be twice the x coordinate */
	MUST_HAVE((serialized_pub_key_size == (2 * peerpubkeylen)), ret, err);

	/* Compression is either 02 or 03 */
	MUST_HAVE(((compression == 0x02) || (compression == 0x03)), ret, err);

	/* Import our x coordinate */
	ret = fp_init_from_buf(&x, &(params->ec_fp), peerpubkey, peerpubkeylen); EG(ret, err);
	ret = fp_init(&tmp, &(params->ec_fp)); EG(ret, err);
	/* Compute the Weierstrass equation y^2 = x^3 + ax + b solutions */
	ret = aff_pt_y_from_x(&tmp, &x, &x, &(params->ec_curve)); EG(ret, err);

	/* Choose the square root depending on the compression information */
	sign = (compression - 2);

	ret = fp_cmp(&x, &tmp, &check); EG(ret, err);

	y = ((check > 0) == sign) ? &x : &tmp;

	/* Export the point to our buffer */
	ret = local_memcpy(&serialized_pub_key[0], &peerpubkey[0], (serialized_pub_key_size / 2)); EG(ret, err);
	ret = fp_export_to_buf(&serialized_pub_key[(serialized_pub_key_size / 2)], (serialized_pub_key_size / 2), y);

err:
	fp_uninit(&x);
	fp_uninit(&tmp);
	PTR_NULLIFY(y);

	return ret;
}

/* Check all ECDH test vectors */
static unsigned int ecdh_acceptable_invalid = 0;
static unsigned int ecdh_acceptable_valid = 0;
static unsigned int ecdh_all_performed = 0;
static int check_wycheproof_ecdh(void)
{
#if defined(WITH_ECCCDH)
	int ret;
	unsigned int i;

#ifdef WITH_OPENMP_SELF_TESTS
        #pragma omp parallel
        #pragma omp for schedule(static, 1) nowait
#endif
	for(i = 0; i < NUM_WYCHEPROOF_ECDH_TESTS; i++){
		int check;
		const wycheproof_ecdh_test *t = wycheproof_ecdh_all_tests[i];
		ec_pub_key peerpub_key;
		ec_pub_key ourpub_key;
		ec_pub_key ourpub_key_check;
		ec_priv_key priv_key;
		ec_params params;
		u8 sharedsecret_check[EC_PRIV_KEY_MAX_SIZE];
		u8 sharedsecretsize;
		u8 serialized_pub_key[EC_PUB_KEY_MAX_SIZE];
		u8 serialized_pub_key_check[EC_PUB_KEY_MAX_SIZE];
		u8 serialized_pub_key_size;

		if (t == NULL){
			continue;
		}

		OPENMP_LOCK();
		ecdh_all_performed++;
		OPENMP_UNLOCK();

		ret = local_memset(&peerpub_key, 0, sizeof(peerpub_key)); OPENMP_EG(ret, err);
		ret = local_memset(&ourpub_key, 0, sizeof(ourpub_key)); OPENMP_EG(ret, err);
		ret = local_memset(&ourpub_key_check, 0, sizeof(ourpub_key_check)); OPENMP_EG(ret, err);
		ret = local_memset(&priv_key, 0, sizeof(priv_key)); OPENMP_EG(ret, err);
		ret = local_memset(&params, 0, sizeof(params)); OPENMP_EG(ret, err);
		ret = local_memset(sharedsecret_check, 0, sizeof(sharedsecret_check)); OPENMP_EG(ret, err);
		ret = local_memset(serialized_pub_key, 0, sizeof(serialized_pub_key)); OPENMP_EG(ret, err);

		/* Import EC params from test case */
		ret = import_params(&params, t->curve);
		if (ret) {
			ext_printf("Error: ECDH tests error importing params\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}

		/* Get the sizes */
		ret = ecccdh_shared_secret_size(&params, &sharedsecretsize);
		if (ret) {
			ext_printf("Error: ECDH tests error getting shared secret size\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}
		OPENMP_MUST_HAVE((sharedsecretsize <= sizeof(sharedsecret_check)), ret, err);
		ret = ecccdh_serialized_pub_key_size(&params, &serialized_pub_key_size);
		if (ret) {
			ext_printf("Error: ECDH tests error getting serialized public key size\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}
		OPENMP_MUST_HAVE((serialized_pub_key_size <= sizeof(serialized_pub_key)), ret, err);
		OPENMP_MUST_HAVE((serialized_pub_key_size <= sizeof(serialized_pub_key_check)), ret, err);

		/* Import the private key */
		ret = ec_priv_key_import_from_buf(&priv_key, &params, t->privkey, (u8)(t->privkeylen), t->ecdh_alg);
		if (ret) {
			ext_printf("Error: ECDH tests error importing private key\n");
			ret = -1;
			OPENMP_EG(ret, err);
		}

		if(t->ourpubkeylen != 0){
			/* Import our public key if it exists */
			ret = ec_pub_key_import_from_aff_buf(&ourpub_key, &params, t->ourpubkey, (u8)(t->ourpubkeylen), t->ecdh_alg);
			if (ret && (t->result >= 0)) {
				ext_printf("[-] Error: ECDH tests error when importing our public key\n");
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
			/* Derive our private key to public */
			ret = ecccdh_init_pub_key(&ourpub_key_check, &priv_key);
			if (ret) {
				ext_printf("Error: ECDH tests error deriving our private key to public\n");
				ret = -1;
				OPENMP_EG(ret, err);
			}
			/* Check if we get the same public key by serializing them */
			ret = ecccdh_serialize_pub_key(&ourpub_key, serialized_pub_key, serialized_pub_key_size);
			if (ret){
				ext_printf("Error: ECDH tests error serializing public key\n");
				ret = -1;
				OPENMP_EG(ret, err);
			}
			ret = ecccdh_serialize_pub_key(&ourpub_key_check, serialized_pub_key_check, serialized_pub_key_size);
			if (ret){
				ext_printf("Error: ECDH tests error serializing public key\n");
				ret = -1;
				OPENMP_EG(ret, err);
			}
			ret = are_equal(serialized_pub_key, serialized_pub_key_check, serialized_pub_key_size, &check); OPENMP_EG(ret, err);
			if(!check){
				ext_printf("[-] Error: ECDH tests error when checking our public key\n");
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
		}

		/* Do we have to uncompress our point? */
		if(t->compressed > 0){
			/* Uncompress the point */
			ret = uncompress_ecc_point(&params, t->peerpubkey, (u8)(t->peerpubkeylen), serialized_pub_key, serialized_pub_key_size, t->compressed);
			if ((ret) && (t->result >= 0)) {
				ext_printf("[-] Error: ECDH tests error when uncompressing public key\n");
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
		}
		else{
			/* No point compression is used, copy our raw buffer as public key */
			if((t->peerpubkeylen != serialized_pub_key_size) && (t->result >= 0)){
				ext_printf("[-] Error: ECDH tests error when checking our public key size, got %d instead of %d\n", t->peerpubkeylen, serialized_pub_key_size);
				ext_printf("    (comment = %s)\n", t->comment);
				ret = -1;
				OPENMP_EG(ret, err);
			}
			ret = local_memcpy(serialized_pub_key, t->peerpubkey, serialized_pub_key_size); OPENMP_EG(ret, err);
		}
		/* Now derive the shared secret */
		ret = ecccdh_derive_secret(&priv_key, serialized_pub_key, serialized_pub_key_size, sharedsecret_check, sharedsecretsize);
		if ((ret) && (t->result >= 0)) {
			ext_printf("[-] Error: ECDH tests error when deriving secret while acceptable or valid\n");
			ext_printf("    (comment = %s)\n", t->comment);
			ret = -1;
			OPENMP_EG(ret, err);
		}
		if((!ret) && (t->result == -1)){
			ext_printf("Error: ECDH tests error, secret derived OK while invalid\n");
			ext_printf("    (comment = %s)\n", t->comment);
			ret = -1;
			OPENMP_EG(ret, err);
		}
		if(t->result == -1){
			continue;
		}
		if(sharedsecretsize != t->sharedsecretlen){
			ext_printf("Error: ECDH tests error, bad shared secret size %d instead of %d\n", sharedsecretsize, t->sharedsecretlen);
			ext_printf("    (comment = %s)\n", t->comment);
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Compare */
		ret = are_equal(sharedsecret_check, t->sharedsecret, sharedsecretsize, &check); OPENMP_EG(ret, err);
		if(!check){
			ext_printf("[-] Error: ECDH tests error when checking the computed shared secret, they differ\n");
			ext_printf("    (comment = %s)\n", t->comment);
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Log the acceptable results */
		if (t->result == 0) {
			OPENMP_LOCK();
			ecdh_acceptable_valid++;
#ifdef VERBOSE_ACCEPTABLE
			ext_printf("\t[~] ECDH test %d / %s (shared secret OK while acceptable)\n", i, t->name);
			ext_printf("\t    (comment = %s)\n", t->comment);
#endif
			OPENMP_UNLOCK();
		}

	}
	ret = 0;
#ifndef WITH_OPENMP_SELF_TESTS
err:
#endif
	return ret;
#else
	return 0;
#endif
}

/* Check all HMAC test vectors */
static unsigned int hmac_acceptable_invalid = 0;
static unsigned int hmac_acceptable_valid = 0;
static unsigned int hmac_all_performed = 0;
static int check_wycheproof_hmac(void)
{
#if defined(WITH_HMAC)
	int ret;
	unsigned int i;

#ifdef WITH_OPENMP_SELF_TESTS
        #pragma omp parallel
        #pragma omp for schedule(static, 1) nowait
#endif
	for(i = 0; i < NUM_WYCHEPROOF_HMAC_TESTS; i++){
		int check;
		const wycheproof_hmac_test *t = wycheproof_hmac_all_tests[i];
		u8 hmac_res[MAX_DIGEST_SIZE];
		u8 hlen;

		if (t == NULL){
			continue;
		}

		OPENMP_LOCK();
		hmac_all_performed++;
		OPENMP_UNLOCK();

		ret = local_memset(&hmac_res, 0, sizeof(hmac_res)); OPENMP_EG(ret, err);

		hlen = sizeof(hmac_res);
		ret = hmac(t->key, t->keylen, t->hash, t->msg, t->msglen, hmac_res, &hlen);
		if (ret) {
			ext_printf("[-] Error: HMAC tests error when performin HMAC\n");
			ext_printf("    (comment = %s)\n", t->comment);
			ret = -1;
			OPENMP_EG(ret, err);
		}
		if((hlen < t->taglen) && (t->result >= 0)){
			ext_printf("[-] Error: HMAC tests error: size error %d < %d\n", hlen, t->taglen);
			ext_printf("    (comment = %s)\n", t->comment);
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Compare */
		ret = are_equal(hmac_res, t->tag, t->taglen, &check); OPENMP_EG(ret, err);
		if((!check) && (t->result >= 0)){
			ext_printf("[-] Error: HMAC tests error when checking the computed tag, they differ\n");
			ext_printf("    (comment = %s)\n", t->comment);
			ret = -1;
			OPENMP_EG(ret, err);
		}
		/* Log the acceptable results */
		if (t->result == 0) {
			OPENMP_LOCK();
			hmac_acceptable_valid++;
#ifdef VERBOSE_ACCEPTABLE
			ext_printf("\t[~] HMAC test %d / %s (shared secret OK while acceptable)\n", i, t->name);
			ext_printf("\t    (comment = %s)\n", t->comment);
#endif
			OPENMP_UNLOCK();
		}
	}
	ret = 0;
#ifndef WITH_OPENMP_SELF_TESTS
err:
#endif
	return ret;
#else
	return 0;
#endif
}

int main(int argc, char *argv[])
{
	FORCE_USED_VAR(argc);
	FORCE_USED_VAR(argv);

	/**********************/
	ext_printf("==== Checking ECDH =========== Imported = %d, Skipped = %d (valid = %d, invalid = %d, acceptable = %d)\n", NUM_WYCHEPROOF_ECDH_TESTS_IMPORTED, NUM_WYCHEPROOF_ECDH_TESTS_SKIPPED, NUM_WYCHEPROOF_ECDH_TESTS_VALID, NUM_WYCHEPROOF_ECDH_TESTS_INVALID, NUM_WYCHEPROOF_ECDH_TESTS_ACCEPTABLE);
	if(check_wycheproof_ecdh()){
		goto err;
	}
	ext_printf("[+][%d] All ECDH tests went OK! (%d acceptable/valid, %d acceptable/invalid)\n", ecdh_all_performed, ecdh_acceptable_valid, ecdh_acceptable_invalid);
	/**********************/
	ext_printf("==== Checking XDH =========== Imported = %d, Skipped = %d (valid = %d, invalid = %d, acceptable = %d)\n", NUM_WYCHEPROOF_XDH_TESTS_IMPORTED, NUM_WYCHEPROOF_XDH_TESTS_SKIPPED, NUM_WYCHEPROOF_XDH_TESTS_VALID, NUM_WYCHEPROOF_XDH_TESTS_INVALID, NUM_WYCHEPROOF_XDH_TESTS_ACCEPTABLE);
	if(check_wycheproof_xdh()){
		goto err;
	}
	ext_printf("[+][%d] All XDH tests went OK! (%d acceptable/valid, %d acceptable/invalid)\n", xdh_all_performed, xdh_acceptable_valid, xdh_acceptable_invalid);
	/**********************/
	ext_printf("==== Checking ECDSA =========== Imported = %d, Skipped = %d (valid = %d, invalid = %d, acceptable = %d)\n", NUM_WYCHEPROOF_ECDSA_TESTS_IMPORTED, NUM_WYCHEPROOF_ECDSA_TESTS_SKIPPED, NUM_WYCHEPROOF_ECDSA_TESTS_VALID, NUM_WYCHEPROOF_ECDSA_TESTS_INVALID, NUM_WYCHEPROOF_ECDSA_TESTS_ACCEPTABLE);
	if(check_wycheproof_ecdsa()){
		goto err;
	}
	ext_printf("[+][%d] All ECDSA tests went OK! (%d acceptable/valid, %d acceptable/invalid)\n", ecdsa_all_performed, ecdsa_acceptable_valid, ecdsa_acceptable_invalid);
	/**********************/
	ext_printf("==== Checking EDDSA =========== Imported = %d, Skipped = %d (valid = %d, invalid = %d, acceptable = %d)\n", NUM_WYCHEPROOF_EDDSA_TESTS_IMPORTED, NUM_WYCHEPROOF_EDDSA_TESTS_SKIPPED, NUM_WYCHEPROOF_EDDSA_TESTS_VALID, NUM_WYCHEPROOF_EDDSA_TESTS_INVALID, NUM_WYCHEPROOF_EDDSA_TESTS_ACCEPTABLE);
	if(check_wycheproof_eddsa()){
		goto err;
	}
	ext_printf("[+][%d] All EDDSA tests went OK! (%d acceptable/valid, %d acceptable/invalid)\n", eddsa_all_performed, eddsa_acceptable_valid, eddsa_acceptable_invalid);
	/**********************/
	ext_printf("==== Checking HMAC =========== Imported = %d, Skipped = %d (valid = %d, invalid = %d, acceptable = %d)\n", NUM_WYCHEPROOF_HMAC_TESTS_IMPORTED, NUM_WYCHEPROOF_HMAC_TESTS_SKIPPED, NUM_WYCHEPROOF_HMAC_TESTS_VALID, NUM_WYCHEPROOF_HMAC_TESTS_INVALID, NUM_WYCHEPROOF_HMAC_TESTS_ACCEPTABLE);
	if(check_wycheproof_hmac()){
		goto err;
	}
	ext_printf("[+][%d] All HMAC tests went OK! (%d acceptable/valid, %d acceptable/invalid)\n", hmac_all_performed, hmac_acceptable_valid, hmac_acceptable_invalid);

err:
	return 0;
}
