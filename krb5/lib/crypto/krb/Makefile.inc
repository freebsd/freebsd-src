#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH: ${KRB5_DIR}/lib/crypto/krb

SRCS+=	aead.c \
	block_size.c \
	cf2.c \
	checksum_dk_cmac.c \
	checksum_dk_hmac.c \
	checksum_etm.c \
	checksum_hmac_md5.c \
	checksum_length.c \
	checksum_unkeyed.c \
	cksumtype_to_string.c \
	cksumtypes.c \
	coll_proof_cksum.c \
	crypto_length.c \
	decrypt.c \
	decrypt_iov.c \
	default_state.c \
	derive.c \
	enc_dk_cmac.c \
	enc_dk_hmac.c \
	enc_etm.c \
	enc_raw.c \
	enc_rc4.c \
	encrypt.c \
	encrypt_iov.c \
	encrypt_length.c \
	enctype_util.c \
	etypes.c \
	key.c \
	keyblocks.c \
	keyed_checksum_types.c \
	keyed_cksum.c \
	keylengths.c \
	make_checksum.c \
	make_checksum_iov.c \
	make_random_key.c \
	mandatory_sumtype.c \
	nfold.c \
	old_api_glue.c \
	prf.c \
	prf_aes2.c \
	prf_cmac.c \
	prf_des.c \
	prf_dk.c \
	prf_rc4.c \
	prng.c \
	random_to_key.c \
	s2k_pbkdf2.c \
	s2k_rc4.c \
	state.c \
	string_to_cksumtype.c \
	string_to_key.c \
	valid_cksumtype.c \
	verify_checksum.c \
	verify_checksum_iov.c
