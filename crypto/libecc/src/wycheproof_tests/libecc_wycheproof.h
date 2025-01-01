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

#include <libecc/libsig.h>

typedef struct {
	/* Test case name */
	const char *name;

	ec_alg_type sig_alg;

	hash_alg_type hash;

	/* Curve params */
	const ec_str_params *curve;

	const unsigned char *pubkey;
	unsigned int pubkeylen;

	const unsigned char *msg;
	unsigned int msglen;

	const unsigned char *sig;
	unsigned int siglen;

	int result;

	const char *comment;
} wycheproof_ecdsa_test;

typedef struct {
	/* Test case name */
	const char *name;

	ec_alg_type sig_alg;

	hash_alg_type hash;

	/* Curve params */
	const ec_str_params *curve;

	const unsigned char *pubkey;
	unsigned int pubkeylen;

	const unsigned char *privkey;
	unsigned int privkeylen;

	const unsigned char *msg;
	unsigned int msglen;

	const unsigned char *sig;
	unsigned int siglen;

	int result;

	const char *comment;
} wycheproof_eddsa_test;

typedef struct {
	/* Test case name */
	const char *name;

	ec_alg_type xdh_alg;

	/* Curve params */
	const ec_str_params *curve;

	const unsigned char *peerpubkey;
	unsigned int peerpubkeylen;

	const unsigned char *ourpubkey;
	unsigned int ourpubkeylen;

	const unsigned char *privkey;
	unsigned int privkeylen;

	const unsigned char *sharedsecret;
	unsigned int sharedsecretlen;

	int result;

	const char *comment;
} wycheproof_xdh_test;

typedef struct {
	/* Test case name */
	const char *name;

	ec_alg_type ecdh_alg;

	/* Curve params */
	const ec_str_params *curve;

	const unsigned char *peerpubkey;
	unsigned int peerpubkeylen;
	int compressed;

	const unsigned char *ourpubkey;
	unsigned int ourpubkeylen;

	const unsigned char *privkey;
	unsigned int privkeylen;

	const unsigned char *sharedsecret;
	unsigned int sharedsecretlen;

	int result;

	const char *comment;
} wycheproof_ecdh_test;

typedef struct {
	/* Test case name */
	const char *name;

	hash_alg_type hash;

	const unsigned char *key;
	unsigned int keylen;

	const unsigned char *msg;
	unsigned int msglen;

	const unsigned char *tag;
	unsigned int taglen;

	int result;

	const char *comment;
} wycheproof_hmac_test;
