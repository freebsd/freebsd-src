/* This is a generated file */
#ifndef __krb5_private_h__
#define __krb5_private_h__

#include <stdarg.h>

void
_krb5_aes_cts_encrypt (
	const unsigned char */*in*/,
	unsigned char */*out*/,
	size_t /*len*/,
	const void */*aes_key*/,
	unsigned char */*ivec*/,
	const int /*enc*/);

void
_krb5_crc_init_table (void);

u_int32_t
_krb5_crc_update (
	const char */*p*/,
	size_t /*len*/,
	u_int32_t /*res*/);

int
_krb5_extract_ticket (
	krb5_context /*context*/,
	krb5_kdc_rep */*rep*/,
	krb5_creds */*creds*/,
	krb5_keyblock */*key*/,
	krb5_const_pointer /*keyseed*/,
	krb5_key_usage /*key_usage*/,
	krb5_addresses */*addrs*/,
	unsigned /*nonce*/,
	krb5_boolean /*allow_server_mismatch*/,
	krb5_boolean /*ignore_cname*/,
	krb5_decrypt_proc /*decrypt_proc*/,
	krb5_const_pointer /*decryptarg*/);

krb5_ssize_t
_krb5_get_int (
	void */*buffer*/,
	unsigned long */*value*/,
	size_t /*size*/);

krb5_error_code
_krb5_get_krbtgt (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_realm /*realm*/,
	krb5_creds **/*cred*/);

time_t
_krb5_krb_life_to_time (
	int /*start*/,
	int /*life_*/);

int
_krb5_krb_time_to_life (
	time_t /*start*/,
	time_t /*end*/);

void
_krb5_n_fold (
	const void */*str*/,
	size_t /*len*/,
	void */*key*/,
	size_t /*size*/);

krb5_ssize_t
_krb5_put_int (
	void */*buffer*/,
	unsigned long /*value*/,
	size_t /*size*/);

krb5_error_code
_krb5_store_creds_heimdal_0_7 (
	krb5_storage */*sp*/,
	krb5_creds */*creds*/);

krb5_error_code
_krb5_store_creds_heimdal_pre_0_7 (
	krb5_storage */*sp*/,
	krb5_creds */*creds*/);

krb5_error_code
_krb5_store_creds_internal (
	krb5_storage */*sp*/,
	krb5_creds */*creds*/,
	int /*v0_6*/);

int
_krb5_xlock (
	krb5_context /*context*/,
	int /*fd*/,
	krb5_boolean /*exclusive*/,
	const char */*filename*/);

int
_krb5_xunlock (int /*fd*/);

#endif /* __krb5_private_h__ */
